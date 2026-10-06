// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSING_DATA_CORE_BROWSING_DATA_POLICIES_UTILS_H_
#define COMPONENTS_BROWSING_DATA_CORE_BROWSING_DATA_POLICIES_UTILS_H_

#include "base/containers/flat_set.h"
#include "base/values.h"
#include "components/prefs/pref_value_map.h"

namespace browsing_data {

// The data types of the BrowsingDataSettings policy.
enum class PolicyDataType {
  kBrowsingHistory = 0,
  kPasswordSignin = 1,
  kAutofill = 2,
  kSiteSettings = 3,
  kHostedAppData = 4,
  kDownloadHistory = 5,
  kCookiesAndOtherSiteData = 6,
  kCachedImagesAndFiles = 7,
  kNumTypes = kCachedImagesAndFiles + 1
};

// Returns a list of data types that are not supported on the current platform.
base::flat_set<std::string> GetBrowsingDataLifetimePlatformUnsupportedTypes(
    const base::Value& policy_value);

// Converts the browsing data type string to its integer value.
// The conversion is used to ensure that the number of policy data types that
// can be managed at any point in time is known.
std::optional<PolicyDataType> NameToPolicyDataType(const std::string& type);

}  // namespace browsing_data
#endif  // COMPONENTS_BROWSING_DATA_CORE_BROWSING_DATA_POLICIES_UTILS_H_
