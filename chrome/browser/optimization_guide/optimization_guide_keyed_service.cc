// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"

#include <memory>
#include <optional>

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/system/sys_info.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/optimization_guide/chrome_hints_manager.h"
#include "chrome/browser/optimization_guide/model_execution/optimization_guide_global_state.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_paths.h"
#include "components/component_updater/component_updater_paths.h"
#include "components/component_updater/pref_names.h"
#include "components/leveldb_proto/public/proto_database_provider.h"
#include "components/metrics/metrics_log.h"
#include "components/metrics/version_utils.h"
#include "components/metrics_services_manager/metrics_services_manager.h"
#include "components/optimization_guide/core/delivery/model_util.h"
#include "components/optimization_guide/core/delivery/prediction_manager.h"
#include "components/optimization_guide/core/hints/command_line_top_host_provider.h"
#include "components/optimization_guide/core/hints/hints_processing_util.h"
#include "components/optimization_guide/core/hints/optimization_guide_navigation_data.h"
#include "components/optimization_guide/core/hints/optimization_guide_store.h"
#include "components/optimization_guide/core/hints/tab_url_provider.h"
#include "components/optimization_guide/core/hints/top_host_provider.h"
#include "components/optimization_guide/core/optimization_guide_common.mojom-shared.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_logger.h"
#include "components/optimization_guide/core/optimization_guide_prefs.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/optimization_guide_buildflags.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/prefs/pref_service.h"
#include "components/user_prefs/user_prefs.h"
#include "components/variations/service/variations_service.h"
#include "components/variations/synthetic_trials.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/storage_partition.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/commerce/price_tracking/android/price_tracking_notification_bridge.h"
#include "chrome/browser/optimization_guide/android/jni_headers/OptimizationGuideBridge_shared_jni.h"
#include "chrome/browser/optimization_guide/android/optimization_guide_bridge.h"
#include "chrome/browser/optimization_guide/android/optimization_guide_tab_url_provider_android.h"
#else
#include "chrome/browser/optimization_guide/optimization_guide_tab_url_provider.h"
#endif


namespace {


// Used to override the value of `version_info::IsOfficialBuild()` for tests.
std::optional<bool> g_is_official_build_for_testing;

// Returns the profile to use for when setting up the keyed service when the
// profile is Off-The-Record.
Profile* GetProfileForOTROptimizationGuide(Profile* profile) {
  DCHECK(profile);
  DCHECK(profile->IsOffTheRecord());
  return profile->GetOriginalProfile();
}

}  // namespace

// static
std::unique_ptr<optimization_guide::PushNotificationManager>
OptimizationGuideKeyedService::MaybeCreatePushNotificationManager(
    Profile* profile) {
  if (optimization_guide::features::IsPushNotificationsEnabled()) {
    auto push_notification_manager =
        std::make_unique<optimization_guide::PushNotificationManager>();
#if BUILDFLAG(IS_ANDROID)
    push_notification_manager->AddObserver(
        PriceTrackingNotificationBridge::GetForBrowserContext(profile));
#endif
    return push_notification_manager;
  }
  return nullptr;
}

// static
void OptimizationGuideKeyedService::SetIsOfficialBuildForTesting(
    bool is_official_build) {
  g_is_official_build_for_testing = is_official_build;
}

OptimizationGuideKeyedService::OptimizationGuideKeyedService(
    content::BrowserContext* browser_context)
    : browser_context_(browser_context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (browser_context_) {  // Null in MockOptimizationGuideKeyedService.
    Initialize();
  }
}

OptimizationGuideKeyedService::~OptimizationGuideKeyedService() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

#if BUILDFLAG(IS_ANDROID)
base::android::ScopedJavaLocalRef<JOptimizationGuideBridge>
OptimizationGuideKeyedService::GetJavaObject() {
  if (!android_bridge_) {
    android_bridge_ =
        std::make_unique<optimization_guide::android::OptimizationGuideBridge>(
            this);
  }
  return android_bridge_->GetJavaObject();
}
#endif

