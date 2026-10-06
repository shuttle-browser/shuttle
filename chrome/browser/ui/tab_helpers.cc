// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tab_helpers.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <utility>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/time/default_tick_clock.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/buildflags.h"
#include "chrome/browser/chained_back_navigation_tracker.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/content_settings/mixed_content_settings_tab_helper.h"
#include "chrome/browser/content_settings/page_specific_content_settings_delegate.h"
#include "chrome/browser/content_settings/sound_content_setting_observer.h"
#include "chrome/browser/external_protocol/external_protocol_observer.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/file_system_access/file_system_access_features.h"
#include "chrome/browser/file_system_access/file_system_access_permission_request_manager.h"
#include "chrome/browser/file_system_access/file_system_access_tab_helper.h"
#include "chrome/browser/finds/core/finds_features.h"
#include "chrome/browser/finds/core/finds_tab_helper.h"
#include "chrome/browser/finds/finds_service_factory.h"
#include "chrome/browser/history/history_tab_helper.h"
#include "chrome/browser/history/top_sites_factory.h"
#include "chrome/browser/history_clusters/history_clusters_tab_helper.h"
#include "chrome/browser/image_fetcher/image_fetcher_service_factory.h"
#include "chrome/browser/login_detection/login_detection_tab_helper.h"
#include "chrome/browser/lookalikes/safety_tip_web_contents_observer.h"
#include "chrome/browser/media/media_engagement_service.h"
#include "chrome/browser/metrics/desktop_session_duration/desktop_session_duration_observer.h"
#include "chrome/browser/navigation_predictor/navigation_predictor_preconnect_client.h"
#include "chrome/browser/net/net_error_tab_helper.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/optimization_guide/optimization_guide_web_contents_observer.h"
#include "chrome/browser/page_content_annotations/multi_source_page_context_fetcher.h"
#include "chrome/browser/page_content_annotations/page_content_annotations_service_factory.h"
#include "chrome/browser/page_content_annotations/page_content_extraction_service_factory.h"
#include "chrome/browser/page_info/about_this_site_tab_helper.h"
#include "chrome/browser/page_info/page_info_features.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_initialize.h"
#include "chrome/browser/permissions/one_time_permissions_tracker_helper.h"
#include "chrome/browser/picture_in_picture/auto_picture_in_picture_tab_helper.h"
#include "chrome/browser/predictors/loading_predictor_factory.h"
#include "chrome/browser/predictors/loading_predictor_tab_helper.h"
#include "chrome/browser/preloading/prefetch/no_state_prefetch/no_state_prefetch_manager_factory.h"
#include "chrome/browser/preloading/prefetch/no_state_prefetch/no_state_prefetch_tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/resource_coordinator/tab_helper.h"
#include "chrome/browser/safe_browsing/safe_browsing_navigation_observer_manager_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/sessions/session_tab_helper_factory.h"
#include "chrome/browser/site_protection/site_protection_metrics_observer.h"
#include "chrome/browser/ssl/chrome_security_blocking_page_factory.h"
#include "chrome/browser/ssl/https_only_mode_tab_helper.h"
#include "chrome/browser/storage_access_api/storage_access_api_service_factory.h"
#include "chrome/browser/storage_access_api/storage_access_api_service_impl.h"
#include "chrome/browser/storage_access_api/storage_access_api_tab_helper.h"
#include "chrome/browser/subresource_filter/chrome_content_subresource_filter_web_contents_helper_factory.h"
#include "chrome/browser/supervised_user/supervised_user_navigation_observer.h"
#include "chrome/browser/tab_contents/navigation_metrics_recorder.h"
#include "chrome/browser/task_manager/web_contents_tags.h"
#include "chrome/browser/ui/find_bar/find_bar_state.h"
#include "chrome/browser/ui/omnibox/omnibox_tab_helper.h"
#include "chrome/browser/ui/prefs/prefs_tab_helper.h"
#include "chrome/browser/ui/recently_audible_helper.h"
#include "chrome/browser/ui/search_engines/search_engine_tab_helper.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper.h"
#include "chrome/browser/ui/tab_dialogs.h"
#include "chrome/browser/ui/views/tab_sharing/tab_capture_contents_border_helper.h"
#include "chrome/browser/v8_compile_hints/v8_compile_hints_tab_helper.h"
#include "chrome/browser/vr/vr_tab_helper.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_isolated_world_ids.h"
#include "chrome/common/chrome_switches.h"
#include "components/blocked_content/popup_blocker_tab_helper.h"
#include "components/blocked_content/popup_opener_tab_helper.h"
#include "components/captive_portal/core/buildflags.h"
#include "components/client_hints/browser/client_hints_web_contents_observer.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/dom_distiller/core/dom_distiller_features.h"
#include "components/download/content/factory/navigation_monitor_factory.h"
#include "components/download/content/public/download_navigation_observer.h"
#include "components/enterprise/buildflags/buildflags.h"
#include "components/history/content/browser/web_contents_top_sites_observer.h"
#include "components/history/core/browser/top_sites.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/javascript_dialogs/tab_modal_dialog_manager.h"
#include "components/metrics/content/metrics_services_web_contents_observer.h"
#include "components/metrics_services_manager/metrics_services_manager.h"
#include "components/offline_pages/buildflags/buildflags.h"
#include "components/omnibox/common/omnibox_feature_configs.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/page_content_annotations/content/annotate_page_content_request.h"
#include "components/page_content_annotations/content/page_content_annotations_web_contents_observer.h"
#include "components/page_content_annotations/core/page_content_extraction_types.h"
#include "components/page_info/core/features.h"
#include "components/performance_manager/embedder/performance_manager_registry.h"
#include "components/performance_manager/public/features.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_recovery_success_rate_tracker.h"
#include "components/permissions/permission_request_manager.h"
#include "components/safe_browsing/content/browser/async_check_tracker.h"
#include "components/safe_browsing/content/browser/safe_browsing_navigation_observer.h"
#include "components/safe_browsing/content/browser/ui_manager.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/search/ntp_features.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/site_engagement/content/site_engagement_helper.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "components/tabs/public/tab_interface.h"
#include "components/tracing/common/tracing_switches.h"
#include "components/ukm/content/source_url_recorder.h"
#include "components/zoom/zoom_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/buildflags.h"
#include "extensions/buildflags/buildflags.h"
#include "media/base/media_switches.h"
#include "pdf/buildflags.h"
#include "printing/buildflags/buildflags.h"
#include "rlz/buildflags/buildflags.h"
#include "ui/accessibility/accessibility_features.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/android_info.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/android/oom_intervention/oom_intervention_tab_helper.h"
#include "chrome/browser/android/persisted_tab_data/language_persisted_tab_data_android.h"
#include "chrome/browser/android/persisted_tab_data/sensitivity_persisted_tab_data_android.h"
#include "chrome/browser/android/policy/policy_auditor_bridge.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/android/tab_web_contents_delegate_android.h"
#include "chrome/browser/banners/android/chrome_app_banner_manager_android.h"
#include "chrome/browser/content_settings/request_desktop_site_web_contents_observer_android.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/loader/from_gws_navigation_and_keep_alive_request_tab_helper.h"
#include "chrome/browser/plugins/plugin_observer_android.h"
#include "chrome/browser/ui/android/context_menu_helper.h"
#include "chrome/browser/ui/javascript_dialogs/javascript_tab_modal_dialog_manager_delegate_android.h"
#include "components/content_capture/common/content_capture_features.h"
#include "components/page_load_metrics/browser/features.h"
#include "content/public/common/content_features.h"
#else
#include "chrome/browser/ui/javascript_dialogs/javascript_tab_modal_dialog_manager_delegate_desktop.h"
#include "chrome/browser/ui/read_anything/read_anything_side_panel_controller.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/uma_browsing_activity_observer.h"
#include "components/image_fetcher/core/image_fetcher_service.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ash/boot_times_recorder/boot_times_recorder_tab_helper.h"
#include "chrome/browser/ash/child_accounts/time_limits/web_time_navigation_observer.h"
#include "chrome/browser/ash/growth/campaigns_manager_session_tab_helper.h"
#include "chrome/browser/ash/mahi/web_contents/mahi_tab_helper.h"
#include "chrome/browser/chromeos/gemini_app/gemini_app_tab_helper.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_content_tab_helper.h"
#include "chrome/browser/ui/ash/google_one/google_one_offer_iph_tab_helper.h"
#include "chromeos/ash/experiences/isolated_web_app/cros_isolated_web_app_enabler.h"
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ui/shared_highlighting/shared_highlighting_promo.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "chrome/browser/font_prewarmer_tab_helper.h"
#endif

