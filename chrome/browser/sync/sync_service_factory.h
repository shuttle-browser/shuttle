// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_SYNC_SERVICE_FACTORY_H_
#define CHROME_BROWSER_SYNC_SYNC_SERVICE_FACTORY_H_

#include <memory>
#include <vector>

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace syncer {
class SyncService;
}  // namespace syncer

// Agent Chrome: sync is removed. This factory is a stub that never creates a
// SyncService; it only remains so that callers keep compiling.
class SyncServiceFactory : public ProfileKeyedServiceFactory {
 public:
  // Always returns nullptr.
  static syncer::SyncService* GetForProfile(Profile* profile);

  SyncServiceFactory(const SyncServiceFactory&) = delete;
  SyncServiceFactory& operator=(const SyncServiceFactory&) = delete;

  // Always returns false.
  static bool HasSyncService(Profile* profile);

  // Always returns false.
  static bool IsSyncAllowed(Profile* profile);

  static SyncServiceFactory* GetInstance();

  // Always returns an empty vector.
  static std::vector<const syncer::SyncService*> GetAllSyncServices();

 private:
  friend base::NoDestructor<SyncServiceFactory>;

  SyncServiceFactory();
  ~SyncServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_SYNC_SYNC_SERVICE_FACTORY_H_