void OptimizationGuideKeyedService::Initialize() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  Profile* profile = Profile::FromBrowserContext(browser_context_);

  base::FilePath profile_path = profile->GetOriginalProfile()->GetPath();

  // We have different behavior if |this| is created for an incognito profile.
  // For incognito profiles, we act in "read-only" mode of the original
  // profile's store and do not fetch any new hints or models.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory;
  base::WeakPtr<optimization_guide::OptimizationGuideStore> hint_store;
  if (profile->IsOffTheRecord()) {
    Profile* profile_for_ogks = GetProfileForOTROptimizationGuide(profile);
    OptimizationGuideKeyedService* original_ogks =
        profile_for_ogks ? OptimizationGuideKeyedServiceFactory::GetForProfile(
                               profile_for_ogks)
                         : nullptr;
    if (original_ogks) {
      hint_store = original_ogks->GetHintsManager()->hint_store();
    }
  } else {
    // Use the database associated with the original profile.
    auto* proto_db_provider = profile->GetOriginalProfile()
                                  ->GetDefaultStoragePartition()
                                  ->GetProtoDatabaseProvider();
    url_loader_factory = profile->GetDefaultStoragePartition()
                             ->GetURLLoaderFactoryForBrowserProcess();

    // Only create a top host provider from the command line if provided.
    top_host_provider_ =
        optimization_guide::CommandLineTopHostProvider::CreateIfEnabled();

    bool optimization_guide_fetching_enabled =
        optimization_guide::IsUserPermittedToFetchFromRemoteOptimizationGuide(
            profile->IsOffTheRecord(), profile->GetPrefs());
    UMA_HISTOGRAM_BOOLEAN("OptimizationGuide.RemoteFetchingEnabled",
                          optimization_guide_fetching_enabled);
    ChromeMetricsServiceAccessor::RegisterSyntheticFieldTrial(
        "SyntheticOptimizationGuideRemoteFetching",
        optimization_guide_fetching_enabled ? "Enabled" : "Disabled",
        variations::SyntheticTrialAnnotationMode::kCurrentLog);

#if BUILDFLAG(IS_ANDROID)
    tab_url_provider_ = std::make_unique<
        optimization_guide::android::OptimizationGuideTabUrlProviderAndroid>(
        profile);
#else
    tab_url_provider_ =
        std::make_unique<OptimizationGuideTabUrlProvider>(profile);
#endif

    hint_store_ =
        optimization_guide::features::ShouldPersistHintsToDisk()
            ? std::make_unique<optimization_guide::OptimizationGuideStore>(
                  proto_db_provider,
                  profile_path.Append(
                      optimization_guide::kOptimizationGuideHintStore),
                  base::ThreadPool::CreateSequencedTaskRunner(
                      {base::MayBlock(), base::TaskPriority::BEST_EFFORT}))
            : nullptr;
    hint_store = hint_store_ ? hint_store_->AsWeakPtr() : nullptr;
  }

  optimization_guide_logger_ = OptimizationGuideLogger::GetInstance();
  DCHECK(optimization_guide_logger_);
  hints_manager_ = std::make_unique<optimization_guide::ChromeHintsManager>(
      profile, profile->GetPrefs(), hint_store, top_host_provider_.get(),
      tab_url_provider_.get(), url_loader_factory,
      MaybeCreatePushNotificationManager(profile),
      IdentityManagerFactory::GetForProfile(profile),
      optimization_guide_logger_.get());

  // Register for profile initialization event to initialize the model
  // downloads.
  profile_observation_.Observe(profile);

  OPTIMIZATION_GUIDE_LOG(
      optimization_guide_common::mojom::LogSource::SERVICE_AND_SETTINGS,
      optimization_guide_logger_,
      "OptimizationGuide: KeyedService is initalized");

  optimization_guide::LogFeatureFlagsInfo(optimization_guide_logger_.get(),
                                          profile->IsOffTheRecord(),
                                          profile->GetPrefs());
}

optimization_guide::ChromeHintsManager*
OptimizationGuideKeyedService::GetHintsManager() {
  return hints_manager_.get();
}

void OptimizationGuideKeyedService::OnNavigationStartOrRedirect(
    OptimizationGuideNavigationData* navigation_data) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  base::flat_set<optimization_guide::proto::OptimizationType>
      registered_optimization_types =
          hints_manager_->registered_optimization_types();
  if (!registered_optimization_types.empty()) {
    hints_manager_->OnNavigationStartOrRedirect(navigation_data,
                                                base::DoNothing());
  }

  if (navigation_data) {
    navigation_data->set_registered_optimization_types(
        hints_manager_->registered_optimization_types());
  }
}

void OptimizationGuideKeyedService::OnNavigationFinish(
    const std::vector<GURL>& navigation_redirect_chain) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  hints_manager_->OnNavigationFinish(navigation_redirect_chain);
}

void OptimizationGuideKeyedService::AddObserverForOptimizationTargetModel(
    optimization_guide::proto::OptimizationTarget optimization_target,
    const std::optional<optimization_guide::proto::Any>& model_metadata,
    scoped_refptr<base::SequencedTaskRunner> model_task_runner,
    optimization_guide::OptimizationTargetModelObserver* observer) {
  GetGlobalState().model_provider().AddObserverForOptimizationTargetModel(
      optimization_target, model_metadata, model_task_runner, observer);
}

