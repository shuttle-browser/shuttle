// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Contains constants for WebUI UI/Host/SubPage constants. Anything else go in
// chrome/common/url_constants.h.

#ifndef CHROME_COMMON_WEBUI_URL_CONSTANTS_H_
#define CHROME_COMMON_WEBUI_URL_CONSTANTS_H_

#include <stddef.h>

#include <string_view>

#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/strings/cstring_view.h"
#include "build/android_buildflags.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/common/buildflags.h"
#include "components/enterprise/buildflags/buildflags.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "content/public/common/url_constants.h"
#include "media/media_buildflags.h"
#include "printing/buildflags/buildflags.h"
#include "url/gurl.h"

namespace chrome {

// chrome: components (without schemes) and URLs (including schemes).
// e.g. kChromeUIFooHost = "foo" and kChromeUIFooURL = "chrome://foo/"
// Not all components have corresponding URLs and vice versa. Only add as
// needed.
// Please keep in alphabetical order, with OS/feature specific sections below.
inline constexpr char kChromeUIAboutHost[] = "about";
inline constexpr char kChromeUIAccessibilityHost[] = "accessibility";
inline constexpr char kChromeUIAccountSettingsURL[] =
    "chrome://settings/account";
inline constexpr char kChromeUIAddressesPath[] = "/addresses";
inline constexpr char kChromeUIAppIconHost[] = "app-icon";
inline constexpr char kChromeUIAppLauncherPageHost[] = "apps";
inline constexpr char kChromeUIAppsURL[] = "chrome://apps/";
inline constexpr char kChromeUIBluetoothInternalsHost[] = "bluetooth-internals";
inline constexpr char kChromeUIBookmarksHost[] = "bookmarks";
inline constexpr char kChromeUICertificateViewerHost[] = "view-cert";
inline constexpr char kChromeUICertificateViewerURL[] = "chrome://view-cert/";
inline constexpr char kChromeUIChromeURLsHost[] = "chrome-urls";
inline constexpr char16_t kChromeUIChromeURLsURL16[] = u"chrome://chrome-urls/";
inline constexpr char kChromeUIColorPipelineInternalsHost[] =
    "color-pipeline-internals";
inline constexpr char kChromeUIComponentsHost[] = "components";
inline constexpr char kChromeUIConflictsHost[] = "conflicts";
inline constexpr char kChromeUIConnectorsInternalsHost[] =
    "connectors-internals";
inline constexpr char kChromeUIConstrainedHTMLTestHost[] = "constrained-test";
inline constexpr char kChromeUIContactInfoPath[] = "/contactInfo";
inline constexpr char kChromeUIContextualTasksHost[] = "contextual-tasks";
inline constexpr char kChromeUIContextualTasksURL[] =
    "chrome://contextual-tasks/";
inline constexpr char kChromeUIContentSettingsHost[] = "content-settings";
inline constexpr char kChromeUIContentSettingsURL[] =
    "chrome://settings/content";
inline constexpr char kChromeUICreditsHost[] = "credits";
inline constexpr char kChromeUICreditsURL[] = "chrome://credits/";
inline constexpr char16_t kChromeUICreditsURL16[] = u"chrome://credits/";
inline constexpr char kChromeUIDefaultHost[] = "version";
inline constexpr char kChromeUIDelayedHangUIHost[] = "delayeduithreadhang";
inline constexpr char kChromeUIDeviceLogHost[] = "device-log";
inline constexpr char kChromeUIDevToolsBlankPath[] = "blank";
inline constexpr char kChromeUIDevToolsBundledPath[] = "bundled";
inline constexpr char kChromeUIDevToolsCustomPath[] = "custom";
inline constexpr char kChromeUIDevToolsHost[] = "devtools";
inline constexpr char kChromeUIDevToolsRemotePath[] = "remote";
inline constexpr char kChromeUIDevToolsURL[] =
    "devtools://devtools/bundled/inspector.html";
inline constexpr char kChromeUIDownloadInternalsHost[] = "download-internals";
inline constexpr char kChromeUIDownloadsHost[] = "downloads";
inline constexpr char kChromeUIDownloadsURL[] = "chrome://downloads/";
inline constexpr char kChromeUIDrivePickerHostHost[] = "drive-picker-host";
inline constexpr char kChromeUIEDUCoexistenceLoginURLV2[] =
    "chrome://chrome-signin/edu-coexistence";
inline constexpr char kChromeUIExtensionIconHost[] = "extension-icon";
inline constexpr char kChromeUIExtensionIconURL[] = "chrome://extension-icon/";
inline constexpr char kChromeUIExtensionsHost[] = "extensions";
inline constexpr char kChromeUIExtensionsInternalsHost[] =
    "extensions-internals";
inline constexpr char kChromeUIExtensionsURL[] = "chrome://extensions/";
inline constexpr char kChromeUIFamilyLinkUserInternalsHost[] =
    "family-link-user-internals";
inline constexpr char kChromeUIFavicon2Host[] = "favicon2";
inline constexpr char kChromeUIFaviconHost[] = "favicon";
inline constexpr char kChromeUIFaviconURL[] = "chrome://favicon/";
inline constexpr char kChromeUIFeedbackURL[] = "chrome://feedback/";
inline constexpr char kChromeUIFileiconURL[] = "chrome://fileicon/";
inline constexpr char kChromeUIFlagsHost[] = "flags";
inline constexpr char16_t kChromeUIFlagsURL16[] = u"chrome://flags/";
inline constexpr char kChromeUIHangUIHost[] = "uithreadhang";
inline constexpr char kChromeUIHelpHost[] = "help";
inline constexpr char kChromeUIHelpURL[] = "chrome://help/";
inline constexpr char kChromeUIHistoryHost[] = "history";
inline constexpr char kChromeUIHistorySyncedTabs[] = "/syncedTabs";
inline constexpr char kChromeUIHistoryURL[] = "chrome://history/";
inline constexpr char kChromeUIInfobarInternalsHost[] = "infobar-internals";
inline constexpr char kChromeUIImageHost[] = "image";
inline constexpr char kChromeUIImageURL[] = "chrome://image/";
inline constexpr char kChromeUIInspectHost[] = "inspect";
inline constexpr char kChromeUIInspectURL[] = "chrome://inspect/";
inline constexpr char kChromeUIInternalDebugPagesDisabledHost[] =
    "debug-webuis-disabled";
inline constexpr char kChromeUIInternalDebugPagesDisabledURL[] =
    "chrome://debug-webuis-disabled/";
inline constexpr char kChromeUIInternalsHost[] = "internals";
inline constexpr char kChromeUIInterstitialHost[] = "interstitials";
inline constexpr char kChromeUIInterstitialURL[] = "chrome://interstitials/";
inline constexpr char kChromeUILensOverlayUntrustedURL[] =
    "chrome-untrusted://lens-overlay/";
inline constexpr char kChromeUILocalStateHost[] = "local-state";
inline constexpr char kChromeUILocationInternalsHost[] = "location-internals";
inline constexpr char kChromeUIManagementHost[] = "management";
inline constexpr char kChromeUIManagementURL[] = "chrome://management";
inline constexpr char16_t kChromeUIManagementURL16[] = u"chrome://management";
inline constexpr char kChromeUIMediaEngagementHost[] = "media-engagement";
inline constexpr char kChromeUIMetricsInternalsHost[] = "metrics-internals";
inline constexpr char kChromeUINetExportHost[] = "net-export";
inline constexpr char kChromeUINetInternalsHost[] = "net-internals";
inline constexpr char kChromeUINewTabHost[] = "newtab";
inline constexpr char kChromeUINewTabURL[] = "chrome://newtab/";
const GURL& ChromeUINewTabURLAsGURL();
inline constexpr char kChromeUIOmniboxHost[] = "omnibox";
inline constexpr char kChromeUIOmniboxPopupHost[] = "omnibox-popup.top-chrome";
inline constexpr char kChromeUIOmniboxPopupURL[] =
    "chrome://omnibox-popup.top-chrome/";
inline constexpr char kChromeUIOrganizerPanelURL[] =
    "chrome://organizer-panel.top-chrome/";
inline constexpr char kChromeUIPageActionInternalsHost[] =
    "page-action-internals";
inline constexpr char kChromeUIPolicyHost[] = "policy";
inline constexpr char kChromeUIPredictorsHost[] = "predictors";
inline constexpr char kChromeUIPrefsInternalsHost[] = "prefs-internals";
inline constexpr char kChromeUIPrintURL[] = "chrome://print/";
inline constexpr char kChromeUIProfileInternalsHost[] = "profile-internals";
inline constexpr char kChromeUIQuitHost[] = "quit";
inline constexpr char kChromeUIQuitURL[] = "chrome://quit/";
inline constexpr char kChromeUIRestartHost[] = "restart";
inline constexpr char kChromeUIRestartURL[] = "chrome://restart/";
inline constexpr char kChromeUISegmentationInternalsHost[] =
    "segmentation-internals";
inline constexpr char kChromeUISettingsHost[] = "settings";
inline constexpr char16_t kChromeUISettingsHost16[] = u"settings";
inline constexpr char kChromeUISettingsURL[] = "chrome://settings/";
inline constexpr char16_t kChromeUISettingsURL16[] = u"chrome://settings/";
inline constexpr char kChromeUISiteEngagementHost[] = "site-engagement";
inline constexpr char kChromeUISplitViewNewTabPageURL[] =
    "chrome://tab-search.top-chrome/split_new_tab_page.html";
inline constexpr char kChromeUISubresourceFilterInternalsHost[] =
    "subresource-filter-internals";
inline constexpr char kChromeUISuggestInternalsHost[] = "suggest-internals";
inline constexpr char kChromeUISyncConfirmationHost[] = "sync-confirmation";
inline constexpr char kChromeUISystemInfoHost[] = "system";
inline constexpr char kChromeUIAiOverlayDialogUntrustedURL[] =
    "chrome-untrusted://ai-overlay-dialog/";
inline constexpr char kChromeUITermsHost[] = "terms";
inline constexpr char kChromeUITermsURL[] = "chrome://terms/";
inline constexpr char kChromeUIThemeHost[] = "theme";
inline constexpr char kChromeUIThemeURL[] = "chrome://theme/";
inline constexpr char kChromeUITopChromeDomain[] = "top-chrome";
inline constexpr char kChromeUIUntrustedDataSharingHost[] = "data-sharing";
inline constexpr char kChromeUIUntrustedFavicon2URL[] =
    "chrome-untrusted://favicon2/";
inline constexpr char kChromeUIUntrustedPrintURL[] =
    "chrome-untrusted://print/";
inline constexpr char kChromeUIUntrustedThemeURL[] =
    "chrome-untrusted://theme/";
inline constexpr char kChromeUIUsbInternalsHost[] = "usb-internals";
inline constexpr char kChromeUIUserActionsHost[] = "user-actions";
inline constexpr char kChromeUIVersionHost[] = "version";
inline constexpr char16_t kChromeUIVersionURL16[] = u"chrome://version/";
inline constexpr char kChromeUIWebRtcLogsHost[] = "webrtc-logs";
inline constexpr char kChromeUIWebuiGalleryHost[] = "webui-gallery";
inline constexpr char kChromeUIWebUITestHost[] = "webui-test";
inline constexpr char kChromeUIWebNNInternalsHost[] = "webnn-internals";

#if BUILDFLAG(IS_ANDROID)
inline constexpr char kChromeUINativeNewTabURL[] = "chrome-native://newtab/";
#else
inline constexpr char kChromeUIMultistepFilterInternalsHost[] =
    "multistep-filter-internals";
inline constexpr char kChromeUIReadLaterHost[] = "read-later.top-chrome";
inline constexpr char kChromeUIReadLaterURL[] =
    "chrome://read-later.top-chrome/";
inline constexpr char kChromeUISearchEngineSettingsPath[] = "/searchEngines";
inline constexpr char kChromeUISearchSettingsPath[] = "/search";
inline constexpr char kChromeUITabSearchHost[] = "tab-search.top-chrome";
inline constexpr char kChromeUIWebUIJsErrorHost[] = "webuijserror";
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS)
// Returns true if this web UI is part of the "system UI". Generally this is
// UI that opens in a window (not a browser tab) and that on other operating
// systems would be considered part of the OS or window manager.
bool IsSystemWebUIHost(std::string_view host);
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_DESKTOP_ANDROID)
inline constexpr char kChromeUIDiscardsHost[] = "discards";
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
inline constexpr char kChromeUIWhatsNewHost[] = "whats-new";
inline constexpr char kChromeUIWhatsNewURL[] = "chrome://whats-new/";
#endif

