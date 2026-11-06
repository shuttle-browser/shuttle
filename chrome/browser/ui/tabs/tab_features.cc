// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/public/tab_features.h"

#include <memory>
#include <optional>

#include "base/feature_list.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/default_tick_clock.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chained_back_navigation_tracker.h"
#include "chrome/browser/content_settings/mixed_content_settings_tab_helper.h"
#include "chrome/browser/content_settings/page_specific_content_settings_delegate.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/file_system_access/file_system_access_permission_request_manager.h"
#include "chrome/browser/file_system_access/file_system_access_tab_helper.h"
#include "chrome/browser/history/history_tab_helper.h"
#include "chrome/browser/history/top_sites_factory.h"
#include "chrome/browser/login_detection/login_detection_tab_helper.h"
#include "chrome/browser/lookalikes/safety_tip_web_contents_observer.h"
#include "chrome/browser/media/media_engagement_service.h"
#include "chrome/browser/net/net_error_tab_helper.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_initialize.h"
#include "chrome/browser/permissions/one_time_permissions_tracker_helper.h"
#include "chrome/browser/predictors/loading_predictor_factory.h"
#include "chrome/browser/predictors/loading_predictor_tab_helper.h"
#include "chrome/browser/preloading/prefetch/no_state_prefetch/no_state_prefetch_manager_factory.h"
#include "chrome/browser/resource_coordinator/tab_helper.h"
#include "chrome/browser/sessions/session_tab_helper_factory.h"
#include "chrome/browser/ssl/https_only_mode_tab_helper.h"
#include "chrome/browser/subresource_filter/chrome_content_subresource_filter_web_contents_helper_factory.h"
#include "chrome/browser/ui/find_bar/find_bar_state.h"
#include "chrome/browser/ui/javascript_dialogs/javascript_tab_modal_dialog_manager_delegate_desktop.h"
#include "chrome/browser/ui/omnibox/omnibox_tab_helper.h"
#include "chrome/browser/ui/prefs/prefs_tab_helper.h"
#include "chrome/browser/ui/recently_audible_helper.h"
#include "chrome/browser/ui/search_engines/search_engine_tab_helper.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper.h"
#include "chrome/browser/ui/tab_dialogs.h"
#include "chrome/browser/ui/tabs/alert/child_tab_alert_helper.h"
#include "components/blocked_content/popup_blocker_tab_helper.h"
#include "components/blocked_content/popup_opener_tab_helper.h"
#include "components/client_hints/browser/client_hints_web_contents_observer.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/history/content/browser/web_contents_top_sites_observer.h"
#include "components/history/core/browser/top_sites.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/javascript_dialogs/tab_modal_dialog_manager.h"
#include "components/metrics/content/metrics_services_web_contents_observer.h"
#include "components/metrics_services_manager/metrics_services_manager.h"
#include "components/performance_manager/embedder/performance_manager_registry.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/permissions/permission_recovery_success_rate_tracker.h"
#include "components/permissions/permission_request_manager.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/sessions/core/session_id.h"
#include "components/site_engagement/content/site_engagement_helper.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "components/ukm/content/source_url_recorder.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "components/zoom/zoom_controller.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/content_settings/sound_content_setting_observer.h"
#include "chrome/browser/external_protocol/external_protocol_observer.h"
#include "chrome/browser/loader/from_gws_navigation_and_keep_alive_request_observer.h"
#include "chrome/browser/navigation_predictor/navigation_predictor_preconnect_client.h"
#include "chrome/browser/net/http_auth_cache_status.h"
#include "chrome/browser/net/qwac_web_contents_observer.h"
#include "chrome/browser/preloading/prefetch/no_state_prefetch/no_state_prefetch_tab_helper.h"
#include "chrome/browser/preloading/prefetch/zero_suggest_prefetch/zero_suggest_prefetch_tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/site_protection/site_protection_metrics_observer.h"
#include "chrome/browser/ssl/ask_before_http_dialog_controller.h"
#include "chrome/browser/ssl/connection_help_tab_helper.h"
#include "chrome/browser/ssl/security_state_event_observer.h"
#include "chrome/browser/storage_access_api/storage_access_api_service_factory.h"
#include "chrome/browser/storage_access_api/storage_access_api_service_impl.h"
#include "chrome/browser/storage_access_api/storage_access_api_tab_helper.h"
#include "chrome/browser/tab_contents/form_interaction_tab_helper.h"
#include "chrome/browser/tab_contents/navigation_metrics_recorder.h"
#include "chrome/browser/task_manager/web_contents_tags.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/blocked_content/framebust_block_tab_helper.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/context_highlight/context_highlight_tab_feature.h"
#include "chrome/browser/ui/focus_tab_after_navigation_helper.h"
#include "chrome/browser/ui/page_action/action_ids.h"
#include "chrome/browser/ui/page_action/page_action_controller.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "chrome/browser/ui/page_action/page_action_properties_provider.h"
#include "chrome/browser/ui/sad_tab_helper.h"
#include "chrome/browser/ui/tab_ui_helper.h"
#include "chrome/browser/ui/tabs/alert/tab_alert_controller.h"
#include "chrome/browser/ui/tabs/back_to_opener/back_to_opener_controller.h"
#include "chrome/browser/ui/tabs/inactive_window_mouse_event_controller.h"
#include "chrome/browser/ui/tabs/public/tab_dialog_manager.h"
#include "chrome/browser/ui/tabs/tab_creation_metrics_controller.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_delegate.h"
#include "chrome/browser/ui/thumbnails/thumbnail_tab_helper.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_toolbar_actions_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/uma_browsing_activity_observer.h"
#include "chrome/browser/ui/views/file_system_access/file_system_access_page_action_controller.h"
#include "chrome/browser/ui/views/js_optimization/js_optimizations_page_action_controller.h"
#include "chrome/browser/ui/views/location_bar/cookie_controls/cookie_controls_page_action_controller.h"
#include "chrome/browser/ui/views/tab_sharing/tab_capture_contents_border_helper.h"
#include "chrome/browser/ui/views/zoom/zoom_view_controller.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "components/download/content/factory/navigation_monitor_factory.h"
#include "components/download/content/public/download_navigation_observer.h"
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
#include "chrome/browser/metrics/desktop_session_duration/desktop_session_duration_observer.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "chrome/browser/metrics/oom/commit_limit_oom_recovery_tracker.h"
#endif
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/tabs/tab_attachment_tracker.h"
#include "chrome/common/chrome_features.h"
#include "components/favicon/content/content_favicon_driver.h"
#include "components/permissions/permission_indicators_tab_data.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/security_interstitials/core/features.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_controller.h"
#include "net/base/features.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/base/unowned_user_data/user_data_factory.h"

