// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OPTIMIZATION_GUIDE_MODEL_EXECUTION_OPTIMIZATION_GUIDE_GLOBAL_STATE_H_
#define CHROME_BROWSER_OPTIMIZATION_GUIDE_MODEL_EXECUTION_OPTIMIZATION_GUIDE_GLOBAL_STATE_H_

#include <memory>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/optimization_guide/prediction/chrome_profile_download_service_tracker.h"
#include "components/optimization_guide/core/delivery/optimization_guide_model_provider.h"
#include "components/optimization_guide/core/delivery/prediction_manager.h"
#include "components/optimization_guide/core/delivery/prediction_model_component_update_listener.h"
#include "components/optimization_guide/core/delivery/prediction_model_store.h"
#include "components/optimization_guide/core/optimization_guide_enums.h"

namespace optimization_guide {

class OptimizationGuideGlobalFeature;
class OptimizationGuideGlobalStateTest;

void RegisterPredictionModelComponent(
    proto::OptimizationTarget target,
    base::WeakPtr<PredictionModelComponentUpdateListener> listener);

// Constructs and initializes a PredictionManager with it's dependencies.
class ChromePredictionManager {
 public:
  ChromePredictionManager();
  ~ChromePredictionManager();

  PredictionModelStore& prediction_model_store() {
    return prediction_model_store_;
  }
  PredictionManager& prediction_manager() { return prediction_manager_; }
  OptimizationGuideModelProvider& model_provider() {
    return prediction_manager_;
  }

 private:
  PredictionModelStore prediction_model_store_;
  PredictionManager prediction_manager_;
  ChromeProfileDownloadServiceTracker profile_download_service_tracker_;
};

// This holds the ManifestBrokerState and other common objects shared between
// profiles. Since some of the members hold raw_ptr to browser process level
// objects, such as local state prefs, profile manager, it must not outlive the
// browser process, so each profile holds a ref to it in
// OptimizationGuideKeyedService to keep it alive until all profiles are
// destroyed.
class OptimizationGuideGlobalState final
    : public base::RefCounted<OptimizationGuideGlobalState> {
 public:
  // Retrieves or creates the instance.
  static scoped_refptr<OptimizationGuideGlobalState> CreateOrGet();

  PredictionModelStore& prediction_model_store() {
    return prediction_manager_.prediction_model_store();
  }
  PredictionManager& prediction_manager() {
    return prediction_manager_.prediction_manager();
  }
  OptimizationGuideModelProvider& model_provider() {
    return *prediction_model_component_update_listener_;
  }
  PredictionModelComponentUpdateListener&
  prediction_model_component_update_listener() {
    return *prediction_model_component_update_listener_;
  }

 private:
  friend base::RefCounted<OptimizationGuideGlobalState>;
  friend OptimizationGuideGlobalStateTest;

  ~OptimizationGuideGlobalState();

  OptimizationGuideGlobalState();

  ChromePredictionManager prediction_manager_;

  // Registers the prediction model component for `target` with the component
  // updater.
  std::unique_ptr<PredictionModelComponentUpdateListener>
      prediction_model_component_update_listener_ =
          std::make_unique<PredictionModelComponentUpdateListener>(
              prediction_manager_.model_provider(),
              base::BindRepeating(&RegisterPredictionModelComponent));

  base::WeakPtrFactory<OptimizationGuideGlobalState> weak_ptr_factory_{this};
};

// This is a wrapper around OptimizationGuideGlobalState that keeps a reference
// to the global state. This is needed for these two reasons:
// 1. Some members of OptimizationGuideGlobalState create task runner, which
// necessitates the unittests to use the full TaskEnvironment instead of
// SingleThreadTaskEnvironment.
// 2. Profiles are destroyed after GlobalFeatures, at least in tests. So the
// OptimizationGuideKeyedService needs to keep a reference to the global state
// to keep it alive.
class OptimizationGuideGlobalFeature {
 public:
  OptimizationGuideGlobalFeature();
  ~OptimizationGuideGlobalFeature();

  OptimizationGuideGlobalState& Get();

  OptimizationGuideModelProvider& GetModelProvider();

 private:
  scoped_refptr<OptimizationGuideGlobalState> global_state_;
};

}  // namespace optimization_guide

#endif  // CHROME_BROWSER_OPTIMIZATION_GUIDE_MODEL_EXECUTION_OPTIMIZATION_GUIDE_GLOBAL_STATE_H_
