// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/top_chrome/top_chrome_webui_profile.h"

#include "base/check.h"
#include "chrome/browser/profiles/profile.h"

// Shuttle: the shared Top Chrome WebUI profile (upstream 158) is an
// off-the-record Profile without an original profile; the fork's Profile has
// no OTR profiles, so TopChromeWebUIProfileImpl is not built and
// features::kTopChromeWebUISharedRenderer has no effect. Top Chrome WebUIs stay
// in their user profile (pre-158 behaviour).

namespace webui {

void CreateTopChromeWebUIProfile() {}

void DestroyTopChromeWebUIProfile() {}

Profile* GetTopChromeWebUIProfile() {
  return nullptr;
}

Profile& GetProfileForTopChromeWebUIContents(Profile* user_profile) {
  CHECK(user_profile);
  return *user_profile;
}

ScopedTopChromeWebUIProfileForTesting::ScopedTopChromeWebUIProfileForTesting() {
  CreateTopChromeWebUIProfile();
}

ScopedTopChromeWebUIProfileForTesting::
    ~ScopedTopChromeWebUIProfileForTesting() {
  DestroyTopChromeWebUIProfile();
}

}  // namespace webui
