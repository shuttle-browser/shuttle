// Copyright 2026 The Agent Chrome Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AGENT_CHROME_AGENT_CHROME_SERVICE_H_
#define CHROME_BROWSER_AGENT_CHROME_AGENT_CHROME_SERVICE_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace agent_chrome {

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

  ProfileRegistry& profiles() { return *profiles_; }
  SessionManager& sessions() { return *sessions_; }

  // KeyedService:
  void Shutdown() override;

 private:
  std::unique_ptr<ProfileRegistry> profiles_;
  std::unique_ptr<SessionManager> sessions_;
};

}  // namespace agent_chrome

#endif  // CHROME_BROWSER_AGENT_CHROME_AGENT_CHROME_SERVICE_H_
