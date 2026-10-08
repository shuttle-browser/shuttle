// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/webui_url_constants.h"

#include <array>
#include <string_view>

#include "base/containers/fixed_flat_set.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "components/history_clusters/history_clusters_internals/webui/url_constants.h"
#include "components/optimization_guide/optimization_guide_internals/webui/url_constants.h"
#include "components/safe_browsing/core/common/web_ui_constants.h"
#include "device/vr/buildflags/buildflags.h"
#include "extensions/buildflags/buildflags.h"
#include "third_party/blink/public/common/chrome_debug_urls.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/constants/chrome_webui_url_constants.h"
#include "ash/constants/webui_url_constants.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace chrome {

// Note: Add hosts to `ChromeURLHosts()` at the bottom of this file to be listed
// by chrome://chrome-urls (about:about) and the built-in AutocompleteProvider.

#if BUILDFLAG(IS_CHROMEOS)

// static assertions to keep the consistency with URLs that ash as OS system
// refers.
static_assert(std::string_view(kChromeUICertificateManagerDialogURL) ==
              ash::chrome_urls::kChromeUICertificateManagerDialogURL);
static_assert(std::string_view(kChromeUIFeedbackURL) ==
              ash::chrome_urls::kChromeUIFeedbackURL);
static_assert(std::u16string_view(kChromeUIManagementURL16) ==
              ash::chrome_urls::kChromeUIManagementURL16);
static_assert(std::string_view(kChromeUINewTabURL) ==
              ash::chrome_urls::kChromeUINewTabURL);
static_assert(std::string_view(kChromeUISettingsHost) ==
              ash::chrome_urls::kChromeUISettingsHost);
static_assert(std::string_view(kChromeUISettingsURL) ==
              ash::chrome_urls::kChromeUISettingsURL);
static_assert(std::string_view(kChromeUITermsHost) ==
              ash::chrome_urls::kChromeUITermsHost);
static_assert(std::string_view(kChromeUITermsURL) ==
              ash::chrome_urls::kChromeUITermsURL);

static_assert(std::string_view(kClearBrowserDataSubPage) ==
              ash::chrome_urls::kClearBrowserDataSubPage);
static_assert(std::string_view(kDownloadsSubPage) ==
              ash::chrome_urls::kDownloadsSubPage);
static_assert(std::string_view(kSearchSubPage) ==
              ash::chrome_urls::kSearchSubPage);
static_assert(std::string_view(kSyncSetupSubPage) ==
              ash::chrome_urls::kSyncSetupSubPage);

bool IsSystemWebUIHost(std::string_view host) {
  // Compares host instead of full URL for performance (the strings are
  // shorter).
  constexpr auto kHosts = base::MakeFixedFlatSet<std::string_view>({
      ash::kChromeUIAccountManagerErrorHost,
      ash::kChromeUIAccountMigrationWelcomeHost,
      ash::kChromeUIAddSupervisionHost,
      ash::kChromeUIAppInstallDialogHost,
      ash::kChromeUIBluetoothPairingHost,
      ash::kChromeUIBorealisCreditsHost,
      ash::kChromeUIBorealisMOTDHost,
      kChromeUICertificateManagerHost,
      ash::kChromeUICloudUploadHost,
      ash::kChromeUICrostiniCreditsHost,
      ash::kChromeUICrostiniInstallerHost,
      ash::kChromeUICryptohomeHost,
      ash::kChromeUIEmojiPickerHost,
      ash::kChromeUIExtendedUpdatesDialogHost,
      ash::kChromeUIInternetConfigDialogHost,
      ash::kChromeUIInternetDetailDialogHost,
      ash::kChromeUILockScreenNetworkHost,
      ash::kChromeUILockScreenStartReauthHost,
      ash::kChromeUIMobileSetupHost,
      ash::kChromeUIMultiDeviceSetupHost,
      ash::kChromeUINetworkHost,
      ash::kChromeUINotificationTesterHost,
      ash::kChromeUIOobeHost,
      ash::kChromeUIOSCreditsHost,
      ash::kChromeUIOSSettingsHost,
      ash::kChromeUIPasswordChangeHost,
      ash::kChromeUIPowerHost,
      ash::kChromeUISetTimeHost,
      ash::kChromeUISmbCredentialsHost,
      ash::kChromeUISmbShareHost,
  });

  return kHosts.contains(host);
}
#endif  // BUILDFLAG(IS_CHROMEOS)

