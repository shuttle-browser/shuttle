// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/prediction_service/permissions_ai_ui_selector.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/rand_util.h"
#include "base/strings/strcat.h"
#include "base/time/default_clock.h"
#include "chrome/browser/permissions/permission_actions_history_factory.h"
#include "chrome/browser/permissions/prediction_service/prediction_model_handler_provider.h"
#include "chrome/browser/permissions/prediction_service/prediction_model_handler_provider_factory.h"
#include "chrome/browser/permissions/prediction_service/prediction_service_factory.h"
#include "chrome/browser/permissions/prediction_service/prediction_service_request.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_actions_history.h"
#include "components/permissions/permission_request.h"
#include "components/permissions/permission_request_enums.h"
#include "components/permissions/permission_uma_util.h"
#include "components/permissions/permission_util.h"
#include "components/permissions/prediction_service/permission_ui_selector.h"
#include "components/permissions/prediction_service/prediction_common.h"
#include "components/permissions/prediction_service/prediction_model_handler.h"
#include "components/permissions/prediction_service/prediction_service.h"
#include "components/permissions/prediction_service/prediction_service_messages.pb.h"
#include "components/permissions/request_type.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/unified_consent/pref_names.h"
#include "content/public/browser/web_contents.h"

namespace {
using ::permissions::PermissionRequest;
using ::permissions::PermissionRequestRelevance;
using ::permissions::PermissionUiSelector;
using ::permissions::PermissionUmaUtil;
using ::permissions::PredictionModelHandlerProvider;
using ::permissions::PredictionModelType;
using ::permissions::PredictionRequestFeatures;
using QuietUiReason = PermissionsAiUiSelector::QuietUiReason;
using Decision = PermissionsAiUiSelector::Decision;
using PredictionSource = ::permissions::PermissionPredictionSource;

constexpr auto VeryUnlikely = permissions::
    PermissionPrediction_Likelihood_DiscretizedLikelihood_VERY_UNLIKELY;
constexpr auto Unlikely =
    permissions::PermissionPrediction_Likelihood_DiscretizedLikelihood_UNLIKELY;

// The data we consider can only be at most 28 days old to match the data that
// the ML model is built on.
constexpr base::TimeDelta kPermissionActionCutoffAge = base::Days(28);

// Only send requests if there are at least 4 action in the user's history for
// the particular permission type.
constexpr size_t kRequestedPermissionMinimumHistoricalActions = 4;

std::optional<
    permissions::PermissionPrediction_Likelihood_DiscretizedLikelihood>
ParsePredictionServiceMockLikelihood(const std::string& value) {
  if (value == "very-unlikely") {
    return permissions::
        PermissionPrediction_Likelihood_DiscretizedLikelihood_VERY_UNLIKELY;
  } else if (value == "unlikely") {
    return permissions::
        PermissionPrediction_Likelihood_DiscretizedLikelihood_UNLIKELY;
  } else if (value == "neutral") {
    return permissions::
        PermissionPrediction_Likelihood_DiscretizedLikelihood_NEUTRAL;
  } else if (value == "likely") {
    return permissions::
        PermissionPrediction_Likelihood_DiscretizedLikelihood_LIKELY;
  } else if (value == "very-likely") {
    return permissions::
        PermissionPrediction_Likelihood_DiscretizedLikelihood_VERY_LIKELY;
  }

  return std::nullopt;
}

bool ShouldPredictionTriggerQuietUi(
    PermissionUiSelector::PredictionGrantLikelihood likelihood,
    PermissionRequestRelevance relevance) {
  if (base::FeatureList::IsEnabled(
          permissions::features::kPermissionsAILikelihoodOrRelevance)) {
    return likelihood == Unlikely || likelihood == VeryUnlikely ||
           relevance == PermissionRequestRelevance::kVeryLow ||
           relevance == PermissionRequestRelevance::kLow;
  }
  if (base::FeatureList::IsEnabled(permissions::features::kPermissionsAIP92)) {
    return likelihood == Unlikely || likelihood == VeryUnlikely;
  }
  return likelihood == VeryUnlikely;
}

PermissionUiSelector::GeolocationAccuracy GetPredictedGeolocationAccuracy(
    const permissions::GeneratePredictionsResponse& response) {
  if (!response.prediction(0).has_geolocation_prediction()) {
    return PermissionUiSelector::GeolocationAccuracy::kUnspecified;
  }
  switch (response.prediction(0).geolocation_prediction().accuracy()) {
    case permissions::PermissionPrediction::GeolocationPrediction::
        ACCURACY_UNSPECIFIED:
      return PermissionUiSelector::GeolocationAccuracy::kUnspecified;
    case permissions::PermissionPrediction::GeolocationPrediction::
        ACCURACY_PRECISE:
      return PermissionUiSelector::GeolocationAccuracy::kPrecise;
    case permissions::PermissionPrediction::GeolocationPrediction::
        ACCURACY_APPROXIMATE:
      return PermissionUiSelector::GeolocationAccuracy::kApproximate;
  }
}

}  // namespace

