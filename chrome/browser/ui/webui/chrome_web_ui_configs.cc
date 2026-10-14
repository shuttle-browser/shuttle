// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chrome_web_ui_configs.h"

#include "build/android_buildflags.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/optimization_guide/optimization_guide_internals_ui.h"
#include "chrome/browser/ui/webui/about/about_ui.h"
#include "chrome/browser/ui/webui/accessibility/accessibility_ui.h"
#include "chrome/browser/ui/webui/bluetooth_internals/bluetooth_internals_ui.h"
#include "chrome/browser/ui/webui/chrome_urls/chrome_urls_ui.h"
#include "chrome/browser/ui/webui/components/components_ui.h"
#include "chrome/browser/ui/webui/connectors_internals/connectors_internals_ui.h"
#include "chrome/browser/ui/webui/constrained_web_dialog_ui.h"
#include "chrome/browser/ui/webui/content_settings/content_settings_ui.h"
#include "chrome/browser/ui/webui/device_log/device_log_ui.h"
#include "chrome/browser/ui/webui/download_internals/download_internals_ui.h"
#include "chrome/browser/ui/webui/engagement/site_engagement_ui.h"
#include "chrome/browser/ui/webui/family_link_user_internals/family_link_user_internals_ui.h"
#include "chrome/browser/ui/webui/flags/flags_ui.h"
#include "chrome/browser/ui/webui/history_clusters/history_clusters_internals_ui_config.h"
#include "chrome/browser/ui/webui/internal_debug_pages_disabled/internal_debug_pages_disabled_ui.h"
#include "chrome/browser/ui/webui/interstitials/interstitial_ui.h"
#include "chrome/browser/ui/webui/local_state/local_state_ui.h"
#include "chrome/browser/ui/webui/location_internals/location_internals_ui.h"
#include "chrome/browser/ui/webui/management/management_ui.h"
#include "chrome/browser/ui/webui/media/media_engagement_ui.h"
#include "chrome/browser/ui/webui/media/webrtc_logs_ui.h"
#include "chrome/browser/ui/webui/metrics_internals/metrics_internals_ui.h"
#include "chrome/browser/ui/webui/net_export_ui.h"
#include "chrome/browser/ui/webui/net_internals/net_internals_ui.h"
#include "chrome/browser/ui/webui/omnibox/omnibox_ui.h"
#include "chrome/browser/ui/webui/policy/policy_ui.h"
#include "chrome/browser/ui/webui/predictors/predictors_ui.h"
#include "chrome/browser/ui/webui/regional_capabilities_internals/regional_capabilities_internals_ui.h"
#include "chrome/browser/ui/webui/segmentation_internals/segmentation_internals_ui.h"
#include "chrome/browser/ui/webui/subresource_filter/subresource_filter_internals_ui.h"
#include "chrome/browser/ui/webui/suggest_internals/suggest_internals_ui.h"
#include "chrome/browser/ui/webui/usb_internals/usb_internals_ui.h"
#include "chrome/browser/ui/webui/version/version_ui.h"
#include "components/enterprise/buildflags/buildflags.h"
#include "components/safe_browsing/buildflags.h"
#include "components/security_interstitials/content/connection_help_ui.h"
#include "components/security_interstitials/content/known_interception_disclosure_ui.h"
#include "components/webui/user_actions/user_actions_ui.h"
#include "content/public/browser/webui_config_map.h"
#include "extensions/buildflags/buildflags.h"
#include "printing/buildflags/buildflags.h"
#include "ui/webui/buildflags.h"

#if BUILDFLAG(ENABLE_WEBUI_NTP)
#endif  // BUILDFLAG(ENABLE_WEBUI_NTP)

#if BUILDFLAG(ENABLE_WEBUI_CERTIFICATE_VIEWER)
#include "chrome/browser/ui/webui/certificate_viewer/certificate_viewer_ui.h"
#endif


