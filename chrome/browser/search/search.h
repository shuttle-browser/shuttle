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

namespace url {
class Origin;
}

namespace search {

// UserData key to tag RenderProcessHosts hosting the NTP in chrome/.
extern const void* const kIsNTPProcessKey;

// Returns whether Google is selected as the default search engine.
bool DefaultSearchProviderIsGoogle(Profile* profile);

// SECURITY: These are exact-match predicates. Besides UI and metrics
// classification, they back security decisions, including (non-exhaustively)
// the permission embedding-origin bypass and DSE canonicalization, the
// permission-prompt omnibox-state bypass, one-time-permission exclusion,
// webRequest hiding, the Unbounded Element allowlist, process-per-site and
// actor gating. IsNTPURL() and IsInstantNTPURL() are built from the GURL
// predicates, so changing those also changes site-isolation and navigation
// decisions such as
// ChromeContentBrowserClient::ShouldStayInParentProcessForNTP() and
// OverrideNavigationParams().
//  - `GURL` overloads compare `url`'s own scheme (`chrome://`) and host. They
//    do not unwrap `blob:` or `filesystem:` inner URLs.
//  - `url::Origin` overloads (chrome://newtab and chrome://new-tab-page only)
//    return false for every opaque origin, including opaque origins whose
//    precursor is an NTP origin.
//  - `Is*SecurityPrincipal()` functions compare the principal's site URL
//    scheme and host.
// Do not broaden these predicates (e.g. to `chrome-untrusted://` or
// `chrome-extension://` origins). If another surface must be treated as an
// NTP, add a separately named predicate and opt call sites in individually,
// with security review for the security-sensitive ones.

// Returns true if `url`, `origin`, or `security_principal` has the
// `chrome://newtab` scheme and host (on desktop, the virtual URL for
// regular-profile NTPs and the committed URL for generic/incognito/guest
// fallback NTPs; when checking a NavigationEntry's actual URL for an NTP tab,
// callers typically also check `search::NavEntryIsInstantNTP`).
bool IsNewTabOrigin(const GURL& url);
bool IsNewTabOrigin(const url::Origin& origin);
bool IsNewTabSecurityPrincipal(
    const content::SecurityPrincipal& security_principal);

// Returns true if `url`, `origin`, or `security_principal` has the 1P WebUI
// New Tab Page scheme and host (`chrome://new-tab-page`).
bool IsNewTabPageOrigin(const GURL& url);
bool IsNewTabPageOrigin(const url::Origin& origin);
bool IsNewTabPageSecurityPrincipal(
    const content::SecurityPrincipal& security_principal);

// Returns true if `url` or `security_principal` has the 3P WebUI New Tab Page
// scheme and host (`chrome://new-tab-page-third-party`).
bool IsThirdPartyNewTabPageOrigin(const GURL& url);
bool IsThirdPartyNewTabPageSecurityPrincipal(
    const content::SecurityPrincipal& security_principal);

// Returns true if `url` has any of the three top-level `chrome://` WebUI New
// Tab Page hosts (`chrome://newtab`, `chrome://new-tab-page`, or
// `chrome://new-tab-page-third-party`). Does not match `chrome-untrusted://`.
bool IsWebUINewTabPageOrigin(const GURL& url);

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
