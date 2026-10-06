// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/template_url_service.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <string>
#include <utility>

#include "base/auto_reset.h"
#include "base/base64.h"
#include "base/base64url.h"
#include "base/check_deref.h"
#include "base/check_is_test.h"
#include "base/containers/fixed_flat_map.h"
#include "base/containers/flat_map.h"
#include "base/debug/crash_logging.h"
#include "base/feature_list.h"
#include "base/format_macros.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/i18n/case_conversion.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/observer_list.h"
#include "base/rand_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/country_codes/country_codes.h"
#include "components/google/core/common/google_util.h"
#include "components/metrics/profile_metrics_service.h"
#include "components/omnibox/common/omnibox_feature_configs.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/regional_capabilities/access/country_access_reason.h"
#include "components/regional_capabilities/regional_capabilities_country_id.h"
#include "components/regional_capabilities/regional_capabilities_service.h"
#include "components/search_engines/choice_made_location.h"
#include "components/search_engines/enterprise/enterprise_search_manager.h"
#include "components/search_engines/keyword_web_data_service.h"
#include "components/search_engines/regulatory_extension_type.h"
#include "components/search_engines/search_engine_choice/search_engine_choice_service.h"
#include "components/search_engines/search_engine_choice/search_engine_choice_utils.h"
#include "components/search_engines/search_engine_split_metrics.h"
#include "components/search_engines/search_engine_type.h"
#include "components/search_engines/search_engines_pref_names.h"
#include "components/search_engines/search_engines_switches.h"
#include "components/search_engines/search_terms_data.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_prepopulate_data.h"
#include "components/search_engines/template_url_prepopulate_data_resolver.h"
#include "components/search_engines/template_url_service_client.h"
#include "components/search_engines/template_url_service_observer.h"
#include "components/search_engines/template_url_starter_pack_data.h"
#include "components/search_engines/ui_utils.h"
#include "components/search_engines/util.h"
#include "components/url_formatter/url_fixer.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/third_party/mozilla/url_parse.h"
#include "url/url_util.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/search_engines/android/template_url_service_android.h"
#endif

typedef SearchHostToURLsMap::TemplateURLSet TemplateURLSet;

namespace {

// TODO(yoangela): Consider sharing this const with
//  "Omnibox.KeywordModeUsageByEngineType.Accepted" in omnibox_edit_model.cc.
const char kKeywordModeUsageByEngineTypeHistogramName[] =
    "Omnibox.KeywordModeUsageByEngineType";

bool Contains(TemplateURLService::OwnedTemplateURLVector* template_urls,
              const TemplateURL* turl) {
  return FindTemplateURL(template_urls, turl) != template_urls->end();
}

bool IsCreatedByExtension(const TemplateURL& template_url) {
  return template_url.type() == TemplateURL::NORMAL_CONTROLLED_BY_EXTENSION ||
         template_url.type() == TemplateURL::OMNIBOX_API_EXTENSION;
}

// Check if `is_active` status should be merged.  This is true if the
// `new_values` is enforced by policy. This handles two scenarios:
// 1. Recommended policy update: If an admin updates a recommended policy
//    (e.g., changes the engine name), a user-deactivated engine should remain
//    deactivated. Returns false.
// 2. Recommended to mandatory policy update: If an admin changes a policy
//    from recommended to mandatory, a user-deactivated engine should be
//    force-activated. Returns true.
// This preserves user deactivation for recommended site search engines unless
// the policy becomes mandatory.
bool ShouldMergeEnterpriseSearchEnginesActiveStatus(
    const TemplateURLData& existing_data,
    const TemplateURL& new_values) {
  return new_values.enforced_by_policy() &&
         existing_data.is_active != new_values.is_active();
}

// Checks if `new_values` has updated versions of `existing_turl`. Only fields
// set by the search engine policies are checked.
bool ShouldMergeEnterpriseSearchEngines(const TemplateURL& existing_turl,
                                        const TemplateURL& new_values) {
  CHECK_EQ(existing_turl.keyword(), new_values.keyword());

  return existing_turl.short_name() != new_values.short_name() ||
         existing_turl.url() != new_values.url() ||
         existing_turl.suggestions_url() != new_values.suggestions_url() ||
         existing_turl.featured_by_policy() !=
             new_values.featured_by_policy() ||
         (existing_turl.policy_origin() ==
              TemplateURLData::PolicyOrigin::kSearchAggregator &&
          existing_turl.favicon_url() != new_values.favicon_url()) ||
         existing_turl.enforced_by_policy() !=
             new_values.enforced_by_policy() ||
         ShouldMergeEnterpriseSearchEnginesActiveStatus(existing_turl.data(),
                                                        new_values);
}

// Creates a new `TemplateURL` that copies updates fields from `new_values` into
// `existing_turl`. Only fields set by policy are copied from `new_values`, all
// other fields are copied unchanged from `existing_turl`.
TemplateURLData MergeEnterpriseSearchEngines(TemplateURLData existing_data,
                                             const TemplateURL& new_values) {
  CHECK_EQ(existing_data.keyword(), new_values.keyword());

  TemplateURLData merged_data(existing_data);
  merged_data.SetShortName(new_values.short_name());
  merged_data.SetURL(new_values.url());
  merged_data.suggestions_url = new_values.suggestions_url();
  merged_data.featured_by_policy = new_values.featured_by_policy();
  if (existing_data.policy_origin ==
          TemplateURLData::PolicyOrigin::kSearchAggregator ||
      (existing_data.policy_origin ==
           TemplateURLData::PolicyOrigin::kSiteSearch &&
       existing_data.url() != new_values.url())) {
    merged_data.favicon_url = new_values.favicon_url();
  }
  merged_data.enforced_by_policy = new_values.enforced_by_policy();
  if (ShouldMergeEnterpriseSearchEnginesActiveStatus(existing_data,
                                                     new_values)) {
    merged_data.is_active = new_values.is_active();
  }
  return merged_data;
}

// Checks if `url` is a Google AI mode URL. Uses the `udm` query param. Only
// works for Google URLs because it's unknown what other search providers will
// use to distinguish their AI mode and traditional search URLs.
bool IsGoogleAiModeUrl(GURL url) {
  // Check that:
  // 1. `url` contains a `udm=50` query param which distinguish Google AI mode
  //    and traditional search URLs. This check alone isn't sufficient because
  //    any website could coincidentally use the same query param for its own
  //    purposes.
  // 2. `url` is a Google URL. This check is done 2nd because it's slower (0.5us
  //    v 5us).

  std::string_view query = url.query();
  url::Component query_iterator(0, query.length());
  url::Component key, value;
  bool udm_50 = false;
  while (url::ExtractQueryKeyValue(query, &query_iterator, &key, &value) &&
         !udm_50) {
    std::string_view key_string = query.substr(key.begin, key.len);
    std::string_view value_string = query.substr(value.begin, value.len);
    udm_50 = key_string == "udm" && value_string == "50";
  }

  return udm_50 && google_util::IsGoogleDomainUrl(
                       url, google_util::DISALLOW_SUBDOMAIN,
                       google_util::DISALLOW_NON_STANDARD_PORTS);
}

}  // namespace

// TemplateURLService::LessWithPrefix -----------------------------------------

class TemplateURLService::LessWithPrefix {
 public:
  // We want to find the set of keywords that begin with a prefix.  The STL
  // algorithms will return the set of elements that are "equal to" the
  // prefix, where "equal(x, y)" means "!(cmp(x, y) || cmp(y, x))".  When
  // cmp() is the typical std::less<>, this results in lexicographic equality;
  // we need to extend this to mark a prefix as "not less than" a keyword it
  // begins, which will cause the desired elements to be considered "equal to"
  // the prefix.  Note: this is still a strict weak ordering, as required by
  // equal_range() (though I will not prove that here).
  //
  // Unfortunately the calling convention is not "prefix and element" but
  // rather "two elements", so we pass the prefix as a fake "element" which has
  // a NULL KeywordDataElement pointer.
  bool operator()(const KeywordToTURL::value_type& elem1,
                  const KeywordToTURL::value_type& elem2) const {
    return (elem1.second == nullptr)
               ? (elem2.first.compare(0, elem1.first.length(), elem1.first) > 0)
               : (elem1.first < elem2.first);
  }
};

// TemplateURLService::Scoper -------------------------------------------------

class TemplateURLService::Scoper {
 public:
  // Keep one of these handles in scope to coalesce all the notifications into a
  // single notification. Likewise, BatchModeScoper defers web data service
  // operations into a batch operation.
  //
  // Notifications are sent when the last outstanding handle is destroyed and
  // |model_mutated_notification_pending_| is true.
  //
  // The web data service batch operation is performed when the batch mode level
  // is 0 and more than one operation is pending. This check happens when
  // BatchModeScoper is destroyed.
  explicit Scoper(TemplateURLService* service)
      : batch_mode_scoper_(
            std::make_unique<KeywordWebDataService::BatchModeScoper>(
                service->web_data_service_.get())),
        service_(service) {
    ++service_->outstanding_scoper_handles_;
  }

  Scoper(const Scoper&) = delete;
  Scoper& operator=(const Scoper&) = delete;

  // When a Scoper is destroyed, the handle count is updated. If the handle
  // count is at zero, notify the observers that the model has changed if
  // service is loaded and model was mutated.
  ~Scoper() {
    DCHECK_GT(service_->outstanding_scoper_handles_, 0);

    --service_->outstanding_scoper_handles_;
    if (service_->outstanding_scoper_handles_ == 0 &&
        service_->model_mutated_notification_pending_) {
      service_->model_mutated_notification_pending_ = false;

      if (!service_->loaded_) {
        return;
      }

      for (auto& observer : service_->model_observers_) {
        observer.OnTemplateURLServiceChanged();
      }
    }
  }

 private:
  std::unique_ptr<KeywordWebDataService::BatchModeScoper> batch_mode_scoper_;
  raw_ptr<TemplateURLService> service_;
};

// TemplateURLService::PreLoadingProviders -------------------------------------

class TemplateURLService::PreLoadingProviders {
 public:
  PreLoadingProviders() = default;
  ~PreLoadingProviders() = default;

  const TemplateURL* default_search_provider() const {
    return default_search_provider_.get();
  }

  TemplateURL* default_search_provider() {
    return default_search_provider_.get();
  }

  void set_default_search_provider(
      std::unique_ptr<TemplateURL> default_search_provider) {
    default_search_provider_ = std::move(default_search_provider);
  }

  TemplateURLService::OwnedTemplateURLVector TakeSearchEngines() {
    return std::move(search_engines_);
  }

  void set_search_engines(
      TemplateURLService::OwnedTemplateURLVector&& search_engines) {
    search_engines_ = std::move(search_engines);
  }

  // Looks up `keyword` and returns the best `TemplateURL` for it. Returns
  // `nullptr` if the keyword was not found. The caller should not try to delete
  // the returned pointer; the data store retains ownership of it.
  const TemplateURL* GetTemplateURLForKeyword(
      const std::u16string& keyword) const {
    return GetTemplateURLForSelector(base::BindRepeating(
        [](const std::u16string& keyword, const TemplateURL& turl) {
          return turl.keyword() == keyword;
        },
        keyword));
  }

  // Returns that `TemplateURL` with the specified GUID, or nullptr if not
  // found.The caller should not try to delete the returned pointer; the data
  // store retains ownership of it.
  const TemplateURL* GetTemplateURLForGUID(const std::string& guid) const {
    return GetTemplateURLForSelector(base::BindRepeating(
        [](const std::string& guid, const TemplateURL& turl) {
          return turl.sync_guid() == guid;
        },
        guid));
  }

  // Returns the best `TemplateURL` found with a URL using the specified `host`,
  // or nullptr if no such URL can be found.
  const TemplateURL* GetTemplateURLForHost(
      const std::string& host,
      const SearchTermsData& search_terms_data) const {
    return GetTemplateURLForSelector(base::BindRepeating(
        [](const std::string& host, const SearchTermsData* search_terms_data,
           const TemplateURL& turl) {
          return turl.GenerateSearchURL(*search_terms_data).host() == host;
        },
        host, &search_terms_data));
  }

 private:
  // Returns a pointer to a `TemplateURL` `t` such that `selector(t) == true`.
  // Prioritizes DSP.
  const TemplateURL* GetTemplateURLForSelector(
      base::RepeatingCallback<bool(const TemplateURL& turl)> selector) const {
    if (default_search_provider() && selector.Run(*default_search_provider())) {
      return default_search_provider();
    }

    for (auto& search_engine : search_engines_) {
      if (selector.Run(*search_engine)) {
        return search_engine.get();
      }
    }

    return nullptr;
  }

  // A temporary location for the DSE until Web Data has been loaded and it can
  // be merged into |template_urls_|.
  std::unique_ptr<TemplateURL> default_search_provider_;

  // A temporary location for site search set by policy until Web Data has been
  // loaded and it can be merged into |template_urls_|.
  TemplateURLService::OwnedTemplateURLVector search_engines_;
};

