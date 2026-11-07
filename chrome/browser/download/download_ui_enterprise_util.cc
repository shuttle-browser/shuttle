// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_ui_enterprise_util.h"

namespace download {

// Shuttle: enterprise download connectors are removed, so no policy can block a
// download here.
bool DoesDownloadConnectorBlock(Profile* profile, const GURL& url) {
  return false;
}

}  // namespace download
