// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data/core/browsing_data_utils.h"

#include <optional>
#include <string>
#include <vector>

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/browsing_data/core/counters/history_counter.h"
#include "components/browsing_data/core/features.h"
#include "components/browsing_data/core/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace {
// Constructs the text to be displayed by the history counter from the given
// `history_result`. The string is based on the unique domains within the
// deletion range.
std::u16string CreateHistoryCounterString(
    const browsing_data::HistoryCounter::HistoryResult* history_result) {
  CHECK(history_result->source()->GetPrefName() ==
            browsing_data::prefs::kDeleteBrowsingHistory);

  if (!history_result->Finished()) {
    // The counter is still counting.
    return l10n_util::GetStringUTF16(IDS_CLEAR_BROWSING_DATA_CALCULATING);
  }

  browsing_data::BrowsingDataCounter::ResultInt unique_domains_count =
      history_result->Value();

  if (unique_domains_count == 0) {
    return l10n_util::GetStringUTF16(IDS_DEL_NO_BROWSING_HISTORY_TEXT);
  }
  std::u16string last_visited_domain =
      base::UTF8ToUTF16(history_result->last_visited_domain());
  CHECK(!last_visited_domain.empty());

  unique_domains_count--;
  if (unique_domains_count > 0) {
    std::u16string domain_count_string = l10n_util::GetPluralStringFUTF16(
        IDS_DEL_BROWSING_HISTORY_DOMAIN_COUNT_TEXT, unique_domains_count);
    return l10n_util::GetStringFUTF16(
        IDS_DEL_BROWSING_HISTORY_COUNTER_MULTIPLE_DOMAINS_TEXT,
        last_visited_domain, domain_count_string);
  }

  return l10n_util::GetStringFUTF16(
      IDS_DEL_BROWSING_HISTORY_COUNTER_SINGLE_DOMAIN_TEXT, last_visited_domain);
}
}  // namespace

