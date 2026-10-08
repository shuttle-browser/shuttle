// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CHROME_BROWSER_CLOUD_MANAGEMENT_CONTROLLER_DESKTOP_H_
#define CHROME_BROWSER_POLICY_CHROME_BROWSER_CLOUD_MANAGEMENT_CONTROLLER_DESKTOP_H_

#include <stdint.h>

#include <map>
#include <vector>

#include "base/task/single_thread_task_runner.h"
#include "components/enterprise/browser/controller/chrome_browser_cloud_management_controller.h"
#include "components/enterprise/client_certificates/core/prefs_certificate_store.h"
#include "extensions/buildflags/buildflags.h"

namespace policy {
class ChromeBrowserCloudManagementRegisterWatcher;

// Desktop implementation of the platform-specific operations of CBCMController.
class ChromeBrowserCloudManagementControllerDesktop
    : public ChromeBrowserCloudManagementController::Delegate {
 public:
  ChromeBrowserCloudManagementControllerDesktop();
  ChromeBrowserCloudManagementControllerDesktop(
      const ChromeBrowserCloudManagementControllerDesktop&) = delete;
  ChromeBrowserCloudManagementControllerDesktop& operator=(
      const ChromeBrowserCloudManagementControllerDesktop&) = delete;

  ~ChromeBrowserCloudManagementControllerDesktop() override;

  // ChromeBrowserCloudManagementController::Delegate implementation.
  void SetDMTokenStorageDelegate() override;
  int GetUserDataDirKey() override;
  base::FilePath GetExternalPolicyDir() override;
  NetworkConnectionTrackerGetter CreateNetworkConnectionTrackerGetter()
      override;
  void InitializeOAuthTokenFactory(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      PrefService* local_state) override;
  void StartWatchingRegistration(
      ChromeBrowserCloudManagementController* controller) override;
  bool WaitUntilPolicyEnrollmentFinished() override;
  bool IsEnterpriseStartupDialogShowing() override;
  void OnServiceAccountSet(CloudPolicyClient* client,
                           const std::string& account_email) override;
  void ShutDown() override;
  MachineLevelUserCloudPolicyManager* GetMachineLevelUserCloudPolicyManager()
      override;
  DeviceManagementService* GetDeviceManagementService() override;
  scoped_refptr<network::SharedURLLoaderFactory> GetSharedURLLoaderFactory()
      override;
  scoped_refptr<base::SingleThreadTaskRunner> GetBestEffortTaskRunner()
      override;
  std::unique_ptr<enterprise_reporting::ReportingDelegateFactory>
  GetReportingDelegateFactory() override;
  std::unique_ptr<enterprise_reporting::SaasUsageReportingDelegateFactory>
  GetSaasUsageReportingDelegateFactory() override;
  std::unique_ptr<enterprise_reporting::BrowserLaunchEventController>
  CreateBrowserLaunchEventController() override;
  void SetGaiaURLLoaderFactory(scoped_refptr<network::SharedURLLoaderFactory>
                                   url_loader_factory) override;
  bool ReadyToCreatePolicyManager() override;
  bool ReadyToInit() override;
  std::unique_ptr<ClientDataDelegate> CreateClientDataDelegate() override;
  std::unique_ptr<enterprise_connectors::DeviceTrustKeyManager>
  CreateDeviceTrustKeyManager() override;
  std::unique_ptr<client_certificates::CertificateProvisioningService>
  CreateCertificateProvisioningService() override;

 private:
  std::unique_ptr<ChromeBrowserCloudManagementRegisterWatcher>
      cloud_management_register_watcher_;

  // Agent Chrome: policy invalidations (FCM) were removed; policies refresh by
  // polling.
  scoped_refptr<network::SharedURLLoaderFactory> gaia_url_loader_factory_;

  // Responsible for storing and retrieving browser-level managed identities.
  std::unique_ptr<client_certificates::CertificateStore> certificate_store_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CHROME_BROWSER_CLOUD_MANAGEMENT_CONTROLLER_DESKTOP_H_
