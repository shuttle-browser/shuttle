// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_data_importer/utility/bookmark_util.h"

#include "base/strings/utf_string_conversions.h"
#include "components/reading_list/core/reading_list_model.h"

namespace user_data_importer {

size_t ImportReadingList(ReadingListModel* reading_list_model,
                         std::vector<ImportedBookmarkEntry> reading_list) {
  if (reading_list.empty() || !reading_list_model) {
    return 0;
  }

  size_t imported_count = 0u;

  auto scoped_reading_list_batch_update =
      reading_list_model->BeginBatchUpdates();

  for (const ImportedBookmarkEntry& reading_list_entry : reading_list) {
    if (!reading_list_entry.url.is_valid()) {
      continue;
    }
    reading_list_model->AddOrReplaceEntry(
        reading_list_entry.url, base::UTF16ToUTF8(reading_list_entry.title),
        reading_list::ADDED_VIA_IMPORT,
        /*estimated_read_time=*/std::nullopt, reading_list_entry.creation_time);
    ++imported_count;
  }

  return imported_count;
}

}  // namespace user_data_importer
