// Copyright 2026 The Agent Chrome Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AGENT_CHROME_PROFILE_REGISTRY_H_
#define CHROME_BROWSER_AGENT_CHROME_PROFILE_REGISTRY_H_

#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "content/public/browser/storage_partition_config.h"

class Profile;

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace agent_chrome {

// Agent profiles: named, persistent storage (cookies, local storage, ...).
// Each one is a fixed StoragePartition of the single Chrome profile, in its
// own partition domain ("ac-<id>") so that it can be wiped on its own.
class ProfileRegistry {
 public:
  struct Entry {
    std::string id;
    base::Time created;
  };

  explicit ProfileRegistry(Profile* profile);
  ProfileRegistry(const ProfileRegistry&) = delete;
  ProfileRegistry& operator=(const ProfileRegistry&) = delete;
  ~ProfileRegistry();

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Kebab-case slug, 1-64 chars: ^[a-z0-9]+(-[a-z0-9]+)*$.
  static bool IsValidId(std::string_view id);

  // Returns the profile id for an agent partition config, or "" if `config`
  // is not one.
  static std::string IdForConfig(const content::StoragePartitionConfig& config);

  bool Exists(const std::string& id) const;
  std::vector<Entry> List() const;

  // Returns false if the profile already exists.
  bool Create(const std::string& id);

  content::StoragePartitionConfig PartitionConfig(const std::string& id) const;

  // Forgets `id` and wipes its storage. Its tabs must be closed already.
  // `done` runs once the data is cleared.
  void Delete(const std::string& id, base::OnceClosure done);

  // Writes pending cookie changes of `id` (or of every loaded partition) to
  // disk. No-op for partitions that aren't loaded.
  void FlushCookies(const std::string& id);
  void FlushAllCookies();

 private:
  void Obliterate(const std::string& id, base::OnceClosure done);
  void OnObliterated(const std::string& id, base::OnceClosure done);
  void SetPendingDelete(const std::string& id, bool pending);

  raw_ptr<Profile> profile_;
  // Ids whose partition was loaded at deletion; their directories go at the
  // next launch.
  std::set<std::string> gc_required_;
  base::WeakPtrFactory<ProfileRegistry> weak_factory_{this};
};

}  // namespace agent_chrome

#endif  // CHROME_BROWSER_AGENT_CHROME_PROFILE_REGISTRY_H_
