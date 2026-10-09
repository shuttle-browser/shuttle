// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/chrome_browser_cloud_management_controller_desktop.h"

#include <stdint.h>

#include <memory>
#include <set>
#include <string>
#include <utility>

#include "base/check_is_test.h"
#include "base/command_line.h"
#include "base/not_fatal_until.h"
#include "base/path_service.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/default_clock.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/device_identity/device_oauth2_token_service_factory.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/key_loader.h"
#include "chrome/browser/enterprise/remote_commands/cbcm_remote_commands_factory.h"
#include "chrome/browser/enterprise/reporting/reporting_delegate_factory_desktop.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/policy/chrome_browser_cloud_management_register_watcher.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/policy/client_data_delegate_desktop.h"
#include "chrome/browser/policy/policy_util.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "components/policy/core/common/cloud/machine_level_user_cloud_policy_manager.h"
#include "components/policy/core/common/features.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/network_service_instance.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

#if BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "base/base_paths_win.h"
#include "chrome/install_static/install_modes.h"
#else
#include "chrome/common/chrome_switches.h"
#endif  // BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/app_controller_mac.h"
#include "chrome/browser/policy/browser_dm_token_storage_mac.h"
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/policy/browser_dm_token_storage_linux.h"
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_WIN)
#include "chrome/browser/policy/browser_dm_token_storage_win.h"
#include "chrome/install_static/install_util.h"
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)
#include "chrome/browser/enterprise/client_certificates/browser_context_delegate.h"
#include "chrome/browser/enterprise/client_certificates/cert_utils.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/device_trust_key_manager_impl.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/key_rotation_launcher.h"
#include "chrome/browser/enterprise/reporting/browser_launch/browser_launch_event_controller_factory_desktop.h"
#include "chrome/browser/enterprise/reporting/saas_usage/saas_usage_reporting_delegate_factory_impl.h"
#include "components/enterprise/browser/reporting/saas_usage/saas_usage_reporting_delegate_factory.h"
#include "components/enterprise/client_certificates/core/browser_cloud_management_delegate.h"
#include "components/enterprise/client_certificates/core/certificate_provisioning_service.h"
#include "components/enterprise/client_certificates/core/dm_server_client.h"
#include "components/enterprise/client_certificates/core/key_upload_client.h"
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)

namespace policy {

namespace {

#if BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
constexpr base::FilePath::StringViewType kCachedPolicyDirname =
    FILE_PATH_LITERAL("Policies");
#endif

}  // namespace

ChromeBrowserCloudManagementControllerDesktop::
    ChromeBrowserCloudManagementControllerDesktop() = default;
ChromeBrowserCloudManagementControllerDesktop::
    ~ChromeBrowserCloudManagementControllerDesktop() = default;

void ChromeBrowserCloudManagementControllerDesktop::
    SetDMTokenStorageDelegate() {
  std::unique_ptr<BrowserDMTokenStorage::Delegate> storage_delegate;

#if BUILDFLAG(IS_MAC)
  storage_delegate = std::make_unique<BrowserDMTokenStorageMac>();
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  storage_delegate = std::make_unique<BrowserDMTokenStorageLinux>();
#elif BUILDFLAG(IS_WIN)
  storage_delegate = std::make_unique<BrowserDMTokenStorageWin>();
#else
  NOTREACHED();
#endif

  BrowserDMTokenStorage::SetDelegate(std::move(storage_delegate));
}

int ChromeBrowserCloudManagementControllerDesktop::GetUserDataDirKey() {
  return chrome::DIR_USER_DATA;
}

base::FilePath
ChromeBrowserCloudManagementControllerDesktop::GetExternalPolicyDir() {
  base::FilePath external_policy_path;
#if BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
  base::PathService::Get(base::DIR_PROGRAM_FILESX86, &external_policy_path);

  external_policy_path =
      external_policy_path.Append(install_static::kCompanyPathName)
          .Append(kCachedPolicyDirname);
#endif

  return external_policy_path;
}

ChromeBrowserCloudManagementController::Delegate::NetworkConnectionTrackerGetter
ChromeBrowserCloudManagementControllerDesktop::
    CreateNetworkConnectionTrackerGetter() {
  return base::BindRepeating(&content::GetNetworkConnectionTracker);
}

void ChromeBrowserCloudManagementControllerDesktop::InitializeOAuthTokenFactory(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    PrefService* local_state) {
  DeviceOAuth2TokenServiceFactory::Initialize(
      url_loader_factory, local_state, g_browser_process->os_crypt_async());
}

void ChromeBrowserCloudManagementControllerDesktop::StartWatchingRegistration(
    ChromeBrowserCloudManagementController* controller) {
  cloud_management_register_watcher_ =
      std::make_unique<ChromeBrowserCloudManagementRegisterWatcher>(controller);
}

bool ChromeBrowserCloudManagementControllerDesktop::
    WaitUntilPolicyEnrollmentFinished() {
  if (cloud_management_register_watcher_) {
    switch (cloud_management_register_watcher_
                ->WaitUntilCloudPolicyEnrollmentFinished()) {
      case ChromeBrowserCloudManagementController::RegisterResult::
          kNoEnrollmentNeeded:
      case ChromeBrowserCloudManagementController::RegisterResult::
          kEnrollmentSuccessBeforeDialogDisplayed:
      case ChromeBrowserCloudManagementController::RegisterResult::
          kEnrollmentFailedSilentlyBeforeDialogDisplayed:
        return true;
      case ChromeBrowserCloudManagementController::RegisterResult::
          kEnrollmentSuccess:
      case ChromeBrowserCloudManagementController::RegisterResult::
          kEnrollmentFailedSilently:
        CHECK(!IsEnterpriseStartupDialogShowing(), base::NotFatalUntil::M155);
#if BUILDFLAG(IS_MAC)
        app_controller_mac::EnterpriseStartupDialogClosed();
#endif
        return true;
      case ChromeBrowserCloudManagementController::RegisterResult::
          kRestartDueToFailure:
        chrome::AttemptRestart();
        return false;
      case ChromeBrowserCloudManagementController::RegisterResult::
          kQuitDueToFailure:
        chrome::AttemptExit();
        return false;
    }
  }
  return true;
}

bool ChromeBrowserCloudManagementControllerDesktop::
    IsEnterpriseStartupDialogShowing() {
  return cloud_management_register_watcher_ &&
         cloud_management_register_watcher_->IsDialogShowing();
}

void ChromeBrowserCloudManagementControllerDesktop::OnServiceAccountSet(
    CloudPolicyClient* client,
    const std::string& account_email) {
  // Shuttle: policy invalidations were removed; nothing to start.
}

void ChromeBrowserCloudManagementControllerDesktop::ShutDown() {

  // In some tests, `DCHECK_CURRENTLY_ON(content::BrowserThread::UI)` fails.
  // Such tests have not initialized device_oauth2_token_service anyway, so
  // skip calling Shutdown() for the service.
  if (!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI)) {
    CHECK_IS_TEST();
    return;
  }
  DeviceOAuth2TokenServiceFactory::Shutdown();
}

