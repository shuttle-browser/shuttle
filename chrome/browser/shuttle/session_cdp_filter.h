// Copyright 2026 Silkweave
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHUTTLE_SESSION_CDP_FILTER_H_
#define CHROME_BROWSER_SHUTTLE_SESSION_CDP_FILTER_H_

#include <map>
#include <set>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "content/public/browser/devtools_manager_delegate.h"

namespace shuttle {

class SessionManager;

// Scopes one CDP connection to an agent session: the connection only sees and
// reaches the session's tabs (their frames and workers), new targets become
// tabs leased to the session, cookies are the agent's, and Browser.close ends
// the session instead of quitting. Used for /devtools/session/<id> (a
// browser-level connection) and for page connections to leased tabs (the
// Target domain of a page session is browser-wide).
class SessionCdpFilter
    : public content::DevToolsManagerDelegate::ConnectionFilter {
 public:
  SessionCdpFilter(base::WeakPtr<SessionManager> manager,
                   std::string session_id,
                   bool page_connection);
  SessionCdpFilter(const SessionCdpFilter&) = delete;
  SessionCdpFilter& operator=(const SessionCdpFilter&) = delete;
  ~SessionCdpFilter() override;

  // content::DevToolsManagerDelegate::ConnectionFilter:
  void Start(SendCallback to_agent, SendCallback to_client) override;
  void OnClientMessage(std::string message) override;
  void OnAgentMessage(std::string message) override;

 private:
  // Whether `target_id` is (or belongs to) a tab leased to the session, or
  // was opened by one (`opener_id`; popups are leased right after creation).
  bool Owns(const std::string& target_id, const std::string& opener_id);

  void HandleTargetEvent(const std::string& method,
                         base::DictValue message,
                         std::string raw);
  void FilterTargetInfos(base::DictValue message);

  void CreateTarget(const base::Value& id,
                    const std::string* session,
                    const base::DictValue* params);
  void CloseSession(const base::Value& id, const std::string* session);
  void GetCookies(const base::Value& id, const std::string* session);
  void SetCookies(const base::Value& id,
                  const std::string* session,
                  const base::DictValue* params);
  void ClearCookies(const base::Value& id, const std::string* session);

  void SendResult(const base::Value& id,
                  const std::string* session,
                  base::DictValue result);
  void SendError(const base::Value& id,
                 const std::string* session,
                 const std::string& message);
  // Sends a command of our own (its response is dropped).
  void SendInternal(const std::string& method,
                    base::DictValue params,
                    const std::string* session);

  base::WeakPtr<SessionManager> manager_;
  const std::string session_id_;
  // Page connections: the root session is the leased page itself, so targets
  // auto-attached to it (its frames and workers) are its own.
  const bool page_connection_;
  SendCallback to_agent_;
  SendCallback to_client_;

  // Target ids announced to the client.
  std::set<std::string> visible_;
  // Child sessions auto-attached to foreign targets, being detached.
  std::set<std::string> foreign_sessions_;
  // Client command ids whose responses list targets (Target.getTargets).
  std::set<int> filter_responses_;
  // Ids of our own commands.
  std::set<int> internal_ids_;
  int next_internal_id_ = 1'000'000'000;
  // Set while Target.createTarget allocates a tab.
  bool allocating_ = false;

  base::WeakPtrFactory<SessionCdpFilter> weak_factory_{this};
};

}  // namespace shuttle

#endif  // CHROME_BROWSER_SHUTTLE_SESSION_CDP_FILTER_H_