// TemplateURLService::CategorizedTemplateUrls --------------------------------

TemplateURLService::CategorizedTemplateUrls::CategorizedTemplateUrls() =
    default;
TemplateURLService::CategorizedTemplateUrls::~CategorizedTemplateUrls() =
    default;
TemplateURLService::CategorizedTemplateUrls::CategorizedTemplateUrls(
    const CategorizedTemplateUrls& other) = default;

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
// TemplateURLService::PrepopulatedAndRecentlyVisitedTemplateUrls -------------

TemplateURLService::PrepopulatedAndRecentlyVisitedTemplateUrls::
    PrepopulatedAndRecentlyVisitedTemplateUrls() = default;
TemplateURLService::PrepopulatedAndRecentlyVisitedTemplateUrls::
    ~PrepopulatedAndRecentlyVisitedTemplateUrls() = default;
TemplateURLService::PrepopulatedAndRecentlyVisitedTemplateUrls::
    PrepopulatedAndRecentlyVisitedTemplateUrls(
        const PrepopulatedAndRecentlyVisitedTemplateUrls& other) = default;
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)

// TemplateURLService ---------------------------------------------------------

TemplateURLService::TemplateURLService(
    PrefService& prefs,
    search_engines::SearchEngineChoiceService& search_engine_choice_service,
    TemplateURLPrepopulateData::Resolver& prepopulate_data_resolver,
    regional_capabilities::RegionalCapabilitiesService&
        regional_capabilities_service,
    metrics::ProfileMetricsService& profile_metrics_service,
    std::unique_ptr<SearchTermsData> search_terms_data,
    const scoped_refptr<KeywordWebDataService>& web_data_service,
    std::unique_ptr<TemplateURLServiceClient> client,
    const base::RepeatingClosure& dsp_change_callback)
    : prefs_(prefs),
      search_engine_choice_service_(search_engine_choice_service),
      prepopulate_data_resolver_(prepopulate_data_resolver),
      regional_capabilities_service_(regional_capabilities_service),
      profile_metrics_service_(profile_metrics_service),
      search_terms_data_(std::move(search_terms_data)),
      web_data_service_(web_data_service),
      client_(std::move(client)),
      dsp_change_callback_(dsp_change_callback),
      pre_loading_providers_(std::make_unique<PreLoadingProviders>()),
      default_search_manager_(
          &prefs,
          &search_engine_choice_service,
          prepopulate_data_resolver_.get(),
          base::BindRepeating(&TemplateURLService::ApplyDefaultSearchChange,
                              base::Unretained(this))),
      enterprise_search_manager_(GetEnterpriseSearchManager(&prefs)) {
  DCHECK(search_terms_data_);
  Init();
}

TemplateURLService::TemplateURLService(
    PrefService& prefs,
    search_engines::SearchEngineChoiceService& search_engine_choice_service,
    TemplateURLPrepopulateData::Resolver& prepopulate_data_resolver,
    regional_capabilities::RegionalCapabilitiesService&
        regional_capabilities_service,
    metrics::ProfileMetricsService& profile_metrics_service,
    base::span<const TemplateURLService::Initializer> initializers)
    : TemplateURLService(
          prefs,
          search_engine_choice_service,
          prepopulate_data_resolver,
          regional_capabilities_service,
          profile_metrics_service,
          /*search_terms_data=*/std::make_unique<SearchTermsData>(),
          /*web_data_service=*/nullptr,
          /*client=*/nullptr,
          /*dsp_change_callback=*/base::RepeatingClosure()) {
  // This constructor is not intended to be used outside of tests.
  CHECK_IS_TEST();
  ApplyInitializersForTesting(initializers);  // IN-TEST
}

TemplateURLService::~TemplateURLService() {
  // |web_data_service_| should be deleted during Shutdown().
  DCHECK(!web_data_service_);
}

// static
void TemplateURLService::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterStringPref(prefs::kDefaultSearchProviderGUID,
                               std::string());
  registry->RegisterBooleanPref(prefs::kDefaultSearchProviderEnabled, true);
  registry->RegisterBooleanPref(
      prefs::kDefaultSearchProviderContextMenuAccessAllowed, true);
}

#if BUILDFLAG(IS_ANDROID)
base::android::ScopedJavaLocalRef<jobject> TemplateURLService::GetJavaObject() {
  if (!template_url_service_android_) {
    template_url_service_android_ =
        std::make_unique<TemplateUrlServiceAndroid>(this);
  }
  return template_url_service_android_->GetJavaObject();
}
#endif

bool TemplateURLService::CanAddAutogeneratedKeyword(
    const std::u16string& keyword,
    const GURL& url) {
  DCHECK(!keyword.empty());  // This should only be called for non-empty
                             // keywords. If we need to support empty keywords
                             // the code needs to change slightly.
  const TemplateURL* existing_url = GetTemplateURLForKeyword(keyword);
  if (existing_url) {
    // TODO(tommycli): Currently, this code goes one step beyond
    // safe_for_autoreplace() and also forbids automatically modifying
    // prepopulated engines. That's debatable, as we already update prepopulated
    // provider favicons as the user browses. See UpdateProviderFavicons().
    return existing_url->safe_for_autoreplace() &&
           existing_url->prepopulate_id() == 0;
  }

  // We don't have a TemplateURL with keyword.  We still may not allow this
  // keyword if there's evidence we may have created this keyword before and
  // the user renamed it (because, for instance, the keyword is a common word
  // that may interfere with search queries).  An easy heuristic for this is
  // whether the user has a TemplateURL that has been manually modified (e.g.,
  // renamed) connected to the same host.
  return !url.is_valid() || url.GetHost().empty() ||
         CanAddAutogeneratedKeywordForHost(url.GetHost());
}

bool TemplateURLService::IsPrepopulatedOrDefaultProviderByPolicy(
    const TemplateURL* t_url) const {
  return (t_url->prepopulate_id() > 0 ||
          t_url->CreatedByDefaultSearchProviderPolicy() ||
          t_url->CreatedByRegulatoryProgram()) &&
         t_url->SupportsReplacement(search_terms_data());
}

bool TemplateURLService::ShowInDefaultList(const TemplateURL* t_url) const {
  return t_url == default_search_provider_ ||
         IsPrepopulatedOrDefaultProviderByPolicy(t_url);
}

bool TemplateURLService::ShowInActivesList(const TemplateURL* t_url) const {
  return t_url->is_active() == TemplateURLData::ActiveStatus::kTrue;
}

bool TemplateURLService::HiddenFromLists(const TemplateURL* t_url) const {
  switch (t_url->policy_origin()) {
    case TemplateURLData::PolicyOrigin::kDefaultSearchProvider:
      return false;

    case TemplateURLData::PolicyOrigin::kNoPolicy:
    case TemplateURLData::PolicyOrigin::kSiteSearch:
    case TemplateURLData::PolicyOrigin::kSearchAggregator:
      // Hide if another engine (e.g., one set by user/policy) takes precedence
      // for the same keyword. `GetTemplateURLForKeyword` already ensures
      // prioritization of search engines, so there is no need to replicate the
      // logic here.
      return t_url != GetTemplateURLForKeyword(t_url->keyword());
  }
}

void TemplateURLService::AddMatchingKeywords(const std::u16string& prefix,
                                             bool supports_replacement_only,
                                             TemplateURLVector* turls) {
  // Sanity check args.
  if (prefix.empty() || !turls) {
    return;
  }

  // Find matching keyword range.  Searches the element map for keywords
  // beginning with |prefix| and stores the endpoints of the resulting set in
  // |match_range|.
  const auto match_range(std::equal_range(
      keyword_to_turl_.begin(), keyword_to_turl_.end(),
      typename KeywordToTURL::value_type(prefix, nullptr), LessWithPrefix()));

  // Add to vector of matching keywords.
  for (auto i = match_range.first; i != match_range.second; ++i) {
    if (!supports_replacement_only ||
        i->second->url_ref().SupportsReplacement(search_terms_data())) {
      turls->push_back(i->second);
    }
  }
}

TemplateURL* TemplateURLService::GetTemplateURLForKeyword(
    const std::u16string& keyword) {
  return const_cast<TemplateURL*>(
      static_cast<const TemplateURLService*>(this)->GetTemplateURLForKeyword(
          keyword));
}

const TemplateURL* TemplateURLService::GetTemplateURLForKeyword(
    const std::u16string& keyword) const {
  // Finds and returns the best match for |keyword|.
  const auto match_range = keyword_to_turl_.equal_range(keyword);
  if (match_range.first != match_range.second) {
    // Among the matches for |keyword| in the multimap, return the best one.
    return std::min_element(
               match_range.first, match_range.second,
               [](const auto& a, const auto& b) {
                 return a.second->IsBetterThanConflictingEngine(b.second);
               })
        ->second;
  }

  return loaded_ ? nullptr
                 : pre_loading_providers_->GetTemplateURLForKeyword(keyword);
}

TemplateURL* TemplateURLService::GetTemplateURLForGUID(
    const std::string& sync_guid) {
  return const_cast<TemplateURL*>(
      static_cast<const TemplateURLService*>(this)->GetTemplateURLForGUID(
          sync_guid));
}

const TemplateURL* TemplateURLService::GetTemplateURLForGUID(
    const std::string& sync_guid) const {
  auto elem(guid_to_turl_.find(sync_guid));
  if (elem != guid_to_turl_.end()) {
    return elem->second;
  }

  return loaded_ ? nullptr
                 : pre_loading_providers_->GetTemplateURLForGUID(sync_guid);
}

TemplateURL* TemplateURLService::GetTemplateURLForHost(
    const std::string& host) {
  return const_cast<TemplateURL*>(
      static_cast<const TemplateURLService*>(this)->GetTemplateURLForHost(
          host));
}

const TemplateURL* TemplateURLService::GetTemplateURLForHost(
    const std::string& host) const {
  if (loaded_) {
    // `provider_map_` takes care of finding the best TemplateURL for `host`.
    return provider_map_->GetTemplateURLForHost(host);
  }

  return loaded_ ? nullptr
                 : pre_loading_providers_->GetTemplateURLForHost(
                       host, search_terms_data());
}

TemplateURL* TemplateURLService::Add(
    std::unique_ptr<TemplateURL> template_url) {
  DCHECK(template_url);
  DCHECK(!IsCreatedByExtension(*template_url.get()) ||
         (!FindTemplateURLForExtension(
              template_url->GetExtensionInfo()->extension_id,
              template_url->type()) &&
          template_url->id() == kInvalidTemplateURLID));

  return Add(std::move(template_url), true);
}

TemplateURL* TemplateURLService::AddWithOverrides(
    std::unique_ptr<TemplateURL> template_url,
    const std::u16string& short_name,
    const std::u16string& keyword,
    const std::string& url) {
  DCHECK(!short_name.empty());
  DCHECK(!keyword.empty());
  DCHECK(!url.empty());
  template_url->set_short_name(short_name);
  template_url->set_keyword(keyword);
  template_url->SetURL(url);
  return Add(std::move(template_url));
}

void TemplateURLService::Remove(const TemplateURL* template_url) {
  const std::u16string keyword = template_url->keyword();

  // CHECK that we aren't trying to Remove() the default search provider.
  // This has happened before, and causes permanent damage to the user Profile,
  // which can then be Synced to other installations. It's better to crash
  // immediately, and that's why this isn't a DCHECK. https://crbug.com/1164024
  {
    const TemplateURL* default_provider = GetDefaultSearchProvider();

    // TODO(tommycli): Once we are sure this never happens in practice, we can
    // remove this CrashKeyString, but we should keep the CHECK.
    static base::debug::CrashKeyString* crash_key =
        base::debug::AllocateCrashKeyString("removed_turl_keyword",
                                            base::debug::CrashKeySize::Size256);
    base::debug::ScopedCrashKeyString auto_clear(crash_key,
                                                 base::UTF16ToUTF8(keyword));

    CHECK_NE(template_url, default_provider);

    // Before we are loaded, we want to CHECK that we aren't accidentally
    // removing the in-table representation of the Default Search Engine.
    //
    // But users in the wild do indeed have engines with duplicated sync GUIDs.
    // For instance, Extensions Override Settings API used to have a bug that
    // would clone GUIDs. So therefore skip the check after loading.
    // https://crbug.com/1166372#c13
    if (!loaded() && default_provider &&
        default_provider->type() !=
            TemplateURL::Type::NORMAL_CONTROLLED_BY_EXTENSION &&
        template_url->type() !=
            TemplateURL::Type::NORMAL_CONTROLLED_BY_EXTENSION) {
      CHECK_NE(template_url->sync_guid(), default_provider->sync_guid());
    }
  }

  auto i = FindTemplateURL(&template_urls_, template_url);
  if (i == template_urls_.end()) {
    return;
  }

  // Capture `CanPolicyBeOverridden()` before `template_url` is removed and
  // erased. Recording the overridden keyword is deferred to the end of
  // `Remove()` to avoid synchronous re-entry while mutating `template_urls_`.
  const bool can_policy_be_overridden = template_url->CanPolicyBeOverridden();

  Scoper scoper(this);
  model_mutated_notification_pending_ = true;

  RemoveFromMaps(template_url);

  // Remove it from the vector containing all TemplateURLs.
  std::unique_ptr<TemplateURL> scoped_turl = std::move(*i);
  template_urls_.erase(i);

  if (template_url->type() == TemplateURL::NORMAL) {
    if (web_data_service_ && template_url->GetLocalData()) {
      web_data_service_->RemoveKeyword(template_url->id());
    }
    // The default search engine can't be deleted. But the user defined DSE can
    // be hidden by an extension. When this happens, deleting the user defined
    // DSE should clear the pref so the extension DSE becomes the default.
    if (default_search_provider_source_ == DefaultSearchManager::FROM_USER &&
        default_search_provider_ == template_url) {
      default_search_manager_.ClearUserSelectedDefaultSearchEngine();
    }

    if (template_url->sync_guid() ==
        prefs_->GetString(prefs::kDefaultSearchProviderGUID)) {
      prefs_->SetString(prefs::kDefaultSearchProviderGUID, std::string());
    }
  }

  if (loaded_ && client_) {
    client_->DeleteAllSearchTermsForKeyword(template_url->id());
  }

  // To ensure that policy engines are not added again on next
  // policy fetch, mark the keyword as overridden in the pref.
  if (can_policy_be_overridden && enterprise_search_manager_) {
    enterprise_search_manager_->AddOverriddenKeyword(
        base::UTF16ToUTF8(keyword));
  }
}

