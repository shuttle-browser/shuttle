// Copyright 2026 Silkweave
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHUTTLE_SWITCHES_H_
#define CHROME_BROWSER_SHUTTLE_SWITCHES_H_

namespace shuttle {

// With no URL arguments, open a window with chrome://agents (the user's home,
// like a Dock click on macOS). Used by the Linux desktop entry, so a menu
// launch shows a window while terminal and agent launches stay windowless.
// Also applies to a launch forwarded to a running browser.
inline constexpr char kOpenAgentsSwitch[] = "shuttle-open-agents";

}  // namespace shuttle

#endif  // CHROME_BROWSER_SHUTTLE_SWITCHES_H_