#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
#include "chrome/browser/captive_portal/captive_portal_service_factory.h"
#include "components/captive_portal/content/captive_portal_tab_helper.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/app_tab_helper.h"
#include "chrome/browser/ui/extensions/extension_side_panel_utils.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/mojom/view_type.mojom.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
#include "chrome/browser/extensions/api/web_navigation/web_navigation_tab_observer.h"
#include "chrome/browser/extensions/navigation_extension_enabler.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "extensions/browser/view_type_utils.h"
#if !BUILDFLAG(IS_ANDROID)
#endif  // !BUILDFLAG(IS_ANDROID)
#endif  // BUILDFLAG(ENABLE_EXTENSIONS_CORE)

#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
#include "chrome/browser/offline_pages/android/auto_fetch_page_load_watcher.h"
#include "chrome/browser/offline_pages/offline_page_tab_helper.h"
#include "chrome/browser/offline_pages/recent_tab_helper.h"
#endif

#if BUILDFLAG(ENABLE_PLUGINS)
#include "chrome/browser/plugins/plugin_observer.h"
#endif

#if BUILDFLAG(ENABLE_PRINTING)
#include "chrome/browser/printing/printing_init.h"
#endif


#if BUILDFLAG(ENABLE_RLZ)
#include "chrome/browser/rlz/chrome_rlz_tracker_web_contents_observer.h"
#endif