#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/webui/agents/agents_ui.h"
#include "chrome/browser/ui/tabs/tab_group_home/tab_group_home_ui.h"
#if !BUILDFLAG(IS_CHROMEOS)
#endif  // !BUILDFLAG(IS_CHROMEOS)
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#endif
#include "chrome/browser/ui/webui/color_pipeline_internals/color_pipeline_internals_ui.h"
#include "chrome/browser/ui/webui/downloads/downloads_ui.h"
#include "chrome/browser/ui/webui/history/history_ui.h"
#include "chrome/browser/ui/webui/infobar_internals/infobar_internals_ui.h"
#include "chrome/browser/ui/webui/inspect/inspect_ui.h"
#if BUILDFLAG(ENABLE_SESSION_SERVICE)
#include "chrome/browser/ui/webui/internals/internals_ui.h"
#include "chrome/browser/ui/webui/tab_strip_internals/tab_strip_internals_ui.h"
#endif  // BUILDFLAG(ENABLE_SESSION_SERVICE)
#include "chrome/browser/ui/webui/multistep_filter_internals/multistep_filter_internals_ui.h"
#if BUILDFLAG(ENABLE_WEBUI_NTP)
#include "chrome/browser/ui/webui/ntp/new_tab_ui.h"
#endif  // BUILDFLAG(ENABLE_WEBUI_NTP)
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_ui.h"
#include "chrome/browser/ui/webui/page_action_internals/page_action_internals_ui.h"
#include "chrome/browser/ui/webui/settings/settings_ui.h"
#include "chrome/browser/ui/webui/webnn_internals/webnn_internals_ui.h"
#include "chrome/browser/ui/webui/webui_gallery/webui_gallery_ui.h"
#include "chrome/browser/ui/webui/webui_js_error/webui_js_error_ui.h"
#else  // !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/webui/chrome_finds_internals/chrome_finds_internals_ui.h"
#include "chrome/browser/ui/webui/feed_internals/feed_internals_ui.h"
#include "chrome/browser/ui/webui/notifications_internals/notifications_internals_ui.h"
#include "chrome/browser/ui/webui/webapks/webapks_ui.h"
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
#include "chrome/browser/ui/webui/extensions/extensions_ui.h"
#endif  // !BUILDFLAG(ENABLE_EXTENSIONS_CORE)

#if BUILDFLAG(ENABLE_EXTENSIONS)
#endif  // ENABLE_EXTENSIONS

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
#include "chrome/browser/ui/webui/print_preview/print_preview_ui.h"
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)

#if BUILDFLAG(IS_CHROMEOS)
#include "base/check_is_test.h"
#include "chrome/browser/ui/webui/ash/config/ash_web_ui_config_manager.h"
#include "chrome/browser/ui/webui/ash/dlp_internals/dlp_internals_ui.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(CHROME_ROOT_STORE_CERT_MANAGEMENT_UI)
#include "chrome/browser/ui/webui/certificate_manager/certificate_manager_ui.h"
#endif  // BUILDFLAG(CHROME_ROOT_STORE_CERT_MANAGEMENT_UI)

#if BUILDFLAG(IS_WIN)
#include "chrome/browser/ui/webui/conflicts/conflicts_ui.h"
#include "chrome/browser/ui/webui/default_browser/visual_guided_setter_ui.h"
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
#include "chrome/browser/ui/webui/browser_switch/browser_switch_ui.h"
#include "chrome/browser/ui/webui/updater/updater_ui.h"
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || \
    BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/webui/sandbox/sandbox_internals_ui.h"
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) ||
        // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_DESKTOP_ANDROID)
#include "chrome/browser/ui/webui/discards/discards_ui.h"
#endif

#if BUILDFLAG(ENABLE_DICE_SUPPORT) || BUILDFLAG(IS_CHROMEOS)
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT) || BUILDFLAG(IS_CHROMEOS)
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ui/webui/signin/inline_login_ui.h"
#endif

#if BUILDFLAG(FULL_SAFE_BROWSING)
#endif

#if !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_ANDROID)
#endif  //  !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
#include "chrome/browser/ui/webui/safe_browsing/chrome_safe_browsing_ui.h"
#endif

#if BUILDFLAG(ENTERPRISE_WATERMARK)
#include "chrome/browser/ui/webui/watermark/watermark_ui.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/ui/webui/unexportable_keys_internals/unexportable_keys_internals_ui.h"
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
        // BUILDFLAG(IS_CHROMEOS)

