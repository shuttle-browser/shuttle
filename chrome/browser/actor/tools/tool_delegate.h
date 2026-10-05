// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_TOOL_DELEGATE_H_
#define CHROME_BROWSER_ACTOR_TOOLS_TOOL_DELEGATE_H_

#include <optional>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/actor/site_policy.h"
#include "chrome/common/actor.mojom-forward.h"
#include "chrome/common/actor_webui.mojom.h"
#include "chrome/common/buildflags.h"
#include "components/actor/public/mojom/actor_types.mojom-forward.h"
#include "components/tabs/public/tab_interface.h"
#include "url/gurl.h"

class Profile;

namespace favicon {
class FaviconService;
}  // namespace favicon

namespace gfx {
class Image;
}  // namespace gfx

namespace actor {

class AggregatedJournal;
class EnterprisePolicyChecker;
class ToolRequest;

// Provides tools with functionality implemented by the code invoking the tool.
class ToolDelegate {
 public:
  virtual ~ToolDelegate() = default;

  // Returns the profile in which the task is running.
  virtual Profile& GetProfile() = 0;

  // Returns the journal so that tools may log information related to their
  // execution.
  virtual AggregatedJournal& GetJournal() = 0;
  // Returns the favicon service for the profile associated with the task.
  virtual favicon::FaviconService* GetFaviconService() = 0;

  // Returns the enterprise policy checker associated with the task.
  virtual const EnterprisePolicyChecker& GetEnterprisePolicyChecker() const = 0;

  // Invokes the given callback according to whether the tool may navigate to
  // the given URL.
  virtual void IsAcceptableNavigationDestination(
      const GURL& url,
      DecisionCallbackWithReason callback) = 0;

  // During tool execution, the tool becomes blocked on the user's attention.
  // The task still has control of the tab.
  virtual void InterruptFromTool() = 0;
  virtual void InterruptFromTool(bool retain_user_control) = 0;
  virtual void UninterruptFromTool() = 0;

  // Enqueues an action to be performed as a followup to the current action.
  virtual void EnqueueFollowupAction(std::unique_ptr<ToolRequest> action) = 0;

  // Adds a tab to the controlled tabs set. If `stop_task_on_detach` is true,
  // then the `ActorTask` will be stopped when the given tab is detached.
  virtual void AddTab(
      tabs::TabHandle tab_handle,
      bool stop_task_on_detach,
      base::OnceCallback<void(mojom::ActionResultPtr)> callback) = 0;
  // Returns true if the tab is in the controlled tabs set.
  virtual bool HasTab(tabs::TabHandle tab_handle) = 0;
  // Removes a tab from the controlled tabs set.
  virtual void RemoveTab(tabs::TabHandle tab_handle) = 0;

  // If there is an ongoing tool request, treat it as having failed with the
  // given reason.
  virtual void FailCurrentTool(mojom::ActionResultCode reason) = 0;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_TOOL_DELEGATE_H_
