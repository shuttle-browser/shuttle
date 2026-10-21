// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search/ntp_features.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace ntp_features {

// If enabled, the EnergyEffect for Realbox will be shown.
BASE_FEATURE(kEnergyEffect, base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, NTP "realbox" will be themed for CR23. Includes realbox
// matching omnibox theme and increased realbox shadow.
BASE_FEATURE(kRealboxCr23Theming,
             "NtpRealboxCr23Theming",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, segmentation data will be collected to decide whether or not to
// show the Drive module.
BASE_FEATURE(kNtpDriveModuleSegmentation, base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, the features of NTP Next (AI action chips etc.) will be shown.
BASE_FEATURE(kNtpNextFeatures, base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, the shortcuts will be redesigned.
BASE_FEATURE(kNtpShortcutsRedesign, base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, the bookmark bar may be auto-removed on the NTP and new
// visibility settings are added.
BASE_FEATURE(kNtpSimplificationBookmarkBar, base::FEATURE_ENABLED_BY_DEFAULT);

const base::FeatureParam<bool> kNtpRealboxCr23ExpandedStateBgMatchesOmnibox(
    &ntp_features::kRealboxCr23Theming,
    "kNtpRealboxCr23ExpandedStateBgMatchesOmnibox",
    true);
const base::FeatureParam<bool> kNtpRealboxCr23SteadyStateShadow(
    &ntp_features::kRealboxCr23Theming,
    "kNtpRealboxCr23SteadyStateShadow",
    false);

const base::FeatureParam<int> kMaxShortcutsInExpandedState{
    &ntp_features::kNtpShortcutsRedesign, "max_shortcuts_in_expanded_state",
    20};
const base::FeatureParam<int> kMaxMostVisitedTilesInExpandedState{
    &ntp_features::kNtpShortcutsRedesign,
    "max_most_visited_tiles_in_expanded_state", 10};

int GetMaxShortcutsInExpandedState() {
  if (!base::FeatureList::IsEnabled(kNtpShortcutsRedesign)) {
    return 10;
  }
  return kMaxShortcutsInExpandedState.Get();
}

int GetMaxMostVisitedTilesInExpandedState() {
  if (!base::FeatureList::IsEnabled(kNtpShortcutsRedesign)) {
    return 8;
  }
  return kMaxMostVisitedTilesInExpandedState.Get();
}

}  // namespace ntp_features
