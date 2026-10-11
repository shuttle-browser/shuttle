// Copyright 2026 Silkweave
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/shuttle/network_access.h"

#include "base/base64url.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "crypto/random.h"
#include "net/base/ip_address.h"

namespace shuttle {

namespace {

constexpr size_t kTokenBytes = 32;
constexpr size_t kMinTokenLength = 16;

struct State {
  std::string listen_address;
  std::string token;
};

State& GetState() {
  static base::NoDestructor<State> state;
  return *state;
}

std::string NewToken() {
  std::string token;
  base::Base64UrlEncode(crypto::RandBytesAsVector(kTokenBytes),
                        base::Base64UrlEncodePolicy::OMIT_PADDING, &token);
  return token;
}

// Tokens are used in URLs and headers: base64url characters only.
bool IsValidToken(const std::string& token) {
  if (token.size() < kMinTokenLength) {
    return false;
  }
  for (char c : token) {
    if (!base::IsAsciiAlphaNumeric(c) && c != '-' && c != '_') {
      return false;
    }
  }
  return true;
}

std::string LoadOrCreateToken(const base::FilePath& user_data_dir) {
  base::FilePath path = user_data_dir.AppendASCII(kAccessTokenFileName);
  std::string stored;
  if (base::ReadFileToString(path, &stored)) {
    std::string token(base::TrimWhitespaceASCII(stored, base::TRIM_ALL));
    if (IsValidToken(token)) {
      return token;
    }
    LOG(WARNING) << "Shuttle: ignoring an invalid " << kAccessTokenFileName
                 << "; writing a new one.";
  }
  std::string token = NewToken();
  base::CreateDirectory(user_data_dir);
  if (!base::WriteFile(path, token + "\n")) {
    LOG(ERROR) << "Shuttle: could not write " << path.value()
               << "; the access token only lasts until restart.";
    return token;
  }
#if BUILDFLAG(IS_POSIX)
  base::SetPosixFilePermissions(path, 0600);
#endif
  return token;
}

}  // namespace

void InitNetworkAccess(const base::FilePath& user_data_dir) {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  State& state = GetState();
  state = State();
  std::string address = command_line.GetSwitchValueASCII(kListenSwitch);
  if (address.empty()) {
    return;
  }
  net::IPAddress ip;
  if (!ip.AssignFromIPLiteral(address)) {
    LOG(ERROR) << "Shuttle: --" << kListenSwitch << "=" << address
               << " is not an IP address; listening on localhost only.";
    return;
  }
  if (ip.IsLoopback()) {
    return;  // Localhost: nothing to protect beyond the default checks.
  }
  std::string token = command_line.GetSwitchValueASCII(kAccessTokenSwitch);
  if (!token.empty() && !IsValidToken(token)) {
    LOG(ERROR) << "Shuttle: --" << kAccessTokenSwitch
               << " needs 16+ characters of [A-Za-z0-9_-]; listening on "
                  "localhost only.";
    return;
  }
  if (token.empty()) {
    token = LoadOrCreateToken(user_data_dir);
  }
  state.listen_address = address;
  state.token = token;
  LOG(WARNING) << "Shuttle: network mode, listening on " << address
               << ". Remote clients need the access token from "
               << user_data_dir.AppendASCII(kAccessTokenFileName).value()
               << " (or --" << kAccessTokenSwitch << ").";
}

const std::string& ListenAddress() {
  return GetState().listen_address;
}

const std::string& AccessToken() {
  return GetState().token;
}

std::string WithAccessToken(const std::string& url) {
  const std::string& token = AccessToken();
  if (token.empty()) {
    return url;
  }
  return url + (url.find('?') == std::string::npos ? "?token=" : "&token=") +
         token;
}

}  // namespace shuttle
