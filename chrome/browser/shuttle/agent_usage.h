// Copyright 2026 Silkweave
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHUTTLE_AGENT_USAGE_H_
#define CHROME_BROWSER_SHUTTLE_AGENT_USAGE_H_

#include <map>
#include <memory>
#include <string>

#include "base/byte_size.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "chrome/browser/task_manager/task_manager_observer.h"

class Profile;

namespace shuttle {

// What one agent (or the user, or the shared browser processes) costs right
// now, from the task manager's last refresh.
struct AgentUsage {
  // Memory footprint of its processes, each process counted once.
  base::ByteSize memory;
  // CPU of its processes, in percent of one core (as the task manager).
  double cpu = 0;
  // Network bytes per second of its tasks.
  base::ByteSize network;
  int processes = 0;
  int tabs = 0;
};

// Per-agent resource usage, measured. Agents are storage partitions of the one
// profile ("shuttle-<id>"), and Chromium never puts two storage partitions in
// one renderer process, so every renderer process (main frames, out-of-process
// iframes, workers) belongs to exactly one agent or to the user's default
// storage. Processes that aren't renderers of this profile (browser, GPU,
// network, utility, other profiles) count as shared.
//
// Samples only while someone holds a Subscription: the task manager refreshes
// only while it has observers, and that costs CPU.
class AgentUsageService : public task_manager::TaskManagerObserver {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // A new sample is in (every 2 s while subscribed).
    virtual void OnAgentUsageRefreshed() = 0;
  };

  // Keeps sampling running while it lives.
  class Subscription {
   public:
    Subscription(const Subscription&) = delete;
    Subscription& operator=(const Subscription&) = delete;
    ~Subscription();

   private:
    friend class AgentUsageService;
    Subscription(base::WeakPtr<AgentUsageService> service, Observer* observer);

    base::WeakPtr<AgentUsageService> service_;
    raw_ptr<Observer> observer_;
  };

  explicit AgentUsageService(Profile* profile);
  AgentUsageService(const AgentUsageService&) = delete;
  AgentUsageService& operator=(const AgentUsageService&) = delete;
  ~AgentUsageService() override;

  // Starts sampling if it isn't running. `observer` (may be null) hears about
  // each sample until the subscription goes.
  [[nodiscard]] std::unique_ptr<Subscription> Subscribe(Observer* observer);

  // Whether a sample since the last start is in (memory, network, processes,
  // tabs). Before that the numbers are empty.
  bool has_sample() const { return samples_ > 0; }
  // Whether CPU is measured: the task manager's first CPU calculation has no
  // earlier CPU time to compare with (NaN), so CPU needs the second sample.
  bool cpu_ready() const { return samples_ >= 2; }
  // Samples since sampling started. A sample is taken when the task manager's
  // background calculations (CPU, memory) of a refresh are done, not when the
  // refresh starts.
  int samples() const { return samples_; }
  // By agent id; agents without processes are missing.
  const std::map<std::string, AgentUsage>& agents() const { return agents_; }
  // The user's tabs (default storage partition).
  const AgentUsage& user() const { return user_; }
  // Browser, GPU, network, utility processes and other profiles.
  const AgentUsage& shared() const { return shared_; }

  // task_manager::TaskManagerObserver:
  void OnTasksRefreshedWithBackgroundCalculations(
      const task_manager::TaskIdList& task_ids) override;

 private:
  void Release(Observer* observer);

  raw_ptr<Profile> profile_;
  int subscriptions_ = 0;
  int samples_ = 0;
  std::map<std::string, AgentUsage> agents_;
  AgentUsage user_;
  AgentUsage shared_;
  base::ObserverList<Observer> observers_;
  base::WeakPtrFactory<AgentUsageService> weak_factory_{this};
};

}  // namespace shuttle

#endif  // CHROME_BROWSER_SHUTTLE_AGENT_USAGE_H_
