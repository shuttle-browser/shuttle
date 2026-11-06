// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/browser/webui/policy_status_provider.h"

#include <functional>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/containers/fixed_flat_map.h"
#include "base/containers/fixed_flat_set.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/i18n/time_formatting.h"
#include "base/no_destructor.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/policy/core/common/policy_logger.h"
#include "components/policy/resources/webui/mojom/policy.mojom-forward.h"
#include "components/policy/resources/webui/mojom/policy.mojom.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"


namespace policy {

namespace {

base::Clock* clock_for_testing_ = nullptr;

const base::Clock* GetClock() {
  if (clock_for_testing_) {
    return clock_for_testing_;
  }
  return base::DefaultClock::GetInstance();
}

}  // namespace

PolicyStatusProvider::PolicyStatusProvider() = default;

PolicyStatusProvider::~PolicyStatusProvider() = default;

void PolicyStatusProvider::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void PolicyStatusProvider::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

base::DictValue PolicyStatusProvider::GetStatus() {
  // This method is called when the client is not enrolled.
  // Thus return an empty dictionary.
  return base::DictValue();
}

policy::mojom::StatusPtr PolicyStatusProvider::GetStatusMojo() {
  return nullptr;
}

void PolicyStatusProvider::NotifyStatusChange() {
  for (auto& observer : observers_) {
    observer.OnPolicyStatusChanged();
  }
}

// static
std::u16string PolicyStatusProvider::GetTimeSinceLastActionString(
    base::Time last_action_time) {
  if (last_action_time.is_null()) {
    return l10n_util::GetStringUTF16(IDS_POLICY_NEVER_FETCHED);
  }
  base::Time now = GetClock()->Now();
  base::TimeDelta elapsed_time;
  if (now > last_action_time) {
    elapsed_time = now - last_action_time;
  }
  return ui::TimeFormat::Simple(ui::TimeFormat::FORMAT_ELAPSED,
                                ui::TimeFormat::LENGTH_SHORT, elapsed_time);
}

// static
base::ScopedClosureRunner PolicyStatusProvider::OverrideClockForTesting(
    base::Clock* clock_for_testing) {
  CHECK(!clock_for_testing_);
  clock_for_testing_ = clock_for_testing;
  return base::ScopedClosureRunner(
      base::BindOnce([]() { clock_for_testing_ = nullptr; }));
}

}  // namespace policy
