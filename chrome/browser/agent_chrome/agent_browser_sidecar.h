// Copyright 2026 The Agent Chrome Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AGENT_CHROME_AGENT_BROWSER_SIDECAR_H_
#define CHROME_BROWSER_AGENT_CHROME_AGENT_BROWSER_SIDECAR_H_

#include <deque>
#include <map>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/sequence_bound.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/types/expected.h"
#include "base/values.h"

namespace agent_chrome {

// One `agent-browser mcp` child process (stdio, newline-delimited JSON-RPC)
// serving one agent session. Lives on the UI thread; spawning, writing and
// killing run on a blocking thread pool sequence, reading on its own task.
// Created lazily by PageToolManager on the first page-tool call.
//
// Tool calls are queued and run one at a time (agent-browser keeps "current
// tab" state, so a tab switch and the call that follows must not interleave).
class Sidecar {
 public:
  struct Config {
    Config();
    Config(const Config&);
    ~Config();

    base::FilePath binary;
    // Where the daemon sockets / screenshots go (created on start).
    base::FilePath socket_dir;
    base::FilePath screenshot_dir;
    // Also created on start (saved states live there).
    std::vector<base::FilePath> extra_dirs;
    // ws://host/devtools/session/<id>
    std::string cdp_endpoint;
    std::string session_id;
  };

  // The JSON-RPC `result` object of tools/call, or a transport error.
  using ResultCallback =
      base::OnceCallback<void(base::expected<base::DictValue, std::string>)>;

  explicit Sidecar(Config config);
  Sidecar(const Sidecar&) = delete;
  Sidecar& operator=(const Sidecar&) = delete;
  // Kills the process without waiting.
  ~Sidecar();

  // Runs agent-browser tool `tool` (no `agent_browser_` prefix) on tab
  // `target_id` (switching to it first if needed). `timeout` bounds the
  // whole call including a cold start; on expiry the process is killed and
  // restarted by the next call.
  void CallTool(std::string target_id,
                std::string tool,
                base::DictValue arguments,
                base::TimeDelta timeout,
                ResultCallback callback);

  // Fails queued calls, asks agent-browser to shut its daemon down
  // (`close`; it leaves the browser session alone), then kills the process.
  // `done` runs once the process is gone (or right away if none runs).
  void Close(base::OnceClosure done);

  // Immediate kill, for browser shutdown.
  void Kill();

 private:
  class Io;
  enum class State { kIdle, kStarting, kReady };
  enum class StartResult { kOk, kNotInstalled, kFailed };
  struct Job {
    Job();
    Job(Job&&);
    Job& operator=(Job&&);
    ~Job();
    std::string target_id;
    std::string tool;
    base::DictValue arguments;
    base::TimeDelta timeout;
    ResultCallback callback;
  };
  using ResponseCallback = base::OnceCallback<void(base::DictValue)>;

  void Start();
  void OnStarted(StartResult result);
  void OnInitialized(base::DictValue response);
  void PumpJobs();
  void OnTabSwitched(base::DictValue response);
  void SendToolCall();
  void OnToolResponse(base::DictValue response);
  void FinishJob(base::expected<base::DictValue, std::string> result);
  void FailAll(const std::string& message);
  void OnJobTimeout();
  void OnLine(int generation, std::string line);
  void OnExit(int generation);
  void KillProcess();
  int SendRequest(const std::string& method,
                  base::DictValue params,
                  ResponseCallback callback);
  void SendNotification(const std::string& method);
  void Write(const base::DictValue& message);
  void OnCloseResponse(base::DictValue response);
  void FinishClose();

  const Config config_;
  State state_ = State::kIdle;
  base::SequenceBound<Io> io_;
  int next_id_ = 1;
  // Bumped per process, so late events of a killed process are ignored.
  int generation_ = 0;
  std::map<int, ResponseCallback> pending_;
  std::deque<Job> jobs_;
  bool busy_ = false;
  // The tab agent-browser is bound to, "" if unknown.
  std::string bound_target_;
  base::OneShotTimer job_timer_;
  bool closing_ = false;
  base::OnceClosure close_done_;
  base::WeakPtrFactory<Sidecar> weak_factory_{this};
};

}  // namespace agent_chrome

#endif  // CHROME_BROWSER_AGENT_CHROME_AGENT_BROWSER_SIDECAR_H_
