// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_FEATURE_REGISTRY_FEATURE_REGISTRATION_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_FEATURE_REGISTRY_FEATURE_REGISTRATION_H_

#include "base/component_export.h"

class PrefRegistrySimple;
namespace optimization_guide {

namespace prefs {
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kComposeEnterprisePolicyAllowed[];
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kWallpaperSearchEnterprisePolicyAllowed[];
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kHistorySearchEnterprisePolicyAllowed[];
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kAutomatedPasswordChangeEnterprisePolicyAllowed[];
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kProductSpecificationsEnterprisePolicyAllowed[];
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kAutofillPredictionImprovementsEnterprisePolicyAllowed[];
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kBlingPrototypingEnterprisePolicyAllowed[];
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kContextualTasksContextEnterprisePolicyAllowed[];
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kFindsEnterprisePolicyAllowed[];
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kChromeSuggestionsSettings[];
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kGeminiSettings[];
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kFindAndFillWithGeminiSettings[];

// Values for the `kGeminiSettings` pref. Integer pref that determines
// the Gemini enabling state for this user profile. This is controlled by
// enterprise policy.
enum class GeminiSettingsPolicyState {
  kMinValue = 0,

  kEnabled = kMinValue,
  kDisabled = 1,

  kMaxValue = kDisabled
};
}  // namespace prefs


COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
void RegisterGenAiFeatures(PrefRegistrySimple* registry);

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_FEATURE_REGISTRY_FEATURE_REGISTRATION_H_
