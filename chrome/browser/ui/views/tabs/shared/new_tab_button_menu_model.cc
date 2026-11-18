// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/shared/new_tab_button_menu_model.h"

#include <algorithm>
#include <optional>
#include <string>

#include "base/metrics/user_metrics.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/shuttle/shuttle_service.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/split_tab_metrics.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/grit/generated_resources.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tabs/public/tab_group.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/l10n/l10n_util.h"

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(NewTabButtonMenuModel, kNewTab);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(NewTabButtonMenuModel, kNewTabInGroup);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(NewTabButtonMenuModel, kNewSplitView);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(NewTabButtonMenuModel,
                                      kCreateNewTabGroup);

namespace {
// Command ids of the "New tab in <agent>" items (clear of the IDC_ range).
constexpr int kAgentCommandBase = 60000;
// Agents listed in the menu itself; the rest go under "More agents".
constexpr size_t kMaxAgentsInMenu = 8;
}  // namespace

NewTabButtonMenuModel::NewTabButtonMenuModel(BrowserWindowInterface* browser)
    : ui::SimpleMenuModel(this), browser_(browser) {
  CHECK(browser_);
  TabStripModel* const tab_strip_model = browser_->GetTabStripModel();
  CHECK(tab_strip_model);

  // Build the menu.
  AddItemWithStringId(IDC_NEW_TAB, IDS_NEW_TAB_BUTTON_CXMENU_NEW_TAB);
  SetElementIdentifierAt(GetIndexOfCommandId(IDC_NEW_TAB).value(), kNewTab);
  // Shuttle: tab groups are agents, so "in group" is "in agent".
  AddNewTabInAgentItems();

  // Shuttle: no "New tab group"; tab groups are agents, made over MCP.

  AddSeparator(ui::NORMAL_SEPARATOR);
  AddNewSplitTabItem();
}

NewTabButtonMenuModel::~NewTabButtonMenuModel() = default;

void NewTabButtonMenuModel::ExecuteCommand(int command_id, int event_flags) {
  CHECK(browser_);

  if (command_id >= kAgentCommandBase &&
      static_cast<size_t>(command_id - kAgentCommandBase) < agent_ids_.size()) {
    base::RecordAction(
        base::UserMetricsAction("NewTabButton_ContextMenu_NewTabInAgent"));
    if (auto* agents = shuttle::ShuttleService::Get(browser_->GetProfile())) {
      agents->OpenNewTabInAgent(agent_ids_[command_id - kAgentCommandBase],
                                {.browser = browser_});
    }
    return;
  }

  switch (command_id) {
    case IDC_NEW_TAB:
      base::RecordAction(
          base::UserMetricsAction("NewTabButton_ContextMenu_NewTab"));
      break;
    case IDC_ADD_NEW_TAB_RECENT_GROUP:
      base::RecordAction(
          base::UserMetricsAction("NewTabButton_ContextMenu_NewTabInGroup"));
      break;
    case IDC_CREATE_NEW_TAB_GROUP:
      base::RecordAction(
          base::UserMetricsAction("NewTabButton_ContextMenu_NewGroup"));
      break;
    case IDC_NEW_SPLIT_TAB:
      base::RecordAction(
          base::UserMetricsAction("NewTabButton_ContextMenu_NewSplitTab"));
      break;
  }

  if (command_id == IDC_NEW_SPLIT_TAB) {
    // Handle this command directly because we want to specify the source
    // as the new tab button.
    chrome::NewSplitTab(browser_, split_tabs::SplitTabLayout::kSideBySide,
                        split_tabs::SplitTabCreatedSource::kNewTabButton);
    return;
  }

  chrome::ExecuteCommand(browser_, command_id);
}

bool NewTabButtonMenuModel::GetAcceleratorForCommandId(
    int command_id,
    ui::Accelerator* accelerator) const {
  CHECK(browser_);
  if (command_id < 0 || command_id >= kAgentCommandBase - 1) {
    // A non-interactive title, or a Shuttle agent item (no accelerator).
    return false;
  }

  return BrowserWindow::FromBrowser(browser_)
      ->GetAcceleratorProvider()
      ->GetAcceleratorForCommandId(command_id, accelerator);
}

void NewTabButtonMenuModel::AddNewTabInAgentItems() {
  auto* agents = shuttle::ShuttleService::Get(browser_->GetProfile());
  if (!agents) {
    return;
  }
  // The agent of the tab being looked at goes first.
  const std::string current =
      agents->AgentOfTab(browser_->GetTabStripModel()->GetActiveWebContents());
  std::vector<shuttle::ShuttleService::AgentSummary> list =
      agents->ListAgents();
  std::stable_partition(list.begin(), list.end(),
                        [&](const auto& a) { return a.id == current; });
  if (list.empty()) {
    AddItem(IDC_ADD_NEW_TAB_RECENT_GROUP,
            l10n_util::GetStringUTF16(IDS_NEW_TAB_IN_GROUP_NO_GROUPS));
    SetEnabledAt(GetIndexOfCommandId(IDC_ADD_NEW_TAB_RECENT_GROUP).value(),
                 false);
    return;
  }
  for (size_t i = 0; i < list.size(); ++i) {
    // Agents without open tabs only show in "More agents", except the first.
    const bool in_menu = i < kMaxAgentsInMenu && (list[i].open || i == 0);
    const int id = kAgentCommandBase + static_cast<int>(agent_ids_.size());
    agent_ids_.push_back(list[i].id);
    const std::u16string label = l10n_util::GetStringFUTF16(
        IDS_NEW_TAB_IN_GROUP, base::UTF8ToUTF16(list[i].name));
    if (in_menu) {
      AddItem(id, label);
      if (list[i].id == current) {
        SetElementIdentifierAt(GetIndexOfCommandId(id).value(), kNewTabInGroup);
      }
    } else {
      if (!more_agents_) {
        more_agents_ = std::make_unique<ui::SimpleMenuModel>(this);
      }
      more_agents_->AddItem(id, base::UTF8ToUTF16(list[i].name));
    }
  }
  if (more_agents_) {
    AddSubMenu(kAgentCommandBase - 1, u"New tab in another agent",
               more_agents_.get());
  }
}

void NewTabButtonMenuModel::AddNewSplitTabItem() {
  AddItemWithStringId(IDC_NEW_SPLIT_TAB, IDS_TAB_CXMENU_NEW_SPLIT_WITH_CURRENT);
  SetElementIdentifierAt(GetIndexOfCommandId(IDC_NEW_SPLIT_TAB).value(),
                         kNewSplitView);

  TabStripModel* tab_strip_model = browser_->GetTabStripModel();
  CHECK(tab_strip_model);

  const tabs::TabInterface* const active_tab = tab_strip_model->GetActiveTab();
  bool is_active_tab_split = active_tab && active_tab->IsSplit();

  SetEnabledAt(GetIndexOfCommandId(IDC_NEW_SPLIT_TAB).value(),
               !is_active_tab_split);
}
