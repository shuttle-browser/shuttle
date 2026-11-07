// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_browser_context_manager.h"

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_manager.h"

namespace {

// Returns the profile backing the default browser context, or nullptr if the
// last used profile can't be loaded (for example because its directory can't
// be created or is scheduled for deletion).
Profile* GetDefaultProfile() {
  Profile* profile = ProfileManager::GetLastUsedProfile();
  return profile ? profile->GetOriginalProfile() : nullptr;
}

}  // namespace

DevToolsBrowserContextManager::DevToolsBrowserContextManager() = default;

DevToolsBrowserContextManager::~DevToolsBrowserContextManager() = default;

// static
DevToolsBrowserContextManager& DevToolsBrowserContextManager::GetInstance() {
  static base::NoDestructor<DevToolsBrowserContextManager> instance;
  return *instance;
}

Profile* DevToolsBrowserContextManager::GetProfileById(
    const std::string& context_id) {
  if (Profile* default_profile = GetDefaultProfile();
      default_profile && context_id == default_profile->UniqueId()) {
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
