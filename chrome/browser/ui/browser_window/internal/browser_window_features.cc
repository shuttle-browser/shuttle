// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_window/public/browser_window_features.h"

#include <memory>

#include "base/check_is_test.h"
#include "base/containers/to_vector.h"
#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/devtools/devtools_ui_controller.h"
#include "chrome/browser/enterprise/data_protection/data_protection_ui_controller.h"
#include "chrome/browser/extensions/browser_extension_window_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_service_factory.h"
#include "chrome/browser/ui/animation/browser_animation_controller.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_active_state_manager/browser_active_state_manager.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_content_setting_bubble_model_delegate.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_init_state.h"
#include "chrome/browser/ui/browser_live_tab_context.h"
#include "chrome/browser/ui/browser_location_bar_model_delegate.h"
#include "chrome/browser/ui/browser_select_file_dialog_controller.h"
#include "chrome/browser/ui/browser_tab_menu_model_delegate.h"
#include "chrome/browser/ui/browser_ui_controller/browser_ui_controller.h"
#include "chrome/browser/ui/browser_web_contents_delegate/browser_web_contents_delegate.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/desktop_browser_window_capabilities.h"
#include "chrome/browser/ui/browser_window/public/embedder_browser_window_features.h"
#include "chrome/browser/ui/browser_window_theme_observer.h"
#include "chrome/browser/ui/call_to_action/call_to_action_lock.h"
#include "chrome/browser/ui/context_highlight/context_highlight_window_feature.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/extensions/extension_installed_watcher.h"
#include "chrome/browser/ui/find_bar/find_bar.h"
#include "chrome/browser/ui/find_bar/find_bar_controller.h"
#include "chrome/browser/ui/focus/browser_focus_controller.h"
#include "chrome/browser/ui/focus/browser_focus_controller_views.h"
#include "chrome/browser/ui/focus/browser_focus_controller_webui.h"
#include "chrome/browser/ui/fullscreen/browser_window_fullscreen_controller.h"
#include "chrome/browser/ui/immersive/immersive_mode_controller.h"
#include "chrome/browser/ui/sessions/session_service_browser_helper.h"
#include "chrome/browser/ui/tabs/organizer/organizer_panel_controller.h"
#include "chrome/browser/ui/tabs/split_tab_highlight_controller.h"
#include "chrome/browser/ui/tabs/tab_drag_api/desktop_tab_drag_impl/tab_drag_window_adapter_impl.h"
#include "chrome/browser/ui/tabs/tab_drag_api/tab_drag_service_feature.h"
#include "chrome/browser/ui/tabs/tab_list_bridge.h"
#include "chrome/browser/ui/tabs/tab_strip_api/controllers/tab_strip_ui_controller_impl.h"
#include "chrome/browser/ui/tabs/tab_strip_api/controllers/tab_strip_ui_controller_injector_impl.h"
#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_model_impl/tab_strip_model_injector.h"
#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_service_feature.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_prefs.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"
#include "chrome/browser/ui/toasts/toast_features.h"
#include "chrome/browser/ui/toasts/toast_service.h"
#include "chrome/browser/ui/toolbar/chrome_labs/chrome_labs_utils.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/unload_controller.h"
#include "chrome/browser/ui/views/animations/tab_strip_animations.h"
#include "chrome/browser/ui/views/color_provider_browser_helper.h"
#include "chrome/browser/ui/views/extensions/extension_keybinding_registry_views.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/contents_border_controller.h"
#include "chrome/browser/ui/views/frame/find_bar_owner_views.h"
#include "chrome/browser/ui/views/frame/horizontal_tab_strip_region_view.h"
#include "chrome/browser/ui/views/frame/multi_contents_view.h"
#include "chrome/browser/ui/views/frame/scrim_view_controller.h"
#include "chrome/browser/ui/views/fullscreen_control/fullscreen_control_host.h"
#include "chrome/browser/ui/views/interaction/browser_elements_views.h"
#include "chrome/browser/ui/views/interaction/browser_elements_views_impl.h"
#include "chrome/browser/ui/views/location_bar/cookie_controls/cookie_controls_bubble_coordinator.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/location_bar/zoom_bubble_coordinator.h"
#include "chrome/browser/ui/views/location_bar/zoom_bubble_manager_views.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_closer.h"
#include "chrome/browser/ui/views/tabs/organizer/organizer_panel_utils.h"
#include "chrome/browser/ui/views/toolbar/chrome_labs/chrome_labs_coordinator.h"
#include "chrome/browser/ui/views/toolbar/pinned_toolbar_actions.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/views/upgrade_notification_controller.h"
#include "chrome/browser/ui/web_modal/browser_window_modal_dialog_delegate.h"
#include "chrome/browser/ui/window_feature_controller/window_feature_controller.h"
#include "chrome/browser/ui/window_metadata/window_metadata_controller.h"
#include "chrome/browser/ui/zoom/browser_window_zoom_observer.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/content_settings/browser/ui/cookie_controls_controller.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/omnibox/browser/location_bar_model.h"
#include "components/omnibox/browser/location_bar_model_impl.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/prefs/pref_service.h"
#include "components/profile_metrics/browser_profile_type.h"
#include "components/search/ntp_features.h"
#include "components/search/search.h"
#include "components/sessions/core/session_id.h"
#include "content/public/common/content_constants.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension_features.h"
#include "ui/views/interaction/element_highlighter_views.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/extension_browser_window_helper.h"

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
#include "chrome/browser/ui/extensions/settings_overridden_params_providers.h"
#include "chrome/browser/ui/search_engines/default_search_extension_controlled_controller.h"
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
#include "chrome/browser/ui/pdf/infobar/pdf_infobar_controller.h"
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
#include "chrome/browser/ui/views/session_restore_infobar/session_restore_infobar_controller.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "chrome/browser/ui/views/frame/windows_taskbar_icon_updater.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ash/boca/on_task/on_task_locked_controller.h"
#include "chrome/browser/ui/chromeos/locked_state/locked_state_controller.h"
#endif

