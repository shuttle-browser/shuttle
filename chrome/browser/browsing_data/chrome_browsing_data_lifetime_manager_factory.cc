// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/chrome_browsing_data_lifetime_manager_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_lifetime_manager.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_delegate_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_context.h"
#include "extensions/buildflags/buildflags.h"
// static
ChromeBrowsingDataLifetimeManagerFactory*
ChromeBrowsingDataLifetimeManagerFactory::GetInstance() {
  static base::NoDestructor<ChromeBrowsingDataLifetimeManagerFactory> instance;
  return instance.get();
}

// static
ChromeBrowsingDataLifetimeManager*
ChromeBrowsingDataLifetimeManagerFactory::GetForProfile(Profile* profile) {
  return static_cast<ChromeBrowsingDataLifetimeManager*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

ChromeBrowsingDataLifetimeManagerFactory::
    ChromeBrowsingDataLifetimeManagerFactory()
    : ProfileKeyedServiceFactory(
          "BrowsingDataLifetimeManager",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              .WithGuest(ProfileSelection::kOffTheRecordOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOwnInstance)
              .Build()) {
  DependsOn(ChromeBrowsingDataRemoverDelegateFactory::GetInstance());
}

ChromeBrowsingDataLifetimeManagerFactory::
    ~ChromeBrowsingDataLifetimeManagerFactory() = default;

std::unique_ptr<KeyedService>
ChromeBrowsingDataLifetimeManagerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<ChromeBrowsingDataLifetimeManager>(context);
}

bool ChromeBrowsingDataLifetimeManagerFactory::
    ServiceIsCreatedWithBrowserContext() const {
  return true;
}