void RegisterChromeWebUIConfigs() {
  // Don't add calls to `AddWebUIConfig()` for Ash-specific WebUIs here. Add
  // them to `AshWebUIConfigManager::RegisterWebUIConfigs()` in
  // ash_web_ui_config_manager.cc.
#if BUILDFLAG(IS_CHROMEOS)
  if (auto* ash_webui_config_manager =
          ash::AshWebUIConfigManager::GetInstance()) {
    ash_webui_config_manager->RegisterWebUIConfigs();
  } else {
    // AshWebUIConfigManager is created in
    // ChromeBrowserMainPartsAsh::PreProfileInit() and is not instantiated in
    // unit tests by default. Unit tests that require specific WebUIs should
    // register their configs individually in test fixtures.
    CHECK_IS_TEST();
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  auto& map = content::WebUIConfigMap::GetInstance();
  map.AddWebUIConfig(std::make_unique<AccessibilityUIConfig>());
  map.AddWebUIConfig(std::make_unique<BluetoothInternalsUIConfig>());
  map.AddWebUIConfig(std::make_unique<chrome_urls::ChromeUrlsUIConfig>());
  map.AddWebUIConfig(std::make_unique<chromeos::DeviceLogUIConfig>());
  map.AddWebUIConfig(std::make_unique<ComponentsUIConfig>());
  map.AddWebUIConfig(
      std::make_unique<security_interstitials::ConnectionHelpUIConfig>());
  map.AddWebUIConfig(
      std::make_unique<enterprise_connectors::ConnectorsInternalsUIConfig>());
  map.AddWebUIConfig(
      std::make_unique<content_settings_internals::ContentSettingsUIConfig>());
  map.AddWebUIConfig(std::make_unique<ConstrainedWebDialogUIConfig>());
  map.AddWebUIConfig(std::make_unique<CreditsUIConfig>());
  map.AddWebUIConfig(std::make_unique<DownloadInternalsUIConfig>());
  map.AddWebUIConfig(std::make_unique<FamilyLinkUserInternalsUIConfig>());
  map.AddWebUIConfig(std::make_unique<FlagsUIConfig>());
  map.AddWebUIConfig(
      std::make_unique<
          history_clusters_internals::HistoryClustersInternalsUIConfig>());
  map.AddWebUIConfig(std::make_unique<InternalDebugPagesDisabledUIConfig>());
  map.AddWebUIConfig(std::make_unique<InterstitialUIConfig>());
  map.AddWebUIConfig(std::make_unique<LocalStateUIConfig>());
  map.AddWebUIConfig(std::make_unique<LocationInternalsUIConfig>());
  map.AddWebUIConfig(std::make_unique<ManagementUIConfig>());
  map.AddWebUIConfig(std::make_unique<MediaEngagementUIConfig>());
  map.AddWebUIConfig(std::make_unique<MetricsInternalsUIConfig>());
  map.AddWebUIConfig(std::make_unique<NetExportUIConfig>());
  map.AddWebUIConfig(std::make_unique<NetInternalsUIConfig>());
  map.AddWebUIConfig(std::make_unique<OmniboxUIConfig>());
  map.AddWebUIConfig(std::make_unique<OptimizationGuideInternalsUIConfig>());
  map.AddWebUIConfig(std::make_unique<PolicyUIConfig>());
  map.AddWebUIConfig(std::make_unique<PredictorsUIConfig>());
  map.AddWebUIConfig(
      std::make_unique<
          security_interstitials::KnownInterceptionDisclosureUIConfig>());
  map.AddWebUIConfig(std::make_unique<SegmentationInternalsUIConfig>());
  map.AddWebUIConfig(std::make_unique<
                     subresource_filter::SubresourceFilterInternalsUIConfig>());
  map.AddWebUIConfig(std::make_unique<SiteEngagementUIConfig>());
  map.AddWebUIConfig(std::make_unique<SuggestInternalsUIConfig>());
  map.AddWebUIConfig(std::make_unique<UsbInternalsUIConfig>());
  map.AddWebUIConfig(std::make_unique<user_actions_ui::UserActionsUIConfig>());
  map.AddWebUIConfig(std::make_unique<VersionUIConfig>());
  map.AddWebUIConfig(std::make_unique<WebRtcLogsUIConfig>());
  map.AddWebUIConfig(std::make_unique<RegionalCapabilitiesInternalsUIConfig>());
#if BUILDFLAG(ENABLE_WEBUI_NTP)
#endif  // BUILDFLAG(ENABLE_WEBUI_NTP)

#if BUILDFLAG(ENABLE_WEBUI_CERTIFICATE_VIEWER)
  map.AddWebUIConfig(std::make_unique<CertificateViewerUIConfig>());
#endif

#if BUILDFLAG(IS_ANDROID)
  map.AddWebUIConfig(
      std::make_unique<chrome_finds_internals::ChromeFindsInternalsUIConfig>());
  map.AddWebUIConfig(std::make_unique<FeedInternalsUIConfig>());
  map.AddWebUIConfig(std::make_unique<NotificationsInternalsUIConfig>());
  map.AddWebUIConfig(std::make_unique<WebApksUIConfig>());
#else  // BUILDFLAG(IS_ANDROID)
#if !BUILDFLAG(IS_CHROMEOS)
#endif  // !BUILDFLAG(IS_CHROMEOS)
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
  map.AddWebUIConfig(std::make_unique<shuttle::AgentsUIConfig>());
  map.AddWebUIConfig(std::make_unique<ColorPipelineInternalsUIConfig>());
  map.AddWebUIConfig(std::make_unique<DownloadsUIConfig>());
  map.AddWebUIConfig(std::make_unique<HistoryUIConfig>());
  map.AddWebUIConfig(std::make_unique<InfobarInternalsUIConfig>());
  map.AddWebUIConfig(std::make_unique<InspectUIConfig>());
#if BUILDFLAG(ENABLE_SESSION_SERVICE)
  map.AddWebUIConfig(std::make_unique<InternalsUIConfig>());
  map.AddWebUIConfig(std::make_unique<TabStripInternalsUIConfig>());
#endif  // BUILDFLAG(ENABLE_SESSION_SERVICE)
  map.AddWebUIConfig(
      std::make_unique<
          multistep_filter_internals::MultistepFilterInternalsUIConfig>());
#if BUILDFLAG(ENABLE_WEBUI_NTP)
#endif  // BUILDFLAG(ENABLE_WEBUI_NTP)
  map.AddWebUIConfig(std::make_unique<NewTabUIConfig>());
  map.AddWebUIConfig(std::make_unique<OmniboxPopupUIConfig>());
  map.AddWebUIConfig(std::make_unique<PageActionInternalsUIConfig>());
  map.AddWebUIConfig(std::make_unique<settings::SettingsUIConfig>());
  map.AddWebUIConfig(std::make_unique<tabs::TabGroupHomeUIConfig>());
  map.AddWebUIConfig(std::make_unique<TermsUIConfig>());
  map.AddWebUIConfig(std::make_unique<WebNNInternalsUIConfig>());
  map.AddWebUIConfig(std::make_unique<WebuiGalleryUIConfig>());
  map.AddWebUIConfig(std::make_unique<WebUIJsErrorUIConfig>());
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_OPENBSD)
  map.AddWebUIConfig(std::make_unique<LinuxProxyConfigUI>());
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) ||
        // BUILDFLAG(IS_OPENBSD)

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  map.AddWebUIConfig(std::make_unique<extensions::ExtensionsUIConfig>());
#endif  // !BUILDFLAG(ENABLE_EXTENSIONS_CORE)

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
  map.AddWebUIConfig(std::make_unique<printing::PrintPreviewUIConfig>());
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)

