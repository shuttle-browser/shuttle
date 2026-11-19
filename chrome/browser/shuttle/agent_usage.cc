// Copyright 2026 Silkweave
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/shuttle/agent_usage.h"

#include <cmath>
#include <optional>
#include <set>
#include <string_view>

#include "base/memory/ptr_util.h"
#include "base/process/process_handle.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/shuttle/agent_partition.h"
#include "chrome/browser/task_manager/task_manager_interface.h"
#include "components/sessions/core/session_id.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/storage_partition_config.h"

namespace shuttle {

namespace {

constexpr base::TimeDelta kRefreshInterval = base::Seconds(2);

constexpr int64_t kRefreshTypes = task_manager::REFRESH_TYPE_CPU |
                                  task_manager::REFRESH_TYPE_MEMORY_FOOTPRINT |
                                  task_manager::REFRESH_TYPE_NETWORK_USAGE;

enum class Owner { kAgent, kUser, kShared };

// Whose process `task_id` runs in; `agent_id` is set for kAgent.
Owner OwnerOf(task_manager::TaskManagerInterface* tm,
              task_manager::TaskId task_id,
              Profile* profile,
              std::string* agent_id) {
  content::RenderProcessHost* host =
      content::RenderProcessHost::FromID(tm->GetChildProcessUniqueId(task_id));
  if (!host || host->GetBrowserContext() != profile) {
    return Owner::kShared;
  }
  const std::string& domain =
      host->GetStoragePartition()->GetConfig().partition_domain();
  if (domain.empty()) {
    return Owner::kUser;
  }
  if (!base::StartsWith(domain, kAgentPartitionDomainPrefix)) {
    // Another non-default partition of this profile (none in Shuttle today).
    return Owner::kShared;
  }
  *agent_id =
      domain.substr(std::string_view(kAgentPartitionDomainPrefix).size());
  return Owner::kAgent;
}

}  // namespace

AgentUsageService::Subscription::Subscription(
    base::WeakPtr<AgentUsageService> service,
    Observer* observer)
    : service_(std::move(service)), observer_(observer) {}

AgentUsageService::Subscription::~Subscription() {
  if (service_) {
    service_->Release(observer_.get());
  }
}

AgentUsageService::AgentUsageService(Profile* profile)
    : task_manager::TaskManagerObserver(kRefreshInterval, kRefreshTypes),
      profile_(profile) {}

AgentUsageService::~AgentUsageService() {
  if (subscriptions_ > 0) {
    task_manager::TaskManagerInterface::GetTaskManager()->RemoveObserver(this);
  }
}

std::unique_ptr<AgentUsageService::Subscription> AgentUsageService::Subscribe(
    Observer* observer) {
  if (subscriptions_++ == 0) {
    samples_ = 0;
    task_manager::TaskManagerInterface::GetTaskManager()->AddObserver(this);
  }
  if (observer) {
    observers_.AddObserver(observer);
  }
  return base::WrapUnique(
      new Subscription(weak_factory_.GetWeakPtr(), observer));
}

void AgentUsageService::Release(Observer* observer) {
  if (observer) {
    observers_.RemoveObserver(observer);
  }
  if (--subscriptions_ == 0) {
    task_manager::TaskManagerInterface::GetTaskManager()->RemoveObserver(this);
    samples_ = 0;
    agents_.clear();
    user_ = AgentUsage();
    shared_ = AgentUsage();
  }
}

// Not OnTasksRefreshed: that fires when a refresh starts, before its CPU and
// memory are calculated (the first one synchronously inside AddObserver).
void AgentUsageService::OnTasksRefreshedWithBackgroundCalculations(
    const task_manager::TaskIdList& task_ids) {
  task_manager::TaskManagerInterface* tm = observed_task_manager();
  if (!tm) {
    return;
  }
  std::map<std::string, AgentUsage> agents;
  AgentUsage user;
  AgentUsage shared;
  std::set<base::ProcessId> counted;
  // A tab has several tasks when it has out-of-process iframes.
  std::set<SessionID> counted_tabs;
  for (task_manager::TaskId id : task_ids) {
    std::string agent_id;
    AgentUsage* usage = nullptr;
    switch (OwnerOf(tm, id, profile_, &agent_id)) {
      case Owner::kAgent:
        usage = &agents[agent_id];
        break;
      case Owner::kUser:
        usage = &user;
        break;
      case Owner::kShared:
        usage = &shared;
        break;
    }
    // Network is per task; memory and CPU are per process (every task of a
    // process reports the process's numbers).
    usage->network += tm->GetNetworkUsage(id);
    if (const SessionID tab = tm->GetTabId(id);
        tab.is_valid() && counted_tabs.insert(tab).second) {
      ++usage->tabs;
    }
    if (!counted.insert(tm->GetProcessId(id)).second) {
      continue;
    }
    ++usage->processes;
    if (std::optional<base::ByteSize> footprint =
            tm->GetMemoryFootprintUsage(id)) {
      usage->memory += *footprint;
    }
    const double cpu = tm->GetPlatformIndependentCPUUsage(id);
    if (!std::isnan(cpu)) {
      usage->cpu += cpu;
    }
  }
  agents_ = std::move(agents);
  user_ = user;
  shared_ = shared;
  ++samples_;
  for (Observer& observer : observers_) {
    observer.OnAgentUsageRefreshed();
  }
}

}  // namespace shuttle
