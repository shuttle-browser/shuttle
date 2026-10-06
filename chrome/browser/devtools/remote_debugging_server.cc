// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/remote_debugging_server.h"

#include <memory>
#include <optional>
#include <string_view>
#include <utility>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/task/thread_pool.h"
#include "base/types/expected.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/devtools/chrome_devtools_manager_delegate.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/devtools/features.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_socket_factory.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_switches.h"
#include "net/base/filename_util.h"
#include "net/base/net_errors.h"
#include "net/log/net_log_source.h"
#include "net/socket/tcp_server_socket.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"
#include "third_party/blink/public/public_buildflags.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

bool g_tethering_enabled = false;

const uint16_t kMinTetheringPort = 9333;
const uint16_t kMaxTetheringPort = 9444;
const int kBackLog = 10;

// Returns a port if the string is a valid port number, otherwise returns
// nullopt. A valid port is a number between 0 and 65535, inclusive.
std::optional<uint16_t> ParsePort(std::string_view port_str) {
  int port;
  if (base::StringToInt(port_str, &port) && port >= 0 && port <= 65535) {
    return static_cast<uint16_t>(port);
  }
  return std::nullopt;
}

class TCPServerSocketFactory
    : public content::DevToolsSocketFactory {
 public:
  explicit TCPServerSocketFactory(uint16_t port)
      : port_(port), last_tethering_port_(kMinTetheringPort) {}

  TCPServerSocketFactory(const TCPServerSocketFactory&) = delete;
  TCPServerSocketFactory& operator=(const TCPServerSocketFactory&) = delete;

 protected:
  // content::DevToolsSocketFactory.
  std::unique_ptr<net::ServerSocket> CreateForHttpServer() override {
    return CreateLocalHostServerSocket(port_);
  }
  uint16_t port_;

 private:
  std::unique_ptr<net::ServerSocket> CreateLocalHostServerSocket(int port) {
    std::unique_ptr<net::ServerSocket> socket(
        new net::TCPServerSocket(nullptr, net::NetLogSource()));
    if (socket->ListenWithAddressAndPort(
            "127.0.0.1", port, kBackLog) == net::OK)
      return socket;
    if (socket->ListenWithAddressAndPort("::1", port, kBackLog) == net::OK)
      return socket;
    return nullptr;
  }

  std::unique_ptr<net::ServerSocket> CreateForTethering(
      std::string* name) override {
    if (!g_tethering_enabled) {
      return nullptr;
    }

    if (last_tethering_port_ == kMaxTetheringPort)
      last_tethering_port_ = kMinTetheringPort;
    uint16_t port = ++last_tethering_port_;
    *name = base::NumberToString(port);
    return CreateLocalHostServerSocket(port);
  }

  uint16_t last_tethering_port_;
};

// Creates a server socket on a specific port, or any available port if the port
// is busy. Prefers a free port over switching from IPv4 to IPv6.
class TCPServerSocketFactoryWithPortFallback : public TCPServerSocketFactory {
 public:
  using TCPServerSocketFactory::TCPServerSocketFactory;

 private:
  std::unique_ptr<net::ServerSocket> CreateSocketOnAddress(const char* address,
                                                           uint16_t port) {
    auto socket =
        std::make_unique<net::TCPServerSocket>(nullptr, net::NetLogSource());
    if (socket->ListenWithAddressAndPort(address, port, kBackLog) == net::OK) {
      return socket;
    }
    return nullptr;
  }

  // content::DevToolsSocketFactory.
  std::unique_ptr<net::ServerSocket> CreateForHttpServer() override {
    std::unique_ptr<net::ServerSocket> socket =
        CreateSocketOnAddress("127.0.0.1", port_);
    if (socket) {
      return socket;
    }

    if (port_ != 0) {
      socket = CreateSocketOnAddress("127.0.0.1", 0);
      if (socket) {
        return socket;
      }
    }

    socket = CreateSocketOnAddress("::1", port_);
    if (socket) {
      return socket;
    }

    if (port_ != 0) {
      socket = CreateSocketOnAddress("::1", 0);
      if (socket) {
        return socket;
      }
    }
    return nullptr;
  }
};

}  // namespace

