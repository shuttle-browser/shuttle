// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_browser_context_manager.h"

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_manager.h"

DevToolsBrowserContextManager::DevToolsBrowserContextManager() = default;

DevToolsBrowserContextManager::~DevToolsBrowserContextManager() = default;

// static
DevToolsBrowserContextManager& DevToolsBrowserContextManager::GetInstance() {
  static base::NoDestructor<DevToolsBrowserContextManager> instance;
  return *instance;
}

Profile* DevToolsBrowserContextManager::GetProfileById(
    const std::string& context_id) {
  Profile* default_profile =
      ProfileManager::GetLastUsedProfile()->GetOriginalProfile();
  if (context_id == default_profile->UniqueId()) {
    return default_profile;
  }
  return nullptr;
}

content::BrowserContext* DevToolsBrowserContextManager::CreateBrowserContext() {
  return nullptr;
}

std::vector<base::WeakPtr<content::BrowserContext>>
DevToolsBrowserContextManager::GetBrowserContexts() {
  return {};
}

content::BrowserContext*
DevToolsBrowserContextManager::GetDefaultBrowserContext() {
  // Do not force profile loading (or it will blow up if called on shutdown).
  auto* last_profile = ProfileManager::GetLastUsedProfileIfLoaded();
  return last_profile ? last_profile->GetOriginalProfile() : nullptr;
}

void DevToolsBrowserContextManager::DisposeBrowserContext(
    content::BrowserContext* context,
    content::DevToolsManagerDelegate::DisposeCallback callback) {
  std::move(callback).Run(false, "No such browser context");
}
