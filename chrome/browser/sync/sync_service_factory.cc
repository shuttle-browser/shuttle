// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/sync_service_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "components/keyed_service/core/keyed_service.h"

// static
syncer::SyncService* SyncServiceFactory::GetForProfile(Profile* profile) {
  return nullptr;
}

// static
bool SyncServiceFactory::HasSyncService(Profile* profile) {
  return false;
}

// static
bool SyncServiceFactory::IsSyncAllowed(Profile* profile) {
  return false;
}

// static
SyncServiceFactory* SyncServiceFactory::GetInstance() {
  static base::NoDestructor<SyncServiceFactory> instance;
  return instance.get();
}

// static
std::vector<const syncer::SyncService*> SyncServiceFactory::GetAllSyncServices() {
  return {};
}

SyncServiceFactory::SyncServiceFactory()
    : ProfileKeyedServiceFactory(
          "SyncService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .WithGuest(ProfileSelection::kOriginalOnly)
              .Build()) {}

SyncServiceFactory::~SyncServiceFactory() = default;

std::unique_ptr<KeyedService>
SyncServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return nullptr;
}
