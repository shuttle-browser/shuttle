// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/extensions/api/management/chrome_management_api_delegate.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/api/management/management_api.h"
#include "extensions/common/api/management.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"
#include "ui/base/window_open_disposition.h"

namespace extensions {
namespace {

// Web apps and app launching are removed from this browser, so no app can be
// generated for a link. The delegate only exists to satisfy the interface.
class ChromeAppForLinkDelegate : public AppForLinkDelegate {
 public:
  ChromeAppForLinkDelegate() = default;

  ChromeAppForLinkDelegate(const ChromeAppForLinkDelegate&) = delete;
  ChromeAppForLinkDelegate& operator=(const ChromeAppForLinkDelegate&) = delete;

  ~ChromeAppForLinkDelegate() override = default;

  api::management::ExtensionInfo CreateExtensionInfoFromWebApp(
      const ExtensionId& app_id,
      content::BrowserContext* context) override {
    return api::management::ExtensionInfo();
  }
};

}  // namespace

bool ChromeManagementAPIDelegate::LaunchAppFunctionDelegate(
    const Extension* extension,
    content::BrowserContext* context) const {
  // App launching (App Service, application_launch) is removed.
  return false;
}

bool ChromeManagementAPIDelegate::CreateAppShortcutFunctionDelegate(
    ManagementCreateAppShortcutFunction* function,
    const Extension* extension,
    std::string* error) const {
  // App shortcuts (and the app launching they rely on) are removed.
  *error = "Creating app shortcuts is not supported.";
  return false;
}

std::unique_ptr<AppForLinkDelegate>
ChromeManagementAPIDelegate::GenerateAppForLinkFunctionDelegate(
    ManagementGenerateAppForLinkFunction* function,
    content::BrowserContext* context,
    const std::string& title,
    const GURL& launch_url) const {
  // Web apps are removed; report an install failure asynchronously, as the
  // caller expects the response after this returns.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&ManagementGenerateAppForLinkFunction::FinishCreateWebApp,
                     base::RetainedRef(function), std::string(),
                     /*install_success=*/false));
  return std::make_unique<ChromeAppForLinkDelegate>();
}

bool ChromeManagementAPIDelegate::CanContextInstallWebApps(
    content::BrowserContext* context) const {
  return false;
}

void ChromeManagementAPIDelegate::InstallOrLaunchReplacementWebApp(
    content::BrowserContext* context,
    const GURL& web_app_url,
    InstallOrLaunchWebAppCallback callback) const {
  std::move(callback).Run(InstallOrLaunchWebAppResult::kInvalidWebApp);
}

}  // namespace extensions
