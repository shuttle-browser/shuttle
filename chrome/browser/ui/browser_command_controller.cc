// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_command_controller.h"

#include <stddef.h>

#include <algorithm>
#include <string>

#include "base/check_deref.h"
#include "base/command_line.h"
#include "base/debug/debugging_buildflags.h"
#include "base/debug/profiler.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/i18n/rtl.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/notimplemented.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/actor/ui/actor_overlay_web_view.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browsing_data/browsing_data_important_sites_util.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/devtools/features.h"
#include "chrome/browser/enterprise/isolated_mode/isolated_mode_settings_service_factory.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/actions/chrome_action_properties.h"
#include "chrome/browser/ui/actions/command_action_updater.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_select_file_dialog_controller.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/bubble_anchor_util.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/dialogs/browser_dialogs.h"
#include "chrome/browser/ui/fullscreen/browser_window_fullscreen_controller.h"
#include "chrome/browser/ui/managed_ui.h"
#include "chrome/browser/ui/navigator/browser_navigator.h"
#include "chrome/browser/ui/page_info/page_info_dialog.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/tabs/split_tab_metrics.h"
#include "chrome/browser/ui/tabs/tab_change_type.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_user_gesture_details.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"
#include "chrome/browser/ui/toolbar/chrome_labs/chrome_labs_utils.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/webui/inspect/inspect_ui.h"
#include "chrome/browser/ui/webui/util/webui_util_desktop.h"
#include "chrome/browser/ui/window_feature_controller/window_feature_controller.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/content_restriction.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "components/dom_distiller/core/dom_distiller_features.h"
#include "components/input/native_web_keyboard_event.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/sessions/core/tab_restore_service.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/profiling.h"
#include "content/public/common/url_constants.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension_urls.h"
#include "printing/buildflags/buildflags.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/actions/actions.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/window_open_disposition.h"
#include "ui/events/keycodes/keyboard_codes.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/devtools/devtools_policy_dialog.h"
#include "chrome/browser/lifetime/application_lifetime_desktop.h"
#include "chrome/browser/ui/interaction/browser_elements.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/ui/browser_commands_mac.h"
#endif

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include "base/win/windows_version.h"
#include "content/public/browser/gpu_data_manager.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ash/boca/on_task/on_task_locked_controller.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_context_menu.h"
#include "chrome/browser/ui/browser_commands_chromeos.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/user_manager.h"
#endif

#if BUILDFLAG(IS_LINUX)
#include "ui/base/ime/text_edit_commands.h"
#include "ui/base/ime/text_input_flags.h"
#include "ui/linux/linux_ui.h"
#endif

#if BUILDFLAG(IS_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)
#include "chrome/browser/ui/shortcuts/desktop_shortcuts_utils.h"
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)

static_assert(BUILDFLAG(ENABLE_EXTENSIONS));

using Extension = extensions::Extension;
using ExtensionRegistry = extensions::ExtensionRegistry;
using ExtensionRegistryObserver = extensions::ExtensionRegistryObserver;
using UnloadedExtensionReason = extensions::UnloadedExtensionReason;
using WebExposedIsolationLevel = content::WebExposedIsolationLevel;

DEFINE_USER_DATA(chrome::BrowserCommandController);

namespace chrome {

namespace {

bool CanOpenFile(BrowserWindowInterface* browser) {
  if (browser->GetType() == BrowserWindowInterface::Type::TYPE_DEVTOOLS ||
      browser->GetType() == BrowserWindowInterface::Type::TYPE_APP ||
      browser->GetType() == BrowserWindowInterface::Type::TYPE_APP_POPUP) {
    return false;
  }

  PrefService* local_state = g_browser_process->local_state();
  // May be null in unit tests.
  if (local_state) {
    return local_state->GetBoolean(prefs::kAllowFileSelectionDialogs);
  }

  return true;
}

void InvokeAction(actions::ActionId id, actions::ActionItem* scope) {
  actions::ActionManager::Get().FindAction(id, scope)->InvokeAction();
}

actions::ActionItem* FindAction(actions::ActionId action_id,
                                BrowserWindowInterface* browser) {
  actions::ActionItem* const root_action_item =
      BrowserActions::From(browser)->root_action_item();
  if (!root_action_item) {
    return nullptr;
  }
  return actions::ActionManager::Get().FindAction(action_id, root_action_item);
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// BrowserCommandController::ExtensionStateObserver

// Observes for extension state changes and notifies the controller.
class BrowserCommandController::ExtensionStateObserver
    : public ExtensionRegistryObserver {
 public:
  ExtensionStateObserver(BrowserCommandController* controller, Profile* profile)
      : controller_(CHECK_DEREF(controller)) {
    registry_observation_.Observe(ExtensionRegistry::Get(profile));
  }
  ExtensionStateObserver(const ExtensionStateObserver&) = delete;
  ExtensionStateObserver& operator=(const ExtensionStateObserver&) = delete;
  ~ExtensionStateObserver() override = default;

  // ExtensionRegistryObserver:
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const Extension* extension) override {
    controller_->ExtensionStateChanged();
  }
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const Extension* extension,
                           UnloadedExtensionReason reason) override {
    controller_->ExtensionStateChanged();
  }

