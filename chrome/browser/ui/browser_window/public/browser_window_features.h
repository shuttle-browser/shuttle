// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This class is used to construct and hold window-scoped state.
//
// This class exists for 3 reasons:
//  (1) It provides explicit construction and destruction ordering.
//  (2) It allows for dependency-injection at construction time of features.
//  (3) It pairs with the UnownedUserData design pattern to ensure dependencies
//      are precisely specified by BUILD.gn files. This prevents circular
//      dependencies.
//
// If you want to make a new BrowserWindowFeature, following these steps:
//  (1) Make a regular C++ class. It should NOT inherit from SupportsUserData.
//  (2) Forward declare the class, and add a std::unique_ptr member to this
//      header file.
//  (3) Construct the member in browser_window_features.cc.
//  (4) If consumers need to access the feature, expose it via
//      BrowserWindowInterface and UnownedUserData.
//
// For more details on UnownedUserData, see ui/base/unowned_user_data/README.md.

#ifndef CHROME_BROWSER_UI_BROWSER_WINDOW_PUBLIC_BROWSER_WINDOW_FEATURES_H_
#define CHROME_BROWSER_UI_BROWSER_WINDOW_PUBLIC_BROWSER_WINDOW_FEATURES_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/common/buildflags.h"
#include "extensions/buildflags/buildflags.h"
#include "ui/base/unowned_user_data/user_data_factory.h"


class BrowserActions;
class BrowserActiveStateManager;
class BrowserAnimationController;
class BrowserContentSettingBubbleModelDelegate;
class BrowserElements;
class BrowserFocusController;
class BrowserLiveTabContext;
namespace sessions {
class LiveTabContext;
}
class BrowserLocationBarModelDelegate;
class BrowserSelectFileDialogController;
class BrowserUiController;
class BrowserView;
class BrowserWebContentsDelegate;
class BrowserWindowFullscreenController;
class BrowserWindowInterface;
class BrowserWindowModalDialogDelegate;
class BrowserWindowThemeObserver;
class BrowserWindowZoomObserver;
class CallToActionLock;
class ChromeLabsCoordinator;
class ColorProviderBrowserHelper;
class ContentsBorderController;
class ContextHighlightWindowFeature;

class CookieControlsBubbleCoordinator;
class DesktopBrowserWindowCapabilities;
class DevtoolsUIController;
class EmbedderBrowserWindowFeatures;
class ExclusiveAccessManager;
class ExtensionInstalledWatcher;
class ExtensionKeybindingRegistryViews;
class FindBarController;
class FindBarOwner;
class FullscreenControlHost;
class ImmersiveModeController;
class LocationBar;
class LocationBarModel;
class PinnedToolbarActions;
class OrganizerPanelController;
class ScrimViewController;
class SessionServiceBrowserHelper;
class TabDragServiceFeature;
class TabListBridge;

class TabMenuModelDelegate;
class TabStripModel;
class TabStripServiceFeature;
class ToastService;
class UnloadController;
class UpgradeNotificationController;
class WindowFeatureController;
class WindowMetadataController;
class ZoomBubbleCoordinator;
class ZoomBubbleManager;

#if BUILDFLAG(IS_WIN)
class WindowsTaskbarIconUpdater;
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

#if !BUILDFLAG(IS_CHROMEOS)
class DownloadToolbarUIController;
#endif

#if defined(USE_AURA)
class OverscrollPrefManager;
#endif  // defined(USE_AURA)

#if BUILDFLAG(ENABLE_EXTENSIONS) && (BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC))
class DefaultSearchExtensionControlledController;
#endif

#if BUILDFLAG(IS_CHROMEOS)
namespace ash::boca {
class OnTaskLockedController;
}  // namespace ash::boca

namespace chromeos {
class LockedStateController;
}  // namespace chromeos
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace chrome {
class BrowserCommandController;
}  // namespace chrome

namespace content_settings {
class CookieControlsController;
}  // namespace content_settings

namespace enterprise_data_protection {
class DataProtectionUIController;
}  // namespace enterprise_data_protection

namespace extensions {
class BrowserExtensionWindowController;
#if BUILDFLAG(ENABLE_EXTENSIONS)
class ExtensionBrowserWindowHelper;
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
}  // namespace extensions

namespace omnibox {
class OmniboxPopupCloser;
}  // namespace omnibox

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
namespace pdf::infobar {
class PdfInfoBarController;
}  // namespace pdf::infobar
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
namespace session_restore_infobar {
class SessionRestoreInfobarController;
}  // namespace session_restore_infobar
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

namespace split_tabs {
class SplitTabHighlightController;
}  // namespace split_tabs

namespace tabs {
class VerticalTabStripStateController;
}  // namespace tabs

namespace tabs_api {
class TabStripUIControllerImpl;
}  // namespace tabs_api


