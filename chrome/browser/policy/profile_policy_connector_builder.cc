// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/profile_policy_connector_builder.h"

#include <list>
#include <utility>

#include "base/no_destructor.h"
#include "build/build_config.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/policy/schema_registry_service.h"
#include "chrome/browser/profiles/profile.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/core/common/policy_service_impl.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ash/profiles/profile_helper.h"
#endif

namespace policy {

namespace {

std::list<ConfigurationPolicyProvider*>* GetTestProviders() {
  static base::NoDestructor<std::list<ConfigurationPolicyProvider*>> instances;
  return instances.get();
}

}  // namespace

std::unique_ptr<ProfilePolicyConnector>
CreateProfilePolicyConnectorForBrowserContext(
    SchemaRegistry* schema_registry,
    ConfigurationPolicyProvider* policy_provider,
    policy::ChromeBrowserPolicyConnector* browser_policy_connector,
    bool force_immediate_load,
    content::BrowserContext* context) {
  const user_manager::User* user = nullptr;

#if BUILDFLAG(IS_CHROMEOS)
  Profile* const profile = Profile::FromBrowserContext(context);
  if (ash::ProfileHelper::IsUserProfile(profile)) {
    user = ash::ProfileHelper::Get()->GetUserByProfile(profile);
    CHECK(user);
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  return CreateAndInitProfilePolicyConnector(
      schema_registry, browser_policy_connector, policy_provider,
      force_immediate_load, user);
}

std::unique_ptr<ProfilePolicyConnector> CreateAndInitProfilePolicyConnector(
    SchemaRegistry* schema_registry,
    policy::ChromeBrowserPolicyConnector* browser_policy_connector,
    ConfigurationPolicyProvider* policy_provider,
    bool force_immediate_load,
    const user_manager::User* user) {
  auto connector = std::make_unique<ProfilePolicyConnector>();

  std::list<ConfigurationPolicyProvider*>* test_providers = GetTestProviders();
  if (test_providers->empty()) {
    connector->Init(user, schema_registry, policy_provider,
                    browser_policy_connector, force_immediate_load);
  } else {
    PolicyServiceImpl::Providers providers;
    providers.push_back(test_providers->front());
    test_providers->pop_front();
    auto service = std::make_unique<PolicyServiceImpl>(
        std::move(providers), PolicyServiceImpl::ScopeForMetrics::kUser);
    connector->InitForTesting(std::move(service));
  }

  return connector;
}

void PushProfilePolicyConnectorProviderForTesting(
    ConfigurationPolicyProvider* provider) {
  std::list<ConfigurationPolicyProvider*>* test_providers = GetTestProviders();
  test_providers->push_back(provider);
}

}  // namespace policy
