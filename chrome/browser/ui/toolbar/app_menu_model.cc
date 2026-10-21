// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/app_menu_model.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <optional>

#include "base/command_line.h"
#include "base/debug/debugging_buildflags.h"
#include "base/debug/profiler.h"
#include "base/functional/bind.h"
#include "base/i18n/number_formatting.h"
#include "base/i18n/rtl.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/enterprise/isolated_mode/isolated_mode_settings_service_factory.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/extensions/extension_ui_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/global_error/global_error.h"
#include "chrome/browser/ui/global_error/global_error_service.h"
#include "chrome/browser/ui/global_error/global_error_service_factory.h"
#include "chrome/browser/ui/immersive/immersive_mode_controller.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/managed_ui.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/tabs/recent_tabs_sub_menu_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_metrics.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"
#include "chrome/browser/ui/toolbar/app_menu_icon_controller.h"
#include "chrome/browser/ui/toolbar/chrome_labs/chrome_labs_prefs.h"
#include "chrome/browser/ui/toolbar/chrome_labs/chrome_labs_utils.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/upgrade_detector/upgrade_detector.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/dom_distiller/content/browser/distillable_page_utils.h"
#include "components/dom_distiller/content/browser/uma_helper.h"
#include "components/dom_distiller/core/dom_distiller_features.h"
#include "components/dom_distiller/core/url_utils.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/prefs/pref_member.h"
#include "components/prefs/pref_service.h"
#include "components/profile_metrics/browser_profile_type.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/profiling.h"
#include "content/public/common/url_constants.h"
#include "extensions/common/extension_features.h"
#include "media/base/media_switches.h"
#include "ui/base/accelerators/menu_label_accelerator_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/button_menu_item_model.h"
#include "ui/base/models/image_model.h"
#include "ui/base/models/menu_separator_types.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/window_open_disposition.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_elider.h"
#include "ui/menus/simple_menu_model.h"

#if BUILDFLAG(GOOGLE_CHROME_BRANDING) || BUILDFLAG(IS_CHROMEOS)
#include "base/feature_list.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/policy/system_features_disable_list_policy_handler.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "ui/display/screen.h"
#else
#endif

#if BUILDFLAG(IS_WIN)
#include "base/time/time.h"
#include "base/win/shortcut.h"
#include "base/win/windows_version.h"
#include "content/public/browser/gpu_data_manager.h"
#endif

using base::UserMetricsAction;
using content::WebContents;

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(AppMenuModel, kProfileMenuItem);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(AppMenuModel, kBookmarksMenuItem);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(AppMenuModel, kTabGroupsMenuItem);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(AppMenuModel, kDownloadsMenuItem);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(AppMenuModel, kHistoryMenuItem);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(AppMenuModel, kExtensionsMenuItem);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(AppMenuModel, kClearBrowsingDataMenuItem);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(AppMenuModel, kMoreToolsMenuItem);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(AppMenuModel, kIncognitoMenuItem);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(AppMenuModel,
                                      kPasswordAndAutofillMenuItem);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(AppMenuModel, kPasswordManagerMenuItem);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(AppMenuModel, kContactInfoMenuItem);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(AppMenuModel, kIdentityDocsMenuItem);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(AppMenuModel, kTravelMenuItem);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(AppMenuModel, kShowLensOverlay);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(AppMenuModel, kSaveAndShareMenuItem);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(AppMenuModel, kCastTitleItem);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(AppMenuModel, kInstallAppItem);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(AppMenuModel, kCreateShortcutItem);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(AppMenuModel,
                                      kSetBrowserAsDefaultMenuItem);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(AppMenuModel, kHelpMenuItem);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(ToolsMenuModel, kPerformanceMenuItem);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(ToolsMenuModel, kChromeLabsMenuItem);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(ExtensionsMenuModel,
                                      kManageExtensionsMenuItem);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(ExtensionsMenuModel,
                                      kVisitChromeWebStoreMenuItem);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(HelpMenuModel, kReportUnsafeSiteMenuItem);

