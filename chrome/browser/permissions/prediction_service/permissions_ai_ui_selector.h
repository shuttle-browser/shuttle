// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_PREDICTION_SERVICE_PERMISSIONS_AI_UI_SELECTOR_H_
#define CHROME_BROWSER_PERMISSIONS_PREDICTION_SERVICE_PERMISSIONS_AI_UI_SELECTOR_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "components/permissions/permission_actions_history.h"
#include "components/permissions/permission_request_enums.h"
#include "components/permissions/prediction_service/permission_ui_selector.h"
#include "components/permissions/prediction_service/prediction_request_features.h"
#include "components/permissions/request_type.h"
// pref_names include is needed for the browser tests to work as they cannot
// include this dependency themselves
#include "components/unified_consent/pref_names.h"

class PredictionServiceRequest;
class Profile;

namespace permissions {
struct PredictionRequestFeatures;
class GeneratePredictionsResponse;
}  // namespace permissions

// Each instance of this class is long-lived and can support multiple requests,
// but only one at a time.
class PermissionsAiUiSelector : public permissions::PermissionUiSelector {
 public:
  // Contains information that are not important as features for the
  // prediction service, but contain details about the workflow and the origin
  // of feature data.
  struct PredictionRequestMetadata {
    permissions::PermissionPredictionSource prediction_source;
    permissions::RequestType request_type;
  };

  using PredictionGrantLikelihood =
      permissions::PermissionUiSelector::PredictionGrantLikelihood;

  // The timeout to ensure that deciding which UI to chose for a permission
  // request is not taking longer than n seconds..
  static const int kPermissionRequestUiDecisionTimeout = 3;

  // Constructs an instance in the context of the given |profile|.
  explicit PermissionsAiUiSelector(Profile* profile);
  ~PermissionsAiUiSelector() override;

  PermissionsAiUiSelector(const PermissionsAiUiSelector&) = delete;
  PermissionsAiUiSelector& operator=(const PermissionsAiUiSelector&) = delete;

  // NotificationPermissionUiSelector:
  void SelectUiToUse(content::WebContents* web_contents,
                     permissions::PermissionRequest* request,
                     DecisionMadeCallback callback) override;

  // Resets the callback, which cancels all async operations.
  void Cancel() override;

  bool IsPermissionRequestSupported(
      permissions::RequestType request_type) override;

  std::optional<PredictionGrantLikelihood> PredictedGrantLikelihoodForUKM()
      override;

  std::optional<permissions::PermissionRequestRelevance>
  PermissionRequestRelevanceForUKM() override;

  std::optional<permissions::PermissionAiRelevanceModel>
  PermissionAiRelevanceModelForUKM() override;

  std::optional<bool> WasSelectorDecisionHeldback() override;

  std::optional<permissions::PermissionRequestRelevance>
  get_permission_request_relevance_for_testing();

  void set_callback_for_testing(DecisionMadeCallback callback) {
    callback_ = std::move(callback);
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(
      PredictionBasedPermissionUiExpectedPredictionSourceTest,
      GetPredictionTypeToUse);
  FRIEND_TEST_ALL_PREFIXES(PermissionsAiUiSelectorTest,
                           GetPredictionTypeToUseCpssV1);
  FRIEND_TEST_ALL_PREFIXES(
      PredictionBasedPermissionUiExpectedHoldbackChanceTest,
      HoldbackHistogramTest);
  FRIEND_TEST_ALL_PREFIXES(PermissionsAiUiSelectorTest, HoldbackDecisionTest);

  // A safe way to invoke the callback with a decision.
  void FinishRequest(Decision decision, bool timeout = false);

  // Resets the permission request, which cancels all async operations.
  void Cleanup();

  // Called when the global timeout for the entire UI selection process is
  // reached.
  void OnTimeout();

  permissions::PredictionRequestFeatures BuildPredictionRequestFeatures(
      permissions::PermissionRequest* request,
      permissions::PermissionPredictionSource prediction_source);
  void LookupResponseReceived(
      base::TimeTicks model_inquire_start_time,
      PredictionRequestMetadata request_metadata,
      bool lookup_successful,
      bool response_from_cache,
      const std::optional<permissions::GeneratePredictionsResponse>& response);
  permissions::PermissionPredictionSource GetPredictionTypeToUse(
      permissions::RequestType request_type);

  void set_likelihood_override(PredictionGrantLikelihood mock_likelihood) {
    likelihood_override_for_testing_ = mock_likelihood;
  }

  bool ShouldHoldBack(const PredictionRequestMetadata& request_metadata) const;

  void InquireServerModel(
      const permissions::PredictionRequestFeatures& features,
      PredictionRequestMetadata request_metadata);

  // Use on device CPSSv1 tflite model.
  void InquireCpssV1OnDeviceModelIfAvailable(
      const permissions::PredictionRequestFeatures& features,
      PredictionRequestMetadata request_metadata);

  raw_ptr<Profile> profile_;
  std::unique_ptr<PredictionServiceRequest> request_;
  std::optional<PredictionGrantLikelihood> last_request_grant_likelihood_;
  std::optional<permissions::PermissionRequestRelevance>
      last_permission_request_relevance_;
  std::optional<permissions::PermissionAiRelevanceModel>
      last_permission_ai_relevance_model_;
  std::optional<float> cpss_v1_model_holdback_probability_;
  std::optional<bool> was_decision_held_back_;

  std::optional<PredictionGrantLikelihood> likelihood_override_for_testing_;

  DecisionMadeCallback callback_;

  base::OneShotTimer timeout_timer_;

  // Used to asynchronously call the callback during on device model execution.
  base::WeakPtrFactory<PermissionsAiUiSelector> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_PERMISSIONS_PREDICTION_SERVICE_PERMISSIONS_AI_UI_SELECTOR_H_