 private:
  raw_ref<BrowserCommandController> controller_;
  base::ScopedObservation<ExtensionRegistry, ExtensionRegistryObserver>
      registry_observation_{this};
};

///////////////////////////////////////////////////////////////////////////////
// BrowserCommandController, public:

// static
BrowserCommandController* BrowserCommandController::From(
    BrowserWindowInterface* browser) {
  return Get(browser->GetUnownedUserDataHost());
}

// static
const BrowserCommandController* BrowserCommandController::From(
    const BrowserWindowInterface* browser) {
  return Get(browser->GetUnownedUserDataHost());
}

// TODO(crbug.com/434734349): Implement dependency injection for this class to
// allow removing the Browser dependency.
BrowserCommandController::BrowserCommandController(BrowserWindowInterface* bwi)
    : browser_(bwi),
      command_updater_(CreateCommandUpdater()),
      scoped_unowned_user_data_(bwi->GetUnownedUserDataHost(), *this) {
  browser_->tab_strip_model()->AddObserver(this);
  PrefService* local_state = g_browser_process->local_state();
  if (local_state) {
    local_pref_registrar_.Init(local_state);
    local_pref_registrar_.Add(
        prefs::kAllowFileSelectionDialogs,
        base::BindRepeating(
            &BrowserCommandController::UpdateCommandsForFileSelectionDialogs,
            base::Unretained(this)));
  }

  profile_pref_registrar_.Init(profile()->GetPrefs());
  if (!base::FeatureList::IsEnabled(features::kDevToolsShowPolicyDialog)) {
    profile_pref_registrar_.Add(
        prefs::kDevToolsAvailability,
        base::BindRepeating(
            &BrowserCommandController::UpdateCommandsForDevTools,
            base::Unretained(this)));
    profile_pref_registrar_.Add(
        prefs::kDeveloperToolsAvailabilityAllowlist,
        base::BindRepeating(
            &BrowserCommandController::UpdateCommandsForDevTools,
            base::Unretained(this)));
    profile_pref_registrar_.Add(
        prefs::kDeveloperToolsAvailabilityBlocklist,
        base::BindRepeating(
            &BrowserCommandController::UpdateCommandsForDevTools,
            base::Unretained(this)));
  }
#if BUILDFLAG(ENABLE_PRINTING)
  profile_pref_registrar_.Add(
      prefs::kPrintingEnabled,
      base::BindRepeating(&BrowserCommandController::UpdatePrintingState,
                          base::Unretained(this)));
#endif  // BUILDFLAG(ENABLE_PRINTING)
  profile_pref_registrar_.Add(
      policy::policy_prefs::kDownloadRestrictions,
      base::BindRepeating(&BrowserCommandController::UpdateSaveAsState,
                          base::Unretained(this)));
#if !BUILDFLAG(IS_MAC)
  profile_pref_registrar_.Add(
      prefs::kFullscreenAllowed,
      base::BindRepeating(
          &BrowserCommandController::UpdateCommandsForFullscreenMode,
          base::Unretained(this)));
#endif  //! BUILDFLAG(IS_MAC)


  InitCommandState();


  sessions::TabRestoreService* tab_restore_service =
      TabRestoreServiceFactory::GetForProfile(profile());
  if (tab_restore_service) {
    tab_restore_service->AddObserver(this);
    if (!tab_restore_service->IsLoaded()) {
      tab_restore_service->LoadTabsFromLastSession();
    }
  }

  extension_state_observer_ =
      std::make_unique<ExtensionStateObserver>(this, profile());
}

BrowserCommandController::~BrowserCommandController() {
  extension_state_observer_.reset();
  // TabRestoreService may have been shutdown by the time we get here. Don't
  // trigger creating it.
  sessions::TabRestoreService* tab_restore_service =
      TabRestoreServiceFactory::GetForProfileIfExisting(profile());
  if (tab_restore_service) {
    tab_restore_service->RemoveObserver(this);
  }
  profile_pref_registrar_.RemoveAll();
  local_pref_registrar_.RemoveAll();
}

bool BrowserCommandController::IsReservedCommandOrKey(
    int command_id,
    const input::NativeWebKeyboardEvent& event) {
  // In Apps mode, no keys are reserved.
  if (browser_->GetType() == BrowserWindowInterface::Type::TYPE_APP ||
      browser_->GetType() == BrowserWindowInterface::Type::TYPE_APP_POPUP) {
    return false;
  }

#if BUILDFLAG(IS_CHROMEOS)
  // On Chrome OS, the top row of keys are mapped to browser actions like
  // back/forward or refresh. We don't want web pages to be able to change the
  // behavior of these keys.  Ash handles F4 and up; this leaves us needing to
  // reserve browser back/forward and refresh here.
  ui::KeyboardCode key_code =
      static_cast<ui::KeyboardCode>(event.windows_key_code);
  if ((key_code == ui::VKEY_BROWSER_BACK && command_id == IDC_BACK) ||
      (key_code == ui::VKEY_BROWSER_FORWARD && command_id == IDC_FORWARD) ||
      (key_code == ui::VKEY_BROWSER_REFRESH && command_id == IDC_RELOAD)) {
    return true;
  }
#endif

  if (window()->IsFullscreen()) {
    // In fullscreen, all commands except for IDC_FULLSCREEN and IDC_EXIT should
    // be delivered to the web page. The intent to implement and ship can be
    // found in http://crbug.com/40501396.
    const bool is_exit_fullscreen =
        (command_id == IDC_EXIT || command_id == IDC_FULLSCREEN);
#if BUILDFLAG(IS_MAC)
    // This behavior is different on Mac OS, which has a unique user-initiated
    // full-screen mode. According to the discussion in
    // http://crbug.com/40511102, the commands should be reserved for
    // browser-side handling if the browser window's toolbar is visible.
    if (window()->IsToolbarShowing()) {
      if (command_id == IDC_FULLSCREEN) {
        return true;
      }
    } else {
      return is_exit_fullscreen;
    }
#else
    return is_exit_fullscreen;
#endif
  }

#if BUILDFLAG(IS_LINUX)
  // If this key was registered by the user as a content editing hotkey, then
  // it is not reserved.
  auto* linux_ui = ui::LinuxUi::instance();
  if (linux_ui && event.os_event &&
      linux_ui->GetTextEditCommandForEvent(*event.os_event,
                                           ui::TEXT_INPUT_FLAG_NONE) !=
          ui::TextEditCommand::INVALID_COMMAND) {
    return false;
  }
#endif

  return command_id == IDC_CLOSE_TAB || command_id == IDC_CLOSE_WINDOW ||
         command_id == IDC_NEW_TAB ||
         command_id == IDC_NEW_WINDOW || command_id == IDC_RESTORE_TAB ||
         command_id == IDC_SELECT_NEXT_TAB ||
         command_id == IDC_SELECT_PREVIOUS_TAB ||
         command_id == IDC_CYCLE_TO_NEXT_TAB ||
         command_id == IDC_CYCLE_TO_PREV_TAB || command_id == IDC_EXIT;
}

void BrowserCommandController::TabStateChanged() {
  UpdateCommandsForTabState();
  UpdateCommandsForWebContentsFocus();
}

void BrowserCommandController::ZoomStateChanged() {
  UpdateCommandsForZoomState();
}

void BrowserCommandController::ContentRestrictionsChanged() {
  UpdateCommandsForContentRestrictionState();
}

void BrowserCommandController::FullscreenStateChanged() {
  UpdateCommandsForFullscreenMode();
}

#if BUILDFLAG(IS_CHROMEOS)
void BrowserCommandController::LockedFullscreenStateChanged() {
  UpdateCommandsForLockedFullscreenMode();
}
#endif

void BrowserCommandController::PrintingStateChanged() {
  UpdatePrintingState();
}

void BrowserCommandController::LoadingStateChanged(bool is_loading,
                                                   bool force) {
  UpdateReloadStopState(is_loading, force);
}




void BrowserCommandController::FindBarVisibilityChanged() {
  // Block find command updates in locked fullscreen mode unless the instance is
  // locked for OnTask (only relevant for non-web browser scenarios).
  // TODO(crbug.com/365146870): Remove once we consolidate locked fullscreen
  // with OnTask.
#if BUILDFLAG(IS_CHROMEOS)
  bool should_block_command_update = is_locked_fullscreen_;
  if (ash::boca::OnTaskLockedController::From(browser_)
          ->is_locked_for_on_task()) {
    should_block_command_update = false;
  }
  if (should_block_command_update) {
    return;
  }
#endif
  UpdateCloseFindOrStop();
}

void BrowserCommandController::ExtensionStateChanged() {}

void BrowserCommandController::TabKeyboardFocusChangedTo(
    std::optional<int> index) {
  UpdateCommandsForTabKeyboardFocus(index);
}

void BrowserCommandController::WebContentsFocusChanged() {
  UpdateCommandsForWebContentsFocus();
}

////////////////////////////////////////////////////////////////////////////////
// BrowserCommandController, CommandUpdater implementation:

bool BrowserCommandController::SupportsCommand(int id) const {
  return command_updater_->SupportsCommand(id);
}

bool BrowserCommandController::IsCommandEnabled(int id) const {
  return command_updater_->IsCommandEnabled(id);
}

bool BrowserCommandController::ExecuteCommandWithDispositionAndContext(
    int id,
    WindowOpenDisposition disposition,
    std::optional<actions::ActionInvocationContext> context,
    base::TimeTicks time_stamp) {
  if (!SupportsCommand(id) || !IsCommandEnabled(id)) {
    return false;
  }

  // No commands are enabled if there is not yet any selected tab.
  // TODO(pkasting): It seems like we should not need this, because either
  // most/all commands should not have been enabled yet anyway or the ones that
  // are enabled should be global, or safe themselves against having no selected
  // tab.  However, Ben says he tried removing this before and got lots of
  // crashes, e.g. from Windows sending WM_COMMANDs at random times during
  // window construction.  This probably could use closer examination someday.
  if (browser_->tab_strip_model()->active_index() == TabStripModel::kNoTab) {
    return true;
  }

  DCHECK(IsCommandEnabled(id)) << "Invalid/disabled command " << id;

  return command_updater_->ExecuteCommandWithDisposition(
      id, disposition, std::move(context), time_stamp);
}

void BrowserCommandController::HandleCommandWithDisposition(
    int id,
    WindowOpenDisposition disposition,
    base::TimeTicks time_stamp) {
  DCHECK(IsCommandEnabled(id)) << "Invalid/disabled command " << id;

  // The order of commands in this switch statement must match the function
  // declaration order in browser.h!
  switch (id) {
    // Navigation commands
    case IDC_BACK:
      GoBack(browser_, disposition);
      break;
    case IDC_FORWARD:
      GoForward(browser_, disposition);
      break;
    case IDC_RELOAD:
      Reload(browser_, disposition);
      break;
    case IDC_RELOAD_CLEARING_CACHE:
      ClearCache(browser_);
      [[fallthrough]];
    case IDC_RELOAD_BYPASSING_CACHE:
      ReloadBypassingCache(browser_, disposition);
      break;
    case IDC_HOME:
      Home(browser_, disposition);
      break;
    case IDC_OPEN_CURRENT_URL:
      OpenCurrentURL(browser_);
      break;
    case IDC_STOP:
      Stop(browser_);
      break;
    case IDC_TAB_SEARCH_TOGGLE_PIN:
      ToggleTabSearchPin(browser_);
      break;
    case IDC_TAB_SCROLL_BUTTONS_TOGGLE_PIN:
      ToggleTabScrollButtonsPin(browser_);
      break;
    case IDC_TOGGLE_VERTICAL_TABS:
      ToggleVerticalTabs(browser_);
      break;
    case IDC_TOGGLE_VERTICAL_TABS_COLLAPSE:
#if !BUILDFLAG(IS_MAC)
      // On Mac, the logging for both the keyboard shortcut and the view menu
      // is handled in BrowserNativeWidgetMac::ExecuteCommand to correctly
      // distinguish between the two trigger sources.
      if (auto* controller =
              tabs::VerticalTabStripStateController::From(browser_)) {
        if (controller->IsCollapsed()) {
          base::RecordAction(base::UserMetricsAction(
              "VerticalTabs_TabStrip_KeyboardShortcutToggleUncollapsed"));
        } else {
          base::RecordAction(base::UserMetricsAction(
              "VerticalTabs_TabStrip_KeyboardShortcutToggleCollapsed"));
        }
      }
#endif  // !BUILDFLAG(IS_MAC)
      ToggleCollapseVerticalTabs(browser_);
      break;
    case IDC_TOGGLE_VERTICAL_TABS_EXPAND_ON_HOVER:
      ToggleVerticalTabsExpandOnHover(browser_);
      break;
    // Window management commands
    case IDC_NEW_WINDOW:
      NewWindow(browser_);
      break;
    case IDC_CLOSE_WINDOW:
      base::RecordAction(base::UserMetricsAction("CloseWindowByKey"));
      CloseWindow(browser_);
      break;
    case IDC_NEW_TAB: {
      NewTab(browser_, NewTabTypes::kNewTabCommand);
      break;
    }
    case IDC_NEW_TAB_TO_RIGHT: {
      NewTabToRight(browser_);
      break;
    }
    case IDC_CLOSE_TAB:
      base::RecordAction(base::UserMetricsAction("CloseTabByKey"));
      CloseTab(browser_);
      break;
    case IDC_SELECT_NEXT_TAB:
      base::RecordAction(base::UserMetricsAction("Accel_SelectNextTab"));
      SelectNextTab(
          browser_,
          TabStripUserGestureDetails(
              TabStripUserGestureDetails::GestureType::kKeyboard, time_stamp));
      break;
    case IDC_SELECT_PREVIOUS_TAB:
      base::RecordAction(base::UserMetricsAction("Accel_SelectPreviousTab"));
      SelectPreviousTab(
          browser_,
          TabStripUserGestureDetails(
              TabStripUserGestureDetails::GestureType::kKeyboard, time_stamp));
      break;
    case IDC_CYCLE_TO_NEXT_TAB:
      if (IsCtrlTabMruEnabled(browser_)) {
        base::RecordAction(base::UserMetricsAction("Accel_CycleToNextTab"));
        CycleToMruTab(browser_,
                      TabStripUserGestureDetails(
                          TabStripUserGestureDetails::GestureType::kKeyboard,
                          time_stamp));
      } else {
        base::RecordAction(base::UserMetricsAction("Accel_SelectNextTab"));
        SelectNextTab(browser_,
                      TabStripUserGestureDetails(
                          TabStripUserGestureDetails::GestureType::kKeyboard,
                          time_stamp));
      }
      break;
    case IDC_CYCLE_TO_PREV_TAB:
      if (IsCtrlTabMruEnabled(browser_)) {
        base::RecordAction(base::UserMetricsAction("Accel_CycleToPrevTab"));
        CycleToMruTab(browser_,
                      TabStripUserGestureDetails(
                          TabStripUserGestureDetails::GestureType::kKeyboard,
                          time_stamp));
      } else {
        base::RecordAction(base::UserMetricsAction("Accel_SelectPreviousTab"));
        SelectPreviousTab(
            browser_, TabStripUserGestureDetails(
                          TabStripUserGestureDetails::GestureType::kKeyboard,
                          time_stamp));
      }
      break;
    case IDC_MOVE_TAB_NEXT:
      MoveTabNext(browser_);
      break;
    case IDC_MOVE_TAB_PREVIOUS:
      MoveTabPrevious(browser_);
      break;
    case IDC_SELECT_TAB_0:
    case IDC_SELECT_TAB_1:
    case IDC_SELECT_TAB_2:
    case IDC_SELECT_TAB_3:
    case IDC_SELECT_TAB_4:
    case IDC_SELECT_TAB_5:
    case IDC_SELECT_TAB_6:
    case IDC_SELECT_TAB_7:
      base::RecordAction(base::UserMetricsAction("Accel_SelectNumberedTab"));
      SelectNumberedTab(
          browser_, id - IDC_SELECT_TAB_0,
          TabStripUserGestureDetails(
              TabStripUserGestureDetails::GestureType::kKeyboard, time_stamp));
      break;
    case IDC_SELECT_LAST_TAB:
      base::RecordAction(base::UserMetricsAction("Accel_SelectNumberedTab"));
      SelectLastTab(
          browser_,
          TabStripUserGestureDetails(
              TabStripUserGestureDetails::GestureType::kKeyboard, time_stamp));
      break;
    case IDC_DUPLICATE_TAB:
      DuplicateTab(browser_);
      break;
    case IDC_RESTORE_TAB:
      RestoreTab(browser_);
      break;
    case IDC_SHOW_AS_TAB:
      ConvertPopupToTabbedBrowser(browser_);
      break;
    case IDC_FULLSCREEN:
      chrome::ToggleFullscreenMode(browser_, /*user_initiated=*/true);
      break;
    case IDC_MOVE_TAB_TO_NEW_WINDOW:
      MoveActiveTabToNewWindow(browser_);
      break;
    case IDC_NEW_SPLIT_TAB:
      if (!browser_->tab_strip_model()->GetActiveTab()->IsSplit()) {
        NewSplitTab(browser_, split_tabs::SplitTabLayout::kSideBySide,
                    split_tabs::SplitTabCreatedSource::kKeyboardShortcut);
      }
      break;
    case IDC_NAME_WINDOW:
      PromptToNameWindow(browser_);
      break;

#if BUILDFLAG(IS_CHROMEOS)
    case IDC_TAKE_SCREENSHOT:
      TakeScreenshot();
      break;
    case IDC_TOGGLE_MULTITASK_MENU:
      ToggleMultitaskMenu(browser_);
      break;
    case IDC_VISIT_DESKTOP_OF_LRU_USER_2:
    case IDC_VISIT_DESKTOP_OF_LRU_USER_3:
    case IDC_VISIT_DESKTOP_OF_LRU_USER_4:
    case IDC_VISIT_DESKTOP_OF_LRU_USER_5:
      ExecuteVisitDesktopCommand(id, window()->GetNativeWindow());
      break;
#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)
    case IDC_MINIMIZE_WINDOW:
      browser_->GetWindow()->Minimize();
      break;
    case IDC_MAXIMIZE_WINDOW:
      browser_->GetWindow()->Maximize();
      break;
    case IDC_RESTORE_WINDOW:
      browser_->GetWindow()->Restore();
      break;

#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_LINUX)
    case IDC_USE_SYSTEM_TITLE_BAR: {
      PrefService* prefs = profile()->GetPrefs();
      prefs->SetBoolean(prefs::kUseCustomChromeFrame,
                        !prefs->GetBoolean(prefs::kUseCustomChromeFrame));
      break;
    }
#endif  // BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_WIN)
    case IDC_MOVE_WINDOW:
      chrome::OpenMoveWindow(browser_);
      break;
    case IDC_SIZE_WINDOW:
      chrome::OpenSizeWindow(browser_);
      break;
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_MAC)
    case IDC_TOGGLE_FULLSCREEN_TOOLBAR:
      chrome::ToggleAlwaysShowToolbarInFullscreen(browser_);
      break;
    case IDC_TOGGLE_JAVASCRIPT_APPLE_EVENTS: {
      content::WebContents* web_contents =
          browser_->tab_strip_model()->GetActiveWebContents();
      if (base::FeatureList::IsEnabled(features::kDevToolsShowPolicyDialog) &&
          !DevToolsWindow::AllowDevToolsFor(profile(), web_contents)) {
        DevToolsPolicyDialog::Show(web_contents);
      } else {
        chrome::ToggleJavaScriptFromAppleEventsAllowed(browser_);
      }
      break;
    }
#endif
    case IDC_EXIT:
      Exit();
      break;

    // Page-related commands
    case IDC_SAVE_PAGE:
      SavePage(browser_);
      break;
    case IDC_VIEW_SOURCE: {
      content::WebContents* web_contents =
          browser_->tab_strip_model()->GetActiveWebContents();
      if (base::FeatureList::IsEnabled(features::kDevToolsShowPolicyDialog) &&
          !DevToolsWindow::AllowDevToolsFor(profile(), web_contents)) {
#if !BUILDFLAG(IS_ANDROID)
        DevToolsPolicyDialog::Show(web_contents);
#endif
      } else {
        web_contents->GetPrimaryMainFrame()->ViewSource();
      }
      break;
    }
    case IDC_PRINT:
      Print(browser_);
      break;

#if BUILDFLAG(ENABLE_PRINTING)
    case IDC_BASIC_PRINT:
      base::RecordAction(base::UserMetricsAction("Accel_Advanced_Print"));
      BasicPrint(browser_);
      break;
#endif  // ENABLE_PRINTING

    // Clipboard commands
    case IDC_CUT:
      InvokeAction(actions::kActionCut,
                   BrowserActions::From(browser_)->root_action_item());
      break;
    case IDC_COPY:
      InvokeAction(actions::kActionCopy,
                   BrowserActions::From(browser_)->root_action_item());
      break;
    case IDC_PASTE:
      InvokeAction(actions::kActionPaste,
                   BrowserActions::From(browser_)->root_action_item());
      break;

    // Find-in-page
    case IDC_FIND:
      Find(browser_);
      break;
    case IDC_FIND_NEXT:
      FindNext(browser_);
      break;
    case IDC_FIND_PREVIOUS:
      FindPrevious(browser_);
      break;
    case IDC_CLOSE_FIND_OR_STOP:
      if (CanCloseFind(browser_)) {
        CloseFind(browser_);
      } else if (IsCommandEnabled(IDC_STOP)) {
        ExecuteCommand(IDC_STOP);
      }
      break;

    // Zoom
    case IDC_ZOOM_PLUS:
      Zoom(browser_, content::PAGE_ZOOM_IN);
      break;
    case IDC_ZOOM_NORMAL:
      Zoom(browser_, content::PAGE_ZOOM_RESET);
      break;
    case IDC_ZOOM_MINUS:
      Zoom(browser_, content::PAGE_ZOOM_OUT);
      break;

    // Focus various bits of UI
    case IDC_FOCUS_TOOLBAR:
      base::RecordAction(base::UserMetricsAction("Accel_Focus_Toolbar"));
      FocusToolbar(browser_);
      break;
    case IDC_FOCUS_LOCATION:
      if (!window()->IsLocationBarVisible()) {
        break;
      }
      base::RecordAction(base::UserMetricsAction("Accel_Focus_Location"));
      FocusLocationBar(browser_);
      break;
    case IDC_FOCUS_SEARCH:
      base::RecordAction(base::UserMetricsAction("Accel_Focus_Search"));
      FocusSearch(browser_);
      break;
    case IDC_FOCUS_MENU_BAR:
      FocusAppMenu(browser_);
      break;
    case IDC_FOCUS_INACTIVE_POPUP_FOR_ACCESSIBILITY:
      FocusInactivePopupForAccessibility(browser_);
      break;
    case IDC_FOCUS_NEXT_PANE:
      FocusNextPane(browser_);
      break;
    case IDC_FOCUS_PREVIOUS_PANE:
      FocusPreviousPane(browser_);
      break;
    case IDC_FOCUS_WEB_CONTENTS_PANE:
      FocusWebContentsPane(browser_);
      break;

    // Show various bits of UI
    case IDC_OPEN_FILE:
      BrowserSelectFileDialogController::From(browser_)->OpenFile();
      break;
    case IDC_CREATE_SHORTCUT:
      base::RecordAction(base::UserMetricsAction("CreateShortcut"));
      chrome::CreateDesktopShortcutForActiveWebContents(browser_);
      break;
    case IDC_DEV_TOOLS:
      ToggleDevToolsWindow(browser_, DevToolsToggleAction::Show(),
                           DevToolsOpenedByAction::kMainMenuOrMainShortcut);
      break;
    case IDC_DEV_TOOLS_CONSOLE:
      ToggleDevToolsWindow(browser_, DevToolsToggleAction::ShowConsolePanel(),
                           DevToolsOpenedByAction::kConsoleShortcut);
      break;
    case IDC_DEV_TOOLS_DEVICES:
      InspectUI::InspectDevices(browser_);
      break;
    case IDC_DEV_TOOLS_INSPECT:
      ToggleDevToolsWindow(browser_, DevToolsToggleAction::Inspect(),
                           DevToolsOpenedByAction::kInspectorModeShortcut);
      break;
    case IDC_DEV_TOOLS_TOGGLE:
      ToggleDevToolsWindow(browser_, DevToolsToggleAction::Toggle(),
                           DevToolsOpenedByAction::kToggleShortcut);
      break;
    case IDC_TASK_MANAGER:
      OpenTaskManager(browser_);
      break;
    case IDC_TASK_MANAGER_APP_MENU:
      OpenTaskManager(browser_, task_manager::StartAction::kMoreTools);
      break;
    case IDC_TASK_MANAGER_SHORTCUT:
      OpenTaskManager(browser_, task_manager::StartAction::kShortcut);
      break;
    case IDC_TASK_MANAGER_CONTEXT_MENU:
      OpenTaskManager(browser_, task_manager::StartAction::kContextMenu);
      break;
    case IDC_TASK_MANAGER_MAIN_MENU:
      OpenTaskManager(browser_, task_manager::StartAction::kMainMenu);
      break;
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    case IDC_REPORT_UNSAFE_SITE:
      OpenReportUnsafeSiteDialog(browser_);
      break;
#endif
    case IDC_SHOW_CHROME_LABS:
      window()->ShowChromeLabs();
      break;
    case IDC_SHOW_FULL_URLS:
      ToggleShowFullURLs(browser_);
      break;
    case IDC_SHOW_SEARCH_TOOLS:
      ToggleShowSearchTools(browser_);
      break;
    case IDC_PROFILING_ENABLED:
      content::Profiling::Toggle();
      break;
    case IDC_CARET_BROWSING_TOGGLE:
      ToggleCaretBrowsing(browser_);
      break;
    case IDC_SHOW_APP_MENU:
      base::RecordAction(base::UserMetricsAction("Accel_Show_App_Menu"));
      ShowAppMenu(browser_);
      break;
    case IDC_SHOW_HISTORY:
      ShowHistory(webui::GetBrowserForOpeningWebUi(browser_));
      break;
    case IDC_SHOW_DOWNLOADS:
      ShowDownloads(webui::GetBrowserForOpeningWebUi(browser_));
      break;
    case IDC_MANAGE_EXTENSIONS:
    case IDC_SAFETY_HUB_MANAGE_EXTENSIONS:
      ShowExtensions(webui::GetBrowserForOpeningWebUi(browser_));
      break;
    case IDC_EXTENSIONS_SUBMENU_MANAGE_EXTENSIONS:
      ShowExtensions(webui::GetBrowserForOpeningWebUi(browser_));
      break;
    case IDC_EXTENSIONS_SUBMENU_VISIT_CHROME_WEB_STORE:
    case IDC_FIND_EXTENSIONS:
      ShowWebStore(browser_, extension_urls::kAppMenuUtmSource);
      break;
    case IDC_PERFORMANCE:
      ShowSettingsSubPage(webui::GetBrowserForOpeningWebUi(browser_),
                          chrome::kPerformanceSubPage);
      break;
    case IDC_OPTIONS:
      ShowSettings(webui::GetBrowserForOpeningWebUi(browser_));
      break;
    case IDC_EDIT_SEARCH_ENGINES:
      ShowSearchEngineSettings(webui::GetBrowserForOpeningWebUi(browser_));
      break;
    case IDC_CLEAR_BROWSING_DATA: {
      ShowClearBrowsingDataDialog(webui::GetBrowserForOpeningWebUi(browser_));
#if !BUILDFLAG(IS_ANDROID)
      ui::ElementContext context =
          BrowserElements::From(browser_)->GetContext();
      ui::TrackedElement* const tracked_element =
          ui::ElementTracker::GetElementTracker()->GetUniqueElement(
              kBrowserViewElementId, context);
      if (tracked_element) {
        ui::ElementTracker::GetFrameworkDelegate()->NotifyCustomEvent(
            tracked_element, browsing_data_important_sites_util::
                                 kShowClearBrowsingDataDialogEventId);
      }
#endif  // !BUILDFLAG(IS_ANDROID)
      break;
    }
    case IDC_TOGGLE_REQUEST_TABLET_SITE:
      ToggleRequestTabletSite(browser_);
      break;
    case IDC_ABOUT:
      ShowAboutChrome(webui::GetBrowserForOpeningWebUi(browser_));
      break;
    case IDC_UPGRADE_DIALOG:
      OpenUpdateChromeDialog(browser_);
      break;
    case IDC_OPEN_SAFETY_HUB:
      ShowSettingsSubPage(webui::GetBrowserForOpeningWebUi(browser_),
                          chrome::kSafetyHubSubPage);
      break;
    case IDC_HELP_PAGE_VIA_KEYBOARD:
      ShowHelp(browser_, chrome::HelpSource::kKeyboard);
      break;
    case IDC_HELP_PAGE_VIA_MENU:
      ShowHelp(browser_, chrome::HelpSource::kMenu);
      break;
    case IDC_CHROME_TIPS:
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
      ShowChromeTips(browser_);
      break;
#else
      NOTREACHED();
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
    case IDC_CHROME_WHATS_NEW:
#if BUILDFLAG(GOOGLE_CHROME_BRANDING) && \
    (BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX))
      ShowChromeWhatsNew(browser_);
      break;
#else
      NOTREACHED();
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING) && \
        // (BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX))
    case IDC_SHOW_BETA_FORUM:
      ShowBetaForum(browser_);
      break;
    case IDC_CHROME_ENTERPRISE_RELEASE_NOTES:
      if (base::FeatureList::IsEnabled(features::kEnterpriseReleaseNotes)) {
        chrome::ShowChromeEnterpriseReleaseNotes(browser_);
      }
      break;
    case IDC_WINDOW_MUTE_SITE:
      MuteSite(browser_);
      break;
    case IDC_WINDOW_PIN_TAB:
      PinTab(browser_);
      break;
    case IDC_WINDOW_GROUP_TAB:
      GroupTab(browser_);
      break;

    // Tab group commands.
    case IDC_FOCUS_NEXT_TAB_GROUP:
      if (base::i18n::IsRTL()) {
        FocusPreviousTabGroup(browser_);
      } else {
        FocusNextTabGroup(browser_);
      }
      base::UmaHistogramEnumeration("TabGroups.Shortcuts",
                                    TabGroupShortcut::kFocusNextTabGroup);
      break;
    case IDC_FOCUS_PREV_TAB_GROUP:
      if (base::i18n::IsRTL()) {
        FocusNextTabGroup(browser_);
      } else {
        FocusPreviousTabGroup(browser_);
      }
      base::UmaHistogramEnumeration("TabGroups.Shortcuts",
                                    TabGroupShortcut::kFocusPrevTabGroup);
      break;
    case IDC_CLOSE_TAB_GROUP:
      CloseTabGroup(browser_);
      base::UmaHistogramEnumeration("TabGroups.Shortcuts",
                                    TabGroupShortcut::kCloseTabGroup);
      break;
    case IDC_CREATE_NEW_TAB_GROUP:
      CreateNewTabGroup(browser_);
      base::UmaHistogramEnumeration("TabGroups.Shortcuts",
                                    TabGroupShortcut::kCreateNewTabGroup);
      break;
    case IDC_ADD_NEW_TAB_TO_GROUP:
      AddNewTabToGroup(browser_);
      base::UmaHistogramEnumeration("TabGroups.Shortcuts",
                                    TabGroupShortcut::kAddNewTabToGroup);
      break;
    case IDC_GROUP_UNGROUPED_TABS:
      GroupAllUngroupedTabs(browser_);
      base::RecordAction(
          base::UserMetricsAction("TabGroups_GroupAllUngroupedTabs"));
      break;
    case IDC_ADD_NEW_TAB_RECENT_GROUP:
      AddNewTabToRecentGroup(browser_);
      break;
    case IDC_UNFOCUS_TAB_GROUP:
      UnfocusTabGroup(browser_, TabGroupFocusExitReason::kTabStripButton);
      break;
    case IDC_WINDOW_CLOSE_TABS_TO_RIGHT:
      CloseTabsToRight(browser_);
      break;
    case IDC_WINDOW_CLOSE_OTHER_TABS:
      CloseOtherTabs(browser_);
      break;
    case IDC_SHOW_MANAGEMENT_PAGE: {
      ShowSingletonTab(browser_, GetManagedUiUrl(profile()));
      break;
    }
    case IDC_MUTE_TARGET_SITE:
      MuteSiteForKeyboardFocusedTab(browser_);
      break;
    case IDC_PIN_TARGET_TAB:
      PinKeyboardFocusedTab(browser_);
      break;
    case IDC_GROUP_TARGET_TAB:
      GroupKeyboardFocusedTab(browser_);
      break;
    case IDC_DUPLICATE_TARGET_TAB:
      DuplicateKeyboardFocusedTab(browser_);
      break;
    // Hosted App commands
    case IDC_COPY_URL:
      CopyURL(browser_, browser_->tab_strip_model()->GetActiveWebContents());
      break;

    // UI debug commands
    case IDC_DEBUG_TOGGLE_TABLET_MODE:
    case IDC_DEBUG_PRINT_VIEW_TREE:
    case IDC_DEBUG_PRINT_VIEW_TREE_DETAILS:
    case IDC_DEBUG_PRINT_WINDOW_HIERARCHY:
    case IDC_DEBUG_PRINT_LAYER_HIERARCHY:
      ExecuteUIDebugCommand(id, browser_);
      break;