#if !BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/download/bubble/download_bubble_ui_controller.h"
#include "chrome/browser/download/bubble/download_display_controller.h"
#include "chrome/browser/ui/views/download/bubble/download_toolbar_ui_controller.h"
#endif

#if defined(USE_AURA)
#include "chrome/browser/ui/overscroll_pref_manager.h"
#endif  // defined(USE_AURA)

BrowserWindowFeatures::BrowserWindowFeatures() = default;
BrowserWindowFeatures::~BrowserWindowFeatures() = default;

void BrowserWindowFeatures::Init(BrowserWindowInterface* browser) {
  // This is used only for the controllers which will be created on demand
  // later.
  browser_ = browser;

  // Foundational state used throughout this function. Computed/assigned early
  // so that the alphabetical sections below can rely on them.
  Profile* const profile = browser->GetProfile();
  tab_strip_model_ = browser->GetTabStripModel();

  // One-time, process-wide registration. Idempotent across browser windows.
  ui::ElementHighlighter::GetElementHighlighter()
      ->MaybeRegisterBackend<views::ElementHighlighterViews>();

  // ---------------------------------------------------------------------------
  // Members owned by all browser window types (please keep this list ordered
  // without taking into consideration buildflags, repeating buildflags is ok;
  // dependency ordering takes precedence and exceptions are called out with
  // `// Must be after X.` comments):

  // UnloadController must be created first / destroyed last to ensure
  // features are able to register / de-register close callbacks.
  unload_controller_ = std::make_unique<UnloadController>(browser);


  browser_active_state_manager_ =
      GetUserDataFactory().CreateInstance<BrowserActiveStateManager>(
          *browser, *browser);


  // Foundational members hoisted out of alphabetical order:
  //   window_feature_controller_ depends on fullscreen_controller_.
  //   Later features depend on browser_command_controller_.
  //   Later features depend on browser_actions_.
  fullscreen_controller_ =
      std::make_unique<BrowserWindowFullscreenController>(*browser);
  window_feature_controller_ =
      GetUserDataFactory().CreateInstance<WindowFeatureController>(
          *browser, fullscreen_controller_.get(), browser->GetType(),
          BrowserInitState::From(browser)
              ->browser_window_create_params()
              .is_trusted_source,
          browser->GetUnownedUserDataHost());

  // IMPORTANT: BrowserActions must be initialized before
  // BrowserCommandController. When BrowserCommandController is created, it
  // initializes the default enabled states for UI commands
  // (InitCommandState). If BrowserActions is not built first, those initial
  // enabled updates will be completely dropped.
  browser_actions_ = std::make_unique<BrowserActions>(browser);
  browser_actions_->InitializeBrowserActions();

  browser_command_controller_ =
      std::make_unique<chrome::BrowserCommandController>(browser);

  browser_animation_controller_ =
      GetUserDataFactory().CreateInstance<BrowserAnimationController>(*browser,
                                                                      *browser);
  browser_animation_controller_->AddAnimationProvider(
      std::make_unique<TabStripAnimations>());
  browser_animation_controller_->AddAnimationProvider(
      std::make_unique<OrganizerPanelAnimations>());

  browser_elements_ =
      GetUserDataFactory().CreateInstance<BrowserElementsViewsImpl>(*browser,
                                                                    *browser);

  browser_window_modal_dialog_delegate_ =
      GetUserDataFactory().CreateInstance<BrowserWindowModalDialogDelegate>(
          *browser, browser);

  browser_window_theme_observer_ =
      GetUserDataFactory().CreateInstance<BrowserWindowThemeObserver>(*browser,
                                                                      browser);

  browser_window_zoom_observer_ =
      GetUserDataFactory().CreateInstance<BrowserWindowZoomObserver>(*browser,
                                                                     browser);

  call_to_action_lock_ =
      GetUserDataFactory().CreateInstance<CallToActionLock>(*browser, browser);

  content_setting_bubble_model_delegate_ =
      GetUserDataFactory()
          .CreateInstance<BrowserContentSettingBubbleModelDelegate>(*browser,
                                                                    browser);

  context_highlight_window_feature_ =
      std::make_unique<ContextHighlightWindowFeature>(*browser);

  cookie_controls_bubble_coordinator_ =
      GetUserDataFactory().CreateInstance<CookieControlsBubbleCoordinator>(
          *browser, browser, browser_actions_->root_action_item());

  cookie_controls_controller_ =
      std::make_unique<content_settings::CookieControlsController>(
          browser->GetUnownedUserDataHost(),
          CookieSettingsFactory::GetForProfile(profile),
          profile->IsOffTheRecord() ? CookieSettingsFactory::GetForProfile(
                                          profile->GetOriginalProfile())
                                    : nullptr,
          HostContentSettingsMapFactory::GetForProfile(profile),
          profile->IsIncognitoProfile());


#if BUILDFLAG(ENABLE_EXTENSIONS)
  extension_browser_window_helper_ =
      std::make_unique<extensions::ExtensionBrowserWindowHelper>(browser,
                                                                 profile);
#endif

  extension_installed_watcher_ =
      std::make_unique<ExtensionInstalledWatcher>(browser);

  // Must be after window_feature_controller_.
  immersive_mode_controller_ =
      GetUserDataFactory()
          .CreateInstanceWithFactoryMethod<ImmersiveModeController,
                                           WindowFeatureController*,
                                           ui::UnownedUserDataHost&>(
              *browser_, &chrome::CreateImmersiveModeController,
              window_feature_controller_.get(),
              browser->GetUnownedUserDataHost());


  // Must be before location_bar_model_.
  location_bar_model_delegate_ =
      std::make_unique<BrowserLocationBarModelDelegate>(tab_strip_model_);
  location_bar_model_ = std::make_unique<LocationBarModelImpl>(
      location_bar_model_delegate_.get(), content::kMaxURLDisplayChars);


#if BUILDFLAG(IS_CHROMEOS)
  locked_state_controller_ =
      GetUserDataFactory().CreateInstance<chromeos::LockedStateController>(
          *browser, browser);
  on_task_locked_controller_ =
      GetUserDataFactory().CreateInstance<ash::boca::OnTaskLockedController>(
          *browser, browser);
#endif  // BUILDFLAG(IS_CHROMEOS)

#if defined(USE_AURA)
  overscroll_pref_manager_ =
      GetUserDataFactory().CreateInstance<OverscrollPrefManager>(
          *browser, tab_strip_model_,
          browser->GetType() == BrowserWindowInterface::Type::TYPE_DEVTOOLS,
          browser->GetUnownedUserDataHost());
#endif  // defined(USE_AURA)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  if (base::FeatureList::IsEnabled(features::kPdfInfoBar)) {
    pdf_infobar_controller_ =
        GetUserDataFactory().CreateInstance<pdf::infobar::PdfInfoBarController>(
            *browser, browser);
  }
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  session_restore_infobar_controller_ =
      GetUserDataFactory()
          .CreateInstance<
              session_restore_infobar::SessionRestoreInfobarController>(
              *browser, browser);
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

  session_service_browser_helper_ =
      GetUserDataFactory().CreateInstance<SessionServiceBrowserHelper>(
          *browser, browser->GetTabStripModel(), browser->GetSessionID(),
          browser->GetType(), browser->GetProfile(),
          &BrowserInitState::From(browser)->browser_window_create_params(),
          browser->GetUnownedUserDataHost());

  // Must be after session_service_browser_helper_:
  //   tab_list_bridge_ depends on initialized session tab/window state.
  tab_list_bridge_ = std::make_unique<TabListBridge>(
      *tab_strip_model_, browser->GetUnownedUserDataHost());

  {
    auto adapter = std::make_unique<TabDragWindowAdapterImpl>(browser);
    tab_drag_service_feature_ =
        GetUserDataFactory().CreateInstance<TabDragServiceFeature>(
            *browser, std::move(adapter), browser->GetUnownedUserDataHost());
  }

  tab_menu_model_delegate_ =
      GetUserDataFactory().CreateInstance<chrome::BrowserTabMenuModelDelegate>(
          *browser, browser->GetSessionID(), profile,
          browser->GetUnownedUserDataHost());

  tab_strip_service_feature_ =
      GetUserDataFactory().CreateInstance<TabStripServiceFeature>(
          *browser,
          std::make_unique<tabs_api::tab_strip_model::TabStripModelInjector>(
              browser, tab_strip_model_),
          browser->GetUnownedUserDataHost());

  tab_strip_ui_controller_ =
      GetUserDataFactory().CreateInstance<tabs_api::TabStripUIControllerImpl>(
          *browser,
          std::make_unique<tabs_api::TabStripUIControllerInjectorImpl>(
              browser, tab_strip_model_),
          browser->GetUnownedUserDataHost());


  window_metadata_controller_ = std::make_unique<WindowMetadataController>(
      *browser, BrowserInitState::From(browser)->create_params().user_title);

  // ---------------------------------------------------------------------------
  // Members owned only when the browser is TYPE_NORMAL (e.g. a window with an
  // omnibox and a tab strip). By default most new features should be
  // instantiated in this block (please keep this list ordered without taking
  // into consideration buildflags, repeating buildflags is ok):
  if (browser->GetType() == BrowserWindowInterface::Type::TYPE_NORMAL) {

    if (organizer_panel::IsOrganizerPanelFeatureEnabled()) {
      organizer_panel_controller_ =
          GetUserDataFactory().CreateInstance<OrganizerPanelController>(
              *browser, *browser, browser_actions_->root_action_item());
    }

    std::optional<bool> restored_state_collapsed =
        BrowserInitState::From(browser)->is_vertical_tabs_initially_collapsed();
    std::optional<int> restored_state_uncollapsed_width =
        BrowserInitState::From(browser)
            ->get_vertical_tabs_initial_uncollapsed_width();

    if (!restored_state_collapsed.has_value() &&
        !restored_state_uncollapsed_width.has_value() &&
        !browser->CreatedBySessionRestore()) {
      restored_state_collapsed =
          profile->GetPrefs()->GetBoolean(prefs::kVerticalTabsCollapsedState);
      restored_state_uncollapsed_width =
          profile->GetPrefs()->GetInteger(prefs::kVerticalTabsUncollapsedWidth);
    }

    vertical_tab_strip_state_controller_ =
        GetUserDataFactory()
            .CreateInstance<tabs::VerticalTabStripStateController>(
                *browser, browser, profile->GetPrefs(),
                browser_actions_->root_action_item(),
                SessionServiceFactory::GetForProfile(browser_->GetProfile()),
                browser_->GetSessionID(), restored_state_collapsed,
                restored_state_uncollapsed_width);
  }


  // Initialize embedder features last.
  embedder_browser_window_features_ =
      GetUserDataFactory().CreateInstance<EmbedderBrowserWindowFeatures>(
          *browser, browser);
  embedder_browser_window_features_->Init(browser);
}

