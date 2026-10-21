// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/public/tab_features.h"

#include <memory>

#include "base/feature_list.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/content_settings/sound_content_setting_observer.h"
#include "chrome/browser/enterprise/data_protection/data_protection_navigation_controller.h"
#include "chrome/browser/enterprise/reporting/saas_usage/saas_usage_navigation_observer.h"
#include "chrome/browser/external_protocol/external_protocol_observer.h"
#include "chrome/browser/loader/from_gws_navigation_and_keep_alive_request_observer.h"
#include "chrome/browser/multistep_filter/chrome_filter_navigation_observer.h"
#include "chrome/browser/multistep_filter/ui/filter_ui_controller.h"
#include "chrome/browser/navigation_predictor/navigation_predictor_preconnect_client.h"
#include "chrome/browser/net/http_auth_cache_status.h"
#include "chrome/browser/net/qwac_web_contents_observer.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
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
#include "chrome/browser/ui/tabs/page_context_eligibility_helper.h"
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
#include "components/enterprise/browser/reporting/reporting_features.h"
#include "components/multistep_filter/core/features.h"
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
#include "chrome/browser/metrics/desktop_session_duration/desktop_session_duration_observer.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "chrome/browser/metrics/oom/commit_limit_oom_recovery_tracker.h"
#endif
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/tabs/tab_attachment_tracker.h"
#include "chrome/browser/v8_compile_hints/v8_compile_hints_tab_helper.h"
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

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
#include "chrome/browser/extensions/navigation_extension_enabler.h"
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

  if (page_action_controller_->ActionExists(kActionShowFileSystemAccess)) {
    file_system_access_page_action_controller_ =
        std::make_unique<FileSystemAccessPageActionController>(tab);
  }

  if (page_action_controller_->ActionExists(kActionShowZoomBubble)) {
    zoom_view_controller_ = std::make_unique<zoom::ZoomViewController>(
        tab, *page_action_controller_);
  }

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

  page_context_eligibility_helper_ =
      GetUserDataFactory().CreateInstance<tabs::PageContextEligibilityHelper>(
          tab, tab);

  // Features that are only enabled for normal browser windows. By default most
  // features should be instantiated in this block.
  if (tab.IsInNormalWindow()) {
    permission_indicators_tab_data_ =
        std::make_unique<permissions::PermissionIndicatorsTabData>(
            tab.GetContents());

  }  // IsInNormalWindow() end.

  tab_dialog_manager_ = std::make_unique<TabDialogManager>(&tab);

  data_protection_tab_controller_ = std::make_unique<
      enterprise_data_protection::DataProtectionNavigationController>(&tab);

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

  tab_alert_controller_ =
      GetUserDataFactory().CreateInstance<TabAlertController>(tab, tab);


  // The controller is created for all tabs but only affects back button
  // behavior for destination tabs with opener relationships.
  if (base::FeatureList::IsEnabled(tabs::kBackToOpener)) {
    back_to_opener_controller_ =
        std::make_unique<back_to_opener::BackToOpenerController>(tab);
  }

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || \
    BUILDFLAG(IS_CHROMEOS)
  if (base::FeatureList::IsEnabled(enterprise_reporting::kSaasUsageReporting)) {
    saas_usage_navigation_observer_ =
        std::make_unique<enterprise_reporting::SaasUsageNavigationObserver>(
            tab.GetContents());
  }
#endif

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
  if (base::FeatureList::IsEnabled(multistep_filter::kMultistepFilter)) {
    filter_ui_controller_ =
        GetUserDataFactory()
            .CreateInstance<multistep_filter::FilterUiController>(tab, tab);
    filter_navigation_observer_ =
        GetUserDataFactory()
            .CreateInstance<multistep_filter::ChromeFilterNavigationObserver>(
                tab, tab);
  }
#endif


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

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  navigation_extension_enabler_ =
      std::make_unique<extensions::NavigationExtensionEnabler>(
          tab.GetContents());
#endif

  v8_compile_hints_tab_helper_ =
      v8_compile_hints::V8CompileHintsTabHelper::MaybeCreate(tab.GetContents());

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

  sound_content_setting_observer_ =
      GetUserDataFactory().CreateInstance<SoundContentSettingObserver>(
          tab, tab, tab.GetContents());

  download_navigation_observer_ =
      std::make_unique<download::DownloadNavigationObserver>(
          tab.GetContents(), download::NavigationMonitorFactory::GetForKey(
                                 profile->GetProfileKey()));
}

TabUIHelper* TabFeatures::SetTabUIHelperForTesting(
    std::unique_ptr<TabUIHelper> tab_ui_helper) {
  tab_ui_helper_ = std::move(tab_ui_helper);
  return tab_ui_helper_.get();
}

void TabFeatures::WillDiscardContents(tabs::TabInterface* tab,
                                      content::WebContents* old_contents,
                                      content::WebContents* new_contents) {
  DCHECK_EQ(old_contents, tab->GetContents());

  Profile* profile = tab->GetBrowserWindowInterface()->GetProfile();

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

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  navigation_extension_enabler_ =
      std::make_unique<extensions::NavigationExtensionEnabler>(new_contents);
#endif

  v8_compile_hints_tab_helper_ =
      v8_compile_hints::V8CompileHintsTabHelper::MaybeCreate(new_contents);

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

  sound_content_setting_observer_.reset();
  sound_content_setting_observer_ =
      GetUserDataFactory().CreateInstance<SoundContentSettingObserver>(
          *tab, *tab, new_contents);

  download_navigation_observer_ =
      std::make_unique<download::DownloadNavigationObserver>(
          new_contents, download::NavigationMonitorFactory::GetForKey(
                            profile->GetProfileKey()));
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