PermissionsAiUiSelector::PermissionsAiUiSelector(Profile* profile)
    : profile_(profile) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kPredictionServiceMockLikelihood)) {
    auto mock_likelihood = ParsePredictionServiceMockLikelihood(
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kPredictionServiceMockLikelihood));
    if (mock_likelihood.has_value()) {
      set_likelihood_override(mock_likelihood.value());
    }
  }
}

PermissionsAiUiSelector::~PermissionsAiUiSelector() = default;

void PermissionsAiUiSelector::InquireServerModel(
    const PredictionRequestFeatures& features,
    PredictionRequestMetadata request_metadata) {
  permissions::PredictionService* service =
      PredictionServiceFactory::GetForProfile(profile_);

  VLOG(1) << "[CPSS] Starting prediction service request";

  request_ = std::make_unique<PredictionServiceRequest>(
      service, features,
      base::BindOnce(&PermissionsAiUiSelector::LookupResponseReceived,
                     base::Unretained(this),
                     /*model_inquire_start_time=*/base::TimeTicks::Now(),
                     std::move(request_metadata)));
}

void PermissionsAiUiSelector::InquireCpssV1OnDeviceModelIfAvailable(
    const PredictionRequestFeatures& features,
    PredictionRequestMetadata request_metadata) {
  PredictionModelHandlerProvider* prediction_model_handler_provider =
      PredictionModelHandlerProviderFactory::GetForBrowserContext(profile_);
  permissions::PredictionModelHandler* prediction_model_handler = nullptr;
  if (prediction_model_handler_provider) {
    prediction_model_handler =
        prediction_model_handler_provider->GetPredictionModelHandler(
            request_metadata.request_type);
  }
  if (prediction_model_handler && prediction_model_handler->ModelAvailable() &&
      prediction_model_handler->GetModelInfo().has_value()) {
    VLOG(1) << "[CPSS] Using locally available CPSSv1 model";
    auto proto_request = GetPredictionRequestProto(features);
    cpss_v1_model_holdback_probability_ =
        prediction_model_handler->HoldBackProbability();
    prediction_model_handler->ExecuteModelWithMetadata(
        base::BindOnce(&PermissionsAiUiSelector::LookupResponseReceived,
                       weak_ptr_factory_.GetWeakPtr(),
                       /*model_inquire_start_time=*/base::TimeTicks::Now(),
                       std::move(request_metadata),
                       /*lookup_succesful=*/true,
                       /*response_from_cache=*/false),
        std::move(proto_request));
    return;
  }
  VLOG(1) << "[CPSS] On device CPSSv1 model unavailable";
  FinishRequest(Decision::UseNormalUiAndShowNoWarning());
}