#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_ANDROID)
inline constexpr char kChromeUILinuxProxyConfigHost[] = "linux-proxy-config";
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || \
    BUILDFLAG(IS_ANDROID)
inline constexpr char kChromeUISandboxHost[] = "sandbox";
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
inline constexpr char kChromeUIBrowserSwitchHost[] = "browser-switch";
inline constexpr char kChromeUIBrowserSwitchURL[] = "chrome://browser-switch/";
inline constexpr char kChromeUIUpdaterHost[] = "updater";
#endif

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
inline constexpr char kChromeUIPrintHost[] = "print";
#endif

#if BUILDFLAG(ENABLE_SESSION_SERVICE)
inline constexpr char kChromeUISessionServiceInternalsPath[] =
    "session-service";
#endif

#if BUILDFLAG(ENTERPRISE_WATERMARK)
inline constexpr char kChromeUIWatermarkHost[] = "watermark";
#endif

#if BUILDFLAG(ENABLE_SESSION_SERVICE)
inline constexpr char kChromeUITabStripInternalsHost[] = "tab-strip-internals";
#endif

// Settings sub-pages.
//
// NOTE: Add sub page paths to |kChromeSettingsSubPages| in
// chrome_autocomplete_provider_client.cc to be listed by the built-in
// AutocompleteProvider.

