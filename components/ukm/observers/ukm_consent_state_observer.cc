// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ukm/observers/ukm_consent_state_observer.h"

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "components/metrics/metrics_features.h"
#include "components/metrics/metrics_profile_pref_names.h"
#include "components/metrics/metrics_switches.h"
#include "components/prefs/pref_service.h"
#include "components/unified_consent/pref_names.h"
#include "components/unified_consent/url_keyed_data_collection_consent_helper.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/components/kiosk/kiosk_utils.h"  // nogncheck
#include "chromeos/components/mgs/managed_guest_session_utils.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

using unified_consent::UrlKeyedDataCollectionConsentHelper;

namespace ukm {
namespace {

bool IsMsbbConsentStateAllowed() {
#if BUILDFLAG(IS_CHROMEOS)
  // MGS should report only AppKM metrics.
  return !chromeos::IsManagedGuestSession();
#else  // !BUILDFLAG(IS_CHROMEOS)
  return true;
#endif
}

}  // namespace

UkmConsentStateObserver::UkmConsentStateObserver()
    : ukm_consent_state_(UkmConsentState()) {}

UkmConsentStateObserver::UkmConsentStateObserver(NoInitialUkmConsentStateTag)
    : ukm_consent_state_(std::nullopt) {}

UkmConsentStateObserver::~UkmConsentStateObserver() {
  for (const auto& entry : consent_helpers_) {
    entry.second->RemoveObserver(this);
  }
}

bool UkmConsentStateObserver::ProfileState::IsUkmConsented() const {
#if BUILDFLAG(IS_CHROMEOS)
  return consent_state.Has(MSBB) || consent_state.Has(APPS);
#else
  return consent_state.Has(MSBB);
#endif
}

void UkmConsentStateObserver::ProfileState::SetConsentType(
    UkmConsentType type) {
  consent_state.Put(type);
}

UkmConsentStateObserver::ProfileState UkmConsentStateObserver::GetProfileState(
    UrlKeyedDataCollectionConsentHelper* consent_helper) {
  DCHECK(consent_helper);
  ProfileState state;

  const bool msbb_consent =
      consent_helper->IsEnabled() || metrics::IsMsbbSettingForcedOnForUkm();

  if (msbb_consent && IsMsbbConsentStateAllowed()) {
    state.SetConsentType(MSBB);
  }

  // Extension and app consent require sync of the corresponding data types,
  // which does not exist in this browser, so they are never granted.
#if BUILDFLAG(IS_CHROMEOS)
  // Demo mode is a special managed guest session that doesn't support
  // AppKM. To support AppKM an exception needs to be made within UKM.
  if (IsDeviceInDemoMode()) {
    state.SetConsentType(APPS);
  }
#endif

  return state;
}

void UkmConsentStateObserver::StartObserving(PrefService* prefs) {
  bool msbb_enabled = prefs->GetBoolean(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled);
  // Without sync, advanced reporting is just MSBB.
  bool advanced_reporting_enabled = msbb_enabled;

  if (base::FeatureList::IsEnabled(
          metrics::features::kRestructureMetricsConsentSettings)) {
    if (!prefs->GetBoolean(
            metrics::prefs::kAdvancedReportingProfileMigrationDone)) {
      prefs->SetBoolean(metrics::prefs::kAdvancedReportingEnabled,
                        advanced_reporting_enabled);
      prefs->SetBoolean(metrics::prefs::kAdvancedReportingProfileMigrationDone,
                        true);
    }
  }

  std::unique_ptr<UrlKeyedDataCollectionConsentHelper> consent_helper =
      UrlKeyedDataCollectionConsentHelper::
          NewAnonymizedDataCollectionConsentHelper(prefs);

  ProfileState state = GetProfileState(consent_helper.get());
  previous_states_[prefs] = state;

  consent_helper->AddObserver(this);
  consent_helpers_[prefs] = std::move(consent_helper);
  UpdateUkmAllowedForAllProfiles(/*total_purge*/ false);
}

void UkmConsentStateObserver::StopObserving(PrefService* prefs) {
  auto found = consent_helpers_.find(prefs);
  if (found == consent_helpers_.end()) {
    return;
  }
  found->second->RemoveObserver(this);
  consent_helpers_.erase(found);
  previous_states_.erase(prefs);
  UpdateUkmAllowedForAllProfiles(/*total_purge=*/false);
}

void UkmConsentStateObserver::UpdateUkmAllowedForAllProfiles(bool total_purge) {
  const UkmConsentState new_state = GetPreviousStatesForAllProfiles();

  // Any change in profile states needs to call OnUkmAllowedStateChanged so that
  // the new settings take effect. If ukm_consent_state_ is std::nullopt (i.e.
  // no profile loaded yet), this will always be considered as a change.
  if (total_purge || new_state != ukm_consent_state_) {
    if (ukm_consent_state_.has_value()) {
      // Records whether the App sync consent changed when the consent state is
      // updated. This is to see how often App sync is changed by users.
      base::UmaHistogramBoolean(
          "UKM.ConsentObserver.AppSyncConsentChanged",
          ukm_consent_state_->Has(APPS) != new_state.Has(APPS));
    }

    const auto previous_consent_state = GetUkmConsentState();
    ukm_consent_state_ = new_state;
    OnUkmAllowedStateChanged(total_purge, previous_consent_state);
  }
}

UkmConsentState UkmConsentStateObserver::GetPreviousStatesForAllProfiles() {
  // No profiles are being observed, no consent is possible.
  if (previous_states_.empty()) {
    return UkmConsentState();
  }

  // Consent for each type must be given by all profiles for metrics of that
  // type to be collected. See components/ukm/ukm_consent_state.h for details.
  // Performs an AND over all of the profiles' consents states in
  // |profile_states_|. Must assume all consent types are granted for the
  // AND operation to work as expected.
  auto state = UkmConsentState::All();
  for (const auto& kv : previous_states_) {
    const ProfileState& profile = kv.second;
    state = base::Intersection(state, profile.consent_state);
  }

  return state;
}

void UkmConsentStateObserver::OnUrlKeyedDataCollectionConsentStateChanged(
    unified_consent::UrlKeyedDataCollectionConsentHelper* consent_helper) {
  DCHECK(consent_helper);
  PrefService* prefs = nullptr;
  for (const auto& entry : consent_helpers_) {
    if (consent_helper == entry.second.get()) {
      prefs = entry.first;
      break;
    }
  }
  DCHECK(prefs);
  UpdateProfileState(prefs, consent_helper);
}

void UkmConsentStateObserver::UpdateProfileState(
    PrefService* prefs,
    UrlKeyedDataCollectionConsentHelper* consent_helper) {
  DCHECK(previous_states_.contains(prefs));
  const ProfileState& previous_state = previous_states_[prefs];
  DCHECK(consent_helper);
  ProfileState state = GetProfileState(consent_helper);

  // Trigger a total purge of all local UKM data if the current state no longer
  // allows recording of UKMs.
  bool total_purge = previous_state.IsUkmConsented() && !state.IsUkmConsented();

  base::UmaHistogramBoolean("UKM.ConsentObserver.Purge", total_purge);

  previous_states_[prefs] = state;
  UpdateUkmAllowedForAllProfiles(total_purge);
}

bool UkmConsentStateObserver::IsUkmAllowedForAllProfiles() {
  const UkmConsentState ukm_consent_state = GetUkmConsentState();
#if BUILDFLAG(IS_CHROMEOS)
  return ukm_consent_state.Has(MSBB) || ukm_consent_state.Has(APPS);
#else
  return ukm_consent_state.Has(MSBB);
#endif
}

// TODO(crbug.com/394931297): Add a comment in UkmConsentStateObserver header to
// explain why it has IsDwaAllowedForAllProfiles.
bool UkmConsentStateObserver::IsDwaAllowedForAllProfiles() {
  const UkmConsentState ukm_consent_state = GetUkmConsentState();
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  return ukm_consent_state.Has(ukm::MSBB);
#else
  return ukm_consent_state.HasAll({ukm::MSBB, ukm::APPS, ukm::EXTENSIONS});
#endif
}

UkmConsentState UkmConsentStateObserver::GetUkmConsentState() {
  // Consider that the state is empty if it is std::nullopt (the not set
  // state is only relevant to detect when the first profile is loaded).
  return ukm_consent_state_.value_or(UkmConsentState());
}

#if BUILDFLAG(IS_CHROMEOS)
void UkmConsentStateObserver::SetIsDemoMode(bool is_device_in_demo_mode) {
  is_device_in_demo_mode_ = is_device_in_demo_mode;
}

bool UkmConsentStateObserver::IsDeviceInDemoMode() {
  return is_device_in_demo_mode_;
}
#endif

}  // namespace ukm
