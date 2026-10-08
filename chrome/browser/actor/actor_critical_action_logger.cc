// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/actor_critical_action_logger.h"

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/json/json_writer.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/uuid.h"
#include "base/values.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/tools/tool_request.h"
#include "chrome/browser/actor/tools/type_tool_request.h"
#include "chrome/browser/critical_actions/critical_action_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "components/actor/core/task_id.h"
#include "components/critical_actions/core/browser/critical_action_service.h"
#include "components/critical_actions/core/browser/features.h"

namespace actor {


void ActorCriticalActionLogger::MaybeLogAction(
    ActorTask& task,
    Profile* profile,
    const ToolRequest& action,
    const mojom::ActionResult& result,
    int64_t navigation_id) {
  // Do not log the action if the tool execution failed.
  if (result.code != mojom::ActionResultCode::kOk) {
    return;
  }

  // The only actions that were logged here were login, OTP filling and form
  // filling, all of which have been removed. Nothing is logged for the
  // remaining tools.
}

void ActorCriticalActionLogger::LogAgentSelfReportedAction(
    Profile* profile,
    std::string conversation_id,
    critical_actions::ActionType action_type,
    const GURL& url,
    int64_t navigation_id,
    TaskId actor_task_id,
    std::string metadata) {
  if (!base::FeatureList::IsEnabled(
          critical_actions::features::kCriticalActionHistory)) {
    return;
  }

  if (!profile) {
    return;
  }

  critical_actions::CriticalActionService* service =
      critical_actions::CriticalActionFactory::GetForProfile(profile);
  if (!service) {
    return;
  }

  LogEntry(*service, action_type, std::move(conversation_id), actor_task_id,
           url, std::move(metadata), navigation_id);
}

void ActorCriticalActionLogger::LogEntry(
    critical_actions::CriticalActionService& service,
    critical_actions::ActionType action_type,
    std::string conversation_id,
    TaskId actor_task_id,
    const GURL& url,
    std::string metadata,
    int64_t navigation_id) {
  critical_actions::CriticalActionEntry entry;
  entry.critical_action_id = base::Uuid::GenerateRandomV4().AsLowercaseString();
  entry.timestamp = base::Time::Now();
  entry.action_source = critical_actions::ActionSource::kActor;
  entry.action_type = action_type;
  entry.conversation_id = std::move(conversation_id);
  entry.actor_task_id = actor_task_id.is_null()
                            ? ""
                            : base::NumberToString(actor_task_id.value());
  entry.url = url;
  entry.metadata = std::move(metadata);

  service.AddCriticalActionWithNavigationId(entry, navigation_id);
}

}  // namespace actor
