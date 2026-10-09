// Copyright 2026 Silkweave
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/shuttle/shuttle_service_factory.h"

#include "chrome/browser/shuttle/shuttle_service.h"
#include "chrome/browser/shuttle/profile_registry.h"
#include "chrome/browser/profiles/profile.h"

namespace shuttle {

// static
ShuttleService* ShuttleServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<ShuttleService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

// static
ShuttleServiceFactory* ShuttleServiceFactory::GetInstance() {
  static base::NoDestructor<ShuttleServiceFactory> instance;
  return instance.get();
}

ShuttleServiceFactory::ShuttleServiceFactory()
    : ProfileKeyedServiceFactory(
          "ShuttleService",
          // Shuttle runs one regular profile; nothing else gets agents.
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .Build()) {}

ShuttleServiceFactory::~ShuttleServiceFactory() = default;

std::unique_ptr<KeyedService>
ShuttleServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<ShuttleService>(
      Profile::FromBrowserContext(context));
}

void ShuttleServiceFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  ProfileRegistry::RegisterProfilePrefs(registry);
}

// Created with the profile, so pending profile deletions are finished before
// any agent tab can load their storage.
bool ShuttleServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

}  // namespace shuttle
