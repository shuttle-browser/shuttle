// Copyright 2026 Silkweave
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TASK_MANAGER_AGENT_USAGE_TABLE_MODEL_H_
#define CHROME_BROWSER_UI_TASK_MANAGER_AGENT_USAGE_TABLE_MODEL_H_

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "chrome/browser/shuttle/agent_usage.h"
#include "ui/base/models/table_model.h"

namespace shuttle {
class ShuttleService;
}

namespace ui {
class TableModelObserver;
}

namespace task_manager {

// The Task Manager's "Agents" tab: one row per Shuttle agent with processes
// or open sessions, plus the user's tabs ("You") and the shared browser
// processes. Numbers come from shuttle::AgentUsageService.
class AgentUsageTableModel : public ui::TableModel,
                             public shuttle::AgentUsageService::Observer,
                             public ProfileObserver {
 public:
  // Column ids. Small numbers, far from the IDS_TASK_MANAGER_* column ids of
  // TaskManagerTableModel, so nothing mixes the two up.
  enum ColumnId {
    kNameColumn = 1,
    kMemoryColumn,
    kCpuColumn,
    kNetworkColumn,
    kProcessesColumn,
    kTabsColumn,
  };

  // Returns null if `profile` is null or has no Shuttle service.
  // `on_rows_changed` runs after each update of the rows.
  static std::unique_ptr<AgentUsageTableModel> CreateForProfile(
      Profile* profile,
      base::RepeatingClosure on_rows_changed);

  AgentUsageTableModel(Profile* profile,
                       base::RepeatingClosure on_rows_changed);
  AgentUsageTableModel(const AgentUsageTableModel&) = delete;
  AgentUsageTableModel& operator=(const AgentUsageTableModel&) = delete;
  ~AgentUsageTableModel() override;

  // The columns, titled and sortable.
  static std::vector<ui::TableColumn> GetColumns();

  // Samples (and updates the rows) only while active.
  void SetActive(bool active);
  // Keeps the rows whose name matches `search_terms` (all if empty).
  void SetSearchTerms(std::u16string_view search_terms);

  // Whether `row` is an agent with open sessions.
  bool CanCloseSessions(size_t row) const;
  // Closes the sessions of the agent in `row` (its storage stays).
  void CloseSessions(size_t row);

  // ui::TableModel:
  size_t RowCount() const override;
  std::u16string GetText(size_t row, int column_id) const override;
  void SetObserver(ui::TableModelObserver* observer) override;
  int CompareValues(size_t row1, size_t row2, int column_id) const override;

  // shuttle::AgentUsageService::Observer:
  void OnAgentUsageRefreshed() override;

  // ProfileObserver:
  void OnProfileWillBeDestroyed(Profile* profile) override;

 private:
  enum class Kind { kAgent, kUser, kShared };

  struct Row {
    Kind kind = Kind::kAgent;
    // Agents only.
    std::string agent_id;
    std::u16string name;
    shuttle::AgentUsage usage;
    bool has_sessions = false;
  };

  // The service, or null once the profile is going away.
  shuttle::ShuttleService* service() const;

  // Recomputes the rows, keeping the rows that stay in place (so the table
  // keeps its selection), and tells the observer.
  void UpdateRows();

  raw_ptr<Profile> profile_;
  base::RepeatingClosure on_rows_changed_;
  raw_ptr<ui::TableModelObserver> observer_ = nullptr;
  std::unique_ptr<shuttle::AgentUsageService::Subscription> subscription_;
  std::u16string search_terms_;
  std::vector<Row> rows_;
  base::ScopedObservation<Profile, ProfileObserver> profile_observation_{this};
};

}  // namespace task_manager

#endif  // CHROME_BROWSER_UI_TASK_MANAGER_AGENT_USAGE_TABLE_MODEL_H_
