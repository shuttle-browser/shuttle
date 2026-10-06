// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data/core/counters/history_counter.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "components/browsing_data/core/pref_names.h"

namespace browsing_data {

HistoryCounter::HistoryCounter(history::HistoryService* history_service)
    : history_service_(history_service) {
  DCHECK(history_service_);
}

HistoryCounter::~HistoryCounter() = default;

bool HistoryCounter::HasTrackedTasksForTesting() {
  return cancelable_task_tracker_.HasTrackedTasks();
}

const char* HistoryCounter::GetPrefName() const {
  return browsing_data::prefs::kDeleteBrowsingHistory;
}

void HistoryCounter::Count() {
  // Reset the state.
  cancelable_task_tracker_.TryCancelAll();
  weak_ptr_factory_.InvalidateWeakPtrs();

  history_service_->GetUniqueDomainsVisited(
      GetPeriodStart(), GetPeriodEnd(),
      history::VisitQuery404sPolicy::kInclude404s,
      base::BindOnce(&HistoryCounter::OnGetUniqueDomains,
                     weak_ptr_factory_.GetWeakPtr()),
      &cancelable_task_tracker_);
}

void HistoryCounter::OnGetUniqueDomains(history::DomainsVisitedResult result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ReportResult(std::make_unique<HistoryResult>(
      this, result.all_visited_domains.size(),
      result.all_visited_domains.empty() ? std::string()
                                         : result.all_visited_domains.front()));
}

HistoryCounter::HistoryResult::HistoryResult(const HistoryCounter* source,
                                             ResultInt value,
                                             std::string last_visited_domain)
    : FinishedResult(source, value),
      last_visited_domain_(std::move(last_visited_domain)) {}

HistoryCounter::HistoryResult::~HistoryResult() = default;

}  // namespace browsing_data
