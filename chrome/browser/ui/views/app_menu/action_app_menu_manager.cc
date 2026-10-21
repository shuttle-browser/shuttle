// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/app_menu/action_app_menu_manager.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/debug/profiler.h"
#include "base/functional/bind.h"
#include "base/functional/function_ref.h"
#include "base/i18n/rtl.h"
#include "base/memory/raw_ptr.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/enterprise/isolated_mode/isolated_mode_settings_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/actions/command_action_updater.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/managed_ui.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/search/ntp_features.h"
#include "ui/base/class_property.h"
#include "ui/base/models/menu_separator_types.h"
#if BUILDFLAG(IS_CHROMEOS)
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "ui/display/screen.h"
#endif
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"
#include "chrome/browser/ui/toolbar/app_menu_icon_controller.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/browser/ui/toolbar/chrome_labs/chrome_labs_prefs.h"
#include "chrome/browser/ui/toolbar/chrome_labs/chrome_labs_utils.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/app_menu/app_menu_zoom_view.h"
#include "chrome/browser/ui/views/app_menu/recent_tabs_dynamic_menu.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "components/vector_icons/vector_icons.h"
#include "extensions/buildflags/buildflags.h"
#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/extension_ui_util.h"
#endif
#include "ui/actions/action_id.h"
#include "ui/actions/actions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/base/models/menu_model.h"
#include "ui/base/ui_base_features.h"
#include "ui/menus/simple_menu_model.h"
#include "ui/views/view_class_properties.h"

namespace {

#if BUILDFLAG(IS_CHROMEOS)
bool IsRequestingTabletSite(BrowserWindowInterface* browser) {
  if (!browser || !browser->GetTabStripModel()) {
    return false;
  }
  content::WebContents* current_tab =
      browser->GetTabStripModel()->GetActiveWebContents();
  if (!current_tab) {
    return false;
  }
  content::NavigationEntry* entry =
      current_tab->GetController().GetLastCommittedEntry();
  if (!entry) {
    return false;
  }
  return entry->GetIsOverridingUserAgent();
}
#endif

bool ArePromotionsEnabled() {
  PrefService* local_state = g_browser_process->local_state();
  return local_state && local_state->GetBoolean(prefs::kPromotionsEnabled);
}

// Builder helper to simplify declaring the action item structure for the app
// menu.
class AppMenuBuilder {
 public:
  using DisplayType = AppMenuActionItem::DisplayType;

  AppMenuBuilder(actions::ActionItem* parent,
                 BrowserWindowInterface* browser_window_interface,
                 std::optional<ui::ColorId> bg_color = std::nullopt,
                 DisplayType default_display_type = DisplayType::kRow)
      : AppMenuBuilder(static_cast<actions::BaseAction*>(parent),
                       browser_window_interface,
                       bg_color,
                       default_display_type) {}
  AppMenuBuilder(actions::BaseAction* parent,
                 BrowserWindowInterface* browser_window_interface,
                 std::optional<ui::ColorId> bg_color = std::nullopt,
                 DisplayType default_display_type = DisplayType::kRow)
      : parent_(parent),
        browser_window_interface_(browser_window_interface),
        scope_(BrowserActions::From(browser_window_interface_)
                   ->root_action_item()),
        bg_color_(bg_color),
        default_display_type_(default_display_type) {}
  AppMenuBuilder(const AppMenuBuilder&) = delete;
  AppMenuBuilder& operator=(const AppMenuBuilder&) = delete;
  ~AppMenuBuilder() = default;

  // Adds an action item.
  AppMenuBuilder& AddAction(actions::ActionId id,
                            AppMenuActionItem::ActionParams params = {}) {
    if (!params.container_color.has_value()) {
      params.container_color = bg_color_;
    }
    if (!params.display_type.has_value()) {
      params.display_type = default_display_type_;
    }
    if (!params.is_alerted.has_value()) {
      params.is_alerted = IsElementIdAlerted(params.element_id);
    }
    if (params.is_alerted.value()) {
      has_alerted_child_ = true;
    }
    auto item =
        AppMenuActionItem::CreateIndirect(id, scope_, std::move(params));
    if (item && parent_) {
      parent_->AddChild(std::move(item));
    }
    return *this;
  }

  // Adds a header item to the current parent without modifying the parent.
  AppMenuBuilder& AddHeader(int string_id) {
    auto header_item = AppMenuActionItem::CreateHeader(
        l10n_util::GetStringUTF16(string_id), bg_color_);
    if (parent_) {
      parent_->AddChild(std::move(header_item));
    }
    return *this;
  }

