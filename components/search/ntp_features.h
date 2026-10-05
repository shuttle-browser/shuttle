// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEARCH_NTP_FEATURES_H_
#define COMPONENTS_SEARCH_NTP_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace ntp_features {

// The features should be documented alongside the definition of their values in
// the .cc file.

BASE_DECLARE_FEATURE(kEnergyEffect);
BASE_DECLARE_FEATURE(kRealboxCr23Theming);
BASE_DECLARE_FEATURE(kNtpDriveModuleSegmentation);
BASE_DECLARE_FEATURE(kNtpNextFeatures);
BASE_DECLARE_FEATURE(kNtpShortcutsRedesign);
BASE_DECLARE_FEATURE(kNtpSimplificationBookmarkBar);

// Parameters of kRealboxCr23Theming.
extern const base::FeatureParam<bool> kNtpRealboxCr23ExpandedStateBgMatchesOmnibox;
extern const base::FeatureParam<bool> kNtpRealboxCr23SteadyStateShadow;

// Parameters of kNtpShortcutsRedesign.
extern const base::FeatureParam<int> kMaxShortcutsInExpandedState;
extern const base::FeatureParam<int> kMaxMostVisitedTilesInExpandedState;

// Returns the maximum number of shortcuts shown in the expanded state.
int GetMaxShortcutsInExpandedState();

// Returns the maximum number of most visited tiles shown in the expanded state.
int GetMaxMostVisitedTilesInExpandedState();

}  // namespace ntp_features

#endif  // COMPONENTS_SEARCH_NTP_FEATURES_H_
