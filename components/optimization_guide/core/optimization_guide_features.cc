// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/optimization_guide_features.h"

#include <algorithm>
#include <optional>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/system/sys_info.h"
#include "base/time/time.h"

namespace optimization_guide::features {

BASE_FEATURE(kOverrideNumThreadsForModelExecution,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Whether to enable model execution.
BASE_FEATURE(kOptimizationGuideModelExecution,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Whether to scan the full text when running the language detection in the text
// safety classifier.
BASE_FEATURE(kTextSafetyScanLanguageDetection,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kOnDeviceModelPerformanceParams, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kAnnotatedPageContentWithActionableElements,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAnnotatedPageContentWithMediaData,
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<std::string> kPerformanceClassListForOnDeviceModel{
    &kOnDeviceModelPerformanceParams,
    "compatible_on_device_performance_classes", "3,4,5,6"};

const base::FeatureParam<std::string>
    kLowTierPerformanceClassListForOnDeviceModel{
        &kOnDeviceModelPerformanceParams,
        "compatible_low_tier_on_device_performance_classes", "3,4"};

const base::FeatureParam<std::string> kPerformanceClassListForImageInput{
    &kOnDeviceModelPerformanceParams,
    "compatible_on_device_performance_classes_image_input", "3,4,5,6"};

// Controls whether to enforce a timeout for subframe page content extraction.
// If enabled, defaults to 1 second. If disabled, wait indefinitely for all
// subframes to respond.
BASE_FEATURE(kGetAIPageContentSubframeTimeoutEnabled,
             base::FEATURE_ENABLED_BY_DEFAULT);
const base::FeatureParam<base::TimeDelta> kGetAIPageContentSubframeTimeoutParam{
    &kGetAIPageContentSubframeTimeoutEnabled, "timeout", base::Seconds(10)};

// Controls whether to enforce a timeout for main frame page content extraction.
// If enabled, defaults to 30 seconds. If disabled, wait indefinitely for the
// main frame to respond.
BASE_FEATURE(kGetAIPageContentMainFrameTimeoutEnabled,
             base::FEATURE_ENABLED_BY_DEFAULT);
const base::FeatureParam<base::TimeDelta>
    kGetAIPageContentMainFrameTimeoutParam{
        &kGetAIPageContentMainFrameTimeoutEnabled, "timeout",
        base::Seconds(30)};

// Controls whether to enforce a timeout for GetImageBytes. If enabled, defaults
// to 10 seconds.
BASE_FEATURE(kGetAIPageContentGetImageBytesTimeoutEnabled,
             base::FEATURE_ENABLED_BY_DEFAULT);
const base::FeatureParam<base::TimeDelta>
    kGetAIPageContentGetImageBytesTimeoutParam{
        &kGetAIPageContentGetImageBytesTimeoutEnabled, "timeout",
        base::Seconds(10)};

std::optional<int> OverrideNumThreadsForOptTarget(
    proto::OptimizationTarget opt_target) {
  if (!base::FeatureList::IsEnabled(kOverrideNumThreadsForModelExecution)) {
    return std::nullopt;
  }

  // 0 is an invalid value to pass to TFLite, so make that nullopt. -1 is valid,
  // but not anything less than that.
  int num_threads = GetFieldTrialParamByFeatureAsInt(
      kOverrideNumThreadsForModelExecution,
      proto::OptimizationTarget_Name(opt_target), 0);
  if (num_threads == 0 || num_threads < -1) {
    return std::nullopt;
  }

  // Cap to the number of CPUs on the device.
  return std::min(num_threads, base::SysInfo::NumberOfProcessors());
}

std::optional<base::TimeDelta> GetSubframeGetAIPageContentTimeout() {
  if (!base::FeatureList::IsEnabled(kGetAIPageContentSubframeTimeoutEnabled)) {
    return std::nullopt;
  }
  return kGetAIPageContentSubframeTimeoutParam.Get();
}

std::optional<base::TimeDelta> GetMainFrameGetAIPageContentTimeout() {
  if (!base::FeatureList::IsEnabled(kGetAIPageContentMainFrameTimeoutEnabled)) {
    return std::nullopt;
  }
  return kGetAIPageContentMainFrameTimeoutParam.Get();
}
std::optional<base::TimeDelta> GetAIPageContentGetImageBytesTimeout() {
  if (!base::FeatureList::IsEnabled(
          kGetAIPageContentGetImageBytesTimeoutEnabled)) {
    return std::nullopt;
  }
  return kGetAIPageContentGetImageBytesTimeoutParam.Get();
}

}  // namespace optimization_guide::features
