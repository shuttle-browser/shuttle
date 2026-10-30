// Copyright 2026 Silkweave
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHUTTLE_AGENT_PARTITION_H_
#define CHROME_BROWSER_SHUTTLE_AGENT_PARTITION_H_

#include <string>

class Profile;

namespace content {
class WebContents;
}

namespace base {
class FilePath;
}

namespace shuttle {

// Partition domain prefix of agent storage partitions ("shuttle-<id>").
inline constexpr char kAgentPartitionDomainPrefix[] = "shuttle-";

// Returns the agent id for the storage partition at `relative_partition_path`
// (relative to the profile directory, as passed to
// ContentBrowserClient::ConfigureNetworkContextParams), or "" if it isn't an
// agent partition. Doesn't check that the agent exists.
std::string AgentIdForPartitionPath(
    const base::FilePath& relative_partition_path);

// Hook for ProfileNetworkContextService: whether the on-disk partition at
// `relative_partition_path` persists and restores session cookies across
// browser restarts. True for an existing agent unless it opted out
// (ProfileRegistry::Entry::persist_session_cookies); false for every other
// non-default partition (upstream behavior). Read once, when the partition's
// network context is configured: a change applies at the next start.
bool ShouldPersistSessionCookies(Profile* profile,
                                 const base::FilePath& relative_partition_path);

// Whether `contents` shows an agent tab: its storage partition is an agent
// partition ("shuttle-<id>"). Doesn't check that the agent exists.
bool IsAgentWebContents(content::WebContents* contents);

}  // namespace shuttle

#endif  // CHROME_BROWSER_SHUTTLE_AGENT_PARTITION_H_
