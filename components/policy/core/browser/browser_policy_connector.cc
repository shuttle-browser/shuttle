// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/browser/browser_policy_connector.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "base/check_is_test.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "build/branding_buildflags.h"
#include "components/policy/core/common/configuration_policy_provider.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/policy/core/common/policy_statistics_collector.h"
#include "components/policy/core/common/policy_switches.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/signin/public/identity_manager/account_managed_status_finder.h"

namespace policy {

BrowserPolicyConnector::BrowserPolicyConnector(
    const HandlerListFactory& handler_list_factory)
    : BrowserPolicyConnectorBase(handler_list_factory) {}

BrowserPolicyConnector::~BrowserPolicyConnector() = default;

void BrowserPolicyConnector::InitInternal(PrefService* local_state) {
  policy_statistics_collector_ =
      std::make_unique<policy::PolicyStatisticsCollector>(
          base::BindRepeating(&GetChromePolicyDetails), GetChromeSchema(),
          GetPolicyService(), local_state,
          base::SingleThreadTaskRunner::GetCurrentDefault());
  policy_statistics_collector_->Initialize();
}

void BrowserPolicyConnector::Shutdown() {
  BrowserPolicyConnectorBase::Shutdown();
  policy_statistics_collector_.reset();
}

bool BrowserPolicyConnector::ProviderHasPolicies(
    const ConfigurationPolicyProvider* provider) const {
  if (!provider || !provider->is_active()) {
    return false;
  }
  for (const auto& pair : provider->policies()) {
    if (!pair.second.empty()) {
      return true;
    }
  }
  return false;
}

}  // namespace policy
