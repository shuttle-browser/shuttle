// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/settings/settings_localized_strings_provider.h"
#include "components/strings/grit/privacy_sandbox_strings.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/webui/webui_util.h"

namespace settings {

void AddPrivacySandboxStrings(content::WebUIDataSource* html_source,
                              Profile* /*profile*/) {
  // Strings used outside the privacy sandbox page. The i18n preprocessor might
  // replace those before the corresponding flag value is checked, which is why
  // they are included independently of the flag value.
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"cookiePageSettingsAllowBulletOne",
       IDS_SETTINGS_COOKIES_BLOCK_THIRD_PARTY_SETTINGS_ALLOW_BULLET_ONE},
      {"cookiePageSettingsAllowBulletTwo",
       IDS_SETTINGS_COOKIES_BLOCK_THIRD_PARTY_SETTINGS_ALLOW_BULLET_TWO},
      {"cookiePageSettingsAllowBulletThree",
       IDS_SETTINGS_COOKIES_BLOCK_THIRD_PARTY_SETTINGS_ALLOW_BULLET_THREE},
      {"cookiePageSettingsBlockBulletOne",
       IDS_SETTINGS_COOKIES_BLOCK_THIRD_PARTY_SETTINGS_BLOCK_BULLET_ONE},
      {"cookiePageSettingsBlockBulletTwo",
       IDS_SETTINGS_COOKIES_BLOCK_THIRD_PARTY_SETTINGS_BLOCK_BULLET_TWO},
      {"cookiePageSettingsBlockBulletThree",
       IDS_SETTINGS_COOKIES_BLOCK_THIRD_PARTY_SETTINGS_BLOCK_BULLET_THREE},
      {"allowThirdPartyCookiesExpandA11yLabel",
       IDS_SETTINGS_ALLOW_THIRD_PARTY_COOKIES_EXPAND_A11Y_LABEL},
      {"blockThirdPartyCookiesExpandA11yLabel",
       IDS_SETTINGS_BLOCK_THIRD_PARTY_COOKIES_EXPAND_A11Y_LABEL}};
  html_source->AddLocalizedStrings(kLocalizedStrings);
}

}  // namespace settings
