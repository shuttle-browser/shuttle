// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data/core/browsing_data_policies_utils.h"

#include <algorithm>
#include <forward_list>

#include "base/containers/fixed_flat_map.h"
#include "base/containers/span.h"
#include "components/browsing_data/core/browsing_data_utils.h"
#include "components/browsing_data/core/pref_names.h"

namespace browsing_data {

namespace {

namespace policy_data_types {
constexpr char kBrowsingHistoryName[] = "browsing_history";
constexpr char kPasswordSigninName[] = "password_signin";
constexpr char kAutofillName[] = "autofill";
constexpr char kSiteSettingsName[] = "site_settings";
constexpr char kHostedAppDataName[] = "hosted_app_data";
constexpr char kDownloadHistoryName[] = "download_history";
constexpr char kCookiesAndOtherSiteDataName[] = "cookies_and_other_site_data";
constexpr char kCachedImagesAndFilesName[] = "cached_images_and_files";
}  // namespace policy_data_types

}  // namespace

base::flat_set<std::string> GetBrowsingDataLifetimePlatformUnsupportedTypes(
    const base::Value& policy_value) {
#if !BUILDFLAG(IS_ANDROID)
  return base::flat_set<std::string>();
#else
  // The use of GetList() and GetDict() without type checking are safe because
  // this function is only called if the policy schema is valid.
  // On Android, hosted_app_data and download_history are not supported for the
  // BrowsingDataLifetime policy, while the ClearBrowsingDataOnExit policy is
  // not supported at all.
  static constexpr std::string_view kUnsupportedTypes[] = {
      policy_data_types::kHostedAppDataName,
      policy_data_types::kDownloadHistoryName};
  std::forward_list<std::string> unsupported_types;
  const auto& items = policy_value.GetList();
  for (const auto& item : items) {
    const base::ListValue& data_types =
        item.GetDict().Find("data_types")->GetList();
    std::for_each(data_types.begin(), data_types.end(),
                  [&unsupported_types](const base::Value& type) {
                    const std::string& type_string = type.GetString();
                    if (std::ranges::contains(kUnsupportedTypes, type_string)) {
                      unsupported_types.push_front(type_string);
                    }
                  });
  }
  return base::flat_set<std::string>(unsupported_types.begin(),
                                     unsupported_types.end());
#endif  // !BUILDFLAG(IS_ANDROID)
}

std::optional<PolicyDataType> NameToPolicyDataType(
    const std::string& type_name) {
  static constexpr auto kNameToDataType =
      base::MakeFixedFlatMap<std::string_view, PolicyDataType>({
          {policy_data_types::kBrowsingHistoryName,
           PolicyDataType::kBrowsingHistory},
          {policy_data_types::kPasswordSigninName,
           PolicyDataType::kPasswordSignin},
          {policy_data_types::kAutofillName, PolicyDataType::kAutofill},
          {policy_data_types::kSiteSettingsName, PolicyDataType::kSiteSettings},
          {policy_data_types::kHostedAppDataName,
           PolicyDataType::kHostedAppData},
          {policy_data_types::kDownloadHistoryName,
           PolicyDataType::kDownloadHistory},
          {policy_data_types::kCookiesAndOtherSiteDataName,
           PolicyDataType::kCookiesAndOtherSiteData},
          {policy_data_types::kCachedImagesAndFilesName,
           PolicyDataType::kCachedImagesAndFiles},
      });

  const auto it = kNameToDataType.find(type_name);
  if (it == kNameToDataType.end()) {
    return std::nullopt;
  }
  return it->second;
}

}  // namespace browsing_data