namespace {

////////////////////////////////////////////////////////////////////////////////
// AddItemWithStringIdAndVectorIcon
void AddItemWithStringIdAndVectorIcon(ui::SimpleMenuModel* model,
                                      int command_id,
                                      int string_id,
                                      const gfx::VectorIcon& vector_icon) {
  return model->AddItemWithStringIdAndIcon(
      command_id, string_id,
      ui::ImageModel::FromVectorIcon(vector_icon, ui::kColorMenuIcon,
                                     ui::SimpleMenuModel::kDefaultIconSize));
}

////////////////////////////////////////////////////////////////////////////////
// AddSubMenuWithStringIdAndVectorIcon
void AddSubMenuWithStringIdAndVectorIcon(ui::SimpleMenuModel* model,
                                         int command_id,
                                         int string_id,
                                         ui::MenuModel* sub_menu,
                                         const gfx::VectorIcon& vector_icon) {
  return model->AddSubMenuWithStringIdAndIcon(
      command_id, string_id, sub_menu,
      ui::ImageModel::FromVectorIcon(vector_icon, ui::kColorMenuIcon,
                                     ui::SimpleMenuModel::kDefaultIconSize));
}


class FindAndEditSubMenuModel : public ui::SimpleMenuModel {
 public:
  explicit FindAndEditSubMenuModel(ui::SimpleMenuModel::Delegate* delegate);
  FindAndEditSubMenuModel(const FindAndEditSubMenuModel&) = delete;
  FindAndEditSubMenuModel& operator=(const FindAndEditSubMenuModel&) = delete;
  ~FindAndEditSubMenuModel() override = default;
};

FindAndEditSubMenuModel::FindAndEditSubMenuModel(
    ui::SimpleMenuModel::Delegate* delegate)
    : SimpleMenuModel(delegate) {
  AddItemWithStringIdAndVectorIcon(
      this, IDC_FIND, IDS_FIND,
      features::IsRoundedIconsEnabled() ? kFindInPageIcon : kSearchMenuOldIcon);
  AddSeparator(ui::NORMAL_SEPARATOR);
  AddItemWithStringIdAndVectorIcon(
      this, IDC_CUT, IDS_CUT,
      features::IsRoundedIconsEnabled() ? kContentCutIcon : kCutMenuOldIcon);
  AddItemWithStringIdAndVectorIcon(this, IDC_COPY, IDS_COPY,
                                   features::IsRoundedIconsEnabled()
                                       ? vector_icons::kContentCopyIcon
                                       : kCopyMenuOldIcon);
  AddItemWithStringIdAndVectorIcon(this, IDC_PASTE, IDS_PASTE,
                                   features::IsRoundedIconsEnabled()
                                       ? kContentPasteIcon
                                       : kPasteMenuOldIcon);
}

class SaveAndShareSubMenuModel : public ui::SimpleMenuModel {
 public:
  SaveAndShareSubMenuModel(ui::SimpleMenuModel::Delegate* delegate,
                           BrowserWindowInterface* browser);
  SaveAndShareSubMenuModel(const SaveAndShareSubMenuModel&) = delete;
  SaveAndShareSubMenuModel& operator=(const SaveAndShareSubMenuModel&) = delete;
  ~SaveAndShareSubMenuModel() override = default;
};

SaveAndShareSubMenuModel::SaveAndShareSubMenuModel(
    ui::SimpleMenuModel::Delegate* delegate,
    BrowserWindowInterface* /*browser*/)
    : SimpleMenuModel(delegate) {
  AddTitle(l10n_util::GetStringUTF16(IDS_SAVE_AND_SHARE_MENU_SAVE));
  AddItemWithStringIdAndVectorIcon(this, IDC_SAVE_PAGE, IDS_SAVE_PAGE,
                                   features::IsRoundedIconsEnabled()
                                       ? kFileSaveIcon
                                       : kFileSaveChromeRefreshOldIcon);
  AddSeparator(ui::NORMAL_SEPARATOR);
  AddTitle(l10n_util::GetStringUTF16(IDS_SAVE_AND_SHARE_MENU_SHARE));
  AddItemWithStringIdAndVectorIcon(
      this, IDC_COPY_URL, IDS_APP_MENU_COPY_LINK,
      features::IsRoundedIconsEnabled() ? vector_icons::kLinkIcon
                                        : kLinkChromeRefreshOldIcon);
}

bool ArePromotionsEnabled() {
  PrefService* local_state = g_browser_process->local_state();
  return local_state && local_state->GetBoolean(prefs::kPromotionsEnabled);
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// SetCommandIcon
void SetCommandIcon(ui::SimpleMenuModel* model,
                    int command_id,
                    const gfx::VectorIcon& vector_icon) {
  auto index = model->GetIndexOfCommandId(command_id);
  if (index) {
    model->SetIcon(index.value(), ui::ImageModel::FromVectorIcon(
                                      vector_icon, ui::kColorMenuIcon,
                                      ui::SimpleMenuModel::kDefaultIconSize));
  }
}

////////////////////////////////////////////////////////////////////////////////
// LogWrenchMenuAction
void LogWrenchMenuAction(AppMenuAction action_id) {
  base::UmaHistogramEnumeration("WrenchMenu.MenuAction", action_id,
                                LIMIT_MENU_ACTION);
}

////////////////////////////////////////////////////////////////////////////////
// HelpMenuModel
// Only used in branded builds.

HelpMenuModel::HelpMenuModel(ui::SimpleMenuModel::Delegate* delegate,
                             BrowserWindowInterface* browser)
    : SimpleMenuModel(delegate) {
  Build(browser);
}

HelpMenuModel::~HelpMenuModel() = default;

void HelpMenuModel::Build(BrowserWindowInterface* browser) {
#if BUILDFLAG(IS_CHROMEOS) && defined(OFFICIAL_BUILD)
  int help_string_id = IDS_GET_HELP;
#else
  int help_string_id = IDS_HELP_PAGE;
#endif
  AddItemWithStringIdAndVectorIcon(this, IDC_ABOUT, IDS_ABOUT,
                                   features::IsRoundedIconsEnabled()
                                       ? vector_icons::kInfoIcon
                                       : vector_icons::kInfoRefreshOldIcon);
  AddItemWithStringId(IDC_HELP_PAGE_VIA_MENU, help_string_id);
  if (browser_defaults::kShowHelpMenuItemIcon) {
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    SetIconForCommandId(
        IDC_HELP_PAGE_VIA_MENU,
        ui::ImageModel::FromImage(rb.GetNativeImageNamed(IDR_HELP_MENU)));
  } else {
    SetCommandIcon(
        this, IDC_HELP_PAGE_VIA_MENU,
        features::IsRoundedIconsEnabled() ? kHelpCustomIcon : kHelpMenuOldIcon);
  }
}

////////////////////////////////////////////////////////////////////////////////
// ToolsMenuModel

ToolsMenuModel::ToolsMenuModel(ui::SimpleMenuModel::Delegate* delegate,
                               BrowserWindowInterface* browser)
    : SimpleMenuModel(delegate) {
  Build(browser);
}

ToolsMenuModel::~ToolsMenuModel() = default;

// More tools submenu is constructed as follows:
// - Page specific actions overflow (save page, adding to desktop).
// - Browser / OS level tools (extensions, task manager).
// - Reading mode.
// - Developer tools.
// - Option to enable profiling.
void ToolsMenuModel::Build(BrowserWindowInterface* browser) {
  AddItemWithStringIdAndVectorIcon(
      this, IDC_NAME_WINDOW, IDS_NAME_WINDOW,
      features::IsRoundedIconsEnabled() ? kWebAssetIcon : kNameWindowOldIcon);

  if (auto* controller = tabs::VerticalTabStripStateController::From(browser)) {
    if (controller->ShouldDisplayVerticalTabs()) {
      AddItemWithStringIdAndVectorIcon(
          this, IDC_TOGGLE_VERTICAL_TABS, IDS_SWITCH_TO_HORIZONTAL_TAB,
          features::IsRoundedIconsEnabled() ? kToolbarIcon : kToolbarOldIcon);
    } else {
      AddItemWithStringIdAndVectorIcon(
          this, IDC_TOGGLE_VERTICAL_TABS, IDS_SWITCH_TO_VERTICAL_TAB,
          base::i18n::IsRTL() ? features::IsRoundedIconsEnabled()
                                    ? kDockToLeftIcon
                                    : kDockToRightOldIcon
          : features::IsRoundedIconsEnabled() ? kDockToRightIcon
                                              : kDockToLeftOldIcon);
    }
  }

  AddSeparator(ui::NORMAL_SEPARATOR);

  AddItemWithStringIdAndVectorIcon(
      this, IDC_PERFORMANCE, IDS_SHOW_PERFORMANCE,
      features::IsRoundedIconsEnabled() ? kSpeedIcon : kPerformanceOldIcon);
  SetElementIdentifierAt(GetIndexOfCommandId(IDC_PERFORMANCE).value(),
                         kPerformanceMenuItem);

  if (chrome::CanOpenTaskManager()) {
    AddItemWithStringIdAndVectorIcon(this, IDC_TASK_MANAGER_APP_MENU,
                                     IDS_TASK_MANAGER,
                                     vector_icons::kTableChartIcon);
  }
#if BUILDFLAG(IS_CHROMEOS)
  AddItemWithStringId(IDC_TAKE_SCREENSHOT, IDS_TAKE_SCREENSHOT);
#endif
  AddSeparator(ui::NORMAL_SEPARATOR);
  AddItemWithStringIdAndVectorIcon(
      this, IDC_DEV_TOOLS, IDS_DEV_TOOLS,
      features::IsRoundedIconsEnabled() ? kCodeIcon : kDeveloperToolsOldIcon);

  if (base::debug::IsProfilingSupported()) {
    AddSeparator(ui::NORMAL_SEPARATOR);
    AddCheckItemWithStringId(IDC_PROFILING_ENABLED, IDS_PROFILING_ENABLED);
  }
  if (IsChromeLabsEnabled()) {
    auto* profile = browser->GetProfile();
    UpdateChromeLabsNewBadgePrefs(profile);
    if (ShouldShowChromeLabsUI(profile)) {
      BooleanPrefMember show_chrome_labs_item;
      show_chrome_labs_item.Init(
          chrome_labs_prefs::kBrowserLabsEnabledEnterprisePolicy,
          profile->GetPrefs());
      if (show_chrome_labs_item.GetValue()) {
        AddSeparator(ui::NORMAL_SEPARATOR);
        AddItemWithStringIdAndVectorIcon(
            this, IDC_SHOW_CHROME_LABS, IDS_CHROMELABS,
            features::IsRoundedIconsEnabled() ? vector_icons::kScienceIcon
                                              : vector_icons::kScienceOldIcon);
        SetElementIdentifierAt(
            GetIndexOfCommandId(IDC_SHOW_CHROME_LABS).value(),
            kChromeLabsMenuItem);
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// ExtensionsMenuModel

ExtensionsMenuModel::ExtensionsMenuModel(
    ui::SimpleMenuModel::Delegate* delegate,
    BrowserWindowInterface* browser)
    : SimpleMenuModel(delegate) {
  Build(browser);
}

ExtensionsMenuModel::~ExtensionsMenuModel() = default;

// Extensions (sub)menu is constructed as follows:
// - An overflow with two items:
//   - An item to manage extensions at chrome://extensions
//   - An item to visit the Chrome Web Store
void ExtensionsMenuModel::Build(BrowserWindowInterface* browser) {
  AddItemWithStringIdAndVectorIcon(
      this, IDC_EXTENSIONS_SUBMENU_MANAGE_EXTENSIONS,
      IDS_EXTENSIONS_SUBMENU_MANAGE_EXTENSIONS_ITEM,
      features::IsRoundedIconsEnabled()
          ? vector_icons::kChromeExtensionIcon
          : vector_icons::kExtensionChromeRefreshOldIcon);
  SetElementIdentifierAt(
      GetIndexOfCommandId(IDC_EXTENSIONS_SUBMENU_MANAGE_EXTENSIONS).value(),
      kManageExtensionsMenuItem);
  AddItemWithStringId(IDC_EXTENSIONS_SUBMENU_VISIT_CHROME_WEB_STORE,
                      IDS_EXTENSIONS_SUBMENU_CHROME_WEBSTORE_ITEM);
  SetElementIdentifierAt(
      GetIndexOfCommandId(IDC_EXTENSIONS_SUBMENU_VISIT_CHROME_WEB_STORE)
          .value(),
      kVisitChromeWebStoreMenuItem);
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  SetCommandIcon(this, IDC_EXTENSIONS_SUBMENU_VISIT_CHROME_WEB_STORE,
                 vector_icons::kGoogleChromeWebstoreIcon);
#endif
}

////////////////////////////////////////////////////////////////////////////////
// AppMenuModel

// static
AlertMenuItem AppMenuModel::GetAlertItemForRunningTutorial(
    BrowserWindowInterface* browser) {
  if (!browser || !browser->GetWindow()) {
    return AlertMenuItem::kNone;
  }
  return AlertMenuItem::kNone;
}

// static
std::u16string AppMenuModel::GetUpgradeDialogTitleText() {
  if (UpgradeDetector::GetInstance()->is_outdated_install() ||
      UpgradeDetector::GetInstance()->is_outdated_install_no_au()) {
    return l10n_util::GetStringUTF16(IDS_UPGRADE_BUBBLE_MENU_ITEM);
  }
#if BUILDFLAG(GOOGLE_CHROME_BRANDING) && \
    (BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX))
  return l10n_util::GetStringUTF16(IDS_RELAUNCH_TO_UPDATE_ALT);
#else
  return l10n_util::GetStringUTF16(IDS_RELAUNCH_TO_UPDATE);
#endif
}

// static
std::u16string AppMenuModel::GetUpgradeDialogSubstringText() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING) && \
    (BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX))
  if (!UpgradeDetector::GetInstance()->is_outdated_install() &&
      !UpgradeDetector::GetInstance()->is_outdated_install_no_au()) {
    return l10n_util::GetStringUTF16(IDS_RELAUNCH_TO_UPDATE_ALT_MINOR_TEXT);
  }
#endif
  return std::u16string();
}

AppMenuModel::AppMenuModel(ui::AcceleratorProvider* provider,
                           BrowserWindowInterface* browser,
                           AppMenuIconController* app_menu_icon_controller,
                           AlertMenuItem alert_item)
    : ui::SimpleMenuModel(this),
      uma_action_recorded_(false),
      provider_(provider),
      browser_(browser),
      app_menu_icon_controller_(app_menu_icon_controller) {
  DCHECK(browser_);
}

AppMenuModel::~AppMenuModel() = default;

void AppMenuModel::Init() {
  Build();

#if BUILDFLAG(IS_CHROMEOS)
  PrefService* const local_state = g_browser_process->local_state();
  if (local_state) {
    local_state_pref_change_registrar_.Init(local_state);
    local_state_pref_change_registrar_.Add(
        policy::policy_prefs::kSystemFeaturesDisableList,
        base::BindRepeating(&AppMenuModel::UpdateSettingsItemState,
                            base::Unretained(this)));
    UpdateSettingsItemState();
  }
#endif  // BUILDFLAG(IS_CHROMEOS)
}

bool AppMenuModel::DoesCommandIdDismissMenu(int command_id) const {
  return command_id != IDC_ZOOM_MINUS && command_id != IDC_ZOOM_PLUS;
}

void AppMenuModel::ExecuteCommand(int command_id, int event_flags) {
  GlobalError* error =
      GlobalErrorServiceFactory::GetForProfile(browser_->GetProfile())
          ->GetGlobalErrorByMenuItemCommandID(command_id);
  if (error) {
    error->ExecuteMenuItem(browser_);
    return;
  }

  LogMenuMetrics(command_id);

  chrome::ExecuteCommand(browser_, command_id);
}

void AppMenuModel::LogMenuMetrics(int command_id) {
  base::TimeDelta delta = timer_.Elapsed();

  switch (command_id) {
    case IDC_UPGRADE_DIALOG:
      LogMenuAction(MENU_ACTION_UPGRADE_DIALOG);
      break;
    case IDC_NEW_TAB:
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes("WrenchMenu.TimeToAction.NewTab", delta);
      }
      LogMenuAction(MENU_ACTION_NEW_TAB);
      break;
    case IDC_NEW_WINDOW:
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes("WrenchMenu.TimeToAction.NewWindow",
                                      delta);
      }
      LogMenuAction(MENU_ACTION_NEW_WINDOW);
      break;

    // Bookmarks sub menu.
    case IDC_SHOW_BOOKMARK_BAR:
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes("WrenchMenu.TimeToAction.ShowBookmarkBar",
                                      delta);
      }
      LogMenuAction(MENU_ACTION_SHOW_BOOKMARK_BAR);
      break;
    case IDC_SHOW_BOOKMARK_MANAGER:
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes("WrenchMenu.TimeToAction.ShowBookmarkMgr",
                                      delta);
      }
      LogMenuAction(MENU_ACTION_SHOW_BOOKMARK_MANAGER);
      break;
    case IDC_BOOKMARK_THIS_TAB:
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes("WrenchMenu.TimeToAction.BookmarkPage",
                                      delta);
      }
      LogMenuAction(MENU_ACTION_BOOKMARK_THIS_TAB);
      break;
    case IDC_BOOKMARK_ALL_TABS:
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes("WrenchMenu.TimeToAction.BookmarkAllTabs",
                                      delta);
      }
      LogMenuAction(MENU_ACTION_BOOKMARK_ALL_TABS);
      break;
    // Lens overlay.
    case IDC_CONTENT_CONTEXT_LENS_OVERLAY:
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes("WrenchMenu.TimeToAction.ShowLensOverlay",
                                      delta);
      }
      LogMenuAction(MENU_ACTION_SHOW_LENS_OVERLAY);
      break;
    // Extensions menu.
    case IDC_EXTENSIONS_SUBMENU_MANAGE_EXTENSIONS:
      // Logging the original histograms for experiment comparison purposes.
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes(
            "WrenchMenu.TimeToAction.ManageExtensions", delta);
      }
      LogMenuAction(MENU_ACTION_MANAGE_EXTENSIONS);
      break;
    case IDC_EXTENSIONS_SUBMENU_VISIT_CHROME_WEB_STORE:
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes(
            "WrenchMenu.TimeToAction.VisitChromeWebStore", delta);
      }
      LogMenuAction(MENU_ACTION_VISIT_CHROME_WEB_STORE);
      break;
    case IDC_FIND_EXTENSIONS:
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes("WrenchMenu.TimeToAction.FindExtensions",
                                      delta);
      }
      LogMenuAction(MENU_ACTION_FIND_EXTENSIONS);
      break;
    // Recent tabs menu.
    case IDC_RESTORE_TAB:
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes("WrenchMenu.TimeToAction.RestoreTab",
                                      delta);
      }
      LogMenuAction(MENU_ACTION_RESTORE_TAB);
      break;
    case IDC_OPEN_RECENT_TAB:
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes("WrenchMenu.TimeToAction.OpenRecentTab",
                                      delta);
      }
      LogMenuAction(MENU_ACTION_RECENT_TAB);
      break;
    case IDC_FIND:
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes("WrenchMenu.TimeToAction.Find", delta);
      }
      LogMenuAction(MENU_ACTION_FIND);
      break;
    case IDC_PRINT:
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes("WrenchMenu.TimeToAction.Print", delta);
      }
      LogMenuAction(MENU_ACTION_PRINT);
      break;
    case IDC_OPEN_GLIC:
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes("WrenchMenu.TimeToAction.OpenGlic",
                                      delta);
      }
      LogMenuAction(MENU_ACTION_OPEN_GLIC);
      break;

    case IDC_SHOW_TRANSLATE:
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes("WrenchMenu.TimeToAction.ShowTranslate",
                                      delta);
      }
      LogMenuAction(MENU_ACTION_SHOW_TRANSLATE);
      break;

    // Edit menu.
    case IDC_CUT:
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes("WrenchMenu.TimeToAction.Cut", delta);
      }
      LogMenuAction(MENU_ACTION_CUT);
      break;
    case IDC_COPY:
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes("WrenchMenu.TimeToAction.Copy", delta);
      }
      LogMenuAction(MENU_ACTION_COPY);
      break;
    case IDC_PASTE:
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes("WrenchMenu.TimeToAction.Paste", delta);
      }
      LogMenuAction(MENU_ACTION_PASTE);
      break;

    // Save and share menu.
    case IDC_SAVE_PAGE:
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes("WrenchMenu.TimeToAction.SavePage",
                                      delta);
      }
      LogMenuAction(MENU_ACTION_SAVE_PAGE);
      break;
    case IDC_INSTALL_PWA:
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes("WrenchMenu.TimeToAction.InstallPwa",
                                      delta);
      }
      LogMenuAction(MENU_ACTION_INSTALL_PWA);
      break;
    case IDC_OPEN_IN_PWA_WINDOW:
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes("WrenchMenu.TimeToAction.OpenInPwaWindow",
                                      delta);
      }
      LogMenuAction(MENU_ACTION_OPEN_IN_PWA_WINDOW);
      break;
    case IDC_CREATE_SHORTCUT:
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes("WrenchMenu.TimeToAction.CreateHostedApp",
                                      delta);
      }
      LogMenuAction(MENU_ACTION_CREATE_HOSTED_APP);
      break;
    case IDC_COPY_URL:
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes("WrenchMenu.TimeToAction.CopyUrl", delta);
      }
      LogMenuAction(MENU_ACTION_COPY_URL);
      break;
    case IDC_SEND_TAB_TO_SELF:
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes("WrenchMenu.TimeToAction.SendToDevices",
                                      delta);
      }
      LogMenuAction(MENU_ACTION_SEND_TO_DEVICES);
      break;
    case IDC_QRCODE_GENERATOR:
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes("WrenchMenu.TimeToAction.CreateQrCode",
                                      delta);
      }
      LogMenuAction(MENU_ACTION_CREATE_QR_CODE);
      break;
    case IDC_ROUTE_MEDIA:
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes("WrenchMenu.TimeToAction.Cast", delta);
      }
      LogMenuAction(MENU_ACTION_CAST);
      break;

    // Tools menu.
    case IDC_MANAGE_EXTENSIONS:
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes(
            "WrenchMenu.TimeToAction.ManageExtensions", delta);
      }
      LogMenuAction(MENU_ACTION_MANAGE_EXTENSIONS);
      break;
    case IDC_TASK_MANAGER:
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes("WrenchMenu.TimeToAction.TaskManager",
                                      delta);
      }
      LogMenuAction(MENU_ACTION_TASK_MANAGER);
      break;
    case IDC_CLEAR_BROWSING_DATA:
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes(
            "WrenchMenu.TimeToAction.ClearBrowsingData", delta);
      }
      LogMenuAction(MENU_ACTION_CLEAR_BROWSING_DATA);
      break;
    case IDC_VIEW_SOURCE:
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes("WrenchMenu.TimeToAction.ViewSource",
                                      delta);
      }
      LogMenuAction(MENU_ACTION_VIEW_SOURCE);
      break;
    case IDC_DEV_TOOLS:
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes("WrenchMenu.TimeToAction.DevTools",
                                      delta);
      }
      LogMenuAction(MENU_ACTION_DEV_TOOLS);
      break;
    case IDC_DEV_TOOLS_CONSOLE:
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes("WrenchMenu.TimeToAction.DevToolsConsole",
                                      delta);
      }
      LogMenuAction(MENU_ACTION_DEV_TOOLS_CONSOLE);
      break;
    case IDC_DEV_TOOLS_DEVICES:
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes("WrenchMenu.TimeToAction.DevToolsDevices",
                                      delta);
      }
      LogMenuAction(MENU_ACTION_DEV_TOOLS_DEVICES);
      break;
    case IDC_PROFILING_ENABLED:
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes(
            "WrenchMenu.TimeToAction.ProfilingEnabled", delta);
      }
      LogMenuAction(MENU_ACTION_PROFILING_ENABLED);
      break;
    case IDC_SHOW_CHROME_LABS:
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes("WrenchMenu.TimeToAction.ShowChromeLabs",
                                      delta);
      }
      LogMenuAction(MENU_ACTION_SHOW_CHROME_LABS);
      break;
    // Zoom menu
    case IDC_ZOOM_MINUS:
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes("WrenchMenu.TimeToAction.ZoomMinus",
                                      delta);
        LogMenuAction(MENU_ACTION_ZOOM_MINUS);
      }
      break;
    case IDC_ZOOM_PLUS:
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes("WrenchMenu.TimeToAction.ZoomPlus",
                                      delta);
        LogMenuAction(MENU_ACTION_ZOOM_PLUS);
      }
      break;
    case IDC_FULLSCREEN:
      base::RecordAction(UserMetricsAction("EnterFullScreenWithWrenchMenu"));

      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes("WrenchMenu.TimeToAction.EnterFullScreen",
                                      delta);
      }
      LogMenuAction(MENU_ACTION_FULLSCREEN);
      break;
    case IDC_SHOW_HISTORY:
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes("WrenchMenu.TimeToAction.ShowHistory",
                                      delta);
      }
      LogMenuAction(MENU_ACTION_SHOW_HISTORY);
      break;
    case IDC_SHOW_DOWNLOADS:
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes("WrenchMenu.TimeToAction.ShowDownloads",
                                      delta);
      }
      LogMenuAction(MENU_ACTION_SHOW_DOWNLOADS);
      base::UmaHistogramEnumeration(
          "Download.OpenDownloadsFromMenu.PerProfileType",
          profile_metrics::GetBrowserProfileType(browser_->GetProfile()));
      break;
    case IDC_OPTIONS:
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes("WrenchMenu.TimeToAction.Settings",
                                      delta);
      }
      LogMenuAction(MENU_ACTION_OPTIONS);
      base::UmaHistogramEnumeration(
          "Settings.OpenSettingsFromMenu.PerProfileType",
          profile_metrics::GetBrowserProfileType(browser_->GetProfile()));
      break;
    case IDC_ABOUT:
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes("WrenchMenu.TimeToAction.About", delta);
      }
      LogMenuAction(MENU_ACTION_ABOUT);
      break;
    // Help menu.
    case IDC_HELP_PAGE_VIA_MENU:
      base::RecordAction(UserMetricsAction("ShowHelpTabViaWrenchMenu"));

      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes("WrenchMenu.TimeToAction.HelpPage",
                                      delta);
      }
      LogMenuAction(MENU_ACTION_HELP_PAGE_VIA_MENU);
      break;
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    case IDC_SHOW_BETA_FORUM:
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes("WrenchMenu.TimeToAction.BetaForum",
                                      delta);
      }
      LogMenuAction(MENU_ACTION_BETA_FORUM);
      break;
    case IDC_FEEDBACK:
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes("WrenchMenu.TimeToAction.Feedback",
                                      delta);
      }
      LogMenuAction(MENU_ACTION_FEEDBACK);
      break;
    case IDC_CHROME_TIPS:
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes("WrenchMenu.TimeToAction.ChromeTips",
                                      delta);
      }
      LogMenuAction(MENU_ACTION_CHROME_TIPS);
      break;
    case IDC_CHROME_WHATS_NEW:
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes("WrenchMenu.TimeToAction.ChromeWhatsNew",
                                      delta);
      }
      LogMenuAction(MENU_ACTION_CHROME_WHATS_NEW);
      break;
