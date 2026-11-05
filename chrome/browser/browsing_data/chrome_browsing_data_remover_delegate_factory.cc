// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/chrome_browsing_data_remover_delegate_factory.h"

#include "base/no_destructor.h"
#include "build/build_config.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_delegate.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry_factory.h"
#include "chrome/browser/download/download_core_service_factory.h"
#include "chrome/browser/file_system_access/file_system_access_permission_context_factory.h"
#include "chrome/browser/heavy_ad_intervention/heavy_ad_service_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/language/url_language_histogram_factory.h"
#include "chrome/browser/preloading/prefetch/no_state_prefetch/no_state_prefetch_manager_factory.h"
#include "chrome/browser/preloading/prefetch/search_prefetch/search_prefetch_service_factory.h"
#include "chrome/browser/private_verification_tokens/private_verification_tokens_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/verdict_cache_manager_factory.h"
#include "chrome/browser/search_engine_choice/search_engine_choice_service_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/spellchecker/spellcheck_factory.h"
#include "chrome/browser/ui/find_bar/find_bar_state_factory.h"
#include "chrome/browser/webdata_services/web_data_service_factory.h"
#include "content/public/browser/browser_context.h"
#include "extensions/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
#include "chrome/browser/extensions/activity_log/activity_log.h"
#include "extensions/browser/extension_prefs_factory.h"
#endif

#if BUILDFLAG(ENABLE_SESSION_SERVICE)
#include "chrome/browser/sessions/session_service_factory.h"
#endif


// static
ChromeBrowsingDataRemoverDelegateFactory*
ChromeBrowsingDataRemoverDelegateFactory::GetInstance() {
  static base::NoDestructor<ChromeBrowsingDataRemoverDelegateFactory> instance;
  return instance.get();
}

// static
ChromeBrowsingDataRemoverDelegate*
ChromeBrowsingDataRemoverDelegateFactory::GetForProfile(Profile* profile) {
  return static_cast<ChromeBrowsingDataRemoverDelegate*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

ChromeBrowsingDataRemoverDelegateFactory::
    ChromeBrowsingDataRemoverDelegateFactory()
    : ProfileKeyedServiceFactory(
          "BrowsingDataRemover",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOwnInstance)
              .Build()) {
  DependsOn(HistoryServiceFactory::GetInstance());
  DependsOn(HostContentSettingsMapFactory::GetInstance());
  DependsOn(prerender::NoStatePrefetchManagerFactory::GetInstance());
  DependsOn(PrivateVerificationTokensServiceFactory::GetInstance());
  DependsOn(ProtocolHandlerRegistryFactory::GetInstance());
  DependsOn(safe_browsing::VerdictCacheManagerFactory::GetInstance());
  DependsOn(search_engines::SearchEngineChoiceServiceFactory::GetInstance());
  DependsOn(SearchPrefetchServiceFactory::GetInstance());
  DependsOn(SpellcheckServiceFactory::GetInstance());
  DependsOn(TabRestoreServiceFactory::GetInstance());
  DependsOn(TemplateURLServiceFactory::GetInstance());
  DependsOn(UrlLanguageHistogramFactory::GetInstance());
  DependsOn(WebDataServiceFactory::GetInstance());

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  DependsOn(extensions::ActivityLog::GetFactoryInstance());
  DependsOn(extensions::ExtensionPrefsFactory::GetInstance());
#endif

#if BUILDFLAG(ENABLE_SESSION_SERVICE)
  DependsOn(SessionServiceFactory::GetInstance());
#endif

  // In Android, depends on offline_pages::OfflinePageModelFactory in
  // SimpleDependencyManager.
}

ChromeBrowsingDataRemoverDelegateFactory::
    ~ChromeBrowsingDataRemoverDelegateFactory() = default;

std::unique_ptr<KeyedService>
ChromeBrowsingDataRemoverDelegateFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<ChromeBrowsingDataRemoverDelegate>(context);
}
