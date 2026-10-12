// Copyright 2026 Silkweave
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/shuttle/session_manager.h"

#include <algorithm>
#include <array>
#include <utility>

#include "base/auto_reset.h"
#include "base/functional/bind.h"
#include "base/containers/span.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_writer.h"
#include "base/rand_util.h"
#include "base/types/expected_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/values.h"
#include "chrome/browser/shuttle/profile_registry.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/navigator/browser_navigator.h"
#include "chrome/browser/ui/navigator/browser_navigator_params.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_delegate.h"
#include "components/tab_groups/tab_group_color.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "components/tabs/public/tab_group.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_agent_host_client.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "ui/base/base_window.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/geometry/rect.h"
#include "url/url_constants.h"

namespace shuttle {

namespace {

// Chrome's minimum window size doesn't apply to a hidden tab's viewport, but
// keep sizes sane.
constexpr int kMinViewport = 100;
constexpr int kMaxViewport = 8192;

// Applies an agent's viewport size with device emulation (an internal
// DevTools session): unlike WebContents::Resize, it also holds for the active
// tab, whose view otherwise follows the window.
class ViewportEmulator : public content::DevToolsAgentHostClient {
 public:
  ViewportEmulator() = default;
  ViewportEmulator(const ViewportEmulator&) = delete;
  ViewportEmulator& operator=(const ViewportEmulator&) = delete;
  ~ViewportEmulator() override {
    if (host_) {
      host_->DetachClient(this);
    }
  }

  void Apply(content::WebContents* contents, const gfx::Size& size) {
    if (!host_) {
      host_ = content::DevToolsAgentHost::GetOrCreateFor(contents);
      if (!host_->AttachClient(this)) {
        host_ = nullptr;
        return;
      }
    }
    base::DictValue message;
    message.Set("id", ++last_id_);
    message.Set("method", "Emulation.setDeviceMetricsOverride");
    message.Set("params", base::DictValue()
                              .Set("width", size.width())
                              .Set("height", size.height())
                              .Set("deviceScaleFactor", 0)
                              .Set("mobile", false));
    std::string json = base::WriteJson(message).value_or("{}");
    host_->DispatchProtocolMessage(this, base::as_byte_span(json));
  }

  // content::DevToolsAgentHostClient:
  void DispatchProtocolMessage(content::DevToolsAgentHost* host,
                               base::span<const uint8_t> message) override {}
  void AgentHostClosed(content::DevToolsAgentHost* host) override {
    host_ = nullptr;
  }
  std::string GetTypeForMetrics() override { return "Other"; }

 private:
  scoped_refptr<content::DevToolsAgentHost> host_;
  int last_id_ = 0;
};

// Lease of a tab to a session: keeps the tab rendering like a foreground tab
// (a capture handle) and re-applies its viewport size after activation.
class AgentTabLease : public content::WebContentsUserData<AgentTabLease>,
                      public content::WebContentsObserver {
 public:
  ~AgentTabLease() override = default;

  const std::string& session_id() const { return session_id_; }
  const std::optional<gfx::Size>& viewport() const { return viewport_; }
  void set_viewport(const gfx::Size& size) {
    viewport_ = size;
    emulator_.Apply(web_contents(), size);
  }

  // content::WebContentsObserver:
  void DidOpenRequestedURL(content::WebContents* new_contents,
                           content::RenderFrameHost* source_render_frame_host,
                           const GURL& url,
                           const content::Referrer& referrer,
                           WindowOpenDisposition disposition,
                           ui::PageTransition transition,
                           bool started_from_context_menu,
                           bool renderer_initiated) override {
    if (manager_ && new_contents != web_contents()) {
      manager_->OnTabOpened(session_id_, new_contents);
    }
  }

  void OnVisibilityChanged(content::Visibility visibility) override {
    // Activating a tab snaps it to the window size; restore the agent's size
    // once it is in the background again.
    if (visibility != content::Visibility::HIDDEN || !viewport_) {
      return;
    }
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(
                       [](base::WeakPtr<content::WebContents> contents,
                          gfx::Size size) {
                         if (contents) {
                           contents->Resize(gfx::Rect(size));
                         }
                       },
                       web_contents()->GetWeakPtr(), *viewport_));
  }

 private:
  friend class content::WebContentsUserData<AgentTabLease>;

