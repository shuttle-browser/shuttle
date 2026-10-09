// Copyright 2026 Silkweave
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHUTTLE_SHUTTLE_SERVICE_FACTORY_H_
#define CHROME_BROWSER_SHUTTLE_SHUTTLE_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace shuttle {

class ShuttleService;

class ShuttleServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static ShuttleService* GetForProfile(Profile* profile);
  static ShuttleServiceFactory* GetInstance();

  ShuttleServiceFactory(const ShuttleServiceFactory&) = delete;
  ShuttleServiceFactory& operator=(const ShuttleServiceFactory&) =
      delete;

 private:
  friend base::NoDestructor<ShuttleServiceFactory>;

  ShuttleServiceFactory();
  ~ShuttleServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
  bool ServiceIsCreatedWithBrowserContext() const override;
};

}  // namespace shuttle

#endif  // CHROME_BROWSER_SHUTTLE_SHUTTLE_SERVICE_FACTORY_H_
