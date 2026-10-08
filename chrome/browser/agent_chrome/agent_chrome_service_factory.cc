// Copyright 2026 The Agent Chrome Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/agent_chrome/agent_chrome_service_factory.h"

#include "chrome/browser/agent_chrome/agent_chrome_service.h"
#include "chrome/browser/agent_chrome/profile_registry.h"
#include "chrome/browser/profiles/profile.h"

namespace agent_chrome {

// static
AgentChromeService* AgentChromeServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<AgentChromeService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

// static
AgentChromeServiceFactory* AgentChromeServiceFactory::GetInstance() {
  static base::NoDestructor<AgentChromeServiceFactory> instance;
  return instance.get();
}

AgentChromeServiceFactory::AgentChromeServiceFactory()
    : ProfileKeyedServiceFactory(
          "AgentChromeService",
          // Agent Chrome runs one regular profile; nothing else gets agents.
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .Build()) {}

AgentChromeServiceFactory::~AgentChromeServiceFactory() = default;

std::unique_ptr<KeyedService>
AgentChromeServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<AgentChromeService>(
      Profile::FromBrowserContext(context));
}

void AgentChromeServiceFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  ProfileRegistry::RegisterProfilePrefs(registry);
}

// Created with the profile, so pending profile deletions are finished before
// any agent tab can load their storage.
bool AgentChromeServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

}  // namespace agent_chrome
