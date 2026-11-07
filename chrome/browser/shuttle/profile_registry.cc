// Copyright 2026 Silkweave
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/shuttle/profile_registry.h"

#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <array>
#include <utility>

#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/hash/hash.h"
#include "base/json/values_util.h"
#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "base/rand_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
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

// Opens directory `name` in `parent_fd` without following a link; it must be
// owned by this user. Never creates it.
base::ScopedFD OpenOwnDir(int parent_fd, const char* name) {
  base::ScopedFD fd(HANDLE_EINTR(openat(
      parent_fd, name, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC)));
  struct stat st;
  if (!fd.is_valid() || fstat(fd.get(), &st) != 0 || st.st_uid != geteuid()) {
    return base::ScopedFD();
  }
  return fd;
}

// Removes <profile>/shuttle/secrets/<id> for each id `matches` accepts:
// the files in it, then the directory. Nothing is followed through a link
// (an entry that is a link is removed as a link); subdirectories (never
// created by save-text) are left alone, and so is their parent then.
void DeleteSecretDirs(base::FilePath profile_path,
                      base::RepeatingCallback<bool(std::string_view)> matches) {
  base::ScopedFD profile_fd(HANDLE_EINTR(
      open(profile_path.value().c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC)));
  if (!profile_fd.is_valid()) {
    return;
  }
  base::ScopedFD shuttle_fd = OpenOwnDir(profile_fd.get(), "shuttle");
  if (!shuttle_fd.is_valid()) {
    return;
  }
  base::ScopedFD secrets_fd = OpenOwnDir(shuttle_fd.get(), "secrets");
  if (!secrets_fd.is_valid()) {
    return;
  }
  std::vector<std::string> ids;
  {
    // fdopendir takes over the fd it is given.
    int list_fd = HANDLE_EINTR(dup(secrets_fd.get()));
    DIR* dir = list_fd >= 0 ? fdopendir(list_fd) : nullptr;
    if (!dir) {
      if (list_fd >= 0) {
        close(list_fd);
      }
      return;
    }
    while (dirent* entry = readdir(dir)) {
      std::string_view name(entry->d_name);
      if (name != "." && name != ".." && matches.Run(name)) {
        ids.emplace_back(name);
      }
    }
    closedir(dir);
  }
  for (const std::string& id : ids) {
    base::ScopedFD agent_fd = OpenOwnDir(secrets_fd.get(), id.c_str());
    if (!agent_fd.is_valid()) {
      // A link (or not ours): remove the link itself, never its target.
      struct stat st;
      if (fstatat(secrets_fd.get(), id.c_str(), &st, AT_SYMLINK_NOFOLLOW) ==
              0 &&
          S_ISLNK(st.st_mode)) {
        unlinkat(secrets_fd.get(), id.c_str(), 0);
      }
      continue;
    }
    std::vector<std::string> files;
    int list_fd = HANDLE_EINTR(dup(agent_fd.get()));
    DIR* dir = list_fd >= 0 ? fdopendir(list_fd) : nullptr;
    if (!dir) {
      if (list_fd >= 0) {
        close(list_fd);
      }
      continue;
    }
    while (dirent* entry = readdir(dir)) {
      std::string_view name(entry->d_name);
      if (name != "." && name != "..") {
        files.emplace_back(name);
      }
    }
    closedir(dir);
    for (const std::string& file : files) {
      // Fails (and keeps) a subdirectory: unlinkat without AT_REMOVEDIR.
      unlinkat(agent_fd.get(), file.c_str(), 0);
    }
    unlinkat(secrets_fd.get(), id.c_str(), AT_REMOVEDIR);
  }
}

// Removes the on-disk directories of throwaway agents' (in-memory) storage
// partitions that `matches` accepts: <profile>/Storage/ext/shuttle-tmp-<id>.
// Chromium still keeps the GPU shader caches of in-memory partitions there;
// nothing else is written.
void DeleteThrowawayPartitionDirs(
    base::FilePath profile_path,
    base::RepeatingCallback<bool(std::string_view)> matches) {
  const std::string prefix =
      std::string(kAgentPartitionDomainPrefix) + kThrowawayAgentIdPrefix;
  base::FileEnumerator dirs(profile_path.Append(FILE_PATH_LITERAL("Storage"))
                                .Append(FILE_PATH_LITERAL("ext")),
                            /*recursive=*/false,
                            base::FileEnumerator::DIRECTORIES);
  for (base::FilePath dir = dirs.Next(); !dir.empty(); dir = dirs.Next()) {
    const std::string name = dir.BaseName().AsUTF8Unsafe();
    if (base::StartsWith(name, prefix) &&
        matches.Run(std::string_view(name).substr(
            sizeof(kAgentPartitionDomainPrefix) - 1))) {
      base::DeletePathRecursively(dir);
    }
  }
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
  // Throwaway agents never outlive the browser; their save-text files and
  // GPU caches left by a crash go now.
  auto any_throwaway = base::BindRepeating([](std::string_view name) {
    return ProfileRegistry::IsThrowawayId(name);
  });
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&DeleteSecretDirs, profile_->GetPath(), any_throwaway)
          .Then(base::BindOnce(&DeleteThrowawayPartitionDirs,
                               profile_->GetPath(), any_throwaway)));
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
bool ProfileRegistry::IsThrowawayId(std::string_view id) {
  return base::StartsWith(id, kThrowawayPrefix);
}

