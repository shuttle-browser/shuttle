// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/feature_registry/feature_registration.h"

#include "components/prefs/pref_registry_simple.h"

// Shuttle: model execution, MQLS logging and the AI settings registries are
// removed. Only the GenAI policy/settings prefs remain (policies and settings
// pages still read them); each is registered with its old default (0 = allow).

namespace optimization_guide {

namespace prefs {


const char kComposeEnterprisePolicyAllowed[] =
    "optimization_guide.model_execution.compose_enterprise_policy_allowed";

const char kWallpaperSearchEnterprisePolicyAllowed[] =
    "optimization_guide.model_execution.wallpaper_search_enterprise_policy_"
    "allowed";

const char kHistorySearchEnterprisePolicyAllowed[] =
    "optimization_guide.model_execution.history_search_"
    "enterprise_policy_allowed";

const char kProductSpecificationsEnterprisePolicyAllowed[] =
    "optimization_guide.model_execution.tab_compare_settings_enterprise_policy";

const char kAutofillPredictionImprovementsEnterprisePolicyAllowed[] =
    "optimization_guide.model_execution.autofill_prediction_improvements_"
    "enterprise_policy_allowed";

const char kAutomatedPasswordChangeEnterprisePolicyAllowed[] =
    "optimization_guide.model_execution.password_change_submission_"
    "enterprise_policy_allowed";

const char kNotificationContentDetectionEnterprisePolicyAllowed[] =
    "optimization_guide.model_execution.notification_content_detection_"
    "enterprise_policy_allowed";

const char kBlingPrototypingEnterprisePolicyAllowed[] =
    "optimization_guide.model_execution.bling_prototyping_enterprise_policy_"
    "allowed";

const char kContextualTasksContextEnterprisePolicyAllowed[] =
    "optimization_guide.model_execution.contextual_tasks_context_enterprise_"
    "policy_allowed";

const char kGeminiAntiscamProtectionEnterprisePolicyAllowed[] =
    "optimization_guide.model_execution.gemini_antiscam_protection_enterprise_"
    "policy_allowed";

const char kFindsEnterprisePolicyAllowed[] =
    "optimization_guide.model_execution.finds_enterprise_policy_allowed";

const char kChromeSuggestionsSettings[] =
    "contextual_cueing.chrome_suggestions_settings";

const char kGeminiSettings[] = "browser.gemini_settings";

// LINT.IfChange(FindAndFillWithGeminiSettings)
const char kFindAndFillWithGeminiSettings[] =
    "autofill.personal_context.find_and_fill_with_gemini_settings";
// LINT.ThenChange(//chrome/browser/ui/android/autofill/internal/java/src/org/chromium/chrome/browser/ui/autofill/AtMemoryBottomSheetMediator.java:FindAndFillWithGeminiSettings)

}  // namespace prefs

void RegisterGenAiFeatures(PrefRegistrySimple* pref_registry) {
  pref_registry->RegisterIntegerPref(prefs::kComposeEnterprisePolicyAllowed, 0, PrefRegistry::LOSSY_PREF);
  pref_registry->RegisterIntegerPref(prefs::kWallpaperSearchEnterprisePolicyAllowed, 0, PrefRegistry::LOSSY_PREF);
  pref_registry->RegisterIntegerPref(prefs::kHistorySearchEnterprisePolicyAllowed, 0, PrefRegistry::LOSSY_PREF);
  pref_registry->RegisterIntegerPref(prefs::kAutomatedPasswordChangeEnterprisePolicyAllowed, 0, PrefRegistry::LOSSY_PREF);
  pref_registry->RegisterIntegerPref(prefs::kProductSpecificationsEnterprisePolicyAllowed, 0, PrefRegistry::LOSSY_PREF);
  pref_registry->RegisterIntegerPref(prefs::kAutofillPredictionImprovementsEnterprisePolicyAllowed, 0, PrefRegistry::LOSSY_PREF);
  pref_registry->RegisterIntegerPref(prefs::kNotificationContentDetectionEnterprisePolicyAllowed, 0, PrefRegistry::LOSSY_PREF);
  pref_registry->RegisterIntegerPref(prefs::kBlingPrototypingEnterprisePolicyAllowed, 0, PrefRegistry::LOSSY_PREF);
  pref_registry->RegisterIntegerPref(prefs::kContextualTasksContextEnterprisePolicyAllowed, 0, PrefRegistry::LOSSY_PREF);
  pref_registry->RegisterIntegerPref(prefs::kGeminiAntiscamProtectionEnterprisePolicyAllowed, 0, PrefRegistry::LOSSY_PREF);
  pref_registry->RegisterIntegerPref(prefs::kFindsEnterprisePolicyAllowed, 0, PrefRegistry::LOSSY_PREF);
  pref_registry->RegisterIntegerPref(prefs::kChromeSuggestionsSettings, 0, PrefRegistry::LOSSY_PREF);
  pref_registry->RegisterIntegerPref(prefs::kFindAndFillWithGeminiSettings, 0, PrefRegistry::LOSSY_PREF);
}

}  // namespace optimization_guide
