// Copyright 2026 The Agent Chrome Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AGENT_CHROME_PREF_NAMES_H_
#define CHROME_BROWSER_AGENT_CHROME_PREF_NAMES_H_

namespace agent_chrome::prefs {

// Dictionary of agent profiles: id -> {"created": <base::Time as double>}.
inline constexpr char kProfiles[] = "agent_chrome.profiles";

// List of deleted profile ids whose storage still has to be removed from
// disk (the partition was loaded when the profile was deleted).
inline constexpr char kPendingDeletes[] = "agent_chrome.pending_deletes";

}  // namespace agent_chrome::prefs

#endif  // CHROME_BROWSER_AGENT_CHROME_PREF_NAMES_H_