#if !BUILDFLAG(IS_CHROMEOS)
    case IDC_SET_BROWSER_AS_DEFAULT:
      base::MakeRefCounted<shell_integration::DefaultBrowserWorker>()
          ->StartSetAsDefault(base::DoNothing());
      break;
#endif
    default:
      LOG(WARNING) << "Received Unimplemented Command: " << id;
      break;
  }
}

void BrowserCommandController::AddCommandObserver(int id,
                                                  CommandObserver* observer) {
  command_updater_->AddCommandObserver(id, observer);
}

void BrowserCommandController::RemoveCommandObserver(
    int id,
    CommandObserver* observer) {
  command_updater_->RemoveCommandObserver(id, observer);
}

void BrowserCommandController::RemoveCommandObserver(
    CommandObserver* observer) {
  command_updater_->RemoveCommandObserver(observer);
}

bool BrowserCommandController::UpdateCommandEnabled(int id, bool state) {
  // Block individual command updates in locked fullscreen mode unless the
  // instance is locked for OnTask (only relevant for non-web browser
  // scenarios).
  // TODO(crbug.com/365146870): Remove once we consolidate locked fullscreen
  // with OnTask.
#if BUILDFLAG(IS_CHROMEOS)
  bool should_block_command_update = is_locked_fullscreen_;
  if (ash::boca::OnTaskLockedController::From(browser_)
          ->is_locked_for_on_task()) {
    should_block_command_update = false;
  }
  if (should_block_command_update) {
    return false;
  }
#endif

  return command_updater_->UpdateCommandEnabled(id, state);
}

