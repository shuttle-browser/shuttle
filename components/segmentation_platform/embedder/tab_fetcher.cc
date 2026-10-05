// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/tab_fetcher.h"

#include "base/memory/raw_ptr.h"
#include "base/notimplemented.h"
#include "base/time/time.h"

namespace segmentation_platform {
TabFetcher::TabEntry::TabEntry(SessionID tab_id, const std::string& session_tag)
    : session_tag(session_tag), tab_id(tab_id), web_contents_data(nullptr) {}

TabFetcher::TabEntry::TabEntry(SessionID tab_id,
                               content::WebContents* webcontents,
                               TabAndroid* tab_android)
    : tab_id(tab_id),
      web_contents_data(webcontents),
      tab_android_data(tab_android) {}

bool TabFetcher::FillAllLocalTabs(std::vector<TabEntry>& tabs) {
  return FillAllLocalTabsFromTabModel(tabs);
}

TabFetcher::Tab TabFetcher::FindTab(const TabEntry& entry) {
  return FindLocalTab(entry);
}

bool TabFetcher::FillAllLocalTabsFromTabModel(std::vector<TabEntry>& tabs) {
  NOTIMPLEMENTED();
  return false;
}

TabFetcher::Tab TabFetcher::FindLocalTab(const TabEntry& entry) {
  NOTIMPLEMENTED();
  return Tab{};
}

}  // namespace segmentation_platform
