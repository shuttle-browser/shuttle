// Copyright 2026 The Agent Chrome Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AGENT_CHROME_AGENT_CHROME_SERVICE_H_
#define CHROME_BROWSER_AGENT_CHROME_AGENT_CHROME_SERVICE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
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

namespace agent_chrome {

class AvatarCache;
class PageToolManager;
class ProfileRegistry;
class SessionManager;

// The in-browser MCP server: agent profiles, sessions and tabs. Reached
// through POST /mcp on the remote debugging port (see
// ChromeDevToolsManagerDelegate::HandleMcpRequest).
class AgentChromeService : public KeyedService {
 public:
  using HttpResponseCallback = base::OnceCallback<
      void(int status, std::string content_type, std::string body)>;

  explicit AgentChromeService(Profile* profile);
  AgentChromeService(const AgentChromeService&) = delete;
  AgentChromeService& operator=(const AgentChromeService&) = delete;
  ~AgentChromeService() override;

  // Returns the service of `profile`, or nullptr for profiles without one.
  static AgentChromeService* Get(Profile* profile);

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

  ProfileRegistry& profiles() { return *profiles_; }
  SessionManager& sessions() { return *sessions_; }

  // KeyedService:
  void Shutdown() override;

 private:
  std::unique_ptr<ProfileRegistry> profiles_;
  std::unique_ptr<SessionManager> sessions_;
  std::unique_ptr<AvatarCache> avatars_;
  // After sessions_: sidecars die before the sessions they serve.
  std::unique_ptr<PageToolManager> page_tools_;
};

}  // namespace agent_chrome

#endif  // CHROME_BROWSER_AGENT_CHROME_AGENT_CHROME_SERVICE_H_
