// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tab_contents/chrome_web_contents_view_handle_drop.h"

#include <utility>

#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_view_delegate.h"
#include "content/public/common/drop_data.h"

void HandleOnPerformingDrop(
    content::WebContents* web_contents,
    content::DropData drop_data,
    content::WebContentsViewDelegate::DropCompletionCallback callback) {
  CHECK(callback);

  // Shuttle: no content analysis, so there is nothing to scan: forward the drop
  // unchanged (as upstream does without content analysis).
  if (web_contents->GetDelegate()) {
    web_contents->GetDelegate()->HandleDragEnded();
  }
  std::move(callback).Run(std::move(drop_data));
}