#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
#include "chrome/browser/safe_browsing/chrome_safe_browsing_tab_observer_delegate.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/safe_browsing/trigger_creator.h"
#include "components/safe_browsing/content/browser/safe_browsing_tab_observer.h"
#endif

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
#include "components/signin/public/base/signin_switches.h"
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

using content::WebContents;

namespace {

const char kTabContentsAttachedTabHelpersUserDataKey[] =
    "TabContentsAttachedTabHelpers";

std::optional<int64_t> GetPageContentAnnotationsTabId(
    content::WebContents* web_contents) {
#if BUILDFLAG(IS_ANDROID)
  if (TabAndroid* tab = TabAndroid::FromWebContents(web_contents)) {
    return tab->GetAndroidId();
  }
#else
  SessionID id = sessions::SessionTabHelper::IdForTab(web_contents);
  if (id.is_valid()) {
    return id.id();
  }
#endif
  return std::nullopt;
}

}  // namespace

// static
// WARNING: Do not use this class for desktop chrome. Use TabFeatures instead.
// See
// https://chromium.googlesource.com/chromium/src/+/main/docs/chrome_browser_design_principles.md
void TabHelpers::AttachTabHelpers(WebContents* web_contents,
                                  bool enable_browser_autofill) {
  // If already adopted, nothing to be done.
  base::SupportsUserData::Data* adoption_tag =
      web_contents->GetUserData(&kTabContentsAttachedTabHelpersUserDataKey);
  if (adoption_tag) {
    return;
  }

  // Mark as adopted.
  web_contents->SetUserData(&kTabContentsAttachedTabHelpersUserDataKey,
                            std::make_unique<base::SupportsUserData::Data>());

  // Create all the tab helpers.

  // SessionTabHelper comes first because it sets up the tab ID, and other
  // helpers may rely on that.
  CreateSessionServiceTabHelper(web_contents);

  // ZoomController comes before common tab helpers since ChromeAutofillClient
  // may want to register as a ZoomObserver with it.
  zoom::ZoomController::CreateForWebContents(web_contents);

  // infobars::ContentInfoBarManager comes before common tab helpers since
  // ChromeSubresourceFilterClient has it as a dependency.
  infobars::ContentInfoBarManager::CreateForWebContents(web_contents);

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());

  // --- Section 1: Common tab helpers ---
  if (page_info::IsAboutThisSiteFeatureEnabled()) {
    if (auto* optimization_guide_decider =
            OptimizationGuideKeyedServiceFactory::GetForProfile(profile)) {
      AboutThisSiteTabHelper::CreateForWebContents(web_contents,
                                                   optimization_guide_decider);
    }
  }