namespace browsing_data {

const char kDeleteBrowsingDataDialogHistogram[] =
    "Privacy.DeleteBrowsingData.Dialog";

base::Time CalculateBeginDeleteTime(TimePeriod time_period) {
  base::TimeDelta diff;
  base::Time delete_begin_time = base::Time::Now();
  switch (time_period) {
    case TimePeriod::LAST_15_MINUTES:
      diff = base::Minutes(15);
      break;
    case TimePeriod::LAST_HOUR:
      diff = base::Hours(1);
      break;
    case TimePeriod::LAST_DAY:
      diff = base::Hours(24);
      break;
    case TimePeriod::LAST_WEEK:
      diff = base::Hours(7 * 24);
      break;
    case TimePeriod::FOUR_WEEKS:
      diff = base::Hours(4 * 7 * 24);
      break;
    case TimePeriod::ALL_TIME:
    case TimePeriod::OLDER_THAN_30_DAYS:
      delete_begin_time = base::Time();
      break;
  }
  return delete_begin_time - diff;
}

base::Time CalculateEndDeleteTime(TimePeriod time_period) {
  if (time_period == TimePeriod::OLDER_THAN_30_DAYS) {
    return base::Time::Now() - base::Days(30);
  }
  return base::Time::Max();
}

void RecordDeletionForPeriod(TimePeriod period) {
  switch (period) {
    case TimePeriod::LAST_15_MINUTES:
      base::RecordAction(
          base::UserMetricsAction("ClearBrowsingData_Last15Minutes"));
      break;
    case TimePeriod::LAST_HOUR:
      base::RecordAction(base::UserMetricsAction("ClearBrowsingData_LastHour"));
      break;
    case TimePeriod::LAST_DAY:
      base::RecordAction(base::UserMetricsAction("ClearBrowsingData_LastDay"));
      break;
    case TimePeriod::LAST_WEEK:
      base::RecordAction(base::UserMetricsAction("ClearBrowsingData_LastWeek"));
      break;
    case TimePeriod::FOUR_WEEKS:
      base::RecordAction(
          base::UserMetricsAction("ClearBrowsingData_LastMonth"));
      break;
    case TimePeriod::ALL_TIME:
      base::RecordAction(
          base::UserMetricsAction("ClearBrowsingData_Everything"));
      break;
    case TimePeriod::OLDER_THAN_30_DAYS:
      base::RecordAction(
          base::UserMetricsAction("ClearBrowsingData_OlderThan30Days"));
      break;
  }
}

void RecordTimePeriodChange(TimePeriod period) {
  switch (period) {
    case TimePeriod::LAST_15_MINUTES:
      base::RecordAction(base::UserMetricsAction(
          "ClearBrowsingData_TimePeriodChanged_Last15Minutes"));
      break;
    case TimePeriod::LAST_HOUR:
      base::RecordAction(base::UserMetricsAction(
          "ClearBrowsingData_TimePeriodChanged_LastHour"));
      break;
    case TimePeriod::LAST_DAY:
      base::RecordAction(base::UserMetricsAction(
          "ClearBrowsingData_TimePeriodChanged_LastDay"));
      break;
    case TimePeriod::LAST_WEEK:
      base::RecordAction(base::UserMetricsAction(
          "ClearBrowsingData_TimePeriodChanged_LastWeek"));
      break;
    case TimePeriod::FOUR_WEEKS:
      base::RecordAction(base::UserMetricsAction(
          "ClearBrowsingData_TimePeriodChanged_LastMonth"));
      break;
    case TimePeriod::ALL_TIME:
      base::RecordAction(base::UserMetricsAction(
          "ClearBrowsingData_TimePeriodChanged_Everything"));
      break;
    case TimePeriod::OLDER_THAN_30_DAYS:
      base::RecordAction(base::UserMetricsAction(
          "ClearBrowsingData_TimePeriodChanged_OlderThan30Days"));
      break;
  }
}

void RecordDeleteBrowsingDataAction(DeleteBrowsingDataAction cbd_action) {
  UMA_HISTOGRAM_ENUMERATION("Privacy.DeleteBrowsingData.Action", cbd_action);
}

std::u16string GetCounterTextFromResult(
    const BrowsingDataCounter::Result* result) {
  std::string pref_name = result->source()->GetPrefName();

  if (!result->Finished()) {
    // The counter is still counting.
    return l10n_util::GetStringUTF16(IDS_CLEAR_BROWSING_DATA_CALCULATING);
  }

  if (pref_name == prefs::kDeleteDownloadHistory) {
    BrowsingDataCounter::ResultInt count =
        static_cast<const BrowsingDataCounter::FinishedResult*>(result)
            ->Value();
    return l10n_util::GetPluralStringFUTF16(IDS_DEL_DOWNLOADS_COUNTER, count);
  }

  if (pref_name == prefs::kDeleteSiteSettings) {
    BrowsingDataCounter::ResultInt count =
        static_cast<const BrowsingDataCounter::FinishedResult*>(result)
            ->Value();
    return l10n_util::GetPluralStringFUTF16(IDS_DEL_SITE_SETTINGS_COUNTER,
                                            count);
  }

  if (pref_name == prefs::kDeleteBrowsingHistory) {
    // History counter.
    return CreateHistoryCounterString(
          static_cast<const HistoryCounter::HistoryResult*>(result));
  }

  NOTREACHED();
}

const char* GetTimePeriodPreferenceName() {
  return prefs::kDeleteTimePeriod;
}

bool GetDeletionPreferenceFromDataType(
    BrowsingDataType data_type,
    std::string* out_pref) {
  switch (data_type) {
    case BrowsingDataType::HISTORY:
      *out_pref = prefs::kDeleteBrowsingHistory;
      return true;
    case BrowsingDataType::CACHE:
      *out_pref = prefs::kDeleteCache;
      return true;
    case BrowsingDataType::SITE_DATA:
      *out_pref = prefs::kDeleteCookies;
      return true;
    case BrowsingDataType::PASSWORDS:
    case BrowsingDataType::FORM_DATA:
      return false;
    case BrowsingDataType::SITE_SETTINGS:
      *out_pref = prefs::kDeleteSiteSettings;
      return true;
    case BrowsingDataType::DOWNLOADS:
      *out_pref = prefs::kDeleteDownloadHistory;
      return true;
    case BrowsingDataType::HOSTED_APPS_DATA:
      *out_pref = prefs::kDeleteHostedAppsData;
      return true;
    case BrowsingDataType::TABS:
      *out_pref = prefs::kCloseTabs;
      return true;
  }
  NOTREACHED();
}

std::optional<BrowsingDataType> GetDataTypeFromDeletionPreference(
    const std::string& pref_name) {
  using DataTypeMap = base::flat_map<std::string, BrowsingDataType>;
  static base::NoDestructor<DataTypeMap> preference_to_datatype(
      std::initializer_list<DataTypeMap::value_type>{
          {prefs::kDeleteBrowsingHistory, BrowsingDataType::HISTORY},
          {prefs::kDeleteCache, BrowsingDataType::CACHE},
          {prefs::kDeleteCookies, BrowsingDataType::SITE_DATA},
          {prefs::kDeleteSiteSettings, BrowsingDataType::SITE_SETTINGS},
          {prefs::kDeleteDownloadHistory, BrowsingDataType::DOWNLOADS},
          {prefs::kDeleteHostedAppsData, BrowsingDataType::HOSTED_APPS_DATA},
      });

  auto iter = preference_to_datatype->find(pref_name);
  if (iter != preference_to_datatype->end()) {
    return iter->second;
  }
  return std::nullopt;
}

bool IsHttpsCookieSourceScheme(net::CookieSourceScheme cookie_source_scheme) {
  switch (cookie_source_scheme) {
    case net::CookieSourceScheme::kSecure:
      return true;
    case net::CookieSourceScheme::kNonSecure:
      return false;
    case net::CookieSourceScheme::kUnset:
      // Older cookies don't have a source scheme. Associate them with https
      // since the majority of pageloads are https.
      return true;
  }
}

}  // namespace browsing_data