void BrowserCommandController::DisableAllCommands() {
  command_updater_->DisableAllCommands();
}

std::vector<int> BrowserCommandController::GetAllIds() const {
  return command_updater_->GetAllIds();
}

// BrowserCommandController, TabStripModelObserver implementation:

void BrowserCommandController::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  UpdateCommandsForTabStripStateChanged();
}

void BrowserCommandController::TabGroupedStateChanged(
    TabStripModel* tab_strip_model,
    std::optional<tab_groups::TabGroupId> old_group,
    std::optional<tab_groups::TabGroupId> new_group,
    tabs::TabInterface* tab,
    int index) {
  UpdateCommandsForTabStripStateChanged();
}

void BrowserCommandController::OnTabChangedAt(tabs::TabInterface* tab,
                                              TabChangeType change_type) {
  if (change_type == TabChangeType::kBlockedOnly) {
    PrintingStateChanged();
    FullscreenStateChanged();
    UpdateCommandsForFind();
  }
}

void BrowserCommandController::OnTabPinnedStateChanged(tabs::TabInterface* tab,
                                                       int index) {
  UpdateCommandsForTabStripStateChanged();
}

void BrowserCommandController::OnTabGroupFocusChanged(
    std::optional<tab_groups::TabGroupId> new_focused_group,
    std::optional<tab_groups::TabGroupId> old_focused_group) {
  UpdateCommandsForTabGroupFocusChanged();
}

