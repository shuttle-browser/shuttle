// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/critical_actions/critical_action_ui_utils.h"

#include "base/strings/strcat.h"
#include "chrome/common/webui_url_constants.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/base/url_util.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace critical_actions {

std::string GetCriticalActionLinkoutUrl(const CriticalActionEntry& action) {
  switch (action.action_type) {
    case ActionType::kCredentialAccess:
    case ActionType::kGooglePasswordManager:
    case ActionType::kFederatedLogin:
    case ActionType::kCredentialsOtp:
    case ActionType::kFormFill:
      // Agent Chrome: no password manager or autofill settings pages.
      return chrome::kChromeUISettingsURL;
    case ActionType::kDownload:
      return chrome::kChromeUIDownloadsURL;
    case ActionType::kSettingChange:
      return net::AppendQueryParameter(
                 GURL(base::StrCat({chrome::kChromeUISettingsURL,
                                    chrome::kSiteDetailsSubpage})),
                 "site", url::Origin::Create(action.url).Serialize())
          .spec();
    case ActionType::kUnknown:
      return chrome::kChromeUISettingsURL;
  }
}

}  // namespace critical_actions
