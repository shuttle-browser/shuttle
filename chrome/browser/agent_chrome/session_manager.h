// Copyright 2026 The Agent Chrome Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AGENT_CHROME_SESSION_MANAGER_H_
#define CHROME_BROWSER_AGENT_CHROME_SESSION_MANAGER_H_

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "components/tab_groups/tab_group_id.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

class Profile;

namespace content {
class WebContents;
}

namespace agent_chrome {

class ProfileRegistry;

// A session is one agent working in one agent profile. Its tabs are leased
// to it exclusively and shown as one tab group.
struct Session {
  Session();
  Session(const Session&);
  ~Session();

  std::string id;
  std::string profile;
  std::string agent;
  base::Time opened;
  // Created with the first tab (a tab group can't be empty).
  std::optional<tab_groups::TabGroupId> group;
};

class SessionManager {
 public:
  SessionManager(Profile* profile, ProfileRegistry* registry);
  SessionManager(const SessionManager&) = delete;
  SessionManager& operator=(const SessionManager&) = delete;
  ~SessionManager();

  const Session& Open(const std::string& profile_id, const std::string& agent);
  const Session* Get(const std::string& session_id) const;
  std::vector<const Session*> List() const;

  // Closes the session's tabs and forgets it. Returns false if unknown.
  bool Close(const std::string& session_id);
  void CloseProfileSessions(const std::string& profile_id);

  // Opens a background tab in the session's profile partition, leased to
  // the session, and starts loading `url` (about:blank if empty).
  base::expected<content::WebContents*, std::string> AllocateTab(
      const std::string& session_id,
      const GURL& url,
      std::optional<gfx::Size> viewport);
  void ReleaseTab(content::WebContents* contents);

  // Leased tabs of a session, in tab strip order.
  std::vector<content::WebContents*> TabsOf(
      const std::string& session_id) const;

  // Resizes a leased tab's viewport; it is re-applied when the tab goes back
  // to the background. Returns the size actually applied.
  gfx::Size SetViewport(content::WebContents* contents, const gfx::Size& size);

  // Returns the session a tab is leased to, or "".
  static std::string SessionOf(content::WebContents* contents);

  // Returns the tab for a CDP page target id, or nullptr.
  static content::WebContents* FindTab(const std::string& target_id);
  static std::string TargetIdOf(content::WebContents* contents);

  // Called by a lease when its tab opened `new_contents` (popup, noopener or
  // Cmd-click tab): it is leased to the same session.
  void OnTabOpened(const std::string& session_id,
                   content::WebContents* new_contents);

 private:
  void Lease(content::WebContents* contents, const std::string& session_id);
  void AddToSessionGroup(base::WeakPtr<content::WebContents> contents,
                         const std::string& session_id);

  raw_ptr<Profile> profile_;
  raw_ptr<ProfileRegistry> registry_;
  std::map<std::string, Session> sessions_;
  base::WeakPtrFactory<SessionManager> weak_factory_{this};
};

}  // namespace agent_chrome

#endif  // CHROME_BROWSER_AGENT_CHROME_SESSION_MANAGER_H_