// Add hosts here to be suggested by BuiltinProvider.
base::span<const base::cstring_view> ChromeURLHosts() {
  static constexpr auto kChromeURLHosts = std::to_array<base::cstring_view>({
      kChromeUIAboutHost,
      kChromeUIAccessibilityHost,
      kChromeUIActorInternalsHost,
      kChromeUIBluetoothInternalsHost,
      kChromeUIChromeURLsHost,
      kChromeUIComponentsHost,
      kChromeUIConnectorsInternalsHost,
      kChromeUICreditsHost,
      kChromeUIDeviceLogHost,
      kChromeUIDownloadInternalsHost,
      kChromeUIFamilyLinkUserInternalsHost,
      kChromeUIFlagsHost,
      kChromeUIHistoryHost,
      history_clusters_internals::kChromeUIHistoryClustersInternalsHost,
      kChromeUIInterstitialHost,
      kChromeUILocalStateHost,
#if !BUILDFLAG(IS_ANDROID)
      kChromeUIManagementHost,
#endif
      kChromeUIMediaEngagementHost,
      kChromeUIMetricsInternalsHost,
      kChromeUINetExportHost,
      kChromeUINetInternalsHost,
      kChromeUINewTabHost,
      kChromeUIOmniboxHost,
      optimization_guide_internals::kChromeUIOptimizationGuideInternalsHost,
      kChromeUIPolicyHost,
      kChromeUIPredictorsHost,
      kChromeUIPrefsInternalsHost,
      kChromeUIProfileInternalsHost,
      content::kChromeUIQuotaInternalsHost,
#if !BUILDFLAG(IS_ANDROID)
      kChromeUIWebUIToolbarHost,
#endif
      kChromeUISiteEngagementHost,
      kChromeUISubresourceFilterInternalsHost,
      kChromeUISuggestInternalsHost,
      safe_browsing::kChromeUISafeBrowsingHost,
#if !BUILDFLAG(IS_ANDROID)
      kChromeUITabSearchHost,
      kChromeUITermsHost,
#endif
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
      kChromeUIUpdaterHost,
#endif
      kChromeUIUsbInternalsHost,
      kChromeUIUserActionsHost,
      kChromeUIVersionHost,
      content::kChromeUIBlobInternalsHost,
      content::kChromeUIDinoHost,
      content::kChromeUIGpuHost,
      content::kChromeUIHistogramHost,
      content::kChromeUIIndexedDBInternalsHost,
      content::kChromeUIMediaInternalsHost,
      content::kChromeUINetworkErrorsListingHost,
      content::kChromeUIProcessInternalsHost,
      content::kChromeUIServiceWorkerInternalsHost,
#if !BUILDFLAG(IS_ANDROID)
      content::kChromeUITracingHost,
#endif
      content::kChromeUIUkmHost,
      content::kChromeUIWebRTCInternalsHost,
#if BUILDFLAG(ENABLE_VR)
      content::kChromeUIWebXrInternalsHost,
#endif
#if !BUILDFLAG(IS_ANDROID)
#if !BUILDFLAG(IS_CHROMEOS)
      kChromeUIAppLauncherPageHost,
#endif
      kChromeUIBookmarksHost,
      kChromeUIDownloadsHost,
      kChromeUIHelpHost,
      kChromeUIInspectHost,
      kChromeUISettingsHost,
      kChromeUISystemInfoHost,
#if !BUILDFLAG(IS_CHROMEOS)
      kChromeUIWhatsNewHost,
#endif
#endif
#if BUILDFLAG(IS_CHROMEOS)
      ash::kChromeUIBorealisCreditsHost,
      kChromeUICertificateManagerHost,
      ash::kChromeUICrostiniCreditsHost,
      ash::kChromeUICryptohomeHost,
      ash::kChromeUIDriveInternalsHost,
      ash::kChromeUINetworkHost,
      ash::kChromeUILockScreenNetworkHost,
      ash::kChromeUIOobeHost,
      ash::kChromeUIOSCreditsHost,
      ash::kChromeUIOSSettingsHost,
      ash::kChromeUIPowerHost,
      ash::kChromeUISysInternalsHost,
      ash::kChromeUIInternetConfigDialogHost,
      ash::kChromeUIInternetDetailDialogHost,
#endif
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_DESKTOP_ANDROID)
      kChromeUIDiscardsHost,
#endif
#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_ANDROID)
      kChromeUILinuxProxyConfigHost,
#endif
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || \
    BUILDFLAG(IS_ANDROID)
      kChromeUISandboxHost,
#endif
#if BUILDFLAG(IS_WIN)
      kChromeUIConflictsHost,
#endif
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
      kChromeUIExtensionsHost,
      kChromeUIExtensionsInternalsHost,
#endif
#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
      kChromeUIPrintHost,
#endif
      kChromeUIWebRtcLogsHost,
      kChromeUIWebNNInternalsHost,
#if BUILDFLAG(IS_CHROMEOS)
      ash::kChromeUIDlpInternalsHost,
#endif  // BUILDFLAG(IS_CHROMEOS)
  });

  return base::span(kChromeURLHosts);
}