#if BUILDFLAG(IS_ANDROID)
  if (base::FeatureList::IsEnabled(media::kAutoPictureInPictureAndroid)) {
    AutoPictureInPictureTabHelper::CreateForWebContents(web_contents);
  }
#else
  AutoPictureInPictureTabHelper::CreateForWebContents(web_contents);
#endif

  ChainedBackNavigationTracker::CreateForWebContents(web_contents);
  chrome_browser_net::NetErrorTabHelper::CreateForWebContents(web_contents);
  CreateSubresourceFilterWebContentsHelper(web_contents);
#if BUILDFLAG(ENABLE_RLZ)
  ChromeRLZTrackerWebContentsObserver::CreateForWebContentsIfNeeded(
      web_contents);
#endif
  client_hints::ClientHintsWebContentsObserver::CreateForWebContents(
      web_contents);
  content_settings::PageSpecificContentSettings::CreateForWebContents(
      web_contents,
      std::make_unique<PageSpecificContentSettingsDelegate>(web_contents));
  CoreTabHelper::CreateForWebContents(web_contents);
  ExternalProtocolObserver::CreateForWebContents(web_contents);
  favicon::CreateContentFaviconDriverForWebContents(web_contents);
  FileSystemAccessPermissionRequestManager::CreateForWebContents(web_contents);
  FileSystemAccessTabHelper::CreateForWebContents(web_contents);
  FindBarState::ConfigureWebContents(web_contents);
  download::DownloadNavigationObserver::CreateForWebContents(
      web_contents,
      download::NavigationMonitorFactory::GetForKey(profile->GetProfileKey()));
  history::WebContentsTopSitesObserver::CreateForWebContents(
      web_contents, TopSitesFactory::GetForProfile(profile).get());
  if (!profile->IsOffTheRecord()) {
    auto* history_tab_helper =
        HistoryTabHelper::GetOrCreateForWebContents(web_contents);
    HistoryClustersTabHelper::CreateForWebContents(web_contents,
                                                   history_tab_helper);
  }
  HttpsOnlyModeTabHelper::CreateForWebContents(web_contents);
  login_detection::LoginDetectionTabHelper::MaybeCreateForWebContents(
      web_contents);
  if (MediaEngagementService::IsEnabled()) {
    MediaEngagementService::CreateWebContentsObserver(web_contents);
  }
  if (auto* metrics_services_manager =
          g_browser_process->GetMetricsServicesManager()) {
    metrics::MetricsServicesWebContentsObserver::CreateForWebContents(
        web_contents, metrics_services_manager->GetOnDidStartLoadingCb(),
        metrics_services_manager->GetOnDidStopLoadingCb(),
        metrics_services_manager->GetOnRendererUnresponsiveCb());
  }
  MixedContentSettingsTabHelper::CreateForWebContents(web_contents);
  NavigationMetricsRecorder::CreateForWebContents(web_contents);
  NavigationPredictorPreconnectClient::CreateForWebContents(web_contents);
  if (optimization_guide::features::IsOptimizationHintsEnabled()) {
    OptimizationGuideWebContentsObserver::CreateForWebContents(web_contents);
  }
  page_content_annotations::PageContentAnnotationsService*
      page_content_annotations_service =
          PageContentAnnotationsServiceFactory::GetForProfile(profile);
  if (page_content_annotations_service) {
    page_content_annotations::PageContentAnnotationsWebContentsObserver::
        CreateForWebContents(web_contents, *page_content_annotations_service);

    // TODO(b/478883979): Consider decoupling this from
    // PageContentAnnotationsService.
    auto* page_content_extraction_service = page_content_annotations::
        PageContentExtractionServiceFactory::GetForProfile(profile);
    if (page_content_extraction_service) {
      page_content_annotations::AnnotatedPageContentRequest::
          CreateForWebContents(
              web_contents, *page_content_extraction_service,
              base::BindRepeating(&page_content_annotations::FetchPageContext),
              base::BindRepeating(&GetPageContentAnnotationsTabId));
    }

#if BUILDFLAG(IS_ANDROID)
    // If enabled, save sensitivity data for each non-incognito android tab.
    // TODO(crbug.com/40276584): Consider moving check conditions or the
    // registration logic to sensitivity_persisted_tab_data_android.*
    if (!profile->IsOffTheRecord()) {
      if (auto* tab = TabAndroid::FromWebContents(web_contents); tab) {
        SensitivityPersistedTabDataAndroid::From(
            tab,
            base::BindOnce(
                [](page_content_annotations::PageContentAnnotationsService*
                       page_content_annotations_service,
                   PersistedTabDataAndroid* persisted_tab_data) {
                  auto* sensitivity_persisted_tab_data_android =
                      static_cast<SensitivityPersistedTabDataAndroid*>(
                          persisted_tab_data);
                  sensitivity_persisted_tab_data_android->RegisterPCAService(
                      page_content_annotations_service);
                },
                page_content_annotations_service));
      }
    }
#endif  // BUILDFLAG(IS_ANDROID)
  }
  InitializePageLoadMetricsForWebContents(web_contents);
  if (auto* pm_registry =
          performance_manager::PerformanceManagerRegistry::GetInstance()) {
    pm_registry->SetPageType(web_contents, performance_manager::PageType::kTab);
  }
  permissions::PermissionRequestManager::CreateForWebContents(web_contents);
  permissions::PermissionRecoverySuccessRateTracker::CreateForWebContents(
      web_contents);
  // The PopupBlockerTabHelper has an implicit dependency on
  // ChromeSubresourceFilterClient being available in its constructor.
  blocked_content::PopupBlockerTabHelper::CreateForWebContents(web_contents);
  blocked_content::PopupOpenerTabHelper::CreateForWebContents(
      web_contents, base::DefaultTickClock::GetInstance(),
      HostContentSettingsMapFactory::GetForProfile(profile));
  if (predictors::LoadingPredictorFactory::GetForProfile(profile)) {
    predictors::LoadingPredictorTabHelper::CreateForWebContents(web_contents);
  }
  PrefsTabHelper::CreateForWebContents(web_contents);
  prerender::NoStatePrefetchTabHelper::CreateForWebContents(web_contents);
  RecentlyAudibleHelper::CreateForWebContents(web_contents);