  AppMenuBuilder& AddDivider(
      ui::MenuSeparatorType type = ui::NORMAL_SEPARATOR) {
    auto item = AppMenuActionItem::CreateDivider(type);
    if (parent_) {
      parent_->AddChild(std::move(item));
    }
    return *this;
  }

  // Adds a static submenu via lambda.
  AppMenuBuilder& AddSubmenu(
      actions::ActionId id,
      base::FunctionRef<void(AppMenuBuilder&)> build_submenu,
      AppMenuActionItem::ActionParams params = {}) {
    if (!params.container_color.has_value()) {
      params.container_color = bg_color_;
    }
    if (!params.display_type.has_value()) {
      params.display_type = default_display_type_;
    }
    if (!params.is_alerted.has_value()) {
      params.is_alerted = IsElementIdAlerted(params.element_id);
    }
    const bool self_alerted = params.is_alerted.value();
    auto item =
        AppMenuActionItem::CreateIndirect(id, scope_, std::move(params));
    if (!item || !parent_) {
      return *this;
    }
    auto* item_ptr = parent_->AddChild(std::move(item));
    AppMenuBuilder sub_builder(item_ptr, browser_window_interface_);
    build_submenu(sub_builder);
    // If any item inside the submenu is alerted as part of a running tutorial,
    // also alert this parent submenu row so the user knows which submenu to
    // open, and propagate the alerted state up to any outer submenu.
    if (sub_builder.has_alerted_child_) {
      item_ptr->SetProperty(AppMenuActionItem::kIsAlertedKey, true);
    }
    if (self_alerted || sub_builder.has_alerted_child_) {
      has_alerted_child_ = true;
    }
    return *this;
  }

  // Adds a structural section container, optionally populated via lambda.
  AppMenuBuilder& AddSection(
      DisplayType display_type,
      std::optional<base::FunctionRef<void(AppMenuBuilder&)>> build_section =
          std::nullopt,
      std::optional<ui::ColorId> bg_color = std::nullopt) {
    if (!parent_) {
      return *this;
    }
    const std::optional<ui::ColorId> section_bg_color =
        bg_color.has_value() ? bg_color : bg_color_;
    auto item = actions::ActionItem::Builder().Build();
    item->SetProperty(AppMenuActionItem::kDisplayTypeKey, display_type);
    if (section_bg_color.has_value()) {
      item->SetProperty(AppMenuActionItem::kContainerColorKey,
                        section_bg_color.value());
    }
    auto* item_ptr = parent_->AddChild(std::move(item));
    if (build_section.has_value()) {
      AppMenuBuilder section_builder(item_ptr, browser_window_interface_,
                                     section_bg_color);
      (*build_section)(section_builder);
    }
    return *this;
  }

  // Adds a dynamic submenu populated at runtime.
  AppMenuBuilder& AddDynamicSubmenu(
      actions::ActionId id,
      actions::BaseAction::PopulateChildActions populate_callback,
      std::optional<base::FunctionRef<void(AppMenuBuilder&)>> build_submenu =
          std::nullopt,
      AppMenuActionItem::ActionParams params = {}) {
    if (!params.container_color.has_value()) {
      params.container_color = bg_color_;
    }
    if (!params.is_alerted.has_value()) {
      params.is_alerted = IsElementIdAlerted(params.element_id);
    }
    const bool self_alerted = params.is_alerted.value();
    auto item =
        AppMenuActionItem::CreateIndirect(id, scope_, std::move(params));
    if (!item || !parent_) {
      return *this;
    }
    bool sub_alerted = false;
    if (build_submenu.has_value()) {
      AppMenuBuilder sub_builder(item.get(), browser_window_interface_);
      (*build_submenu)(sub_builder);
      sub_alerted = sub_builder.has_alerted_child_;
    }
    // Propagate any alerted child item in `build_submenu` to this parent
    // submenu row and to any enclosing submenu.
    if (sub_alerted) {
      item->SetProperty(AppMenuActionItem::kIsAlertedKey, true);
    }
    if (self_alerted || sub_alerted) {
      has_alerted_child_ = true;
    }

    item->SetPopulateChildrenCallback(std::move(populate_callback));
    item->PopulateChildItems();
    parent_->AddChild(std::move(item));
    return *this;
  }

  // Adds a dynamic section directly into the current parent item via callback.
  AppMenuBuilder& AddDynamicSection(
      base::FunctionRef<void(actions::BaseAction*)> build_section) {
    if (parent_) {
      build_section(parent_);
    }
    return *this;
  }

 private:
  // Returns true if `element_id` is targeted by a running tutorial.
  bool IsElementIdAlerted(ui::ElementIdentifier element_id) const {
    // Shuttle: user education (tutorials) is removed; nothing is alerted.
    return false;
  }

