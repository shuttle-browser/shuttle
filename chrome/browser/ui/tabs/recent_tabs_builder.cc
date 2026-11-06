// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/recent_tabs_builder.h"

#include <algorithm>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/ui/accelerator_table.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/tabs/recent_tabs_sub_menu_model.h"
#include "chrome/browser/ui/tabs/tab_group_theme.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/grit/generated_resources.h"
#include "components/history/core/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/sessions/core/session_types.h"
#include "components/sessions/core/tab_restore_service.h"
#include "components/split_tabs/split_tab_id.h"
#include "components/split_tabs/split_tab_visual_data.h"
#include "components/strings/grit/components_strings.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/navigation_entry.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/favicon_size.h"

namespace {

constexpr int kMaxLocalEntries = 8;

using TabIterator =
    std::vector<std::unique_ptr<sessions::tab_restore::Tab>>::const_iterator;

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

// Returns the title to show for a tab, falling back to the URL.
std::u16string GetTabTitle(
    const sessions::SerializedNavigationEntry& current_navigation) {
  return current_navigation.title().empty()
             ? base::UTF8ToUTF16(current_navigation.virtual_url().spec())
             : current_navigation.title();
}

RecentTabItem BuildTabItem(const sessions::tab_restore::Tab& tab,
                           bool show_group_minor_icon = false) {
  const sessions::SerializedNavigationEntry& current_navigation =
      tab.navigations.at(tab.normalized_navigation_index());

  RecentTabItem item(RecentTabItem::Type::kTab,
                     GetTabTitle(current_navigation));
  item.set_session_id(tab.id);
  item.set_url(current_navigation.virtual_url());
  item.set_icon(favicon::GetDefaultFaviconModel());
  item.set_is_local(true);

  if (show_group_minor_icon && tab.group_visual_data.has_value() &&
      !tab.is_group_ephemeral) {
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

  RecentTabItem restore_cmd(RecentTabItem::Type::kSplit,
                            l10n_util::GetStringUTF16(IDS_RESTORE_SPLIT));
  restore_cmd.set_session_id(split.id);
  restore_cmd.set_icon(ui::ImageModel::FromVectorIcon(
      features::IsRoundedIconsEnabled() ? vector_icons::kOpenInNewFlippableIcon
                                        : vector_icons::kLaunchOldIcon,
      ui::kColorMenuIcon, gfx::kFaviconSize));
  split_item.add_child(std::move(restore_cmd));
  split_item.add_child(RecentTabItem(RecentTabItem::Type::kDivider, u""));

  for (const auto& tab : split.tabs) {
    split_item.add_child(BuildTabItem(*tab));
  }

  return split_item;
}

void BuildAndAddSplit(
    RecentTabItem& parent,
    TabIterator& it,
    TabIterator end,
    const std::map<split_tabs::SplitTabId,
                   std::unique_ptr<sessions::tab_restore::Split>>& split_tabs) {
  const split_tabs::SplitTabId split_id = (*it)->split_id.value();
  auto split_run_end = std::find_if(it, end, [&](const auto& t) {
    return !t->split_id.has_value() || t->split_id.value() != split_id;
  });
  CHECK(split_tabs.contains(split_id));
  const sessions::tab_restore::Split& split = *split_tabs.at(split_id);
  RecentTabItem split_item = BuildSplitItem(split);
  while (it != split_run_end) {
    split_item.add_child(BuildTabItem(**it));
    ++it;
  }
  parent.add_child(std::move(split_item));
}

RecentTabItem BuildStandaloneGroupItem(
    const sessions::tab_restore::Group& group,
    std::optional<size_t> tab_count_override = std::nullopt) {
  const size_t num_tabs = tab_count_override.value_or(group.tabs.size());
  std::u16string item_label =
      GetGroupItemLabel(group.visual_data.title(), num_tabs);
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
  group_item.add_child(RecentTabItem(RecentTabItem::Type::kDivider, u""));

  auto it = group.tabs.begin();
  const auto end = group.tabs.end();
  while (it != end) {
    if ((*it)->split_id.has_value()) {
      BuildAndAddSplit(group_item, it, end, group.split_tabs);
    } else {
      group_item.add_child(BuildTabItem(**it));
      ++it;
    }
  }

  return group_item;
}

void BuildAndAddWindowGroup(
    RecentTabItem& parent,
    TabIterator& it,
    TabIterator end,
    const std::map<tab_groups::TabGroupId,
                   std::unique_ptr<sessions::tab_restore::Group>>& tab_groups,
    const std::map<split_tabs::SplitTabId,
                   std::unique_ptr<sessions::tab_restore::Split>>& split_tabs) {
  const tab_groups::TabGroupId group_id = (*it)->group.value();
  auto group_run_end = std::find_if(it, end, [&](const auto& t) {
    return !t->group.has_value() || t->group.value() != group_id;
  });
  CHECK(tab_groups.contains(group_id));
  const sessions::tab_restore::Group& group = *tab_groups.at(group_id);
  const size_t num_tabs = static_cast<size_t>(std::distance(it, group_run_end));
  RecentTabItem group_item = BuildStandaloneGroupItem(group, num_tabs);
  while (it != group_run_end) {
    if ((*it)->split_id.has_value()) {
      BuildAndAddSplit(group_item, it, group_run_end, split_tabs);
    } else {
      group_item.add_child(BuildTabItem(**it));
      ++it;
    }
  }
  parent.add_child(std::move(group_item));
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
  window_item.add_child(RecentTabItem(RecentTabItem::Type::kDivider, u""));

  auto it = window.tabs.begin();
  const auto end = window.tabs.end();
  while (it != end) {
    if ((*it)->group.has_value()) {
      BuildAndAddWindowGroup(window_item, it, end, window.tab_groups,
                             window.split_tabs);
    } else if ((*it)->split_id.has_value()) {
      BuildAndAddSplit(window_item, it, end, window.split_tabs);
    } else {
      window_item.add_child(BuildTabItem(**it));
      ++it;
    }
  }

  return window_item;
}

RecentTabItem BuildEntryItem(const sessions::tab_restore::Entry& entry) {
  switch (entry.type) {
    case sessions::tab_restore::Type::TAB:
      return BuildTabItem(static_cast<const sessions::tab_restore::Tab&>(entry),
                          /*show_group_minor_icon=*/true);
    case sessions::tab_restore::Type::WINDOW:
      return BuildWindowItem(
          static_cast<const sessions::tab_restore::Window&>(entry));
    case sessions::tab_restore::Type::GROUP:
      return BuildStandaloneGroupItem(
          static_cast<const sessions::tab_restore::Group&>(entry));
    case sessions::tab_restore::Type::SPLIT:
      return BuildSplitItem(
          static_cast<const sessions::tab_restore::Split&>(entry));
  }
  NOTREACHED();
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
    BrowserWindowInterface* browser) {
  std::vector<RecentTabItem> items;
  if (!profile || !browser) {
    return items;
  }

  auto history_items = BuildHistoryEntries(profile, browser);
  items.insert(items.end(), std::make_move_iterator(history_items.begin()),
               std::make_move_iterator(history_items.end()));

  // When history saving is disabled by policy, don't populate local recently
  // closed tabs or synced tabs from other devices.
  if (profile->GetPrefs()->GetBoolean(prefs::kSavingBrowserHistoryDisabled)) {
    return items;
  }

  auto local_items = BuildLocalEntries(profile);
  if (!local_items.empty()) {
    items.emplace_back(RecentTabItem::Type::kDivider, u"");
    items.insert(items.end(), std::make_move_iterator(local_items.begin()),
                 std::make_move_iterator(local_items.end()));
  }

  return items;
}

std::vector<RecentTabItem> RecentTabsBuilder::BuildHistoryEntries(
    Profile* profile,
    BrowserWindowInterface* browser) {
  std::vector<RecentTabItem> items;
  if (!profile || !browser) {
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

  ui::Accelerator accelerator;
  const bool has_accelerator =
      GetAcceleratorForCommandId(IDC_RESTORE_TAB, &accelerator);

  const bool is_empty = !service || service->entries().empty();

  RecentTabItem header_item(
      is_empty ? RecentTabItem::Type::kCommand : RecentTabItem::Type::kHeader,
      l10n_util::GetStringUTF16(IDS_RECENT_TABS));

  if (is_empty) {
    header_item.set_enabled(false);
    if (has_accelerator) {
      header_item.set_accelerator(accelerator);
    }
  }

  items.push_back(std::move(header_item));

  if (!service) {
    return items;
  }

  int added_count = 0;
  for (const auto& entry : service->entries()) {
    if (added_count == kMaxLocalEntries) {
      break;
    }

    RecentTabItem item = BuildEntryItem(*entry);

    if (added_count == 0 && has_accelerator) {
      if (!item.children().empty()) {
        item.children().front().set_accelerator(accelerator);
      } else {
        item.set_accelerator(accelerator);
        item.set_minor_icon(ui::ImageModel());
      }
    }

    items.push_back(std::move(item));
    ++added_count;
  }

  return items;
}