void PermissionsAiUiSelector::SelectUiToUse(
    content::WebContents* web_contents,
    permissions::PermissionRequest* request,
    DecisionMadeCallback callback) {
  VLOG(1) << "[CPSS] Selector activated";

  // If callback is already set, it means that the selector was already
  // activated for a previous permission request and the decision has not been
  // delivered yet. This can happen if the page triggers a permission request
  // while the previous permission request is still pending. In this case, we
  // ignore the new request as we cannot stop previously activated evaluation.
  // callback_ is reset to prevent the decision from being delivered to the
  // obsolete request.
  if (callback_) {
    VLOG(1) << "[CPSS] Concurrent permission requests evaluations are not "
               "supported.";
    Cancel();

    PermissionUmaUtil::RecordPermissionPredictionConcurrentRequests(
        request->request_type());

    std::move(callback).Run(Decision::UseNormalUiAndShowNoWarning());
    return;
  }

  callback_ = std::move(callback);
  timeout_timer_.Start(FROM_HERE,
                       base::Seconds(kPermissionRequestUiDecisionTimeout),
                       base::BindOnce(&PermissionsAiUiSelector::OnTimeout,
                                      weak_ptr_factory_.GetWeakPtr()));
  last_permission_ai_relevance_model_ = std::nullopt;
  last_permission_request_relevance_ = std::nullopt;
  last_request_grant_likelihood_ = std::nullopt;
  cpss_v1_model_holdback_probability_ = std::nullopt;
  was_decision_held_back_ = std::nullopt;

  const PredictionSource prediction_source =
      GetPredictionTypeToUse(request->request_type());

  PermissionUmaUtil::RecordPermissionPredictionSource(prediction_source,
                                                      *request);

  if (prediction_source == PredictionSource::kNoCpssModel) {
    VLOG(1) << "[CPSS] Configuration does not allow CPSS requests";
    FinishRequest(Decision::UseNormalUiAndShowNoWarning());
    return;
  }

  PredictionRequestFeatures features =
      BuildPredictionRequestFeatures(request, prediction_source);
  if (prediction_source == PredictionSource::kOnDeviceCpssV1Model) {
    if (features.requested_permission_counts.total() <
        kRequestedPermissionMinimumHistoricalActions) {
      VLOG(1) << "[CPSS] Historic prompt count ("
              << features.requested_permission_counts.total()
              << ") is smaller than threshold ("
              << kRequestedPermissionMinimumHistoricalActions << ")";
      FinishRequest(Decision::UseNormalUiAndShowNoWarning());
      return;
    }
  }

  if (likelihood_override_for_testing_.has_value()) {
    VLOG(1) << "[CPSS] Using likelihood override value that was provided via "
               "command line";
    if (ShouldPredictionTriggerQuietUi(
            likelihood_override_for_testing_.value(),
            PermissionRequestRelevance::kUnspecified)) {
      FinishRequest(Decision::UseQuietUi(
          QuietUiReason::kServicePredictedVeryUnlikelyGrant,
          Decision::ShowNoWarning()));
    } else {
      FinishRequest(Decision::UseNormalUiAndShowNoWarning());
    }
    return;
  }

  DCHECK(!request_);
  PredictionRequestMetadata request_metadata = {
      .prediction_source = prediction_source,
      .request_type = request->request_type()};

  switch (prediction_source) {
    case PredictionSource::kServerSideCpssV3Model:
      return InquireServerModel(features, std::move(request_metadata));
    case PredictionSource::kOnDeviceCpssV1Model:
      return InquireCpssV1OnDeviceModelIfAvailable(features,
                                                   std::move(request_metadata));
    case PredictionSource::kNoCpssModel:
      [[fallthrough]];
    default:
      NOTREACHED();
  }
}

void PermissionsAiUiSelector::OnTimeout() {
  VLOG(1) << "[CPSS] Overall timeout for prediction reached.";
  Cleanup();
  FinishRequest(Decision::UseNormalUiAndShowNoWarning(), /*timeout=*/true);
}

void PermissionsAiUiSelector::Cancel() {
  timeout_timer_.Stop();
  callback_.Reset();
  weak_ptr_factory_.InvalidateWeakPtrs();
  Cleanup();
}

void PermissionsAiUiSelector::FinishRequest(Decision decision, bool timeout) {
  timeout_timer_.Stop();
  PermissionUmaUtil::RecordPredictionServiceTimeout(timeout);
  if (!callback_) {
    VLOG(1) << "[CPSS] FinishRequest called but callback is null";
    return;
  }

  VLOG(1) << "[CPSS] Finishing permission prediction request.";
  std::move(callback_).Run(std::move(decision));
}

void PermissionsAiUiSelector::Cleanup() {
  request_.reset();
}

bool PermissionsAiUiSelector::IsPermissionRequestSupported(
    permissions::RequestType request_type) {
  return request_type == permissions::RequestType::kNotifications ||
         request_type == permissions::RequestType::kGeolocation;
}

std::optional<PermissionUiSelector::PredictionGrantLikelihood>
PermissionsAiUiSelector::PredictedGrantLikelihoodForUKM() {
  return last_request_grant_likelihood_;
}

std::optional<PermissionRequestRelevance>
PermissionsAiUiSelector::PermissionRequestRelevanceForUKM() {
  return last_permission_request_relevance_;
}

std::optional<permissions::PermissionAiRelevanceModel>
PermissionsAiUiSelector::PermissionAiRelevanceModelForUKM() {
  return last_permission_ai_relevance_model_;
}