  raw_ptr<actions::BaseAction> parent_;
  raw_ptr<BrowserWindowInterface> browser_window_interface_;
  raw_ptr<actions::ActionItem> scope_;
  std::optional<ui::ColorId> bg_color_;
  DisplayType default_display_type_ = DisplayType::kRow;
  // Tracks whether any action or nested submenu added by this builder is
  // alerted, so parent submenu rows can also be highlighted when a child item
  // inside the submenu is targeted by a tutorial.
  bool has_alerted_child_ = false;
};

}  // namespace

ActionAppMenuManager::ActionAppMenuManager(
    BrowserWindowInterface* browser_window_interface)
    : browser_window_interface_(browser_window_interface),
      recent_tabs_menu_(
          std::make_unique<RecentTabsDynamicMenu>(browser_window_interface)) {}

ActionAppMenuManager::~ActionAppMenuManager() = default;

actions::ActionItem* ActionAppMenuManager::GetAppMenuRoot() const {
  return actions::ActionManager::Get().FindAction(
      kActionAppMenuRoot,
      BrowserActions::From(browser_window_interface_)->root_action_item());
}

void ActionAppMenuManager::CreateMenuHierarchy() {
  actions::ActionItem* root = GetAppMenuRoot();
  if (!root) {
    return;
  }

  AddNotificationActions(root);
  AddSearchBarAction(root);
  AddBlockHeaderActions(root);
  AddYourChromeActions(root);
  AddToolsAndActionsActions(root);
  AddFooterActions(root);
}

void ActionAppMenuManager::OnMenuClosed() {
  GetAppMenuRoot()->ResetActionList();
}

void ActionAppMenuManager::AddNotificationActions(actions::ActionItem* root) {
  actions::ActionItem* scope =
      BrowserActions::From(browser_window_interface_)->root_action_item();
  AppMenuBuilder(root, browser_window_interface_,
                 ui::kColorAppMenuUpgradeRowBackground)
      .AddSection(DisplayType::kSection, [this,
                                          scope](AppMenuBuilder& section) {
        bool has_notification = false;

        // Helper for adding a notification if its action is visible, with an
        // optional leading spacer if there is another notification already
        // added.
        auto maybe_add_notification =
            [&section, scope, &has_notification](
                actions::ActionId id,
                AppMenuActionItem::ActionParams params = {}) {
              actions::ActionItem* action =
                  actions::ActionManager::Get().FindAction(id, scope);
              CHECK(action);
              if (!action->GetVisible()) {
                return false;
              }
              if (has_notification) {
                section.AddDivider(ui::MenuSeparatorType::SPACING_SEPARATOR);
              }
              params.display_type = DisplayType::kNotification;
              section.AddAction(id, std::move(params));
              has_notification = true;
              return true;
            };

        maybe_add_notification(
            kActionUpgradeDialog,
            {.minor_text = AppMenuModel::GetUpgradeDialogSubstringText()});

        // At most one non-upgrade notification item (Global Error or Default
        // Browser, ordered by priority) should be shown at a time. Return early
        // as soon as the first one is added. Shuttle: no Safety Hub.
        if (maybe_add_notification(kActionGlobalError)) {
          return;
        }

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)
        maybe_add_notification(
            kActionSetBrowserAsDefault,
            {.element_id = AppMenuModel::kSetBrowserAsDefaultMenuItem});
#endif
      });
}

void ActionAppMenuManager::AddSearchBarAction(actions::ActionItem* root) {
  if (base::FeatureList::IsEnabled(features::kChroMenuSearch)) {
    AppMenuBuilder(root, browser_window_interface_)
        .AddSection(DisplayType::kSearch);
  }
}

void ActionAppMenuManager::AddBlockHeaderActions(actions::ActionItem* root) {
  AppMenuBuilder(root, browser_window_interface_)
      .AddSection(DisplayType::kBlock, [](AppMenuBuilder& section) {
        section
            .AddAction(
                kActionNewTab,
                {.display_type = DisplayType::kBlock,
                 .icon_override = ui::ImageModel::FromVectorIcon(
                     features::IsRoundedIconsEnabled() ? kTabIcon
                                                       : kNewTabRefreshOldIcon,
                     ui::kColorIcon, ui::SimpleMenuModel::kDefaultIconSize)})
            .AddAction(kActionNewWindow, {.display_type = DisplayType::kBlock});
      });
}

