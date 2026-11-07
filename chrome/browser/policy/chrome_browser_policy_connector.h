// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CHROME_BROWSER_POLICY_CONNECTOR_H_
#define CHROME_BROWSER_POLICY_CHROME_BROWSER_POLICY_CONNECTOR_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/policy_service.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/policy/core/browser/android/policy_cache_updater_android.h"
#endif

class PrefService;

namespace policy {
class ConfigurationPolicyProvider;
class LocalTestPolicyProvider;

// Extends BrowserPolicyConnector with the setup shared among the desktop
// implementations and Android.
class ChromeBrowserPolicyConnector : public BrowserPolicyConnector {
 public:
  // Builds an uninitialized ChromeBrowserPolicyConnector, suitable for testing.
  // Init() should be called to create and start the policy machinery.
  ChromeBrowserPolicyConnector();
  ChromeBrowserPolicyConnector(const ChromeBrowserPolicyConnector&) = delete;
  ChromeBrowserPolicyConnector& operator=(const ChromeBrowserPolicyConnector&) =
      delete;
  ~ChromeBrowserPolicyConnector() override;

  // Called once the resource bundle has been created. Calls through to super
  // class to notify observers.
  void OnResourceBundleCreated();

  void Init(PrefService* local_state,
            scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
      override;

  // Called to signal the browser has started.
  virtual void OnBrowserStarted();

  bool IsDeviceEnterpriseManaged() const override;

  bool HasMachineLevelPolicies() override;

  void Shutdown() override;

  ConfigurationPolicyProvider* GetPlatformProvider();

  // Refreshes only platform (non-cloud) policies. This is used by the
  // --refresh-platform-policy command line switch to provide fast policy
  // updates without triggering network requests to cloud policy servers.
  void RefreshPlatformPolicies();

  ConfigurationPolicyProvider* local_test_policy_provider();
  void SetLocalTestPolicyProviderForTesting(
      ConfigurationPolicyProvider* provider);

  // If the kLocalTestPoliciesForNextStartup pref is non-empty, read and apply
  // the policies stored in it, and then clear the pref. This must be called
  // right after the `local_state` is created to ensure policies are applied
  // at the right time.
  void MaybeApplyLocalTestPolicies(PrefService* local_state);

#if !BUILDFLAG(IS_CHROMEOS)
  ConfigurationPolicyProvider* command_line_policy_provider() {
    return command_line_provider_;
  }
#endif  // !BUILDFLAG(IS_CHROMEOS)

  // BrowserPolicyConnector:
  // Command line switch only supports Dev and Canary channel, trunk and
  // browser tests on Win, Mac, Linux and Android.
  bool IsCommandLineSwitchSupported() const override;

  static void EnableCommandLineSupportForTesting();

  virtual base::flat_set<std::string> device_affiliation_ids() const;
  void SetDeviceAffiliatedIdsForTesting(
      const base::flat_set<std::string>& device_affiliation_ids);

 protected:
  // BrowserPolicyConnectorBase::
  std::vector<std::unique_ptr<policy::ConfigurationPolicyProvider>>
  CreatePolicyProviders() override;

  base::flat_set<std::string> device_affiliation_ids_for_testing_;

 private:
  // Returns the policy provider that supplies platform policies.
  std::unique_ptr<ConfigurationPolicyProvider> CreatePlatformProvider();

#if BUILDFLAG(IS_ANDROID)
  std::unique_ptr<android::PolicyCacheUpdater> policy_cache_updater_;
#endif  // BUILDFLAG(IS_ANDROID)

  // Owned by base class.
  raw_ptr<ConfigurationPolicyProvider> platform_provider_ = nullptr;

  // Owned by base class.
  raw_ptr<ConfigurationPolicyProvider> command_line_provider_ = nullptr;

  raw_ptr<ConfigurationPolicyProvider> local_test_provider_for_testing_ =
      nullptr;
  std::unique_ptr<LocalTestPolicyProvider> local_test_provider_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CHROME_BROWSER_POLICY_CONNECTOR_H_
