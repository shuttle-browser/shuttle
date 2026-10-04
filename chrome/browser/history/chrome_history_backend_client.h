// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_CHROME_HISTORY_BACKEND_CLIENT_H_
#define CHROME_BROWSER_HISTORY_CHROME_HISTORY_BACKEND_CLIENT_H_

#include "components/history/core/browser/history_backend_client.h"

// ChromeHistoryBackendClient implements history::HistoryBackendClient interface
// to provides access to embedder-specific features.
class ChromeHistoryBackendClient : public history::HistoryBackendClient {
 public:
  ChromeHistoryBackendClient();

  ChromeHistoryBackendClient(const ChromeHistoryBackendClient&) = delete;
  ChromeHistoryBackendClient& operator=(const ChromeHistoryBackendClient&) =
      delete;

  ~ChromeHistoryBackendClient() override;

  // history::HistoryBackendClient implementation.
  bool IsPinnedURL(const GURL& url) override;
  std::vector<history::URLAndTitle> GetPinnedURLs() override;
  bool IsWebSafe(const GURL& url) override;
};

#endif  // CHROME_BROWSER_HISTORY_CHROME_HISTORY_BACKEND_CLIENT_H_
