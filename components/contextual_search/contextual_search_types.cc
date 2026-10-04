// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_search/contextual_search_types.h"

namespace contextual_search {

bool IsTerminalContextStatus(ContextUploadStatus status) {
  return status == ContextUploadStatus::kUploadFailed ||
         status == ContextUploadStatus::kUploadSuccessful ||
         status == ContextUploadStatus::kValidationFailed ||
         status == ContextUploadStatus::kUploadExpired ||
         status == ContextUploadStatus::kUploadReplaced;
}

}  // namespace contextual_search
