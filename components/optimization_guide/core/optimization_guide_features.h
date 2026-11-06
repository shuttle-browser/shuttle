// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_GUIDE_FEATURES_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_GUIDE_FEATURES_H_

// Guidelines for adding new features:
// 1. Don't add new features in this file. Put them somewhere near the thing
//    they control or the interface they affect the behavior of.
// 2. Features should be declared with a comment indicating what they are for
//    and some reference that can be used to decide when they are no longer
//    needed.
//     a. Rollout gates should link a tracker bug for the rollout.
//     b. Killswitches should link a playbook describing their usage scenarios.
//     c. Speculative parameters and debugging flags should indicate that
//        status, so they can be removed whenever they are inconvenient to keep.
// 3. Parameters should not be added to existing features. In general, it's
//    better to make an independent Feature for each parameter.
// 4. See //docs/flag_guarding_guidelines.md and //docs/configuration.md for
//    general best practices and advice for the Chromium repo.

// Shuttle: optimization_guide is removed except for the page-content carve-out
// (CDP Page.getAnnotatedPageContent) and a few constants still read by kept
// code (on-device model service, echo AI, language detection, TFLite op
// resolver). Only those remain here.

#include <optional>
#include <string>

#include "base/component_export.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"
#include "components/optimization_guide/proto/models.pb.h"

namespace optimization_guide {

namespace features {

COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
BASE_DECLARE_FEATURE(kOverrideNumThreadsForModelExecution);
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
BASE_DECLARE_FEATURE(kOptimizationGuideModelExecution);
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
BASE_DECLARE_FEATURE(kTextSafetyScanLanguageDetection);
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
BASE_DECLARE_FEATURE(kAnnotatedPageContentWithActionableElements);
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
BASE_DECLARE_FEATURE(kAnnotatedPageContentWithMediaData);

// Feature for controlling the on-device model performance class params.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
BASE_DECLARE_FEATURE(kOnDeviceModelPerformanceParams);

// Comma-separated list of performance classes that are compatible with the
// on-device model.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const base::FeatureParam<std::string>
    kPerformanceClassListForOnDeviceModel;

// Comma-separated list of performance classes that are low tier.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const base::FeatureParam<std::string>
    kLowTierPerformanceClassListForOnDeviceModel;

// Comma-separated list of performance classes that support image input.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const base::FeatureParam<std::string> kPerformanceClassListForImageInput;

COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
BASE_DECLARE_FEATURE(kGetAIPageContentSubframeTimeoutEnabled);
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const base::FeatureParam<base::TimeDelta>
    kGetAIPageContentSubframeTimeoutParam;

COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
BASE_DECLARE_FEATURE(kGetAIPageContentMainFrameTimeoutEnabled);
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const base::FeatureParam<base::TimeDelta>
    kGetAIPageContentMainFrameTimeoutParam;

COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
BASE_DECLARE_FEATURE(kGetAIPageContentGetImageBytesTimeoutEnabled);
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const base::FeatureParam<base::TimeDelta>
    kGetAIPageContentGetImageBytesTimeoutParam;

// Returns the number of threads to use for model inference on the given
// optimization target.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
std::optional<int> OverrideNumThreadsForOptTarget(
    proto::OptimizationTarget opt_target);

// Default/max sampling parameters for on-device model sessions.
inline constexpr int GetOnDeviceModelDefaultTopK() {
  return 64;
}
inline constexpr int GetOnDeviceModelMaxTopK() {
  return 128;
}
inline constexpr double GetOnDeviceModelDefaultTemperature() {
  return 1.0;
}

// Timeout for subframe page content extraction, or nullopt to wait
// indefinitely.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
std::optional<base::TimeDelta> GetSubframeGetAIPageContentTimeout();

// Timeout for main frame page content extraction, or nullopt to wait
// indefinitely.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
std::optional<base::TimeDelta> GetMainFrameGetAIPageContentTimeout();

// Timeout for GetImageBytes, or nullopt to wait indefinitely.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
std::optional<base::TimeDelta> GetAIPageContentGetImageBytesTimeout();

}  // namespace features
}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_GUIDE_FEATURES_H_
