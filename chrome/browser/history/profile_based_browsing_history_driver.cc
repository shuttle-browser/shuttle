// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/profile_based_browsing_history_driver.h"

#include <utility>

#include "chrome/browser/profiles/profile.h"
#include "components/history/core/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "extensions/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
#include "chrome/browser/extensions/activity_log/activity_log.h"
#endif

void ProfileBasedBrowsingHistoryDriver::OnRemoveVisits(
    const std::vector<history::ExpireHistoryArgs>& expire_list) {
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  // If the profile has activity logging enabled also clean up any URLs from the
  // extension activity log. The extension activity log contains URLS which
  // websites an extension has activity on so it will indirectly contain
  // websites that a user has visited.
  extensions::ActivityLog* activity_log =
      extensions::ActivityLog::GetInstance(GetProfile());
  for (const history::ExpireHistoryArgs& expire_entry : expire_list) {
    activity_log->RemoveURLs(expire_entry.urls);
  }
#endif
}

bool ProfileBasedBrowsingHistoryDriver::AllowHistoryDeletions() {
  return GetProfile()->GetPrefs()->GetBoolean(
      prefs::kAllowDeletingBrowserHistory);
}
