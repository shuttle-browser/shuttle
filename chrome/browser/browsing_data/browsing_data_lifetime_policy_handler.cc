// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/browsing_data_lifetime_policy_handler.h"

#include "base/strings/string_util.h"
#include "components/browsing_data/core/browsing_data_policies_utils.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_logger.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/strings/grit/components_strings.h"

BrowsingDataLifetimePolicyHandler::BrowsingDataLifetimePolicyHandler(
    const char* policy_name,
    const char* pref_path,
    policy::Schema schema)
    : policy::SimpleSchemaValidatingPolicyHandler(
          policy_name,
          pref_path,
          schema,
          policy::SchemaOnErrorStrategy::SCHEMA_ALLOW_UNKNOWN,
          SimpleSchemaValidatingPolicyHandler::RECOMMENDED_PROHIBITED,
          SimpleSchemaValidatingPolicyHandler::MANDATORY_ALLOWED),
      pref_path_(pref_path) {}

BrowsingDataLifetimePolicyHandler::~BrowsingDataLifetimePolicyHandler() =
    default;

bool BrowsingDataLifetimePolicyHandler::CheckPolicySettings(
    const policy::PolicyMap& policies,
    policy::PolicyErrorMap* errors) {
  if (!policy::SimpleSchemaValidatingPolicyHandler::CheckPolicySettings(
          policies, errors)) {
    return false;
  }

  const base::Value* browsing_data_policy =
      policies.GetValue(this->policy_name(), base::Value::Type::LIST);
  if (!browsing_data_policy) {
    return true;
  }

  const base::flat_set<std::string> unsupported_types =
      browsing_data::GetBrowsingDataLifetimePlatformUnsupportedTypes(
          *browsing_data_policy);
  if (!unsupported_types.empty()) {
    errors->AddError(this->policy_name(),
                     IDS_POLICY_BROWSING_DATA_PLATFORM_UNSUPPORTED,
                     base::JoinString(unsupported_types, ", "), {},
                     policy::PolicyMap::MessageType::kWarning);
  }

  return true;
}

void BrowsingDataLifetimePolicyHandler::ApplyPolicySettings(
    const policy::PolicyMap& policies,
    PrefValueMap* prefs) {
  const base::Value* browsing_data_policy =
      policies.GetValue(this->policy_name(), base::Value::Type::LIST);
  if (!browsing_data_policy) {
    return;
  }

  const base::flat_set<std::string> unsupported_types =
      browsing_data::GetBrowsingDataLifetimePlatformUnsupportedTypes(
          *browsing_data_policy);
  if (unsupported_types.empty()) {
    SimpleSchemaValidatingPolicyHandler::ApplyPolicySettings(policies, prefs);
  } else {
    // Make a copy of the policy value so as to remove unsupported types before
    // adding into prefs.
    base::Value filtered_policy_value = browsing_data_policy->Clone();
    for (auto& item : filtered_policy_value.GetList()) {
      base::ListValue& data_types =
          item.GetDict().Find("data_types")->GetList();
      data_types.erase(
          std::remove_if(data_types.begin(), data_types.end(),
                         [&unsupported_types](const base::Value& type) {
                           return unsupported_types.contains(type.GetString());
                         }),
          data_types.end());
    }
    prefs->SetValue(pref_path_, std::move(filtered_policy_value));
  }
}
