// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_EXTENSIONS_ACTIVITY_MONITOR_H_
#define CHROME_BROWSER_SYNC_GLUE_EXTENSIONS_ACTIVITY_MONITOR_H_

#include "base/memory/scoped_refptr.h"
#include "content/public/browser/browser_context.h"

namespace syncer {
class ExtensionsActivity;
}

namespace browser_sync {

// Holds the ExtensionsActivity record handed to sync. The bookmarks extension
// API it used to observe has been removed, so no activity is recorded.
class ExtensionsActivityMonitor {
 public:
  explicit ExtensionsActivityMonitor(content::BrowserContext* context);

  ExtensionsActivityMonitor(const ExtensionsActivityMonitor&) = delete;
  ExtensionsActivityMonitor& operator=(const ExtensionsActivityMonitor&) =
      delete;

  ~ExtensionsActivityMonitor();

  const scoped_refptr<syncer::ExtensionsActivity>& GetExtensionsActivity();

 private:
  scoped_refptr<syncer::ExtensionsActivity> extensions_activity_;
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_EXTENSIONS_ACTIVITY_MONITOR_H_
