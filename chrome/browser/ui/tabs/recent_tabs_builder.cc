// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/recent_tabs_builder.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/tabs/recent_tabs_sub_menu_model.h"
#include "chrome/browser/ui/tabs/tab_group_theme.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/side_panel/history_clusters/history_clusters_side_panel_coordinator.h"
#include "chrome/grit/generated_resources.h"
#include "components/sessions/core/tab_restore_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/split_tabs/split_tab_visual_data.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/base/features.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/favicon_size.h"

namespace {

constexpr int kMaxLocalEntries = 8;

ui::ImageModel CreateFavicon(const gfx::VectorIcon& icon) {
  return ui::ImageModel::FromVectorIcon(icon, ui::kColorMenuIcon,
                                        gfx::kFaviconSize);
}

std::u16string GetGroupItemLabel(const std::u16string& title, size_t num_tabs) {
  if (title.empty()) {
    return l10n_util::GetPluralStringFUTF16(IDS_RECENTLY_CLOSED_GROUP_UNNAMED,
                                            static_cast<int>(num_tabs));
  }
  std::u16string item_label = l10n_util::GetPluralStringFUTF16(
      IDS_RECENTLY_CLOSED_GROUP, static_cast<int>(num_tabs));
  return base::ReplaceStringPlaceholders(item_label, title, nullptr);
}

RecentTabItem BuildTabItem(const sessions::tab_restore::Tab& tab) {
  const sessions::SerializedNavigationEntry& current_navigation =
      tab.navigations.at(tab.normalized_navigation_index());
  std::u16string title =
      current_navigation.title().empty()
          ? base::UTF8ToUTF16(current_navigation.virtual_url().spec())
          : current_navigation.title();

  RecentTabItem item(RecentTabItem::Type::kTab, title);
  item.set_session_id(tab.id);
  item.set_url(current_navigation.virtual_url());
  item.set_icon(favicon::GetDefaultFaviconModel());
  item.set_is_local(true);

  if (tab.group_visual_data.has_value()) {
    const ui::ColorId color_id =
        GetTabGroupContextMenuColorId(tab.group_visual_data.value().color());
    constexpr int kIconSize = 12;
    item.set_minor_icon(ui::ImageModel::FromVectorIcon(
        features::IsRoundedIconsEnabled() ? kCircleFilledIcon
                                          : kTabGroupOldIcon,
        color_id, kIconSize));
  }

  return item;
}

RecentTabItem BuildWindowItem(const sessions::tab_restore::Window& window) {
  std::u16string label = l10n_util::GetPluralStringFUTF16(
      IDS_RECENTLY_CLOSED_WINDOW, static_cast<int>(window.tabs.size()));

  RecentTabItem window_item(RecentTabItem::Type::kWindow, label);
  window_item.set_session_id(window.id);
  window_item.set_icon(CreateFavicon(
      features::IsRoundedIconsEnabled() ? kTabIcon : kTabOldIcon));

  RecentTabItem restore_cmd(RecentTabItem::Type::kCommand,
                            l10n_util::GetStringUTF16(IDS_RESTORE_WINDOW));
  restore_cmd.set_session_id(window.id);
  restore_cmd.set_icon(ui::ImageModel::FromVectorIcon(
      features::IsRoundedIconsEnabled() ? vector_icons::kOpenInNewFlippableIcon
                                        : vector_icons::kLaunchOldIcon,
      ui::kColorMenuIcon, gfx::kFaviconSize));
  window_item.add_child(std::move(restore_cmd));

  for (const auto& tab : window.tabs) {
    window_item.add_child(BuildTabItem(*tab));
  }

  return window_item;
}

RecentTabItem BuildGroupItem(const sessions::tab_restore::Group& group) {
  std::u16string item_label =
      GetGroupItemLabel(group.visual_data.title(), group.tabs.size());
  const ui::ColorId color_id =
      GetTabGroupContextMenuColorId(group.visual_data.color());
  ui::ImageModel group_icon = ui::ImageModel::FromVectorIcon(
      features::IsRoundedIconsEnabled() ? kCircleFilledIcon : kTabGroupOldIcon,
      color_id, gfx::kFaviconSize);

  RecentTabItem group_item(RecentTabItem::Type::kGroup, item_label);
  group_item.set_session_id(group.id);
  group_item.set_icon(group_icon);

  RecentTabItem restore_cmd(RecentTabItem::Type::kCommand,
                            l10n_util::GetStringUTF16(IDS_RESTORE_GROUP));
  restore_cmd.set_session_id(group.id);
  restore_cmd.set_icon(ui::ImageModel::FromVectorIcon(
      features::IsRoundedIconsEnabled() ? vector_icons::kOpenInNewFlippableIcon
                                        : vector_icons::kLaunchOldIcon,
      ui::kColorMenuIcon, gfx::kFaviconSize));
  group_item.add_child(std::move(restore_cmd));

  for (const auto& tab : group.tabs) {
    group_item.add_child(BuildTabItem(*tab));
  }

  return group_item;
}

RecentTabItem BuildSplitItem(const sessions::tab_restore::Split& split) {
  std::u16string item_label =
      l10n_util::GetStringUTF16(IDS_RECENTLY_CLOSED_SPLIT);

  const gfx::VectorIcon* icon = nullptr;
  if (split.visual_data.split_layout() ==
      split_tabs::SplitTabLayout::kStacked) {
    icon = &kSplitScene2Icon;
  } else {
    icon = &(features::IsRoundedIconsEnabled() ? kSplitSceneIcon
                                               : kSplitSceneOldIcon);
  }

  RecentTabItem split_item(RecentTabItem::Type::kSplit, item_label);
  split_item.set_session_id(split.id);
  split_item.set_icon(CreateFavicon(*icon));

  RecentTabItem restore_cmd(RecentTabItem::Type::kCommand,
                            l10n_util::GetStringUTF16(IDS_RESTORE_SPLIT));
  restore_cmd.set_session_id(split.id);
  restore_cmd.set_icon(ui::ImageModel::FromVectorIcon(
      vector_icons::kLaunchOldIcon, ui::kColorMenuIcon, gfx::kFaviconSize));
  split_item.add_child(std::move(restore_cmd));

  for (const auto& tab : split.tabs) {
    split_item.add_child(BuildTabItem(*tab));
  }

  return split_item;
}

}  // namespace

