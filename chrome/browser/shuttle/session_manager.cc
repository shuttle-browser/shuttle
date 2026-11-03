// Copyright 2026 Silkweave
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/shuttle/session_manager.h"

#include <algorithm>
#include <array>
#include <utility>

#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/functional/bind.h"
#include "base/containers/span.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_writer.h"
#include "base/rand_util.h"
#include "base/scoped_observation.h"
#include "base/types/expected_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/values.h"
#include "chrome/browser/headless/headless_mode_util.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/shuttle/home_tab_manager.h"
#include "chrome/browser/shuttle/profile_registry.h"
#include "chrome/browser/shuttle/switches.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_collection_observer.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/navigator/browser_navigator.h"
#include "chrome/browser/ui/navigator/browser_navigator_params.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_delegate.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
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

// How often idle sessions are looked for (also bounds the timeout's
// precision).
constexpr base::TimeDelta kIdleCheckInterval = base::Seconds(5);
constexpr base::TimeDelta kDefaultIdleTimeout = base::Minutes(30);

// True if `contents` is the active tab of the focused window: a person is
// looking at it.
bool IsInFrontOfUser(content::WebContents* contents) {
  // Headless windows can be "active" (activation is emulated) with no one
  // looking.
  if (headless::IsHeadlessMode()) {
    return false;
  }
  for (BrowserWindowInterface* browser : GetAllBrowserWindowInterfaces()) {
    TabStripModel* tab_strip = browser->GetTabStripModel();
    if (!tab_strip || tab_strip->GetActiveWebContents() != contents) {
      continue;
    }
    ui::BaseWindow* window = browser->GetWindow();
    return window && window->IsActive();
  }
  return false;
}

std::string NewSessionId() {
  uint64_t value = base::RandUint64();
  return base::HexEncodeLower(base::byte_span_from_ref(value)).substr(0, 10);
}

}  // namespace

#if BUILDFLAG(IS_LINUX)
// Drops the session keep-alives when the user quits (menu Exit, chrome://quit):
// AttemptExit() marks the browser as quitting, then closes every window, and
// the keep-alives would otherwise keep the process running windowless.
class SessionManager::QuitWatcher : public BrowserCollectionObserver {
 public:
  explicit QuitWatcher(SessionManager* manager) : manager_(manager) {
    observation_.Observe(GlobalBrowserCollection::GetInstance());
  }
  void OnBrowserClosed(BrowserWindowInterface* browser) override {
    if (browser_shutdown::IsTryingToQuit()) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(&SessionManager::ReleaseKeepAliveForQuit,
                                    manager_->weak_factory_.GetWeakPtr()));
    }
  }

 private:
  raw_ptr<SessionManager> manager_;
  base::ScopedObservation<GlobalBrowserCollection, BrowserCollectionObserver>
      observation_{this};
};
#endif  // BUILDFLAG(IS_LINUX)

base::TimeDelta DefaultSessionIdleTimeout() {
  static const base::TimeDelta timeout = [] {
    const base::CommandLine* command_line =
        base::CommandLine::ForCurrentProcess();
    if (!command_line->HasSwitch(kSessionIdleTimeoutSwitch)) {
      return kDefaultIdleTimeout;
    }
    std::string value =
        command_line->GetSwitchValueASCII(kSessionIdleTimeoutSwitch);
    const bool seconds = !value.empty() && value.back() == 's';
    if (seconds) {
      value.pop_back();
    }
    int amount = 0;
    if (!base::StringToInt(value, &amount) || amount < 0) {
      LOG(WARNING) << "Ignoring --" << kSessionIdleTimeoutSwitch << "="
                   << command_line->GetSwitchValueASCII(
                          kSessionIdleTimeoutSwitch);
      return kDefaultIdleTimeout;
    }
    return seconds ? base::Seconds(amount) : base::Minutes(amount);
  }();
  return timeout;
}

Session::Session() = default;
Session::Session(const Session&) = default;
Session::~Session() = default;

SessionManager::SessionManager(Profile* profile, ProfileRegistry* registry)
    : profile_(profile),
      registry_(registry),
      home_tabs_(std::make_unique<HomeTabManager>(profile)) {}

