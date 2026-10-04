// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/importer/profile_writer.h"

#include <stddef.h>

#include <algorithm>
#include <map>
#include <set>
#include <string>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/password_manager/factories/profile_password_store_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/webdata_services/web_data_service_factory.h"
#include "chrome/common/pref_names.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/favicon/core/favicon_service.h"
#include "components/history/core/browser/history_service.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_store/password_form_converters.h"
#include "components/password_manager/core/browser/password_store/password_store_interface.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "components/user_data_importer/common/imported_bookmark_entry.h"

ProfileWriter::ProfileWriter(Profile* profile) : profile_(profile) {}

bool ProfileWriter::TemplateURLServiceIsLoaded() const {
  return TemplateURLServiceFactory::GetForProfile(profile_)->loaded();
}

void ProfileWriter::AddPasswordForm(
    const password_manager::PasswordForm& form) {
  DCHECK(profile_);

  if (profile_->GetPrefs()->GetBoolean(
          password_manager::prefs::kCredentialsEnableService)) {
    ProfilePasswordStoreFactory::GetForProfile(
        profile_, ServiceAccessType::EXPLICIT_ACCESS)
        ->AddLogin(password_manager::FromPasswordForm(form));
  }
}

void ProfileWriter::AddHistoryPage(const history::URLRows& page,
                                   history::VisitSource visit_source) {
  if (!page.empty()) {
    HistoryServiceFactory::GetForProfile(profile_,
                                         ServiceAccessType::EXPLICIT_ACCESS)
        ->AddPagesWithDetails(page, visit_source);
  }
}

void ProfileWriter::AddHomepage(const GURL& home_page) {
  DCHECK(profile_);

  PrefService* prefs = profile_->GetPrefs();
  // NOTE: We set the kHomePage value, but keep the NewTab page as the homepage.
  const PrefService::Preference* pref = prefs->FindPreference(prefs::kHomePage);
  if (pref && !pref->IsManaged()) {
    prefs->SetString(prefs::kHomePage, home_page.spec());
  }
}

void ProfileWriter::AddBookmarks(
    const std::vector<user_data_importer::ImportedBookmarkEntry>& bookmarks,
    const std::u16string& top_level_folder_name) {
  // Bookmarks have been removed; imported bookmarks are dropped.
}

void ProfileWriter::AddFavicons(
    const favicon_base::FaviconUsageDataList& favicons) {
  FaviconServiceFactory::GetForProfile(profile_,
                                       ServiceAccessType::EXPLICIT_ACCESS)
      ->SetImportedFavicons(favicons);
}

typedef std::map<std::string, TemplateURL*> HostPathMap;

// Returns the key for the map built by BuildHostPathMap. If url_string is not
// a valid URL, an empty string is returned, otherwise host+path is returned.
static std::string HostPathKeyForURL(const GURL& url) {
  return url.is_valid() ? url.GetHost() + url.GetPath() : std::string();
}

// Builds the key to use in HostPathMap for the specified TemplateURL. Returns
// an empty string if a host+path can't be generated for the TemplateURL.
// If an empty string is returned, the TemplateURL should not be added to
// HostPathMap.
//
// If |try_url_if_invalid| is true, and |t_url| isn't valid, a string is built
// from the raw TemplateURL string. Use a value of true for |try_url_if_invalid|
// when checking imported URLs as the imported URL may not be valid yet may
// match the host+path of one of the default URLs. This is used to catch the
// case of IE using an invalid OSDD URL for Live Search, yet the host+path
// matches our prepopulate data. IE's URL for Live Search is something like
// 'http://...{Language}...'. As {Language} is not a valid OSDD parameter value
// the TemplateURL is invalid.
static std::string BuildHostPathKey(const TemplateURL* t_url,
                                    const SearchTermsData& search_terms_data,
                                    bool try_url_if_invalid) {
  if (try_url_if_invalid && !t_url->url_ref().IsValid(search_terms_data))
    return HostPathKeyForURL(GURL(t_url->url()));

  if (t_url->url_ref().SupportsReplacement(search_terms_data)) {
    return HostPathKeyForURL(GURL(t_url->url_ref().ReplaceSearchTerms(
        TemplateURLRef::SearchTermsArgs(u"x"), search_terms_data)));
  }
  return std::string();
}

// Builds a set that contains an entry of the host+path for each TemplateURL in
// the TemplateURLService that has a valid search url.
static void BuildHostPathMap(TemplateURLService* model,
                             HostPathMap* host_path_map) {
  TemplateURLService::TemplateURLVector template_urls =
      model->GetTemplateURLs();
  for (size_t i = 0; i < template_urls.size(); ++i) {
    const std::string host_path = BuildHostPathKey(
        template_urls[i], model->search_terms_data(), false);
    if (!host_path.empty()) {
      const TemplateURL* existing_turl = (*host_path_map)[host_path];
      TemplateURL* t_url = template_urls[i];
      if (!existing_turl || (model->ShowInDefaultList(t_url) &&
                             !model->ShowInDefaultList(existing_turl))) {
        // If there are multiple TemplateURLs with the same host+path, favor
        // those shown in the default list.  If there are multiple potential
        // defaults, favor the first one, which should be the more commonly used
        // one.
        (*host_path_map)[host_path] = t_url;
      }
    }  // else case, TemplateURL doesn't have a search url, doesn't support
       // replacement, or doesn't have valid GURL. Ignore it.
  }
}

void ProfileWriter::AddKeywords(
    TemplateURLService::OwnedTemplateURLVector template_urls,
    bool unique_on_host_and_path) {
  TemplateURLService* model =
      TemplateURLServiceFactory::GetForProfile(profile_);
  HostPathMap host_path_map;
  if (unique_on_host_and_path)
    BuildHostPathMap(model, &host_path_map);

  for (auto& turl : template_urls) {
    // TemplateURLService requires keywords to be unique. If there is already a
    // TemplateURL with this keyword, don't import it again.
    if (model->GetTemplateURLForKeyword(turl->keyword()) != nullptr)
      continue;

    // The omnibox doesn't properly handle search keywords with whitespace,
    // so skip importing them.
    if (turl->keyword().find_first_of(base::kWhitespaceUTF16) !=
        std::u16string::npos)
      continue;

    // For search engines if there is already a keyword with the same
    // host+path, we don't import it. This is done to avoid both duplicate
    // search providers (such as two Googles, or two Yahoos) as well as making
    // sure the search engines we provide aren't replaced by those from the
    // imported browser.
    if (unique_on_host_and_path && (host_path_map.find(BuildHostPathKey(
                                        turl.get(), model->search_terms_data(),
                                        true)) != host_path_map.end()))
      continue;

    // Only add valid TemplateURLs to the model.
    if (turl->url_ref().IsValid(model->search_terms_data()))
      model->Add(std::move(turl));
  }
}

void ProfileWriter::AddAutocompleteFormDataEntries(
    const std::vector<autofill::AutocompleteEntry>& autocomplete_entries) {
  scoped_refptr<autofill::AutofillWebDataService> web_data_service =
      WebDataServiceFactory::GetAutofillWebDataForProfile(
          profile_, ServiceAccessType::EXPLICIT_ACCESS);
  if (web_data_service.get())
    web_data_service->UpdateAutocompleteEntries(autocomplete_entries);
}

ProfileWriter::~ProfileWriter() = default;
