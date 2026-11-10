// Copyright 2026 Silkweave
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHUTTLE_PAGE_TOOLS_H_
#define CHROME_BROWSER_SHUTTLE_PAGE_TOOLS_H_

#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "chrome/browser/shuttle/mcp_tools.h"

namespace shuttle {

class Sidecar;
class SessionManager;
class ProfileRegistry;

// The curated page tools (snapshot, click, fill, ...) of /mcp. Each forwards
// to the agent-browser sidecar of the tab's session. Definitions come from
// config/agent-browser-tools.json in the tooling repo, compiled into
// page_tools_data.inc by scripts/gen-page-tools.mjs.
class PageToolManager {
 public:
  PageToolManager(SessionManager* sessions,
                  ProfileRegistry* registry,
                  base::FilePath profile_path);
  PageToolManager(const PageToolManager&) = delete;
  PageToolManager& operator=(const PageToolManager&) = delete;
  ~PageToolManager();

  // `tools/list` entries (name, description, inputSchema) of all page tools.
  static base::ListValue ListTools();
  static bool IsPageTool(std::string_view name);

  // Runs page tool `name`; `callback` runs asynchronously (or at once for
  // argument errors).
  void Call(std::string_view name,
            const ToolContext& context,
            const base::DictValue& args,
            ToolCallback callback);

  // The session's sidecar (if any) is closed gracefully.
  void OnSessionClosed(const std::string& session_id);

  // Browser shutdown: kill all sidecars now.
  void Shutdown();

  // Path of the agent-browser binary: $SHUTTLE_AGENT_BROWSER, else
  // <app>/Contents/Helpers/agent-browser (macOS) or next to the executable.
  static base::FilePath FindBinary();

 private:
  // A page tool call in flight (with what it needs for a retry or for
  // post-processing the result).
  struct PendingCall;

  void Send(PendingCall call, ToolCallback callback);
  void OnResult(PendingCall call,
                ToolCallback callback,
                base::expected<base::DictValue, std::string> result);
  void SaveText(PendingCall call, ToolCallback callback, std::string text);
  // Sends a state-save / state-load call once its states directory checked out.
  void SendIfStatesDir(const base::FilePath& states_dir,
                       PendingCall call,
                       ToolCallback callback,
                       bool ok);

  Sidecar* GetOrCreateSidecar(const std::string& session_id,
                              const std::string& host);
  void OnSidecarClosed(Sidecar* sidecar);

  raw_ptr<SessionManager> sessions_;
  raw_ptr<ProfileRegistry> registry_;
  const base::FilePath profile_path_;
  std::map<std::string, std::unique_ptr<Sidecar>> sidecars_;
  // Closing sidecars, kept alive until their process is gone.
  std::vector<std::unique_ptr<Sidecar>> retiring_;
  // Reads and rescales screenshot captures, one at a time.
  scoped_refptr<base::SequencedTaskRunner> screenshot_runner_;
  base::WeakPtrFactory<PageToolManager> weak_factory_{this};
};

}  // namespace shuttle

#endif  // CHROME_BROWSER_SHUTTLE_PAGE_TOOLS_H_
