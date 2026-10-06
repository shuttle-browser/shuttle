// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/unified_consent/url_keyed_data_collection_consent_helper.h"

#include <algorithm>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/unified_consent/pref_names.h"

namespace unified_consent {

namespace {

class PrefBasedUrlKeyedDataCollectionConsentHelper
    : public UrlKeyedDataCollectionConsentHelper {
 public:
  explicit PrefBasedUrlKeyedDataCollectionConsentHelper(
      PrefService* pref_service);

  PrefBasedUrlKeyedDataCollectionConsentHelper(
      const PrefBasedUrlKeyedDataCollectionConsentHelper&) = delete;
  PrefBasedUrlKeyedDataCollectionConsentHelper& operator=(
      const PrefBasedUrlKeyedDataCollectionConsentHelper&) = delete;

  ~PrefBasedUrlKeyedDataCollectionConsentHelper() override = default;

  // UrlKeyedDataCollectionConsentHelper:
  State GetConsentState() override;

 private:
  void OnPrefChanged();
  raw_ptr<PrefService> pref_service_;  // weak (must outlive this)
  PrefChangeRegistrar pref_change_registrar_;
};

PrefBasedUrlKeyedDataCollectionConsentHelper::
    PrefBasedUrlKeyedDataCollectionConsentHelper(PrefService* pref_service)
    : pref_service_(pref_service) {
  pref_change_registrar_.Init(pref_service_);
  pref_change_registrar_.Add(
      prefs::kUrlKeyedAnonymizedDataCollectionEnabled,
      base::BindRepeating(
          &PrefBasedUrlKeyedDataCollectionConsentHelper::OnPrefChanged,
          base::Unretained(this)));
}

UrlKeyedDataCollectionConsentHelper::State
PrefBasedUrlKeyedDataCollectionConsentHelper::GetConsentState() {
  // There's no initializing state for pref-based helpers.
  return pref_service_->GetBoolean(
             prefs::kUrlKeyedAnonymizedDataCollectionEnabled)
             ? State::kEnabled
             : State::kDisabled;
}

void PrefBasedUrlKeyedDataCollectionConsentHelper::OnPrefChanged() {
  FireOnStateChanged();
}

}  // namespace

UrlKeyedDataCollectionConsentHelper::UrlKeyedDataCollectionConsentHelper() =
    default;
UrlKeyedDataCollectionConsentHelper::~UrlKeyedDataCollectionConsentHelper() =
    default;

// static
std::unique_ptr<UrlKeyedDataCollectionConsentHelper>
UrlKeyedDataCollectionConsentHelper::NewAnonymizedDataCollectionConsentHelper(
    PrefService* pref_service) {
  return std::make_unique<PrefBasedUrlKeyedDataCollectionConsentHelper>(
      pref_service);
}

// static
bool UrlKeyedDataCollectionConsentHelper::IsEnabled() {
  return GetConsentState() == State::kEnabled;
}

void UrlKeyedDataCollectionConsentHelper::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}
void UrlKeyedDataCollectionConsentHelper::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void UrlKeyedDataCollectionConsentHelper::FireOnStateChanged() {
  for (auto& observer : observer_list_)
    observer.OnUrlKeyedDataCollectionConsentStateChanged(this);
}

}  // namespace unified_consent
