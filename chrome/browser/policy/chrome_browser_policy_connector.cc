// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/chrome_browser_policy_connector.h"

#include <memory>
#include <string>
#include <utility>

#include "base/check_is_test.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/path_service.h"
#include "base/task/thread_pool.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/policy/configuration_policy_handler_list_factory.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_paths.h"
#include "chrome_browser_policy_connector.h"
#include "components/policy/core/common/async_policy_provider.h"
#include "components/policy/core/common/command_line_policy_provider.h"
#include "components/policy/core/common/configuration_policy_provider.h"
#include "components/policy/core/common/local_test_policy_provider.h"
#include "components/policy/core/common/policy_logger.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/common/content_switches.h"
#include "extensions/buildflags/buildflags.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/registry.h"
#include "chrome/browser/browser_switcher/browser_switcher_policy_migrator.h"
#include "components/policy/core/common/policy_loader_win.h"
#elif BUILDFLAG(IS_MAC)
#include <CoreFoundation/CoreFoundation.h>

#include "base/apple/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#include "components/policy/core/common/policy_loader_mac.h"
#include "components/policy/core/common/preferences_mac.h"
#elif BUILDFLAG(IS_ANDROID)
#include "components/policy/core/common/android/android_combined_policy_provider.h"
#elif BUILDFLAG(IS_POSIX)
#include "components/policy/core/common/config_dir_policy_loader.h"
#endif

namespace policy {
namespace {
bool g_command_line_enabled_for_testing = false;
}  // namespace

ChromeBrowserPolicyConnector::ChromeBrowserPolicyConnector()
    : BrowserPolicyConnector(base::BindRepeating(&BuildHandlerList)) {}

ChromeBrowserPolicyConnector::~ChromeBrowserPolicyConnector() {
  if (local_test_provider_) {
    local_test_provider_->Shutdown();
  }
}

void ChromeBrowserPolicyConnector::OnResourceBundleCreated() {
  BrowserPolicyConnectorBase::OnResourceBundleCreated();
}

void ChromeBrowserPolicyConnector::Init(
    PrefService* local_state,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
#if BUILDFLAG(IS_ANDROID)
  policy_cache_updater_ = std::make_unique<android::PolicyCacheUpdater>(
      GetPolicyService(), GetHandlerList());
#endif

  PolicyLogger::GetInstance()->EnableLogCompression(
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN}));

  InitInternal(local_state);
}

void ChromeBrowserPolicyConnector::OnBrowserStarted() {}

bool ChromeBrowserPolicyConnector::IsDeviceEnterpriseManaged() const {
  NOTREACHED() << "This method is only defined for ChromeOS";
}

bool ChromeBrowserPolicyConnector::HasMachineLevelPolicies() {
  if (ProviderHasPolicies(GetPlatformProvider())) {
    return true;
  }
  if (ProviderHasPolicies(command_line_provider_)) {
    return true;
  }
  return false;
}

void ChromeBrowserPolicyConnector::Shutdown() {
#if !BUILDFLAG(IS_CHROMEOS)
  if (HasPolicyService()) {
    GetPolicyService()->UseLocalTestPolicyProvider(nullptr);
  }
#endif

  BrowserPolicyConnector::Shutdown();
}

ConfigurationPolicyProvider*
ChromeBrowserPolicyConnector::GetPlatformProvider() {
  if (ConfigurationPolicyProvider* provider =
          BrowserPolicyConnectorBase::GetPolicyProviderForTesting()) {
    CHECK_IS_TEST();
    return provider;
  }
  return platform_provider_.get();
}

void ChromeBrowserPolicyConnector::RefreshPlatformPolicies() {
  if (ConfigurationPolicyProvider* platform_provider = GetPlatformProvider()) {
    platform_provider->RefreshPolicies(policy::PolicyFetchReason::kUserRequest);
  }
}

ConfigurationPolicyProvider*
ChromeBrowserPolicyConnector::local_test_policy_provider() {
  if (local_test_provider_for_testing_) {
    return local_test_provider_for_testing_.get();
  }
  return local_test_provider_.get();
}

void ChromeBrowserPolicyConnector::SetLocalTestPolicyProviderForTesting(
    ConfigurationPolicyProvider* provider) {
  local_test_provider_for_testing_ = provider;
}

void ChromeBrowserPolicyConnector::MaybeApplyLocalTestPolicies(
    PrefService* local_state) {
  // Early return if the policy test page is disabled by any policy. This is
  // done because that policy is a profile level policy and we have not yet
  // loaded any profile to access its prefs.
  const auto& chrome_policies =
      GetPolicyService()->GetPolicies(policy::PolicyNamespace(
          policy::PolicyDomain::POLICY_DOMAIN_CHROME, std::string()));
  if (auto* policy_test_page_enabled = chrome_policies.GetValue(
          policy::key::kPolicyTestPageEnabled, base::Value::Type::BOOLEAN);
      policy_test_page_enabled && !policy_test_page_enabled->GetBool()) {
    return;
  }

  std::string policies_to_apply =
      local_state->GetString(policy_prefs::kLocalTestPoliciesForNextStartup);
  if (policies_to_apply.empty()) {
    return;
  }

  LocalTestPolicyProvider* test_provider =
      local_test_provider_for_testing_ ? static_cast<LocalTestPolicyProvider*>(
                                             local_test_provider_for_testing_)
                                       : local_test_provider_.get();
  test_provider->set_active(true);
  GetPolicyService()->UseLocalTestPolicyProvider(test_provider);
  test_provider->LoadJsonPolicies(policies_to_apply);
  local_state->ClearPref(policy_prefs::kLocalTestPoliciesForNextStartup);
}

