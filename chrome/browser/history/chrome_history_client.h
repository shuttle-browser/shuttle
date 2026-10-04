// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_CHROME_HISTORY_CLIENT_H_
#define CHROME_BROWSER_HISTORY_CHROME_HISTORY_CLIENT_H_

#include <memory>
#include <set>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "components/history/core/browser/history_client.h"

class GURL;

// This class implements history::HistoryClient to abstract operations that
// depend on Chrome environment.
class ChromeHistoryClient : public history::HistoryClient {
 public:
  ChromeHistoryClient();

  ChromeHistoryClient(const ChromeHistoryClient&) = delete;
  ChromeHistoryClient& operator=(const ChromeHistoryClient&) = delete;

  ~ChromeHistoryClient() override;

  // history::HistoryClient implementation.
  void OnHistoryServiceCreated(
      history::HistoryService* history_service) override;
  void Shutdown() override;
  history::CanAddURLCallback GetThreadSafeCanAddURLCallback() const override;
  void NotifyProfileError(sql::InitStatus init_status,
                          const std::string& diagnostics) override;
  std::unique_ptr<history::HistoryBackendClient> CreateBackendClient() override;
  void UpdateBookmarkLastUsedTime(int64_t bookmark_node_id,
                                  base::Time time) override;

};

#endif  // CHROME_BROWSER_HISTORY_CHROME_HISTORY_CLIENT_H_
