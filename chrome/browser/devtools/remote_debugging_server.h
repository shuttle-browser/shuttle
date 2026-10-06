// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_REMOTE_DEBUGGING_SERVER_H_
#define CHROME_BROWSER_DEVTOOLS_REMOTE_DEBUGGING_SERVER_H_

#include <stdint.h>

#include <memory>

#include "base/files/file_path.h"
#include "content/public/browser/devtools_agent_host.h"

namespace content {
class DevToolsSocketFactory;
}

class RemoteDebuggingServer {
 public:
  static constexpr int kDefaultDevToolsPort = 9222;

  // Starts the always-on remote debugging server (Agent Chrome): HTTP on
  // localhost (--remote-debugging-port, default 9222 with fallback), plus the
  // pipe handler when --remote-debugging-pipe is given.
  static std::unique_ptr<RemoteDebuggingServer> Start();

  static void EnableTetheringForDebug();

  RemoteDebuggingServer(const RemoteDebuggingServer&) = delete;
  RemoteDebuggingServer& operator=(const RemoteDebuggingServer&) = delete;

  virtual ~RemoteDebuggingServer();

  void StopServer();

 protected:
  RemoteDebuggingServer();

  virtual void StartHttpServer(
      std::unique_ptr<content::DevToolsSocketFactory> factory,
      const base::FilePath& output_dir,
      const base::FilePath& debug_frontend_dir,
      content::DevToolsAgentHost::RemoteDebuggingServerMode mode);
  virtual void StopHttpServer();
  virtual void StartPipeHandler();
  virtual void StopPipeHandler();
};

#endif  // CHROME_BROWSER_DEVTOOLS_REMOTE_DEBUGGING_SERVER_H_