inline constexpr char kAdPrivacySubPage[] = "adPrivacy";
inline constexpr char kAllSitesSettingsSubpage[] = "content/all";
inline constexpr char kClearBrowserDataSubPage[] = "clearBrowserData";
inline constexpr char kContentSettingsSubPage[] = "content";
inline constexpr char kCookieSettingsSubPage[] = "cookies";
inline constexpr char kDownloadsSubPage[] = "downloads";
inline constexpr char kExperimentalAISettingsSubPage[] = "ai";
inline constexpr char kFileSystemSettingsSubpage[] =
    "content/filesystem/siteDetails";
inline constexpr char kHandlerSettingsSubPage[] = "handlers";
inline constexpr char kLanguageOptionsSubPage[] = "languages";
inline constexpr char kManageProfileSubPage[] = "manageProfile";
inline constexpr char kOnDeviceSiteDataSubpage[] = "content/siteData";
inline constexpr char kPeopleSubPage[] = "people";
inline constexpr char kPerformanceSubPage[] = "performance";
inline constexpr char kPrivacyGuideSubPage[] = "privacy/guide";
inline constexpr char kPrivacySandboxMeasurementSubpage[] =
    "adPrivacy/measurement";
inline constexpr char kResetProfileSettingsSubPage[] = "resetProfileSettings";
inline constexpr char kSafeBrowsingEnhancedProtectionSubPage[] =
    "security?q=enhanced";
