// Copyright 2026 Silkweave
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHUTTLE_SESSION_MANAGER_H_
#define CHROME_BROWSER_SHUTTLE_SESSION_MANAGER_H_

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "build/build_config.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "components/tab_groups/tab_group_id.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

class Profile;
class ScopedKeepAlive;
class ScopedProfileKeepAlive;

namespace content {
class WebContents;
}

namespace network::mojom {
class CookieManager;
}

namespace shuttle {

class ProfileRegistry;

// A session is one run of an agent (an agent profile). Its tabs are leased
// to it exclusively. All tabs of an agent, across its sessions, form one tab
// group with the agent's name and color.
struct Session {
  Session();
  Session(const Session&);
  ~Session();

  std::string id;
  // Agent (profile) id.
  std::string agent;
  base::Time opened;
};

// Also keeps the session tab groups in sync with the agents' identities, both
// ways: identity changes restyle the groups, and renaming or recoloring a
// group in the tab strip updates the agent.
class SessionManager : public TabStripModelObserver {
 public:
  SessionManager(Profile* profile, ProfileRegistry* registry);
  SessionManager(const SessionManager&) = delete;
  SessionManager& operator=(const SessionManager&) = delete;
  ~SessionManager() override;

  const Session& Open(const std::string& agent_id);
  const Session* Get(const std::string& session_id) const;
  std::vector<const Session*> List() const;

  // Closes the session's tabs and forgets it. Returns false if unknown.
  bool Close(const std::string& session_id);
  // Runs after a session closed (explicitly, or with its agent).
  using SessionClosedCallback =
      base::RepeatingCallback<void(const std::string& session_id)>;
  void SetSessionClosedCallback(SessionClosedCallback callback) {
    session_closed_callback_ = std::move(callback);
  }
  void CloseProfileSessions(const std::string& agent_id);

  // Re-applies the agent's name and color to its tab group (and repaints it,
  // for the avatar).
  void RefreshGroups(const std::string& agent_id);

  // Opens a background tab in the session's profile partition, leased to
  // the session, and starts loading `url` (about:blank if empty).
  base::expected<content::WebContents*, std::string> AllocateTab(
      const std::string& session_id,
      const GURL& url,
      std::optional<gfx::Size> viewport);
  void ReleaseTab(content::WebContents* contents);

  // For the user (chrome://agents): opens `url` (about:blank if empty) in a
  // new foreground tab of `agent_id`, leased to its latest session (or a new
  // one), and brings its window to the front.
  base::expected<content::WebContents*, std::string> OpenTabForUser(
      const std::string& agent_id,
      const GURL& url);
  // Activates `contents` in its tab strip and brings its window to the
  // front. Returns false if it isn't in a tab strip.
  static bool ShowTab(content::WebContents* contents);
  // For agents (CDP Page.bringToFront / Target.activateTarget): activates
  // `contents` in its tab strip, but only if its window isn't the active one
  // (that would switch the tab the user is looking at). Never activates the
  // window or the app, so agents can't steal focus.
  static void SelectTab(content::WebContents* contents);

  // For the user (tab context menu): hands a tab to `agent_id` (joins the
  // agent's tabs, see AdoptTab), starting the agent's tab group in the tab's
  // window if it has none there.
  void HandTabToAgent(content::WebContents* contents,
                      const std::string& agent_id);
  // For the user: takes a tab back from its agent. The page reopens in the
  // user's own storage as an ordinary tab next to it; the agent's tab closes.
  void TakeBackTab(content::WebContents* contents);
  // Returns the agent whose tab group is `group`, or "".
  std::string AgentOfGroup(const tab_groups::TabGroupId& group) const;
  // Agent of the session `contents` is leased to, or "".
  std::string AgentOf(content::WebContents* contents) const;

  // Leased tabs of a session, in tab strip order.
  std::vector<content::WebContents*> TabsOf(
      const std::string& session_id) const;

  // Sets a leased tab's viewport (device emulation, so it also holds while
  // the tab is active; background tabs are resized too). Returns the size
  // actually applied (clamped).
  gfx::Size SetViewport(content::WebContents* contents, const gfx::Size& size);