#if BUILDFLAG(IS_WIN)
#include "chrome/browser/font_prewarmer_tab_helper.h"
#endif

#if BUILDFLAG(ENABLE_RLZ)
#include "chrome/browser/rlz/chrome_rlz_tracker_web_contents_observer.h"
#endif

#if BUILDFLAG(ENABLE_PLUGINS)
#include "chrome/browser/plugins/plugin_observer.h"
#endif

#include "components/captive_portal/core/buildflags.h"
#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
#include "chrome/browser/captive_portal/captive_portal_service_factory.h"
#include "chrome/browser/ssl/chrome_security_blocking_page_factory.h"
#include "components/captive_portal/content/captive_portal_tab_helper.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/app_tab_helper.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
#include "chrome/browser/extensions/api/web_navigation/web_navigation_tab_observer.h"
#include "chrome/browser/extensions/navigation_extension_enabler.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "extensions/browser/view_type_utils.h"
#include "extensions/common/mojom/view_type.mojom.h"
#endif

#include "printing/buildflags/buildflags.h"
#if BUILDFLAG(ENABLE_PRINTING)
#include "chrome/browser/printing/printing_init.h"
#endif

#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
#include "chrome/browser/safe_browsing/chrome_safe_browsing_tab_observer_delegate.h"
#include "chrome/browser/safe_browsing/safe_browsing_navigation_observer_manager_factory.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/safe_browsing/trigger_creator.h"
#include "components/safe_browsing/content/browser/async_check_tracker.h"
#include "components/safe_browsing/content/browser/safe_browsing_navigation_observer.h"
#include "components/safe_browsing/content/browser/safe_browsing_tab_observer.h"
#include "components/safe_browsing/content/browser/ui_manager.h"
#endif

