// Copyright 2026 Silkweave
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHUTTLE_TAB_PLACEMENT_H_
#define CHROME_BROWSER_SHUTTLE_TAB_PLACEMENT_H_

#include "base/memory/raw_ptr.h"

class BrowserWindowInterface;

namespace shuttle {

// Where a tab opened for the user in an agent goes
// (ShuttleService::OpenNewTabInAgent, SessionManager::OpenTabForUser).
struct TabPlacement {
  // Null: wherever agent tabs open (the profile's last active window).
  raw_ptr<BrowserWindowInterface> browser = nullptr;
  // Index in `browser`'s tab strip; -1 = the end. The tab then joins the
  // agent's tab group, which can move it next to the group.
  int index = -1;
  // Activate the tab and bring its window to the front.
  bool foreground = true;
};

}  // namespace shuttle

#endif  // CHROME_BROWSER_SHUTTLE_TAB_PLACEMENT_H_