void BrowserWindowFeatures::InitPostWindowConstruction(
    BrowserWindowInterface* browser) {
  // Foundational state used throughout this function. Computed/assigned early
  // so the alphabetical sections below can rely on them.
  Profile* const profile = browser_->GetProfile();
  BrowserView* const browser_view =
      BrowserView::GetBrowserViewForBrowser(browser);
  views::FocusManager* focus_manager = nullptr;
  if (browser_view) {
    focus_manager = browser_view->GetFocusManager();
  }

  // ---------------------------------------------------------------------------
  // Members owned by all browser window types (please keep this list ordered
  // without taking into consideration buildflags, repeating buildflags is ok;
  // BrowserView and WebUIBrowserWindow implementations of the same role are
  // co-located via inline if/else dispatch; dependency exceptions are called
  // out with `// Must be after X.` / `// Must be before X.` comments):

  if (browser_view) {

  if (browser_view) {
    browser_focus_controller_ =
        GetUserDataFactory().CreateInstance<BrowserFocusControllerViews>(
            *browser, browser->GetWindow(), browser->GetUnownedUserDataHost(),
            profile, browser_elements_.get(),
            ToolbarButtonProvider::From(browser));
  } else {
    browser_focus_controller_ =
        GetUserDataFactory().CreateInstance<StubBrowserFocusController>(
            *browser, browser->GetWindow(), browser->GetUnownedUserDataHost());
  }

  browser_select_file_dialog_controller_ =
      GetUserDataFactory().CreateInstance<BrowserSelectFileDialogController>(
          *browser, browser->GetProfile(), browser->GetTabStripModel(),
          BrowserWindow::FromBrowser(browser), browser,
          browser->GetUnownedUserDataHost());

  browser_ui_controller_ =
      GetUserDataFactory().CreateInstance<BrowserUiController>(
          *browser, *browser, *tab_strip_model_,
          *BrowserWindow::FromBrowser(browser));

  if (browser_view) {
    color_provider_browser_helper_ =
        std::make_unique<ColorProviderBrowserHelper>(
            browser->GetTabStripModel(), browser_view->GetWidget(), browser);
  }

  if (browser_view) {
    contents_border_controller_ =
        std::make_unique<ContentsBorderController>(browser_view);
  }

  if (browser_view) {
    if (browser_->GetType() == BrowserWindowInterface::Type::TYPE_NORMAL ||
        browser_->GetType() == BrowserWindowInterface::Type::TYPE_POPUP) {
      data_protection_ui_controller_ =
          GetUserDataFactory()
              .CreateInstance<
                  enterprise_data_protection::DataProtectionUIController>(
                  *browser_view->browser(), browser_view);
    }
  }

#if BUILDFLAG(ENABLE_EXTENSIONS) && (BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC))
  if (browser_view) {
    if (!browser_->GetProfile()->IsOffTheRecord()) {
      base::UmaHistogramBoolean(
          "Extensions.SettingsOverridden."
          "UnacknowledgedMatchingDseExtensionPresent",
          settings_overridden_params::HasUnacknowledgedMatchingDseExtension(
              browser_->GetProfile()));
    }

    if (base::FeatureList::IsEnabled(
            extensions_features::kSearchEngineExplicitChoiceDialog)) {
      default_search_extension_controlled_controller_ =
          GetUserDataFactory()
              .CreateInstance<DefaultSearchExtensionControlledController>(
                  *browser_, *browser_, *browser_->GetProfile());
    }
  }
#endif

  desktop_browser_window_capabilities_ =
      GetUserDataFactory().CreateInstance<DesktopBrowserWindowCapabilities>(
          *browser, browser_window_modal_dialog_delegate_.get(),
          unload_controller_.get(), BrowserWindow::FromBrowser(browser),
          browser->GetUnownedUserDataHost());

  if (browser_view) {
    devtools_ui_controller_ =
        GetUserDataFactory().CreateInstance<DevtoolsUIController>(
            *browser, browser_, browser_view->GetContentsContainerViews());
  }

  exclusive_access_manager_ = std::make_unique<ExclusiveAccessManager>(
      browser, BrowserWindow::FromBrowser(browser)->GetExclusiveAccessContext(),
      browser_command_controller_.get());

  // Must be after exclusive_access_manager_,
  // desktop_browser_window_capabilities_, and browser_ui_controller_.
  browser_web_contents_delegate_ = std::make_unique<BrowserWebContentsDelegate>(
      browser, *exclusive_access_manager_, *browser_command_controller_,
      *unload_controller_, *BrowserWindow::FromBrowser(browser),
      *desktop_browser_window_capabilities_, *browser_ui_controller_);

  // Must be after exclusive_access_manager_.
#if !BUILDFLAG(IS_CHROMEOS)
  if (browser_view) {
    download_toolbar_ui_controller_ =
        GetUserDataFactory().CreateInstance<DownloadToolbarUIController>(
            *browser_view->browser(), browser_view);
    download_toolbar_ui_controller_->display_controller()
        ->ListenToFullScreenChanges();
  }
#endif

  // Focus manager can be null in tests.
  if (focus_manager) {
    extension_keybinding_registry_ =
        std::make_unique<ExtensionKeybindingRegistryViews>(
            browser, extensions::ExtensionKeybindingRegistry::ALL_EXTENSIONS,
            focus_manager);
  }

  extension_window_controller_ =
      std::make_unique<extensions::BrowserExtensionWindowController>(browser);

  if (browser_view) {
    find_bar_owner_ = GetUserDataFactory().CreateInstance<FindBarOwnerViews>(
        *browser, browser_view, browser->GetUnownedUserDataHost());
  }

  // Must be after exclusive_access_manager_.
  if (browser_view) {
    fullscreen_control_host_ =
        GetUserDataFactory().CreateInstance<FullscreenControlHost>(
            *browser, browser_view, exclusive_access_manager_.get(),
            browser->GetUnownedUserDataHost());
  }

  live_tab_context_ =
      GetUserDataFactory().CreateInstance<BrowserLiveTabContext>(
          *browser, browser, browser->GetTabStripModel(), profile,
          browser->GetWindow(), browser->GetType(),
          BrowserInitState::From(browser)->create_params().app_name,
          browser->GetSessionID());

  if (browser_view) {
    omnibox_popup_closer_ =
        GetUserDataFactory().CreateInstance<omnibox::OmniboxPopupCloser>(
            *browser, browser_view, browser->GetUnownedUserDataHost());
  }

  if (browser_view) {
    scrim_view_controller_ =
        std::make_unique<ScrimViewController>(browser_view);
  }

  if (browser->GetType() == BrowserWindowInterface::Type::TYPE_NORMAL ||
      browser->GetType() == BrowserWindowInterface::Type::TYPE_APP) {
    toast_service_ =
        GetUserDataFactory().CreateInstance<ToastService>(*browser, browser);
  }

  upgrade_notification_controller_ =
      std::make_unique<UpgradeNotificationController>(browser);


#if BUILDFLAG(IS_WIN)
  if (browser_view) {
    windows_taskbar_icon_updater_ =
        std::make_unique<WindowsTaskbarIconUpdater>(*browser_view);
  }
#endif

  if (browser_view) {
    zoom_bubble_manager_ =
        std::make_unique<ZoomBubbleManagerViews>(browser_view);
    // Must be after zoom_bubble_manager_.
    zoom_bubble_coordinator_ =
        GetUserDataFactory().CreateInstance<ZoomBubbleCoordinator>(
            *browser_, *browser_, zoom_bubble_manager_.get());
  }

  // ---------------------------------------------------------------------------
  // Members owned only when the browser is TYPE_NORMAL (e.g. a window with an
  // omnibox and a tab strip). By default most new features should be
  // instantiated in this block (please keep this list ordered without taking
  // into consideration buildflags, repeating buildflags is ok):
  if (browser->GetType() == BrowserWindowInterface::Type::TYPE_NORMAL) {

    if (IsChromeLabsEnabled()) {
      chrome_labs_coordinator_ =
          GetUserDataFactory().CreateInstance<ChromeLabsCoordinator>(*browser,
                                                                     browser);
    }


    if (browser_view) {
      // Get the PinnedToolbarActions for the browser; it might not exist for
      // browsers with a custom tab toolbar.
      pinned_toolbar_actions_ =
          browser_view->toolbar_button_provider()->GetPinnedToolbarActions();
    }


    if (browser_view) {
      split_tab_highlight_controller_ =
          GetUserDataFactory()
              .CreateInstance<split_tabs::SplitTabHighlightController>(
                  *browser, browser_view->browser(),
                  browser_view->multi_contents_view());
    }


  }

  // Initialize post-window dependent embedder features last.
  embedder_browser_window_features_->InitPostWindowConstruction(browser);
}

void BrowserWindowFeatures::InitPostBrowserViewConstruction(
    BrowserView* browser_view) {
  // WARNING: DO NOT ADD ANY MORE CODE HERE. Please perform setup in
  // InitPostWindowConstruction() instead.

  // Initialize post-BrowserView-dependent embedder features last.
  embedder_browser_window_features_->InitPostBrowserViewConstruction(
      browser_view);
}

void BrowserWindowFeatures::TearDownPreBrowserWindowDestruction() {
  // Tear down in reverse order of construction: embedder first, then
  // InitPostWindowConstruction, then Init. Within each section, members are
  // torn down in reverse of the corresponding Init* ordering. Dependency
  // exceptions are called out with `// Must be before X.` /
  // `// Must be after X.` comments.

  // Tear down embedder features first, in reverse order of initialization.
  embedder_browser_window_features_->TearDownPreBrowserWindowDestruction();

  // ---------------------------------------------------------------------------
  // InitPostWindowConstruction (reverse).

  // TYPE_NORMAL members.
  split_tab_highlight_controller_.reset();
  pinned_toolbar_actions_ = nullptr;
  if (chrome_labs_coordinator_) {
    chrome_labs_coordinator_->TearDown();
  }

  // Owned-by-all members.
  zoom_bubble_coordinator_.reset();
  zoom_bubble_manager_.reset();
#if BUILDFLAG(IS_WIN)
  windows_taskbar_icon_updater_.reset();
#endif
  upgrade_notification_controller_.reset();
  toast_service_.reset();
  scrim_view_controller_.reset();
  omnibox_popup_closer_.reset();
  live_tab_context_.reset();
  // Must be before exclusive_access_manager_ (destroy fullscreen control host
  // before exclusive access manager).
  fullscreen_control_host_.reset();
  find_bar_owner_.reset();
  extension_window_controller_.reset();
  extension_keybinding_registry_.reset();
  // Must be before exclusive_access_manager_.
#if !BUILDFLAG(IS_CHROMEOS)
  if (download_toolbar_ui_controller_) {
    download_toolbar_ui_controller_->TearDownPreBrowserWindowDestruction();
  }
#endif
  browser_web_contents_delegate_.reset();
  exclusive_access_manager_.reset();
  if (devtools_ui_controller_) {
    devtools_ui_controller_->TearDown();
  }
  desktop_browser_window_capabilities_.reset();
#if BUILDFLAG(ENABLE_EXTENSIONS) && (BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC))
  default_search_extension_controlled_controller_.reset();
#endif
  data_protection_ui_controller_.reset();
  contents_border_controller_.reset();
  color_provider_browser_helper_.reset();
  browser_ui_controller_.reset();
  browser_select_file_dialog_controller_.reset();
  browser_focus_controller_.reset();

  // ---------------------------------------------------------------------------
  // Init (reverse).

  // TYPE_NORMAL members.

  // Owned-by-all members.
  // Must be before window_feature_controller_ (raw pointer).
  immersive_mode_controller_.reset();
  // TODO(crbug.com/423956131): Update reset order once FindBarController is
  // deterministically constructed.
  find_bar_controller_.reset();
  extension_installed_watcher_.reset();
  context_highlight_window_feature_.reset();
  if (auto* const provider = browser_elements_->AsA<BrowserElementsViews>()) {
    provider->TearDown();
  }
  browser_animation_controller_.reset();
}

LocationBar* BrowserWindowFeatures::location_bar() {
  // Return nullptr if not initialized. This can happen in tests where
  // BrowserWindowFeatures is stubbed without being initialized with a browser.
  if (!browser_) {
    return nullptr;
  }
  return BrowserWindow::FromBrowser(browser_)->GetLocationBar();
}

const LocationBar* BrowserWindowFeatures::location_bar() const {
  // Return nullptr if not initialized. This can happen in tests where
  // BrowserWindowFeatures is stubbed without being initialized with a browser.
  if (!browser_) {
    return nullptr;
  }
  return BrowserWindow::FromBrowser(browser_)->GetLocationBar();
}

FindBarController* BrowserWindowFeatures::GetFindBarController() {
  if (!find_bar_controller_.get()) {
    CHECK(browser_);
    find_bar_controller_ = std::make_unique<FindBarController>(
        BrowserWindow::FromBrowser(browser_)->CreateFindBar(),
        browser_command_controller_.get());
    find_bar_controller_->find_bar()->SetFindBarController(
        find_bar_controller_.get());
    find_bar_controller_->ChangeWebContents(
        tab_strip_model_->GetActiveWebContents());
    find_bar_controller_->find_bar()->MoveWindowIfNecessary();
  }
  return find_bar_controller_.get();
}

bool BrowserWindowFeatures::HasFindBarController() const {
  return find_bar_controller_.get() != nullptr;
}

// static
ui::UserDataFactoryWithOwner<BrowserWindowInterface>&
BrowserWindowFeatures::GetUserDataFactoryForTesting() {
  return GetUserDataFactory();
}

// static
ui::UserDataFactoryWithOwner<BrowserWindowInterface>&
BrowserWindowFeatures::GetUserDataFactory() {
  static base::NoDestructor<
      ui::UserDataFactoryWithOwner<BrowserWindowInterface>>
      factory;
  return *factory;
}