void TemplateURLService::RemoveExtensionControlledTURL(
    const std::string& extension_id,
    TemplateURL::Type type) {
  TemplateURL* url = FindTemplateURLForExtension(extension_id, type);
  if (!url) {
    return;
  }
  // NULL this out so that we can call Remove.
  if (default_search_provider_ == url) {
    default_search_provider_ = nullptr;
  }
  Remove(url);
  RemoveFromUnscopedModeExtensionIdsIfPresent(extension_id);
}

void TemplateURLService::RemoveAutoGeneratedBetween(base::Time created_after,
                                                    base::Time created_before) {
  RemoveAutoGeneratedForUrlsBetween(base::NullCallback(), created_after,
                                    created_before);
}

void TemplateURLService::RemoveAutoGeneratedForUrlsBetween(
    const base::RepeatingCallback<bool(const GURL&)>& url_filter,
    base::Time created_after,
    base::Time created_before) {
  Scoper scoper(this);

  for (size_t i = 0; i < template_urls_.size();) {
    TemplateURL* turl = template_urls_[i].get();
    if (turl->date_created() >= created_after &&
        (created_before.is_null() || turl->date_created() < created_before) &&
        turl->safe_for_autoreplace() && turl->prepopulate_id() == 0 &&
        turl->starter_pack_id() ==
            template_url_starter_pack_data::StarterPackId::kNone &&
        !MatchesDefaultSearchProvider(turl) &&
        (url_filter.is_null() ||
         url_filter.Run(turl->GenerateSearchURL(search_terms_data())))) {
      Remove(turl);
    } else {
      ++i;
    }
  }
}

void TemplateURLService::RegisterExtensionControlledTURL(
    const std::string& extension_id,
    const std::string& extension_name,
    const std::string& keyword,
    const std::string& template_url_string,
    const base::Time& extension_install_time,
    const bool unscoped_mode_allowed) {
  DCHECK(loaded_);

  if (FindTemplateURLForExtension(extension_id,
                                  TemplateURL::OMNIBOX_API_EXTENSION)) {
    return;
  }

  if (unscoped_mode_allowed) {
    AddToUnscopedModeExtensionIds(extension_id);
  }

  TemplateURLData data;
  data.SetShortName(base::UTF8ToUTF16(extension_name));
  data.SetKeyword(base::UTF8ToUTF16(keyword));
  data.SetURL(template_url_string);
  Add(std::make_unique<TemplateURL>(data, TemplateURL::OMNIBOX_API_EXTENSION,
                                    extension_id, extension_install_time,
                                    false));
}

TemplateURLService::TemplateURLVector TemplateURLService::GetTemplateURLs() {
  TemplateURLVector result;
  for (const auto& turl : template_urls_) {
    result.push_back(turl.get());
  }
  return result;
}

std::unique_ptr<search_engines::ChoiceScreenData>
TemplateURLService::GetChoiceScreenData() {
  return search_engine_choice_service_->GetChoiceScreenData(
      search_terms_data(), GetDefaultSearchProvider());
}

TemplateURL* TemplateURLService::GetEnterpriseSearchAggregatorEngine() const {
  auto it = std::ranges::find_if(
      enterprise_search_keyword_to_turl_, [](const auto& keyword_and_turl) {
        return keyword_and_turl.second
            ->CreatedByEnterpriseSearchAggregatorPolicy();
      });
  return it == enterprise_search_keyword_to_turl_.end() ? nullptr : it->second;
}

bool TemplateURLService::IsShortcutRequiredForSearchAggregatorEngine() const {
  return enterprise_search_manager_ &&
         enterprise_search_manager_->GetRequireShortcutValue();
}

TemplateURLService::TemplateURLVector
TemplateURLService::GetFeaturedEnterpriseSiteSearchEngines() const {
  TemplateURLVector result;
  for (const auto& turl : template_urls_) {
    if (turl->CreatedByNonDefaultSearchProviderPolicy() &&
        !turl->CreatedByEnterpriseSearchAggregatorPolicy() &&
        turl->featured_by_policy()) {
      result.push_back(turl.get());
    }
  }
  return result;
}

#if BUILDFLAG(IS_ANDROID)
TemplateURLService::OwnedTemplateURLDataVector
TemplateURLService::GetTemplateURLsForCountry(const std::string& country_code) {
  return TemplateURLPrepopulateData::GetLocalPrepopulatedEngines(country_code,
                                                                 prefs_.get());
}
#endif

void TemplateURLService::IncrementUsageCount(TemplateURL* url) {
  DCHECK(url);
  // Extension-controlled search engines are not persisted.
  if (url->type() != TemplateURL::NORMAL) {
    return;
  }
  if (!Contains(&template_urls_, url)) {
    return;
  }
  url->IncrementUsageCount();

  if (web_data_service_) {
    web_data_service_->UpdateKeyword(url->data());
  }
}

void TemplateURLService::ResetTemplateURL(TemplateURL* url,
                                          const std::u16string& title,
                                          const std::u16string& keyword,
                                          const std::string& search_url) {
  DCHECK(!IsCreatedByExtension(*url));
  DCHECK(!keyword.empty());
  DCHECK(!search_url.empty());

  TemplateURLData data(url->data());
  data.SetShortName(title);
  data.SetKeyword(keyword);
  if (search_url != data.url()) {
    data.SetURL(search_url);
    // The urls have changed, reset the favicon url.
    data.favicon_url = GURL();
  }
  data.safe_for_autoreplace = false;
  data.last_modified = clock_->Now();
  data.is_active = TemplateURLData::ActiveStatus::kTrue;
  data.policy_origin = TemplateURLData::PolicyOrigin::kNoPolicy;

  // Capture `CanPolicyBeOverridden()` and the keyword before calling
  // `Update()`, as updating `policy_origin` to `kNoPolicy` makes
  // `CanPolicyBeOverridden()` return false. Recording the overridden keyword is
  // deferred to the end of `ResetTemplateURL()` to avoid synchronous re-entry
  // during `Update()`.
  const bool can_policy_be_overridden = url->CanPolicyBeOverridden();
  const std::u16string old_keyword = url->keyword();

  Update(url, TemplateURL(data));

  // Similar to `TemplateURLService::Remove()`, mark the keyword as overridden
  // in the pref to prevent a policy created search engine from overriding this
  // one.
  if (can_policy_be_overridden && enterprise_search_manager_) {
    enterprise_search_manager_->AddOverriddenKeyword(
        base::UTF16ToUTF8(old_keyword));
  }
}

void TemplateURLService::SetIsActiveTemplateURL(TemplateURL* url,
                                                bool is_active) {
  DCHECK(url);

  TemplateURLData data(url->data());
  std::string histogram_name = kKeywordModeUsageByEngineTypeHistogramName;
  if (is_active) {
    data.is_active = TemplateURLData::ActiveStatus::kTrue;
    data.safe_for_autoreplace = false;
    histogram_name.append(".Activated");
  } else {
    data.is_active = TemplateURLData::ActiveStatus::kFalse;
    histogram_name.append(".Deactivated");
  }

  Update(url, TemplateURL(data));

  base::UmaHistogramEnumeration(
      histogram_name, url->GetBuiltinEngineType(),
      BuiltinEngineType::KEYWORD_MODE_ENGINE_TYPE_MAX);
}

#if BUILDFLAG(IS_ANDROID)
// static
TemplateURLData TemplateURLService::CreatePlayAPITemplateURLData(
    const std::u16string& keyword,
    const std::u16string& name,
    const std::string& search_url,
    const std::string& suggest_url,
    const std::string& favicon_url,
    const std::string& new_tab_url,
    const std::string& image_url,
    const std::string& image_url_post_params,
    const std::string& image_translate_url,
    const std::string& image_translate_source_language_param_key,
    const std::string& image_translate_target_language_param_key) {
  TemplateURLData data;
  data.SetShortName(name);
  data.SetKeyword(keyword);
  data.SetURL(search_url);
  data.suggestions_url = suggest_url;
  data.favicon_url = GURL(favicon_url);
  data.new_tab_url = new_tab_url;
  data.image_url = image_url;
  data.image_url_post_params = image_url_post_params;
  data.image_translate_url = image_translate_url;
  data.image_translate_source_language_param_key =
      image_translate_source_language_param_key;
  data.image_translate_target_language_param_key =
      image_translate_target_language_param_key;
  data.regulatory_origin = RegulatoryExtensionType::kAndroidEEA;
  // Play API engines are created by explicit user gesture, and should not be
  // auto-replaceable by an auto-generated engine as the user browses.
  data.safe_for_autoreplace = false;
  data.is_active = TemplateURLData::ActiveStatus::kTrue;
  return data;
}

bool TemplateURLService::ResetPlayAPISearchEngine(
    const TemplateURLData& new_play_api_turl_data) {
  CHECK(loaded());
  CHECK(new_play_api_turl_data.regulatory_origin ==
        RegulatoryExtensionType::kAndroidEEA);

  auto new_play_api_turl =
      std::make_unique<TemplateURL>(new_play_api_turl_data);

  SCOPED_CRASH_KEY_NUMBER("ResetPlayAPISearchEngine", "OldDspSource",
                          default_search_provider_source_);
  SCOPED_CRASH_KEY_STRING64(
      "ResetPlayAPISearchEngine", "OldDspKw",
      default_search_provider_
          ? base::UTF16ToUTF8(default_search_provider_->keyword())
          : "<null>");
  std::u16string old_play_keyword;

  Scoper scoper{this};

  // Part 1. Add the new play engine
  // Can fail if there is an old play engine or if there is a better engine
  // matching the new keyword.

  // 1.A) The Play API search engine is not guaranteed to be the best engine for
  // `keyword`, if there are user-defined, extension, or policy engines that can
  // take precedence. In practice on Android, this rarely happens, as only
  // policy engines are possible.
  const auto match_range =
      keyword_to_turl_.equal_range(new_play_api_turl->keyword());
  for (auto it = match_range.first; it != match_range.second; ++it) {
    TemplateURL* same_keyword_engine = it->second;
    if (same_keyword_engine->GetRegulatoryExtensionType() ==
        RegulatoryExtensionType::kAndroidEEA) {
      // We will look into replacing this one below, don't consider it a blocker
      // yet.
      continue;
    }

    if (same_keyword_engine->IsBetterThanConflictingEngine(
            new_play_api_turl.get())) {
      // We won't be able to add the new search engine at all.
      return false;
    }
  }

  // 1.B) We can only have 1 Play API engine at a time. Collect and remove all
  // old ones.
  std::vector<TemplateURL*> old_play_api_engines;
  for (const auto& turl : template_urls_) {
    if (turl->GetRegulatoryExtensionType() ==
        RegulatoryExtensionType::kAndroidEEA) {
      old_play_api_engines.push_back(turl.get());
    }
  }

  base::UmaHistogramCounts100(
      "Search.ChoiceDebug.PreexistingProgramTaggedEntries",
      old_play_api_engines.size());

  for (TemplateURL* old_play_api_engine : old_play_api_engines) {
    old_play_keyword = old_play_api_engine->keyword();
    if (old_play_api_engine == default_search_provider_) {
      // The DSE can't be removed from the loaded engines. We need to clear the
      // DSE first. The old Play API engine should be replaceable, since having
      // it as DSE means that we don't have a policy-enforced engine, and we
      // know that the incoming engine otherwise meets the criteria to be to be
      // set as DSE.
      CHECK(CanMakeDefault(new_play_api_turl.get()));

      // Clearing the member is OK here, we just have to make sure it is
      // re-populated by the time `scoper` is cleared.
      default_search_provider_ = nullptr;
    }

    Remove(old_play_api_engine);
  }

  SCOPED_CRASH_KEY_STRING64("ResetPlayAPISearchEngine", "OldPlayKw",
                            base::UTF16ToUTF8(old_play_keyword));

  TemplateURL* new_play_api_turl_ptr = Add(std::move(new_play_api_turl));

  // Adding the engine should be successful, we already checked for blockers
  // above.
  CHECK(new_play_api_turl_ptr);

  // Part 2: Set as DSE.
  // It is still possible that policies control the DSE, so ensure we don't
  // break that.
  if (CanMakeDefault(new_play_api_turl_ptr)) {
    SetUserSelectedDefaultSearchProvider(
        new_play_api_turl_ptr,
        search_engines::ChoiceMadeLocation::kDeviceChoiceImport);
  }

  CHECK(default_search_provider_);
  return true;
}
#endif  // BUILDFLAG(IS_ANDROID)