std::optional<bool> PermissionsAiUiSelector::WasSelectorDecisionHeldback() {
  return was_decision_held_back_;
}

PredictionRequestFeatures
PermissionsAiUiSelector::BuildPredictionRequestFeatures(
    PermissionRequest* request,
    PredictionSource prediction_source) {
  PredictionRequestFeatures features;
  features.gesture = request->GetGestureType();
  features.type = request->request_type();
#if BUILDFLAG(IS_ANDROID)
  if (base::FeatureList::IsEnabled(
          permissions::features::kPermissionDedicatedCpssSettingAndroid)) {
    features.url = request->requesting_origin().GetWithEmptyPath();
  }
#else
  if (base::FeatureList::IsEnabled(
          permissions::features::kPermissionPredictionsV2)) {
    features.url = request->requesting_origin().GetWithEmptyPath();
  }
#endif

  // Init `permission_relevance` here to avoid a crash during
  // `ConvertToProtoRelevance` execution.
  features.permission_relevance = PermissionRequestRelevance::kUnspecified;

  features.experiment_id =
      PredictionRequestFeatures::ExperimentId::kNoExperimentId;

  base::Time cutoff = base::Time::Now() - kPermissionActionCutoffAge;

  permissions::PermissionActionsHistory* action_history =
      PermissionActionsHistoryFactory::GetForProfile(profile_);

  auto actions = action_history->GetHistory(
      cutoff, request->request_type(),
      permissions::PermissionActionsHistory::EntryFilter::WANT_ALL_PROMPTS);
  permissions::PermissionActionsHistory::FillInActionCounts(
      &features.requested_permission_counts, actions);

  actions = action_history->GetHistory(
      cutoff,
      permissions::PermissionActionsHistory::EntryFilter::WANT_ALL_PROMPTS);
  permissions::PermissionActionsHistory::FillInActionCounts(
      &features.all_permission_counts, actions);

  return features;
}

void PermissionsAiUiSelector::LookupResponseReceived(
    base::TimeTicks model_inquire_start_time,
    PredictionRequestMetadata request_metadata,
    bool lookup_successful,
    bool response_from_cache,
    const std::optional<permissions::GeneratePredictionsResponse>& response) {
  // This function is used as callback for request to the CPSSv1 on-device
  // model and the CPSSv3 server-side model. As we have multiple prediction
  // sources that use the server side model in the end, we check for the
  // CPSSv1 here and set is_on_device depending on this.
  bool is_on_device_cpss_v1 = request_metadata.prediction_source ==
                              PredictionSource::kOnDeviceCpssV1Model;
  PermissionUmaUtil::RecordPredictionModelInquireTime(
      is_on_device_cpss_v1 ? PredictionModelType::kOnDeviceCpssV1Model
                           : PredictionModelType::kServerSideCpssV3Model,
      model_inquire_start_time);

  request_.reset();
  if (!callback_) {
    VLOG(1) << "[CPSS] Prediction service response ignored as the request is "
               "canceled";
    return;
  }
  if (!lookup_successful || !response || response->prediction_size() == 0) {
    VLOG(1) << "[CPSS] Prediction service request failed because "
            << (!lookup_successful ? "the lookup was not successful."
                                   : (!response ? "the response is empty."
                                                : "the prediction is empty."));
    FinishRequest(Decision::UseNormalUiAndShowNoWarning());
    return;
  }

  last_request_grant_likelihood_ =
      response->prediction(0).grant_likelihood().discretized_likelihood();

  if (ShouldHoldBack(request_metadata)) {
    VLOG(1) << "[CPSS] Prediction service decision held back";
    was_decision_held_back_ = true;
    FinishRequest(Decision::UseNormalUi(
        Decision::ShowNoWarning(), GetPredictedGeolocationAccuracy(*response)));
    return;
  }
  was_decision_held_back_ = false;
  VLOG(1) << "[CPSS] Prediction service request succeeded and received "
             "likelihood: "
          << last_request_grant_likelihood_.value();

  if (ShouldPredictionTriggerQuietUi(
          last_request_grant_likelihood_.value(),
          last_permission_request_relevance_.value_or(
              PermissionRequestRelevance::kUnspecified))) {
    FinishRequest(Decision::UseQuietUi(
        is_on_device_cpss_v1
            ? QuietUiReason::kOnDevicePredictedVeryUnlikelyGrant
            : QuietUiReason::kServicePredictedVeryUnlikelyGrant,
        Decision::ShowNoWarning()));
    return;
  }

  FinishRequest(Decision(Decision::UseNormalUi(
      Decision::ShowNoWarning(), GetPredictedGeolocationAccuracy(*response))));
}