namespace tabs {

TabFeatures::TabFeatures() = default;
TabFeatures::~TabFeatures() = default;

void TabFeatures::Init(TabInterface& tab, Profile* profile) {
  CHECK(!initialized_);
  initialized_ = true;

  // In tests you may want to disable TabFeatures initialization.
  // See tabs::TabModel::PreventFeatureInitializationForTesting
  CHECK(tab.GetBrowserWindowInterface());

  tab_subscriptions_.push_back(
      tab.RegisterWillDiscardContents(base::BindRepeating(
          &TabFeatures::WillDiscardContents, weak_factory_.GetWeakPtr())));
  tab_subscriptions_.push_back(webui::InitEmbeddingContext(&tab));

  CreateSubresourceFilterWebContentsHelper(tab.GetContents());

  if (!content_settings::PageSpecificContentSettings::GetDelegateForWebContents(
          tab.GetContents())) {
    content_settings::PageSpecificContentSettings::CreateForWebContents(
        tab.GetContents(),
        std::make_unique<PageSpecificContentSettingsDelegate>(
            tab.GetContents()));
  }

  favicon::CreateContentFaviconDriverForWebContents(tab.GetContents());

  if (!profile->IsOffTheRecord()) {
    HistoryTabHelper::GetOrCreateForWebContents(tab.GetContents());
  }

  InitializePageLoadMetricsForWebContents(tab.GetContents());

  // This block instantiate the page action controllers. They do not require any
  // pre-condition. Because some feature need them during their instantiation,
  // therefore this block should come before the feature controllers
  // instantiation.
  auto* pinned_actions_model = PinnedToolbarActionsModel::Get(profile);
  CHECK(pinned_actions_model);
  page_action_controller_ =
      GetUserDataFactory()
          .CreateInstance<page_actions::PageActionControllerImpl>(
              tab, tab,
              page_actions::GetActivePageActionIds(
                  *tab.GetBrowserWindowInterface()),
              page_actions::PageActionPropertiesProvider(),
              pinned_actions_model);

  file_system_access_permission_request_manager_ =
      GetUserDataFactory()
          .CreateInstance<FileSystemAccessPermissionRequestManager>(tab, tab);
  if (page_action_controller_->ActionExists(kActionShowFileSystemAccess)) {
    file_system_access_page_action_controller_ =
        std::make_unique<FileSystemAccessPageActionController>(tab);
  }

  if (page_action_controller_->ActionExists(kActionShowZoomBubble)) {
    zoom_view_controller_ = std::make_unique<zoom::ZoomViewController>(
        tab, *page_action_controller_);
  }

  tab_dialogs_ = TabDialogs::Create(tab, tab.GetContents());

  if (page_action_controller_->ActionExists(kActionShowCookieControls)) {
    cookie_controls_page_action_controller_ =
        GetUserDataFactory().CreateInstance<CookieControlsPageActionController>(
            tab, tab, *profile, *page_action_controller_);
    cookie_controls_page_action_controller_->Init();
  }


  if (page_action_controller_->ActionExists(kActionShowJsOptimizationsIcon)) {
    js_optimizations_page_action_controller_ =
        std::make_unique<JsOptimizationsPageActionController>(
            tab, *page_action_controller_);
  }

  // Features that are only enabled for normal browser windows. By default most
  // features should be instantiated in this block.
  if (tab.IsInNormalWindow()) {
    permission_indicators_tab_data_ =
        std::make_unique<permissions::PermissionIndicatorsTabData>(
            tab.GetContents());

  }  // IsInNormalWindow() end.

  tab_dialog_manager_ = std::make_unique<TabDialogManager>(&tab);

  // Create the HttpAuthCacheStatus to start observing resource load
  // completions.
  http_auth_cache_status_ =
      std::make_unique<HttpAuthCacheStatus>(tab.GetContents());

  security_state_event_observer_ =
      std::make_unique<SecurityStateEventObserver>(tab.GetContents());

  focus_tab_after_navigation_helper_ =
      std::make_unique<FocusTabAfterNavigationHelper>(tab.GetContents());

  framebust_block_tab_helper_ =
      GetUserDataFactory().CreateInstance<FramebustBlockTabHelper>(
          tab, tab, tab.GetContents());

  connection_help_tab_helper_ =
      GetUserDataFactory().CreateInstance<ConnectionHelpTabHelper>(
          tab, tab, tab.GetContents());

  form_interaction_tab_helper_ =
      GetUserDataFactory().CreateInstance<FormInteractionTabHelper>(tab, tab);

  zero_suggest_prefetch_tab_helper_ =
      std::make_unique<ZeroSuggestPrefetchTabHelper>(tab.GetContents());

  if (base::FeatureList::IsEnabled(features::kTabHoverCardImages)) {
    thumbnail_tab_helper_ =
        GetUserDataFactory().CreateInstance<ThumbnailTabHelper>(
            tab, tab, tab.GetContents());
  }

  sad_tab_helper_ = GetUserDataFactory().CreateInstance<SadTabHelper>(
      tab, tab, tab.GetContents());

  from_gws_navigation_and_keep_alive_request_observer_ =
      FromGWSNavigationAndKeepAliveRequestObserver::MaybeCreate(
          tab.GetContents());

  tab_creation_metrics_controller_ =
      std::make_unique<TabCreationMetricsController>(&tab);

  tab_attachment_tracker_ =
      GetUserDataFactory().CreateInstance<TabAttachmentTracker>(tab, &tab);

  tab_ui_helper_ = GetUserDataFactory().CreateInstance<TabUIHelper>(tab, tab);

  task_manager::WebContentsTags::CreateForTabContents(tab.GetContents());

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  desktop_session_duration_observer_ =
      metrics::DesktopSessionDurationObserver::MaybeCreate(tab.GetContents());
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
  inactive_window_mouse_event_controller_ =
      std::make_unique<InactiveWindowMouseEventController>();


#endif

#if BUILDFLAG(IS_WIN)
  commit_limit_oom_recovery_tracker_ =
      GetUserDataFactory().CreateInstance<CommitLimitOOMRecoveryTracker>(tab,
                                                                         tab);
  font_prewarmer_tab_helper_ =
      std::make_unique<FontPrewarmerTabHelper>(tab.GetContents());
#endif

  if (base::FeatureList::IsEnabled(net::features::kVerifyQWACs)) {
    qwac_web_contents_observer_ =
        std::make_unique<QwacWebContentsObserver>(tab);
  }

  if (base::FeatureList::IsEnabled(
          security_interstitials::features::kHttpsFirstDialogUi)) {
    ask_before_http_dialog_controller_ =
        GetUserDataFactory().CreateInstance<AskBeforeHttpDialogController>(
            tab, &tab);
  }

  context_highlight_tab_feature_ =
      GetUserDataFactory().CreateInstance<ContextHighlightTabFeature>(tab, tab);

  recently_audible_helper_ =
      GetUserDataFactory().CreateInstance<RecentlyAudibleHelper>(
          tab, tab, tab.GetContents());

  child_tab_alert_helper_ =
      GetUserDataFactory().CreateInstance<ChildTabAlertHelper>(tab, tab);

  tab_alert_controller_ =
      GetUserDataFactory().CreateInstance<TabAlertController>(tab, tab);


  // The controller is created for all tabs but only affects back button
  // behavior for destination tabs with opener relationships.
  if (base::FeatureList::IsEnabled(tabs::kBackToOpener)) {
    back_to_opener_controller_ =
        GetUserDataFactory()
            .CreateInstance<back_to_opener::BackToOpenerController>(tab, tab);
  }

  uma_browsing_activity_tab_helper_ =
      std::make_unique<UMABrowsingActivityTabHelper>(tab.GetContents());

#if BUILDFLAG(ENABLE_RLZ)
  chrome_rlz_tracker_web_contents_observer_ =
      ChromeRLZTrackerWebContentsObserver::MaybeCreate(tab.GetContents());
#endif

#if BUILDFLAG(ENABLE_PLUGINS)
  plugin_observer_ = GetUserDataFactory().CreateInstance<PluginObserver>(
      tab, tab, tab.GetContents());
#endif

  tab_capture_contents_border_helper_ =
      GetUserDataFactory().CreateInstance<TabCaptureContentsBorderHelper>(tab,
                                                                          tab);

#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
  captive_portal::CaptivePortalTabHelper::CreateForWebContents(
      tab.GetContents(), CaptivePortalServiceFactory::GetForProfile(profile),
      base::BindRepeating(
          &ChromeSecurityBlockingPageFactory::OpenLoginTabForWebContents,
          tab.GetContents(), false));
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
  app_tab_helper_ =
      std::make_unique<extensions::AppTabHelper>(tab, tab.GetContents());
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  // If the web contents already have a view type, don't overwrite it here. One
  // case where this can happen is when the user opens undocked developer tools.
  // For all developer tools web contents, the view type is set to
  // `kDeveloperTools` by the `DevToolsWindow` before tab helpers are attached.
  if (extensions::GetViewType(tab.GetContents()) ==
      extensions::mojom::ViewType::kInvalid) {
    extensions::SetViewType(tab.GetContents(),
                            extensions::mojom::ViewType::kTabContents);
  }
  extensions::WebNavigationTabObserver::CreateForWebContents(tab.GetContents());
  extensions::TabHelper::CreateForWebContents(tab.GetContents());
  navigation_extension_enabler_ =
      std::make_unique<extensions::NavigationExtensionEnabler>(
          tab.GetContents());
#endif

  storage_access_api_tab_helper_ = std::make_unique<StorageAccessAPITabHelper>(
      tab.GetContents(),
      StorageAccessAPIServiceFactory::GetForBrowserContext(profile));

  external_protocol_observer_ =
      std::make_unique<ExternalProtocolObserver>(tab.GetContents());

  no_state_prefetch_tab_helper_ =
      std::make_unique<prerender::NoStatePrefetchTabHelper>(tab.GetContents());

  navigation_predictor_preconnect_client_ =
      std::make_unique<NavigationPredictorPreconnectClient>(tab.GetContents());

  navigation_metrics_recorder_ =
      std::make_unique<NavigationMetricsRecorder>(tab.GetContents());

  site_protection_metrics_observer_ =
      std::make_unique<site_protection::SiteProtectionMetricsObserver>(
          tab.GetContents());

  permissions::PermissionRequestManager::CreateForWebContents(
      tab.GetContents());

#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
  safe_browsing::SafeBrowsingNavigationObserver::MaybeCreateForWebContents(
      tab.GetContents(), HostContentSettingsMapFactory::GetForProfile(profile),
      safe_browsing::SafeBrowsingNavigationObserverManagerFactory::
          GetForBrowserContext(profile),
      profile->GetPrefs(), g_browser_process->safe_browsing_service(),
      /*is_referrer_chain_needed_for_enterprise=*/false);
  if (g_browser_process->safe_browsing_service()) {
    safe_browsing::AsyncCheckTracker::CreateForWebContents(
        tab.GetContents(),
        g_browser_process->safe_browsing_service()->ui_manager(),
        safe_browsing::AsyncCheckTracker::
            IsPlatformEligibleForSyncCheckerCheckAllowlist());
  }
  // Shuttle: no autofill client, so not gated on ContentAutofillClient.
  safe_browsing_tab_observer_ =
      GetUserDataFactory()
          .CreateInstance<safe_browsing::SafeBrowsingTabObserver>(
              tab, tab, tab.GetContents(),
              std::make_unique<
                  safe_browsing::ChromeSafeBrowsingTabObserverDelegate>());
  trigger_creator_ = std::make_unique<safe_browsing::TriggerCreator>(
      tab, profile, tab.GetContents());
#endif

  sound_content_setting_observer_ =
      GetUserDataFactory().CreateInstance<SoundContentSettingObserver>(
          tab, tab, tab.GetContents());

  download_navigation_observer_ =
      std::make_unique<download::DownloadNavigationObserver>(
          tab.GetContents(), download::NavigationMonitorFactory::GetForKey(
                                 profile->GetProfileKey()));

  web_contents_top_sites_observer_ =
      std::make_unique<history::WebContentsTopSitesObserver>(
          tab.GetContents(), TopSitesFactory::GetForProfile(profile).get());

  client_hints_web_contents_observer_ =
      std::make_unique<client_hints::ClientHintsWebContentsObserver>(
          tab.GetContents());

  chained_back_navigation_tracker_ =
      GetUserDataFactory().CreateInstance<ChainedBackNavigationTracker>(
          tab, tab, tab.GetContents());

  file_system_access_tab_helper_ =
      std::make_unique<FileSystemAccessTabHelper>(tab.GetContents());

  search_engine_tab_helper_ =
      GetUserDataFactory().CreateInstance<SearchEngineTabHelper>(
          tab, tab, tab.GetContents());

  net_error_tab_helper_ =
      GetUserDataFactory()
          .CreateInstance<chrome_browser_net::NetErrorTabHelper>(
              tab, tab, tab.GetContents());

  // Shuttle: SearchTabHelper is removed; it used to create OmniboxTabHelper,
  // which ToastController and the omnibox observe on every tab.
  OmniboxTabHelper::CreateForWebContents(tab.GetContents(), profile);

  if (auto* metrics_services_manager =
          g_browser_process->GetMetricsServicesManager()) {
    metrics_services_web_contents_observer_ =
        std::make_unique<metrics::MetricsServicesWebContentsObserver>(
            tab.GetContents(),
            metrics_services_manager->GetOnDidStartLoadingCb(),
            metrics_services_manager->GetOnDidStopLoadingCb(),
            metrics_services_manager->GetOnRendererUnresponsiveCb());
  }

  popup_opener_tab_helper_ =
      GetUserDataFactory()
          .CreateInstance<blocked_content::PopupOpenerTabHelper>(
              tab, tab, tab.GetContents(),
              base::DefaultTickClock::GetInstance(),
              HostContentSettingsMapFactory::GetForProfile(profile));

  core_tab_helper_ = GetUserDataFactory().CreateInstance<CoreTabHelper>(
      tab, tab, tab.GetContents());

  FindBarState::ConfigureWebContents(tab.GetContents());

  if (MediaEngagementService::IsEnabled()) {
    MediaEngagementService::CreateWebContentsObserver(tab.GetContents());
  }

#if BUILDFLAG(ENABLE_PRINTING)
  printing::InitializePrintingForWebContents(tab.GetContents());
#endif

  mixed_content_settings_tab_helper_ =
      GetUserDataFactory().CreateInstance<MixedContentSettingsTabHelper>(
          tab, tab, tab.GetContents());

  permissions::PermissionRecoverySuccessRateTracker::CreateForWebContents(
      tab.GetContents());

  javascript_dialogs::TabModalDialogManager::CreateForWebContents(
      tab.GetContents(),
      std::make_unique<JavaScriptTabModalDialogManagerDelegateDesktop>(
          tab.GetContents()));

  web_modal::WebContentsModalDialogManager::CreateForWebContents(
      tab.GetContents());

  // Track one-time permissions for the tab.
  one_time_permissions_tracker_helper_ =
      std::make_unique<OneTimePermissionsTrackerHelper>(tab.GetContents());

  if (predictors::LoadingPredictorFactory::GetForProfile(profile)) {
    loading_predictor_tab_helper_ =
        GetUserDataFactory()
            .CreateInstance<predictors::LoadingPredictorTabHelper>(
                tab, tab, tab.GetContents());
  }

  PrefsTabHelper::CreateForWebContents(tab.GetContents());

  if (site_engagement::SiteEngagementService::IsEnabled()) {
    site_engagement_helper_ =
        std::make_unique<site_engagement::SiteEngagementService::Helper>(
            tab.GetContents(),
            prerender::NoStatePrefetchManagerFactory::GetForBrowserContext(
                profile));
  }

  SafetyTipWebContentsObserver::CreateForWebContents(tab.GetContents());

  HttpsOnlyModeTabHelper::CreateForWebContents(tab.GetContents());

  login_detection::LoginDetectionTabHelper::MaybeCreateForWebContents(
      tab.GetContents());

  if (auto* pm_registry =
          performance_manager::PerformanceManagerRegistry::GetInstance()) {
    // A WebContents reparented into a new TabModel (e.g. via
    // web_app::ReparentWebContentsIntoBrowserImpl) already has its PageType set
    // to kTab, and PageNodeImpl::SetType CHECKs that SetPageType is only called
    // once per WebContents.
    base::WeakPtr<performance_manager::PageNode> page_node =
        performance_manager::PerformanceManager::
            GetPrimaryPageNodeForWebContents(tab.GetContents());
    if (page_node &&
        page_node->GetType() != performance_manager::PageType::kTab) {
      pm_registry->SetPageType(tab.GetContents(),
                               performance_manager::PageType::kTab);
    }
  }

  popup_blocker_tab_helper_ =
      GetUserDataFactory()
          .CreateInstance<blocked_content::PopupBlockerTabHelper>(
              tab, tab, tab.GetContents());

  resource_coordinator::ResourceCoordinatorTabHelper::CreateForWebContents(
      tab.GetContents());

  ukm::InitializeSourceUrlRecorderForWebContents(tab.GetContents());
}

TabUIHelper* TabFeatures::SetTabUIHelperForTesting(
    std::unique_ptr<TabUIHelper> tab_ui_helper) {
  tab_ui_helper_ = std::move(tab_ui_helper);
  return tab_ui_helper_.get();
}

void TabFeatures::WillDiscardContents(tabs::TabInterface* tab,
                                      content::WebContents* old_contents,
                                      content::WebContents* new_contents) {
  CHECK_EQ(old_contents, tab->GetContents(), base::NotFatalUntil::M162);

  Profile* profile = tab->GetBrowserWindowInterface()->GetProfile();

  CreateSubresourceFilterWebContentsHelper(new_contents);

  if (!content_settings::PageSpecificContentSettings::GetDelegateForWebContents(
          new_contents)) {
    content_settings::PageSpecificContentSettings::CreateForWebContents(
        new_contents,
        std::make_unique<PageSpecificContentSettingsDelegate>(new_contents));
  }

  favicon::CreateContentFaviconDriverForWebContents(new_contents);

  if (!profile->IsOffTheRecord()) {
    HistoryTabHelper::GetOrCreateForWebContents(new_contents);
  }

  InitializePageLoadMetricsForWebContents(new_contents);

  focus_tab_after_navigation_helper_ =
      std::make_unique<FocusTabAfterNavigationHelper>(new_contents);

  // The reset() must happen first so that the old instance deregisters
  // itself from the UnownedUserDataHost before the new instance registers
  // itself.
  framebust_block_tab_helper_.reset();
  framebust_block_tab_helper_ =
      GetUserDataFactory().CreateInstance<FramebustBlockTabHelper>(
          *tab, *tab, new_contents);

  // The reset() must happen first so that the old instance deregisters
  // itself from the UnownedUserDataHost before the new instance registers
  // itself.
  connection_help_tab_helper_.reset();
  connection_help_tab_helper_ =
      GetUserDataFactory().CreateInstance<ConnectionHelpTabHelper>(
          *tab, *tab, new_contents);

  // Recreated to reset its state: the swapped-in contents has not had any
  // form interactions. The reset() must happen first so that the old
  // instance deregisters itself from the UnownedUserDataHost before the new
  // instance registers itself.
  form_interaction_tab_helper_.reset();
  form_interaction_tab_helper_ =
      GetUserDataFactory().CreateInstance<FormInteractionTabHelper>(*tab, *tab);

  zero_suggest_prefetch_tab_helper_ =
      std::make_unique<ZeroSuggestPrefetchTabHelper>(new_contents);

  security_state_event_observer_ =
      std::make_unique<SecurityStateEventObserver>(new_contents);

  http_auth_cache_status_ = std::make_unique<HttpAuthCacheStatus>(new_contents);

  if (thumbnail_tab_helper_) {
    // The old helper stashed its thumbnail data on `new_contents` from
    // AboutToBeDiscarded(); the new helper picks it up in its constructor.
    // The reset() must happen first so that the old instance deregisters
    // itself from the UnownedUserDataHost before the new instance registers
    // itself.
    thumbnail_tab_helper_.reset();
    thumbnail_tab_helper_ =
        GetUserDataFactory().CreateInstance<ThumbnailTabHelper>(*tab, *tab,
                                                                new_contents);
  }

  if (sad_tab_helper_) {
    // The reset() must happen first so that the old instance deregisters
    // itself from the UnownedUserDataHost before the new instance registers
    // itself.
    sad_tab_helper_.reset();
    sad_tab_helper_ = GetUserDataFactory().CreateInstance<SadTabHelper>(
        *tab, *tab, new_contents);
  }

  from_gws_navigation_and_keep_alive_request_observer_ =
      FromGWSNavigationAndKeepAliveRequestObserver::MaybeCreate(new_contents);

  if (permission_indicators_tab_data_) {
    permission_indicators_tab_data_ =
        std::make_unique<permissions::PermissionIndicatorsTabData>(
            new_contents);
  }

  uma_browsing_activity_tab_helper_ =
      std::make_unique<UMABrowsingActivityTabHelper>(new_contents);

#if BUILDFLAG(IS_WIN)
  font_prewarmer_tab_helper_ =
      std::make_unique<FontPrewarmerTabHelper>(new_contents);
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  desktop_session_duration_observer_ =
      metrics::DesktopSessionDurationObserver::MaybeCreate(new_contents);
#endif

#if BUILDFLAG(ENABLE_RLZ)
  chrome_rlz_tracker_web_contents_observer_ =
      ChromeRLZTrackerWebContentsObserver::MaybeCreate(new_contents);
#endif

#if BUILDFLAG(ENABLE_PLUGINS)
  plugin_observer_.reset();
  plugin_observer_ = GetUserDataFactory().CreateInstance<PluginObserver>(
      *tab, *tab, new_contents);
#endif

#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
  captive_portal::CaptivePortalTabHelper::CreateForWebContents(
      new_contents, CaptivePortalServiceFactory::GetForProfile(profile),
      base::BindRepeating(
          &ChromeSecurityBlockingPageFactory::OpenLoginTabForWebContents,
          new_contents, false));
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
  app_tab_helper_.reset();
  app_tab_helper_ =
      std::make_unique<extensions::AppTabHelper>(*tab, new_contents);
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  if (extensions::GetViewType(new_contents) ==
      extensions::mojom::ViewType::kInvalid) {
    extensions::SetViewType(new_contents,
                            extensions::mojom::ViewType::kTabContents);
  }
  extensions::WebNavigationTabObserver::CreateForWebContents(new_contents);
  extensions::TabHelper::CreateForWebContents(new_contents);
  navigation_extension_enabler_ =
      std::make_unique<extensions::NavigationExtensionEnabler>(new_contents);
#endif

  storage_access_api_tab_helper_ = std::make_unique<StorageAccessAPITabHelper>(
      new_contents,
      StorageAccessAPIServiceFactory::GetForBrowserContext(profile));

  external_protocol_observer_ =
      std::make_unique<ExternalProtocolObserver>(new_contents);

  no_state_prefetch_tab_helper_ =
      std::make_unique<prerender::NoStatePrefetchTabHelper>(new_contents);

  navigation_predictor_preconnect_client_.reset();
  navigation_predictor_preconnect_client_ =
      std::make_unique<NavigationPredictorPreconnectClient>(new_contents);

  navigation_metrics_recorder_ =
      std::make_unique<NavigationMetricsRecorder>(new_contents);

  site_protection_metrics_observer_ =
      std::make_unique<site_protection::SiteProtectionMetricsObserver>(
          new_contents);

  permissions::PermissionRequestManager::CreateForWebContents(new_contents);

#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
  safe_browsing::SafeBrowsingNavigationObserver::MaybeCreateForWebContents(
      new_contents, HostContentSettingsMapFactory::GetForProfile(profile),
      safe_browsing::SafeBrowsingNavigationObserverManagerFactory::
          GetForBrowserContext(profile),
      profile->GetPrefs(), g_browser_process->safe_browsing_service(),
      /*is_referrer_chain_needed_for_enterprise=*/false);
  if (g_browser_process->safe_browsing_service()) {
    safe_browsing::AsyncCheckTracker::CreateForWebContents(
        new_contents, g_browser_process->safe_browsing_service()->ui_manager(),
        safe_browsing::AsyncCheckTracker::
            IsPlatformEligibleForSyncCheckerCheckAllowlist());
  }
  safe_browsing_tab_observer_.reset();
  safe_browsing_tab_observer_ =
      GetUserDataFactory()
          .CreateInstance<safe_browsing::SafeBrowsingTabObserver>(
              *tab, *tab, new_contents,
              std::make_unique<
                  safe_browsing::ChromeSafeBrowsingTabObserverDelegate>());
  trigger_creator_.reset();
  trigger_creator_ = std::make_unique<safe_browsing::TriggerCreator>(
      *tab, profile, new_contents);
#endif

  sound_content_setting_observer_.reset();
  sound_content_setting_observer_ =
      GetUserDataFactory().CreateInstance<SoundContentSettingObserver>(
          *tab, *tab, new_contents);

  download_navigation_observer_ =
      std::make_unique<download::DownloadNavigationObserver>(
          new_contents, download::NavigationMonitorFactory::GetForKey(
                            profile->GetProfileKey()));

  web_contents_top_sites_observer_ =
      std::make_unique<history::WebContentsTopSitesObserver>(
          new_contents, TopSitesFactory::GetForProfile(profile).get());

  client_hints_web_contents_observer_ =
      std::make_unique<client_hints::ClientHintsWebContentsObserver>(
          new_contents);

  chained_back_navigation_tracker_.reset();
  chained_back_navigation_tracker_ =
      GetUserDataFactory().CreateInstance<ChainedBackNavigationTracker>(
          *tab, *tab, new_contents);

  file_system_access_tab_helper_ =
      std::make_unique<FileSystemAccessTabHelper>(new_contents);

  search_engine_tab_helper_.reset();
  search_engine_tab_helper_ =
      GetUserDataFactory().CreateInstance<SearchEngineTabHelper>(*tab, *tab,
                                                                 new_contents);

  net_error_tab_helper_.reset();
  net_error_tab_helper_ =
      GetUserDataFactory()
          .CreateInstance<chrome_browser_net::NetErrorTabHelper>(*tab, *tab,
                                                                 new_contents);

  OmniboxTabHelper::CreateForWebContents(new_contents, profile);

  tab_dialogs_.reset();
  tab_dialogs_ = TabDialogs::Create(*tab, new_contents);

  if (auto* metrics_services_manager =
          g_browser_process->GetMetricsServicesManager()) {
    metrics_services_web_contents_observer_ =
        std::make_unique<metrics::MetricsServicesWebContentsObserver>(
            new_contents, metrics_services_manager->GetOnDidStartLoadingCb(),
            metrics_services_manager->GetOnDidStopLoadingCb(),
            metrics_services_manager->GetOnRendererUnresponsiveCb());
  }

  popup_opener_tab_helper_.reset();
  popup_opener_tab_helper_ =
      GetUserDataFactory()
          .CreateInstance<blocked_content::PopupOpenerTabHelper>(
              *tab, *tab, new_contents, base::DefaultTickClock::GetInstance(),
              HostContentSettingsMapFactory::GetForProfile(profile));

  core_tab_helper_.reset();
  core_tab_helper_ = GetUserDataFactory().CreateInstance<CoreTabHelper>(
      *tab, *tab, new_contents);

  recently_audible_helper_.reset();
  recently_audible_helper_ =
      GetUserDataFactory().CreateInstance<RecentlyAudibleHelper>(*tab, *tab,
                                                                 new_contents);

  FindBarState::ConfigureWebContents(new_contents);

  if (MediaEngagementService::IsEnabled()) {
    MediaEngagementService::CreateWebContentsObserver(new_contents);
  }

#if BUILDFLAG(ENABLE_PRINTING)
  printing::InitializePrintingForWebContents(new_contents);
#endif

  mixed_content_settings_tab_helper_.reset();
  mixed_content_settings_tab_helper_ =
      GetUserDataFactory().CreateInstance<MixedContentSettingsTabHelper>(
          *tab, *tab, new_contents);

  permissions::PermissionRecoverySuccessRateTracker::CreateForWebContents(
      new_contents);

  javascript_dialogs::TabModalDialogManager::CreateForWebContents(
      new_contents,
      std::make_unique<JavaScriptTabModalDialogManagerDelegateDesktop>(
          new_contents));

  web_modal::WebContentsModalDialogManager::CreateForWebContents(new_contents);

  one_time_permissions_tracker_helper_.reset();
  one_time_permissions_tracker_helper_ =
      std::make_unique<OneTimePermissionsTrackerHelper>(new_contents);

  if (loading_predictor_tab_helper_) {
    loading_predictor_tab_helper_.reset();
    loading_predictor_tab_helper_ =
        GetUserDataFactory()
            .CreateInstance<predictors::LoadingPredictorTabHelper>(
                *tab, *tab, new_contents);
  }

  PrefsTabHelper::CreateForWebContents(new_contents);

  site_engagement_helper_.reset();
  if (site_engagement::SiteEngagementService::IsEnabled()) {
    site_engagement_helper_ =
        std::make_unique<site_engagement::SiteEngagementService::Helper>(
            new_contents,
            prerender::NoStatePrefetchManagerFactory::GetForBrowserContext(
                profile));
  }

  SafetyTipWebContentsObserver::CreateForWebContents(new_contents);

  HttpsOnlyModeTabHelper::CreateForWebContents(new_contents);

  login_detection::LoginDetectionTabHelper::MaybeCreateForWebContents(
      new_contents);

  if (auto* pm_registry =
          performance_manager::PerformanceManagerRegistry::GetInstance()) {
    base::WeakPtr<performance_manager::PageNode> page_node =
        performance_manager::PerformanceManager::
            GetPrimaryPageNodeForWebContents(new_contents);
    if (page_node &&
        page_node->GetType() != performance_manager::PageType::kTab) {
      pm_registry->SetPageType(new_contents,
                               performance_manager::PageType::kTab);
    }
  }

  popup_blocker_tab_helper_.reset();
  popup_blocker_tab_helper_ =
      GetUserDataFactory()
          .CreateInstance<blocked_content::PopupBlockerTabHelper>(*tab, *tab,
                                                                  new_contents);

  resource_coordinator::ResourceCoordinatorTabHelper::CreateForWebContents(
      new_contents);

  ukm::InitializeSourceUrlRecorderForWebContents(new_contents);
}

TabAlertController* TabFeatures::SetTabAlertControllerForTesting(
    std::unique_ptr<TabAlertController> tab_alert_controller) {
  tab_alert_controller_ = std::move(tab_alert_controller);
  return tab_alert_controller_.get();
}

// static
ui::UserDataFactoryWithOwner<TabInterface>& TabFeatures::GetUserDataFactory() {
  static base::NoDestructor<ui::UserDataFactoryWithOwner<TabInterface>> factory;
  return *factory;
}

// static
ui::UserDataFactoryWithOwner<TabInterface>&
TabFeatures::GetUserDataFactoryForTesting() {
  return GetUserDataFactory();
}

}  // namespace tabs
