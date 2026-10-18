// Copyright 2026 Silkweave
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/shuttle/agent_partition.h"

#include <vector>

#include "base/files/file_path.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/shuttle/pref_names.h"
#include "components/prefs/pref_service.h"

namespace shuttle {

std::string AgentIdForPartitionPath(
    const base::FilePath& relative_partition_path) {
  // Layout of content's StoragePartitionImplMap::GetStoragePartitionPath()
  // for an unnamed partition: Storage/ext/<partition domain>/def. Agent
  // partitions are unnamed (ProfileRegistry::PartitionConfig). If upstream
  // changes the layout, this returns "" and agents fall back to upstream
  // behavior (smoke-mcp phase 2 catches it).
  std::vector<base::FilePath::StringType> parts =
      relative_partition_path.GetComponents();
  if (parts.size() != 4 || parts[0] != FILE_PATH_LITERAL("Storage") ||
      parts[1] != FILE_PATH_LITERAL("ext") ||
      parts[3] != FILE_PATH_LITERAL("def")) {
    return std::string();
  }
  std::string domain = base::FilePath(parts[2]).AsUTF8Unsafe();
  if (!base::StartsWith(domain, kAgentPartitionDomainPrefix)) {
    return std::string();
  }
  return domain.substr(sizeof(kAgentPartitionDomainPrefix) - 1);
}

bool ShouldPersistSessionCookies(
    Profile* profile,
    const base::FilePath& relative_partition_path) {
  std::string id = AgentIdForPartitionPath(relative_partition_path);
  if (id.empty() || !profile->GetPrefs()->FindPreference(prefs::kProfiles)) {
    return false;
  }
  const base::Value* agent =
      profile->GetPrefs()->GetDict(prefs::kProfiles).Find(id);
  if (!agent) {
    // Unknown or deleted agent: nothing to keep.
    return false;
  }
  const base::DictValue* dict = agent->GetIfDict();
  return !dict ||
         dict->FindBool(prefs::kPersistSessionCookies).value_or(true);
}

}  // namespace shuttle