bool ChromeBrowserPolicyConnector::IsCommandLineSwitchSupported() const {
  if (g_command_line_enabled_for_testing) {
    return true;
  }

  version_info::Channel channel = chrome::GetChannel();
  return channel != version_info::Channel::STABLE &&
         channel != version_info::Channel::BETA;
}

// static
void ChromeBrowserPolicyConnector::EnableCommandLineSupportForTesting() {
  g_command_line_enabled_for_testing = true;
}

base::flat_set<std::string>
ChromeBrowserPolicyConnector::device_affiliation_ids() const {
  if (!device_affiliation_ids_for_testing_.empty()) {
    return device_affiliation_ids_for_testing_;
  }
  // Not cloud managed (Shuttle): no device affiliation ids.
  return {};
}

void ChromeBrowserPolicyConnector::SetDeviceAffiliatedIdsForTesting(
    const base::flat_set<std::string>& device_affiliation_ids) {
  device_affiliation_ids_for_testing_ = device_affiliation_ids;
}

std::vector<std::unique_ptr<policy::ConfigurationPolicyProvider>>
ChromeBrowserPolicyConnector::CreatePolicyProviders() {
  auto providers = BrowserPolicyConnector::CreatePolicyProviders();
  std::unique_ptr<ConfigurationPolicyProvider> platform_provider =
      CreatePlatformProvider();
  if (platform_provider) {
    platform_provider_ = platform_provider.get();
    // PlatformProvider should be before all other providers (highest priority).
    providers.insert(providers.begin(), std::move(platform_provider));
  }

  std::unique_ptr<CommandLinePolicyProvider> command_line_provider =
      CommandLinePolicyProvider::CreateIfAllowed(
          *base::CommandLine::ForCurrentProcess(), chrome::GetChannel());
  if (command_line_provider) {
    command_line_provider_ = command_line_provider.get();
    providers.push_back(std::move(command_line_provider));
  }

  local_test_provider_ =
      LocalTestPolicyProvider::CreateIfAllowed(chrome::GetChannel());
  if (local_test_provider_) {
    local_test_provider_->Init(GetSchemaRegistry());
  }

  return providers;
}

std::unique_ptr<ConfigurationPolicyProvider>
ChromeBrowserPolicyConnector::CreatePlatformProvider() {
#if BUILDFLAG(IS_WIN)
  std::unique_ptr<AsyncPolicyLoader> loader(PolicyLoaderWin::Create(
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT}),
      ManagementServiceFactory::GetForPlatform(), kRegistryChromePolicyKey));
  return std::make_unique<AsyncPolicyProvider>(GetSchemaRegistry(),
                                               std::move(loader));
#elif BUILDFLAG(IS_MAC)
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // Explicitly watch the "com.google.Chrome" bundle ID, no matter what this
  // app's bundle ID actually is. All channels of Chrome should obey the same
  // policies.
  CFStringRef bundle_id = CFSTR("com.google.Chrome");
#elif BUILDFLAG(GOOGLE_CHROME_FOR_TESTING_BRANDING)
  CFStringRef bundle_id = CFSTR("com.google.ChromeForTesting");
#else
  base::apple::ScopedCFTypeRef<CFStringRef> bundle_id_scoper =
      base::SysUTF8ToCFStringRef(base::apple::BaseBundleID());
  CFStringRef bundle_id = bundle_id_scoper.get();
#endif
  auto loader = std::make_unique<PolicyLoaderMac>(
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT}),
      ManagementServiceFactory::GetForPlatform(),
      PolicyLoaderMac::GetManagedPolicyPath(bundle_id),
      std::make_unique<MacPreferences>(), bundle_id);
  return std::make_unique<AsyncPolicyProvider>(GetSchemaRegistry(),
                                               std::move(loader));
#elif BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)
  base::FilePath config_dir_path;
  if (base::PathService::Get(chrome::DIR_POLICY_FILES, &config_dir_path)) {
    auto loader = std::make_unique<ConfigDirPolicyLoader>(
        base::ThreadPool::CreateSequencedTaskRunner(
            {base::MayBlock(), base::TaskPriority::BEST_EFFORT}),
        config_dir_path, POLICY_SCOPE_MACHINE);
    return std::make_unique<AsyncPolicyProvider>(GetSchemaRegistry(),
                                                 std::move(loader));
  } else {
    return nullptr;
  }
#elif BUILDFLAG(IS_ANDROID)
  return std::make_unique<android::AndroidCombinedPolicyProvider>(
      GetSchemaRegistry());
#else
  return nullptr;
#endif
}

}  // namespace policy
