// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/realtime/policy_engine.h"

#include <algorithm>

#include "base/metrics/histogram_functions.h"
#include "build/build_config.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/safe_browsing/core/common/utils.h"
#include "components/unified_consent/pref_names.h"
#include "components/user_prefs/user_prefs.h"
#include "components/variations/service/variations_service.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/metrics/field_trial_params.h"
#include "base/system/sys_info.h"
#endif

namespace safe_browsing {

// static
bool RealTimePolicyEngine::IsInExcludedCountry(
    const std::string& country_code) {
  return std::ranges::contains(GetExcludedCountries(), country_code);
}

// static
bool RealTimePolicyEngine::IsUserMbbOptedIn(PrefService* pref_service) {
  return pref_service->GetBoolean(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled);
}

// static
bool RealTimePolicyEngine::IsUserEpOptedIn(PrefService* pref_service) {
  return IsEnhancedProtectionEnabled(*pref_service);
}

// static
bool RealTimePolicyEngine::CanPerformFullURLLookup(
    PrefService* pref_service,
    bool is_off_the_record,
    variations::VariationsService* variations_service) {
  if (is_off_the_record) {
    return false;
  }

  // |variations_service| can be nullptr during shutdown and in tests.
  if (variations_service &&
      IsInExcludedCountry(variations_service->GetLatestCountry())) {
    return false;
  }

  return HasPrefPermissionsToPerformFullURLLookup(pref_service);
}

// static
bool RealTimePolicyEngine::CanPerformFullURLLookupWithToken(
    PrefService* pref_service,
    bool is_off_the_record,
    ClientConfiguredForTokenFetchesCallback client_callback,
    variations::VariationsService* variations_service) {
  if (!CanPerformFullURLLookup(pref_service, is_off_the_record,
                               variations_service)) {
    return false;
  }

  return std::move(client_callback).Run(IsUserEpOptedIn(pref_service));
}

// static
bool RealTimePolicyEngine::HasPrefPermissionsToPerformFullURLLookup(
    PrefService* pref_service) {
  return IsUserEpOptedIn(pref_service) || IsUserMbbOptedIn(pref_service);
}

}  // namespace safe_browsing