  AgentTabLease(content::WebContents* contents,
                std::string session_id,
                base::WeakPtr<SessionManager> manager)
      : content::WebContentsUserData<AgentTabLease>(*contents),
        content::WebContentsObserver(contents),
        session_id_(std::move(session_id)),
        manager_(std::move(manager)),
        capture_(contents->IncrementCapturerCount(gfx::Size(),
                                                  /*stay_hidden=*/false,
                                                  /*stay_awake=*/true,
                                                  /*is_activity=*/false)) {}

  std::string session_id_;
  base::WeakPtr<SessionManager> manager_;
  base::ScopedClosureRunner capture_;
  std::optional<gfx::Size> viewport_;
  ViewportEmulator emulator_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

WEB_CONTENTS_USER_DATA_KEY_IMPL(AgentTabLease);

struct TabLocation {
  raw_ptr<TabStripModel> tab_strip = nullptr;
  int index = TabStripModel::kNoTab;
};

// A tab dragged out of an agent's group: how long to wait for the drag to
// end before deciding (120 x 250 ms = 30 s).
constexpr int kTakeBackAttempts = 120;
constexpr base::TimeDelta kTakeBackRetryDelay = base::Milliseconds(250);

TabLocation FindInTabStrips(content::WebContents* contents) {
  for (BrowserWindowInterface* browser : GetAllBrowserWindowInterfaces()) {
    TabStripModel* tab_strip = browser->GetTabStripModel();
    if (!tab_strip) {
      continue;
    }
    int index = tab_strip->GetIndexOfWebContents(contents);
    if (index != TabStripModel::kNoTab) {
      return {tab_strip, index};
    }
  }
  return {};
}

std::string NewSessionId() {
  uint64_t value = base::RandUint64();
  return base::HexEncodeLower(base::byte_span_from_ref(value)).substr(0, 10);
}

}  // namespace

Session::Session() = default;
Session::Session(const Session&) = default;
Session::~Session() = default;

SessionManager::SessionManager(Profile* profile, ProfileRegistry* registry)
    : profile_(profile), registry_(registry) {}

SessionManager::~SessionManager() = default;

const Session& SessionManager::Open(const std::string& agent_id) {
  std::string id = NewSessionId();
  while (sessions_.contains(id)) {
    id = NewSessionId();
  }
  Session& session = sessions_[id];
  session.id = id;
  session.agent = agent_id;
  session.opened = base::Time::Now();
  return session;
}

const Session* SessionManager::Get(const std::string& session_id) const {
  auto it = sessions_.find(session_id);
  return it == sessions_.end() ? nullptr : &it->second;
}

std::vector<const Session*> SessionManager::List() const {
  std::vector<const Session*> list;
  for (const auto& [id, session] : sessions_) {
    list.push_back(&session);
  }
  return list;
}

bool SessionManager::Close(const std::string& session_id) {
  auto it = sessions_.find(session_id);
  if (it == sessions_.end()) {
    return false;
  }
  std::string agent_id = it->second.agent;
  for (content::WebContents* contents : TabsOf(session_id)) {
    ReleaseTab(contents);
  }
  sessions_.erase(session_id);
  registry_->FlushCookies(agent_id);
  if (session_closed_callback_) {
    session_closed_callback_.Run(session_id);
  }
  return true;
}

void SessionManager::CloseProfileSessions(const std::string& agent_id) {
  std::vector<std::string> ids;
  for (const auto& [id, session] : sessions_) {
    if (session.agent == agent_id) {
      ids.push_back(id);
    }
  }
  for (const std::string& id : ids) {
    Close(id);
  }
}

base::expected<content::WebContents*, std::string> SessionManager::AllocateTab(
    const std::string& session_id,
    const GURL& requested_url,
    std::optional<gfx::Size> viewport) {
  const Session* session = Get(session_id);
  if (!session) {
    return base::unexpected("Unknown session: " + session_id);
  }
  GURL url = requested_url.is_empty() ? GURL(url::kAboutBlankURL)
                                       : requested_url;
  if (!url.is_valid()) {
    return base::unexpected("Invalid URL: " + requested_url.spec());
  }

  std::unique_ptr<content::WebContents> new_contents =
      CreateAgentContents(session->agent, url, /*hidden=*/true);
  content::WebContents* contents = new_contents.get();
  // Lease before insertion, so the tab is never seen unleased.
  Lease(contents, session_id);

  NavigateParams params(profile_.get(), url, ui::PAGE_TRANSITION_AUTO_TOPLEVEL);
  params.contents_to_insert = std::move(new_contents);
  params.disposition = WindowOpenDisposition::NEW_BACKGROUND_TAB;
  params.window_action = NavigateParams::WindowAction::kShowWindowInactive;
  Navigate(&params);
  if (!FindInTabStrips(contents).tab_strip) {
    return base::unexpected("Could not open a tab");
  }
  AddToSessionGroup(contents->GetWeakPtr(), session_id);

  if (viewport) {
    SetViewport(contents, *viewport);
  }
  content::NavigationController::LoadURLParams load_params(url);
  load_params.transition_type = ui::PAGE_TRANSITION_AUTO_TOPLEVEL;
  contents->GetController().LoadURLWithParams(load_params);
  return contents;
}

base::expected<content::WebContents*, std::string>
SessionManager::OpenTabForUser(const std::string& agent_id, const GURL& url) {
  if (!registry_->Exists(agent_id)) {
    return base::unexpected("Unknown agent: " + agent_id);
  }
  const Session* session = LatestSession(agent_id);
  std::string session_id = session ? session->id : Open(agent_id).id;
  ASSIGN_OR_RETURN(content::WebContents * contents,
                   AllocateTab(session_id, url, std::nullopt));
  ShowTab(contents);
  return contents;
}

// static
bool SessionManager::ShowTab(content::WebContents* contents) {
  for (BrowserWindowInterface* browser : GetAllBrowserWindowInterfaces()) {
    TabStripModel* tab_strip = browser->GetTabStripModel();
    int index = tab_strip ? tab_strip->GetIndexOfWebContents(contents)
                          : TabStripModel::kNoTab;
    if (index == TabStripModel::kNoTab) {
      continue;
    }
    tab_strip->ActivateTabAt(index);
    if (ui::BaseWindow* window = browser->GetWindow()) {
      window->Show();
      window->Activate();
    }
    return true;
  }
  return false;
}

std::unique_ptr<content::WebContents> SessionManager::CreateAgentContents(
    const std::string& agent_id,
    const GURL& url,
    bool hidden) {
  content::WebContents::CreateParams create_params(
      profile_, content::SiteInstance::CreateForFixedStoragePartition(
                    profile_, url, registry_->PartitionConfig(agent_id)));
  create_params.initially_hidden = hidden;
  return content::WebContents::Create(create_params);
}

void SessionManager::ReleaseTab(content::WebContents* contents) {
  TabLocation location = FindInTabStrips(contents);
  if (location.tab_strip) {
    location.tab_strip->CloseWebContentsAt(location.index,
                                           TabCloseTypes::CLOSE_NONE);
  } else {
    contents->Close();
  }
}

std::vector<content::WebContents*> SessionManager::TabsOf(
    const std::string& session_id) const {
  std::vector<content::WebContents*> tabs;
  for (BrowserWindowInterface* browser : GetAllBrowserWindowInterfaces()) {
    TabStripModel* tab_strip = browser->GetTabStripModel();
    if (!tab_strip || browser->GetProfile() != profile_) {
      continue;
    }
    for (int i = 0; i < tab_strip->count(); ++i) {
      content::WebContents* contents = tab_strip->GetWebContentsAt(i);
      if (SessionOf(contents) == session_id) {
        tabs.push_back(contents);
      }
    }
  }
  return tabs;
}

gfx::Size SessionManager::SetViewport(content::WebContents* contents,
                                      const gfx::Size& requested) {
  gfx::Size size(std::clamp(requested.width(), kMinViewport, kMaxViewport),
                 std::clamp(requested.height(), kMinViewport, kMaxViewport));
  if (AgentTabLease* lease = AgentTabLease::FromWebContents(contents)) {
    // Device emulation: holds for the active tab too.
    lease->set_viewport(size);
  }
  // Background tabs also get a view of that size (the active tab's view
  // follows the window).
  TabLocation location = FindInTabStrips(contents);
  if (!location.tab_strip ||
      location.tab_strip->active_index() != location.index) {
    contents->Resize(gfx::Rect(size));
  }
  return size;
}

network::mojom::CookieManager* SessionManager::CookieManagerOf(
    const std::string& session_id) {
  const Session* session = Get(session_id);
  if (!session) {
    return nullptr;
  }
  content::StoragePartition* partition = profile_->GetStoragePartition(
      registry_->PartitionConfig(session->agent), /*can_create=*/true);
  return partition ? partition->GetCookieManagerForBrowserProcess() : nullptr;
}

// static
std::optional<gfx::Size> SessionManager::ViewportOf(
    content::WebContents* contents) {
  AgentTabLease* lease = AgentTabLease::FromWebContents(contents);
  return lease ? lease->viewport() : std::nullopt;
}

// static
std::string SessionManager::SessionOf(content::WebContents* contents) {
  AgentTabLease* lease = AgentTabLease::FromWebContents(contents);
  return lease ? lease->session_id() : std::string();
}

// static
content::WebContents* SessionManager::FindTab(const std::string& target_id) {
  return content::DevToolsAgentHost::GetWebContentsForId(target_id);
}

// static
std::string SessionManager::TargetIdOf(content::WebContents* contents) {
  return content::DevToolsAgentHost::GetOrCreateFor(contents)->GetId();
}

void SessionManager::OnTabOpened(const std::string& session_id,
                                 content::WebContents* new_contents) {
  if (!sessions_.contains(session_id) || !SessionOf(new_contents).empty()) {
    return;
  }
  Lease(new_contents, session_id);
  // The tab is inserted into the tab strip after this notification.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&SessionManager::AddToSessionGroup,
                     weak_factory_.GetWeakPtr(), new_contents->GetWeakPtr(),
                     session_id));
}

