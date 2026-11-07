// Copyright 2026 Silkweave
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHUTTLE_SHUTTLE_SERVICE_H_
#define CHROME_BROWSER_SHUTTLE_SHUTTLE_SERVICE_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "chrome/browser/shuttle/profile_registry.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/devtools_manager_delegate.h"
#include "ui/gfx/image/image_skia.h"

class Profile;

namespace content {
class WebContents;
}

namespace tab_groups {
class TabGroupId;
}

namespace shuttle {

class AvatarCache;
class PageToolManager;
class ProfileRegistry;
class SessionManager;

// The in-browser MCP server: agent profiles, sessions and tabs. Reached
// through POST /mcp on the remote debugging port (see
// ChromeDevToolsManagerDelegate::HandleMcpRequest).
class ShuttleService : public KeyedService {
 public:
  using HttpResponseCallback = base::OnceCallback<
      void(int status, std::string content_type, std::string body)>;

  explicit ShuttleService(Profile* profile);
  ShuttleService(const ShuttleService&) = delete;
  ShuttleService& operator=(const ShuttleService&) = delete;
  ~ShuttleService() override;

  // Returns the service of `profile`, or nullptr for profiles without one.
  static ShuttleService* Get(Profile* profile);

  // Handles one JSON-RPC message POSTed to /mcp.
  void HandleMcpRequest(const std::string& host,
                        const std::string& body,
                        HttpResponseCallback callback);

  // A filter scoping a CDP connection to an agent session, or nullptr:
  // /devtools/session/<session_id> (nullptr if unknown), or a page connection
  // to `target_id` (nullptr unless it is a leased tab).
  std::unique_ptr<content::DevToolsManagerDelegate::ConnectionFilter>
  CreateConnectionFilter(const std::string& session_id,
                         const std::string& target_id);

  // For the tab strip UI, which only sees this header: agents as tab
  // groups.
  struct AgentSummary {
    std::string id;
    std::string name;
  };
  // All agents, by name.
  std::vector<AgentSummary> ListAgents() const;
  // The agent `contents` works for (is leased to), or "".
  std::string AgentOfTab(content::WebContents* contents) const;
  // The agent whose tabs form `group`, or "".
  std::string AgentOfGroup(const tab_groups::TabGroupId& group) const;
  // The avatar of the agent whose tabs form `group` (round, see
  // AvatarCache), or an empty image. A late image repaints the group.
  gfx::ImageSkia AvatarOfGroup(const tab_groups::TabGroupId& group);
  // Hands a user's tab (or another agent's) to `agent_id`.
  void HandTabToAgent(content::WebContents* contents,
                      const std::string& agent_id);
  // Takes an agent's tab back: it reopens as an ordinary tab.
  void TakeBackTab(content::WebContents* contents);

  // Creates a throwaway agent (see ProfileRegistry). Returns its id, or an
  // error (the cap of live throwaway agents is reached).
  base::expected<std::string, std::string> CreateThrowawayAgent(
      const ProfileRegistry::Identity& identity);
  // Closes the agent's sessions and deletes it (delete-agent, and throwaway
  // agents idle past their timeout). `done` runs once its data is gone.
  void DeleteAgent(const std::string& agent_id, base::OnceClosure done);
  // When a throwaway agent will be deleted if nothing happens (an estimate:
  // a busy agent is kept), or nullopt for other agents.
  std::optional<base::Time> ThrowawayExpiry(const std::string& agent_id) const;

  ProfileRegistry& profiles() { return *profiles_; }
  SessionManager& sessions() { return *sessions_; }

  // KeyedService:
  void Shutdown() override;

 private:
  // Tool arguments naming a session (`sessionId`) or one of its tabs
  // (`targetId`) count as the session's activity. Returns the session the
  // call works for (the `sessionId` one first), empty if none.
  std::string TouchSessionOf(const base::DictValue& arguments);
  // Deletes throwaway agents idle past their timeout; runs while any exist.
  void CheckThrowaways();
  void UpdateThrowawayTimer();
  base::TimeDelta IdleTimeoutOf(const ProfileRegistry::Entry& entry) const;
  base::TimeTicks LastActivityOf(const std::string& agent_id) const;

  std::unique_ptr<ProfileRegistry> profiles_;
  std::unique_ptr<SessionManager> sessions_;
  std::unique_ptr<AvatarCache> avatars_;
  // After sessions_: sidecars die before the sessions they serve.
  std::unique_ptr<PageToolManager> page_tools_;
  base::RepeatingTimer throwaway_timer_;
  base::WeakPtrFactory<ShuttleService> weak_factory_{this};
};

}  // namespace shuttle

#endif  // CHROME_BROWSER_SHUTTLE_SHUTTLE_SERVICE_H_