RecentTabItem::RecentTabItem(Type type, std::u16string title)
    : type_(type), title_(std::move(title)) {}
RecentTabItem::RecentTabItem(const RecentTabItem&) = default;
RecentTabItem::RecentTabItem(RecentTabItem&&) = default;
RecentTabItem& RecentTabItem::operator=(const RecentTabItem&) = default;
RecentTabItem& RecentTabItem::operator=(RecentTabItem&&) = default;
RecentTabItem::~RecentTabItem() = default;

std::vector<RecentTabItem> RecentTabsBuilder::BuildRecentTabs(
    Profile* profile,
    BrowserWindowFeatures* feature) {
  std::vector<RecentTabItem> items;
  if (!profile || !feature) {
    return items;
  }

  auto history_items = BuildHistoryEntries(profile, feature);
  items.insert(items.end(), std::make_move_iterator(history_items.begin()),
               std::make_move_iterator(history_items.end()));

  auto local_items = BuildLocalEntries(profile);
  items.insert(items.end(), std::make_move_iterator(local_items.begin()),
               std::make_move_iterator(local_items.end()));

  return items;
}

std::vector<RecentTabItem> RecentTabsBuilder::BuildHistoryEntries(
    Profile* profile,
    BrowserWindowFeatures* feature) {
  std::vector<RecentTabItem> items;
  if (!profile || !feature) {
    return items;
  }

  RecentTabItem history(RecentTabItem::Type::kCommand,
                        l10n_util::GetStringUTF16(IDS_HISTORY_SHOW_HISTORY));
  history.set_action_id(kActionShowHistory);
  history.set_icon(ui::ImageModel::FromVectorIcon(
      features::IsRoundedIconsEnabled()
          ? vector_icons::kHistoryIcon
          : vector_icons::kHistoryChromeRefreshOldIcon,
      ui::kColorMenuIcon, gfx::kFaviconSize));
  items.push_back(std::move(history));

  if (feature->side_panel_ui()) {
    if (HistoryClustersSidePanelCoordinator::IsSupported(profile)) {
      RecentTabItem clusters(
          RecentTabItem::Type::kCommand,
          l10n_util::GetStringUTF16(IDS_HISTORY_CLUSTERS_SHOW_SIDE_PANEL));
      clusters.set_action_id(kActionSidePanelShowHistoryCluster);
      clusters.set_icon(ui::ImageModel::FromVectorIcon(
          features::IsRoundedIconsEnabled()
              ? vector_icons::kHistoryIcon
              : vector_icons::kHistoryChromeRefreshOldIcon,
          ui::kColorMenuIcon, gfx::kFaviconSize));
      items.push_back(std::move(clusters));
    }
  }

  return items;
}

std::vector<RecentTabItem> RecentTabsBuilder::BuildLocalEntries(
    Profile* profile) {
  std::vector<RecentTabItem> items;
  if (!profile) {
    return items;
  }

  sessions::TabRestoreService* service =
      TabRestoreServiceFactory::GetForProfile(profile);

  items.emplace_back(RecentTabItem::Type::kHeader,
                     l10n_util::GetStringUTF16(IDS_RECENT_TABS));

  int added_count = 0;
  for (const auto& entry : service->entries()) {
    if (added_count == kMaxLocalEntries) {
      break;
    }
    switch (entry->type) {
      case sessions::tab_restore::Type::TAB: {
        const auto& tab =
            static_cast<const sessions::tab_restore::Tab&>(*entry);
        items.push_back(BuildTabItem(tab));
        break;
      }
      case sessions::tab_restore::Type::WINDOW: {
        const auto& window =
            static_cast<const sessions::tab_restore::Window&>(*entry);
        items.push_back(BuildWindowItem(window));
        break;
      }
      case sessions::tab_restore::Type::GROUP: {
        const auto& group =
            static_cast<const sessions::tab_restore::Group&>(*entry);
        items.push_back(BuildGroupItem(group));
        break;
      }
      case sessions::tab_restore::Type::SPLIT: {
        const auto& split =
            static_cast<const sessions::tab_restore::Split&>(*entry);
        items.push_back(BuildSplitItem(split));
        break;
      }
    }
    ++added_count;
  }

  return items;
}
