// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/counters/browsing_data_counter_factory.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "build/build_config.h"
#include "chrome/browser/browsing_data/counters/browsing_data_counter_utils.h"
#include "chrome/browser/browsing_data/counters/cache_counter.h"
#include "chrome/browser/browsing_data/counters/downloads_counter.h"
#include "chrome/browser/browsing_data/counters/site_data_counter.h"
#include "chrome/browser/browsing_data/counters/site_settings_counter.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/webdata_services/web_data_service_factory.h"
#include "components/browsing_data/core/counters/browsing_data_counter.h"
#include "components/browsing_data/core/counters/history_counter.h"
#include "components/browsing_data/core/pref_names.h"
#include "extensions/buildflags/buildflags.h"


#if !BUILDFLAG(IS_ANDROID)
#include "content/public/browser/host_zoom_map.h"
#else
#include "chrome/browser/browsing_data/counters/tabs_counter.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "device/fido/cros/credential_store.h"
#endif

// static
std::unique_ptr<browsing_data::BrowsingDataCounter>
BrowsingDataCounterFactory::GetForProfileAndPref(Profile* profile,
                                                 const std::string& pref_name) {
  if (pref_name == browsing_data::prefs::kDeleteBrowsingHistory) {
    return std::make_unique<browsing_data::HistoryCounter>(
        HistoryServiceFactory::GetForProfile(
            profile, ServiceAccessType::EXPLICIT_ACCESS));
  }

  if (pref_name == browsing_data::prefs::kDeleteCache) {
    return std::make_unique<CacheCounter>(profile);
  }

  if (pref_name == browsing_data::prefs::kDeleteCookies) {
    return std::make_unique<SiteDataCounter>(profile);
  }

  if (pref_name == browsing_data::prefs::kDeleteDownloadHistory) {
    return std::make_unique<DownloadsCounter>(profile);
  }

  if (pref_name == browsing_data::prefs::kDeleteSiteSettings) {
    return std::make_unique<SiteSettingsCounter>(
        HostContentSettingsMapFactory::GetForProfile(profile),
#if !BUILDFLAG(IS_ANDROID)
        content::HostZoomMap::GetDefaultForBrowserContext(profile),
#else
        nullptr,
#endif
        ProtocolHandlerRegistryFactory::GetForBrowserContext(profile),
        profile->GetPrefs());
  }


#if BUILDFLAG(IS_ANDROID)
  if (pref_name == browsing_data::prefs::kCloseTabs) {
    return std::make_unique<TabsCounter>(profile);
  }
#endif

  return nullptr;
}