void TemplateURLService::UpdateProviderFavicons(
    const GURL& potential_search_url,
    const GURL& favicon_url) {
  DCHECK(loaded_);
  DCHECK(potential_search_url.is_valid());

  const TemplateURLSet* urls_for_host =
      provider_map_->GetURLsForHost(potential_search_url.GetHost());
  if (!urls_for_host) {
    return;
  }

  // Make a copy of the container of the matching TemplateURLs, as the original
  // container is invalidated as we update the contained TemplateURLs.
  TemplateURLSet urls_for_host_copy(*urls_for_host);

  Scoper scoper(this);
  for (TemplateURL* turl : urls_for_host_copy) {
    if (!IsCreatedByExtension(*turl) &&
        turl->policy_origin() !=
            TemplateURLData::PolicyOrigin::kSearchAggregator &&
        turl->IsSearchURL(potential_search_url, search_terms_data()) &&
        turl->favicon_url() != favicon_url) {
      TemplateURLData data(turl->data());
      data.favicon_url = favicon_url;
      UpdateData(turl, data);
    }
  }
}

bool TemplateURLService::CanMakeDefault(const TemplateURL* url) const {
  return CanDefaultSearchProviderBeModifiedByUser() &&
         (url != GetDefaultSearchProvider()) &&
         url->url_ref().SupportsReplacement(search_terms_data()) &&
         (url->type() == TemplateURL::NORMAL) &&
         (url->starter_pack_id() ==
          template_url_starter_pack_data::StarterPackId::kNone) &&
         (!url->CreatedByNonDefaultSearchProviderPolicy());
}

void TemplateURLService::SetUserSelectedDefaultSearchProvider(
    TemplateURL* url,
    search_engines::ChoiceMadeLocation choice_made_location) {
  // Omnibox keywords cannot be made default. Extension-controlled search
  // engines can be made default only by the extension itself because they
  // aren't persisted.
  DCHECK(!url || !IsCreatedByExtension(*url));
  if (url) {
    url->set_is_active(TemplateURLData::ActiveStatus::kTrue);
  }

  bool selection_added = false;

  if (load_failed_) {
    // Skip the DefaultSearchManager, which will persist to user preferences.
    if (CanDefaultSearchProviderBeModifiedByUser()) {
      ApplyDefaultSearchChange(url ? &url->data() : nullptr,
                               DefaultSearchManager::FROM_USER);
      selection_added = true;
    } else {
      // When we are setting the search engine choice from choice screens,
      // the DSP source is expected to allow the search engine to be changed by
      // the user. But theoretically there is a possibility that a policy
      // kicked in after a choice screen was shown, that could be a way to
      // enter this state
      // TODO(crbug.com/328041262): Investigate mitigation options.
      CHECK_NE(choice_made_location,
               search_engines::ChoiceMadeLocation::kOther);
    }
  } else {
    // We rely on the DefaultSearchManager to call ApplyDefaultSearchChange if,
    // in fact, the effective DSE changes.
    if (url) {
      default_search_manager_.SetUserSelectedDefaultSearchEngine(url->data());
      selection_added = true;
    } else {
      default_search_manager_.ClearUserSelectedDefaultSearchEngine();
    }
  }

  if (selection_added &&
      // The choice record below should only be done when called from a path
      // associated with a fully featured search engine choice screen.
      choice_made_location != search_engines::ChoiceMadeLocation::kOther) {
    search_engine_choice_service_->RecordChoiceMade(choice_made_location, this);
  }

#if BUILDFLAG(IS_ANDROID)
  // Commit the pref immediately so it isn't lost if the app is killed.
  // TODO(b/316887441): Investigate removing this.
  prefs_->CommitPendingWrite();
#endif
}

DefaultSearchManager* TemplateURLService::GetDefaultSearchManager() {
  return &default_search_manager_;
}

const TemplateURL* TemplateURLService::GetDefaultSearchProvider() const {
  return loaded_ ? default_search_provider_.get()
                 : pre_loading_providers_->default_search_provider();
}

const TemplateURLService::CategorizedTemplateUrls
TemplateURLService::GetCategorizedTemplateURLs(
    template_url_starter_pack_data::StarterPackIdSet
        disabled_starter_pack_ids) {
  CategorizedTemplateUrls data;

  for (TemplateURL* url : GetTemplateURLs()) {
    // Exclude those URL's that cannot be enabled or should be hidden.
    if (disabled_starter_pack_ids.Has(url->starter_pack_id()) ||
        HiddenFromLists(url)) {
      continue;
    }

    const bool is_starter_pack =
        url->starter_pack_id() !=
        template_url_starter_pack_data::StarterPackId::kNone;
    const bool is_extension = url->type() == TemplateURL::OMNIBOX_API_EXTENSION;

    if (ShowInDefaultList(url)) {
      data.active_site_shortcuts.push_back(url);
    } else if (is_starter_pack || is_extension) {
      if (ShowInActivesList(url)) {
        data.active_feature_shortcuts.push_back(url);
      } else {
        data.inactive_feature_shortcuts.push_back(url);
      }
    } else {
      if (ShowInActivesList(url)) {
        data.active_site_shortcuts.push_back(url);
      } else {
        data.inactive_site_shortcuts.push_back(url);
      }
    }
  }

  std::ranges::sort(
      data.active_site_shortcuts,
      internal::OrderTemplateUrlsByPrepopulatedAndManagedAndAlphabetically(
          prepopulate_data_resolver_->GetPrepopulatedEngines()));
  std::ranges::sort(data.inactive_site_shortcuts,
                    internal::OrderTemplateUrlsByManagedAndAlphabetically());

  return data;
}

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
TemplateURLService::PrepopulatedAndRecentlyVisitedTemplateUrls
TemplateURLService::GetPrepopulatedAndRecentlyVisitedTemplateURLs() {
  PrepopulatedAndRecentlyVisitedTemplateUrls data;

  for (TemplateURL* url : GetTemplateURLs()) {
    if (HiddenFromLists(url)) {
      continue;
    }

    if (ShowInDefaultList(url)) {
      data.prepopulated_urls.push_back(url);
      continue;
    }

    const bool is_starter_pack =
        url->starter_pack_id() !=
        template_url_starter_pack_data::StarterPackId::kNone;
    const bool is_extension = url->type() == TemplateURL::OMNIBOX_API_EXTENSION;

    if (is_starter_pack || is_extension) {
      continue;
    }

    data.recently_visited_urls.push_back(url);
  }

  std::ranges::sort(
      data.prepopulated_urls,
      internal::OrderTemplateUrlsByPrepopulatedAndManagedAndAlphabetically(
          prepopulate_data_resolver_->GetPrepopulatedEngines()));
  internal::SortAndFilterRecentlyVisitedURLs(data.recently_visited_urls);

  return data;
}
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)

url::Origin TemplateURLService::GetDefaultSearchProviderOrigin() const {
  const TemplateURL* template_url = GetDefaultSearchProvider();
  if (template_url) {
    GURL search_url = template_url->GenerateSearchURL(search_terms_data());
    return url::Origin::Create(search_url);
  }
  return url::Origin();
}

const TemplateURL*
TemplateURLService::GetDefaultSearchProviderIgnoringExtensions() const {
  std::unique_ptr<TemplateURLData> next_search =
      default_search_manager_.GetDefaultSearchEngineIgnoringExtensions();
  if (!next_search) {
    return nullptr;
  }

  // Find the TemplateURL matching the data retrieved.
  auto iter = std::ranges::find_if(
      template_urls_, [this, &next_search](const auto& turl_to_check) {
        return TemplateURL::MatchesData(turl_to_check.get(), next_search.get(),
                                        search_terms_data());
      });

  if (iter != template_urls_.end()) {
    return iter->get();
  }

  // If a strict match failed, try to match by GUID.
  // TODO(http://crbug.com/498242147): Properly address this mismatch.
  const TemplateURL* guid_match = GetTemplateURLForGUID(next_search->sync_guid);
  if (guid_match) {
    return guid_match;
  }

  return nullptr;
}

bool TemplateURLService::IsSearchResultsPageFromDefaultSearchProvider(
    const GURL& url) const {
  const TemplateURL* default_provider = GetDefaultSearchProvider();
  return default_provider &&
         default_provider->IsSearchURL(url, search_terms_data());
}

GURL TemplateURLService::GenerateSearchURLForDefaultSearchProvider(
    const std::u16string& search_terms) const {
  const TemplateURL* default_provider = GetDefaultSearchProvider();
  return default_provider ? default_provider->GenerateSearchURL(
                                search_terms_data(), search_terms)
                          : GURL();
}

std::optional<TemplateURLService::SearchMetadata>
TemplateURLService::ExtractSearchMetadata(const GURL& url) const {
  const TemplateURL* template_url = GetTemplateURLForHost(url.GetHost());
  if (!template_url) {
    return std::nullopt;
  }

  GURL normalized_url;
  std::u16string normalized_search_terms;
  bool is_valid_search_url =
      template_url && template_url->KeepSearchTermsInURL(
                          url, search_terms_data(),
                          /*keep_search_intent_params=*/true,

                          /*normalize_search_terms=*/true, &normalized_url,
                          &normalized_search_terms);
  if (!is_valid_search_url) {
    return std::nullopt;
  }

  return SearchMetadata{template_url, normalized_url, normalized_search_terms};
}

bool TemplateURLService::IsExtensionControlledDefaultSearch() const {
  return default_search_provider_source_ ==
         DefaultSearchManager::FROM_EXTENSION;
}

void TemplateURLService::RepairPrepopulatedSearchEngines() {
  // Can't clean DB if it hasn't been loaded.
  DCHECK(loaded());

  Scoper scoper(this);

  if ((default_search_provider_source_ == DefaultSearchManager::FROM_USER) ||
      (default_search_provider_source_ ==
       DefaultSearchManager::FROM_FALLBACK)) {
    // Clear |default_search_provider_| in case we want to remove the engine it
    // points to. This will get reset at the end of the function anyway.
    default_search_provider_ = nullptr;
  }

  std::vector<std::unique_ptr<TemplateURLData>> prepopulated_urls =
      prepopulate_data_resolver_->GetPrepopulatedEngines();
  DCHECK(!prepopulated_urls.empty());
  ActionsFromCurrentData actions(CreateActionsFromCurrentPrepopulateData(
      &prepopulated_urls, template_urls_, default_search_provider_,
      prepopulate_data_resolver_.get()));

  // Remove items.
  for (auto i = actions.removed_engines.begin();
       i < actions.removed_engines.end(); ++i) {
    Remove(*i);
  }

  // Edit items.
  for (auto i(actions.edited_engines.begin()); i < actions.edited_engines.end();
       ++i) {
    UpdateData(i->first, i->second);
  }

  // Add items.
  for (std::vector<TemplateURLData>::const_iterator i =
           actions.added_engines.begin();
       i < actions.added_engines.end(); ++i) {
    Add(std::make_unique<TemplateURL>(*i));
  }

  base::AutoReset<DefaultSearchChangeOrigin> change_origin(
      &dsp_change_origin_, DSP_CHANGE_PROFILE_RESET);

  default_search_manager_.ClearUserSelectedDefaultSearchEngine();

  if (default_search_provider_) {
    // Set fallback engine as user selected, because repair is considered a user
    // action and we are expected to sync new fallback engine to other devices.
    const TemplateURLData* fallback_engine_data =
        default_search_manager_.GetFallbackSearchEngine();
    if (fallback_engine_data) {
      TemplateURL* fallback_engine =
          FindPrepopulatedTemplateURL(fallback_engine_data->prepopulate_id);
      // The fallback engine is created from built-in/override data that should
      // always have a prepopulate ID, so this engine should always exist after
      // a repair.
      DCHECK(fallback_engine);
      // Write the fallback engine's GUID to prefs, which will cause
      // OnDefaultSearchProviderGUIDChanged() to set it as the new
      // user-selected engine.
      prefs_->SetString(prefs::kDefaultSearchProviderGUID,
                        fallback_engine->sync_guid());
    }
  } else {
    // If the default search provider came from a user pref we would have been
    // notified of the new (fallback-provided) value in
    // ClearUserSelectedDefaultSearchEngine() above. Since we are here, the
    // value was presumably originally a fallback value (which may have been
    // repaired).
    DefaultSearchManager::Source source;
    const TemplateURLData* new_dse =
        default_search_manager_.GetDefaultSearchEngine(&source);
    ApplyDefaultSearchChange(new_dse, source);
  }
}