#endif

    case IDC_TOGGLE_REQUEST_TABLET_SITE:
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes(
            "WrenchMenu.TimeToAction.RequestTabletSite", delta);
      }
      LogMenuAction(MENU_ACTION_TOGGLE_REQUEST_TABLET_SITE);
      break;
    case IDC_EXIT:
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes("WrenchMenu.TimeToAction.Exit", delta);
      }
      LogMenuAction(MENU_ACTION_EXIT);
      break;

    // Hosted App menu.
    case IDC_OPEN_IN_CHROME:
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes("WrenchMenu.TimeToAction.OpenInChrome",
                                      delta);
      }
      LogMenuAction(MENU_ACTION_OPEN_IN_CHROME);
      break;
    case IDC_WEB_APP_MENU_APP_INFO:
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes("WrenchMenu.TimeToAction.AppInfo", delta);
      }
      LogMenuAction(MENU_ACTION_APP_INFO);
      break;

    case IDC_PERFORMANCE:
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes(
            "WrenchMenu.TimeToAction.ShowPerformanceSettings", delta);
      }
      LogMenuAction(MENU_ACTION_SHOW_PERFORMANCE_SETTINGS);
      break;
    case IDC_SET_BROWSER_AS_DEFAULT:
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes(
            "WrenchMenu.TimeToAction.SetBrowserAsDefault", delta);
      }
      LogMenuAction(MENU_ACTION_SET_BROWSER_AS_DEFAULT);
      break;
    case IDC_OPEN_SAFETY_HUB:
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes(
            "WrenchMenu.TimeToAction.SafetyHubNotificationOpenSafetyHub",
            delta);
      }
      LogMenuAction(MENU_ACTION_SHOW_SAFETY_HUB);
      break;
    case IDC_SAFETY_HUB_MANAGE_EXTENSIONS:
      if (!uma_action_recorded_) {
        base::UmaHistogramMediumTimes(
            "WrenchMenu.TimeToAction.SafetyHubNotificationManageExtensions",
            delta);
      }
      LogMenuAction(MENU_ACTION_SAFETY_HUB_MANAGE_EXTENSIONS);
      break;
    case IDC_TOGGLE_VERTICAL_TABS:
      if (auto* controller =
              tabs::VerticalTabStripStateController::From(browser_)) {
        const bool is_vertical = !controller->ShouldDisplayVerticalTabs();
        tabs::RecordVerticalTabStripModeChanged(
            is_vertical, tabs::VerticalTabStripEntryPoint::kAppMenu);
      }
      break;
    default:
      break;
  }

  if (!uma_action_recorded_) {
    base::UmaHistogramMediumTimes("WrenchMenu.TimeToAction", delta);
    uma_action_recorded_ = true;
  }
}