void ActionAppMenuManager::AddYourChromeActions(actions::ActionItem* root) {
  AppMenuBuilder(root, browser_window_interface_,
                 kColorAppMenuYourChromeBackground)
      .AddSection(DisplayType::kSection, [this](AppMenuBuilder& section) {
        section.AddHeader(IDS_APP_MENU_YOUR_CHROME_HEADER);

        Profile* profile = browser_window_interface_->GetProfile();

        if (!profile->IsOffTheRecord()) {
          section.AddDynamicSubmenu(
              kActionRecentTabsSubmenu,
              base::BindRepeating(
                  &RecentTabsDynamicMenu::BuildRecentTabsActions,
                  recent_tabs_menu_->GetWeakPtr()),
              /*build_submenu=*/std::nullopt,
              {.element_id = AppMenuModel::kHistoryMenuItem});
        }

        section.AddAction(kActionShowDownloadsPage,
                          {.element_id = AppMenuModel::kDownloadsMenuItem});

#if BUILDFLAG(ENABLE_EXTENSIONS)
        if (ArePromotionsEnabled() &&
            base::FeatureList::IsEnabled(
                features::kExtensionsCollapseMainMenu) &&
            !extensions::ui_util::HasManageableExtensions(profile)) {
          section.AddAction(
              kActionFindExtensions,
              {.element_id =
                   ExtensionsMenuModel::kVisitChromeWebStoreMenuItem});
        } else {
          section.AddSubmenu(
              kActionExtensionsSubmenu,
              [](AppMenuBuilder& sub) {
                sub.AddAction(
                       kActionExtensionsSubmenuManageExtensions,
                       {.element_id =
                            ExtensionsMenuModel::kManageExtensionsMenuItem})
                    .AddAction(kActionExtensionsSubmenuVisitChromeWebStore,
                               {.element_id = ExtensionsMenuModel::
                                    kVisitChromeWebStoreMenuItem});
              },
              {.element_id = AppMenuModel::kExtensionsMenuItem});
        }
#endif

        section.AddAction(
            kActionClearBrowsingData,
            {.element_id = AppMenuModel::kClearBrowsingDataMenuItem});
      });
}

