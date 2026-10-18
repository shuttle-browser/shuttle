// Copyright 2026 Silkweave
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/shuttle/profile_registry.h"

#include <array>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/hash/hash.h"
#include "base/json/values_util.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/shuttle/agent_partition.h"
#include "chrome/browser/shuttle/pref_names.h"
#include "chrome/browser/profiles/profile.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/browser/storage_partition.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"

namespace shuttle {

namespace {

constexpr size_t kMaxIdLength = 64;
constexpr size_t kMaxNameLength = 64;  // UTF-16 code units.
// Avatars live in prefs; keep data: URLs small (a 256x256 PNG or JPEG fits).
constexpr size_t kMaxAvatarLength = 256 * 1024;

constexpr char kName[] = "name";
constexpr char kColor[] = "color";
constexpr char kAvatar[] = "avatar";
constexpr char kCreated[] = "created";

// Indexed by TabGroupColorId.
constexpr std::array<std::string_view, 9> kColorNames = {
    "grey", "blue", "red", "yellow", "green", "pink", "purple", "cyan", "orange",
};
static_assert(kColorNames.size() ==
              static_cast<size_t>(tab_groups::TabGroupColorId::kNumEntries));

// Colors for agents without a chosen one (no grey: it reads as "no agent").
tab_groups::TabGroupColorId DefaultColor(const std::string& id) {
  static constexpr std::array kColors = {
      tab_groups::TabGroupColorId::kBlue,   tab_groups::TabGroupColorId::kRed,
      tab_groups::TabGroupColorId::kYellow, tab_groups::TabGroupColorId::kGreen,
      tab_groups::TabGroupColorId::kPink,   tab_groups::TabGroupColorId::kPurple,
      tab_groups::TabGroupColorId::kCyan,   tab_groups::TabGroupColorId::kOrange,
  };
  return kColors[base::PersistentHash(id) % kColors.size()];
}

void ApplyIdentity(base::DictValue& dict,
                   const ProfileRegistry::Identity& identity) {
  if (identity.name) {
    dict.Set(kName, *identity.name);
  }
  if (identity.color) {
    dict.Set(kColor, static_cast<int>(*identity.color));
  }
  if (identity.avatar) {
    if (identity.avatar->empty()) {
      dict.Remove(kAvatar);
    } else {
      dict.Set(kAvatar, *identity.avatar);
    }
  }
  if (identity.persist_session_cookies) {
    // Stored only when off; missing = on.
    if (*identity.persist_session_cookies) {
      dict.Remove(prefs::kPersistSessionCookies);
    } else {
      dict.Set(prefs::kPersistSessionCookies, false);
    }
  }
}

std::string DomainFor(std::string_view id) {
  return base::StrCat({kAgentPartitionDomainPrefix, id});
}

void FlushPartition(content::StoragePartition* partition) {
  partition->GetCookieManagerForBrowserProcess()->FlushCookieStore(
      base::DoNothing());
}

}  // namespace

ProfileRegistry::Entry::Entry() = default;
ProfileRegistry::Entry::Entry(const Entry&) = default;
ProfileRegistry::Entry::~Entry() = default;
ProfileRegistry::Identity::Identity() = default;
ProfileRegistry::Identity::Identity(const Identity&) = default;
ProfileRegistry::Identity::~Identity() = default;

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
      !base::StartsWith(domain, kAgentPartitionDomainPrefix)) {
    return std::string();
  }
  std::string id = domain.substr(sizeof(kAgentPartitionDomainPrefix) - 1);
  return IsValidId(id) ? id : std::string();
}

// static
std::string ProfileRegistry::CheckName(std::string_view name) {
  std::u16string name16 = base::UTF8ToUTF16(name);
  if (base::TrimWhitespace(name16, base::TRIM_ALL).empty()) {
    return "The name must not be empty";
  }
  if (name16.size() > kMaxNameLength) {
    return "The name is too long (max 64 characters)";
  }
  return std::string();
}

// static
std::string ProfileRegistry::CheckAvatar(std::string_view avatar) {
  if (avatar.empty()) {
    return std::string();
  }
  if (avatar.size() > kMaxAvatarLength) {
    return "The avatar is too large (max 256 KB)";
  }
  if (!base::StartsWith(avatar, "https://") &&
      !base::StartsWith(avatar, "data:image/")) {
    return "The avatar must be an https: or data:image/ URL";
  }
  return std::string();
}

// static
std::string_view ProfileRegistry::ColorName(
    tab_groups::TabGroupColorId color) {
  size_t index = static_cast<size_t>(color);
  return index < kColorNames.size() ? kColorNames[index] : kColorNames[0];
}

// static
std::optional<tab_groups::TabGroupColorId> ProfileRegistry::ColorFromName(
    std::string_view name) {
  for (size_t i = 0; i < kColorNames.size(); ++i) {
    if (kColorNames[i] == name) {
      return static_cast<tab_groups::TabGroupColorId>(i);
    }
  }
  return std::nullopt;
}

bool ProfileRegistry::Exists(const std::string& id) const {
  return profile_->GetPrefs()->GetDict(prefs::kProfiles).contains(id);
}

std::optional<ProfileRegistry::Entry> ProfileRegistry::Get(
    const std::string& id) const {
  const base::Value* value =
      profile_->GetPrefs()->GetDict(prefs::kProfiles).Find(id);
  if (!value) {
    return std::nullopt;
  }
  Entry entry;
  entry.id = id;
  entry.name = id;
  entry.color = DefaultColor(id);
  if (const base::DictValue* dict = value->GetIfDict()) {
    if (const base::Value* created = dict->Find(kCreated)) {
      entry.created = base::ValueToTime(*created).value_or(base::Time());
    }
    if (const std::string* name = dict->FindString(kName);
        name && !name->empty()) {
      entry.name = *name;
    }
    if (std::optional<int> color = dict->FindInt(kColor);
        color && *color >= 0 &&
        *color < static_cast<int>(tab_groups::TabGroupColorId::kNumEntries)) {
      entry.color = static_cast<tab_groups::TabGroupColorId>(*color);
    }
    if (const std::string* avatar = dict->FindString(kAvatar)) {
      entry.avatar = *avatar;
    }
    entry.persist_session_cookies =
        dict->FindBool(prefs::kPersistSessionCookies).value_or(true);
  }
  return entry;
}

std::vector<ProfileRegistry::Entry> ProfileRegistry::List() const {
  std::vector<Entry> entries;
  for (auto [id, value] : profile_->GetPrefs()->GetDict(prefs::kProfiles)) {
    if (std::optional<Entry> entry = Get(id)) {
      entries.push_back(std::move(*entry));
    }
  }
  return entries;
}

bool ProfileRegistry::Create(const std::string& id, const Identity& identity) {
  DCHECK(IsValidId(id));
  if (Exists(id)) {
    return false;
  }
  base::DictValue dict;
  dict.Set(kCreated, base::TimeToValue(base::Time::Now()));
  ApplyIdentity(dict, identity);
  ScopedDictPrefUpdate update(profile_->GetPrefs(), prefs::kProfiles);
  update->Set(id, std::move(dict));
  // A re-created profile must not be wiped at the next launch.
  SetPendingDelete(id, false);
  gc_required_.erase(id);
  return true;
}

bool ProfileRegistry::Update(const std::string& id, const Identity& identity) {
  if (!Exists(id)) {
    return false;
  }
  ScopedDictPrefUpdate update(profile_->GetPrefs(), prefs::kProfiles);
  base::DictValue* dict = update->EnsureDict(id);
  ApplyIdentity(*dict, identity);
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

}  // namespace shuttle
