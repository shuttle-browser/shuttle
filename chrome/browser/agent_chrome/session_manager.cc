// Copyright 2026 The Agent Chrome Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/agent_chrome/session_manager.h"

#include <algorithm>
#include <array>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/containers/span.h"
#include "base/hash/hash.h"
#include "base/rand_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/agent_chrome/profile_registry.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/navigator/browser_navigator.h"
#include "chrome/browser/ui/navigator/browser_navigator_params.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/tab_groups/tab_group_color.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/geometry/rect.h"
#include "url/url_constants.h"

namespace agent_chrome {

namespace {

// Chrome's minimum window size doesn't apply to a hidden tab's viewport, but
// keep sizes sane.
constexpr int kMinViewport = 100;
constexpr int kMaxViewport = 8192;

// Lease of a tab to a session: keeps the tab rendering like a foreground tab
// (a capture handle) and re-applies its viewport size after activation.
class AgentTabLease : public content::WebContentsUserData<AgentTabLease>,
                      public content::WebContentsObserver {
 public:
  ~AgentTabLease() override = default;

  const std::string& session_id() const { return session_id_; }
  const std::optional<gfx::Size>& viewport() const { return viewport_; }
  void set_viewport(const gfx::Size& size) { viewport_ = size; }

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

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

WEB_CONTENTS_USER_DATA_KEY_IMPL(AgentTabLease);

struct TabLocation {
  raw_ptr<TabStripModel> tab_strip = nullptr;
  int index = TabStripModel::kNoTab;
};

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

tab_groups::TabGroupColorId ColorFor(const std::string& profile_id) {
  static constexpr std::array kColors = {
      tab_groups::TabGroupColorId::kBlue,   tab_groups::TabGroupColorId::kRed,
      tab_groups::TabGroupColorId::kYellow, tab_groups::TabGroupColorId::kGreen,
      tab_groups::TabGroupColorId::kPink,   tab_groups::TabGroupColorId::kPurple,
      tab_groups::TabGroupColorId::kCyan,   tab_groups::TabGroupColorId::kOrange,
  };
  return kColors[base::PersistentHash(profile_id) % kColors.size()];
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

const Session& SessionManager::Open(const std::string& profile_id,
                                    const std::string& agent) {
  std::string id = NewSessionId();
  while (sessions_.contains(id)) {
    id = NewSessionId();
  }
  Session& session = sessions_[id];
  session.id = id;
  session.profile = profile_id;
  session.agent = agent;
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
  std::string profile_id = it->second.profile;
  for (content::WebContents* contents : TabsOf(session_id)) {
    ReleaseTab(contents);
  }
  sessions_.erase(session_id);
  registry_->FlushCookies(profile_id);
  return true;
}

void SessionManager::CloseProfileSessions(const std::string& profile_id) {
  std::vector<std::string> ids;
  for (const auto& [id, session] : sessions_) {
    if (session.profile == profile_id) {
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

  content::StoragePartitionConfig config =
      registry_->PartitionConfig(session->profile);
  content::WebContents::CreateParams create_params(
      profile_,
      content::SiteInstance::CreateForFixedStoragePartition(profile_, url,
                                                            config));
  create_params.initially_hidden = true;
  std::unique_ptr<content::WebContents> new_contents =
      content::WebContents::Create(create_params);
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
    lease->set_viewport(size);
  }
  // The active tab follows the window; its size is applied once it is back
  // in the background.
  TabLocation location = FindInTabStrips(contents);
  if (!location.tab_strip ||
      location.tab_strip->active_index() != location.index) {
    contents->Resize(gfx::Rect(size));
  }
  return size;
}

// static
std::string SessionManager::SessionOf(content::WebContents* contents) {
  AgentTabLease* lease = AgentTabLease::FromWebContents(contents);
  return lease ? lease->session_id() : std::string();
}

// static
content::WebContents* SessionManager::FindTab(const std::string& target_id) {
  scoped_refptr<content::DevToolsAgentHost> host =
      content::DevToolsAgentHost::GetForId(target_id);
  return host ? host->GetWebContents() : nullptr;
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
  Session& session = it->second;
  TabLocation location = FindInTabStrips(contents.get());
  if (!location.tab_strip) {
    return;
  }
  TabStripModel* tab_strip = location.tab_strip;
  if (session.group && tab_strip->group_model() &&
      tab_strip->group_model()->ContainsTabGroup(*session.group)) {
    if (tab_strip->GetTabGroupForTab(location.index) != session.group) {
      tab_strip->AddToExistingGroup({location.index}, *session.group);
    }
    return;
  }
  session.group = tab_strip->AddToNewGroup({location.index});
  tab_strip->ChangeTabGroupVisuals(
      *session.group,
      tab_groups::TabGroupVisualData(
          base::UTF8ToUTF16(
              base::StrCat({session.profile, " · ", session.agent})),
          ColorFor(session.profile)));
}

}  // namespace agent_chrome