void ActionAppMenuManager::AddToolsAndActionsActions(
    actions::ActionItem* root) {
  AppMenuBuilder(root, browser_window_interface_,
                 kColorAppMenuToolsAndActionsBackground)
      .AddSection(DisplayType::kSection, [this](AppMenuBuilder& section) {
        section.AddHeader(IDS_APP_MENU_TOOLS_AND_ACTIONS_HEADER)
            .AddSubmenu(
                kActionZoomSubmenu,
                [](AppMenuBuilder& sub) {
                  sub.AddAction(kActionZoomMinus)
                      .AddAction(kActionZoomNormal)
                      .AddAction(kActionZoomPlus)
                      .AddAction(kActionFullscreen);
                },
                {.display_type = DisplayType::kCustom,
                 .item_height = AppMenuActionItem::ItemHeight::kExpanded})
            .AddDivider(ui::MenuSeparatorType::SPACING_SEPARATOR)
            .AddAction(kActionPrint);

        section.AddSubmenu(kActionFindAndEditSubmenu, [](AppMenuBuilder& sub) {
          sub.AddAction(kActionFind)
              .AddDivider()
              .AddAction(actions::kActionCut)
              .AddAction(actions::kActionCopy)
              .AddAction(actions::kActionPaste);
        });

        section.AddSubmenu(
            kActionSaveAndShareSubmenu,
            [](AppMenuBuilder& sub) {
              sub.AddHeader(IDS_SAVE_AND_SHARE_MENU_SAVE)
                  .AddAction(kActionSavePage)
                  .AddDivider()
                  .AddHeader(IDS_SAVE_AND_SHARE_MENU_SHARE)
                  .AddAction(kActionCopyUrl);
            },
            {.text_override = l10n_util::GetStringUTF16(IDS_SAVE_AND_SHARE_MENU),
             .element_id = AppMenuModel::kSaveAndShareMenuItem});

#if BUILDFLAG(IS_CHROMEOS)
        if (display::Screen::Get()->InTabletMode()) {
          section.AddAction(
              kActionToggleRequestTabletSite,
              {.text_override =
                   l10n_util::GetStringUTF16(IDS_TOGGLE_REQUEST_TABLET_SITE),
               .icon_override = ui::ImageModel::FromVectorIcon(
                   IsRequestingTabletSite(browser_window_interface_.get())
                       ? (features::IsRoundedIconsEnabled()
                              ? kMobileCheckIcon
                              : kRequestMobileSiteCheckedOldIcon)
                       : (features::IsRoundedIconsEnabled()
                              ? kMobileIcon
                              : kRequestMobileSiteUncheckedOldIcon),
                   ui::kColorMenuIcon, ui::SimpleMenuModel::kDefaultIconSize)});
        }
#endif

        section.AddSubmenu(
            kActionDeveloperSubmenu,
            [this](AppMenuBuilder& sub) {
              sub.AddAction(kActionNameWindow);

              if (auto* controller =
                      tabs::VerticalTabStripStateController::From(
                          browser_window_interface_.get())) {
                if (controller->ShouldDisplayVerticalTabs()) {
                  sub.AddAction(
                      kActionToggleVerticalTabs,
                      {.text_override = l10n_util::GetStringUTF16(
                           IDS_SWITCH_TO_HORIZONTAL_TAB),
                       .icon_override = ui::ImageModel::FromVectorIcon(
                           features::IsRoundedIconsEnabled() ? kToolbarIcon
                                                             : kToolbarOldIcon,
                           ui::kColorMenuIcon,
                           ui::SimpleMenuModel::kDefaultIconSize)});
                } else {
                  sub.AddAction(
                      kActionToggleVerticalTabs,
                      {.text_override = l10n_util::GetStringUTF16(
                           IDS_SWITCH_TO_VERTICAL_TAB),
                       .icon_override = ui::ImageModel::FromVectorIcon(
                           base::i18n::IsRTL()
                               ? (features::IsRoundedIconsEnabled()
                                      ? kDockToLeftIcon
                                      : kDockToRightOldIcon)
                               : (features::IsRoundedIconsEnabled()
                                      ? kDockToRightIcon
                                      : kDockToLeftOldIcon),
                           ui::kColorMenuIcon,
                           ui::SimpleMenuModel::kDefaultIconSize),
                       .new_badge_feature = &tabs::kVerticalTabsNewBadge});
                }
              }

              sub.AddDivider()
                  .AddAction(
                      kActionPerformance,
                      {.element_id = ToolsMenuModel::kPerformanceMenuItem})
                  .AddAction(kActionTaskManagerAppMenu);

#if BUILDFLAG(IS_CHROMEOS)
              sub.AddAction(kActionTakeScreenshot);
#endif

              sub.AddDivider().AddAction(kActionDevTools);

              if (base::debug::IsProfilingSupported()) {
                sub.AddDivider().AddAction(kActionProfilingEnabled,
                                           {.is_checkable = true});
              }

              Profile* profile = browser_window_interface_->GetProfile();
              if (IsChromeLabsEnabled()) {
                UpdateChromeLabsNewBadgePrefs(profile);
                if (ShouldShowChromeLabsUI(profile) &&
                    profile->GetPrefs()->GetBoolean(
                        chrome_labs_prefs::
                            kBrowserLabsEnabledEnterprisePolicy)) {
                  sub.AddDivider().AddAction(
                      kActionShowChromeLabs,
                      {.element_id = ToolsMenuModel::kChromeLabsMenuItem});
                }
              }
            },
            {.element_id = AppMenuModel::kMoreToolsMenuItem});
      });
}

void ActionAppMenuManager::AddFooterActions(actions::ActionItem* root) {
  AppMenuBuilder(root, browser_window_interface_)
      .AddSection(DisplayType::kFooter, [browser_window_interface =
                                             browser_window_interface_.get()](
                                            AppMenuBuilder& section) {
        section.AddAction(kActionOptions);

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
        section.AddSubmenu(
            kActionHelpSubmenu,
            [](AppMenuBuilder& sub) {
              sub.AddAction(kActionAbout);
#if BUILDFLAG(IS_CHROMEOS) && defined(OFFICIAL_BUILD)
              sub.AddAction(
                  kActionHelpPageViaMenu,
                  {.text_override = l10n_util::GetStringUTF16(IDS_GET_HELP)});
#else
              sub.AddAction(kActionHelpPageViaMenu);
#endif
            },
            {.element_id = AppMenuModel::kHelpMenuItem});
#else
        section.AddAction(kActionAbout);
#endif

        if (browser_defaults::kShowExitMenuItem) {
          section.AddAction(kActionExit);
        }

#if !BUILDFLAG(IS_CHROMEOS)
        Profile* profile = browser_window_interface->GetProfile();
        if (ShouldDisplayManagedUi(profile)) {
          section.AddAction(
              kActionShowManagementPage,
              {.display_type = DisplayType::kRow,
               .text_override = GetManagedUiMenuItemLabel(profile),
               .icon_override =
                   ui::ImageModel::FromVectorIcon(GetManagedUiIcon(profile))});
        }
#endif  // !BUILDFLAG(IS_CHROMEOS)
      });
}