void SessionManager::Lease(content::WebContents* contents,
                           const std::string& session_id) {
  AgentTabLease::CreateForWebContents(contents, session_id,
                                      weak_factory_.GetWeakPtr());
}

void SessionManager::AddToSessionGroup(
    base::WeakPtr<content::WebContents> contents,
    const std::string& session_id) {
  auto it = sessions_.find(session_id);
  if (!contents || it == sessions_.end()) {
    return;
  }
  const std::string& agent_id = it->second.agent;
  TabLocation location = FindInTabStrips(contents.get());
  if (!location.tab_strip) {
    return;
  }
  TabStripModel* tab_strip = location.tab_strip;
  Observe(tab_strip);
  auto group = groups_.find(agent_id);
  if (group != groups_.end() && FindGroup(group->second) == tab_strip) {
    if (tab_strip->GetTabGroupForTab(location.index) != group->second) {
      tab_strip->AddToExistingGroup({location.index}, group->second);
    }
    return;
  }
  // No group yet, or it closed or lives in another window: start one here.
  std::optional<ProfileRegistry::Entry> agent = registry_->Get(agent_id);
  if (!agent) {
    return;
  }
  base::AutoReset<bool> applying(&applying_visuals_, true);
  tab_groups::TabGroupId new_group = tab_strip->AddToNewGroup({location.index});
  groups_.insert_or_assign(agent_id, new_group);
  tab_strip->ChangeTabGroupVisuals(
      new_group, tab_groups::TabGroupVisualData(base::UTF8ToUTF16(agent->name),
                                                agent->color));
}

