// Copyright 2026 Silkweave
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/shuttle/agent_browser_sidecar.h"

#include <fcntl.h>
#include <unistd.h>

#include <utility>

#include "base/containers/span.h"
#include "base/environment.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "base/process/kill.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/task/bind_post_task.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/version_info/version_info.h"

namespace shuttle {

namespace {

constexpr base::TimeDelta kStartTimeout = base::Seconds(20);
constexpr base::TimeDelta kCloseTimeout = base::Seconds(3);
// The daemon agent-browser starts per session exits after this much idle
// time, a fallback for when we couldn't ask it to close (browser shutdown).
constexpr char kDaemonIdleMs[] = "60000";

// Reads `fd` until EOF, running `on_line` for every line and `on_exit` at the
// end. Blocks: runs as its own thread pool task.
void ReadLines(base::ScopedFD fd,
               base::RepeatingCallback<void(std::string)> on_line,
               base::OnceClosure on_exit) {
  std::string buffer;
  char chunk[16 * 1024];
  while (true) {
    ssize_t n = HANDLE_EINTR(read(fd.get(), chunk, sizeof(chunk)));
    if (n <= 0) {
      break;
    }
    buffer.append(chunk, static_cast<size_t>(n));
    size_t newline;
    while ((newline = buffer.find('\n')) != std::string::npos) {
      std::string line = buffer.substr(0, newline);
      buffer.erase(0, newline + 1);
      if (!line.empty()) {
        on_line.Run(std::move(line));
      }
    }
  }
  std::move(on_exit).Run();
}

}  // namespace

// Owns the child process and its stdin. Runs on a blocking sequence.
class Sidecar::Io {
 public:
  Io() = default;
  Io(const Io&) = delete;
  Io& operator=(const Io&) = delete;
  ~Io() { Kill(); }