// static
std::string ProfileRegistry::IdForConfig(
    const content::StoragePartitionConfig& config) {
  const std::string& domain = config.partition_domain();
  if (config.is_default() || !config.partition_name().empty() ||
      !base::StartsWith(domain, kAgentPartitionDomainPrefix)) {
    return std::string();
  }
  std::string id = domain.substr(sizeof(kAgentPartitionDomainPrefix) - 1);
  // Throwaway agents (only they) have in-memory storage.
  if (!IsValidId(id) || config.in_memory() != IsThrowawayId(id)) {
    return std::string();
  }
  return id;
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
  return throwaway_.contains(id) ||
         profile_->GetPrefs()->GetDict(prefs::kProfiles).contains(id);
}

std::optional<ProfileRegistry::Entry> ProfileRegistry::Get(
    const std::string& id) const {
  if (auto it = throwaway_.find(id); it != throwaway_.end()) {
    return it->second.entry;
  }
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
  for (const auto& [id, throwaway] : throwaway_) {
    entries.push_back(throwaway.entry);
  }
  return entries;
}

bool ProfileRegistry::Create(const std::string& id, const Identity& identity) {
  DCHECK(IsValidId(id));
  DCHECK(!IsThrowawayId(id));
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

std::string ProfileRegistry::CreateThrowaway(const Identity& identity) {
  DCHECK(!identity.avatar && !identity.persist_session_cookies);
  std::string id;
  do {
    id = kThrowawayPrefix + base::HexEncodeLower(base::RandBytesAsVector(6));
  } while (Exists(id) || used_throwaway_ids_.contains(id));
  used_throwaway_ids_.insert(id);
  Throwaway& throwaway = throwaway_[id];
  throwaway.entry.id = id;
  throwaway.entry.name = identity.name.value_or(id);
  throwaway.entry.color = identity.color.value_or(DefaultColor(id));
  throwaway.entry.created = base::Time::Now();
  throwaway.entry.persist_session_cookies = false;
  throwaway.entry.throwaway = true;
  throwaway.entry.idle_timeout =
      identity.idle_timeout.value_or(kDefaultThrowawayIdleTimeout);
  throwaway.last_active = base::TimeTicks::Now();
  return id;
}

bool ProfileRegistry::IsThrowaway(const std::string& id) const {
  return throwaway_.contains(id);
}

std::vector<std::string> ProfileRegistry::ThrowawayIds() const {
  std::vector<std::string> ids;
  for (const auto& [id, throwaway] : throwaway_) {
    ids.push_back(id);
  }
  return ids;
}

void ProfileRegistry::TouchThrowaway(const std::string& id) {
  if (auto it = throwaway_.find(id); it != throwaway_.end()) {
    it->second.last_active = base::TimeTicks::Now();
  }
}

base::TimeTicks ProfileRegistry::ThrowawayLastActive(
    const std::string& id) const {
  auto it = throwaway_.find(id);
  return it == throwaway_.end() ? base::TimeTicks() : it->second.last_active;
}

bool ProfileRegistry::Update(const std::string& id, const Identity& identity) {
  if (auto it = throwaway_.find(id); it != throwaway_.end()) {
    DCHECK(!identity.avatar && !identity.persist_session_cookies);
    Entry& entry = it->second.entry;
    if (identity.name) {
      entry.name = identity.name->empty() ? id : *identity.name;
    }
    if (identity.color) {
      entry.color = *identity.color;
    }
    if (identity.idle_timeout) {
      entry.idle_timeout = *identity.idle_timeout;
    }
    return true;
  }
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
  return content::StoragePartitionConfig::Create(
      profile_, DomainFor(id),
      /*partition_name=*/"",
      /*in_memory=*/IsThrowawayId(id));
}

void ProfileRegistry::Delete(const std::string& id, base::OnceClosure done) {
  if (throwaway_.contains(id)) {
    DeleteThrowaway(id, std::move(done));
    return;
  }
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

void ProfileRegistry::DeleteThrowaway(const std::string& id,
                                      base::OnceClosure done) {
  const content::StoragePartitionConfig config = PartitionConfig(id);
  throwaway_.erase(id);
  // Its save-text files go with it.
  base::OnceClosure delete_secrets = base::BindOnce(
      [](base::FilePath profile_path, std::string id, base::OnceClosure done) {
        auto this_one = base::BindRepeating(
            [](const std::string& id, std::string_view name) {
              return name == id;
            },
            id);
        base::ThreadPool::PostTaskAndReply(
            FROM_HERE,
            {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
             base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
            base::BindOnce(&DeleteSecretDirs, profile_path, this_one)
                .Then(base::BindOnce(&DeleteThrowawayPartitionDirs,
                                     profile_path, this_one)),
            std::move(done));
      },
      profile_->GetPath(), id, std::move(done));
  // The in-memory partition can't be unloaded; empty it (the id is never
  // reused, this frees the memory).
  content::StoragePartition* partition =
      profile_->GetStoragePartition(config, /*can_create=*/false);
  if (!partition) {
    std::move(delete_secrets).Run();
    return;
  }
  partition->ClearData(
      content::StoragePartition::REMOVE_DATA_MASK_ALL,
      /*filter_builder=*/nullptr,
      content::StoragePartition::StorageKeyPolicyMatcherFunction(),
      network::mojom::CookieDeletionFilter::New(),
      /*perform_storage_cleanup=*/false, base::Time(), base::Time::Max(),
      std::move(delete_secrets));
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
