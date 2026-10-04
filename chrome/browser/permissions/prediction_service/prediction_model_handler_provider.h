// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_PREDICTION_SERVICE_PREDICTION_MODEL_HANDLER_PROVIDER_H_
#define CHROME_BROWSER_PERMISSIONS_PREDICTION_SERVICE_PREDICTION_MODEL_HANDLER_PROVIDER_H_

#include <memory>

#include "components/keyed_service/core/keyed_service.h"
#include "components/permissions/request_type.h"

namespace optimization_guide {
class OptimizationGuideModelProvider;
}  // namespace optimization_guide

namespace permissions {

class PredictionModelHandler;

class PredictionModelHandlerProvider : public KeyedService {
 public:
  explicit PredictionModelHandlerProvider(
      optimization_guide::OptimizationGuideModelProvider* optimization_guide);
  ~PredictionModelHandlerProvider() override;
  PredictionModelHandlerProvider(const PredictionModelHandlerProvider&) =
      delete;
  PredictionModelHandlerProvider& operator=(
      const PredictionModelHandlerProvider&) = delete;

  // KeyedService:
  // Any new model handlers added to this class must also be reset in
  // `Shutdown` to ensure they are destroyed before their dependencies
  // (like OptimizationGuideKeyedService) are shut down.
  void Shutdown() override;

  PredictionModelHandler* GetPredictionModelHandler(RequestType request_type);

 private:
  // LINT.IfChange(ModelHandlers)
  std::unique_ptr<PredictionModelHandler>
      notification_prediction_model_handler_;
  std::unique_ptr<PredictionModelHandler> geolocation_prediction_model_handler_;
  // LINT.ThenChange(//chrome/browser/permissions/prediction_service/prediction_model_handler_provider.cc:Shutdown)
};
}  // namespace permissions
#endif  // CHROME_BROWSER_PERMISSIONS_PREDICTION_SERVICE_PREDICTION_MODEL_HANDLER_PROVIDER_H_