void SessionManager::RefreshGroups(const std::string& agent_id) {
  std::optional<ProfileRegistry::Entry> agent = registry_->Get(agent_id);
  auto group = groups_.find(agent_id);
  if (!agent || group == groups_.end()) {
    return;
  }
  TabStripModel* tab_strip = FindGroup(group->second);
  if (!tab_strip) {
    return;
  }
  tab_groups::TabGroupVisualData visuals(base::UTF8ToUTF16(agent->name),
                                         agent->color);
  // Also unchanged visuals: the avatar may have changed, and this repaints
  // the group's header.
  base::AutoReset<bool> applying(&applying_visuals_, true);
  tab_strip->ChangeTabGroupVisuals(group->second, visuals);
}

void SessionManager::OnTabGroupChanged(const TabGroupChange& change) {
  if (applying_visuals_ || change.type != TabGroupChange::kVisualsChanged) {
    return;
  }
  std::string agent_id = AgentOfGroup(change.group);
  const TabGroupChange::VisualsChange* visuals = change.GetVisualsChange();
  if (agent_id.empty() || !visuals || !visuals->new_visuals) {
    return;
  }
  std::optional<ProfileRegistry::Entry> agent = registry_->Get(agent_id);
  if (!agent) {
    return;
  }
  // The user renamed or recolored the group: that's the agent's new
  // identity. An empty title isn't a name; the group gets the agent's back.
  std::string title = base::UTF16ToUTF8(visuals->new_visuals->title());
  ProfileRegistry::Identity identity;
  if (ProfileRegistry::CheckName(title).empty() && title != agent->name) {
    identity.name = title;
  }
  if (visuals->new_visuals->color() != agent->color) {
    identity.color = visuals->new_visuals->color();
  }
  if (identity.name || identity.color) {
    registry_->Update(agent_id, identity);
  }
  // Restore an emptied title once the tab strip has finished this change.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&SessionManager::RefreshGroups,
                                weak_factory_.GetWeakPtr(), agent_id));
}

