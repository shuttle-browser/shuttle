// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_SEARCH_H_
#define CHROME_BROWSER_SEARCH_SEARCH_H_

#include <optional>

#include "build/build_config.h"

class GURL;
class Profile;

namespace content {
class BrowserContext;
class NavigationEntry;
class SecurityPrincipal;
class WebContents;
}

namespace search {

// UserData key to tag RenderProcessHosts hosting the NTP in chrome/.
extern const void* const kIsNTPProcessKey;

// Returns whether Google is selected as the default search engine.
bool DefaultSearchProviderIsGoogle(Profile* profile);

// Returns true if |url| is the New Tab page (chrome://newtab).
bool IsNTPOrRelatedURL(const GURL& url, Profile* profile);

// Returns whether a |url| corresponds to a New Tab page.
bool IsNTPURL(const GURL& url);

// Returns whether the Web UI NTP is enabled for Desktop Android. In all other
// cases and platforms, it returns false.
bool IsWebUiNtpEnabledForDesktopAndroid();

// Returns true if |url| corresponds to a Split View New Tab page.
bool IsSplitViewNewTabPage(const GURL& url);

}  // namespace search

#endif  // CHROME_BROWSER_SEARCH_SEARCH_H_
