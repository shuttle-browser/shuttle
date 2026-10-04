// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/chrome_history_client.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/time/time.h"
#include "chrome/browser/history/chrome_history_backend_client.h"
#include "chrome/browser/history/history_utils.h"
#include "chrome/browser/profiles/sql_init_error_message_ids.h"
#include "chrome/browser/ui/profiles/profile_error_dialog.h"
#include "components/history/core/browser/history_service.h"

ChromeHistoryClient::ChromeHistoryClient() = default;

ChromeHistoryClient::~ChromeHistoryClient() = default;

void ChromeHistoryClient::OnHistoryServiceCreated(
    history::HistoryService* history_service) {}

void ChromeHistoryClient::Shutdown() {}

history::CanAddURLCallback ChromeHistoryClient::GetThreadSafeCanAddURLCallback()
    const {
  return base::BindRepeating(&CanAddURLToHistory);
}

void ChromeHistoryClient::NotifyProfileError(sql::InitStatus init_status,
                                             const std::string& diagnostics) {
  ShowProfileErrorDialog(ProfileErrorType::HISTORY,
                         SqlInitStatusToMessageId(init_status), diagnostics);
}

std::unique_ptr<history::HistoryBackendClient>
ChromeHistoryClient::CreateBackendClient() {
  return std::make_unique<ChromeHistoryBackendClient>();
}

void ChromeHistoryClient::UpdateBookmarkLastUsedTime(int64_t bookmark_node_id,
                                                     base::Time time) {}