// This class owns the core controllers for features that are scoped to a given
// browser window on desktop.
//
// To inject alternative versions of features or mocks for testing, make your
// feature compatible with `UnownedUserDataHost` and then use
// `GetUserDataFactoryForTesting()` to inject your test-specific feature
// object(s).
//
// Do not add more public accessors. Instead use the UnownedUserData design
// pattern, see ui/base/unowned_user_data/README.md.
// TODO(crbug.com/481268779a): Remove existing public accessors.
class BrowserWindowFeatures {
 public:
  BrowserWindowFeatures();
  ~BrowserWindowFeatures();

  BrowserWindowFeatures(const BrowserWindowFeatures&) = delete;
  BrowserWindowFeatures& operator=(const BrowserWindowFeatures&) = delete;

  // Called exactly once to initialize features. This is called prior to
  // instantiating BrowserView, to allow the view hierarchy to depend on state
  // in this class.
  void Init(BrowserWindowInterface* browser);

  // Called exactly once to initialize features that depend on the window object
  // being created.
  void InitPostWindowConstruction(BrowserWindowInterface* browser);

  // Called exactly once to initialize features that depend on the view
  // hierarchy in BrowserView.
  void InitPostBrowserViewConstruction(BrowserView* browser_view);

  // Called exactly once to tear down state that depends on the window object.
  void TearDownPreBrowserWindowDestruction();

  // Get the FindBarController for this browser window, creating it if it does
  // not yet exist.
  FindBarController* GetFindBarController();

  // Returns true if a FindBarController exists for this browser window.
  bool HasFindBarController() const;

  // Returns the LocationBar for this browser window. Currently delegates to
  // BrowserWindow::GetLocationBar() via downcast, but should eventually become
  // an owned member of BrowserWindowFeatures.
  LocationBar* location_bar();
  const LocationBar* location_bar() const;

  LocationBarModel* location_bar_model() { return location_bar_model_.get(); }
  const LocationBarModel* location_bar_model() const {
    return location_bar_model_.get();
  }
#if defined(UNIT_TEST)
  void swap_location_bar_models(
      std::unique_ptr<LocationBarModel>* location_bar_model) {
    location_bar_model->swap(location_bar_model_);
  }
#endif

  PinnedToolbarActions* pinned_toolbar_actions() {
    return pinned_toolbar_actions_;
  }

  static ui::UserDataFactoryWithOwner<BrowserWindowInterface>&
  GetUserDataFactoryForTesting();

 private:
  static ui::UserDataFactoryWithOwner<BrowserWindowInterface>&
  GetUserDataFactory();

  // Members owned by all browser window types.
  std::unique_ptr<UnloadController> unload_controller_;

  std::unique_ptr<BrowserActions> browser_actions_;
  std::unique_ptr<BrowserActiveStateManager> browser_active_state_manager_;
  std::unique_ptr<BrowserAnimationController> browser_animation_controller_;
  std::unique_ptr<chrome::BrowserCommandController> browser_command_controller_;
  std::unique_ptr<BrowserElements> browser_elements_;
  std::unique_ptr<BrowserFocusController> browser_focus_controller_;
  std::unique_ptr<BrowserSelectFileDialogController>
      browser_select_file_dialog_controller_;
  std::unique_ptr<BrowserUiController> browser_ui_controller_;
  std::unique_ptr<BrowserWebContentsDelegate> browser_web_contents_delegate_;
  std::unique_ptr<BrowserWindowModalDialogDelegate>
      browser_window_modal_dialog_delegate_;
  std::unique_ptr<BrowserWindowThemeObserver> browser_window_theme_observer_;
  std::unique_ptr<BrowserWindowZoomObserver> browser_window_zoom_observer_;
  std::unique_ptr<CallToActionLock> call_to_action_lock_;
  std::unique_ptr<ChromeLabsCoordinator> chrome_labs_coordinator_;

  // Helper which implements the ContentSettingBubbleModel interface.
  std::unique_ptr<BrowserContentSettingBubbleModelDelegate>
      content_setting_bubble_model_delegate_;

  std::unique_ptr<ContextHighlightWindowFeature>
      context_highlight_window_feature_;

  // Member order dependencies:
  //   extension_window_controller_ depends on tab_list_bridge_.
  std::unique_ptr<TabListBridge> tab_list_bridge_;

  std::unique_ptr<CookieControlsBubbleCoordinator>
      cookie_controls_bubble_coordinator_;
  std::unique_ptr<content_settings::CookieControlsController>
      cookie_controls_controller_;

  // A collection of features specific to desktop versions of Chrome.
  // Member order dependencies:
  //   DesktopBrowserWindowCapabilities depends on modal dialog delegate.
  //   DesktopBrowserWindowCapabilities depends on unload_controller_.
  std::unique_ptr<DesktopBrowserWindowCapabilities>
      desktop_browser_window_capabilities_;

  std::unique_ptr<ExclusiveAccessManager> exclusive_access_manager_;
  std::unique_ptr<ExtensionInstalledWatcher> extension_installed_watcher_;

  // The class that registers for keyboard shortcuts for extension commands,
  // and its delegate.
  std::unique_ptr<ExtensionKeybindingRegistryViews>
      extension_keybinding_registry_;

  // Member order dependency:
  //   extension_window_controller_ depends on tab_list_bridge_.
  std::unique_ptr<extensions::BrowserExtensionWindowController>
      extension_window_controller_;