SessionManager::~SessionManager() = default;

const Session& SessionManager::Open(
    const std::string& agent_id,
    std::optional<base::TimeDelta> idle_timeout) {
  std::string id = NewSessionId();
  while (sessions_.contains(id)) {
    id = NewSessionId();
  }
  Session& session = sessions_[id];
  session.id = id;
  session.agent = agent_id;
  session.opened = base::Time::Now();
  session.last_activity = session.opened;
  session.last_activity_ticks = base::TimeTicks::Now();
  session.idle_timeout = idle_timeout.value_or(base::TimeDelta());
  UpdateIdleTimer();
#if BUILDFLAG(IS_LINUX)
  UpdateKeepAlive();
#endif
  return session;
}

void SessionManager::Touch(const std::string& session_id) {
  auto it = sessions_.find(session_id);
  if (it == sessions_.end()) {
    return;
  }
  it->second.last_activity = base::Time::Now();
  it->second.last_activity_ticks = base::TimeTicks::Now();
}

void SessionManager::BeginWork(const std::string& session_id) {
  auto it = sessions_.find(session_id);
  if (it != sessions_.end()) {
    ++it->second.pending_calls;
  }
}

void SessionManager::EndWork(const std::string& session_id) {
  auto it = sessions_.find(session_id);
  if (it != sessions_.end() && it->second.pending_calls > 0) {
    --it->second.pending_calls;
    Touch(session_id);
  }
}

bool SessionManager::SetIdleTimeout(const std::string& session_id,
                                    base::TimeDelta timeout) {
  auto it = sessions_.find(session_id);
  if (it == sessions_.end()) {
    return false;
  }
  it->second.idle_timeout = timeout;
  Touch(session_id);
  UpdateIdleTimer();
  return true;
}

std::vector<const Session*> SessionManager::SessionsOf(
    const std::string& agent_id) const {
  std::vector<const Session*> list;
  for (const auto& [id, session] : sessions_) {
    if (session.agent == agent_id) {
      list.push_back(&session);
    }
  }
  std::ranges::sort(list, {}, [](const Session* session) {
    return session->opened;
  });
  return list;
}

void SessionManager::CloseCreatedTabs(
    const std::string& session_id,
    const std::vector<std::string>& target_ids) {
  if (browser_shutdown::IsTryingToQuit()) {
    return;
  }
  for (const std::string& target_id : target_ids) {
    content::WebContents* contents = FindTab(target_id);
    if (contents && SessionOf(contents) == session_id) {
      DiscardTab(contents);
    }
  }
}

void SessionManager::UpdateIdleTimer() {
  const bool needed = std::ranges::any_of(sessions_, [](const auto& entry) {
    return entry.second.idle_timeout.is_positive();
  });
  if (!needed) {
    idle_timer_.Stop();
  } else if (!idle_timer_.IsRunning()) {
    idle_timer_.Start(FROM_HERE, kIdleCheckInterval,
                      base::BindRepeating(&SessionManager::CheckIdle,
                                          base::Unretained(this)));
  }
}

void SessionManager::CheckIdle() {
  if (browser_shutdown::IsTryingToQuit()) {
    return;
  }
  const base::TimeTicks now = base::TimeTicks::Now();
  std::vector<std::string> idle;
  for (auto& [id, session] : sessions_) {
    // A running tool call (e.g. a long wait-for) is activity until it ends;
    // each call has its own bounded timeout.
    if (!session.idle_timeout.is_positive() || session.pending_calls > 0 ||
        now - session.last_activity_ticks < session.idle_timeout) {
      continue;
    }
    // A person looking at one of its tabs keeps it open.
    if (std::ranges::any_of(TabsOf(id), &IsInFrontOfUser)) {
      Touch(id);
      continue;
    }
    idle.push_back(id);
  }
  for (const std::string& id : idle) {
    auto it = sessions_.find(id);
    if (it == sessions_.end()) {
      continue;
    }
    LOG(WARNING) << "Shuttle: closing session " << id << " after "
                 << it->second.idle_timeout.InSeconds()
                 << " s without activity";
    Close(id);
  }
}