void TemplateURLService::RepairStarterPackEngines() {
  DCHECK(loaded());

  Scoper scoper(this);

  std::vector<std::unique_ptr<TemplateURLData>> starter_pack_engines =
      template_url_starter_pack_data::GetStarterPackEngines();
  DCHECK(!starter_pack_engines.empty());
  ActionsFromCurrentData actions(CreateActionsFromCurrentStarterPackData(
      &starter_pack_engines, template_urls_));

  // Remove items.
  for (auto i = actions.removed_engines.begin();
       i < actions.removed_engines.end(); ++i) {
    Remove(*i);
  }

  // Edit items.
  for (auto i(actions.edited_engines.begin()); i < actions.edited_engines.end();
       ++i) {
    UpdateData(i->first, i->second);
  }

  // Add items.
  for (std::vector<TemplateURLData>::const_iterator i =
           actions.added_engines.begin();
       i < actions.added_engines.end(); ++i) {
    Add(std::make_unique<TemplateURL>(*i));
  }
}

void TemplateURLService::RemoveUserAddedTemplateURLs() {
  DCHECK(loaded());

  Scoper scoper(this);

  const TemplateURL* default_provider = GetDefaultSearchProvider();

  std::vector<const TemplateURL*> to_remove;
  for (const auto& turl : template_urls_) {
    if (turl.get() == default_provider) {
      continue;
    }
    if (turl->prepopulate_id() == 0 &&
        turl->starter_pack_id() ==
            template_url_starter_pack_data::StarterPackId::kNone &&
        turl->policy_origin() == TemplateURLData::PolicyOrigin::kNoPolicy &&
        turl->type() != TemplateURL::Type::OMNIBOX_API_EXTENSION &&
        turl->type() != TemplateURL::Type::NORMAL_CONTROLLED_BY_EXTENSION) {
      to_remove.push_back(turl.get());
    }
  }

  for (const auto* turl : to_remove) {
    Remove(turl);
  }
}

void TemplateURLService::AddObserver(TemplateURLServiceObserver* observer) {
  model_observers_.AddObserver(observer);
}

void TemplateURLService::RemoveObserver(TemplateURLServiceObserver* observer) {
  model_observers_.RemoveObserver(observer);
}

void TemplateURLService::Load() {
  if (loaded_ || load_handle_ || disable_load_) {
    return;
  }

  if (web_data_service_) {
    load_handle_ = web_data_service_->GetKeywords(this);
  } else {
    ChangeToLoadedState();
  }
}

base::CallbackListSubscription TemplateURLService::RegisterOnLoadedCallback(
    base::OnceClosure callback) {
  return loaded_ ? base::CallbackListSubscription()
                 : on_loaded_callbacks_.Add(std::move(callback));
}

void TemplateURLService::LogActiveTemplateUrlsOnStartup(
    OwnedTemplateURLVector* template_urls) {
  DCHECK(template_urls);

  for (auto& turl : *template_urls) {
    std::string histogram_name = kKeywordModeUsageByEngineTypeHistogramName;
    histogram_name.append(
        (turl->is_active() == TemplateURLData::ActiveStatus::kTrue)
            ? ".ActiveOnStartup"
            : ".InactiveOnStartup");
    base::UmaHistogramEnumeration(
        histogram_name, turl->GetBuiltinEngineType(),
        BuiltinEngineType::KEYWORD_MODE_ENGINE_TYPE_MAX);
  }
}

void TemplateURLService::LogTemplateUrlTypesOnStartup(
    OwnedTemplateURLVector* template_urls) {
  DCHECK(template_urls);

  // Initialize counts for each type of `TemplateURL`.
  int num_total_turl = 0;
  int num_prepopulated = 0;
  int num_featured_policy_set_site_search = 0;
  int num_policy_set_aggregator = 0;
  int num_featured_policy_set_aggregator = 0;
  int num_starter_pack = 0;
  int num_extension_set_search = 0;
  int num_non_featured_policy_set_site_search = 0;
  int num_policy_set_default_search = 0;
  int num_user_set_default_search = 0;
  int num_user_set_substituting_site_search = 0;
  int num_user_set_non_substituting_site_search = 0;
  int num_featured_allow_user_override_policy_set_site_search = 0;
  int num_non_featured_allow_user_override_policy_set_site_search = 0;

  // Count the number of each type of `TemplateURL`.
  for (auto& turl : *template_urls) {
    const TemplateURLData& data = turl->data();
    // Prepopulated keywords can have `is_active()` equal to
    // `ActiveStatus::kTrue` or `ActiveStatus::kUnspecified`.
    bool is_prepopulated =
        data.prepopulate_id != 0 &&
        turl->is_active() != TemplateURLData::ActiveStatus::kFalse;
    if ((!is_prepopulated &&
         turl->is_active() == TemplateURLData::ActiveStatus::kUnspecified) ||
        turl->is_active() == TemplateURLData::ActiveStatus::kFalse) {
      continue;
    }
    num_total_turl++;
    if (is_prepopulated) {
      num_prepopulated++;
    } else if (turl->featured_by_policy()) {
      if (data.CreatedBySiteSearchPolicy()) {
        data.enforced_by_policy
            ? num_featured_policy_set_site_search++
            : num_featured_allow_user_override_policy_set_site_search++;
      } else if (data.CreatedByEnterpriseSearchAggregatorPolicy()) {
        num_featured_policy_set_aggregator++;
      } else {
        NOTREACHED();
      }
    } else if (data.starter_pack_id != 0) {
      num_starter_pack++;
    } else if (turl->type() == TemplateURL::NORMAL_CONTROLLED_BY_EXTENSION ||
               turl->type() == TemplateURL::OMNIBOX_API_EXTENSION) {
      num_extension_set_search++;
    } else if (data.CreatedBySiteSearchPolicy()) {
      data.enforced_by_policy
          ? num_non_featured_policy_set_site_search++
          : num_non_featured_allow_user_override_policy_set_site_search++;
    } else if (data.CreatedByEnterpriseSearchAggregatorPolicy()) {
      num_policy_set_aggregator++;
    } else if (data.CreatedByDefaultSearchProviderPolicy()) {
      num_policy_set_default_search++;
    } else if (GetDefaultSearchProvider() &&
               data.url() == GetDefaultSearchProvider()->url()) {
      num_user_set_default_search++;
    } else if (!data.CreatedByPolicy()) {
      turl->SupportsReplacement(search_terms_data())
          ? num_user_set_substituting_site_search++
          : num_user_set_non_substituting_site_search++;
    } else {
      NOTREACHED();
    }
  }

  base::UmaHistogramExactLinear(base::StringPrintf(kKeywordCountHistogramName),
                                num_total_turl, 50);

  base::UmaHistogramExactLinear(
      base::StringPrintf("%s.FeaturedSiteSearchSetByPolicy",
                         kKeywordCountHistogramName),
      num_featured_policy_set_site_search, 50);

  base::UmaHistogramExactLinear(
      base::StringPrintf("%s.SearchAggregatorSetByPolicy",
                         kKeywordCountHistogramName),
      num_policy_set_aggregator, 50);

  base::UmaHistogramExactLinear(
      base::StringPrintf("%s.FeaturedSearchAggregatorSetByPolicy",
                         kKeywordCountHistogramName),
      num_featured_policy_set_aggregator, 50);

  base::UmaHistogramExactLinear(
      base::StringPrintf("%s.StarterPack", kKeywordCountHistogramName),
      num_starter_pack, 50);

  base::UmaHistogramExactLinear(
      base::StringPrintf("%s.Prepopulated", kKeywordCountHistogramName),
      num_prepopulated, 50);

  base::UmaHistogramExactLinear(
      base::StringPrintf("%s.SearchEngineSetByExtension",
                         kKeywordCountHistogramName),
      num_extension_set_search, 50);

  base::UmaHistogramExactLinear(
      base::StringPrintf("%s.NonFeaturedSiteSearchSetByPolicy",
                         kKeywordCountHistogramName),
      num_non_featured_policy_set_site_search, 50);

  base::UmaHistogramExactLinear(
      base::StringPrintf("%s.DefaultSearchEngineSetByPolicy",
                         kKeywordCountHistogramName),
      num_policy_set_default_search, 50);

  base::UmaHistogramExactLinear(
      base::StringPrintf("%s.DefaultSearchEngineSetByUser",
                         kKeywordCountHistogramName),
      num_user_set_default_search, 50);

  base::UmaHistogramExactLinear(
      base::StringPrintf("%s.SubstitutingSiteSearchSetByUser",
                         kKeywordCountHistogramName),
      num_user_set_substituting_site_search, 50);

  base::UmaHistogramExactLinear(
      base::StringPrintf("%s.NonSubstitutingSiteSearchSetByUser",
                         kKeywordCountHistogramName),
      num_user_set_non_substituting_site_search, 50);

  base::UmaHistogramExactLinear(
      base::StringPrintf("%s.FeaturedAllowUserOverrideSiteSearchSetByPolicy",
                         kKeywordCountHistogramName),
      num_featured_allow_user_override_policy_set_site_search, 50);

  base::UmaHistogramExactLinear(
      base::StringPrintf("%s.NonFeaturedAllowUserOverrideSiteSearchSetByPolicy",
                         kKeywordCountHistogramName),
      num_non_featured_allow_user_override_policy_set_site_search, 50);
}

void TemplateURLService::OnWebDataServiceRequestDone(
    KeywordWebDataService::Handle h,
    std::unique_ptr<WDTypedResult> result) {
  // Reset the load_handle so that we don't try and cancel the load in
  // the destructor.
  load_handle_ = 0;

  if (!result) {
    // Results are null if the database went away or (most likely) wasn't
    // loaded.
    load_failed_ = true;
    web_data_service_ = nullptr;
    ChangeToLoadedState();
    return;
  }

  DCHECK_EQ(KEYWORDS_RESULT, result->GetType());
  std::unique_ptr<OwnedTemplateURLVector> template_urls =
      std::make_unique<OwnedTemplateURLVector>();
  WDKeywordsResult::Metadata updated_keywords_metadata;

  {
    const WDKeywordsResult& keyword_result =
        reinterpret_cast<const WDResult<WDKeywordsResult>*>(result.get())
            ->GetValue();
    initial_keywords_database_country_ =
        keyword_result.metadata.builtin_keyword_country;
    std::set<std::string> removed_keyword_guids;
    GetSearchProvidersUsingKeywordResult(
        keyword_result, web_data_service_.get(), &prefs_.get(),
        prepopulate_data_resolver_.get(), template_urls.get(),
        (default_search_provider_source_ == DefaultSearchManager::FROM_USER)
            ? pre_loading_providers_->default_search_provider()
            : nullptr,
        search_terms_data(), updated_keywords_metadata, &removed_keyword_guids);
    updated_keywords_database_country_ =
        updated_keywords_metadata.builtin_keyword_country;
  }

  Scoper scoper(this);

  {
    PatchMissingSyncGUIDs(template_urls.get());
    MaybeSetIsActiveSearchEngines(template_urls.get());
    LogActiveTemplateUrlsOnStartup(template_urls.get());
    LogTemplateUrlTypesOnStartup(template_urls.get());
    SetTemplateURLs(std::move(template_urls));

    // This initializes provider_map_ which should be done before
    // calling UpdateKeywordSearchTermsForURL.
    ChangeToLoadedState();

    // Index any visits that occurred before we finished loading.
    for (const auto& visit_to_add : visits_to_add_) {
      UpdateKeywordSearchTermsForURL(visit_to_add);
    }
    visits_to_add_.clear();

    if (updated_keywords_metadata.HasBuiltinKeywordData()) {
      web_data_service_->SetBuiltinKeywordDataVersion(
          updated_keywords_metadata.builtin_keyword_data_version);
      web_data_service_->SetBuiltinKeywordCountry(
          updated_keywords_metadata.builtin_keyword_country->GetRestricted(
              regional_capabilities::CountryAccessKey(
                  regional_capabilities::CountryAccessReason::
                      kTemplateURLServiceDatabaseMetadataCaching)));
      web_data_service_->SetPrepopulatedEnginesMigrationState(
          updated_keywords_metadata.prepopulated_engines_migration_state);
    }

    if (updated_keywords_metadata.HasStarterPackData()) {
      web_data_service_->SetStarterPackKeywordVersion(
          updated_keywords_metadata.starter_pack_version);
    }
  }

  if (default_search_provider_) {
    SearchEngineType engine_type =
        default_search_provider_->GetEngineType(search_terms_data());
    // Check for search engines types not present in prepopulated_engines.json.
    // TODO(https://issues.chromium.org/405167888): Remove this check once it is
    // no longer necessary to track these additional search engine types.
    if (engine_type == SEARCH_ENGINE_OTHER) {
      GURL search_url = GURL(default_search_provider_->url());
      if (search_url.is_valid() &&
          url::DomainIs(search_url.host(), "siteadvisor.com")) {
        engine_type = SEARCH_ENGINE_MCAFEE;
      }
    }
    base::UmaHistogramEnumeration("Search.DefaultSearchProviderType2",
                                  engine_type, SEARCH_ENGINE_MAX);
    if (default_search_provider_->CreatedByDefaultSearchProviderPolicy()) {
      base::UmaHistogramEnumeration(
          "Search.DefaultSearchProviderType2.SetByEnterprisePolicy",
          engine_type, SEARCH_ENGINE_MAX);
    } else if (default_search_provider_source_ ==
               DefaultSearchManager::FROM_EXTENSION) {
      base::UmaHistogramEnumeration(
          "Search.DefaultSearchProviderType2.SetByExtension", engine_type,
          SEARCH_ENGINE_MAX);
    } else if (default_search_provider_source_ ==
               DefaultSearchManager::FROM_USER) {
      base::UmaHistogramEnumeration(
          "Search.DefaultSearchProviderType2.SetByUser", engine_type,
          SEARCH_ENGINE_MAX);
    } else if (default_search_provider_source_ ==
               DefaultSearchManager::FROM_FALLBACK) {
      base::UmaHistogramEnumeration(
          "Search.DefaultSearchProviderType2.Fallback", engine_type,
          SEARCH_ENGINE_MAX);
    }
  }

  if (regional_capabilities_service_->IsSearchEngineSplitRegion()) {
    search_engines::RecordSearchEngineSplitProfileLoadMetrics(
        template_urls_, default_search_provider_, search_terms_data(),
        *profile_metrics_service_);
  }
}