  StartResult Start(const Config& config,
                    base::RepeatingCallback<void(std::string)> on_line,
                    base::OnceClosure on_exit) {
    // Checked here, on the blocking sequence: no fork per call when the
    // binary isn't bundled (Linux builds today).
    if (!base::PathExists(config.binary)) {
      return StartResult::kNotInstalled;
    }
    for (const base::FilePath& dir : config.extra_dirs) {
      base::CreateDirectory(dir);
    }
    base::CreateDirectory(config.socket_dir);
    base::CreateDirectory(config.screenshot_dir);

    int in_pipe[2];
    int out_pipe[2];
    if (pipe(in_pipe) != 0) {
      return StartResult::kFailed;
    }
    base::ScopedFD child_stdin(in_pipe[0]);
    base::ScopedFD parent_stdin(in_pipe[1]);
    if (pipe(out_pipe) != 0) {
      return StartResult::kFailed;
    }
    base::ScopedFD parent_stdout(out_pipe[0]);
    base::ScopedFD child_stdout(out_pipe[1]);
    base::ScopedFD dev_null(HANDLE_EINTR(open("/dev/null", O_WRONLY)));

    base::LaunchOptions options;
    options.fds_to_remap.emplace_back(child_stdin.get(), STDIN_FILENO);
    options.fds_to_remap.emplace_back(child_stdout.get(), STDOUT_FILENO);
    options.fds_to_remap.emplace_back(dev_null.get(), STDERR_FILENO);
    options.environment["AGENT_BROWSER_CDP"] = config.cdp_endpoint;
    options.environment["AGENT_BROWSER_SESSION"] = config.session_id;
    // No AGENT_BROWSER_PIN_TAB: pinned, a fresh daemon opens a new tab of its
    // own. Every call switches to its targetId tab first anyway.
    options.environment["AGENT_BROWSER_SOCKET_DIR"] =
        config.socket_dir.AsUTF8Unsafe();
    options.environment["AGENT_BROWSER_SCREENSHOT_DIR"] =
        config.screenshot_dir.AsUTF8Unsafe();
    options.environment["AGENT_BROWSER_IDLE_TIMEOUT_MS"] = kDaemonIdleMs;

    // "all": our tools span the core, debug (console/errors), state and
    // wait_for_url profiles; we only list the curated ones anyway.
    process_ = base::LaunchProcess(
        std::vector<std::string>{config.binary.AsUTF8Unsafe(), "mcp", "--tools",
                                 "all"},
        options);
    if (!process_.IsValid()) {
      return StartResult::kFailed;
    }
    stdin_fd_ = std::move(parent_stdin);
    // Our copies of the child's ends close here (end of scope).
    base::ThreadPool::PostTask(
        FROM_HERE,
        {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
         base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
        base::BindOnce(&ReadLines, std::move(parent_stdout),
                       std::move(on_line), std::move(on_exit)));
    return StartResult::kOk;
  }

  void Write(std::string line) {
    if (!stdin_fd_.is_valid()) {
      return;
    }
    line.push_back('\n');
    // On EPIPE the reader sees the exit.
    base::WriteFileDescriptor(stdin_fd_.get(), line);
  }

  void Kill() {
    stdin_fd_.reset();
    if (process_.IsValid()) {
      process_.Terminate(/*exit_code=*/0, /*wait=*/false);
      base::EnsureProcessTerminated(std::move(process_));
    }
  }

 private:
  base::Process process_;
  base::ScopedFD stdin_fd_;
};

Sidecar::Config::Config() = default;
Sidecar::Config::Config(const Config&) = default;
Sidecar::Config::~Config() = default;

Sidecar::Job::Job() = default;
Sidecar::Job::Job(Job&&) = default;
Sidecar::Job& Sidecar::Job::operator=(Job&&) = default;
Sidecar::Job::~Job() = default;

Sidecar::Sidecar(Config config) : config_(std::move(config)) {}

Sidecar::~Sidecar() {
  // Destroying `io_` posts Io's destructor, which kills the process.
}

void Sidecar::CallTool(std::string target_id,
                       std::string tool,
                       base::DictValue arguments,
                       base::TimeDelta timeout,
                       ResultCallback callback) {
  if (closing_) {
    std::move(callback).Run(base::unexpected("The session is closing."));
    return;
  }
  Job job;
  job.target_id = std::move(target_id);
  job.tool = std::move(tool);
  job.arguments = std::move(arguments);
  job.timeout = timeout;
  job.callback = std::move(callback);
  jobs_.push_back(std::move(job));
  PumpJobs();
}

void Sidecar::Close(base::OnceClosure done) {
  closing_ = true;
  close_done_ = std::move(done);
  FailAll("The session was closed.");
  pending_.clear();
  if (state_ != State::kReady) {
    FinishClose();
    return;
  }
  job_timer_.Start(FROM_HERE, kCloseTimeout,
                   base::BindOnce(&Sidecar::FinishClose,
                                  weak_factory_.GetWeakPtr()));
  SendRequest(
      "tools/call",
      base::DictValue()
          .Set("name", "agent_browser_close")
          .Set("arguments", base::DictValue()),
      base::BindOnce(&Sidecar::OnCloseResponse, weak_factory_.GetWeakPtr()));
}

void Sidecar::Kill() {
  job_timer_.Stop();
  FailAll("The session was closed.");
  KillProcess();
}

void Sidecar::OnCloseResponse(base::DictValue) {
  FinishClose();
}

void Sidecar::FinishClose() {
  job_timer_.Stop();
  KillProcess();
  if (close_done_) {
    std::move(close_done_).Run();  // May delete `this`.
  }
}

void Sidecar::KillProcess() {
  state_ = State::kIdle;
  bound_target_.clear();
  pending_.clear();
  io_.Reset();  // Io::~Io kills the child.
}

void Sidecar::Start() {
  state_ = State::kStarting;
  const int generation = ++generation_;
  // BLOCK_SHUTDOWN so a kill queued at browser exit still runs.
  io_ = base::SequenceBound<Io>(base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::BLOCK_SHUTDOWN}));
  auto weak = weak_factory_.GetWeakPtr();
  io_.AsyncCall(&Io::Start)
      .WithArgs(config_,
                base::BindPostTaskToCurrentDefault(
                    base::BindRepeating(&Sidecar::OnLine, weak, generation)),
                base::BindPostTaskToCurrentDefault(
                    base::BindOnce(&Sidecar::OnExit, weak, generation)))
      .Then(base::BindOnce(&Sidecar::OnStarted, weak));
}

void Sidecar::OnStarted(StartResult result) {
  if (state_ != State::kStarting) {
    return;  // Killed (timeout) while starting.
  }
  if (result == StartResult::kNotInstalled) {
    FailAll("agent-browser is not installed (" +
            config_.binary.AsUTF8Unsafe() + ").");
    KillProcess();
    return;
  }
  if (result != StartResult::kOk) {
    FailAll("Could not start agent-browser (" + config_.binary.AsUTF8Unsafe() +
            ").");
    KillProcess();
    return;
  }
  SendRequest(
      "initialize",
      base::DictValue()
          .Set("protocolVersion", "2025-11-25")
          .Set("capabilities", base::DictValue())
          .Set("clientInfo", base::DictValue()
                                 .Set("name", "shuttle")
                                 .Set("version", version_info::GetVersionNumber())),
      base::BindOnce(&Sidecar::OnInitialized, weak_factory_.GetWeakPtr()));
}

void Sidecar::OnInitialized(base::DictValue response) {
  if (response.contains("error")) {
    FailAll("agent-browser refused to initialize.");
    KillProcess();
    return;
  }
  SendNotification("notifications/initialized");
  state_ = State::kReady;
  PumpJobs();
}