inline constexpr char kSafetyCheckSubPage[] = "safetyCheck";
inline constexpr char kSafetyHubSubPage[] = "safetyCheck";
inline constexpr char kSearchEnginesSubPage[] = "searchEngines";
inline constexpr char kSearchSubPage[] = "search";
inline constexpr char kSecuritySubPage[] = "security";
inline constexpr char kSignOutSubPage[] = "signOut";
inline constexpr char kSiteDetailsSubpage[] = "content/siteDetails";
inline constexpr char kSuggestionsSubPage[] = "ai/suggestions";
inline constexpr char kSyncSetupSubPage[] = "syncSetup";
inline constexpr char kTriggeredResetProfileSettingsSubPage[] =
    "triggeredResetProfileSettings";

#if BUILDFLAG(CHROME_ROOT_STORE_CERT_MANAGEMENT_UI)
inline constexpr char kChromeUICertificateManagerDialogURL[] =
    "chrome://certificate-manager";
inline constexpr char kChromeUICertificateManagerHost[] = "certificate-manager";
inline constexpr char kChromeUICertificateRedirectPath[] = "/certificates";
#endif  // BUILDFLAG(CHROME_ROOT_STORE_CERT_MANAGEMENT_UI)

#if BUILDFLAG(IS_MAC)
inline constexpr char kChromeUIUnexportableKeysInternalsHost[] =
    "unexportable-keys-internals";
#endif  // BUILDFLAG(IS_MAC)

// Extensions sub pages.
inline constexpr char kExtensionConfigureCommandsSubPage[] =
    "configureCommands";

// Gets the hosts/domains that are shown in chrome://chrome-urls.
base::span<const base::cstring_view> ChromeURLHosts();

// Gets the URL strings of "debug" pages which are dangerous and not for general
// consumption.
base::span<const base::cstring_view> ChromeDebugURLs();

}  // namespace chrome

#endif  // CHROME_COMMON_WEBUI_URL_CONSTANTS_H_
