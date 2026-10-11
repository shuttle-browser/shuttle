// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/chrome_safe_browsing_delegate.h"

#include "components/safe_browsing/buildflags.h"

static_assert(BUILDFLAG(FULL_SAFE_BROWSING));

namespace extensions {

ChromeSafeBrowsingDelegate::ChromeSafeBrowsingDelegate() = default;

ChromeSafeBrowsingDelegate::~ChromeSafeBrowsingDelegate() = default;

// The extension telemetry service was removed from this fork, so these are
// all no-ops.
bool ChromeSafeBrowsingDelegate::IsExtensionTelemetryServiceEnabled(
    content::BrowserContext* context) const {
  return false;
}

void ChromeSafeBrowsingDelegate::NotifyExtensionApiTabExecuteScript(
    content::BrowserContext* context,
    const ExtensionId& extension_id,
    const std::string& code) const {}

void ChromeSafeBrowsingDelegate::NotifyExtensionApiDeclarativeNetRequest(
    content::BrowserContext* context,
    const ExtensionId& extension_id,
    const std::vector<api::declarative_net_request::Rule>& rules) const {}

void ChromeSafeBrowsingDelegate::
    NotifyExtensionDeclarativeNetRequestRedirectAction(
        content::BrowserContext* context,
        const ExtensionId& extension_id,
        const GURL& request_url,
        const GURL& redirect_url) const {}

}  // namespace extensions