bool AppMenuModel::IsCommandIdChecked(int command_id) const {
  if (command_id == IDC_PROFILING_ENABLED) {
    return content::Profiling::BeingProfiled();
  }
  if (command_id == IDC_TOGGLE_REQUEST_TABLET_SITE) {
    return chrome::IsRequestingTabletSite(browser_);
  }

  return false;
}

bool AppMenuModel::IsCommandIdEnabled(int command_id) const {
  GlobalError* error =
      GlobalErrorServiceFactory::GetForProfile(browser_->GetProfile())
          ->GetGlobalErrorByMenuItemCommandID(command_id);
  if (error) {
    return true;
  }

  return chrome::IsCommandEnabled(browser_, command_id);
}

bool AppMenuModel::IsCommandIdAlerted(int command_id) const {
  return false;
}

bool AppMenuModel::GetAcceleratorForCommandId(
    int command_id,
    ui::Accelerator* accelerator) const {
  return provider_->GetAcceleratorForCommandId(command_id, accelerator);
}

void AppMenuModel::LogMenuAction(AppMenuAction action_id) {
  LogWrenchMenuAction(action_id);
}

// Note: When adding new menu items please place under an appropriate section.
// Capitalization Policy (go/chrome-capitalization):
// In-browser 3-dot app menus use sentence case across all platforms, including
// macOS. Native macOS system menus (the main menu bar and right-click context
// menus) use Title Case separately.
// Menu is organised as follows:
// - Extension toolbar overflow.
// - Global browser errors and warnings.
// - Tabs and windows.
// - Places previously been e.g. History, bookmarks, recent tabs.
// - Page actions e.g. zoom, edit, find, print.
// - Learn about the browser and global customisation e.g. settings, help.
// - Browser relaunch, quit.
void AppMenuModel::Build() {
  // Build (and, by extension, Init) should only be called once.
  DCHECK_EQ(0u, GetItemCount());

  if (app_menu_icon_controller_ &&
      app_menu_icon_controller_->GetTypeAndSeverity().type ==
          AppMenuIconController::IconType::kUpgradeNotification) {
    AddSeparator(ui::SPACING_SEPARATOR);
    const auto update_icon = ui::ImageModel::FromVectorIcon(
        features::IsRoundedIconsEnabled()
            ? kRocketLaunchIcon
            : kBrowserToolsUpdateChromeRefreshOldIcon,
        ui::kColorMenuIconOnEmphasizedBackground, kDefaultIconSize);
    if (browser_defaults::kShowUpgradeMenuItem) {
      AddItemWithIcon(IDC_UPGRADE_DIALOG, GetUpgradeDialogTitleText(),
                      update_icon);
      AddSeparator(ui::SPACING_SEPARATOR);
    }
  }

  if (AddGlobalErrorMenuItems()) {
    AddSeparator(ui::NORMAL_SEPARATOR);
  }
  AddItemWithStringIdAndVectorIcon(
      this, IDC_NEW_TAB,
      IDS_NEW_TAB,
      features::IsRoundedIconsEnabled() ? kTabIcon : kNewTabRefreshOldIcon);

  AddItemWithStringIdAndVectorIcon(
      this, IDC_NEW_WINDOW, IDS_NEW_WINDOW,
      features::IsRoundedIconsEnabled() ? kNewWindowIcon : kNewWindowOldIcon);

  AddSeparator(ui::NORMAL_SEPARATOR);

  if (!browser_->GetProfile()->IsOffTheRecord()) {
    auto recent_tabs_sub_menu =
        std::make_unique<RecentTabsSubMenuModel>(provider_, browser_);
    recent_tabs_sub_menu->RegisterLogMenuMetricsCallback(base::BindRepeating(
        &AppMenuModel::LogMenuMetrics, base::Unretained(this)));
    sub_menus_.push_back(std::move(recent_tabs_sub_menu));
    AddSubMenuWithStringIdAndVectorIcon(
        this, kRecentTabsMenuPlaceholder, IDS_HISTORY_MENU,
        sub_menus_.back().get(),
        features::IsRoundedIconsEnabled() ? kHistoryIcon : kHistoryOldIcon);
    SetElementIdentifierAt(
        GetIndexOfCommandId(kRecentTabsMenuPlaceholder).value(),
        kHistoryMenuItem);
  }
  AddItemWithStringIdAndVectorIcon(
      this, IDC_SHOW_DOWNLOADS, IDS_SHOW_DOWNLOADS,
      features::IsRoundedIconsEnabled() ? kDownloadIcon : kDownloadMenuOldIcon);
  SetElementIdentifierAt(GetIndexOfCommandId(IDC_SHOW_DOWNLOADS).value(),
                         kDownloadsMenuItem);
  // Extensions sub menu.
  if (ArePromotionsEnabled() &&
      base::FeatureList::IsEnabled(features::kExtensionsCollapseMainMenu) &&
      !extensions::ui_util::HasManageableExtensions(browser_->GetProfile())) {
    AddItemWithStringIdAndVectorIcon(
        this, IDC_FIND_EXTENSIONS, IDS_FIND_EXTENSIONS,
        features::IsRoundedIconsEnabled()
            ? vector_icons::kChromeExtensionIcon
            : vector_icons::kExtensionChromeRefreshOldIcon);
    SetElementIdentifierAt(GetIndexOfCommandId(IDC_FIND_EXTENSIONS).value(),
                           ExtensionsMenuModel::kVisitChromeWebStoreMenuItem);
  } else {
    sub_menus_.push_back(std::make_unique<ExtensionsMenuModel>(this, browser_));
    AddSubMenuWithStringIdAndVectorIcon(
        this, kExtensionsSubmenuPlaceholder, IDS_EXTENSIONS_SUBMENU,
        sub_menus_.back().get(),
        features::IsRoundedIconsEnabled()
            ? vector_icons::kChromeExtensionIcon
            : vector_icons::kExtensionChromeRefreshOldIcon);
    SetElementIdentifierAt(
        GetIndexOfCommandId(kExtensionsSubmenuPlaceholder).value(),
        kExtensionsMenuItem);
  }

  AddItemWithStringIdAndVectorIcon(
      this, IDC_CLEAR_BROWSING_DATA, IDS_CLEAR_BROWSING_DATA,
      features::IsRoundedIconsEnabled() ? kDeleteIcon
                                        : kTrashCanRefreshOldIcon);
  SetElementIdentifierAt(GetIndexOfCommandId(IDC_CLEAR_BROWSING_DATA).value(),
                         kClearBrowsingDataMenuItem);

  AddSeparator(ui::NORMAL_SEPARATOR);
  CreateZoomMenu();
  AddSeparator(ui::NORMAL_SEPARATOR);

  AddItemWithStringIdAndVectorIcon(
      this, IDC_PRINT, IDS_PRINT,
      features::IsRoundedIconsEnabled() ? kPrintIcon : kPrintMenuOldIcon);

  CreateFindAndEditSubMenu();

  sub_menus_.push_back(
      std::make_unique<SaveAndShareSubMenuModel>(this, browser_));
  AddSubMenuWithStringIdAndVectorIcon(
      this, kSaveAndShareMenuPlaceholder, IDS_SAVE_AND_SHARE_MENU,
      sub_menus_.back().get(),
      features::IsRoundedIconsEnabled() ? kFileSaveIcon
                                        : kFileSaveChromeRefreshOldIcon);
  SetElementIdentifierAt(
      GetIndexOfCommandId(kSaveAndShareMenuPlaceholder).value(),
      kSaveAndShareMenuItem);

#if BUILDFLAG(IS_CHROMEOS)
  // Always show this option if we're in tablet mode on Chrome OS.
  if (display::Screen::Get()->InTabletMode()) {
    AddItemWithStringIdAndVectorIcon(
        this, IDC_TOGGLE_REQUEST_TABLET_SITE, IDS_TOGGLE_REQUEST_TABLET_SITE,
        chrome::IsRequestingTabletSite(browser_)
            ? features::IsRoundedIconsEnabled()
                  ? kMobileCheckIcon
                  : kRequestMobileSiteCheckedOldIcon
        : features::IsRoundedIconsEnabled()
            ? kMobileIcon
            : kRequestMobileSiteUncheckedOldIcon);
  }
#endif

  sub_menus_.push_back(std::make_unique<ToolsMenuModel>(this, browser_));
  AddSubMenuWithStringIdAndVectorIcon(
      this, kMoreToolsMenuPlaceholder, IDS_MORE_TOOLS_MENU,
      sub_menus_.back().get(),
      features::IsRoundedIconsEnabled() ? kHomeRepairServiceIcon
                                        : kMoreToolsMenuOldIcon);
  SetElementIdentifierAt(GetIndexOfCommandId(kMoreToolsMenuPlaceholder).value(),
                         kMoreToolsMenuItem);

  AddSeparator(ui::NORMAL_SEPARATOR);
// The help submenu is only displayed on official Chrome builds. As the
// 'About' item has been moved to this submenu, it's reinstated here for
// Chromium builds.
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  sub_menus_.push_back(std::make_unique<HelpMenuModel>(this, browser_));
  AddSubMenuWithStringIdAndVectorIcon(
      this, kHelpMenuPlaceholder, IDS_HELP_MENU, sub_menus_.back().get(),
      features::IsRoundedIconsEnabled() ? kHelpCustomIcon : kHelpMenuOldIcon);
  SetElementIdentifierAt(GetIndexOfCommandId(kHelpMenuPlaceholder).value(),
                         kHelpMenuItem);
#else
#if BUILDFLAG(IS_CHROMEOS)
  AddItem(IDC_ABOUT, l10n_util::GetStringUTF16(IDS_ABOUT));
#else
  AddItemWithStringIdAndVectorIcon(this, IDC_ABOUT, IDS_ABOUT,
                                   features::IsRoundedIconsEnabled()
                                       ? vector_icons::kInfoIcon
                                       : vector_icons::kInfoRefreshOldIcon);
#endif
#endif

  AddItemWithStringIdAndVectorIcon(
      this, IDC_OPTIONS, IDS_SETTINGS,
      features::IsRoundedIconsEnabled() ? kSettingsIcon : kSettingsMenuOldIcon);

  if (browser_defaults::kShowExitMenuItem) {
    AddItemWithStringIdAndVectorIcon(
        this, IDC_EXIT, IDS_EXIT,
        features::IsRoundedIconsEnabled() ? kExitToAppIcon : kExitMenuOldIcon);
  }

  // On Chrome OS, similar UI is displayed in the system tray menu, instead of
  // this menu.
#if !BUILDFLAG(IS_CHROMEOS)
  if (ShouldDisplayManagedUi(browser_->GetProfile())) {
    AddSeparator(ui::NORMAL_SEPARATOR);
    AddItemWithIcon(
        IDC_SHOW_MANAGEMENT_PAGE,
        GetManagedUiMenuItemLabel(browser_->GetProfile()),
        ui::ImageModel::FromVectorIcon(GetManagedUiIcon(browser_->GetProfile()),
                                       ui::kColorMenuIcon, kDefaultIconSize));

    SetAccessibleNameAt(GetIndexOfCommandId(IDC_SHOW_MANAGEMENT_PAGE).value(),
                        GetManagedUiMenuItemTooltip(browser_->GetProfile()));
#if BUILDFLAG(IS_LINUX)
    if (enterprise_util::IsBrowserManaged(browser_->GetProfile()) &&
        base::FeatureList::IsEnabled(features::kEnterpriseReleaseNotes)) {
      AddItemWithStringIdAndVectorIcon(
          this, IDC_CHROME_ENTERPRISE_RELEASE_NOTES,
          IDS_CHROME_ENTERPRISE_RELEASE_NOTES, omnibox::kChromeProductIcon);
    }
#endif  // BUILDFLAG(IS_LINUX)
  }
#endif  // !BUILDFLAG(IS_CHROMEOS)

  uma_action_recorded_ = false;
}