void TemplateURLService::OnHistoryURLVisited(const URLVisitedDetails& details) {
  if (!loaded_) {
    visits_to_add_.push_back(details);
  } else {
    UpdateKeywordSearchTermsForURL(details);
  }
}

void TemplateURLService::Shutdown() {
  for (auto& observer : model_observers_) {
    observer.OnTemplateURLServiceShuttingDown();
  }

  if (client_) {
    client_->Shutdown();
  }
  // This check has to be done at Shutdown() instead of in the dtor to ensure
  // that no clients of KeywordWebDataService are holding ptrs to it after the
  // first phase of the KeyedService Shutdown() process.
  if (load_handle_) {
    DCHECK(web_data_service_);
    web_data_service_->CancelRequest(load_handle_);
  }
  web_data_service_ = nullptr;
}

std::string TemplateURLService::GetSessionToken() {
  base::TimeTicks current_time(base::TimeTicks::Now());
  // Renew token if it expired.
  if (current_time > token_expiration_time_) {
    std::array<uint8_t, 12> raw_data;
    base::RandBytes(raw_data);
    base::Base64UrlEncode(raw_data,
                          base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                          &current_token_);
  }

  // Extend expiration time another 60 seconds.
  token_expiration_time_ = current_time + base::Seconds(60);
  return current_token_;
}

void TemplateURLService::ClearSessionToken() {
  token_expiration_time_ = base::TimeTicks();
}

void TemplateURLService::Init() {
  if (client_) {
    client_->SetOwner(this);
  }

  pref_change_registrar_.Init(&prefs_.get());
  pref_change_registrar_.Add(
      prefs::kDefaultSearchProviderGUID,
      base::BindRepeating(
          &TemplateURLService::OnDefaultSearchProviderGUIDChanged,
          base::Unretained(this)));

  DefaultSearchManager::Source source = DefaultSearchManager::FROM_USER;
  const TemplateURLData* dse =
      default_search_manager_.GetDefaultSearchEngine(&source);

  Scoper scoper(this);

  ApplyDefaultSearchChange(dse, source);
}

void TemplateURLService::ApplyInitializersForTesting(
    base::span<const TemplateURLService::Initializer> initializers) {
  // This path is only hit by test code and is used to simulate a loaded
  // TemplateURLService.
  CHECK_IS_TEST();

  if (initializers.empty()) {
    return;
  }

  ChangeToLoadedState();

  // Add specific initializers, if any.
  for (size_t i = 0; i < initializers.size(); ++i) {
    CHECK(initializers[i].keyword);
    CHECK(initializers[i].url);
    CHECK(initializers[i].content);

    // TemplateURLService ends up owning the TemplateURL, don't try and free
    // it.
    TemplateURLData data;
    data.SetShortName(base::UTF8ToUTF16(initializers[i].content));
    data.SetKeyword(base::UTF8ToUTF16(initializers[i].keyword));
    data.SetURL(initializers[i].url);
    // Set all to active by default for testing purposes.
    data.is_active = TemplateURLData::ActiveStatus::kTrue;
    Add(std::make_unique<TemplateURL>(data));

    // Set the first provided identifier to be the default.
    if (i == 0) {
      default_search_manager_.SetUserSelectedDefaultSearchEngine(data);
    }
  }
}

void TemplateURLService::RemoveFromMaps(const TemplateURL* template_url) {
  const std::u16string& keyword = template_url->keyword();

  // Remove from |keyword_to_turl_|. No need to find the best
  // fallback. We choose the best one as-needed from the multimap.
  const auto match_range = keyword_to_turl_.equal_range(keyword);
  for (auto it = match_range.first; it != match_range.second;) {
    if (it->second == template_url) {
      it = keyword_to_turl_.erase(it);
    } else {
      ++it;
    }
  }

  if (template_url->type() == TemplateURL::OMNIBOX_API_EXTENSION) {
    return;
  }

  if (!template_url->sync_guid().empty()) {
    guid_to_turl_.erase(template_url->sync_guid());
    if (postponed_deleted_default_engine_guid_ == template_url->sync_guid()) {
      // `template_url` has been updated locally or removed, discard incoming
      // deletion.
      postponed_deleted_default_engine_guid_.clear();
    }
  }

  if (template_url->CreatedByNonDefaultSearchProviderPolicy()) {
    enterprise_search_keyword_to_turl_.erase(keyword);
  }

  // |provider_map_| is only initialized after loading has completed.
  if (loaded_) {
    provider_map_->Remove(template_url);
  }
}

void TemplateURLService::AddToMaps(TemplateURL* template_url) {
  const std::u16string& keyword = template_url->keyword();
  keyword_to_turl_.insert(std::make_pair(keyword, template_url));

  if (template_url->type() == TemplateURL::OMNIBOX_API_EXTENSION) {
    return;
  }

  if (!template_url->sync_guid().empty()) {
    guid_to_turl_[template_url->sync_guid()] = template_url;
  }

  if (template_url->CreatedByNonDefaultSearchProviderPolicy()) {
    enterprise_search_keyword_to_turl_[keyword] = template_url;
  }

  // |provider_map_| is only initialized after loading has completed.
  if (loaded_) {
    provider_map_->Add(template_url, search_terms_data());
  }
}

void TemplateURLService::SetTemplateURLs(
    std::unique_ptr<OwnedTemplateURLVector> urls) {
  Scoper scoper(this);

  // Partition the URLs first, instead of implementing the loops below by simply
  // scanning the input twice.  While it's not supposed to happen normally, it's
  // possible for corrupt databases to return multiple entries with the same
  // keyword.  In this case, the first loop may delete the first entry when
  // adding the second.  If this happens, the second loop must not attempt to
  // access the deleted entry.  Partitioning ensures this constraint.
  auto first_invalid = std::partition(
      urls->begin(), urls->end(), [](const std::unique_ptr<TemplateURL>& turl) {
        return turl->id() != kInvalidTemplateURLID;
      });

  // First, add the items that already have id's, so that the next_id_ gets
  // properly set.
  for (auto i = urls->begin(); i != first_invalid; ++i) {
    next_id_ = std::max(next_id_, (*i)->id());
    Add(std::move(*i), false);
  }

  // Next add the new items that don't have id's.
  for (auto i = first_invalid; i != urls->end(); ++i) {
    Add(std::move(*i));
  }
}

void TemplateURLService::ChangeToLoadedState() {
  DCHECK(!loaded_);

  provider_map_->Init(template_urls_, search_terms_data());
  loaded_ = true;

  ApplyDefaultSearchChangeNoMetrics(
      pre_loading_providers_->default_search_provider()
          ? &pre_loading_providers_->default_search_provider()->data()
          : nullptr,
      default_search_provider_source_);
  ApplyEnterpriseSearchChanges(pre_loading_providers_->TakeSearchEngines());
  pre_loading_providers_.reset();

  on_loaded_callbacks_.Notify();
}

bool TemplateURLService::CanAddAutogeneratedKeywordForHost(
    const std::string& host) const {
  const TemplateURLSet* urls = provider_map_->GetURLsForHost(host);
  if (!urls) {
    return true;
  }

  return std::ranges::all_of(*urls, [](const TemplateURL* turl) {
    return turl->safe_for_autoreplace();
  });
}

bool TemplateURLService::Update(TemplateURL* existing_turl,
                                const TemplateURL& new_values) {
  DCHECK(existing_turl);
  if (!Contains(&template_urls_, existing_turl)) {
    return false;
  }

  // Account data is never used without sync.
  const bool should_remove_account_data = new_values.GetAccountData().has_value();
  // It is possible that corresponding local data didn't exist before and now
  // `new_values` writes local data. In such case, an add operation needs to be
  // performed on the database instead of update.
  const bool is_newly_adding_local_data =
      new_values.GetLocalData() && !existing_turl->GetLocalData();

  Scoper scoper(this);
  model_mutated_notification_pending_ = true;

  TemplateURLID previous_id = existing_turl->id();
  RemoveFromMaps(existing_turl);

  // Update existing turl with new values and add back to the map.
  // We don't do any keyword conflict handling here, as TemplateURLService
  // already can pick the best engine out of duplicates. Replaceable duplicates
  // will be culled during next startup's Add() loop. We did this to keep
  // Update() simple: it never fails, and never deletes |existing_engine|.
  if (should_remove_account_data) {
    existing_turl->CopyFrom(TemplateURL(*new_values.GetLocalData()));
  } else {
    existing_turl->CopyFrom(new_values);
  }
  existing_turl->set_id(previous_id);

  AddToMaps(existing_turl);

  if (existing_turl->type() == TemplateURL::NORMAL) {
    if (web_data_service_ && existing_turl->GetLocalData()) {
      if (is_newly_adding_local_data) {
        web_data_service_->AddKeyword(*existing_turl->GetLocalData());
      } else {
        web_data_service_->UpdateKeyword(*existing_turl->GetLocalData());
      }
    }

    if (!applying_default_search_engine_change_ &&
        GetDefaultSearchProvider() == existing_turl &&
        CanDefaultSearchProviderBeModifiedByUser()) {
      default_search_manager_.SetUserSelectedDefaultSearchEngine(
          existing_turl->data());
    }
  }

  return true;
}

bool TemplateURLService::UpdateData(TemplateURL* existing_turl,
                                    TemplateURLData new_data) {
  return Update(existing_turl,
                TemplateURL(new_data, existing_turl->GetAccountData()));
}

void TemplateURLService::UpdateKeywordSearchTermsForURL(
    const URLVisitedDetails& details) {
  if (!details.url.is_valid()) {
    return;
  }

  // AI mode URLs should not be stored. Otherwise, since they fit the
  // traditional search `TemplateURL`'s URL, those would be incorrectly
  // attributed.
  if (IsGoogleAiModeUrl(details.url)) {
    return;
  }

  const TemplateURLSet* urls_for_host =
      provider_map_->GetURLsForHost(details.url.GetHost());
  if (!urls_for_host) {
    return;
  }

  TemplateURL* visited_url = nullptr;
  for (auto i = urls_for_host->begin(); i != urls_for_host->end(); ++i) {
    TemplateURL& template_url = **i;

    // AI mode keyword should not be attributed. Otherwise, they would be
    // incorrectly attributed by traditional search URLs, which fit the AI mode
    // `TemplateURL`'s URL.
    if (template_url.starter_pack_id() ==
        template_url_starter_pack_data::StarterPackId::kAiMode) {
      continue;
    }

    std::u16string search_terms;
    if (template_url.ExtractSearchTermsFromURL(details.url, search_terms_data(),
                                               &search_terms) &&
        !search_terms.empty()) {
      if (details.is_keyword_transition) {
        // The visit is the result of the user entering a keyword, generate a
        // KEYWORD_GENERATED visit for the KEYWORD so that the keyword typed
        // count is boosted.
        AddTabToSearchVisit(template_url);
      }
      if (client_) {
        client_->SetKeywordSearchTermsForURL(details.url, template_url.id(),
                                             search_terms);
      }
      // Caches the matched TemplateURL so its last_visited could be updated
      // later after iteration.
      // Note: Update() will replace the entry from the container of this
      // iterator, so update here directly will cause an error about it.
      if (!IsCreatedByExtension(template_url)) {
        visited_url = &template_url;
      }
    }
  }
  if (visited_url) {
    UpdateTemplateURLVisitTime(visited_url);
  }
}