void SessionManager::TabGroupedStateChanged(
    TabStripModel* tab_strip,
    std::optional<tab_groups::TabGroupId> old_group,
    std::optional<tab_groups::TabGroupId> new_group,
    tabs::TabInterface* tab,
    int index) {
  if (!tab || !tab->GetContents()) {
    return;
  }
  if (!new_group) {
    // Out of an agent's group (a drag, or the tab strip's own ungroup):
    // decide once the tab strip has settled. Moving into another agent's
    // group also passes here first; the tab is grouped again by then.
    if (old_group && !AgentOfGroup(*old_group).empty() &&
        AgentOf(tab->GetContents()) == AgentOfGroup(*old_group)) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(&SessionManager::TakeBackIfUngrouped,
                         weak_factory_.GetWeakPtr(),
                         tab->GetContents()->GetWeakPtr(),
                         kTakeBackAttempts));
    }
    return;
  }
  std::string agent_id = AgentOfGroup(*new_group);
  if (agent_id.empty() || AgentOf(tab->GetContents()) == agent_id) {
    return;
  }
  // Not while the tab strip is mid-change.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&SessionManager::AdoptTab, weak_factory_.GetWeakPtr(),
                     tab->GetContents()->GetWeakPtr(), agent_id));
}

void SessionManager::TakeBackIfUngrouped(
    base::WeakPtr<content::WebContents> contents,
    int attempts_left) {
  if (!contents || SessionOf(contents.get()).empty()) {
    return;  // Closed, or already taken back.
  }
  TabLocation location = FindInTabStrips(contents.get());
  bool dragging = !location.tab_strip ||
                  (location.tab_strip->delegate() &&
                   !location.tab_strip->delegate()->IsTabStripEditable());
  if (dragging) {
    if (attempts_left > 0) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&SessionManager::TakeBackIfUngrouped,
                         weak_factory_.GetWeakPtr(), contents,
                         attempts_left - 1),
          kTakeBackRetryDelay);
    }
    return;
  }
  if (location.tab_strip->GetTabGroupForTab(location.index)) {
    return;  // Dropped back into a group (its own or another agent's).
  }
  TakeBackTab(contents.get());
}

void SessionManager::AdoptTab(base::WeakPtr<content::WebContents> contents,
                              const std::string& agent_id) {
  auto group = groups_.find(agent_id);
  if (!contents || AgentOf(contents.get()) == agent_id ||
      group == groups_.end()) {
    return;
  }
  TabLocation location = FindInTabStrips(contents.get());
  TabStripModel* tab_strip = location.tab_strip;
  if (!tab_strip ||
      tab_strip->GetTabGroupForTab(location.index) != group->second) {
    return;  // Moved out of the group again.
  }
  const Session* session = LatestSession(agent_id);
  std::string session_id = session ? session->id : Open(agent_id).id;

  // Already in the agent's storage: just (re-)lease it.
  if (contents->GetPrimaryMainFrame()->GetStoragePartition() ==
      profile_->GetStoragePartition(registry_->PartitionConfig(agent_id),
                                    /*can_create=*/false)) {
    contents->RemoveUserData(AgentTabLease::UserDataKey());
    Lease(contents.get(), session_id);
    return;
  }

  // Reopen the page in the agent's storage. Browser pages (the New Tab page
  // and other WebUI) don't belong in agent storage: they become about:blank.
  GURL url = contents->GetLastCommittedURL();
  if (!url.SchemeIsHTTPOrHTTPS()) {
    url = GURL(url::kAboutBlankURL);
  }
  bool active = tab_strip->active_index() == location.index;
  std::unique_ptr<content::WebContents> new_contents =
      CreateAgentContents(agent_id, url, /*hidden=*/!active);
  content::WebContents* adopted = new_contents.get();
  Lease(adopted, session_id);
  tab_strip->InsertWebContentsAt(location.index + 1, std::move(new_contents),
                                 active ? AddTabTypes::ADD_ACTIVE
                                        : AddTabTypes::ADD_NONE,
                                 group->second);
  int old_index = tab_strip->GetIndexOfWebContents(contents.get());
  if (old_index != TabStripModel::kNoTab) {
    tab_strip->CloseWebContentsAt(old_index, TabCloseTypes::CLOSE_NONE);
  }
  content::NavigationController::LoadURLParams load_params(url);
  load_params.transition_type = ui::PAGE_TRANSITION_AUTO_TOPLEVEL;
  adopted->GetController().LoadURLWithParams(load_params);
}

