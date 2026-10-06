// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/shared_settings_localized_strings_provider.h"

#include <string>

#include "base/feature_list.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/google/core/common/google_util.h"
#include "components/live_caption/caption_util.h"
#include "components/soda/constants.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/content_features.h"
#include "media/base/media_switches.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/webui/webui_util.h"

namespace settings {

#if BUILDFLAG(IS_CHROMEOS)
namespace {

// Generates a Google Help URL which includes a "board type" parameter. Some
// help pages need to be adjusted depending on the type of CrOS device that is
// accessing the page.
std::u16string GetHelpUrlWithBoard(const std::u16string& original_url) {
  return base::StrCat(
      {original_url, u"&b=",
       base::ASCIIToUTF16(base::SysInfo::GetLsbReleaseBoard())});
}

}  // namespace
#endif

void AddAxAnnotationsSectionStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"mainNodeAnnotationsDownloadErrorLabel",
       IDS_SETTINGS_MAIN_NODE_ANNOTATIONS_DOWNLOAD_ERROR},
      {"mainNodeAnnotationsDownloadProgressLabel",
       IDS_SETTINGS_MAIN_NODE_ANNOTATIONS_DOWNLOAD_PROGRESS},
      {"mainNodeAnnotationsDownloadingLabel",
       IDS_SETTINGS_MAIN_NODE_ANNOTATIONS_DOWNLOADING},
      {"mainNodeAnnotationsTitle", IDS_SETTINGS_MAIN_NODE_ANNOTATIONS_TITLE},
      {"mainNodeAnnotationsSubtitle",
       IDS_SETTINGS_MAIN_NODE_ANNOTATIONS_SUBTITLE},
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);
  html_source->AddBoolean(
      "mainNodeAnnotationsEnabled",
      base::FeatureList::IsEnabled(features::kMainNodeAnnotations));
}

void AddCaptionSubpageStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"captionsTitle", IDS_SETTINGS_CAPTIONS},
      {"captionsPreferencesTitle", IDS_SETTINGS_CAPTIONS_PREFERENCES_TITLE},
      {"captionsPreferencesSubtitle",
       IDS_SETTINGS_CAPTIONS_PREFERENCES_SUBTITLE},
      {"captionsTextSize", IDS_SETTINGS_CAPTIONS_TEXT_SIZE},
      {"captionsTextFont", IDS_SETTINGS_CAPTIONS_TEXT_FONT},
      {"captionsTextColor", IDS_SETTINGS_CAPTIONS_TEXT_COLOR},
      {"captionsTextOpacity", IDS_SETTINGS_CAPTIONS_TEXT_OPACITY},
      {"captionsBackgroundOpacity", IDS_SETTINGS_CAPTIONS_BACKGROUND_OPACITY},
      {"captionsOpacityOpaque", IDS_SETTINGS_CAPTIONS_OPACITY_OPAQUE},
      {"captionsOpacitySemiTransparent",
       IDS_SETTINGS_CAPTIONS_OPACITY_SEMI_TRANSPARENT},
      {"captionsOpacityTransparent", IDS_SETTINGS_CAPTIONS_OPACITY_TRANSPARENT},
      {"captionsTextShadow", IDS_SETTINGS_CAPTIONS_TEXT_SHADOW},
      {"captionsTextShadowNone", IDS_SETTINGS_CAPTIONS_TEXT_SHADOW_NONE},
      {"captionsTextShadowRaised", IDS_SETTINGS_CAPTIONS_TEXT_SHADOW_RAISED},
      {"captionsTextShadowDepressed",
       IDS_SETTINGS_CAPTIONS_TEXT_SHADOW_DEPRESSED},
      {"captionsTextShadowUniform", IDS_SETTINGS_CAPTIONS_TEXT_SHADOW_UNIFORM},
      {"captionsTextShadowDropShadow",
       IDS_SETTINGS_CAPTIONS_TEXT_SHADOW_DROP_SHADOW},
      {"captionsBackgroundColor", IDS_SETTINGS_CAPTIONS_BACKGROUND_COLOR},
      {"captionsColorBlack", IDS_SETTINGS_CAPTIONS_COLOR_BLACK},
      {"captionsColorWhite", IDS_SETTINGS_CAPTIONS_COLOR_WHITE},
      {"captionsColorRed", IDS_SETTINGS_CAPTIONS_COLOR_RED},
      {"captionsColorGreen", IDS_SETTINGS_CAPTIONS_COLOR_GREEN},
      {"captionsColorBlue", IDS_SETTINGS_CAPTIONS_COLOR_BLUE},
      {"captionsColorYellow", IDS_SETTINGS_CAPTIONS_COLOR_YELLOW},
      {"captionsColorCyan", IDS_SETTINGS_CAPTIONS_COLOR_CYAN},
      {"captionsColorMagenta", IDS_SETTINGS_CAPTIONS_COLOR_MAGENTA},
      {"captionsDefaultSetting", IDS_SETTINGS_CAPTIONS_DEFAULT_SETTING},
      {"captionsManageLanguagesTitle",
       IDS_SETTINGS_CAPTIONS_MANAGE_LANGUAGES_TITLE},
      {"captionsLiveTranslateTargetLanguage",
       IDS_SETTINGS_CAPTIONS_LIVE_TRANSLATE_TARGET_LANGUAGE},
      {"captionsLiveTranslateTargetLanguageSubtitle",
       IDS_SETTINGS_CAPTIONS_LIVE_TRANSLATE_TARGET_LANGUAGE_SUBTITLE},
      {"removeLanguageLabel", IDS_SETTINGS_CAPTIONS_REMOVE_LANGUAGE_LABEL},
      {"makeDefaultLanguageLabel",
       IDS_SETTINGS_CAPTIONS_MAKE_DEFAULT_LANGUAGE_LABEL},
      {"moreActionsFor", IDS_SETTINGS_MORE_ACTIONS_BUTTON_ARIA_LABEL},
      {"defaultLanguageLabel", IDS_SETTINGS_CAPTIONS_DEFAULT_LANGUAGE_LABEL},
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);
  // Add the caption subtitle string conditionally so that non-cbx chromebooks
  // do not show live translate information.
