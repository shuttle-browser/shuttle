// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSING_DATA_CORE_COUNTERS_HISTORY_COUNTER_H_
#define COMPONENTS_BROWSING_DATA_CORE_COUNTERS_HISTORY_COUNTER_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/cancelable_task_tracker.h"
#include "components/browsing_data/core/counters/browsing_data_counter.h"
#include "components/history/core/browser/history_service.h"

namespace browsing_data {

class HistoryCounter : public browsing_data::BrowsingDataCounter {
 public:
  class HistoryResult : public FinishedResult {
   public:
    HistoryResult(const HistoryCounter* source,
                  ResultInt value,
                  std::string last_visited_domain);
    ~HistoryResult() override;

    const std::string& last_visited_domain() const {
      return last_visited_domain_;
    }

   private:
    std::string last_visited_domain_;
  };

  explicit HistoryCounter(history::HistoryService* history_service);
  ~HistoryCounter() override;

  // Whether there are counting tasks in progress. Only used for testing.
  bool HasTrackedTasksForTesting();

  const char* GetPrefName() const override;

 private:
  void Count() override;

  void OnGetUniqueDomains(history::DomainsVisitedResult result);

  raw_ptr<history::HistoryService, DanglingUntriaged> history_service_;

  base::CancelableTaskTracker cancelable_task_tracker_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<HistoryCounter> weak_ptr_factory_{this};
};

}  // namespace browsing_data

#endif  // COMPONENTS_BROWSING_DATA_CORE_COUNTERS_HISTORY_COUNTER_H_
