// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab/tab_accessibility.h"

#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/tabs/alert/tab_alert_controller.h"
#include "chrome/browser/ui/tabs/tab_data.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/tab_strip_region_view.h"
#include "chrome/browser/ui/views/permissions/chip/chip_controller.h"
#include "chrome/browser/ui/views/tabs/hovercard/tab_hover_card_controller.h"
#include "chrome/browser/ui/window_metadata/window_metadata_controller.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tabs/public/split_tab_data.h"
#include "components/tabs/public/tab_group.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/text/bytes_formatting.h"

namespace {
int GetAccessibleTabLabelFormatStringForSplit(split_tabs::SplitTabLayout layout,
                                              int tab_index_in_split) {
  switch (layout) {
    case split_tabs::SplitTabLayout::kSideBySide:
      switch (tab_index_in_split) {
        case 0:
          return IDS_TAB_AX_LABEL_SPLIT_TAB_LEFT_VIEW_FORMAT;
        case 1:
          return IDS_TAB_AX_LABEL_SPLIT_TAB_RIGHT_VIEW_FORMAT;
        default:
          NOTREACHED();
      }
    case split_tabs::SplitTabLayout::kStacked:
      switch (tab_index_in_split) {
        case 0:
          return IDS_TAB_AX_LABEL_SPLIT_TAB_TOP_VIEW_FORMAT;
        case 1:
          return IDS_TAB_AX_LABEL_SPLIT_TAB_BOTTOM_VIEW_FORMAT;
        default:
          NOTREACHED();
      }
  }
}
}  // namespace

namespace tabs {

bool ShouldUpdateAccessibleName(const TabData& old_data,
                                const TabData& new_data) {
  return ((old_data.network_state != new_data.network_state) ||
          old_data.is_crashed != new_data.is_crashed ||
          old_data.alert_state != new_data.alert_state ||
          old_data.should_show_discard_status !=
              new_data.should_show_discard_status ||
          old_data.discarded_memory_savings !=
              new_data.discarded_memory_savings ||
          old_data.pinned != new_data.pinned ||
          old_data.title != new_data.title);
}

std::u16string GetAccessibleTabLabel(const TabInterface* tab, bool is_for_tab) {
  const BrowserWindowInterface* bwi = tab->GetBrowserWindowInterface();
  if (!bwi) {
    return std::u16string();
  }

  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(bwi);
  const TabStripModel* tab_strip_model = bwi->GetTabStripModel();

  const TabData& tab_data =
      browser_view->tab_strip_view()->GetTabData(tab->GetHandle());

  auto* controller = WindowMetadataController::From(bwi);

  std::u16string title =
      is_for_tab ? controller->GetTitleForTab(tab->GetHandle())
                 : controller->GetWindowTitleForTab(tab->GetHandle());

  if (const std::optional<split_tabs::SplitTabId> split = tab->GetSplit()) {
    if (!tab_strip_model->ContainsSplit(split.value())) {
      // This can happen when a split tab is in the process of being closed and
      // the accessible title needs to be updated.
      return std::u16string();
    }

    const split_tabs::SplitTabData* split_data =
        tab_strip_model->GetSplitData(split.value());
    const std::vector<tabs::TabInterface*> tabs_in_split =
        split_data->ListTabs();

    int tab_index_in_split = std::distance(
        tabs_in_split.begin(),
        std::find(tabs_in_split.begin(), tabs_in_split.end(), tab));
    title = l10n_util::GetStringFUTF16(
        GetAccessibleTabLabelFormatStringForSplit(
            split_data->visual_data()->split_layout(), tab_index_in_split),
        title);
  }

  if (const std::optional<tab_groups::TabGroupId> group = tab->GetGroup()) {
    std::u16string group_title = tab_strip_model->group_model()
                                     ->GetTabGroup(group.value())
                                     ->visual_data()
                                     ->title();
    if (group_title.empty()) {
      title = l10n_util::GetStringFUTF16(IDS_TAB_AX_LABEL_UNNAMED_GROUP_FORMAT,
                                         title);
    } else {
      title = l10n_util::GetStringFUTF16(IDS_TAB_AX_LABEL_NAMED_GROUP_FORMAT,
                                         title, group_title);
    }
  }

  // Tab is pinned.
  if (tab->IsPinned()) {
    title = l10n_util::GetStringFUTF16(IDS_TAB_AX_LABEL_PINNED_FORMAT, title);
  }

  // Tab has crashed.
  if (tab_data.is_crashed) {
    return l10n_util::GetStringFUTF16(IDS_TAB_AX_LABEL_CRASHED_FORMAT, title);
  }

  // Network error interstitial.
  if (tab_data.network_state == tabs::TabNetworkState::kError) {
    return l10n_util::GetStringFUTF16(IDS_TAB_AX_LABEL_NETWORK_ERROR_FORMAT,
                                      title);
  }

  // Tab has a pending permission request.
  LocationBar* location_bar = browser_view->GetLocationBar();
  if (location_bar && location_bar->GetChipController() &&
      location_bar->GetChipController()->IsPermissionPromptChipVisible()) {
    return l10n_util::GetStringFUTF16(
        IDS_TAB_AX_LABEL_PERMISSION_REQUESTED_FORMAT, title);
  }

  // Alert tab states.
  const tabs::TabAlertController* tab_alert_controller =
      tabs::TabAlertController::From(tab);

  // During tab destruction, the alert controller may already be destroyed
  // by the time accessibility name updates are processed.
  if (tab_alert_controller) {
    if (const std::optional<tabs::TabAlert> alert =
            tab_alert_controller->GetAlertToShow()) {
      title = l10n_util::GetStringFUTF16(
          tabs::TabAlertController::GetAccessibleAlertStringId(alert.value()),
          title);
    }
  }

  if (tab_data.should_show_discard_status) {
    title = l10n_util::GetStringFUTF16(IDS_TAB_AX_INACTIVE_TAB, title);
    std::optional<base::ByteSize> discarded_memory_savings =
        tab_data.discarded_memory_savings;
    if (discarded_memory_savings.has_value() &&
        discarded_memory_savings.value().is_positive()) {
      title = l10n_util::GetStringFUTF16(
          IDS_TAB_AX_MEMORY_SAVINGS, title,
          ui::FormatBytes(discarded_memory_savings.value()));
    }
  }

  return title;
}

}  // namespace tabs