  // The Find Bar. This may be NULL if there is no Find Bar, and if it is
  // non-NULL, it may or may not be visible.
  std::unique_ptr<FindBarController> find_bar_controller_;

  std::unique_ptr<FindBarOwner> find_bar_owner_;
  std::unique_ptr<BrowserWindowFullscreenController> fullscreen_controller_;

  // Helper which implements the LiveTabContext interface.
  std::unique_ptr<BrowserLiveTabContext> live_tab_context_;

  // Helper which implements the LocationBarModelDelegate interface.
  std::unique_ptr<BrowserLocationBarModelDelegate> location_bar_model_delegate_;

  // The model for the toolbar view.
  std::unique_ptr<LocationBarModel> location_bar_model_;

  std::unique_ptr<OrganizerPanelController> organizer_panel_controller_;
  std::unique_ptr<SessionServiceBrowserHelper> session_service_browser_helper_;
  std::unique_ptr<TabDragServiceFeature> tab_drag_service_feature_;
  std::unique_ptr<TabMenuModelDelegate> tab_menu_model_delegate_;

  // This is an experimental API that interacts with the TabStripModel.
  std::unique_ptr<TabStripServiceFeature> tab_strip_service_feature_;

  // Controller for managing TabStrip UI decoupled TabStrip platform.
  std::unique_ptr<tabs_api::TabStripUIControllerImpl> tab_strip_ui_controller_;

  std::unique_ptr<ToastService> toast_service_;
  std::unique_ptr<UpgradeNotificationController>
      upgrade_notification_controller_;
  std::unique_ptr<tabs::VerticalTabStripStateController>
      vertical_tab_strip_state_controller_;

  // Must come after fullscreen_controller_.
  std::unique_ptr<WindowFeatureController> window_feature_controller_;
  std::unique_ptr<ImmersiveModeController> immersive_mode_controller_;

  std::unique_ptr<WindowMetadataController> window_metadata_controller_;

  // Must come before zoom_bubble_coordinator_.
  std::unique_ptr<ZoomBubbleManager> zoom_bubble_manager_;
  std::unique_ptr<ZoomBubbleCoordinator> zoom_bubble_coordinator_;

  // Members owned only when a BrowserView is attached.
  std::unique_ptr<ColorProviderBrowserHelper> color_provider_browser_helper_;
  std::unique_ptr<ContentsBorderController> contents_border_controller_;
  std::unique_ptr<enterprise_data_protection::DataProtectionUIController>
      data_protection_ui_controller_;
  std::unique_ptr<DevtoolsUIController> devtools_ui_controller_;

  // The window-scoped extension side-panel manager. There is a separate
  // tab-scoped extension side-panel manager.

  std::unique_ptr<FullscreenControlHost> fullscreen_control_host_;
  std::unique_ptr<omnibox::OmniboxPopupCloser> omnibox_popup_closer_;
  std::unique_ptr<ScrimViewController> scrim_view_controller_;
  std::unique_ptr<split_tabs::SplitTabHighlightController>
      split_tab_highlight_controller_;

  // Platform-specific members.
#if BUILDFLAG(IS_WIN)
  std::unique_ptr<WindowsTaskbarIconUpdater> windows_taskbar_icon_updater_;
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  std::unique_ptr<pdf::infobar::PdfInfoBarController> pdf_infobar_controller_;
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  std::unique_ptr<session_restore_infobar::SessionRestoreInfobarController>
      session_restore_infobar_controller_;
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_CHROMEOS)
  std::unique_ptr<chromeos::LockedStateController> locked_state_controller_;
  std::unique_ptr<ash::boca::OnTaskLockedController> on_task_locked_controller_;
#endif  // BUILDFLAG(IS_CHROMEOS)

#if !BUILDFLAG(IS_CHROMEOS)
  std::unique_ptr<DownloadToolbarUIController> download_toolbar_ui_controller_;
#endif

#if defined(USE_AURA)
  std::unique_ptr<OverscrollPrefManager> overscroll_pref_manager_;
#endif  // defined(USE_AURA)

#if BUILDFLAG(ENABLE_EXTENSIONS)
  std::unique_ptr<extensions::ExtensionBrowserWindowHelper>
      extension_browser_window_helper_;

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  std::unique_ptr<DefaultSearchExtensionControlledController>
      default_search_extension_controlled_controller_;
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

  // Non-owning references.
  // TODO(crbug.com/423956131): Remove this.
  raw_ptr<BrowserWindowInterface> browser_ = nullptr;

  raw_ptr<PinnedToolbarActions> pinned_toolbar_actions_ = nullptr;
  raw_ptr<TabStripModel> tab_strip_model_;

  // Embedder features. Must be declared last.
  // Keep this member last to ensure embedder features are torn down first, in
  // reverse order of initialization.
  std::unique_ptr<EmbedderBrowserWindowFeatures>
      embedder_browser_window_features_;
};

#endif  // CHROME_BROWSER_UI_BROWSER_WINDOW_PUBLIC_BROWSER_WINDOW_FEATURES_H_
