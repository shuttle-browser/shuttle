// Copyright 2026 Silkweave
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHUTTLE_PROFILE_REGISTRY_H_
#define CHROME_BROWSER_SHUTTLE_PROFILE_REGISTRY_H_

#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/shuttle/agent_partition.h"
#include "components/tab_groups/tab_group_color.h"
#include "content/public/browser/storage_partition_config.h"

class Profile;

namespace base {
class SequencedTaskRunner;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace shuttle {

// Agents (agent profiles): an identity (name, color, avatar) shown on the
// tab groups of its sessions, plus named, persistent storage (cookies, local
// storage, ...). The storage is a fixed StoragePartition of the single Chrome
// profile, in its own partition domain ("shuttle-<id>") so that it can be wiped on
// its own.
//
// Throwaway agents (create-agent { throwaway: true }) live only in memory: the
// registry keeps them out of prefs, their storage partition is in-memory, and
// their ids ("tmp-<random>") are generated here and never reused. They are
// deleted after `idle_timeout` without activity (ShuttleService) or with the
// browser. Throwaway-ness is the in-memory registry entry, not the id prefix:
// agents created as "tmp-..." before the prefix was reserved stay persistent
// agents (on-disk storage, kept at startup).
class ProfileRegistry {
 public:
  struct Entry {
    Entry();
    Entry(const Entry&);
    ~Entry();

    std::string id;
    // Friendly name; the id if none was set.
    std::string name;
    tab_groups::TabGroupColorId color = tab_groups::TabGroupColorId::kGrey;
    // https: or data:image/ URL, or "".
    std::string avatar;
    base::Time created;
    // Session cookies survive a browser restart (persisted and restored).
    // Read when the partition's network context is configured, so a change
    // applies at the next start (shuttle::ShouldPersistSessionCookies).
    bool persist_session_cookies = true;
    // A throwaway agent (in memory only, deleted when idle).
    bool throwaway = false;
    // Throwaway agents: deleted after this long without activity.
    base::TimeDelta idle_timeout;
  };

  // Fields to set; unset fields are left as they are.
  struct Identity {
    Identity();
    Identity(const Identity&);
    ~Identity();

    std::optional<std::string> name;
    std::optional<tab_groups::TabGroupColorId> color;
    // "" removes the avatar.
    std::optional<std::string> avatar;
    std::optional<bool> persist_session_cookies;
    // Throwaway agents only.
    std::optional<base::TimeDelta> idle_timeout;
  };

  // Id prefix reserved for throwaway agents.
  static constexpr const char* kThrowawayPrefix = kThrowawayAgentIdPrefix;
  static constexpr base::TimeDelta kDefaultThrowawayIdleTimeout =
      base::Minutes(15);

  explicit ProfileRegistry(Profile* profile);
  ProfileRegistry(const ProfileRegistry&) = delete;
  ProfileRegistry& operator=(const ProfileRegistry&) = delete;
  ~ProfileRegistry();

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Kebab-case slug, 1-64 chars: ^[a-z0-9]+(-[a-z0-9]+)*$.
  static bool IsValidId(std::string_view id);
  // Starts with kThrowawayPrefix (new persistent agents can't use it; older
  // ones may, see IsThrowaway()).
  static bool IsThrowawayId(std::string_view id);

  // Returns the profile id for an agent partition config, or "" if `config`
  // is not one. In-memory configs are accepted for throwaway ids only.
  static std::string IdForConfig(const content::StoragePartitionConfig& config);

  // Validation of identity fields; return an error message or "".
  static std::string CheckName(std::string_view name);
  static std::string CheckAvatar(std::string_view avatar);

  // Color names as used by the MCP tools ("blue", ...).
  static std::string_view ColorName(tab_groups::TabGroupColorId color);
  static std::optional<tab_groups::TabGroupColorId> ColorFromName(
      std::string_view name);

  bool Exists(const std::string& id) const;
  std::optional<Entry> Get(const std::string& id) const;
  std::vector<Entry> List() const;

  // Returns false if the profile already exists. Fields must be valid.
  bool Create(const std::string& id, const Identity& identity);

  // Creates a throwaway agent and returns its generated id. Fields must be
  // valid (no avatar, no persist_session_cookies). Only once
  // ThrowawayCleanupDone(): the startup cleanup of a previous run's files
  // must not see a new agent's.
  std::string CreateThrowaway(const Identity& identity);
  bool ThrowawayCleanupDone() const { return startup_cleanup_done_; }
  // A live throwaway agent (registry entry), whatever its id looks like.
  bool IsThrowaway(const std::string& id) const;
  std::vector<std::string> ThrowawayIds() const;
  // Activity of a throwaway agent outside its sessions (creation, a session
  // closing). No-op for other ids.
  void TouchThrowaway(const std::string& id);
  // The last TouchThrowaway (or creation), or null for other ids.
  base::TimeTicks ThrowawayLastActive(const std::string& id) const;

  // Returns false if the profile doesn't exist. Fields must be valid.
  bool Update(const std::string& id, const Identity& identity);

  content::StoragePartitionConfig PartitionConfig(const std::string& id) const;

  // Forgets `id` and wipes its storage. Its tabs must be closed already.
  // `done` runs once the data is cleared. A throwaway agent's save-text files
  // (<profile>/shuttle/secrets/<id>) go too.
  void Delete(const std::string& id, base::OnceClosure done);

  // The sequence that writes and deletes agents' save-text files
  // (<profile>/shuttle/secrets/<id>): a write posted before a throwaway
  // agent's deletion runs before its cleanup. Writers check Exists() first.
  const scoped_refptr<base::SequencedTaskRunner>& file_task_runner() const {
    return file_task_runner_;
  }

  // Writes pending cookie changes of `id` (or of every loaded partition) to
  // disk. No-op for partitions that aren't loaded.
  void FlushCookies(const std::string& id);
  void FlushAllCookies();

 private:
  void Obliterate(const std::string& id, base::OnceClosure done);
  void OnObliterated(const std::string& id, base::OnceClosure done);
  void SetPendingDelete(const std::string& id, bool pending);
  void DeleteThrowaway(const std::string& id, base::OnceClosure done);

  struct Throwaway {
    Entry entry;
    base::TimeTicks last_active;
  };

  raw_ptr<Profile> profile_;
  scoped_refptr<base::SequencedTaskRunner> file_task_runner_;
  bool startup_cleanup_done_ = false;
  // Ids whose partition was loaded at deletion; their directories go at the
  // next launch.
  std::set<std::string> gc_required_;
  // Throwaway agents, by id. Never in prefs.
  std::map<std::string, Throwaway> throwaway_;
  // Every throwaway id handed out in this run (never reused: a new agent must
  // not find a deleted one's in-memory partition).
  std::set<std::string> used_throwaway_ids_;
  base::WeakPtrFactory<ProfileRegistry> weak_factory_{this};
};

}  // namespace shuttle

#endif  // CHROME_BROWSER_SHUTTLE_PROFILE_REGISTRY_H_