bool PermissionsAiUiSelector::ShouldHoldBack(
    const PredictionRequestMetadata& request_metadata) const {
  permissions::RequestType request_type = request_metadata.request_type;
  PredictionSource prediction_source = request_metadata.prediction_source;
  DCHECK(request_type == permissions::RequestType::kNotifications ||
         request_type == permissions::RequestType::kGeolocation);

  // Holdback probability for this request.
  const double holdback_chance = base::RandDouble();
  bool should_holdback = false;
  PredictionModelType prediction_model;

  should_holdback =
      holdback_chance <
      permissions::feature_params::kPermissionPredictionsV2HoldbackChance.Get();

  switch (prediction_source) {
    case PredictionSource::kOnDeviceCpssV1Model:
      DCHECK(cpss_v1_model_holdback_probability_.has_value());
      should_holdback = holdback_chance < *cpss_v1_model_holdback_probability_;
      prediction_model = PredictionModelType::kOnDeviceCpssV1Model;
      break;
    case PredictionSource::kServerSideCpssV3Model:
      prediction_model = PredictionModelType::kServerSideCpssV3Model;
      break;
    default:
      NOTREACHED();
  }
  PermissionUmaUtil::RecordPermissionPredictionServiceHoldback(
      request_type, prediction_model, should_holdback);
  return should_holdback;
}

PredictionSource PermissionsAiUiSelector::GetPredictionTypeToUse(
    permissions::RequestType request_type) {
  const bool is_msbb_enabled = profile_->GetPrefs()->GetBoolean(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled);

  base::UmaHistogramBoolean("Permissions.PredictionService.MSBB",
                            is_msbb_enabled);

  VLOG(1) << "[CPSS] GetPredictionTypeToUse MSBB: " << is_msbb_enabled;

  const bool is_notification_cpss_enabled =
      profile_->GetPrefs()->GetBoolean(prefs::kEnableNotificationCPSS);

  VLOG(1) << "[CPSS] GetPredictionTypeToUse NotificationCPSS: "
          << is_notification_cpss_enabled;

  const bool is_geolocation_cpss_enabled =
      profile_->GetPrefs()->GetBoolean(prefs::kEnableGeolocationCPSS);

  VLOG(1) << "[CPSS] GetPredictionTypeToUse GeolocationCPSS: "
          << is_geolocation_cpss_enabled;

  if (request_type == permissions::RequestType::kNotifications &&
      !is_notification_cpss_enabled) {
    VLOG(1) << "[CPSS] GetPredictionTypeToUse NoCpssModel";
    return PredictionSource::kNoCpssModel;
  }

  if (request_type == permissions::RequestType::kGeolocation &&
      !is_geolocation_cpss_enabled) {
    VLOG(1) << "[CPSS] GetPredictionTypeToUse NoCpssModel";
    return PredictionSource::kNoCpssModel;
  }

  bool use_server_side = false;
  if (is_msbb_enabled) {
#if BUILDFLAG(IS_ANDROID)
    use_server_side = base::FeatureList::IsEnabled(
        permissions::features::kPermissionDedicatedCpssSettingAndroid);
#else
    use_server_side = base::FeatureList::IsEnabled(
        permissions::features::kPermissionPredictionsV2);
#endif  // BUILDFLAG(IS_ANDROID)
  }
  if (use_server_side) {
    VLOG(1) << "[CPSS] GetPredictionTypeToUse CPSSv3";
    return PredictionSource::kServerSideCpssV3Model;
  }

  if ((request_type == permissions::RequestType::kNotifications &&
       base::FeatureList::IsEnabled(
           permissions::features::
               kPermissionOnDeviceNotificationPredictions)) ||
      (request_type == permissions::RequestType::kGeolocation &&
       base::FeatureList::IsEnabled(
           permissions::features::kPermissionOnDeviceGeolocationPredictions))) {
    VLOG(1) << "[CPSS] GetPredictionTypeToUse CPSSv1";
    return PredictionSource::kOnDeviceCpssV1Model;
  }

  VLOG(1) << "[CPSS] GetPredictionTypeToUse NoCpssModel";
  return PredictionSource::kNoCpssModel;
}