// static
void RemoteDebuggingServer::EnableTetheringForDebug() {
  g_tethering_enabled = true;
}

RemoteDebuggingServer::RemoteDebuggingServer() = default;

RemoteDebuggingServer::~RemoteDebuggingServer() {
  // Ensure Profile is alive, because the whole DevTools subsystem
  // accesses it during shutdown.
  DCHECK(g_browser_process->profile_manager());
  StopServer();
}

void RemoteDebuggingServer::StopServer() {
  StopHttpServer();
  StopPipeHandler();
}

void RemoteDebuggingServer::StartHttpServer(
    std::unique_ptr<content::DevToolsSocketFactory> factory,
    const base::FilePath& output_dir,
    const base::FilePath& debug_frontend_dir,
    content::DevToolsAgentHost::RemoteDebuggingServerMode mode) {
  content::DevToolsAgentHost::StartRemoteDebuggingServer(
      std::move(factory), output_dir, debug_frontend_dir, mode);
}

void RemoteDebuggingServer::StopHttpServer() {
  content::DevToolsAgentHost::StopRemoteDebuggingServer();
}

void RemoteDebuggingServer::StartPipeHandler() {
  content::DevToolsAgentHost::StartRemoteDebuggingPipeHandler(
      base::BindOnce(&ChromeDevToolsManagerDelegate::CloseBrowserSoon));
}

void RemoteDebuggingServer::StopPipeHandler() {
  content::DevToolsAgentHost::StopRemoteDebuggingPipeHandler();
}

// static
std::unique_ptr<RemoteDebuggingServer> RemoteDebuggingServer::Start() {
  // Agent Chrome: the browser exists to be driven by agents, so the
  // remote debugging server is always on. There is no approval prompt, no
  // "controlled by automated test software" infobar, no policy or default
  // user-data-dir gate. The server listens on localhost only and keeps the
  // WebSocket origin check (--remote-allow-origins). Authentication comes
  // with the built-in MCP server.
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  auto server = base::WrapUnique(new RemoteDebuggingServer());

  if (command_line.HasSwitch(switches::kRemoteDebuggingPipe)) {
    server->StartPipeHandler();
  }

  // The selected port is always written to DevToolsActivePort in the user
  // data dir, so clients (e.g. chrome-devtools-mcp --autoConnect) can find it.
  base::FilePath output_dir;
  {
    bool result = base::PathService::Get(chrome::DIR_USER_DATA, &output_dir);
    DCHECK(result);
  }

  base::FilePath debug_frontend_dir;
  if (command_line.HasSwitch(::switches::kCustomDevtoolsFrontend)) {
    GURL custom_devtools_frontend_url(command_line.GetSwitchValueASCII(
        ::switches::kCustomDevtoolsFrontend));
    if (custom_devtools_frontend_url.SchemeIsFile()) {
      net::FileURLToFilePath(custom_devtools_frontend_url, &debug_frontend_dir);
    }
  }

  // An explicit --remote-debugging-port binds exactly that port (0 picks a
  // free one). Without it, use 9222 and fall back to a free port when another
  // instance already holds it.
  std::unique_ptr<content::DevToolsSocketFactory> factory;
  if (auto port = ParsePort(
          command_line.GetSwitchValueASCII(::switches::kRemoteDebuggingPort))) {
    factory = std::make_unique<TCPServerSocketFactory>(*port);
  } else {
    factory = std::make_unique<TCPServerSocketFactoryWithPortFallback>(
        kDefaultDevToolsPort);
  }
  server->StartHttpServer(
      std::move(factory), output_dir, debug_frontend_dir,
      content::DevToolsAgentHost::RemoteDebuggingServerMode::kDefault);
  return server;
}