void AppMenuModel::CreateCutCopyPasteMenu() {
  edit_menu_item_model_ =
      std::make_unique<ui::ButtonMenuItemModel>(IDS_EDIT, this);
  edit_menu_item_model_->AddGroupItemWithStringId(IDC_CUT, IDS_CUT);
  edit_menu_item_model_->AddGroupItemWithStringId(IDC_COPY, IDS_COPY);
  edit_menu_item_model_->AddGroupItemWithStringId(IDC_PASTE, IDS_PASTE);
  AddButtonItem(kEditMenuPlaceholder, edit_menu_item_model_.get());
}

void AppMenuModel::CreateFindAndEditSubMenu() {
  sub_menus_.push_back(std::make_unique<FindAndEditSubMenuModel>(this));
  AddSubMenuWithStringIdAndVectorIcon(
      this, kFindAndEditMenuPlaceholder, IDS_FIND_AND_EDIT_MENU,
      sub_menus_.back().get(),
      features::IsRoundedIconsEnabled() ? kFindInPageIcon : kSearchMenuOldIcon);
}

void AppMenuModel::CreateZoomMenu() {
  zoom_menu_item_model_ =
      std::make_unique<ui::ButtonMenuItemModel>(IDS_ZOOM_MENU, this);
  zoom_menu_item_model_->AddGroupItemWithStringId(IDC_ZOOM_MINUS,
                                                  IDS_ZOOM_MINUS2);
  zoom_menu_item_model_->AddGroupItemWithStringId(IDC_ZOOM_PLUS,
                                                  IDS_ZOOM_PLUS2);
  zoom_menu_item_model_->AddImageItem(IDC_FULLSCREEN);
  AddButtonItem(kZoomMenuPlaceholder, zoom_menu_item_model_.get());
  SetCommandIcon(
      this, kZoomMenuPlaceholder,
      features::IsRoundedIconsEnabled() ? kZoomInIcon : kZoomInOldIcon);
}