void SessionManager::HandTabToAgent(content::WebContents* contents,
                                    const std::string& agent_id) {
  std::optional<ProfileRegistry::Entry> agent = registry_->Get(agent_id);
  TabLocation location = FindInTabStrips(contents);
  if (!agent || !location.tab_strip || AgentOf(contents) == agent_id) {
    return;
  }
  TabStripModel* tab_strip = location.tab_strip;
  Observe(tab_strip);
  auto group = groups_.find(agent_id);
  if (group != groups_.end() && FindGroup(group->second) == tab_strip) {
    // TabGroupedStateChanged adopts it.
    tab_strip->AddToExistingGroup({location.index}, group->second);
    return;
  }
  // The agent has no tabs in this window: the tab starts its group here.
  {
    base::AutoReset<bool> applying(&applying_visuals_, true);
    tab_groups::TabGroupId new_group =
        tab_strip->AddToNewGroup({location.index});
    groups_.insert_or_assign(agent_id, new_group);
    tab_strip->ChangeTabGroupVisuals(
        new_group, tab_groups::TabGroupVisualData(
                       base::UTF8ToUTF16(agent->name), agent->color));
  }
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&SessionManager::AdoptTab, weak_factory_.GetWeakPtr(),
                     contents->GetWeakPtr(), agent_id));
}

void SessionManager::TakeBackTab(content::WebContents* contents) {
  TabLocation location = FindInTabStrips(contents);
  if (!location.tab_strip || SessionOf(contents).empty()) {
    return;
  }
  TabStripModel* tab_strip = location.tab_strip;
  // Out of the agent's group first: the new tab must not land inside it.
  if (tab_strip->GetTabGroupForTab(location.index)) {
    tab_strip->RemoveFromGroup({location.index});
  }
  int index = tab_strip->GetIndexOfWebContents(contents);
  if (index == TabStripModel::kNoTab) {
    return;
  }
  bool active = tab_strip->active_index() == index;
  GURL url = contents->GetLastCommittedURL();
  if (!url.SchemeIsHTTPOrHTTPS()) {
    url = GURL(url::kAboutBlankURL);
  }
  content::WebContents::CreateParams create_params(profile_);
  create_params.initially_hidden = !active;
  std::unique_ptr<content::WebContents> new_contents =
      content::WebContents::Create(create_params);
  content::WebContents* user_tab = new_contents.get();
  tab_strip->InsertWebContentsAt(
      index + 1, std::move(new_contents),
      active ? AddTabTypes::ADD_ACTIVE : AddTabTypes::ADD_NONE, std::nullopt);
  ReleaseTab(contents);
  content::NavigationController::LoadURLParams load_params(url);
  load_params.transition_type = ui::PAGE_TRANSITION_AUTO_TOPLEVEL;
  user_tab->GetController().LoadURLWithParams(load_params);
}

std::string SessionManager::AgentOf(content::WebContents* contents) const {
  const Session* session = Get(SessionOf(contents));
  return session ? session->agent : std::string();
}

const Session* SessionManager::LatestSession(
    const std::string& agent_id) const {
  const Session* latest = nullptr;
  for (const auto& [id, session] : sessions_) {
    if (session.agent == agent_id &&
        (!latest || session.opened > latest->opened)) {
      latest = &session;
    }
  }
  return latest;
}

void SessionManager::OnTabStripModelDestroyed(TabStripModel* tab_strip) {
  observed_.erase(tab_strip);
}

void SessionManager::Observe(TabStripModel* tab_strip) {
  if (observed_.insert(tab_strip).second) {
    tab_strip->AddObserver(this);
  }
}

std::string SessionManager::AgentOfGroup(
    const tab_groups::TabGroupId& group) const {
  for (const auto& [agent_id, agent_group] : groups_) {
    if (agent_group == group) {
      return agent_id;
    }
  }
  return std::string();
}

TabStripModel* SessionManager::FindGroup(
    const tab_groups::TabGroupId& group) const {
  for (TabStripModel* tab_strip : observed_) {
    if (tab_strip->group_model() &&
        tab_strip->group_model()->ContainsTabGroup(group)) {
      return tab_strip;
    }
  }
  return nullptr;
}

}  // namespace shuttle