#if BUILDFLAG(IS_ANDROID)
  RequestDesktopSiteWebContentsObserverAndroid::CreateForWebContents(
      web_contents);
#endif  // BUILDFLAG(IS_ANDROID)
  // TODO(siggi): Remove this once the Resource Coordinator refactoring is done.
  //     See https://crbug.com/40604438.
  resource_coordinator::ResourceCoordinatorTabHelper::CreateForWebContents(
      web_contents);
#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
  safe_browsing::SafeBrowsingNavigationObserver::MaybeCreateForWebContents(
      web_contents, HostContentSettingsMapFactory::GetForProfile(profile),
      safe_browsing::SafeBrowsingNavigationObserverManagerFactory::
          GetForBrowserContext(profile),
      profile->GetPrefs(), g_browser_process->safe_browsing_service());
#endif
  site_protection::SiteProtectionMetricsObserver::CreateForWebContents(
      web_contents);

#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
  if (g_browser_process->safe_browsing_service()) {
    safe_browsing::AsyncCheckTracker::CreateForWebContents(
        web_contents, g_browser_process->safe_browsing_service()->ui_manager(),
        safe_browsing::AsyncCheckTracker::
            IsPlatformEligibleForSyncCheckerCheckAllowlist());
  }
  // SafeBrowsingTabObserver creates a ClientSideDetectionHost, which observes
  // events from PermissionRequestManager and AsyncCheckTracker in its
  // constructor. Therefore, PermissionRequestManager and AsyncCheckTracker need
  // to be created before SafeBrowsingTabObserver is created.
  if (enable_browser_autofill) {
    safe_browsing::SafeBrowsingTabObserver::CreateForWebContents(
        web_contents,
        std::make_unique<
            safe_browsing::ChromeSafeBrowsingTabObserverDelegate>());
  }
  safe_browsing::TriggerCreator::MaybeCreateTriggersForWebContents(
      profile, web_contents);