#if BUILDFLAG(IS_CHROMEOS)
  html_source->AddLocalizedString(
      "captionsManageLanguagesSubtitle",
      base::FeatureList::IsEnabled(media::kFeatureManagementLiveTranslateCrOS)
          ? IDS_SETTINGS_CAPTIONS_MANAGE_LANGUAGES_SUBTITLE
          : IDS_SETTINGS_CAPTIONS_MANAGE_LANGUAGES_SUBTITLE_LIVE_CAPTION_ONLY);
#else
  html_source->AddLocalizedString(
      "captionsManageLanguagesSubtitle",
      IDS_SETTINGS_CAPTIONS_MANAGE_LANGUAGES_SUBTITLE);
#endif

  AddLiveCaptionSectionStrings(html_source);
}

void AddLiveCaptionSectionStrings(content::WebUIDataSource* html_source) {
  html_source->AddLocalizedString(
      "captionsEnableLiveCaptionTitle",
      IDS_SETTINGS_CAPTIONS_ENABLE_LIVE_CAPTION_TITLE);
  html_source->AddLocalizedString(
      "captionsEnableLiveTranslateTitle",
      IDS_SETTINGS_CAPTIONS_ENABLE_LIVE_TRANSLATE_TITLE);
  html_source->AddLocalizedString(
      "captionsEnableLiveTranslateSubtitle",
      IDS_SETTINGS_CAPTIONS_ENABLE_LIVE_TRANSLATE_SUBTITLE);
  html_source->AddLocalizedString(
      "captionsMaskOffensiveWordsTitle",
      IDS_SETTINGS_CAPTIONS_MASK_OFFENSIVE_WORDS_TITLE);

  const bool liveTranslateEnabled = media::IsLiveTranslateEnabled();

  html_source->AddLocalizedString(
      "captionsEnableLiveCaptionSubtitle",
      IDS_SETTINGS_CAPTIONS_ENABLE_LIVE_CAPTION_SUBTITLE);
  html_source->AddBoolean("enableLiveCaption",
                          captions::IsLiveCaptionFeatureSupported());
  html_source->AddBoolean("enableLiveCaptionMultiLanguage", true);

  html_source->AddBoolean("enableLiveTranslate", liveTranslateEnabled);
}

#if BUILDFLAG(IS_CHROMEOS)
void AddPasswordPromptDialogStrings(content::WebUIDataSource* html_source) {
}
#endif  // BUILDFLAG(IS_CHROMEOS)

void AddSecureDnsStrings(content::WebUIDataSource* html_source) {
  webui::LocalizedString kLocalizedStrings[] = {
      {"secureDns", IDS_SETTINGS_SECURE_DNS},
      {"secureDnsDescription", IDS_SETTINGS_SECURE_DNS_DESCRIPTION},
      {"secureDnsDisabledForManagedEnvironment",
       IDS_SETTINGS_SECURE_DNS_DISABLED_FOR_MANAGED_ENVIRONMENT},
      {"secureDnsDisabledForParentalControl",
       IDS_SETTINGS_SECURE_DNS_DISABLED_FOR_PARENTAL_CONTROL},
      {"secureDnsAutomaticModeDescription",
       IDS_SETTINGS_AUTOMATIC_MODE_DESCRIPTION},
      {"secureDnsCustomProviderDescription",
       IDS_SETTINGS_SECURE_DNS_CUSTOM_DESCRIPTION},
      {"secureDnsSecureDropdownModeDescription",
       IDS_SETTINGS_SECURE_DROPDOWN_MODE_DESCRIPTION},
      {"secureDnsSecureDropdownModePrivacyPolicy",
       IDS_SETTINGS_SECURE_DROPDOWN_MODE_PRIVACY_POLICY},
      {"secureDnsCustomPlaceholder",
       IDS_SETTINGS_SECURE_DNS_CUSTOM_PLACEHOLDER},
      {"secureDnsCustomFormatError",
       IDS_SETTINGS_SECURE_DNS_CUSTOM_FORMAT_ERROR},
      {"secureDnsCustomConnectionError",
       IDS_SETTINGS_SECURE_DNS_CUSTOM_CONNECTION_ERROR},
  };

  html_source->AddLocalizedStrings(kLocalizedStrings);
}

}  // namespace settings
