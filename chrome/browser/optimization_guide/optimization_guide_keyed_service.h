// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_KEYED_SERVICE_H_
#define CHROME_BROWSER_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_KEYED_SERVICE_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/scoped_observation.h"
#include "build/build_config.h"
#include "chrome/browser/optimization_guide/model_execution/optimization_guide_global_state.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/optimization_guide/core/delivery/model_info.h"
#include "components/optimization_guide/core/delivery/optimization_guide_model_provider.h"
#include "components/optimization_guide/core/hints/optimization_guide_decider.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/optimization_guide/proto/models.pb.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/scoped_java_ref.h"
#include "chrome/browser/bookmarks/android/bookmark_bridge.h"
#include "chrome/browser/optimization_guide/android/jni_headers/OptimizationGuideBridge_shared_jni.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace content {
class BrowserContext;
}  // namespace content


namespace on_device_internals {
class PageHandler;
}

namespace optimization_guide {
class ChromeHintsManager;
class OptimizationGuideStore;
class OptimizationGuideKeyedServiceBrowserTest;
class PredictionManagerBrowserTestBase;
class PredictionModelDownloadClient;
class PredictionModelStoreBrowserTestBase;
class PushNotificationManager;
class TabUrlProvider;
class TopHostProvider;

#if BUILDFLAG(IS_ANDROID)
namespace android {
class OptimizationGuideBridge;
}  // namespace android
#endif  // BUILDFLAG(IS_ANDROID)
}  // namespace optimization_guide

class GURL;
class OptimizationGuideLogger;
class OptimizationGuideNavigationData;
class Profile;

namespace settings {
class SettingsUI;
}  // namespace settings

