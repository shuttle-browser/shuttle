// Copyright 2026 Silkweave
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHUTTLE_PREF_NAMES_H_
#define CHROME_BROWSER_SHUTTLE_PREF_NAMES_H_

namespace shuttle::prefs {

// Dictionary of agent profiles: id -> {"created": <base::Time as value>,
// optional "name", "color" (TabGroupColorId as int), "avatar" (URL)}.
inline constexpr char kProfiles[] = "shuttle.profiles";

// List of deleted profile ids whose storage still has to be removed from
// disk (the partition was loaded when the profile was deleted).
inline constexpr char kPendingDeletes[] = "shuttle.pending_deletes";

}  // namespace shuttle::prefs

#endif  // CHROME_BROWSER_SHUTTLE_PREF_NAMES_H_
