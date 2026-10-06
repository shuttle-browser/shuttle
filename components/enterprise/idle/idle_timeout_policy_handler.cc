// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/idle/idle_timeout_policy_handler.h"

#include <string>

#include "base/json/values_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/enterprise/idle/action_type.h"
#include "components/enterprise/idle/idle_pref_names.h"
#include "components/policy/core/browser/configuration_policy_handler.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_logger.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/schema.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/strings/grit/components_strings.h"

namespace enterprise_idle {

namespace {

// If `other_policy_name` is unset, adds an error to `errors` and returns false.
bool CheckOtherPolicySet(const policy::PolicyMap& policies,
                         const std::string& this_policy_name,
                         const std::string& other_policy_name,
                         policy::PolicyErrorMap* errors) {
  if (policies.GetValueUnsafe(other_policy_name)) {
    return true;
  }

  errors->AddError(this_policy_name, IDS_POLICY_DEPENDENCY_ERROR_ANY_VALUE,
                   other_policy_name);
  return false;
}

bool CheckPolicyScopeSupported(const policy::PolicyMap& policies,
                               const std::string& policy_name,
                               policy::PolicyErrorMap* errors) {
// The policies will not be supported as user policies on iOS until we clear
// data on sign out for managed users which requires new UI.
#if BUILDFLAG(IS_IOS)
  bool is_user_policy =
      policies.Get(policy_name)->scope == policy::POLICY_SCOPE_USER;
  if (is_user_policy) {
    errors->AddError(policy_name,
                     IDS_POLICY_NOT_SUPPORTED_AS_USER_POLICY_ON_IOS);
  }
  return !is_user_policy;
#else
  // Return true on all other platforms.
  return true;
#endif  // BUILDFLAG(IS_IOS)
}

}  // namespace

IdleTimeoutPolicyHandler::IdleTimeoutPolicyHandler()
    : policy::IntRangePolicyHandler(policy::key::kIdleTimeout,
                                    prefs::kIdleTimeout,
                                    1,
                                    INT_MAX,
                                    true) {}

IdleTimeoutPolicyHandler::~IdleTimeoutPolicyHandler() = default;

void IdleTimeoutPolicyHandler::ApplyPolicySettings(
    const policy::PolicyMap& policies,
    PrefValueMap* prefs) {
  const base::Value* value =
      policies.GetValue(policy_name(), base::Value::Type::INTEGER);
  DCHECK(value);

  // Apply a minimum of 1.
  base::TimeDelta time_delta = base::Minutes(std::max(value->GetInt(), 1));
  prefs->SetValue(prefs::kIdleTimeout, base::TimeDeltaToValue(time_delta));
}

bool IdleTimeoutPolicyHandler::CheckPolicySettings(
    const policy::PolicyMap& policies,
    policy::PolicyErrorMap* errors) {
  // Nothing to do if unset.
  if (!policies.GetValueUnsafe(policy_name())) {
    return false;
  }

  // Check that it's an integer, and that it's >= 1.
  if (!policy::IntRangePolicyHandler::CheckPolicySettings(policies, errors)) {
    return false;
  }

  // If IdleTimeoutActions is unset, add an error and do nothing.
  if (!CheckOtherPolicySet(policies, policy_name(),
                           policy::key::kIdleTimeoutActions, errors)) {
    return false;
  }

  if (!CheckPolicyScopeSupported(policies, policy_name(), errors)) {
    return false;
  }

  return true;
}

IdleTimeoutActionsPolicyHandler::IdleTimeoutActionsPolicyHandler(
    policy::Schema schema)
    : policy::SchemaValidatingPolicyHandler(
          policy::key::kIdleTimeoutActions,
          schema.GetKnownProperty(policy::key::kIdleTimeoutActions),
          policy::SCHEMA_ALLOW_UNKNOWN_AND_INVALID_LIST_ENTRY) {}

IdleTimeoutActionsPolicyHandler::~IdleTimeoutActionsPolicyHandler() = default;

void IdleTimeoutActionsPolicyHandler::ApplyPolicySettings(
    const policy::PolicyMap& policies,
    PrefValueMap* prefs) {
  const base::Value* policy_value =
      policies.GetValue(policy_name(), base::Value::Type::LIST);
  DCHECK(policy_value);

  // Convert strings to integers (from the ActionType enum).
  base::ListValue converted_actions;
  for (const base::Value& action : policy_value->GetList()) {
    if (!action.is_string()) {
      continue;
    }
    if (std::optional<ActionType> action_type =
            NameToActionType(action.GetString())) {
      converted_actions.Append(static_cast<int>(action_type.value()));
    }
  }
  prefs->SetValue(prefs::kIdleTimeoutActions,
                  base::Value(std::move(converted_actions)));

#if BUILDFLAG(IS_IOS)
  // Set the `kIdleTimeoutPolicyAppliesToUserOnly`pref if the policy is set as a
  // user policy. This will determine whether data should be cleared for
  // `TimePeriod::ALL_TIME` or only for the time the user was signed in.
  bool user_policy =
      policies.Get(policy_name())->scope == policy::POLICY_SCOPE_USER;
  prefs->SetBoolean(prefs::kIdleTimeoutPolicyAppliesToUserOnly, user_policy);
#endif  // BUILDFLAG(IS_IOS)
}

bool IdleTimeoutActionsPolicyHandler::CheckPolicySettings(
    const policy::PolicyMap& policies,
    policy::PolicyErrorMap* errors) {
  // Nothing to do if unset.
  if (!policies.GetValueUnsafe(policy_name())) {
    return false;
  }

  // Check that it's a list of strings, and that they're supported enum values.
  // Unsupported enum values are dropped, with a warning on chrome://policy.
  if (!policy::SchemaValidatingPolicyHandler::CheckPolicySettings(policies,
                                                                  errors)) {
    return false;
  }

  // If IdleTimeout is unset, add an error and do nothing.
  if (!CheckOtherPolicySet(policies, policy_name(), policy::key::kIdleTimeout,
                           errors)) {
    return false;
  }

  if (!CheckPolicyScopeSupported(policies, policy_name(), errors)) {
    return false;
  }

  return true;
}

}  // namespace enterprise_idle