  // The cookie manager of the session's agent storage, or nullptr.
  network::mojom::CookieManager* CookieManagerOf(const std::string& session_id);

  base::WeakPtr<SessionManager> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  // The viewport an agent set for a leased tab, if any.
  static std::optional<gfx::Size> ViewportOf(content::WebContents* contents);

  // Returns the session a tab is leased to, or "".
  static std::string SessionOf(content::WebContents* contents);

  // Returns the tab for a CDP page target id, or nullptr.
  static content::WebContents* FindTab(const std::string& target_id);
  static std::string TargetIdOf(content::WebContents* contents);

  // Called by a lease when its tab opened `new_contents` (popup, noopener or
  // Cmd-click tab): it is leased to the same session.
  void OnTabOpened(const std::string& session_id,
                   content::WebContents* new_contents);

  // TabStripModelObserver:
  void OnTabGroupChanged(const TabGroupChange& change) override;
  void TabGroupedStateChanged(TabStripModel* tab_strip,
                              std::optional<tab_groups::TabGroupId> old_group,
                              std::optional<tab_groups::TabGroupId> new_group,
                              tabs::TabInterface* tab,
                              int index) override;
  void OnTabStripModelDestroyed(TabStripModel* tab_strip) override;

 private:
  void Lease(content::WebContents* contents, const std::string& session_id);
  void AddToSessionGroup(base::WeakPtr<content::WebContents> contents,
                         const std::string& session_id);

  // A new tab in `agent_id`'s storage partition, not yet in a tab strip.
  std::unique_ptr<content::WebContents> CreateAgentContents(
      const std::string& agent_id,
      const GURL& url,
      bool hidden);
  // Makes a tab the user put into an agent's group the agent's (also one taken
  // from another agent): leased to the agent's latest session (or a new one),
  // reopened in the agent's storage if it isn't there (a page can't change
  // storage partitions).
  void AdoptTab(base::WeakPtr<content::WebContents> contents,
                const std::string& agent_id);
  // A leased tab the user dragged out of its agent's group goes back to the
  // user, like "Take back from agent". Waits while a tab drag runs (the tab
  // may still be dropped into a group); gives up after `attempts_left`.
  void TakeBackIfUngrouped(base::WeakPtr<content::WebContents> contents,
                           int attempts_left);
  const Session* LatestSession(const std::string& agent_id) const;
  void Observe(TabStripModel* tab_strip);
  // Returns the tab strip holding `group`, or nullptr.
  TabStripModel* FindGroup(const tab_groups::TabGroupId& group) const;

  raw_ptr<Profile> profile_;
  raw_ptr<ProfileRegistry> registry_;
  std::map<std::string, Session> sessions_;
#if BUILDFLAG(IS_LINUX)
  // Keeps the browser running while any session is open, so closing the last
  // window (Shuttle started from the menu or a terminal) doesn't stop the
  // server under a working agent. With no session it quits like an app.
  // macOS keeps running anyway (AppController), and an extra keep-alive there
  // would block Cmd-Q.
  // The profile keep-alive stops the profile (and this manager with its
  // sessions) from being destroyed when its last window closes.
  // Both are dropped once the user quits (menu Exit): see QuitWatcher.
  void UpdateKeepAlive();
  void ReleaseKeepAliveForQuit();
  class QuitWatcher;
  std::unique_ptr<QuitWatcher> quit_watcher_;
  std::unique_ptr<ScopedKeepAlive> sessions_keep_alive_;
  std::unique_ptr<ScopedProfileKeepAlive> sessions_profile_keep_alive_;
#endif
  SessionClosedCallback session_closed_callback_;
  // Agent id -> its tab group. Created with the agent's first tab (a tab
  // group can't be empty); gone once its last tab closes.
  std::map<std::string, tab_groups::TabGroupId> groups_;
  std::set<raw_ptr<TabStripModel>> observed_;
  // Set while this class changes group visuals itself.
  bool applying_visuals_ = false;
  base::WeakPtrFactory<SessionManager> weak_factory_{this};
};

}  // namespace shuttle

#endif  // CHROME_BROWSER_SHUTTLE_SESSION_MANAGER_H_
