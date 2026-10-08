// Copyright 2026 The Agent Chrome Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/agent_chrome/profile_registry.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/json/values_util.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chrome/browser/agent_chrome/pref_names.h"
#include "chrome/browser/profiles/profile.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/browser/storage_partition.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"

namespace agent_chrome {

namespace {

constexpr char kDomainPrefix[] = "ac-";
constexpr size_t kMaxIdLength = 64;

std::string DomainFor(std::string_view id) {
  return base::StrCat({kDomainPrefix, id});
}

void FlushPartition(content::StoragePartition* partition) {
  partition->GetCookieManagerForBrowserProcess()->FlushCookieStore(
      base::DoNothing());
}

}  // namespace

ProfileRegistry::ProfileRegistry(Profile* profile) : profile_(profile) {
  // Profiles deleted while their partition was loaded: their directories can
  // be removed now, before any agent tab loads them again.
  const base::ListValue& pending =
      profile_->GetPrefs()->GetList(prefs::kPendingDeletes);
  std::vector<std::string> ids;
  for (const base::Value& value : pending) {
    if (value.is_string() && !Exists(value.GetString())) {
      ids.push_back(value.GetString());
    }
  }
  for (const std::string& id : ids) {
    Obliterate(id, base::DoNothing());
  }
}

ProfileRegistry::~ProfileRegistry() = default;

// static
void ProfileRegistry::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(prefs::kProfiles);
  registry->RegisterListPref(prefs::kPendingDeletes);
}

// static
bool ProfileRegistry::IsValidId(std::string_view id) {
  if (id.empty() || id.size() > kMaxIdLength || id.front() == '-' ||
      id.back() == '-') {
    return false;
  }
  char previous = 0;
  for (char c : id) {
    bool ok = base::IsAsciiLower(c) || base::IsAsciiDigit(c) || c == '-';
    if (!ok || (c == '-' && previous == '-')) {
      return false;
    }
    previous = c;
  }
  return true;
}

// static
std::string ProfileRegistry::IdForConfig(
    const content::StoragePartitionConfig& config) {
  const std::string& domain = config.partition_domain();
  if (config.is_default() || config.in_memory() ||
      !config.partition_name().empty() ||
      !base::StartsWith(domain, kDomainPrefix)) {
    return std::string();
  }
  std::string id = domain.substr(sizeof(kDomainPrefix) - 1);
  return IsValidId(id) ? id : std::string();
}

bool ProfileRegistry::Exists(const std::string& id) const {
  return profile_->GetPrefs()->GetDict(prefs::kProfiles).contains(id);
}

std::vector<ProfileRegistry::Entry> ProfileRegistry::List() const {
  std::vector<Entry> entries;
  for (auto [id, value] : profile_->GetPrefs()->GetDict(prefs::kProfiles)) {
    Entry entry{id, base::Time()};
    if (const base::DictValue* dict = value.GetIfDict()) {
      if (const base::Value* created = dict->Find("created")) {
        entry.created = base::ValueToTime(*created).value_or(base::Time());
      }
    }
    entries.push_back(std::move(entry));
  }
  return entries;
}

bool ProfileRegistry::Create(const std::string& id) {
  DCHECK(IsValidId(id));
  if (Exists(id)) {
    return false;
  }
  ScopedDictPrefUpdate update(profile_->GetPrefs(), prefs::kProfiles);
  update->Set(id,
              base::DictValue().Set("created", base::TimeToValue(
                                                   base::Time::Now())));
  // A re-created profile must not be wiped at the next launch.
  SetPendingDelete(id, false);
  gc_required_.erase(id);
  return true;
}

content::StoragePartitionConfig ProfileRegistry::PartitionConfig(
    const std::string& id) const {
  return content::StoragePartitionConfig::Create(profile_, DomainFor(id),
                                                 /*partition_name=*/"",
                                                 /*in_memory=*/false);
}

void ProfileRegistry::Delete(const std::string& id, base::OnceClosure done) {
  {
    ScopedDictPrefUpdate update(profile_->GetPrefs(), prefs::kProfiles);
    update->Remove(id);
  }
  SetPendingDelete(id, true);
  Obliterate(id, std::move(done));
}

void ProfileRegistry::Obliterate(const std::string& id,
                                 base::OnceClosure done) {
  profile_->AsyncObliterateStoragePartition(
      DomainFor(id),
      base::BindOnce(
          [](base::WeakPtr<ProfileRegistry> self, std::string id) {
            if (self) {
              self->gc_required_.insert(id);
            }
          },
          weak_factory_.GetWeakPtr(), id),
      base::BindOnce(&ProfileRegistry::OnObliterated,
                     weak_factory_.GetWeakPtr(), id, std::move(done)));
}

void ProfileRegistry::OnObliterated(const std::string& id,
                                    base::OnceClosure done) {
  if (!gc_required_.contains(id) && !Exists(id)) {
    SetPendingDelete(id, false);
  }
  std::move(done).Run();
}

void ProfileRegistry::SetPendingDelete(const std::string& id, bool pending) {
  ScopedListPrefUpdate update(profile_->GetPrefs(), prefs::kPendingDeletes);
  update->EraseValue(base::Value(id));
  if (pending) {
    update->Append(id);
  }
}

void ProfileRegistry::FlushCookies(const std::string& id) {
  if (content::StoragePartition* partition = profile_->GetStoragePartition(
          PartitionConfig(id), /*can_create=*/false)) {
    FlushPartition(partition);
  }
}

void ProfileRegistry::FlushAllCookies() {
  profile_->ForEachLoadedStoragePartition(&FlushPartition);
}

}  // namespace agent_chrome