#if BUILDFLAG(IS_LINUX)
void SessionManager::ReleaseKeepAliveForQuit() {
  quit_watcher_.reset();
  sessions_profile_keep_alive_.reset();
  sessions_keep_alive_.reset();
}

void SessionManager::UpdateKeepAlive() {
  if (browser_shutdown::IsTryingToQuit()) {
    return;
  }
  if (sessions_.empty()) {
    quit_watcher_.reset();
    sessions_profile_keep_alive_.reset();
    sessions_keep_alive_.reset();
  } else if (!sessions_keep_alive_) {
    sessions_keep_alive_ = std::make_unique<ScopedKeepAlive>(
        KeepAliveOrigin::REMOTE_DEBUGGING, KeepAliveRestartOption::DISABLED);
    sessions_profile_keep_alive_ = std::make_unique<ScopedProfileKeepAlive>(
        profile_, ProfileKeepAliveOrigin::kRemoteDebugging);
    quit_watcher_ = std::make_unique<QuitWatcher>(this);
  }
}
#endif

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
  std::vector<content::WebContents*> tabs = TabsOf(session_id);
  // The user's active tab is one of the session's: the home tab gets focus
  // (the tabs close one by one; the next-tab rule would pick a sibling).
  for (content::WebContents* contents : tabs) {
    TabLocation location = FindInTabStrips(contents);
    if (location.tab_strip &&
        location.tab_strip->active_index() == location.index) {
      const int home = location.tab_strip->GetShuttleHomeTabIndex();
      if (home != TabStripModel::kNoTab) {
        location.tab_strip->ActivateTabAt(home);
      }
    }
  }
  // Discarded, not closed: a beforeunload dialog would keep a tab whose
  // session is gone.
  for (content::WebContents* contents : tabs) {
    DiscardTab(contents);
  }
  sessions_.erase(session_id);
  UpdateIdleTimer();
  registry_->FlushCookies(agent_id);
  if (session_closed_callback_) {
    session_closed_callback_.Run(session_id);
  }
#if BUILDFLAG(IS_LINUX)
  // Last: releasing the keep-alive can start shutdown (no windows left).
  UpdateKeepAlive();
#endif
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
  {
    // A window this opens is the agent's: it closes with the agent's tabs.
    base::AutoReset<bool> agent_tab = home_tabs_->OpeningAgentTab();
    Navigate(&params);
  }
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

// static
void SessionManager::SelectTab(content::WebContents* contents) {
  for (BrowserWindowInterface* browser : GetAllBrowserWindowInterfaces()) {
    TabStripModel* tab_strip = browser->GetTabStripModel();
    int index = tab_strip ? tab_strip->GetIndexOfWebContents(contents)
                          : TabStripModel::kNoTab;
    if (index == TabStripModel::kNoTab) {
      continue;
    }
    ui::BaseWindow* window = browser->GetWindow();
    if (index != tab_strip->active_index() && !(window && window->IsActive())) {
      tab_strip->ActivateTabAt(index);
    }
    return;
  }
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

void SessionManager::DiscardTab(content::WebContents* contents) {
  TabLocation location = FindInTabStrips(contents);
  if (location.tab_strip) {
    location.tab_strip->DetachAndDeleteWebContentsAt(location.index);
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
  // Keep the group's collapsed state: it lives in the same visual data, and
  // the default (expanded) would undo every collapse.
  tab_groups::TabGroupVisualData visuals(
      base::UTF8ToUTF16(agent->name), agent->color,
      tab_strip->IsGroupCollapsed(group->second));
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
  // Collapsing or expanding the group changes no identity.
  if (visuals->old_visuals &&
      visuals->old_visuals->title() == visuals->new_visuals->title() &&
      visuals->old_visuals->color() == visuals->new_visuals->color()) {
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

void SessionManager::OnTabStripModelChanged(
    TabStripModel* tab_strip,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (selection.active_tab_changed() && selection.new_contents) {
    const std::string session_id = SessionOf(selection.new_contents);
    if (!session_id.empty()) {
      Touch(session_id);
    }
  }
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
