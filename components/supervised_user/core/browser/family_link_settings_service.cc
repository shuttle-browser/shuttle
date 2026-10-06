// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/family_link_settings_service.h"

#include <stddef.h>

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/escape.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/prefs/json_pref_store.h"
#include "components/prefs/pref_filter.h"
#include "components/supervised_user/core/browser/supervised_user_url_filtering_service.h"
#include "components/supervised_user/core/browser/supervised_user_utils.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"
#include "components/url_formatter/url_formatter.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"
#include "url/gurl.h"

namespace supervised_user {

namespace {

using base::JSONReader;
using base::UserMetricsAction;

const char kAtomicSettings[] = "atomic_settings";
const char kSupervisedUserInternalItemPrefix[] = "X-";
const char kQueuedItems[] = "queued_items";
const char kSplitSettingKeySeparator = ':';
const char kSplitSettings[] = "split_settings";

bool SettingShouldApplyToPrefs(const std::string& name) {
  return !base::StartsWith(name, kSupervisedUserInternalItemPrefix,
                           base::CompareCase::INSENSITIVE_ASCII);
}

}  // namespace

FamilyLinkSettingsService::FamilyLinkSettingsService()
    : active_(false), initialization_failed_(false) {}

FamilyLinkSettingsService::~FamilyLinkSettingsService() = default;

void FamilyLinkSettingsService::Init(
    base::FilePath profile_path,
    scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner,
    bool load_synchronously) {
  base::FilePath path = profile_path.Append(kSupervisedUserSettingsFilename);
  PersistentPrefStore* store = new JsonPrefStore(
      path, std::unique_ptr<PrefFilter>(), std::move(sequenced_task_runner));
  Init(store);
  if (load_synchronously) {
    store_->ReadPrefs();
    DCHECK(IsReady());
  } else {
    store_->ReadPrefsAsync(nullptr);
  }
}

void FamilyLinkSettingsService::Init(scoped_refptr<PersistentPrefStore> store) {
  DCHECK(!store_.get());
  store_ = store;
  store_->AddObserver(this);
}

base::CallbackListSubscription
FamilyLinkSettingsService::SubscribeForSettingsChange(
    const SettingsCallback& callback) {
  if (IsReady()) {
    base::DictValue settings = GetSettingsWithDefault();
    callback.Run(std::move(settings));
  }

  return settings_callback_list_.Add(callback);
}

base::CallbackListSubscription
FamilyLinkSettingsService::SubscribeForNewWebsiteApproval(
    const WebsiteApprovalCallback& callback) {
  return website_approval_callback_list_.Add(callback);
}

void FamilyLinkSettingsService::RecordLocalWebsiteApproval(
    const std::string& host) {
  // Write the sync setting.
  std::string setting_key =
      MakeSplitSettingKey(kContentPackManualBehaviorHosts, host);
  SaveItem(setting_key, base::Value(true));

  // Now notify subscribers of the updates.
  website_approval_callback_list_.Notify(setting_key);
}

base::CallbackListSubscription FamilyLinkSettingsService::SubscribeForShutdown(
    const ShutdownCallback& callback) {
  return shutdown_callback_list_.Add(callback);
}

bool FamilyLinkSettingsService::IsCustomPassphraseAllowed() const {
  return !active_;
}

void FamilyLinkSettingsService::SetActive(bool active) {
  active_ = active;

  if (active_) {
    // Child account supervised users must be signed in.
    SetLocalSetting(kSigninAllowed, base::Value(true));
    SetLocalSetting(kSigninAllowedOnNextStartup, base::Value(true));

    // Always allow cookies, to avoid website compatibility issues.
    SetLocalSetting(kCookiesAlwaysAllowed, base::Value(true));

    // SafeSearch and GeolocationDisabled are controlled at the account level,
    // so don't override them client-side.
  } else {
    RemoveLocalSetting(kSigninAllowed);
    RemoveLocalSetting(kCookiesAlwaysAllowed);
  }

  InformSubscribers();
}

bool FamilyLinkSettingsService::IsReady() const {
  // Initialization cannot be complete but have failed at the same time.
  DCHECK(!(store_->IsInitializationComplete() && initialization_failed_));
  return initialization_failed_ || store_->IsInitializationComplete();
}

void FamilyLinkSettingsService::Clear() {
  store_->RemoveValue(kAtomicSettings,
                      WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
  store_->RemoveValue(kSplitSettings,
                      WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
}

// static
std::string FamilyLinkSettingsService::MakeSplitSettingKey(
    const std::string& prefix,
    const std::string& key) {
  return prefix + kSplitSettingKeySeparator + key;
}

void FamilyLinkSettingsService::SaveItem(const std::string& key,
                                         base::Value value) {
  // Sync is gone: items are stored in the queued items dictionary.
  std::string key_suffix = key;
  base::RecordAction(UserMetricsAction("ManagedUsers_UploadItem_Queued"));
  base::DictValue* dict = GetQueuedItems();
  dict->Set(key_suffix, std::move(value));

  // Now notify subscribers of the updates; notify both settings keys.
  store_->ReportValueChanged(kAtomicSettings,
                             WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
  store_->ReportValueChanged(kSplitSettings,
                             WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
  InformSubscribers();
}

void FamilyLinkSettingsService::SetLocalSetting(std::string_view key,
                                                base::Value value) {
  local_settings_.Set(key, std::move(value));
  InformSubscribers();
}

void FamilyLinkSettingsService::SetLocalSetting(std::string_view key,
                                                base::DictValue dict) {
  local_settings_.Set(key, std::move(dict));
  InformSubscribers();
}

void FamilyLinkSettingsService::RemoveLocalSetting(std::string_view key) {
  local_settings_.Remove(key);
  InformSubscribers();
}

void FamilyLinkSettingsService::Shutdown() {
  // Allow calling `Shutdown()` even if `Init(...)` was never
  // invoked on the service.
  if (store_) {
    store_->RemoveObserver(this);
  }
  shutdown_callback_list_.Notify();
}

base::WeakPtr<const FamilyLinkSettingsService>
FamilyLinkSettingsService::GetWeakPtr() const {
  return weak_ptr_factory_.GetWeakPtr();
}

void FamilyLinkSettingsService::OnInitializationCompleted(bool success) {
  if (!success) {
    // If this happens, it means the profile directory was not found. There is
    // not much we can do, but the whole profile will probably be useless
    // anyway. Just mark initialization as failed and continue otherwise,
    // because subscribers might still expect to be called back.
    initialization_failed_ = true;
  }

  DCHECK(IsReady());
  InformSubscribers();
}

const base::DictValue& FamilyLinkSettingsService::LocalSettingsForTest() const {
  return local_settings_;
}

base::DictValue* FamilyLinkSettingsService::GetDictionaryAndSplitKey(
    std::string* key) const {
  size_t pos = key->find_first_of(kSplitSettingKeySeparator);
  if (pos == std::string::npos) {
    return GetAtomicSettings();
  }

  base::DictValue* split_settings = GetSplitSettings();
  std::string prefix = key->substr(0, pos);
  base::DictValue* dict = split_settings->EnsureDict(prefix);
  key->erase(0, pos + 1);
  return dict;
}

base::DictValue* FamilyLinkSettingsService::GetOrCreateDictionary(
    std::string_view key) const {
  base::Value* value = nullptr;
  if (!store_->GetMutableValue(key, &value)) {
    store_->SetValue(key, base::Value(base::DictValue()),
                     WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
    store_->GetMutableValue(key, &value);
  }
  DCHECK(value->is_dict());
  return &value->GetDict();
}

base::DictValue* FamilyLinkSettingsService::GetAtomicSettings() const {
  return GetOrCreateDictionary(kAtomicSettings);
}

base::DictValue* FamilyLinkSettingsService::GetSplitSettings() const {
  return GetOrCreateDictionary(kSplitSettings);
}

base::DictValue* FamilyLinkSettingsService::GetQueuedItems() const {
  return GetOrCreateDictionary(kQueuedItems);
}

base::DictValue FamilyLinkSettingsService::GetSettingsWithDefault() const {
  DCHECK(IsReady());
  if (!active_ || initialization_failed_) {
    return base::DictValue();
  }

  base::DictValue settings(local_settings_.Clone());

  base::DictValue* atomic_settings = GetAtomicSettings();
  for (const auto it : *atomic_settings) {
    if (!SettingShouldApplyToPrefs(it.first)) {
      continue;
    }

    settings.Set(it.first, it.second.Clone());
  }

  base::DictValue* split_settings = GetSplitSettings();
  for (const auto it : *split_settings) {
    if (!SettingShouldApplyToPrefs(it.first)) {
      continue;
    }

    settings.Set(it.first, it.second.Clone());
  }

  return settings;
}

void FamilyLinkSettingsService::InformSubscribers() {
  SCOPED_CRASH_KEY_STRING32("SupervisedUser", "RaceInFLSSShutdown",
                            "InInformSubscribers");

  if (!IsReady()) {
    return;
  }

  base::DictValue settings = GetSettingsWithDefault();

  // This check prevents re-emitting the same settings, including empty
  // settings. Main scenario is when this service is inactive but receives new
  // settings from its backends. Then GetSettingsWithDefault() short-circuits
  // and always returns empty settings. Re-emitting empty settings could clear
  // the supervised user prefs store that can contain settings from device
  // parental controls. Note: new subscribers always receive the current
  // settings at least once.
  if (last_notified_settings_.has_value() &&
      settings == *last_notified_settings_) {
    return;
  }

  last_notified_settings_ = settings.Clone();
  settings_callback_list_.Notify(std::move(settings));
}

WebFilterType FamilyLinkSettingsService::GetWebFilterType() const {
  // When the service is inactive or failed to initialize, we consider the web
  // filter to be disabled to not interfere with regular browsing experience.
  // Otherwise, we try to infer the filter type from the settings: block
  // always takes precedence regardless the safe sites setting. Then, safe
  // sites value determines the remaining allow or try-to-block verdict.

  // LINT.IfChange(GetWebFilterType)
  if (!active_ || initialization_failed_) {
    return WebFilterType::kDisabled;
  }

  base::DictValue settings = GetSettingsWithDefault();
  if (GetDefaultFilteringBehavior(settings) == FilteringBehavior::kBlock) {
    return WebFilterType::kCertainSites;
  }
  return IsSafeSitesEnabled(settings) ? WebFilterType::kTryToBlockMatureSites
                                      : WebFilterType::kAllowAllSites;
  // LINT.ThenChange(//components/supervised_user/core/browser/supervised_user_url_filter.cc:GetWebFilterType)
}

FilteringBehavior FamilyLinkSettingsService::GetDefaultFilteringBehavior()
    const {
  return GetDefaultFilteringBehavior(GetSettingsWithDefault());
}

FilteringBehavior FamilyLinkSettingsService::GetDefaultFilteringBehavior(
    const base::DictValue& settings) const {
  // The default value for the default filtering behavior is "allow",
  // including malformed data from remote.
  int value = settings.FindInt(kContentPackDefaultFilteringBehavior)
                  .value_or(static_cast<int>(FilteringBehavior::kAllow));

  // Only explicit match renders as `FilteringBehavior::kBlock`.
  if (value == static_cast<int>(FilteringBehavior::kBlock)) {
    return FilteringBehavior::kBlock;
  }
  // All other values are equivalent to `FilteringBehavior::kAllow` (do not
  // CHECK value, it comes from external system; default instead).
  return FilteringBehavior::kAllow;
}

bool FamilyLinkSettingsService::IsSafeSitesEnabled() const {
  return IsSafeSitesEnabled(GetSettingsWithDefault());
}

bool FamilyLinkSettingsService::IsSafeSitesEnabled(
    const base::DictValue& settings) const {
  // In Family Link, safe sites setting defaults to true.
  return settings.FindBool(kSafeSitesEnabled).value_or(true);
}

FamilyLinkSettingsService::HostExceptions
FamilyLinkSettingsService::GetHostExceptions() const {
  const base::DictValue defaults = GetSettingsWithDefault();
  const base::DictValue* manual_behavior_hosts =
      defaults.FindDict(kContentPackManualBehaviorHosts);
  if (!manual_behavior_hosts) {
    return {};
  }

  HostExceptions host_exceptions;
  for (const auto&& [host, value] : *manual_behavior_hosts) {
    if (value.GetIfBool().value_or(false)) {
      host_exceptions.allowed_hosts.insert(host);
    } else {
      host_exceptions.blocked_hosts.insert(host);
    }
  }

  return host_exceptions;
}

FamilyLinkSettingsService::UrlExceptions
FamilyLinkSettingsService::GetUrlExceptions() const {
  const base::DictValue defaults = GetSettingsWithDefault();
  const base::DictValue* manual_behavior_urls =
      defaults.FindDict(kContentPackManualBehaviorURLs);
  if (!manual_behavior_urls) {
    return {};
  }

  UrlExceptions url_exceptions;
  for (const auto&& [url, value] : *manual_behavior_urls) {
    url_exceptions.emplace(GURL(url), value.GetIfBool().value_or(false));
  }
  return url_exceptions;
}

FamilyLinkSettingsService::HostExceptions::HostExceptions() = default;
FamilyLinkSettingsService::HostExceptions::~HostExceptions() = default;
FamilyLinkSettingsService::HostExceptions::HostExceptions(
    const HostExceptions& other) = default;
FamilyLinkSettingsService::HostExceptions&
FamilyLinkSettingsService::HostExceptions::operator=(
    const HostExceptions& other) = default;

bool FamilyLinkSettingsService::IsHostManuallyBlocked(
    std::string_view host) const {
  base::DictValue defaults = GetSettingsWithDefault();
  const base::DictValue* manual_behavior_hosts =
      defaults.FindDict(kContentPackManualBehaviorHosts);
  if (!manual_behavior_hosts) {
    return false;
  }

  std::optional<bool> is_allowed = manual_behavior_hosts->FindBool(host);
  return is_allowed.has_value() && !*is_allowed;
}

GURL FamilyLinkSettingsService::GetUnnormalizedEffectiveUrlToUnblock(
    const WebFilteringResult& result) const {
  // If the URL is blocked because of an exact match in the manual blocklist,
  // then the URL should be unblocked by itself to remove the blocklist entry
  // too.
  if (result.IsFromManualList() && IsHostManuallyBlocked(result.url.host())) {
    return result.url;
  }

  // Otherwise, prepare a canonical version of the URL to unblock.
  return GURL(url_formatter::FormatUrl(
      result.url, url_formatter::kFormatUrlOmitTrivialSubdomains,
      base::UnescapeRule::SPACES, /*new_parsed=*/nullptr,
      /*prefix_end=*/nullptr, /*offset_for_adjustment=*/nullptr));
}

GURL FamilyLinkSettingsService::GetEffectiveUrlToUnblock(
    const WebFilteringResult& result) const {
#if !BUILDFLAG(IS_CHROMEOS)
  return NormalizeUrl(GetUnnormalizedEffectiveUrlToUnblock(result));
#else
  return GetUnnormalizedEffectiveUrlToUnblock(result);
#endif  // !BUILDFLAG(IS_CHROMEOS)
}

}  // namespace supervised_user