void TemplateURLService::AddToUnscopedModeExtensionIds(
    const std::string& extension_id) {
  CHECK(!extension_id.empty());
  unscoped_mode_extension_ids_.insert(extension_id);
}

void TemplateURLService::RemoveFromUnscopedModeExtensionIdsIfPresent(
    const std::string& extension_id) {
  if (unscoped_mode_extension_ids_.contains(extension_id)) {
    unscoped_mode_extension_ids_.erase(extension_id);
  }
}

std::set<std::string> TemplateURLService::GetUnscopedModeExtensionIds() const {
  return unscoped_mode_extension_ids_;
}

void TemplateURLService::UpdateTemplateURLVisitTime(TemplateURL* url) {
  TemplateURLData data(url->data());
  data.last_visited = clock_->Now();
  UpdateData(url, data);
}

void TemplateURLService::AddTabToSearchVisit(const TemplateURL& t_url) {
  // Only add visits for entries the user hasn't modified. If the user modified
  // the entry the keyword may no longer correspond to the host name. It may be
  // possible to do something more sophisticated here, but it's so rare as to
  // not be worth it.
  if (!t_url.safe_for_autoreplace()) {
    return;
  }

  if (!client_) {
    return;
  }

  GURL url(url_formatter::FixupURL(base::UTF16ToUTF8(t_url.keyword())));
  if (!url.is_valid()) {
    return;
  }

  // Synthesize a visit for the keyword. This ensures the url for the keyword is
  // autocompleted even if the user doesn't type the url in directly.
  client_->AddKeywordGeneratedVisit(url);
}

void TemplateURLService::ApplyDefaultSearchChange(
    const TemplateURLData* data,
    DefaultSearchManager::Source source) {
  if (!ApplyDefaultSearchChangeNoMetrics(data, source)) {
    return;
  }

  if (GetDefaultSearchProvider() &&
      GetDefaultSearchProvider()->HasGoogleBaseURLs(search_terms_data()) &&
      !dsp_change_callback_.is_null()) {
    dsp_change_callback_.Run();
  }
}

bool TemplateURLService::ApplyDefaultSearchChangeForTesting(
    const TemplateURLData* data,
    DefaultSearchManager::Source source) {
  CHECK_IS_TEST();
  return ApplyDefaultSearchChangeNoMetrics(data, source);
}

bool TemplateURLService::ApplyDefaultSearchChangeNoMetrics(
    const TemplateURLData* data,
    DefaultSearchManager::Source source) {
  // We do not want any sort of reentrancy while changing the default search
  // engine. This can occur when resolving conflicting entries. In those cases,
  // it's best to early exit and let the original process finish.
  if (applying_default_search_engine_change_) {
    return false;
  }
  base::AutoReset<bool> applying_change(&applying_default_search_engine_change_,
                                        true);

  if (!loaded_) {
    // Set pre-loading default search provider from the preferences. This is
    // mainly so we can hold ownership until we get to the point where the list
    // of keywords from Web Data is the owner of everything including the
    // default.
    bool changed = !TemplateURL::MatchesData(
        pre_loading_providers_->default_search_provider(), data,
        search_terms_data());
    TemplateURL::Type initial_engine_type =
        (source == DefaultSearchManager::FROM_EXTENSION)
            ? TemplateURL::NORMAL_CONTROLLED_BY_EXTENSION
            : TemplateURL::NORMAL;
    pre_loading_providers_->set_default_search_provider(
        data ? std::make_unique<TemplateURL>(*data, initial_engine_type)
             : nullptr);
    default_search_provider_source_ = source;
    return changed;
  }

  // This may be deleted later. Use exclusively for pointer comparison to detect
  // a change.
  TemplateURL* previous_default_search_engine = default_search_provider_;
  std::string previous_default_search_engine_guid =
      previous_default_search_engine
          ? previous_default_search_engine->sync_guid()
          : "";

  Scoper scoper(this);

  if (default_search_provider_source_ == DefaultSearchManager::FROM_POLICY ||
      default_search_provider_source_ ==
          DefaultSearchManager::FROM_POLICY_RECOMMENDED ||
      source == DefaultSearchManager::FROM_POLICY ||
      source == DefaultSearchManager::FROM_POLICY_RECOMMENDED) {
    // We do this both to remove any no-longer-applicable policy-defined DSE as
    // well as to add the new one, if appropriate.
    UpdateDefaultProvidersCreatedByPolicy(
        &template_urls_,
        source == DefaultSearchManager::FROM_POLICY ||
                source == DefaultSearchManager::FROM_POLICY_RECOMMENDED
            ? data
            : nullptr,
        /*is_mandatory=*/source == DefaultSearchManager::FROM_POLICY);
  }

  // |default_search_provider_source_| must be set before calling Update(),
  // since that function needs to know the source of the update in question.
  default_search_provider_source_ = source;

  if (source != DefaultSearchManager::FROM_POLICY) {
    UpdateRecommendedDefaultSearchProvider();
  }

  if (!data) {
    default_search_provider_ = nullptr;
  } else if (source == DefaultSearchManager::FROM_EXTENSION) {
    default_search_provider_ = FindMatchingDefaultExtensionTemplateURL(*data);
  } else if (source == DefaultSearchManager::FROM_FALLBACK) {
    default_search_provider_ =
        FindPrepopulatedTemplateURL(data->prepopulate_id);
    if (default_search_provider_) {
      TemplateURLData update_data(*data);
      update_data.sync_guid = default_search_provider_->sync_guid();

      // Now that we are auto-updating the favicon_url as the user browses,
      // respect the favicon_url entry in the database, instead of falling back
      // to the one in the prepopulated list.
      update_data.favicon_url = default_search_provider_->favicon_url();

      if (!default_search_provider_->safe_for_autoreplace()) {
        update_data.safe_for_autoreplace = false;
        update_data.SetKeyword(default_search_provider_->keyword());
        update_data.SetShortName(default_search_provider_->short_name());
      }
      UpdateData(default_search_provider_, update_data);
    } else {
      // Normally the prepopulated fallback should be present in
      // |template_urls_|, but in a few cases it might not be:
      // (1) Tests that initialize the TemplateURLService in peculiar ways.
      // (2) If the user deleted the pre-populated default and we subsequently
      // lost their user-selected value.
      default_search_provider_ = Add(std::make_unique<TemplateURL>(*data));
      DCHECK(default_search_provider_)
          << "Add() to repair the DSE must never fail.";
    }
  } else if (source == DefaultSearchManager::FROM_USER) {
    default_search_provider_ = GetTemplateURLForGUID(data->sync_guid);
    if (!default_search_provider_ && data->prepopulate_id) {
      default_search_provider_ =
          FindPrepopulatedTemplateURL(data->prepopulate_id);
    }
    TemplateURLData new_data(*data);
    if (default_search_provider_) {
      UpdateData(default_search_provider_, new_data);
    } else {
      new_data.id = kInvalidTemplateURLID;
      default_search_provider_ = Add(std::make_unique<TemplateURL>(new_data));
      DCHECK(default_search_provider_)
          << "Add() to repair the DSE must never fail.";
    }
    if (default_search_provider_) {
      prefs_->SetString(prefs::kDefaultSearchProviderGUID,
                        default_search_provider_->sync_guid());
    }
  }

  if (default_search_provider_ == previous_default_search_engine) {
    // Default search engine hasn't changed.
    return false;
  }

  // We must fetch the previous DSE via its GUID rather than using the
  // `previous_default_search_engine` pointer directly. This is because
  // operations earlier in this function (like
  // `UpdateDefaultProvidersCreatedByPolicy()`) may have deleted the engine
  // from memory, leaving the original pointer dangling.
  TemplateURL* previous_turl =
      previous_default_search_engine_guid.empty()
          ? nullptr
          : GetTemplateURLForGUID(previous_default_search_engine_guid);
  if (previous_turl &&
      previous_turl->starter_pack_id() ==
          template_url_starter_pack_data::StarterPackId::kNone &&
      !IsPrepopulatedOrDefaultProviderByPolicy(previous_turl) &&
      base::FeatureList::IsEnabled(
          switches::kVisitCustomSearchOnUndefaulting)) {
    UpdateTemplateURLVisitTime(previous_turl);
  }

  model_mutated_notification_pending_ = true;
  if (!postponed_deleted_default_engine_guid_.empty()) {
    // There was a postponed deletion for the previous default search engine,
    // remove it now.
    TemplateURL* existing_turl =
        GetTemplateURLForGUID(postponed_deleted_default_engine_guid_);
    if (existing_turl) {
      // Remove() below CHECKs that the current default search engine is not
      // deleted.
      Remove(existing_turl);
    }
    postponed_deleted_default_engine_guid_.clear();
  }

  return true;
}

void TemplateURLService::ApplyEnterpriseSearchChanges(
    TemplateURLService::OwnedTemplateURLVector&& policy_search_engines) {
  CHECK(loaded_);

  Scoper scoper(this);

  base::flat_set<std::u16string> new_keywords;
  std::ranges::transform(
      policy_search_engines, std::inserter(new_keywords, new_keywords.begin()),
      [](const std::unique_ptr<TemplateURL>& turl) { return turl->keyword(); });

  // Remove old site search entries no longer present in the policy's list.
  //
  // Note: auxiliary `keywords_to_remove` is needed to avoid reentry issues
  //       while removing elements from
  //       `enterprise_search_keyword_to_turl_`.
  //
  // Note: This can be made more idiomatic once Chromium style allows
  //       `std::views::keys`:
  //       std::copy_if(
  //           std::views::keys(enterprise_search_keyword_to_turl_),
  //           std::inserter(keywords_to_remove, keywords_to_remove.begin()),
  //           [new_keywords] (const std::u16string& keyword) {
  //             new_keywords.find(keyword) == new_keywords.end()
  //           });
  base::flat_set<std::u16string> keywords_to_remove;
  for (auto [keyword, _] : enterprise_search_keyword_to_turl_) {
    if (new_keywords.find(keyword) == new_keywords.end()) {
      keywords_to_remove.insert(keyword);
    }
  }
  std::ranges::for_each(keywords_to_remove,
                        [this](const std::u16string& keyword) {
                          Remove(enterprise_search_keyword_to_turl_[keyword]);
                        });

  // Either add new site search entries or update existing ones if necessary.
  for (auto& search_engine : policy_search_engines) {
    const std::u16string& keyword = search_engine->keyword();
    auto it = enterprise_search_keyword_to_turl_.find(keyword);
    if (it == enterprise_search_keyword_to_turl_.end()) {
      Add(std::move(search_engine), /*newly_adding=*/true);
    } else if (ShouldMergeEnterpriseSearchEngines(
                   /*existing_turl=*/*it->second,
                   /*new_values=*/*search_engine)) {
      UpdateData(/*existing_turl=*/it->second,
                 /*new_data=*/MergeEnterpriseSearchEngines(
                     /*existing_data=*/it->second->data(),
                     /*new_values=*/*search_engine));
    }
  }
}

void TemplateURLService::EnterpriseSearchChanged(
    OwnedTemplateURLDataVector&& policy_search_engines) {
  OwnedTemplateURLVector turl_search_engines;
  for (const std::unique_ptr<TemplateURLData>& it : policy_search_engines) {
    turl_search_engines.push_back(
        std::make_unique<TemplateURL>(*it, TemplateURL::NORMAL));
  }

  if (loaded_) {
    ApplyEnterpriseSearchChanges(std::move(turl_search_engines));
  } else {
    pre_loading_providers_->set_search_engines(std::move(turl_search_engines));
  }
}

TemplateURL* TemplateURLService::Add(std::unique_ptr<TemplateURL> template_url,
                                     bool newly_adding) {
  DCHECK(template_url);

  Scoper scoper(this);

  // Remove account data from `template_url`: there is no account sync.
  if (template_url->GetAccountData()) {
    if (!template_url->GetLocalData()) {
      return nullptr;
    }
    template_url = std::make_unique<TemplateURL>(template_url->GetLocalData(),
                                                 std::nullopt);
  }

  if (newly_adding) {
    DCHECK_EQ(kInvalidTemplateURLID, template_url->id());
    DCHECK(!Contains(&template_urls_, template_url.get()));
    template_url->set_id(++next_id_);
  }

  template_url->ResetKeywordIfNecessary(search_terms_data(), false);

  // Early exit if the newly added TemplateURL was a replaceable duplicate.
  // No need to inform either Sync or flag on the model-mutated in that case.
  if (RemoveDuplicateReplaceableEnginesOf(template_url.get())) {
    return nullptr;
  }

  TemplateURL* template_url_ptr = template_url.get();
  template_urls_.push_back(std::move(template_url));
  AddToMaps(template_url_ptr);

  if (newly_adding && (template_url_ptr->type() == TemplateURL::NORMAL)) {
    if (web_data_service_ && template_url_ptr->GetLocalData()) {
      web_data_service_->AddKeyword(*template_url_ptr->GetLocalData());
    }
  }

  if (template_url_ptr) {
    model_mutated_notification_pending_ = true;
  }

  return template_url_ptr;
}

