// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/chrome_history_backend_client.h"

#include "content/public/browser/child_process_security_policy.h"
#include "url/gurl.h"

ChromeHistoryBackendClient::ChromeHistoryBackendClient() = default;

ChromeHistoryBackendClient::~ChromeHistoryBackendClient() = default;

bool ChromeHistoryBackendClient::IsPinnedURL(const GURL& url) {
  // Bookmarks have been removed, so no URL is pinned.
  return false;
}

std::vector<history::URLAndTitle> ChromeHistoryBackendClient::GetPinnedURLs() {
  return {};
}

bool ChromeHistoryBackendClient::IsWebSafe(const GURL& url) {
  return content::ChildProcessSecurityPolicy::GetInstance()->IsWebSafeScheme(
      url.GetScheme());
}