MachineLevelUserCloudPolicyManager*
ChromeBrowserCloudManagementControllerDesktop::
    GetMachineLevelUserCloudPolicyManager() {
  return g_browser_process->browser_policy_connector()
      ->machine_level_user_cloud_policy_manager();
}

DeviceManagementService*
ChromeBrowserCloudManagementControllerDesktop::GetDeviceManagementService() {
  return g_browser_process->browser_policy_connector()
      ->device_management_service();
}

scoped_refptr<network::SharedURLLoaderFactory>
ChromeBrowserCloudManagementControllerDesktop::GetSharedURLLoaderFactory() {
  return g_browser_process->system_network_context_manager()
      ->GetSharedURLLoaderFactory();
}

scoped_refptr<base::SingleThreadTaskRunner>
ChromeBrowserCloudManagementControllerDesktop::GetBestEffortTaskRunner() {
  // ChromeBrowserCloudManagementControllerDesktop is bound to BrowserThread::UI
  // and so must its best-effort task runner.
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return content::GetUIThreadTaskRunner({base::TaskPriority::BEST_EFFORT});
}

std::unique_ptr<enterprise_reporting::ReportingDelegateFactory>
ChromeBrowserCloudManagementControllerDesktop::GetReportingDelegateFactory() {
  return std::make_unique<
      enterprise_reporting::ReportingDelegateFactoryDesktop>();
}

std::unique_ptr<enterprise_reporting::SaasUsageReportingDelegateFactory>
ChromeBrowserCloudManagementControllerDesktop::
    GetSaasUsageReportingDelegateFactory() {
#if BUILDFLAG(IS_CHROMEOS)
  return nullptr;
#else
  return enterprise_reporting::SaasUsageReportingDelegateFactoryImpl::
      CreateForBrowser();
#endif  // !BUILDFLAG(IS_CHROMEOS)
}

std::unique_ptr<enterprise_reporting::BrowserLaunchEventController>
ChromeBrowserCloudManagementControllerDesktop::
    CreateBrowserLaunchEventController() {
  return enterprise_reporting::BrowserLaunchEventControllerFactoryDesktop::
      CreateForBrowser();
}

void ChromeBrowserCloudManagementControllerDesktop::SetGaiaURLLoaderFactory(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  gaia_url_loader_factory_ = url_loader_factory;
}

bool ChromeBrowserCloudManagementControllerDesktop::
    ReadyToCreatePolicyManager() {
  return true;
}

bool ChromeBrowserCloudManagementControllerDesktop::ReadyToInit() {
  return true;
}

std::unique_ptr<ClientDataDelegate>
ChromeBrowserCloudManagementControllerDesktop::CreateClientDataDelegate() {
  return std::make_unique<ClientDataDelegateDesktop>();
}

std::unique_ptr<enterprise_connectors::DeviceTrustKeyManager>
ChromeBrowserCloudManagementControllerDesktop::CreateDeviceTrustKeyManager() {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)
  auto* browser_dm_token_storage = BrowserDMTokenStorage::Get();
  auto* device_management_service = GetDeviceManagementService();
  auto shared_url_loader_factory = GetSharedURLLoaderFactory();

  auto key_rotation_launcher =
      enterprise_connectors::KeyRotationLauncher::Create(
          browser_dm_token_storage, device_management_service,
          shared_url_loader_factory);
  auto key_loader = enterprise_connectors::KeyLoader::Create(
      device_management_service, shared_url_loader_factory);

  return std::make_unique<enterprise_connectors::DeviceTrustKeyManagerImpl>(
      std::move(key_rotation_launcher), std::move(key_loader));
#else
  return nullptr;
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)
}

std::unique_ptr<client_certificates::CertificateProvisioningService>
ChromeBrowserCloudManagementControllerDesktop::
    CreateCertificateProvisioningService() {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)
  if (!certificate_store_) {
    certificate_store_ =
        std::make_unique<client_certificates::PrefsCertificateStore>(
            g_browser_process->local_state(),
            client_certificates::CreatePrivateKeyFactory());
  }

  return client_certificates::CreateBrowserCertificateProvisioningService(
      g_browser_process->local_state(), certificate_store_.get(),
      GetDeviceManagementService(), GetSharedURLLoaderFactory());
#else
  return nullptr;
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)
}

}  // namespace policy