////////////////////////////////////////////////////////////////////////////////
// BrowserCommandController, TabRestoreServiceObserver implementation:

void BrowserCommandController::TabRestoreServiceChanged(
    sessions::TabRestoreService* service) {
  UpdateTabRestoreCommandState();
}

void BrowserCommandController::TabRestoreServiceDestroyed(
    sessions::TabRestoreService* service) {
  service->RemoveObserver(this);
}

void BrowserCommandController::TabRestoreServiceLoaded(
    sessions::TabRestoreService* service) {
  UpdateTabRestoreCommandState();
}

////////////////////////////////////////////////////////////////////////////////
// BrowserCommandController, private:

bool BrowserCommandController::IsShowingMainUI() {
  return WindowFeatureController::From(browser_)->SupportsWindowFeature(
      WindowFeatureController::WindowFeature::kFeatureTabStrip);
}

bool BrowserCommandController::IsShowingLocationBar() {
  return WindowFeatureController::From(browser_)->SupportsWindowFeature(
      WindowFeatureController::WindowFeature::kFeatureLocationBar);
}

void BrowserCommandController::InitCommandState() {
  // All browser commands whose state isn't set automagically some other way
  // (like Back & Forward with initial page load) must have their state
  // initialized here, otherwise they will be forever disabled.

#if BUILDFLAG(IS_CHROMEOS)
  if (is_locked_fullscreen_) {
    return;
  }
#endif

  // Navigation commands
  const bool can_reload = CanReload(browser_);
  command_updater_->UpdateCommandEnabled(IDC_RELOAD, can_reload);
  command_updater_->UpdateCommandEnabled(IDC_RELOAD_BYPASSING_CACHE,
                                         can_reload);
  command_updater_->UpdateCommandEnabled(IDC_RELOAD_CLEARING_CACHE, can_reload);

  // Tab group (agent) commands. Agent Chrome: no commands that create
  // groups; agents are made over MCP.
  command_updater_->UpdateCommandEnabled(IDC_ADD_NEW_TAB_TO_GROUP, true);
  command_updater_->UpdateCommandEnabled(IDC_FOCUS_NEXT_TAB_GROUP, true);
  command_updater_->UpdateCommandEnabled(IDC_FOCUS_PREV_TAB_GROUP, true);
  command_updater_->UpdateCommandEnabled(IDC_CLOSE_TAB_GROUP, true);
  command_updater_->UpdateCommandEnabled(IDC_ADD_NEW_TAB_RECENT_GROUP, true);
  command_updater_->UpdateCommandEnabled(IDC_UNFOCUS_TAB_GROUP, true);

  // Omnibox commands
  command_updater_->UpdateCommandEnabled(IDC_SHOW_FULL_URLS, true);
  command_updater_->UpdateCommandEnabled(IDC_SHOW_SEARCH_TOOLS, true);

  // Window management commands
  command_updater_->UpdateCommandEnabled(IDC_CLOSE_WINDOW, true);
  command_updater_->UpdateCommandEnabled(IDC_NEW_TAB, true);
  command_updater_->UpdateCommandEnabled(IDC_CLOSE_TAB, true);
  command_updater_->UpdateCommandEnabled(
      IDC_DUPLICATE_TAB,
      browser_->GetType() !=
          BrowserWindowInterface::Type::TYPE_PICTURE_IN_PICTURE);
  UpdateTabRestoreCommandState();
  command_updater_->UpdateCommandEnabled(IDC_EXIT, true);
  command_updater_->UpdateCommandEnabled(IDC_NAME_WINDOW, true);
  command_updater_->UpdateCommandEnabled(IDC_TOGGLE_VERTICAL_TABS, true);
  command_updater_->UpdateCommandEnabled(
      IDC_TOGGLE_VERTICAL_TABS_EXPAND_ON_HOVER, true);
  command_updater_->UpdateCommandEnabled(IDC_TOGGLE_VERTICAL_TABS_COLLAPSE,
                                         true);
#if BUILDFLAG(IS_CHROMEOS)
  command_updater_->UpdateCommandEnabled(IDC_TOGGLE_MULTITASK_MENU, true);
  command_updater_->UpdateCommandEnabled(IDC_MINIMIZE_WINDOW, true);
  // The VisitDesktop command is only supported for up to 5 logged in users
  // because that's the max number of user sessions. If that number is increased
  // the IDC_VISIT_DESKTOP_OF_LRU_USER_ command ids should be updated as well.
  // crbug.com/41446075
  static_assert(
      session_manager::kMaximumNumberOfUserSessions <=
          IDC_VISIT_DESKTOP_OF_LRU_USER_LAST -
              IDC_VISIT_DESKTOP_OF_LRU_USER_NEXT + 2,
      "The max number of user sessions exceeds the number of users supported.");
  command_updater_->UpdateCommandEnabled(IDC_VISIT_DESKTOP_OF_LRU_USER_2, true);
  command_updater_->UpdateCommandEnabled(IDC_VISIT_DESKTOP_OF_LRU_USER_3, true);
  command_updater_->UpdateCommandEnabled(IDC_VISIT_DESKTOP_OF_LRU_USER_4, true);
  command_updater_->UpdateCommandEnabled(IDC_VISIT_DESKTOP_OF_LRU_USER_5, true);
#endif
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)
  command_updater_->UpdateCommandEnabled(IDC_MINIMIZE_WINDOW, true);
  command_updater_->UpdateCommandEnabled(IDC_MAXIMIZE_WINDOW, true);
  command_updater_->UpdateCommandEnabled(IDC_RESTORE_WINDOW, true);
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_LINUX)
  bool use_system_title_bar = true;
#if BUILDFLAG(IS_OZONE)
  use_system_title_bar = ui::OzonePlatform::GetInstance()
                             ->GetPlatformRuntimeProperties()
                             .supports_server_side_window_decorations;
#endif  // BUILDFLAG(IS_OZONE)
  command_updater_->UpdateCommandEnabled(IDC_USE_SYSTEM_TITLE_BAR,
                                         use_system_title_bar);
#endif  // BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_WIN)
  command_updater_->UpdateCommandEnabled(IDC_MOVE_WINDOW, true);
  command_updater_->UpdateCommandEnabled(IDC_SIZE_WINDOW, true);
#endif  // BUILDFLAG(IS_WIN)


  // Zoom
  command_updater_->UpdateCommandEnabled(IDC_ZOOM_MENU, true);
  command_updater_->UpdateCommandEnabled(IDC_ZOOM_PLUS, true);
  command_updater_->UpdateCommandEnabled(IDC_ZOOM_NORMAL, false);
  command_updater_->UpdateCommandEnabled(IDC_ZOOM_MINUS, true);

  // Show various bits of UI
  const bool normal_window =
      browser_->GetType() == BrowserWindowInterface::Type::TYPE_NORMAL;

  command_updater_->UpdateCommandEnabled(IDC_OPEN_FILE, CanOpenFile(browser_));

  if (base::FeatureList::IsEnabled(features::kDevToolsShowPolicyDialog)) {
    const bool dev_tools_enabled = true;
    command_updater_->UpdateCommandEnabled(IDC_DEV_TOOLS, dev_tools_enabled);
    command_updater_->UpdateCommandEnabled(IDC_DEV_TOOLS_CONSOLE,
                                           dev_tools_enabled);
    command_updater_->UpdateCommandEnabled(IDC_DEV_TOOLS_DEVICES,
                                           dev_tools_enabled);
    command_updater_->UpdateCommandEnabled(IDC_DEV_TOOLS_INSPECT,
                                           dev_tools_enabled);
    command_updater_->UpdateCommandEnabled(IDC_DEV_TOOLS_TOGGLE,
                                           dev_tools_enabled);
    command_updater_->UpdateCommandEnabled(IDC_VIEW_SOURCE, dev_tools_enabled);
#if BUILDFLAG(IS_MAC)
    command_updater_->UpdateCommandEnabled(IDC_TOGGLE_JAVASCRIPT_APPLE_EVENTS,
                                           dev_tools_enabled);
#endif
  } else {
    UpdateCommandsForDevTools();
  }
  command_updater_->UpdateCommandEnabled(IDC_TASK_MANAGER,
                                         CanOpenTaskManager());
  command_updater_->UpdateCommandEnabled(IDC_TASK_MANAGER_APP_MENU,
                                         CanOpenTaskManager());
  command_updater_->UpdateCommandEnabled(IDC_TASK_MANAGER_SHORTCUT,
                                         CanOpenTaskManager());
  command_updater_->UpdateCommandEnabled(IDC_TASK_MANAGER_CONTEXT_MENU,
                                         CanOpenTaskManager());
  command_updater_->UpdateCommandEnabled(IDC_TASK_MANAGER_MAIN_MENU,
                                         CanOpenTaskManager());
  command_updater_->UpdateCommandEnabled(IDC_SHOW_HISTORY, true);
  command_updater_->UpdateCommandEnabled(IDC_SHOW_DOWNLOADS, true);
  command_updater_->UpdateCommandEnabled(IDC_FIND_AND_EDIT_MENU, true);
  command_updater_->UpdateCommandEnabled(IDC_SAVE_AND_SHARE_MENU, true);
  command_updater_->UpdateCommandEnabled(IDC_HELP_MENU, true);
  command_updater_->UpdateCommandEnabled(IDC_HELP_PAGE_VIA_KEYBOARD, true);
  command_updater_->UpdateCommandEnabled(IDC_HELP_PAGE_VIA_MENU, true);
  command_updater_->UpdateCommandEnabled(IDC_SHOW_BETA_FORUM, true);
  command_updater_->UpdateCommandEnabled(
      IDC_CHROME_ENTERPRISE_RELEASE_NOTES,
      base::FeatureList::IsEnabled(features::kEnterpriseReleaseNotes));
#if !BUILDFLAG(IS_CHROMEOS)
#endif  // BUILDFLAG(IS_CHROMEOS)

  command_updater_->UpdateCommandEnabled(IDC_CLEAR_BROWSING_DATA, true);

#if BUILDFLAG(IS_CHROMEOS)
  command_updater_->UpdateCommandEnabled(IDC_TAKE_SCREENSHOT, true);