base::span<const base::cstring_view> ChromeDebugURLs() {
  // TODO(crbug.com/40253037): make this list comprehensive
  static constexpr auto kChromeDebugURLs = std::to_array<base::cstring_view>(
      {blink::kChromeUIBadCastCrashURL,
       blink::kChromeUIBrowserCrashURL,
       blink::kChromeUIBrowserDcheckURL,
       blink::kChromeUIBrowserUIHang,
       blink::kChromeUIBrowserHeapMemberDerefAfterFreeURL,
       blink::kChromeUIBrowserHeapOverflowURL,
       blink::kChromeUIBrowserHeapUaFURL,
       blink::kChromeUIBrowserHeapUnderflowURL,
       blink::kChromeUIGpuHeapMemberDerefAfterFreeURL,
       blink::kChromeUIGpuHeapOverflowURL,
       blink::kChromeUIGpuHeapUaFURL,
       blink::kChromeUIGpuHeapUnderflowURL,
       blink::kChromeUIRendererHeapMemberDerefAfterFreeURL,
       blink::kChromeUIRendererHeapOverflowURL,
       blink::kChromeUIRendererHeapUaFURL,
       blink::kChromeUIRendererHeapUnderflowURL,
       blink::kChromeUICrashURL,
       blink::kChromeUICrashRustURL,
#if defined(ADDRESS_SANITIZER)
       blink::kChromeUICrashRustOverflowURL,
#endif
       blink::kChromeUIDumpURL,
       blink::kChromeUIKillURL,
       blink::kChromeUIHangURL,
       blink::kChromeUIShorthangURL,
       blink::kChromeUIGpuCleanURL,
       blink::kChromeUIGpuCrashURL,
       blink::kChromeUIGpuHangURL,
       blink::kChromeUIMemoryExhaustURL,
       blink::kChromeUIMemoryPressureCriticalURL,
       blink::kChromeUIMemoryPressureModerateURL,
#if BUILDFLAG(IS_WIN)
       blink::kChromeUIBrowserHeapCorruptionURL,
       blink::kChromeUICfgViolationCrashURL,
       blink::kChromeUIHeapCorruptionCrashURL,
#endif
#if BUILDFLAG(IS_ANDROID)
       blink::kChromeUIGpuJavaCrashURL,
#else
#endif  // BUILDFLAG(IS_ANDROID)
       kChromeUIQuitURL,
       kChromeUIRestartURL});

  return base::span(kChromeDebugURLs);
}

const GURL& ChromeUINewTabURLAsGURL() {
  static base::NoDestructor<GURL> instance(kChromeUINewTabURL);
  return *instance;
}

}  // namespace chrome
