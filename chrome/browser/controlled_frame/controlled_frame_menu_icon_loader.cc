// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/controlled_frame/controlled_frame_menu_icon_loader.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/types/expected.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "extensions/common/extension.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"

namespace controlled_frame {

ControlledFrameMenuIconLoader::ControlledFrameMenuIconLoader() = default;
ControlledFrameMenuIconLoader::~ControlledFrameMenuIconLoader() = default;

void ControlledFrameMenuIconLoader::LoadIcon(
    content::BrowserContext* context,
    const extensions::Extension* extension,
    const extensions::MenuItem::ExtensionKey& extension_key) {
  // Controlled Frame requires Isolated Web Apps, which have been removed.
  NOTREACHED();
}

gfx::Image ControlledFrameMenuIconLoader::GetIcon(
    const extensions::MenuItem::ExtensionKey& extension_key) {
  auto it = icons_.find(extension_key);
  CHECK(it != icons_.end());

  gfx::Image result = it->second;
  return result;
}

void ControlledFrameMenuIconLoader::RemoveIcon(
    const extensions::MenuItem::ExtensionKey& extension_key) {
  size_t count = icons_.erase(extension_key);
  count += pending_icons_.erase(extension_key);
  // The icon should only be present in one of these two containers.
  CHECK_EQ(1u, count);
}

void ControlledFrameMenuIconLoader::SetNotifyOnLoadedCallbackForTesting(
    base::RepeatingClosure callback) {
  on_loaded_callback_ = callback;
}

void ControlledFrameMenuIconLoader::OnIconLoaded(
    const extensions::MenuItem::ExtensionKey& extension_key,
    SkBitmap bitmap) {
  gfx::Image image = gfx::Image::CreateFrom1xBitmap(bitmap);
  CHECK(!image.IsEmpty());

  // The icon may have been removed while waiting for it to load. Do nothing in
  // that case.
  if (pending_icons_.erase(extension_key) == 0) {
    return;
  }

  if (image.Height() != gfx::kFaviconSize ||
      image.Width() != gfx::kFaviconSize) {
    gfx::Size favicon_size(gfx::kFaviconSize, gfx::kFaviconSize);
    gfx::ImageSkia scaled_image = gfx::ImageSkiaOperations::CreateResizedImage(
        image.AsImageSkia(), skia::ImageOperations::ResizeMethod::RESIZE_GOOD,
        favicon_size);

    image = gfx::Image(scaled_image);
  }

  CHECK_EQ(gfx::kFaviconSize, image.Width());
  CHECK_EQ(gfx::kFaviconSize, image.Height());
  icons_[extension_key] = gfx::Image(image);

  if (on_loaded_callback_) {
    on_loaded_callback_.Run();
  }
}

}  // namespace controlled_frame