void OptimizationGuideKeyedService::RemoveObserverForOptimizationTargetModel(
    optimization_guide::proto::OptimizationTarget optimization_target,
    optimization_guide::OptimizationTargetModelObserver* observer) {
  GetGlobalState().model_provider().RemoveObserverForOptimizationTargetModel(
      optimization_target, observer);
}

void OptimizationGuideKeyedService::RegisterOptimizationTypes(
    const std::vector<optimization_guide::proto::OptimizationType>&
        optimization_types) {
  hints_manager_->RegisterOptimizationTypes(optimization_types);
}

optimization_guide::OptimizationGuideDecision
OptimizationGuideKeyedService::CanApplyOptimization(
    const GURL& url,
    optimization_guide::proto::OptimizationType optimization_type,
    optimization_guide::OptimizationMetadata* optimization_metadata) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  optimization_guide::OptimizationTypeDecision optimization_type_decision =
      hints_manager_->CanApplyOptimization(url, optimization_type,
                                           optimization_metadata);
  base::UmaHistogramEnumeration(
      base::StrCat({"OptimizationGuide.ApplyDecision.",
                    optimization_guide::GetStringNameForOptimizationType(
                        optimization_type)}),
      optimization_type_decision);
  return optimization_guide::ChromeHintsManager::
      GetOptimizationGuideDecisionFromOptimizationTypeDecision(
          optimization_type_decision);
}

void OptimizationGuideKeyedService::CanApplyOptimization(
    const GURL& url,
    optimization_guide::proto::OptimizationType optimization_type,
    optimization_guide::OptimizationGuideDecisionCallback callback) {
  hints_manager_->CanApplyOptimization(url, optimization_type,
                                       std::move(callback));
}

void OptimizationGuideKeyedService::CanApplyOptimizationOnDemand(
    const std::vector<GURL>& urls,
    const base::flat_set<optimization_guide::proto::OptimizationType>&
        optimization_types,
    optimization_guide::proto::RequestContext request_context,
    optimization_guide::OnDemandOptimizationGuideDecisionRepeatingCallback
        callback,
    std::optional<optimization_guide::proto::RequestContextMetadata>
        request_context_metadata) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(request_context !=
         optimization_guide::proto::RequestContext::CONTEXT_UNSPECIFIED);

  hints_manager_->CanApplyOptimizationOnDemand(urls, optimization_types,
                                               request_context, callback,
                                               request_context_metadata);
}

void OptimizationGuideKeyedService::OnProfileInitializationComplete(
    Profile* profile) {
  DCHECK(profile_observation_.IsObservingSource(profile));
  profile_observation_.Reset();
}

void OptimizationGuideKeyedService::AddHintForTesting(
    const GURL& url,
    optimization_guide::proto::OptimizationType optimization_type,
    const std::optional<optimization_guide::OptimizationMetadata>& metadata) {
  hints_manager_->AddHintForTesting(url, optimization_type, metadata);
}

void OptimizationGuideKeyedService::AddHintWithMultipleOptimizationsForTesting(
    const GURL& url,
    const std::vector<optimization_guide::proto::OptimizationType>&
        optimization_types) {
  hints_manager_->AddHintWithMultipleOptimizationsForTesting(  // IN-TEST
      url, optimization_types);
}

void OptimizationGuideKeyedService::AddHintWithMultipleOptimizationsForTesting(
    const GURL& url,
    const std::vector<
        std::pair<optimization_guide::proto::OptimizationType,
                  std::optional<optimization_guide::OptimizationMetadata>>>&
        optimization_types_and_metadata) {
  hints_manager_->AddHintWithMultipleOptimizationsForTesting(  // IN-TEST
      url, optimization_types_and_metadata);
}

void OptimizationGuideKeyedService::AddOnDemandHintForTesting(
    const GURL& url,
    optimization_guide::proto::OptimizationType optimization_type,
    const optimization_guide::OptimizationGuideDecisionWithMetadata& decision) {
  hints_manager_->AddOnDemandHintForTesting(url, optimization_type,  // IN-TEST
                                            decision);
}

void OptimizationGuideKeyedService::ClearData() {
  hints_manager_->ClearFetchedHints();
}

void OptimizationGuideKeyedService::Shutdown() {
  hints_manager_->Shutdown();
}

void OptimizationGuideKeyedService::OverrideTargetModelForTesting(
    optimization_guide::proto::OptimizationTarget optimization_target,
    std::optional<optimization_guide::ModelInfo> model_info) {
  GetPredictionManager()->OverrideTargetModelForTesting(  // IN-TEST
      optimization_target, std::move(model_info));
}