#endif
  command_updater_->UpdateCommandEnabled(IDC_CARET_BROWSING_TOGGLE, true);
  // Navigation commands
  command_updater_->UpdateCommandEnabled(
      IDC_HOME,
      normal_window ||
          browser_->GetType() == BrowserWindowInterface::Type::TYPE_APP ||
          browser_->GetType() == BrowserWindowInterface::Type::TYPE_APP_POPUP);

  // Hosted app browser commands.
  command_updater_->UpdateCommandEnabled(IDC_COPY_URL, CanCopyUrl(browser_));

  // Tab management commands
  const bool supports_tabs =
      WindowFeatureController::From(browser_)->SupportsWindowFeature(
          WindowFeatureController::WindowFeature::kFeatureTabStrip);
  command_updater_->UpdateCommandEnabled(IDC_SELECT_NEXT_TAB, supports_tabs);
  command_updater_->UpdateCommandEnabled(IDC_SELECT_PREVIOUS_TAB,
                                         supports_tabs);
  command_updater_->UpdateCommandEnabled(IDC_CYCLE_TO_NEXT_TAB, supports_tabs);
  command_updater_->UpdateCommandEnabled(IDC_CYCLE_TO_PREV_TAB, supports_tabs);
  command_updater_->UpdateCommandEnabled(IDC_MOVE_TAB_NEXT, supports_tabs);
  command_updater_->UpdateCommandEnabled(IDC_MOVE_TAB_PREVIOUS, supports_tabs);
  command_updater_->UpdateCommandEnabled(IDC_SELECT_TAB_0, supports_tabs);
  command_updater_->UpdateCommandEnabled(IDC_SELECT_TAB_1, supports_tabs);
  command_updater_->UpdateCommandEnabled(IDC_SELECT_TAB_2, supports_tabs);
  command_updater_->UpdateCommandEnabled(IDC_SELECT_TAB_3, supports_tabs);
  command_updater_->UpdateCommandEnabled(IDC_SELECT_TAB_4, supports_tabs);
  command_updater_->UpdateCommandEnabled(IDC_SELECT_TAB_5, supports_tabs);
  command_updater_->UpdateCommandEnabled(IDC_SELECT_TAB_6, supports_tabs);
  command_updater_->UpdateCommandEnabled(IDC_SELECT_TAB_7, supports_tabs);
  command_updater_->UpdateCommandEnabled(IDC_SELECT_LAST_TAB, supports_tabs);
  command_updater_->UpdateCommandEnabled(IDC_NEW_TAB_TO_RIGHT, supports_tabs);
  command_updater_->UpdateCommandEnabled(IDC_TAB_SCROLL_BUTTONS_TOGGLE_PIN,
                                         supports_tabs);

  // These are always enabled; the menu determines their menu item visibility.
  command_updater_->UpdateCommandEnabled(IDC_UPGRADE_DIALOG, true);
  command_updater_->UpdateCommandEnabled(IDC_SET_BROWSER_AS_DEFAULT, true);

  // Safety Hub commands.
  command_updater_->UpdateCommandEnabled(IDC_OPEN_SAFETY_HUB, true);

  command_updater_->UpdateCommandEnabled(IDC_WINDOW_MUTE_SITE, normal_window);
  command_updater_->UpdateCommandEnabled(IDC_WINDOW_PIN_TAB, normal_window);
  command_updater_->UpdateCommandEnabled(IDC_WINDOW_CLOSE_TABS_TO_RIGHT,
                                         normal_window);
  command_updater_->UpdateCommandEnabled(IDC_WINDOW_CLOSE_OTHER_TABS,
                                         normal_window);

  const bool enable_tab_search_commands =
      browser_->GetType() == BrowserWindowInterface::Type::TYPE_NORMAL;
  command_updater_->UpdateCommandEnabled(IDC_TAB_SEARCH_TOGGLE_PIN,
                                         enable_tab_search_commands);

  if (base::FeatureList::IsEnabled(features::kUIDebugTools)) {
    command_updater_->UpdateCommandEnabled(IDC_DEBUG_TOGGLE_TABLET_MODE, true);
    command_updater_->UpdateCommandEnabled(IDC_DEBUG_PRINT_VIEW_TREE, true);
    command_updater_->UpdateCommandEnabled(IDC_DEBUG_PRINT_VIEW_TREE_DETAILS,
                                           true);
    command_updater_->UpdateCommandEnabled(IDC_DEBUG_PRINT_WINDOW_HIERARCHY,
                                           true);
    command_updater_->UpdateCommandEnabled(IDC_DEBUG_PRINT_LAYER_HIERARCHY,
                                           true);
  }


  if (browser_->GetType() == BrowserWindowInterface::Type::TYPE_NORMAL) {
    // Reading list commands.
    command_updater_->UpdateCommandEnabled(IDC_READING_LIST_MENU, true);
    command_updater_->UpdateCommandEnabled(IDC_READING_LIST_MENU_ADD_TAB, true);
    command_updater_->UpdateCommandEnabled(IDC_READING_LIST_MENU_SHOW_UI, true);
  }
  if (IsChromeLabsEnabled()) {
    command_updater_->UpdateCommandEnabled(IDC_SHOW_CHROME_LABS, true);
  }


  // Initialize other commands whose state changes based on various conditions.
  UpdateCommandsForFullscreenMode();
  UpdateCommandsForContentRestrictionState();
  UpdateCommandsForIncognitoAvailability();
  UpdateCommandsForExtensionsMenu();
  UpdateCommandsForTabKeyboardFocus(GetKeyboardFocusedTabIndex(browser_));
  UpdateCommandsForWebContentsFocus();
  UpdateCommandsForTabGroupFocusChanged();
}

// static
void BrowserCommandController::UpdateSharedCommandsForIncognitoAvailability(
    CommandUpdater* command_updater,
    Profile* profile) {
  command_updater->UpdateCommandEnabled(IDC_NEW_WINDOW, true);

  extensions::ExtensionRegistrar* extension_registrar =
      extensions::ExtensionRegistrar::Get(profile);
  const bool enable_extensions =
      extension_registrar && extension_registrar->extensions_enabled();

  command_updater->UpdateCommandEnabled(IDC_MANAGE_EXTENSIONS,
                                        enable_extensions);
  command_updater->UpdateCommandEnabled(IDC_SAFETY_HUB_MANAGE_EXTENSIONS,
                                        enable_extensions);

  command_updater->UpdateCommandEnabled(IDC_OPTIONS, true);
  command_updater->UpdateCommandEnabled(IDC_PERFORMANCE, true);
}

void BrowserCommandController::UpdateCommandsForIncognitoAvailability() {
#if BUILDFLAG(IS_CHROMEOS)
  if (is_locked_fullscreen_) {
    return;
  }
#endif

  UpdateSharedCommandsForIncognitoAvailability(command_updater_.get(),
                                               profile());

  if (!IsShowingMainUI()) {
    command_updater_->UpdateCommandEnabled(IDC_OPTIONS, false);
  }
}

void BrowserCommandController::UpdateCommandsForExtensionsMenu() {
  // TODO(crbug.com/41124423): Talk with isandrk@chromium.org about whether this
  // is necessary for the experiment or not.
#if BUILDFLAG(IS_CHROMEOS)
  if (is_locked_fullscreen_) {
    return;
  }
#endif

  command_updater_->UpdateCommandEnabled(
      IDC_EXTENSIONS_SUBMENU_MANAGE_EXTENSIONS,
      /*state=*/true);
  command_updater_->UpdateCommandEnabled(
      IDC_EXTENSIONS_SUBMENU_VISIT_CHROME_WEB_STORE, /*state=*/true);
  command_updater_->UpdateCommandEnabled(IDC_FIND_EXTENSIONS,
                                         /*state=*/true);
}