// Keyed service that can be used to get information received from the remote
// Optimization Guide Service. For regular profiles, this will do the work to
// fetch the necessary information from the remote Optimization Guide Service
// in anticipation for when it is needed. For off the record profiles, this will
// act in a "read-only" mode where it will only serve information that was
// received from the remote Optimization Guide Service when not off the record
// and no information will be retrieved.
class OptimizationGuideKeyedService
    : public KeyedService,
      public optimization_guide::OptimizationGuideDecider,
      public optimization_guide::OptimizationGuideModelProvider,
      public ProfileObserver {
 public:
  explicit OptimizationGuideKeyedService(
      content::BrowserContext* browser_context);

  OptimizationGuideKeyedService(const OptimizationGuideKeyedService&) = delete;
  OptimizationGuideKeyedService& operator=(
      const OptimizationGuideKeyedService&) = delete;

  ~OptimizationGuideKeyedService() override;

#if BUILDFLAG(IS_ANDROID)
  base::android::ScopedJavaLocalRef<JOptimizationGuideBridge> GetJavaObject();
#endif

  // optimization_guide::OptimizationGuideDecider implementation:
  void RegisterOptimizationTypes(
      const std::vector<optimization_guide::proto::OptimizationType>&
          optimization_types) override;
  void CanApplyOptimization(
      const GURL& url,
      optimization_guide::proto::OptimizationType optimization_type,
      optimization_guide::OptimizationGuideDecisionCallback callback) override;
  optimization_guide::OptimizationGuideDecision CanApplyOptimization(
      const GURL& url,
      optimization_guide::proto::OptimizationType optimization_type,
      optimization_guide::OptimizationMetadata* optimization_metadata) override;

  // optimization_guide::OptimizationGuideModelProvider implementation:
  void AddObserverForOptimizationTargetModel(
      optimization_guide::proto::OptimizationTarget optimization_target,
      const std::optional<optimization_guide::proto::Any>& model_metadata,
      scoped_refptr<base::SequencedTaskRunner> model_task_runner,
      optimization_guide::OptimizationTargetModelObserver* observer) override;
  void RemoveObserverForOptimizationTargetModel(
      optimization_guide::proto::OptimizationTarget optimization_target,
      optimization_guide::OptimizationTargetModelObserver* observer) override;

  // Adds hints for a URL with provided metadata to the optimization guide.
  // For testing purposes only. This will flush any callbacks for |url| that
  // were registered via |CanApplyOptimization|. If no applicable callbacks
  // were registered, this will just add the hint for later use.
  void AddHintForTesting(
      const GURL& url,
      optimization_guide::proto::OptimizationType optimization_type,
      const std::optional<optimization_guide::OptimizationMetadata>& metadata);

  // Adds hints for a URL for the given optimization types to the optimization
  // guide. For testing purposes only. This will flush any callbacks for |url|
  // that were registered via |CanApplyOptimization|. If no applicable callbacks
  // were registered, this will just add the hint for later use.
  void AddHintWithMultipleOptimizationsForTesting(
      const GURL& url,
      const std::vector<optimization_guide::proto::OptimizationType>&
          optimization_types);

  // Adds hints for a URL with provided optimization types and metadata to the
  // optimization guide. For testing purposes only. This will flush any
  // callbacks for |url| that were registered via |CanApplyOptimization|. If no
  // applicable callbacks were registered, this will just add the hint for later
  // use.
  void AddHintWithMultipleOptimizationsForTesting(
      const GURL& url,
      const std::vector<
          std::pair<optimization_guide::proto::OptimizationType,
                    std::optional<optimization_guide::OptimizationMetadata>>>&
          optimization_types_and_metadata);

  // Adds hints for a URL with provided metadata to the optimization guide.
  // Hints added via this method will work for `CanApplyOptimizationOnDemand`
  // calls. For testing purposes only.
  void AddOnDemandHintForTesting(
      const GURL& url,
      optimization_guide::proto::OptimizationType optimization_type,
      const optimization_guide::OptimizationGuideDecisionWithMetadata&
          decision);

  // Override the model file sent to observers of |optimization_target|. Use
  // ModelInfo aggregate initialization to construct the model metadata. For
  // testing purposes only.
  void OverrideTargetModelForTesting(
      optimization_guide::proto::OptimizationTarget optimization_target,
      std::optional<optimization_guide::ModelInfo> model_info);

  // Creates the platform specific push notification manager. May returns
  // nullptr for desktop or when the push notification feature is disabled.
  static std::unique_ptr<optimization_guide::PushNotificationManager>
  MaybeCreatePushNotificationManager(Profile* profile);

  OptimizationGuideLogger* GetOptimizationGuideLogger() {
    return optimization_guide_logger_.get();
  }

#if BUILDFLAG(IS_ANDROID)
  friend class optimization_guide::android::OptimizationGuideBridge;
#endif  // BUILDFLAG(IS_ANDROID)

  // Allows tests to override the value of `version_info::IsOfficialBuild()`.
  static void SetIsOfficialBuildForTesting(bool is_official_build);

  // Initializes |this|.
  void Initialize();

  // Virtualized for testing.
  virtual optimization_guide::ChromeHintsManager* GetHintsManager();

  optimization_guide::TopHostProvider* GetTopHostProvider() {
    return top_host_provider_.get();
  }

  optimization_guide::PredictionManager* GetPredictionManager() {
    return &GetGlobalState().prediction_manager();
  }

  optimization_guide::OptimizationGuideGlobalState& GetGlobalState() {
    if (!optimization_guide_global_state_) {
      optimization_guide_global_state_ =
          optimization_guide::OptimizationGuideGlobalState::CreateOrGet();
    }
    return *optimization_guide_global_state_;
  }

  // Notifies |hints_manager_| that the navigation associated with
  // |navigation_data| has started or redirected. Virtual for testing.
  virtual void OnNavigationStartOrRedirect(
      OptimizationGuideNavigationData* navigation_data);

  // Notifies |hints_manager_| that the navigation associated with
  // |navigation_redirect_chain| has finished. Virtual for testing.
  virtual void OnNavigationFinish(
      const std::vector<GURL>& navigation_redirect_chain);

  // Clears data specific to the user.
  void ClearData();

  // KeyedService implementation:
  void Shutdown() override;

  // ProfileObserver implementation:
  void OnProfileInitializationComplete(Profile* profile) override;

  // optimization_guide::NewOptimizationGuideDecider implementation:
  void CanApplyOptimizationOnDemand(
      const std::vector<GURL>& urls,
      const base::flat_set<optimization_guide::proto::OptimizationType>&
          optimization_types,
      optimization_guide::proto::RequestContext request_context,
      optimization_guide::OnDemandOptimizationGuideDecisionRepeatingCallback
          callback,
      std::optional<optimization_guide::proto::RequestContextMetadata>
          request_context_metadata = std::nullopt) override;

  bool ComponentUpdatesEnabledProvider() const;

  raw_ptr<content::BrowserContext> browser_context_;

  // The store of hints.
  std::unique_ptr<optimization_guide::OptimizationGuideStore> hint_store_;

  // The logger that plumbs the debug logs to the optimization guide
  // internals page. Must outlive `prediction_manager_` and `hints_manager_`.
  raw_ptr<OptimizationGuideLogger> optimization_guide_logger_;

  // Keep a reference to this so it stays alive.
  // Even though this can be obtained from OptimizationGuideGlobalFeature, we
  // keep a reference here to handle difference in lifetime issues. At least in
  // tests, the GlobalFeatures is destroyed before the Profile.
  scoped_refptr<optimization_guide::OptimizationGuideGlobalState>
      optimization_guide_global_state_;

  // The tab URL provider to use for fetching information for the user's active
  // tabs. Will be null if the user is off the record.
  std::unique_ptr<optimization_guide::TabUrlProvider> tab_url_provider_;

  // The top host provider to use for fetching information for the user's top
  // hosts. Will be null if the user has not consented to this type of browser
  // behavior.
  std::unique_ptr<optimization_guide::TopHostProvider> top_host_provider_;

  // Manages the storing, loading, and fetching of hints.
  std::unique_ptr<optimization_guide::ChromeHintsManager> hints_manager_;

#if BUILDFLAG(IS_ANDROID)
  // Manage and fetch the java object that wraps this OptimizationGuide on
  // android.
  std::unique_ptr<optimization_guide::android::OptimizationGuideBridge>
      android_bridge_;
#endif

  // Used to observe profile initialization event.
  base::ScopedObservation<Profile, ProfileObserver> profile_observation_{this};

  base::WeakPtrFactory<OptimizationGuideKeyedService> weak_factory_{this};
};

#endif  // CHROME_BROWSER_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_KEYED_SERVICE_H_