#if BUILDFLAG(CHROME_ROOT_STORE_CERT_MANAGEMENT_UI)
  map.AddWebUIConfig(std::make_unique<CertificateManagerUIConfig>());
#endif  // BUILDFLAG(CHROME_ROOT_STORE_CERT_MANAGEMENT_UI)

#if BUILDFLAG(IS_CHROMEOS)
  map.AddWebUIConfig(std::make_unique<policy::DlpInternalsUIConfig>());
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_WIN)
  map.AddWebUIConfig(std::make_unique<ConflictsUIConfig>());
  map.AddWebUIConfig(std::make_unique<VisualGuidedSetterUIConfig>());
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  map.AddWebUIConfig(std::make_unique<BrowserSwitchUIConfig>());
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || \
    BUILDFLAG(IS_ANDROID)
  map.AddWebUIConfig(std::make_unique<SandboxInternalsUIConfig>());
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) ||
        // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_DESKTOP_ANDROID)
  map.AddWebUIConfig(std::make_unique<DiscardsUIConfig>());
#endif

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
  map.AddWebUIConfig(std::make_unique<UpdaterUIConfig>());
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)

#if BUILDFLAG(ENABLE_DICE_SUPPORT) || BUILDFLAG(IS_CHROMEOS)
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT) || BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
  map.AddWebUIConfig(std::make_unique<InlineLoginUIConfig>());
#endif

#if BUILDFLAG(FULL_SAFE_BROWSING)
#endif

#if !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_ANDROID)
#endif  //  !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
  map.AddWebUIConfig(
      std::make_unique<safe_browsing::ChromeSafeBrowsingUIConfig>());
#endif

#if BUILDFLAG(ENTERPRISE_WATERMARK)
  map.AddWebUIConfig(std::make_unique<WatermarkUIConfig>());
#endif

#if BUILDFLAG(IS_MAC)
  map.AddWebUIConfig(std::make_unique<UnexportableKeysInternalsUIConfig>());
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
        // BUILDFLAG(IS_CHROMEOS)
}
