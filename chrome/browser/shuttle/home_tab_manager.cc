// Copyright 2026 Silkweave
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/shuttle/home_tab_manager.h"

#include <memory>
#include <optional>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/shuttle/home_tab.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/browser_window/public/desktop_browser_window_capabilities.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_delegate.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/referrer.h"
#include "ui/base/base_window.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace shuttle {

namespace {

// A tab drag holds the strip (IsTabStripEditable() false): retry every
// 250 ms until it ends.
constexpr base::TimeDelta kDragRetryDelay = base::Milliseconds(250);

bool IsHomeWindow(BrowserWindowInterface* browser, Profile* profile) {
  return browser->GetProfile() == profile &&
         browser->GetType() == BrowserWindowInterface::TYPE_NORMAL;
}

}  // namespace

HomeTabManager::HomeTabManager(Profile* profile) : profile_(profile) {
  for (BrowserWindowInterface* browser : GetAllBrowserWindowInterfaces()) {
    OnBrowserCreated(browser);
    ScheduleUpdate(browser->GetTabStripModel());
  }
  collection_observation_.Observe(GlobalBrowserCollection::GetInstance());
}

HomeTabManager::~HomeTabManager() = default;

base::AutoReset<bool> HomeTabManager::OpeningAgentTab() {
  return base::AutoReset<bool>(&opening_agent_tab_, true);
}

void HomeTabManager::OnBrowserCreated(BrowserWindowInterface* browser) {
  if (!IsHomeWindow(browser, profile_)) {
    return;
  }
  if (opening_agent_tab_) {
    agent_windows_.insert(browser);
  }
  // The strip is still empty here (callers add tabs after creation); the
  // home tab follows the window's first tab.
  browser->GetTabStripModel()->AddObserver(this);
}

void HomeTabManager::OnBrowserClosed(BrowserWindowInterface* browser) {
  agent_windows_.erase(browser);
  pending_.erase(browser);
  if (TabStripModel* tab_strip = browser->GetTabStripModel()) {
    tab_strip->RemoveObserver(this);
  }
}

void HomeTabManager::OnTabStripModelChanged(
    TabStripModel* tab_strip,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (change.type() == TabStripModelChange::kInserted ||
      change.type() == TabStripModelChange::kRemoved) {
    ScheduleUpdate(tab_strip);
  }
}

void HomeTabManager::ScheduleUpdate(TabStripModel* tab_strip) {
  if (!tab_strip || !tab_strip->delegate()) {
    return;
  }
  BrowserWindowInterface* browser =
      tab_strip->delegate()->GetBrowserWindowInterface();
  if (!browser || !IsHomeWindow(browser, profile_) ||
      !pending_.insert(browser).second) {
    return;
  }
  // Not from inside the model's notification: it can't be changed there.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&HomeTabManager::Update,
                                weak_factory_.GetWeakPtr(),
                                browser->GetWeakPtr()));
}

void HomeTabManager::Update(base::WeakPtr<BrowserWindowInterface> browser) {
  if (!browser) {
    return;  // OnBrowserClosed() dropped its pending entry.
  }
  pending_.erase(browser.get());
  if (browser_shutdown::IsTryingToQuit() ||
      browser->capabilities()->IsAttemptingToCloseBrowser()) {
    return;
  }
  TabStripModel* tab_strip = browser->GetTabStripModel();
  if (!tab_strip || tab_strip->empty() || tab_strip->closing_all()) {
    return;
  }
  if (tab_strip->delegate() && !tab_strip->delegate()->IsTabStripEditable()) {
    pending_.insert(browser.get());
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&HomeTabManager::Update, weak_factory_.GetWeakPtr(),
                       browser),
        kDragRetryDelay);
    return;
  }

  if (tab_strip->GetShuttleHomeTabIndex() != TabStripModel::kNoTab) {
    if (tab_strip->count() == 1 && agent_windows_.contains(browser.get())) {
      // The agent's tabs are gone: the window it opened goes too.
      browser->GetWindow()->Close();
    }
    return;
  }

  // A window that opens with chrome://agents (--shuttle-open-agents, "Manage
  // agents" without a window) keeps that tab as its home tab.
  for (int i = 0; i < tab_strip->count(); ++i) {
    content::WebContents* contents = tab_strip->GetWebContentsAt(i);
    if (IsAgentsURL(contents->GetVisibleURL()) &&
        !tab_strip->GetTabGroupForTab(i) &&
        !tab_strip->GetTabAtIndex(i)->IsSplit()) {
      int index = tab_strip->SetTabPinned(i, true);
      tab_strip->MoveWebContentsAt(index, 0, /*select_after_move=*/false);
      MarkHomeTab(contents);
      return;
    }
  }

  std::unique_ptr<content::WebContents> contents =
      content::WebContents::Create(
          content::WebContents::CreateParams(profile_));
  content::WebContents* home = contents.get();
  MarkHomeTab(home);
  tab_strip->InsertWebContentsAt(0, std::move(contents),
                                 AddTabTypes::ADD_PINNED, std::nullopt);
  home->GetController().LoadURL(GURL(kHomeTabURL), content::Referrer(),
                                ui::PAGE_TRANSITION_AUTO_TOPLEVEL,
                                std::string());
}

}  // namespace shuttle
