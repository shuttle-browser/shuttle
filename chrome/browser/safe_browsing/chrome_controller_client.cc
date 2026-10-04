// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/chrome_controller_client.h"

#include "base/feature_list.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/security_interstitials/content/settings_page_helper.h"
#include "components/security_interstitials/core/metrics_helper.h"
#include "extensions/buildflags/buildflags.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#endif  // !BUILDFLAG(IS_ANDROID)

ChromeControllerClient::ChromeControllerClient(
    content::WebContents* web_contents,
    std::unique_ptr<security_interstitials::MetricsHelper> metrics_helper,
    PrefService* prefs,
    const std::string& app_locale,
    const GURL& default_safe_page,
    std::unique_ptr<security_interstitials::SettingsPageHelper>
        settings_page_helper)
    : safe_browsing::SafeBrowsingControllerClient(
          web_contents,
          std::move(metrics_helper),
          prefs,
          app_locale,
          default_safe_page,
          std::move(settings_page_helper)) {}

ChromeControllerClient::~ChromeControllerClient() = default;

void ChromeControllerClient::Proceed() {
  safe_browsing::SafeBrowsingControllerClient::Proceed();
}
