// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/extensions_activity_monitor.h"

#include "components/sync/base/extensions_activity.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace browser_sync {

ExtensionsActivityMonitor::ExtensionsActivityMonitor(
    content::BrowserContext* context)
    : extensions_activity_(base::MakeRefCounted<syncer::ExtensionsActivity>()) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

ExtensionsActivityMonitor::~ExtensionsActivityMonitor() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

const scoped_refptr<syncer::ExtensionsActivity>&
ExtensionsActivityMonitor::GetExtensionsActivity() {
  return extensions_activity_;
}

}  // namespace browser_sync
