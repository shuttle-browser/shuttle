// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/shortcuts/document_icon_fetcher_task.h"

#include <iterator>
#include <memory>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/types/expected.h"
#include "chrome/browser/shortcuts/shortcut_icon_generator.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/favicon/favicon_url.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/size.h"

namespace shortcuts {

DocumentIconFetcherTask::DocumentIconFetcherTask(
    content::WebContents& web_contents,
    FetchIconsFromDocumentCallback callback)
    : web_contents_(web_contents.GetWeakPtr()),
      fallback_letter_(GenerateIconLetterFromName(web_contents.GetTitle())),
      final_callback_(std::move(callback)) {}

DocumentIconFetcherTask::~DocumentIconFetcherTask() {
  // If the final_callback_ has not been run yet, prevent that from hanging by
  // returning an error message. Although this function calls
  // `OnIconFetchingCompleteSelfDestruct()`, for this use-case, self-destruction
  // will not occur since this is being triggered as part of destruction itself.
  if (!final_callback_.is_null()) {
    OnIconFetchingCompleteSelfDestruct(
        base::unexpected(FetchIconsForDocumentError::kTaskDestroyed));
  }
}

void DocumentIconFetcherTask::StartIconFetching() {
  // Only favicons are used: the web page metadata agent (manifest-less icon
  // links) was part of the removed web app stack.
  std::vector<GURL> icons;
  for (const auto& favicon_url : web_contents_->GetFaviconURLs()) {
    icons.push_back(favicon_url->icon_url);
  }

  // Eliminate duplicates.
  base::flat_set<GURL> icon_set(std::move(icons));
  num_pending_image_requests_ = icon_set.size();

  for (const GURL& url : icon_set) {
    web_contents_->DownloadImage(
        url, /*is_favicon=*/true,
        /*preferred_size=*/gfx::Size(),
        /*max_bitmap_size=*/0, /*bypass_cache=*/false,
        base::BindOnce(&DocumentIconFetcherTask::DidDownloadFavicon,
                       weak_factory_.GetWeakPtr()));
  }
  MaybeCompleteImageDownloadAndSelfDestruct();
}

void DocumentIconFetcherTask::DidDownloadFavicon(
    int id,
    int http_status_code,
    const GURL& image_url,
    const std::vector<SkBitmap>& bitmaps,
    const std::vector<gfx::Size>& sizes) {
  icons_.reserve(icons_.size() + bitmaps.size());
  for (const SkBitmap& bitmap : bitmaps) {
    if (bitmap.drawsNothing()) {
      continue;
    }
    icons_.push_back(bitmap);
  }
  --num_pending_image_requests_;
  MaybeCompleteImageDownloadAndSelfDestruct();
}

void DocumentIconFetcherTask::MaybeCompleteImageDownloadAndSelfDestruct() {
  if (num_pending_image_requests_ > 0) {
    return;
  }
  OnIconFetchingCompleteSelfDestruct(base::ok(std::move(icons_)));
}

void DocumentIconFetcherTask::OnIconFetchingCompleteSelfDestruct(
    FetchIconsFromDocumentResult result) {
  if (result.has_value() && result->empty()) {
    result->push_back(GenerateBitmap(128, fallback_letter_));
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(final_callback_), result));
}

}  // namespace shortcuts
