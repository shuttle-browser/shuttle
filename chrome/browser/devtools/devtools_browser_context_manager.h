// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_DEVTOOLS_BROWSER_CONTEXT_MANAGER_H_
#define CHROME_BROWSER_DEVTOOLS_DEVTOOLS_BROWSER_CONTEXT_MANAGER_H_

#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/devtools_manager_delegate.h"

// Agent Chrome: incognito (off-the-record) profiles no longer exist, so
// `Target.createBrowserContext` is not supported and only the default browser
// context is exposed to DevTools.
class DevToolsBrowserContextManager {
 public:
  static DevToolsBrowserContextManager& GetInstance();

  DevToolsBrowserContextManager(const DevToolsBrowserContextManager&) = delete;
  DevToolsBrowserContextManager& operator=(
      const DevToolsBrowserContextManager&) = delete;

  Profile* GetProfileById(const std::string& browser_context_id);
  std::vector<base::WeakPtr<content::BrowserContext>> GetBrowserContexts();
  content::BrowserContext* GetDefaultBrowserContext();
  // Always returns nullptr (the protocol reports an error).
  content::BrowserContext* CreateBrowserContext();
  // Always fails: there are no additional browser contexts to dispose.
  void DisposeBrowserContext(
      content::BrowserContext* context,
      content::DevToolsManagerDelegate::DisposeCallback callback);

 private:
  friend class base::NoDestructor<DevToolsBrowserContextManager>;
  DevToolsBrowserContextManager();
  ~DevToolsBrowserContextManager();
};

#endif  // CHROME_BROWSER_DEVTOOLS_DEVTOOLS_BROWSER_CONTEXT_MANAGER_H_
