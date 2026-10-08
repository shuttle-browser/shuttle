// Copyright 2026 The Agent Chrome Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AGENT_CHROME_AGENT_CHROME_SERVICE_FACTORY_H_
#define CHROME_BROWSER_AGENT_CHROME_AGENT_CHROME_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace agent_chrome {

class AgentChromeService;

class AgentChromeServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static AgentChromeService* GetForProfile(Profile* profile);
  static AgentChromeServiceFactory* GetInstance();

  AgentChromeServiceFactory(const AgentChromeServiceFactory&) = delete;
  AgentChromeServiceFactory& operator=(const AgentChromeServiceFactory&) =
      delete;

 private:
  friend base::NoDestructor<AgentChromeServiceFactory>;

  AgentChromeServiceFactory();
  ~AgentChromeServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
  bool ServiceIsCreatedWithBrowserContext() const override;
};

}  // namespace agent_chrome

#endif  // CHROME_BROWSER_AGENT_CHROME_AGENT_CHROME_SERVICE_FACTORY_H_
