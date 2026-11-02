// Copyright 2026 Silkweave
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHUTTLE_HOME_TAB_H_
#define CHROME_BROWSER_SHUTTLE_HOME_TAB_H_

class GURL;

namespace content {
class WebContents;
}

namespace shuttle {

// The home tab: chrome://agents, pinned at index 0 of every normal window of
// the Shuttle profile. It closes only with its window; it can't be unpinned,
// moved, grouped or dragged out. HomeTabManager creates it; the tab strip
// guards ask IsHomeTab().
inline constexpr char kHomeTabURL[] = "chrome://agents/";

// True if `contents` is a window's home tab (a marker set at creation, not a
// URL match: other chrome://agents tabs are ordinary tabs).
bool IsHomeTab(const content::WebContents* contents);

// Marks `contents` as its window's home tab.
void MarkHomeTab(content::WebContents* contents);

// True for chrome://agents URLs.
bool IsAgentsURL(const GURL& url);

}  // namespace shuttle

#endif  // CHROME_BROWSER_SHUTTLE_HOME_TAB_H_