#endif  // BUILDFLAG(SAFE_BROWSING_AVAILABLE)
  SafetyTipWebContentsObserver::CreateForWebContents(web_contents);
  SearchEngineTabHelper::CreateForWebContents(web_contents);
  if (site_engagement::SiteEngagementService::IsEnabled()) {
    site_engagement::SiteEngagementService::Helper::CreateForWebContents(
        web_contents,
        prerender::NoStatePrefetchManagerFactory::GetForBrowserContext(
            profile));
  }
  SoundContentSettingObserver::CreateForWebContents(web_contents);
  StorageAccessAPITabHelper::CreateForWebContents(
      web_contents, StorageAccessAPIServiceFactory::GetForBrowserContext(
                        web_contents->GetBrowserContext()));
#if BUILDFLAG(IS_CHROMEOS)
  // Do not create for Incognito mode.
  if (!profile->IsIncognitoProfile()) {
    SupervisedUserNavigationObserver::CreateForWebContents(web_contents);
  }
#else
  // Do not create for OTR.
  if (!profile->IsOffTheRecord()) {
    SupervisedUserNavigationObserver::CreateForWebContents(web_contents);
  }
#endif
#if !BUILDFLAG(IS_ANDROID)
  TabCaptureContentsBorderHelper::CreateForWebContents(web_contents);
#endif  // BUILDFLAG(IS_ANDROID)
  ukm::InitializeSourceUrlRecorderForWebContents(web_contents);
  v8_compile_hints::V8CompileHintsTabHelper::MaybeCreateForWebContents(
      web_contents);
  vr::VrTabHelper::CreateForWebContents(web_contents);
  OneTimePermissionsTrackerHelper::CreateForWebContents(web_contents);

  // NO! Do not just add your tab helper here. This is a large alphabetized
  // block; please insert your tab helper above in alphabetical order.

  // --- Section 2: Platform-specific tab helpers ---

#if BUILDFLAG(IS_ANDROID)
  webapps::MLInstallabilityPromoter::CreateForWebContents(web_contents);
  {
    // Remove after fixing https://crbug.com/41426655
    TRACE_EVENT0("browser", "AppBannerManagerAndroid::CreateForWebContents");
    webapps::AppBannerManagerAndroid::CreateForWebContents(
        web_contents, std::make_unique<webapps::ChromeAppBannerManagerAndroid>(
                          *web_contents));
  }
  ContextMenuHelper::CreateForWebContents(web_contents);

  if (base::FeatureList::IsEnabled(finds::features::kChromeFinds)) {
    if (auto* finds_service =
            finds::FindsServiceFactory::GetForProfile(profile)) {
      finds::FindsTabHelper::CreateForWebContents(
          web_contents, finds_service,
          OptimizationGuideKeyedServiceFactory::GetForProfile(profile),
          TemplateURLServiceFactory::GetForProfile(profile),
          profile->GetPrefs());
    }
  }

  if (base::FeatureList::IsEnabled(
          page_load_metrics::features::kBeaconLeakageLogging)) {
    FromGWSNavigationAndKeepAliveRequestTabHelper::CreateForWebContents(
        web_contents);
  }

  javascript_dialogs::TabModalDialogManager::CreateForWebContents(
      web_contents,
      std::make_unique<JavaScriptTabModalDialogManagerDelegateAndroid>(
          web_contents));
  if (OomInterventionTabHelper::IsEnabled()) {
    OomInterventionTabHelper::CreateForWebContents(web_contents);
  }
  PolicyAuditorBridge::CreateForWebContents(web_contents);
  PluginObserverAndroid::CreateForWebContents(web_contents);
  task_manager::WebContentsTags::CreateForTabContents(web_contents);

