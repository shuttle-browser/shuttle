// Copyright 2026 Silkweave
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/task_manager/agent_usage_table_model.h"

#include <algorithm>
#include <iterator>
#include <utility>

#include "base/byte_size.h"
#include "base/check_op.h"
#include "base/i18n/number_formatting.h"
#include "base/i18n/rtl.h"
#include "base/i18n/string_search.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/shuttle/shuttle_service.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/table_model_observer.h"
#include "ui/base/text/bytes_formatting.h"

namespace task_manager {
namespace {

// Formatted as the task manager formats its own columns
// (TaskManagerValuesStringifier).
std::u16string MemoryText(base::ByteSize memory) {
#if BUILDFLAG(IS_MAC)
  return ui::FormatBytes(memory);
#else
  std::u16string text = base::FormatNumber(memory.InKiB());
  base::i18n::AdjustStringForLocaleDirection(&text);
  return l10n_util::GetStringFUTF16(IDS_TASK_MANAGER_MEM_CELL_TEXT, text);
#endif  // BUILDFLAG(IS_MAC)
}

std::u16string NetworkText(base::ByteSize network) {
  if (network.is_zero()) {
    return base::FormatNumber(0);
  }
  return base::i18n::GetDisplayStringInLTRDirectionality(
      ui::FormatSpeed(network));
}

template <typename T>
int Compare(const T& a, const T& b) {
  return a < b ? -1 : (b < a ? 1 : 0);
}

ui::TableColumn MakeColumn(int id,
                           std::u16string title,
                           ui::TableColumn::Alignment alignment,
                           float percent,
                           bool initial_sort_is_ascending) {
  ui::TableColumn column;
  column.id = id;
  column.title = std::move(title);
  column.alignment = alignment;
  column.width = -1;
  column.percent = percent;
  column.sortable = true;
  column.initial_sort_is_ascending = initial_sort_is_ascending;
  return column;
}

}  // namespace

// static
std::unique_ptr<AgentUsageTableModel> AgentUsageTableModel::CreateForProfile(
    Profile* profile,
    base::RepeatingClosure on_rows_changed) {
  if (!profile) {
    return nullptr;
  }
  profile = profile->GetOriginalProfile();
  if (!shuttle::ShuttleService::Get(profile)) {
    return nullptr;
  }
  return std::make_unique<AgentUsageTableModel>(profile,
                                                std::move(on_rows_changed));
}

AgentUsageTableModel::AgentUsageTableModel(
    Profile* profile,
    base::RepeatingClosure on_rows_changed)
    : profile_(profile), on_rows_changed_(std::move(on_rows_changed)) {
  profile_observation_.Observe(profile_);
}

AgentUsageTableModel::~AgentUsageTableModel() = default;

// static
std::vector<ui::TableColumn> AgentUsageTableModel::GetColumns() {
  // Same headers as the task manager's columns where one fits.
  return {
      MakeColumn(kNameColumn, u"Agent", ui::TableColumn::LEFT, 1.0f,
                 /*initial_sort_is_ascending=*/true),
      MakeColumn(
          kMemoryColumn,
          l10n_util::GetStringUTF16(IDS_TASK_MANAGER_MEM_FOOTPRINT_COLUMN),
          ui::TableColumn::RIGHT, 0, false),
      MakeColumn(kCpuColumn,
                 l10n_util::GetStringUTF16(IDS_TASK_MANAGER_CPU_COLUMN),
                 ui::TableColumn::RIGHT, 0, false),
      MakeColumn(kNetworkColumn,
                 l10n_util::GetStringUTF16(IDS_TASK_MANAGER_NET_COLUMN),
                 ui::TableColumn::RIGHT, 0, false),
      MakeColumn(kProcessesColumn, u"Processes", ui::TableColumn::RIGHT, 0,
                 false),
      MakeColumn(kTabsColumn, u"Tabs", ui::TableColumn::RIGHT, 0, false),
  };
}

void AgentUsageTableModel::SetActive(bool active) {
  if (active == !!subscription_) {
    return;
  }
  if (!active) {
    subscription_.reset();
    return;
  }
  shuttle::ShuttleService* shuttle_service = service();
  if (!shuttle_service) {
    return;
  }
  subscription_ = shuttle_service->usage().Subscribe(this);
  UpdateRows();
}

void AgentUsageTableModel::SetSearchTerms(std::u16string_view search_terms) {
  if (search_terms_ == search_terms) {
    return;
  }
  search_terms_ = std::u16string(search_terms);
  UpdateRows();
}

bool AgentUsageTableModel::CanCloseSessions(size_t row) const {
  return row < rows_.size() && rows_[row].kind == Kind::kAgent &&
         rows_[row].has_sessions;
}

void AgentUsageTableModel::CloseSessions(size_t row) {
  shuttle::ShuttleService* shuttle_service = service();
  if (!shuttle_service || !CanCloseSessions(row)) {
    return;
  }
  // Copy: UpdateRows() below may drop the row.
  const std::string agent_id = rows_[row].agent_id;
  shuttle_service->CloseAgentSessions(agent_id);
  UpdateRows();
}

size_t AgentUsageTableModel::RowCount() const {
  return rows_.size();
}

std::u16string AgentUsageTableModel::GetText(size_t row, int column_id) const {
  CHECK_LT(row, rows_.size());
  const Row& r = rows_[row];
  if (column_id == kNameColumn) {
    return r.name;
  }
  shuttle::ShuttleService* shuttle_service = service();
  if (!shuttle_service || !shuttle_service->usage().has_sample()) {
    return l10n_util::GetStringUTF16(IDS_TASK_MANAGER_NA_CELL_TEXT);
  }
  switch (column_id) {
    case kMemoryColumn:
      return MemoryText(r.usage.memory);
    case kCpuColumn:
      // The task manager's first CPU calculation has nothing to compare with.
      if (!shuttle_service->usage().cpu_ready()) {
        return l10n_util::GetStringUTF16(IDS_TASK_MANAGER_NA_CELL_TEXT);
      }
      return base::FormatDouble(r.usage.cpu, /*min_fractional_digits=*/1,
                                /*max_fractional_digits=*/1);
    case kNetworkColumn:
      return NetworkText(r.usage.network);
    case kProcessesColumn:
      return base::FormatNumber(r.usage.processes);
    case kTabsColumn:
      return base::FormatNumber(r.usage.tabs);
  }
  NOTREACHED();
}

void AgentUsageTableModel::SetObserver(ui::TableModelObserver* observer) {
  observer_ = observer;
}

int AgentUsageTableModel::CompareValues(size_t row1,
                                        size_t row2,
                                        int column_id) const {
  CHECK_LT(row1, rows_.size());
  CHECK_LT(row2, rows_.size());
  const shuttle::AgentUsage& a = rows_[row1].usage;
  const shuttle::AgentUsage& b = rows_[row2].usage;
  switch (column_id) {
    case kMemoryColumn:
      return Compare(a.memory.InBytes(), b.memory.InBytes());
    case kCpuColumn:
      return Compare(a.cpu, b.cpu);
    case kNetworkColumn:
      return Compare(a.network.InBytes(), b.network.InBytes());
    case kProcessesColumn:
      return Compare(a.processes, b.processes);
    case kTabsColumn:
      return Compare(a.tabs, b.tabs);
    default:
      // The name: a locale-aware string compare.
      return ui::TableModel::CompareValues(row1, row2, column_id);
  }
}

void AgentUsageTableModel::OnAgentUsageRefreshed() {
  UpdateRows();
}

void AgentUsageTableModel::OnProfileWillBeDestroyed(Profile* profile) {
  subscription_.reset();
  profile_observation_.Reset();
  profile_ = nullptr;
  UpdateRows();
}

shuttle::ShuttleService* AgentUsageTableModel::service() const {
  return profile_ ? shuttle::ShuttleService::Get(profile_) : nullptr;
}

void AgentUsageTableModel::UpdateRows() {
  // The rows as they should be now, in their initial order: you, the shared
  // processes, then the agents (those with open sessions first, by name).
  std::vector<Row> wanted;
  if (shuttle::ShuttleService* shuttle_service = service()) {
    const shuttle::AgentUsageService& usage = shuttle_service->usage();
    wanted.push_back(
        {.kind = Kind::kUser, .name = u"You", .usage = usage.user()});
    wanted.push_back({.kind = Kind::kShared,
                      .name = u"Browser (shared)",
                      .usage = usage.shared()});
    for (const shuttle::ShuttleService::AgentSummary& agent :
         shuttle_service->ListAgents()) {
      const auto it = usage.agents().find(agent.id);
      const bool has_processes =
          it != usage.agents().end() && it->second.processes > 0;
      // Idle agents (no processes, no sessions) aren't interesting.
      if (!has_processes && !agent.open) {
        continue;
      }
      wanted.push_back({.kind = Kind::kAgent,
                        .agent_id = agent.id,
                        .name = base::UTF8ToUTF16(agent.name),
                        .usage = it != usage.agents().end()
                                     ? it->second
                                     : shuttle::AgentUsage(),
                        .has_sessions = agent.open});
    }
  }
  if (!search_terms_.empty()) {
    std::erase_if(wanted, [this](const Row& row) {
      return !base::i18n::StringSearchIgnoringCaseAndAccents(
          search_terms_, row.name, /*match_index=*/nullptr,
          /*match_length=*/nullptr);
    });
  }

  auto same_row = [](const Row& a, const Row& b) {
    return a.kind == b.kind && a.agent_id == b.agent_id;
  };

  // Drop the rows that went (one at a time, so the table keeps track of its
  // selection), update the ones that stay, append the new ones.
  for (size_t i = rows_.size(); i-- > 0;) {
    const bool stays = std::ranges::any_of(
        wanted, [&](const Row& row) { return same_row(row, rows_[i]); });
    if (!stays) {
      rows_.erase(rows_.begin() + i);
      if (observer_) {
        observer_->OnItemsRemoved(i, 1);
      }
    }
  }
  std::vector<Row> added;
  for (Row& row : wanted) {
    auto it = std::ranges::find_if(
        rows_, [&](const Row& existing) { return same_row(existing, row); });
    if (it != rows_.end()) {
      *it = std::move(row);
    } else {
      added.push_back(std::move(row));
    }
  }
  if (observer_ && !rows_.empty()) {
    observer_->OnItemsChanged(0, rows_.size());
  }
  if (!added.empty()) {
    const size_t first_new = rows_.size();
    std::ranges::move(added, std::back_inserter(rows_));
    if (observer_) {
      observer_->OnItemsAdded(first_new, added.size());
    }
  }
  if (on_rows_changed_) {
    on_rows_changed_.Run();
  }
}

}  // namespace task_manager
