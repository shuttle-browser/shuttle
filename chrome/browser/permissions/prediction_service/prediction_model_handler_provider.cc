// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/prediction_service/prediction_model_handler_provider.h"

#include "base/notreached.h"
#include "build/build_config.h"
#include "components/optimization_guide/core/delivery/optimization_guide_model_provider.h"
#include "components/permissions/features.h"
#include "components/permissions/prediction_service/prediction_model_handler.h"
#include "components/permissions/request_type.h"

namespace permissions {

using optimization_guide::proto::OptimizationTarget;

PredictionModelHandlerProvider::PredictionModelHandlerProvider(
    optimization_guide::OptimizationGuideModelProvider* optimization_guide) {
  VLOG(1) << "[PermissionsAI] PredictionModelHandlerProvider ctor "
             "optimization_guide available: "
          << (optimization_guide ? "true" : "false");

  if (!optimization_guide) {
    VLOG(1) << "[PermissionsAI] PredictionModelHandlerProvider optimization "
               "guide is null";
    return;
  }

  // This feature is enabled by default; we add the check here to fix internally
  // failing tests.
  if (base::FeatureList::IsEnabled(
          permissions::features::kPermissionOnDeviceNotificationPredictions)) {
    notification_prediction_model_handler_ =
        std::make_unique<PredictionModelHandler>(
            optimization_guide,
            OptimizationTarget::
                OPTIMIZATION_TARGET_NOTIFICATION_PERMISSION_PREDICTIONS);
  }

  // This feature is enabled by default; we add the check here to fix internally
  // failing tests.
  if (base::FeatureList::IsEnabled(
          permissions::features::kPermissionOnDeviceGeolocationPredictions)) {
    geolocation_prediction_model_handler_ =
        std::make_unique<PredictionModelHandler>(
            optimization_guide,
            OptimizationTarget::
                OPTIMIZATION_TARGET_GEOLOCATION_PERMISSION_PREDICTIONS);
  }
}

PredictionModelHandlerProvider::~PredictionModelHandlerProvider() = default;

void PredictionModelHandlerProvider::Shutdown() {
  // LINT.IfChange(Shutdown)
  notification_prediction_model_handler_.reset();
  geolocation_prediction_model_handler_.reset();
  // LINT.ThenChange(//chrome/browser/permissions/prediction_service/prediction_model_handler_provider.h:ModelHandlers)
}

PredictionModelHandler*
PredictionModelHandlerProvider::GetPredictionModelHandler(
    RequestType request_type) {
  switch (request_type) {
    case RequestType::kNotifications:
      return notification_prediction_model_handler_.get();
    case RequestType::kGeolocation:
      return geolocation_prediction_model_handler_.get();
    default:
      NOTREACHED();
  }
}

}  // namespace permissions