bool AppMenuModel::AddGlobalErrorMenuItems() {
  // TODO(sail): Currently we only build the app menu once per browser
  // window. This means that if a new error is added after the menu is built
  // it won't show in the existing app menu. To fix this we need to some
  // how update the menu if new errors are added.
  const GlobalErrorService::GlobalErrorList& errors =
      GlobalErrorServiceFactory::GetForProfile(browser_->GetProfile())
          ->errors();
  bool menu_items_added = false;
  for (GlobalError* error : errors) {
    DCHECK(error);
    if (error->HasMenuItem()) {
      AddItem(error->MenuItemCommandID(), error->MenuItemLabel());
      SetIconForCommandId(error->MenuItemCommandID(), error->MenuItemIcon());
      menu_items_added = true;
    }
  }
  return menu_items_added;
}

#if BUILDFLAG(IS_CHROMEOS)
void AppMenuModel::UpdateSettingsItemState() {
  bool is_disabled =
      policy::SystemFeaturesDisableListPolicyHandler::IsSystemFeatureDisabled(
          policy::SystemFeature::kBrowserSettings,
          g_browser_process->local_state());

  std::optional<size_t> index = GetIndexOfCommandId(IDC_OPTIONS);
  if (index.has_value()) {
    SetEnabledAt(index.value(), !is_disabled);
  }

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  index = GetIndexOfCommandId(kHelpMenuPlaceholder);
  if (index.has_value()) {
    ui::SimpleMenuModel* help_menu =
        static_cast<ui::SimpleMenuModel*>(GetSubmenuModelAt(index.value()));
    index = help_menu->GetIndexOfCommandId(IDC_ABOUT);
    if (index.has_value()) {
      help_menu->SetEnabledAt(index.value(), !is_disabled);
    }
  }
#else   // BUILDFLAG(GOOGLE_CHROME_BRANDING)
  index = GetIndexOfCommandId(IDC_ABOUT);
  if (index.has_value()) {
    SetEnabledAt(index.value(), !is_disabled);
  }
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
}
#endif  // BUILDFLAG(IS_CHROMEOS)
