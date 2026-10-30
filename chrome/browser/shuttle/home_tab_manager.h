// Copyright 2026 Silkweave
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHUTTLE_HOME_TAB_MANAGER_H_
#define CHROME_BROWSER_SHUTTLE_HOME_TAB_MANAGER_H_

#include <set>

#include "base/auto_reset.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/browser_window/public/browser_collection_observer.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"

class BrowserWindowInterface;
class Profile;

namespace shuttle {

// Gives every normal window of `profile` its home tab (home_tab.h): pinned
// chrome://agents at index 0, added once the window has its first tab. A
// window an agent opened (no window was open for its tab) closes once only
// the home tab is left; a window the user opened stays.
class HomeTabManager : public BrowserCollectionObserver,
                       public TabStripModelObserver {
 public:
  explicit HomeTabManager(Profile* profile);
  HomeTabManager(const HomeTabManager&) = delete;
  HomeTabManager& operator=(const HomeTabManager&) = delete;
  ~HomeTabManager() override;

  // Windows created while the returned scope lives are agent windows.
  [[nodiscard]] base::AutoReset<bool> OpeningAgentTab();

  // BrowserCollectionObserver:
  void OnBrowserCreated(BrowserWindowInterface* browser) override;
  void OnBrowserClosed(BrowserWindowInterface* browser) override;

  // TabStripModelObserver:
  void OnTabStripModelChanged(TabStripModel* tab_strip,
                              const TabStripModelChange& change,
                              const TabStripSelectionChange& selection) override;

 private:
  // Coalesced: at most one pending Update per window.
  void ScheduleUpdate(TabStripModel* tab_strip);
  // Adds the window's home tab if it has none; closes an agent window that
  // holds nothing else. While a tab drag holds the strip it retries until the
  // strip is editable again or the window is gone.
  void Update(base::WeakPtr<BrowserWindowInterface> browser);

  raw_ptr<Profile> profile_;
  bool opening_agent_tab_ = false;
  std::set<raw_ptr<BrowserWindowInterface>> agent_windows_;
  // Windows with an Update posted (erased when it runs or the window closes).
  std::set<raw_ptr<BrowserWindowInterface>> pending_;
  base::ScopedObservation<GlobalBrowserCollection, BrowserCollectionObserver>
      collection_observation_{this};
  base::WeakPtrFactory<HomeTabManager> weak_factory_{this};
};

}  // namespace shuttle

#endif  // CHROME_BROWSER_SHUTTLE_HOME_TAB_MANAGER_H_