void Sidecar::PumpJobs() {
  if (busy_ || jobs_.empty() || closing_) {
    return;
  }
  if (state_ == State::kIdle) {
    job_timer_.Start(FROM_HERE, kStartTimeout,
                     base::BindOnce(&Sidecar::OnJobTimeout,
                                    weak_factory_.GetWeakPtr()));
    Start();
    return;
  }
  if (state_ == State::kStarting) {
    return;  // OnInitialized pumps again.
  }
  busy_ = true;
  job_timer_.Start(FROM_HERE, jobs_.front().timeout,
                   base::BindOnce(&Sidecar::OnJobTimeout,
                                  weak_factory_.GetWeakPtr()));
  if (jobs_.front().target_id.empty() ||
      jobs_.front().target_id == bound_target_) {
    SendToolCall();
    return;
  }
  // Binds the session to the tab (tab_switch accepts a CDP target id).
  SendRequest(
      "tools/call",
      base::DictValue()
          .Set("name", "agent_browser_tab_switch")
          .Set("arguments",
               base::DictValue().Set("tab", jobs_.front().target_id)),
      base::BindOnce(&Sidecar::OnTabSwitched, weak_factory_.GetWeakPtr()));
}

void Sidecar::OnTabSwitched(base::DictValue response) {
  if (jobs_.empty()) {
    return;
  }
  const base::DictValue* result = response.FindDict("result");
  if (!result) {
    FinishJob(base::unexpected(std::string("tab switch failed")));
    return;
  }
  if (result->FindBool("isError").value_or(false)) {
    // Pass agent-browser's error text on ("No tab with target id ...").
    FinishJob(base::ok(result->Clone()));
    return;
  }
  bound_target_ = jobs_.front().target_id;
  SendToolCall();
}

void Sidecar::SendToolCall() {
  Job& job = jobs_.front();
  SendRequest("tools/call",
              base::DictValue()
                  .Set("name", "agent_browser_" + job.tool)
                  .Set("arguments", job.arguments.Clone()),
              base::BindOnce(&Sidecar::OnToolResponse,
                             weak_factory_.GetWeakPtr()));
}

void Sidecar::OnToolResponse(base::DictValue response) {
  if (jobs_.empty()) {
    return;
  }
  if (base::DictValue* result = response.FindDict("result")) {
    FinishJob(base::ok(std::move(*result)));
    return;
  }
  const base::DictValue* error = response.FindDict("error");
  const std::string* message = error ? error->FindString("message") : nullptr;
  FinishJob(base::unexpected(message ? *message
                                       : std::string("agent-browser error")));
}

void Sidecar::FinishJob(base::expected<base::DictValue, std::string> result) {
  job_timer_.Stop();
  busy_ = false;
  Job job = std::move(jobs_.front());
  jobs_.pop_front();
  std::move(job.callback).Run(std::move(result));
  PumpJobs();
}

void Sidecar::FailAll(const std::string& message) {
  std::deque<Job> jobs = std::move(jobs_);
  jobs_.clear();
  busy_ = false;
  for (Job& job : jobs) {
    std::move(job.callback).Run(base::unexpected(message));
  }
}

void Sidecar::OnJobTimeout() {
  FailAll("agent-browser timed out; it is restarted by the next call.");
  KillProcess();
}

void Sidecar::OnLine(int generation, std::string line) {
  if (generation != generation_) {
    return;
  }
  std::optional<base::Value> parsed =
      base::JSONReader::Read(line, base::JSON_PARSE_RFC);
  if (!parsed || !parsed->is_dict()) {
    return;
  }
  base::DictValue message = std::move(*parsed).TakeDict();
  std::optional<int> id = message.FindInt("id");
  if (!id || message.contains("method")) {
    return;  // Notification or server-initiated request: ignored.
  }
  auto it = pending_.find(*id);
  if (it == pending_.end()) {
    return;
  }
  ResponseCallback callback = std::move(it->second);
  pending_.erase(it);
  std::move(callback).Run(std::move(message));
}

void Sidecar::OnExit(int generation) {
  if (generation != generation_ || state_ == State::kIdle) {
    return;  // A process we already killed.
  }
  if (closing_) {
    FinishClose();
    return;
  }
  LOG(WARNING) << "agent-browser sidecar exited (session "
               << config_.session_id << ")";
  FailAll("agent-browser exited unexpectedly; the next call restarts it.");
  KillProcess();
}

int Sidecar::SendRequest(const std::string& method,
                         base::DictValue params,
                         ResponseCallback callback) {
  int id = next_id_++;
  pending_[id] = std::move(callback);
  Write(base::DictValue()
            .Set("jsonrpc", "2.0")
            .Set("id", id)
            .Set("method", method)
            .Set("params", std::move(params)));
  return id;
}

void Sidecar::SendNotification(const std::string& method) {
  Write(base::DictValue().Set("jsonrpc", "2.0").Set("method", method));
}

void Sidecar::Write(const base::DictValue& message) {
  io_.AsyncCall(&Io::Write)
      .WithArgs(base::WriteJson(message).value_or(std::string()));
}

}  // namespace shuttle
