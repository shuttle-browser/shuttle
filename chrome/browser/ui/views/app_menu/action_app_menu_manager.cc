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
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
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
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"
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
                 actions::ActionItem* scope,
                 std::optional<ui::ColorId> bg_color = std::nullopt,
                 DisplayType default_display_type = DisplayType::kRow)
      : AppMenuBuilder(static_cast<actions::BaseAction*>(parent),
                       scope,
                       bg_color,
                       default_display_type) {}
  AppMenuBuilder(actions::BaseAction* parent,
                 actions::ActionItem* scope,
                 std::optional<ui::ColorId> bg_color = std::nullopt,
                 DisplayType default_display_type = DisplayType::kRow)
      : parent_(parent),
        scope_(scope),
        bg_color_(bg_color),
        default_display_type_(default_display_type) {}
  AppMenuBuilder(const AppMenuBuilder&) = delete;
  AppMenuBuilder& operator=(const AppMenuBuilder&) = delete;
  ~AppMenuBuilder() = default;

  // Adds a standard row item.
  AppMenuBuilder& AddAction(
      actions::ActionId id,
      std::optional<DisplayType> type = std::nullopt,
      std::optional<std::u16string> text_override = std::nullopt,
      std::optional<ui::ImageModel> icon_override = std::nullopt) {
    auto item = AppMenuActionItem::CreateIndirect(
        id, scope_, type.value_or(default_display_type_), bg_color_,
        std::move(text_override), std::move(icon_override));
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
      std::optional<DisplayType> type = std::nullopt,
      std::optional<std::u16string> text_override = std::nullopt) {
    auto item = AppMenuActionItem::CreateIndirect(
        id, scope_, type.value_or(default_display_type_), bg_color_,
        std::move(text_override));
    if (!item || !parent_) {
      return *this;
    }
    auto* item_ptr = parent_->AddChild(std::move(item));
    AppMenuBuilder sub_builder(item_ptr, scope_);
    build_submenu(sub_builder);
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
      AppMenuBuilder section_builder(item_ptr, scope_, section_bg_color);
      (*build_section)(section_builder);
    }
    return *this;
  }

  // Adds a dynamic submenu populated at runtime.
  AppMenuBuilder& AddDynamicSubmenu(
      actions::ActionId id,
      actions::BaseAction::PopulateChildActions populate_callback,
      std::optional<base::FunctionRef<void(AppMenuBuilder&)>> build_submenu =
          std::nullopt) {
    auto item = AppMenuActionItem::CreateIndirect(
        id, scope_, default_display_type_, bg_color_);
    if (!item || !parent_) {
      return *this;
    }
    if (build_submenu.has_value()) {
      AppMenuBuilder sub_builder(item.get(), scope_);
      (*build_submenu)(sub_builder);
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
  raw_ptr<actions::BaseAction> parent_;
  raw_ptr<actions::ActionItem> scope_;
  std::optional<ui::ColorId> bg_color_;
  DisplayType default_display_type_ = DisplayType::kRow;
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

  AddSearchBarAction(root);
  AddBlockHeaderActions(root);
  AddYourChromeActions(root);
  AddToolsAndActionsActions(root);
  AddFooterActions(root);
}

void ActionAppMenuManager::AddSearchBarAction(actions::ActionItem* root) {
  if (base::FeatureList::IsEnabled(features::kChroMenuSearch)) {
    AppMenuBuilder(
        root,
        BrowserActions::From(browser_window_interface_)->root_action_item())
        .AddSection(DisplayType::kSearch);
  }
}

void ActionAppMenuManager::AddBlockHeaderActions(actions::ActionItem* root) {
  AppMenuBuilder(
      root, BrowserActions::From(browser_window_interface_)->root_action_item())
      .AddSection(DisplayType::kBlock, [](AppMenuBuilder& section) {
        section
            .AddAction(
                kActionNewTab, DisplayType::kBlock,
                /*text_override=*/std::nullopt,
                /*icon_override=*/
                ui::ImageModel::FromVectorIcon(
                    features::IsRoundedIconsEnabled() ? kTabIcon
                                                      : kNewTabRefreshOldIcon,
                    ui::kColorIcon, ui::SimpleMenuModel::kDefaultIconSize))
            .AddAction(kActionNewWindow, DisplayType::kBlock);
      });
}

void ActionAppMenuManager::AddYourChromeActions(actions::ActionItem* root) {
  AppMenuBuilder(
      root, BrowserActions::From(browser_window_interface_)->root_action_item(),
      kColorAppMenuYourChromeBackground)
      .AddSection(DisplayType::kSection, [this](AppMenuBuilder& section) {
        section.AddHeader(IDS_APP_MENU_YOUR_CHROME_HEADER);

        Profile* profile = browser_window_interface_->GetProfile();

        if (!profile->IsOffTheRecord()) {
          section.AddDynamicSubmenu(
              kActionRecentTabsSubmenu,
              base::BindRepeating(
                  &RecentTabsDynamicMenu::BuildRecentTabsActions,
                  recent_tabs_menu_->GetWeakPtr()));
        }

        section.AddAction(kActionShowDownloadsPage);

#if BUILDFLAG(ENABLE_EXTENSIONS)
        if (ArePromotionsEnabled() &&
            base::FeatureList::IsEnabled(
                features::kExtensionsCollapseMainMenu) &&
            !extensions::ui_util::HasManageableExtensions(profile)) {
          section.AddAction(kActionFindExtensions);
        } else {
          section.AddSubmenu(kActionExtensionsSubmenu, [](AppMenuBuilder& sub) {
            sub.AddAction(kActionExtensionsSubmenuManageExtensions)
                .AddAction(kActionExtensionsSubmenuVisitChromeWebStore);
          });
        }
#endif

        section.AddAction(kActionClearBrowsingData);
      });
}

void ActionAppMenuManager::AddToolsAndActionsActions(
    actions::ActionItem* root) {
  AppMenuBuilder(
      root, BrowserActions::From(browser_window_interface_)->root_action_item(),
      kColorAppMenuToolsAndActionsBackground)
      .AddSection(DisplayType::kSection, [this](AppMenuBuilder& section) {
        section.AddHeader(IDS_APP_MENU_TOOLS_AND_ACTIONS_HEADER)
            .AddSubmenu(
                kActionZoomSubmenu,
                [](AppMenuBuilder& sub) {
                  sub.AddAction(kActionZoomMinus)
                      .AddAction(kActionZoomPlus)
                      .AddAction(kActionFullscreen);
                },
                DisplayType::kCustom)
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
            /*type=*/std::nullopt,
            /*text_override=*/
            l10n_util::GetStringUTF16(IDS_SAVE_AND_SHARE_MENU));

#if BUILDFLAG(IS_CHROMEOS)
        if (display::Screen::Get()->InTabletMode()) {
          section.AddAction(
              kActionToggleRequestTabletSite, /*type=*/std::nullopt,
              /*text_override=*/
              l10n_util::GetStringUTF16(IDS_TOGGLE_REQUEST_TABLET_SITE),
              /*icon_override=*/
              ui::ImageModel::FromVectorIcon(
                  IsRequestingTabletSite(browser_window_interface_.get())
                      ? (features::IsRoundedIconsEnabled()
                             ? kMobileCheckIcon
                             : kRequestMobileSiteCheckedOldIcon)
                      : (features::IsRoundedIconsEnabled()
                             ? kMobileIcon
                             : kRequestMobileSiteUncheckedOldIcon),
                  ui::kColorMenuIcon, ui::SimpleMenuModel::kDefaultIconSize));
        }
#endif

        section.AddSubmenu(kActionDeveloperSubmenu, [this](
                                                        AppMenuBuilder& sub) {
          sub.AddAction(kActionNameWindow);

          if (auto* controller = tabs::VerticalTabStripStateController::From(
                  browser_window_interface_.get())) {
            if (controller->ShouldDisplayVerticalTabs()) {
              sub.AddAction(
                  kActionToggleVerticalTabs, /*type=*/std::nullopt,
                  /*text_override=*/
                  l10n_util::GetStringUTF16(IDS_SWITCH_TO_HORIZONTAL_TAB),
                  /*icon_override=*/
                  ui::ImageModel::FromVectorIcon(
                      features::IsRoundedIconsEnabled() ? kToolbarIcon
                                                        : kToolbarOldIcon,
                      ui::kColorMenuIcon,
                      ui::SimpleMenuModel::kDefaultIconSize));
            } else {
              sub.AddAction(
                  kActionToggleVerticalTabs, /*type=*/std::nullopt,
                  /*text_override=*/
                  l10n_util::GetStringUTF16(IDS_SWITCH_TO_VERTICAL_TAB),
                  /*icon_override=*/
                  ui::ImageModel::FromVectorIcon(
                      base::i18n::IsRTL() ? (features::IsRoundedIconsEnabled()
                                                 ? kDockToLeftIcon
                                                 : kDockToRightOldIcon)
                                          : (features::IsRoundedIconsEnabled()
                                                 ? kDockToRightIcon
                                                 : kDockToLeftOldIcon),
                      ui::kColorMenuIcon,
                      ui::SimpleMenuModel::kDefaultIconSize));
            }
          }

          sub.AddDivider()
              .AddAction(kActionPerformance)
              .AddAction(kActionTaskManagerAppMenu);

#if BUILDFLAG(IS_CHROMEOS)
          sub.AddAction(kActionTakeScreenshot);
#endif

          sub.AddDivider().AddAction(kActionDevTools);

          if (base::debug::IsProfilingSupported()) {
            sub.AddDivider().AddAction(kActionProfilingEnabled);
          }

          Profile* profile = browser_window_interface_->GetProfile();
          if (IsChromeLabsEnabled()) {
            UpdateChromeLabsNewBadgePrefs(profile);
            if (ShouldShowChromeLabsUI(profile) &&
                profile->GetPrefs()->GetBoolean(
                    chrome_labs_prefs::kBrowserLabsEnabledEnterprisePolicy)) {
              sub.AddDivider().AddAction(kActionShowChromeLabs);
            }
          }
        });
      });
}

void ActionAppMenuManager::AddFooterActions(actions::ActionItem* root) {
  AppMenuBuilder(
      root, BrowserActions::From(browser_window_interface_)->root_action_item())
      .AddSection(DisplayType::kFooter, [](AppMenuBuilder& section) {
        section.AddAction(kActionOptions);

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
        section.AddSubmenu(kActionHelpSubmenu, [](AppMenuBuilder& sub) {
          sub.AddAction(kActionAbout);
#if BUILDFLAG(IS_CHROMEOS) && defined(OFFICIAL_BUILD)
          sub.AddAction(
              kActionHelpPageViaMenu, /*type=*/std::nullopt,
              /*text_override=*/l10n_util::GetStringUTF16(IDS_GET_HELP));
#else
          sub.AddAction(kActionHelpPageViaMenu);
#endif
        });
#else
        section.AddAction(kActionAbout);
#endif

        if (browser_defaults::kShowExitMenuItem) {
          section.AddAction(kActionExit);
        }
      });
}