// |template_urls| are the TemplateURLs loaded from the database.
// |default_from_prefs| is the default search provider from the preferences, or
// NULL if the DSE is not policy-defined.
//
// This function removes from the vector and the database all the TemplateURLs
// that were set by policy as default provider, unless it is the current default
// search provider, in which case it is updated with the data from prefs.
void TemplateURLService::UpdateDefaultProvidersCreatedByPolicy(
    OwnedTemplateURLVector* template_urls,
    const TemplateURLData* default_from_prefs,
    bool is_mandatory) {
  DCHECK(template_urls);

  Scoper scoper(this);

  for (auto i = template_urls->begin(); i != template_urls->end();) {
    TemplateURL* template_url = i->get();
    if (template_url->CreatedByDefaultSearchProviderPolicy()) {
      if (default_from_prefs &&
          TemplateURL::MatchesData(template_url, default_from_prefs,
                                   search_terms_data())) {
        // If the database specified a default search provider that was set
        // by policy, and the default search provider from the preferences
        // is also set by policy and they are the same, keep the entry in the
        // database and the |default_search_provider|.
        default_search_provider_ = template_url;
        // Prevent us from saving any other entries, or creating a new one.
        default_from_prefs = nullptr;
        ++i;
        continue;
      }

      // If the previous default search provider was set as a recommended policy
      // and the new provider is not set by policy, keep the previous provider
      // in the database. This allows the recommended provider to remain in the
      // list if the user switches to a different provider.
      if (template_url->enforced_by_policy() || default_from_prefs) {
        TemplateURLID id = template_url->id();
        RemoveFromMaps(template_url);
        i = template_urls->erase(i);
        if (web_data_service_) {
          web_data_service_->RemoveKeyword(id);
        }
      } else {
        ++i;
      }
    } else {
      ++i;
    }
  }

  if (default_from_prefs) {
    default_search_provider_ = nullptr;
    default_search_provider_source_ =
        is_mandatory ? DefaultSearchManager::FROM_POLICY
                     : DefaultSearchManager::FROM_POLICY_RECOMMENDED;
    TemplateURLData new_data(*default_from_prefs);
    if (new_data.sync_guid.empty()) {
      new_data.GenerateSyncGUID();
    }
    new_data.policy_origin =
        TemplateURLData::PolicyOrigin::kDefaultSearchProvider;
    new_data.enforced_by_policy = is_mandatory;
    new_data.safe_for_autoreplace = false;
    new_data.is_active = TemplateURLData::ActiveStatus::kTrue;
    std::unique_ptr<TemplateURL> new_dse_ptr =
        std::make_unique<TemplateURL>(new_data);
    TemplateURL* new_dse = new_dse_ptr.get();
    if (Add(std::move(new_dse_ptr))) {
      default_search_provider_ = new_dse;
    }
  }
}

// Synchronizes recommended policy search engines with template_urls_.
// Removes non-enforced policy engines that no longer match the current
// recommended policy preference, and adds the current recommended policy engine
// if missing (so it remains available in search engine lists when not active).
void TemplateURLService::UpdateRecommendedDefaultSearchProvider() {
  std::unique_ptr<TemplateURLData> rec_data_ptr =
      default_search_manager_.GetRecommendedDefaultSearchEngine();
  bool has_rec_engine = false;

  for (auto it = template_urls_.begin(); it != template_urls_.end();) {
    TemplateURL* turl = it->get();
    if (turl->CreatedByDefaultSearchProviderPolicy() &&
        !turl->enforced_by_policy()) {
      if (rec_data_ptr && TemplateURL::MatchesData(turl, rec_data_ptr.get(),
                                                   search_terms_data())) {
        has_rec_engine = true;
        ++it;
      } else {
        if (default_search_provider_ == turl) {
          default_search_provider_ = nullptr;
        }
        TemplateURLID id = turl->id();
        RemoveFromMaps(turl);
        it = template_urls_.erase(it);
        if (web_data_service_) {
          web_data_service_->RemoveKeyword(id);
        }
      }
    } else {
      ++it;
    }
  }

  if (rec_data_ptr && !has_rec_engine) {
    TemplateURLData rec_data(*rec_data_ptr);
    if (rec_data.sync_guid.empty()) {
      rec_data.GenerateSyncGUID();
    }
    rec_data.policy_origin =
        TemplateURLData::PolicyOrigin::kDefaultSearchProvider;
    rec_data.enforced_by_policy = false;
    rec_data.safe_for_autoreplace = false;
    rec_data.is_active = TemplateURLData::ActiveStatus::kTrue;
    Add(std::make_unique<TemplateURL>(rec_data));
  }
}

void TemplateURLService::ResetTemplateURLGUID(TemplateURL* url,
                                              const std::string& guid) {
  DCHECK(loaded_);
  DCHECK(!guid.empty());

  TemplateURLData data(url->data());
  data.sync_guid = guid;
  UpdateData(url, data);
}

void TemplateURLService::PatchMissingSyncGUIDs(
    OwnedTemplateURLVector* template_urls) {
  DCHECK(template_urls);
  for (auto& template_url : *template_urls) {
    DCHECK(template_url);
    if (template_url->sync_guid().empty() &&
        (template_url->type() == TemplateURL::NORMAL)) {
      template_url->GenerateSyncGUID();
      if (web_data_service_) {
        web_data_service_->UpdateKeyword(template_url->data());
      }
    }
  }
}

void TemplateURLService::OnDefaultSearchProviderGUIDChanged() {
  base::AutoReset<DefaultSearchChangeOrigin> change_origin(
      &dsp_change_origin_, DSP_CHANGE_SYNC_PREF);

  std::string new_guid = prefs_->GetString(prefs::kDefaultSearchProviderGUID);
  if (new_guid.empty()) {
    default_search_manager_.ClearUserSelectedDefaultSearchEngine();
    return;
  }

  const TemplateURL* turl = GetTemplateURLForGUID(new_guid);
  if (turl) {
    default_search_manager_.SetUserSelectedDefaultSearchEngine(turl->data());
  }
}

void TemplateURLService::MaybeSetIsActiveSearchEngines(
    OwnedTemplateURLVector* template_urls) {
  DCHECK(template_urls);
  for (auto& turl : *template_urls) {
    DCHECK(turl);
    // An turl is "active" if it has ever been used or manually added/modified.
    // |safe_for_autoreplace| is false if the entry has been modified.
    if (turl->is_active() == TemplateURLData::ActiveStatus::kUnspecified &&
        (!turl->safe_for_autoreplace() || turl->usage_count() > 0)) {
      turl->set_is_active(TemplateURLData::ActiveStatus::kTrue);
      turl->set_safe_for_autoreplace(false);
      if (web_data_service_) {
        web_data_service_->UpdateKeyword(turl->data());
      }
    }
  }
}

TemplateURL* TemplateURLService::FindPrepopulatedTemplateURL(
    int prepopulated_id) {
  DCHECK(prepopulated_id);
  for (const auto& turl : template_urls_) {
    if (turl->prepopulate_id() == prepopulated_id) {
      return turl.get();
    }
  }
  return nullptr;
}

TemplateURL* TemplateURLService::FindStarterPackTemplateURL(
    template_url_starter_pack_data::StarterPackId starter_pack_id) {
  DCHECK(starter_pack_id !=
         template_url_starter_pack_data::StarterPackId::kNone);
  for (const auto& turl : template_urls_) {
    if (turl->starter_pack_id() == starter_pack_id) {
      return turl.get();
    }
  }
  return nullptr;
}

TemplateURL* TemplateURLService::FindTemplateURLForExtension(
    const std::string& extension_id,
    TemplateURL::Type type) {
  DCHECK_NE(TemplateURL::NORMAL, type);
  for (const auto& turl : template_urls_) {
    if (turl->type() == type && turl->GetExtensionId() == extension_id) {
      return turl.get();
    }
  }
  return nullptr;
}

TemplateURL* TemplateURLService::FindMatchingDefaultExtensionTemplateURL(
    const TemplateURLData& data) {
  for (const auto& turl : template_urls_) {
    if (turl->type() == TemplateURL::NORMAL_CONTROLLED_BY_EXTENSION &&
        turl->GetExtensionInfo()->wants_to_be_default_engine &&
        TemplateURL::MatchesData(turl.get(), &data, search_terms_data())) {
      return turl.get();
    }
  }
  return nullptr;
}

bool TemplateURLService::RemoveDuplicateReplaceableEnginesOf(
    TemplateURL* candidate) {
  DCHECK(candidate);

  // Do not replace existing search engines if `candidate` was created by the
  // policy.
  if (candidate->CreatedByNonDefaultSearchProviderPolicy()) {
    return false;
  }

  const std::u16string& keyword = candidate->keyword();

  // If there's not at least one conflicting TemplateURL, there's nothing to do.
  const auto match_range = keyword_to_turl_.equal_range(keyword);
  if (match_range.first == match_range.second) {
    return false;
  }

  // Gather the replaceable TemplateURLs to be removed. We don't do it in-place,
  // because Remove() invalidates iterators.
  std::vector<TemplateURL*> replaceable_turls;
  for (auto it = match_range.first; it != match_range.second; ++it) {
    TemplateURL* turl = it->second;
    DCHECK_NE(turl, candidate) << "This algorithm runs BEFORE |candidate| is "
                                  "added to the keyword map.";

    // Built-in engines are marked as safe_for_autoreplace(). But because
    // they are shown in the Default Search Engines Settings UI, users would
    // find it confusing if they were ever automatically removed.
    // https://crbug.com/1164024
    if (turl->safe_for_autoreplace() && turl->prepopulate_id() == 0 &&
        turl->starter_pack_id() ==
            template_url_starter_pack_data::StarterPackId::kNone) {
      replaceable_turls.push_back(turl);
    }
  }

  // Find the BEST engine for |keyword| factoring in the new |candidate|.
  TemplateURL* best = GetTemplateURLForKeyword(keyword);
  if (!best || candidate->IsBetterThanConflictingEngine(best)) {
    best = candidate;
  }

  // Remove all the replaceable TemplateURLs that are not the best.
  for (TemplateURL* turl : replaceable_turls) {
    DCHECK_NE(turl, candidate);

    // Never actually remove the DSE during this phase. This handling defers
    // deleting the DSE until it's no longer set as the DSE, analogous to how we
    // handle ACTION_DELETE of the DSE in ProcessSyncChanges().
    if (turl != best && !MatchesDefaultSearchProvider(turl)) {
      Remove(turl);
    }
  }

  // Caller needs to know if |candidate| would have been deleted.
  // Also always successfully add prepopulated engines, for two reasons:
  //  1. The DSE repair logic in ApplyDefaultSearchChangeNoMetrics() relies on
  //     Add()ing back the DSE always succeeding. https://crbug.com/1164024
  //  2. If we don't do this, we have a weird order-dependence on the
  //     replaceability of prepopulated engines, given that we refuse to add
  //     prepopulated engines to the |replaceable_engines| vector.
  //
  // Given the necessary special casing of prepopulated engines, we may consider
  // marking prepopulated engines as NOT safe_for_autoreplace(), but there's a
  // few obstacles to this:
  //  1. Prepopulated engines are not user-created, and therefore meet the
  //     definition of safe_for_autoreplace().
  //  2. If we mark them as NOT safe_for_autoreplace(), we can no longer
  //     distinguish between prepopulated engines that user has edited, vs. not
  //     edited.
  //
  // One more caveat: In 2019, we made prepopulated engines have a
  // deterministically generated Sync GUID in order to prevent duplicate
  // prepopulated engines when two clients start syncing at the same time.
  // When combined with the requirement that we can never fail to add a
  // prepopulated engine, this could leads to two engines having the same GUID.
  //
  // TODO(tommycli): After M89, we need to investigate solving the contradiction
  // above. Most probably: the solution is to stop Syncing prepopulated engines
  // and make the GUIDs actually globally unique again.
  return candidate != best && candidate->safe_for_autoreplace() &&
         candidate->prepopulate_id() == 0 &&
         candidate->starter_pack_id() ==
             template_url_starter_pack_data::StarterPackId::kNone;
}

bool TemplateURLService::MatchesDefaultSearchProvider(TemplateURL* turl) const {
  DCHECK(turl);
  const TemplateURL* default_provider = GetDefaultSearchProvider();
  if (!default_provider) {
    return false;
  }

  return turl->sync_guid() == default_provider->sync_guid();
}

std::unique_ptr<EnterpriseSearchManager>
TemplateURLService::GetEnterpriseSearchManager(PrefService* prefs) {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
  return std::make_unique<EnterpriseSearchManager>(
      prefs, base::BindRepeating(&TemplateURLService::EnterpriseSearchChanged,
                                 base::Unretained(this)));
#else
  return nullptr;
#endif
}