#else   // BUILDFLAG(IS_ANDROID)
  javascript_dialogs::TabModalDialogManager::CreateForWebContents(
      web_contents,
      std::make_unique<JavaScriptTabModalDialogManagerDelegateDesktop>(
          web_contents));

  // Formerly created by SearchTabHelper; ToastController and the omnibox
  // observe it on every tab.
  OmniboxTabHelper::CreateForWebContents(web_contents, profile);
  TabDialogs::CreateForWebContents(web_contents);
  UMABrowsingActivityObserver::TabHelper::CreateForWebContents(web_contents);
  web_modal::WebContentsModalDialogManager::CreateForWebContents(web_contents);
#endif  // BUILDFLAG(IS_ANDROID)


#if BUILDFLAG(IS_CHROMEOS)
  GoogleOneOfferIphTabHelper::CreateForWebContents(web_contents);
  // Do not create for Incognito mode.
  if (!profile->IsOffTheRecord()) {
    CampaignsManagerSessionTabHelper::CreateForWebContents(web_contents);
  }
  ash::BootTimesRecorderTabHelper::MaybeCreateForWebContents(web_contents);

  ash::CrosIsolatedWebAppEnabler::CreateForWebContents(web_contents);
  GeminiAppTabHelper::MaybeCreateForWebContents(web_contents);
  mahi::MahiTabHelper::MaybeCreateForWebContents(web_contents);
  policy::DlpContentTabHelper::MaybeCreateForWebContents(web_contents);
  ash::app_time::WebTimeNavigationObserver::MaybeCreateForWebContents(
      web_contents);
#endif


#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  metrics::DesktopSessionDurationObserver::CreateForWebContents(web_contents);
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
  SharedHighlightingPromo::CreateForWebContents(web_contents);
#endif

#if BUILDFLAG(IS_WIN)
  FontPrewarmerTabHelper::CreateForWebContents(web_contents);
#endif

  // --- Section 3: Feature tab helpers behind BUILDFLAGs ---
  // NOT for "if enabled"; put those in section 1.

#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
  captive_portal::CaptivePortalTabHelper::CreateForWebContents(
      web_contents, CaptivePortalServiceFactory::GetForProfile(profile),
      base::BindRepeating(
          &ChromeSecurityBlockingPageFactory::OpenLoginTabForWebContents,
          web_contents, false));
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  // If the web contents already have a view type, don't overwrite it here. One
  // case where this can happen is when the user opens undocked developer tools.
  // For all developer tools web contents, the view type is set to
  // `kDeveloperTools` by the `DevToolsWindow` before tab helpers are attached.
  if (extensions::GetViewType(web_contents) ==
      extensions::mojom::ViewType::kInvalid) {
    extensions::SetViewType(web_contents,
                            extensions::mojom::ViewType::kTabContents);
  }
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
  extensions::AppTabHelper::CreateForWebContents(web_contents);
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  // These helpers are used on Win/Mac/Linux and also desktop Android.
  extensions::NavigationExtensionEnabler::CreateForWebContents(web_contents);
  extensions::WebNavigationTabObserver::CreateForWebContents(web_contents);
  extensions::TabHelper::CreateForWebContents(web_contents);
#endif  // BUILDFLAG(ENABLE_EXTENSIONS_CORE)

#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
  offline_pages::OfflinePageTabHelper::CreateForWebContents(web_contents);
  offline_pages::RecentTabHelper::CreateForWebContents(web_contents);
  offline_pages::AutoFetchPageLoadWatcher::CreateForWebContents(web_contents);
#endif

#if BUILDFLAG(ENABLE_PLUGINS)
  PluginObserver::CreateForWebContents(web_contents);
#endif

#if BUILDFLAG(ENABLE_PRINTING)
  printing::InitializePrintingForWebContents(web_contents);
#endif

  // --- Section 4: The warning ---

  // NONO    NO   NONONO   !
  // NO NO   NO  NO    NO  !
  // NO  NO  NO  NO    NO  !
  // NO   NO NO  NO    NO  !
  // NO    NONO   NONONO   !

  // Do NOT just drop your tab helpers here! There are three sections above (1.
  // All platforms, 2. Some platforms, 3. Behind BUILDFLAGs). Each is in rough
  // alphabetical order. PLEASE PLEASE PLEASE add your flag to the correct
  // section in the correct order.

  // This is common code for all of us. PLEASE DO YOUR PART to keep it tidy and
  // organized.
}