void BrowserCommandController::UpdateCommandsForTabState() {
  // Keep commands disabled when in locked fullscreen so users cannot exit this
  // mode. Only update navigation ones when the webapp is locked for OnTask
  // (only relevant for non-web browser scenarios).
  // TODO(b/365146870): Remove once we consolidate locked fullscreen with
  // OnTask.
#if BUILDFLAG(IS_CHROMEOS)
  bool skip_all_command_updates = is_locked_fullscreen_;
  if (ash::boca::OnTaskLockedController::From(browser_)
          ->is_locked_for_on_task()) {
    skip_all_command_updates = false;
  }
  if (skip_all_command_updates) {
    return;
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  content::WebContents* current_web_contents =
      browser_->tab_strip_model()->GetActiveWebContents();
  if (!current_web_contents) {  // May be NULL during tab restore.
    return;
  }

  // Navigation commands
  UpdateCommandAndActionEnabled(IDC_BACK, kActionBack,
                                ShouldEnableBackButton(browser_));
  command_updater_->UpdateCommandEnabled(IDC_FORWARD,
                                         ShouldEnableForwardButton(browser_));
  const bool can_reload = CanReload(browser_);
  command_updater_->UpdateCommandEnabled(IDC_RELOAD, can_reload);
  command_updater_->UpdateCommandEnabled(IDC_RELOAD_BYPASSING_CACHE,
                                         can_reload);
  command_updater_->UpdateCommandEnabled(IDC_RELOAD_CLEARING_CACHE, can_reload);
#if BUILDFLAG(IS_CHROMEOS)
  if (is_locked_fullscreen_) {
    // Skip other command updates.
    // NOTE: If new commands are being added, please add them after this
    // conditional and notify the ChromeOS team by filing a bug under this
    // component -- b/?q=componentid:1389107.
    return;
  }
#endif

  // Window management commands
  bool is_app =
      browser_->GetType() == BrowserWindowInterface::Type::TYPE_APP ||
      browser_->GetType() == BrowserWindowInterface::Type::TYPE_APP_POPUP;
  bool is_normal =
      browser_->GetType() == BrowserWindowInterface::Type::TYPE_NORMAL;

  command_updater_->UpdateCommandEnabled(IDC_DUPLICATE_TAB,
                                         !is_app && CanDuplicateTab(browser_));
  command_updater_->UpdateCommandEnabled(IDC_WINDOW_MUTE_SITE, !is_app);
  command_updater_->UpdateCommandEnabled(IDC_WINDOW_PIN_TAB, is_normal);

  // Page-related commands
  command_updater_->UpdateCommandEnabled(IDC_VIEW_SOURCE,
                                         CanViewSource(browser_));

  command_updater_->UpdateCommandEnabled(IDC_OPEN_FILE, CanOpenFile(browser_));

  command_updater_->UpdateCommandEnabled(
      IDC_CREATE_SHORTCUT,
      shortcuts::CanCreateDesktopShortcut(current_web_contents));

  command_updater_->UpdateCommandEnabled(
      IDC_TOGGLE_REQUEST_TABLET_SITE,
      CanRequestTabletSite(current_web_contents));

  UpdateCommandsForContentRestrictionState();
  UpdateCommandsForFind();
  // Update the zoom commands when an active tab is selected.
  UpdateCommandsForZoomState();
  UpdateCommandsForTabKeyboardFocus(GetKeyboardFocusedTabIndex(browser_));
  if (!base::FeatureList::IsEnabled(features::kDevToolsShowPolicyDialog)) {
    UpdateCommandsForDevTools();
  } else {
    // Block the View Source command if DevTools are disabled.
    command_updater_->UpdateCommandEnabled(
        IDC_VIEW_SOURCE,
        DevToolsWindow::AllowDevToolsFor(
            profile(), browser_->tab_strip_model()->GetActiveWebContents()));
  }
}

void BrowserCommandController::UpdateCommandsForZoomState() {
  content::WebContents* contents =
      browser_->tab_strip_model()->GetActiveWebContents();
  if (!contents) {
    return;
  }
  command_updater_->UpdateCommandEnabled(IDC_ZOOM_PLUS, CanZoomIn(contents));
  command_updater_->UpdateCommandEnabled(IDC_ZOOM_NORMAL,
                                         CanResetZoom(contents));
  command_updater_->UpdateCommandEnabled(IDC_ZOOM_MINUS, CanZoomOut(contents));
}

void BrowserCommandController::UpdateCommandsForContentRestrictionState() {
  int restrictions = GetContentRestrictions(browser_);

  command_updater_->UpdateCommandEnabled(
      IDC_COPY, !(restrictions & CONTENT_RESTRICTION_COPY));
  command_updater_->UpdateCommandEnabled(
      IDC_CUT, !(restrictions & CONTENT_RESTRICTION_CUT));
  command_updater_->UpdateCommandEnabled(
      IDC_PASTE, !(restrictions & CONTENT_RESTRICTION_PASTE));
  UpdateSaveAsState();
  UpdatePrintingState();
}

// TODO(crbug.com/442892562): Remove this function once the feature is launched.
void BrowserCommandController::UpdateCommandsForDevTools() {
#if BUILDFLAG(IS_CHROMEOS)
  if (is_locked_fullscreen_) {
    return;
  }
#endif

  bool dev_tools_enabled = DevToolsWindow::AllowDevToolsFor(
      profile(), browser_->tab_strip_model()->GetActiveWebContents());
  command_updater_->UpdateCommandEnabled(IDC_DEV_TOOLS, dev_tools_enabled);
  command_updater_->UpdateCommandEnabled(IDC_DEV_TOOLS_CONSOLE,
                                         dev_tools_enabled);
  command_updater_->UpdateCommandEnabled(IDC_DEV_TOOLS_DEVICES,
                                         dev_tools_enabled);
  command_updater_->UpdateCommandEnabled(IDC_DEV_TOOLS_INSPECT,
                                         dev_tools_enabled);
  command_updater_->UpdateCommandEnabled(IDC_DEV_TOOLS_TOGGLE,
                                         dev_tools_enabled);
  command_updater_->UpdateCommandEnabled(IDC_VIEW_SOURCE, dev_tools_enabled);
#if BUILDFLAG(IS_MAC)
  command_updater_->UpdateCommandEnabled(IDC_TOGGLE_JAVASCRIPT_APPLE_EVENTS,
                                         dev_tools_enabled);
#endif
}



void BrowserCommandController::UpdateCommandsForFileSelectionDialogs() {
#if BUILDFLAG(IS_CHROMEOS)
  if (is_locked_fullscreen_) {
    return;
  }
#endif

  UpdateSaveAsState();
  command_updater_->UpdateCommandEnabled(IDC_OPEN_FILE, CanOpenFile(browser_));
}

void BrowserCommandController::UpdateCommandsForFullscreenMode() {
#if BUILDFLAG(IS_CHROMEOS)
  if (is_locked_fullscreen_) {
    return;
  }
#endif

  const bool is_fullscreen = window() && window()->IsFullscreen();
  const bool show_main_ui = IsShowingMainUI();
  const bool show_location_bar = IsShowingLocationBar();

  const bool main_not_fullscreen = show_main_ui && !is_fullscreen;

  // Navigation commands
  command_updater_->UpdateCommandEnabled(IDC_OPEN_CURRENT_URL, show_main_ui);

  // Window management commands
  command_updater_->UpdateCommandEnabled(
      IDC_SHOW_AS_TAB,
      browser_->GetType() != BrowserWindowInterface::Type::TYPE_NORMAL &&
          !is_fullscreen &&
          browser_->GetType() != BrowserWindowInterface::Type::TYPE_DEVTOOLS &&
          browser_->GetType() !=
              BrowserWindowInterface::Type::TYPE_PICTURE_IN_PICTURE);

  // Focus various bits of UI
  command_updater_->UpdateCommandEnabled(IDC_FOCUS_TOOLBAR, show_main_ui);
  command_updater_->UpdateCommandEnabled(IDC_FOCUS_LOCATION, show_location_bar);
  command_updater_->UpdateCommandEnabled(IDC_FOCUS_SEARCH, show_main_ui);
  command_updater_->UpdateCommandEnabled(IDC_FOCUS_MENU_BAR,
                                         main_not_fullscreen);
  command_updater_->UpdateCommandEnabled(IDC_FOCUS_NEXT_PANE,
                                         main_not_fullscreen);
  command_updater_->UpdateCommandEnabled(IDC_FOCUS_PREVIOUS_PANE,
                                         main_not_fullscreen);
  command_updater_->UpdateCommandEnabled(IDC_FOCUS_WEB_CONTENTS_PANE,
                                         main_not_fullscreen);
  command_updater_->UpdateCommandEnabled(
      IDC_FOCUS_INACTIVE_POPUP_FOR_ACCESSIBILITY, main_not_fullscreen);

  // Show various bits of UI
  command_updater_->UpdateCommandEnabled(IDC_DEVELOPER_MENU, show_main_ui);

  command_updater_->UpdateCommandEnabled(IDC_EDIT_SEARCH_ENGINES, show_main_ui);
  command_updater_->UpdateCommandEnabled(IDC_ABOUT, show_main_ui);
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  command_updater_->UpdateCommandEnabled(IDC_CHROME_TIPS, show_main_ui);
  command_updater_->UpdateCommandEnabled(IDC_CHROME_WHATS_NEW, show_main_ui);
#endif
  command_updater_->UpdateCommandEnabled(IDC_SHOW_APP_MENU, show_main_ui);
  command_updater_->UpdateCommandEnabled(IDC_SHOW_MANAGEMENT_PAGE, true);

  if (base::debug::IsProfilingSupported()) {
    command_updater_->UpdateCommandEnabled(IDC_PROFILING_ENABLED, show_main_ui);
  }

#if !BUILDFLAG(IS_MAC)
  // Disable toggling into fullscreen mode if disallowed by pref.
  const bool fullscreen_enabled =
      is_fullscreen ||
      profile()->GetPrefs()->GetBoolean(prefs::kFullscreenAllowed);
#else
  const bool fullscreen_enabled = true;
#endif

  command_updater_->UpdateCommandEnabled(IDC_FULLSCREEN, fullscreen_enabled);
  command_updater_->UpdateCommandEnabled(IDC_TOGGLE_FULLSCREEN_TOOLBAR,
                                         fullscreen_enabled);

  UpdateCommandsForIncognitoAvailability();
  UpdateCommandsForHostedAppAvailability();
}

void BrowserCommandController::UpdateCommandsForHostedAppAvailability() {
  bool has_toolbar =
      browser_->GetType() == BrowserWindowInterface::Type::TYPE_NORMAL;
  if (BrowserWindowFullscreenController::From(browser_)
          ->ShouldHideUIForFullscreen()) {
    has_toolbar = false;
  }
  command_updater_->UpdateCommandEnabled(IDC_FOCUS_TOOLBAR, has_toolbar);
  command_updater_->UpdateCommandEnabled(IDC_FOCUS_NEXT_PANE, has_toolbar);
  command_updater_->UpdateCommandEnabled(IDC_FOCUS_PREVIOUS_PANE, has_toolbar);
  command_updater_->UpdateCommandEnabled(IDC_SHOW_APP_MENU, has_toolbar);
}

#if BUILDFLAG(IS_CHROMEOS)
namespace {

#if DCHECK_IS_ON()
// Makes sure that all commands that are not allowlisted are disabled. DCHECKs
// otherwise. Compiled only in debug mode.
void NonAllowlistedCommandsAreDisabled(const CommandUpdater* command_updater) {
  constexpr int kAllowlistedIds[] = {IDC_CUT, IDC_COPY, IDC_PASTE};

  // Go through all the command ids, skip the allowlisted ones.
  for (int id : command_updater->GetAllIds()) {
    if (std::ranges::contains(kAllowlistedIds, id)) {
      continue;
    }
    DCHECK(!command_updater->IsCommandEnabled(id));
  }
}
#endif

}  // namespace

void BrowserCommandController::UpdateCommandsForLockedFullscreenMode() {
  bool is_locked_fullscreen =
      platform_util::IsBrowserLockedFullscreen(browser_);
  // Sanity check to make sure this function is called only on state change.
  DCHECK_NE(is_locked_fullscreen, is_locked_fullscreen_);
  if (is_locked_fullscreen == is_locked_fullscreen_) {
    return;
  }
  is_locked_fullscreen_ = is_locked_fullscreen;

  if (is_locked_fullscreen_) {
    command_updater_->DisableAllCommands();
    // Update the state of allowlisted commands:
    // IDC_CUT/IDC_COPY/IDC_PASTE,
    UpdateCommandsForContentRestrictionState();
    // TODO(crbug.com/41426009): Re-enable Find and Zoom in locked fullscreen.
    // All other commands will be disabled (there is an early return in their
    // corresponding UpdateCommandsFor* functions).
#if DCHECK_IS_ON()
    NonAllowlistedCommandsAreDisabled(command_updater_.get());
#endif
    // Enable commands that allow users to switch between tabs and find content
    // within a webpage if the webapp is locked for OnTask
    // (only relevant for non-web browser scenarios).
    if (ash::boca::OnTaskLockedController::From(browser_)
            ->is_locked_for_on_task()) {
      UpdateTabSwitchingCommandState();
      UpdateCommandsForFind();
    }
  } else {
    // Do an init call to re-initialize command state after the
    // DisableAllCommands.
    InitCommandState();
  }
}

void BrowserCommandController::UpdateTabSwitchingCommandState() {
  command_updater_->UpdateCommandEnabled(IDC_SELECT_NEXT_TAB,
                                         is_tab_switching_enabled_);
  command_updater_->UpdateCommandEnabled(IDC_SELECT_PREVIOUS_TAB,
                                         is_tab_switching_enabled_);
  command_updater_->UpdateCommandEnabled(IDC_CYCLE_TO_NEXT_TAB,
                                         is_tab_switching_enabled_);
  command_updater_->UpdateCommandEnabled(IDC_CYCLE_TO_PREV_TAB,
                                         is_tab_switching_enabled_);
  command_updater_->UpdateCommandEnabled(IDC_SELECT_TAB_0,
                                         is_tab_switching_enabled_);
  command_updater_->UpdateCommandEnabled(IDC_SELECT_TAB_1,
                                         is_tab_switching_enabled_);
  command_updater_->UpdateCommandEnabled(IDC_SELECT_TAB_2,
                                         is_tab_switching_enabled_);
  command_updater_->UpdateCommandEnabled(IDC_SELECT_TAB_3,
                                         is_tab_switching_enabled_);
  command_updater_->UpdateCommandEnabled(IDC_SELECT_TAB_4,
                                         is_tab_switching_enabled_);
  command_updater_->UpdateCommandEnabled(IDC_SELECT_TAB_5,
                                         is_tab_switching_enabled_);
  command_updater_->UpdateCommandEnabled(IDC_SELECT_TAB_6,
                                         is_tab_switching_enabled_);
  command_updater_->UpdateCommandEnabled(IDC_SELECT_TAB_7,
                                         is_tab_switching_enabled_);
  command_updater_->UpdateCommandEnabled(IDC_SELECT_LAST_TAB,
                                         is_tab_switching_enabled_);
}

void BrowserCommandController::SetTabSwitchCommandsEnabled(bool enabled) {
  is_tab_switching_enabled_ = enabled;
  UpdateTabSwitchingCommandState();
}
#endif  // BUILDFLAG(IS_CHROMEOS)

void BrowserCommandController::UpdatePrintingState() {
#if BUILDFLAG(IS_CHROMEOS)
  if (is_locked_fullscreen_) {
    return;
  }
#endif

  UpdateCommandAndActionEnabled(IDC_PRINT, kActionPrint, CanPrint(browser_));
#if BUILDFLAG(ENABLE_PRINTING)
  command_updater_->UpdateCommandEnabled(IDC_BASIC_PRINT,
                                         CanBasicPrint(browser_));
#endif
}


void BrowserCommandController::UpdateSaveAsState() {
#if BUILDFLAG(IS_CHROMEOS)
  if (is_locked_fullscreen_) {
    return;
  }
#endif

  command_updater_->UpdateCommandEnabled(IDC_SAVE_PAGE, CanSavePage(browser_));
}

void BrowserCommandController::UpdateReloadStopState(bool is_loading,
                                                     bool force) {
  // Skip command updates when in locked fullscreen mode unless the instance is
  // locked for OnTask (only relevant for non-web browser scenarios).
  // TODO(crbug.com/365146870): Remove once we consolidate locked fullscreen
  // with OnTask.
#if BUILDFLAG(IS_CHROMEOS)
  bool should_skip_command_updates = is_locked_fullscreen_;
  if (ash::boca::OnTaskLockedController::From(browser_)
          ->is_locked_for_on_task()) {
    should_skip_command_updates = false;
  }
  if (should_skip_command_updates) {
    return;
  }
#endif

  window()->UpdateReloadStopState(is_loading, force);
  command_updater_->UpdateCommandEnabled(IDC_STOP, is_loading);
  UpdateCloseFindOrStop();
}

void BrowserCommandController::UpdateTabRestoreCommandState() {
#if BUILDFLAG(IS_CHROMEOS)
  if (is_locked_fullscreen_) {
    return;
  }
#endif

  sessions::TabRestoreService* tab_restore_service =
      TabRestoreServiceFactory::GetForProfile(profile());
  // The command is enabled if the service hasn't loaded yet to trigger loading.
  // The command is updated once the load completes.
  command_updater_->UpdateCommandEnabled(
      IDC_RESTORE_TAB,
      tab_restore_service && (!tab_restore_service->IsLoaded() ||
                              !tab_restore_service->entries().empty()));
}

void BrowserCommandController::UpdateCommandsForFind() {
  TabStripModel* model = browser_->tab_strip_model();
  int active_index = model->active_index();
  bool is_actor_overlay_visible = false;

  // If the actor overlay is visible, we disable find and close it if it's open.
  if (base::FeatureList::IsEnabled(features::kGlicActorUi) &&
      features::kGlicActorUiOverlay.Get()) {
    if (BrowserView* browser_view =
            BrowserView::GetBrowserViewForBrowser(browser_)) {
      if (auto* active_container =
              browser_view->GetActiveContentsContainerView()) {
        if (active_container->actor_overlay_web_view()->GetVisible()) {
          is_actor_overlay_visible = true;
          if (CanCloseFind(browser_)) {
            CloseFind(browser_);
          }
        }
      }
    }
  }

  bool enabled =
      active_index != TabStripModel::kNoTab &&
      !model->IsTabBlocked(active_index) &&
      browser_->GetType() != BrowserWindowInterface::Type::TYPE_DEVTOOLS &&
      !is_actor_overlay_visible;

  command_updater_->UpdateCommandEnabled(IDC_FIND, enabled);
  command_updater_->UpdateCommandEnabled(IDC_FIND_NEXT, enabled);
  command_updater_->UpdateCommandEnabled(IDC_FIND_PREVIOUS, enabled);
}

void BrowserCommandController::UpdateCloseFindOrStop() {
  bool enabled = CanCloseFind(browser_) || IsCommandEnabled(IDC_STOP);
  command_updater_->UpdateCommandEnabled(IDC_CLOSE_FIND_OR_STOP, enabled);
}


void BrowserCommandController::UpdateCommandsForTabKeyboardFocus(
    std::optional<int> target_index) {
  command_updater_->UpdateCommandEnabled(
      IDC_DUPLICATE_TARGET_TAB,
      browser_->GetType() != BrowserWindowInterface::Type::TYPE_APP &&
          browser_->GetType() != BrowserWindowInterface::Type::TYPE_APP_POPUP &&
          target_index.has_value() &&
          CanDuplicateTabAt(browser_, *target_index));
  const bool normal_window =
      browser_->GetType() == BrowserWindowInterface::Type::TYPE_NORMAL;
  command_updater_->UpdateCommandEnabled(
      IDC_MUTE_TARGET_SITE, normal_window && target_index.has_value());
  command_updater_->UpdateCommandEnabled(
      IDC_PIN_TARGET_TAB, normal_window && target_index.has_value());
}

void BrowserCommandController::UpdateCommandsForWebContentsFocus() {
#if BUILDFLAG(IS_MAC)
  // On Mac, toggling caret browsing changes whether it's enabled or not
  // based on web contents focus.
  command_updater_->UpdateCommandEnabled(IDC_CARET_BROWSING_TOGGLE,
                                         CanToggleCaretBrowsing(browser_));
#endif  // BUILDFLAG(IS_MAC)
}

void BrowserCommandController::UpdateCommandsForTabStripStateChanged() {
#if BUILDFLAG(IS_CHROMEOS)
  if (is_locked_fullscreen_) {
    // Keep tab management commands disabled when in locked fullscreen so users
    // cannot exit this mode. Only relevant for non-web browser scenarios.
    return;
  }
#endif

  int tab_index = browser_->tab_strip_model()->active_index();
  // No commands are updated if there is not yet any selected tab.
  if (tab_index == TabStripModel::kNoTab) {
    return;
  }
  command_updater_->UpdateCommandEnabled(IDC_CLOSE_TAB, true);
  command_updater_->UpdateCommandEnabled(IDC_WINDOW_CLOSE_TABS_TO_RIGHT,
                                         CanCloseTabsToRight(browser_));
  command_updater_->UpdateCommandEnabled(IDC_WINDOW_CLOSE_OTHER_TABS,
                                         CanCloseOtherTabs(browser_));
  command_updater_->UpdateCommandEnabled(IDC_MOVE_TAB_TO_NEW_WINDOW,
                                         CanMoveActiveTabToNewWindow(browser_));
  command_updater_->UpdateCommandEnabled(
      IDC_NEW_SPLIT_TAB,
      browser_->GetType() == BrowserWindowInterface::Type::TYPE_NORMAL);
  command_updater_->UpdateCommandEnabled(IDC_GROUP_UNGROUPED_TABS,
                                         CanGroupAllUngroupedTabs(browser_));
}


void BrowserCommandController::UpdateCommandsForTabGroupFocusChanged() {
  if (base::FeatureList::IsEnabled(features::kTabGroupsFocusing)) {
    const bool is_focused =
        browser_->tab_strip_model()->GetFocusedGroup().has_value();
    if (auto* const action = FindAction(kActionUnfocusTabGroup, browser_)) {
      action->SetVisible(is_focused);
    }
    if (auto* const action =
            FindAction(kActionToggleCollapseVertical, browser_)) {
      action->SetVisible(!is_focused);
    }
  }
}

void BrowserCommandController::UpdateCommandAndActionEnabled(
    int command_id,
    actions::ActionId action_id,
    bool enabled) {
  command_updater_->UpdateCommandEnabled(command_id, enabled);
  if (!base::FeatureList::IsEnabled(features::kUseActionsForBrowserCommands)) {
    if (auto* const action = FindAction(action_id, browser_)) {
      action->SetEnabled(enabled);
    }
  }
}

std::unique_ptr<CommandUpdater>
BrowserCommandController::CreateCommandUpdater() {
  if (base::FeatureList::IsEnabled(features::kUseActionsForBrowserCommands)) {
    return std::make_unique<CommandActionUpdater>(
        BrowserActions::From(browser_)->root_action_item());
  }
  return std::make_unique<CommandUpdaterImpl>(this);
}

BrowserWindow* BrowserCommandController::window() {
  return BrowserWindow::FromBrowser(browser_);
}

Profile* BrowserCommandController::profile() {
  return browser_->GetProfile();
}

}  // namespace chrome
