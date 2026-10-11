// Copyright 2026 Silkweave
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHUTTLE_NETWORK_ACCESS_H_
#define CHROME_BROWSER_SHUTTLE_NETWORK_ACCESS_H_

#include <string>

namespace base {
class FilePath;
}

namespace shuttle {

// Network mode: Shuttle serves MCP and CDP on another address than localhost
// (a LAN or Tailscale IP, or 0.0.0.0) so agents on other machines can share
// one browser. Off by default. Loopback clients keep working without a token;
// every other client must present the access token (Authorization: Bearer
// <token>, or ?token=<token> for clients that can't set headers).

// --shuttle-listen=<ip>: the address to listen on ("" = localhost only).
inline constexpr char kListenSwitch[] = "shuttle-listen";
// --shuttle-access-token=<token>: use this token instead of the stored one.
inline constexpr char kAccessTokenSwitch[] = "shuttle-access-token";
// The stored token, in the user data dir (created on first use, mode 0600).
inline constexpr char kAccessTokenFileName[] = "ShuttleAccessToken";

// Reads --shuttle-listen and, in network mode, loads or creates the access
// token. Call once at startup, before the DevTools server starts. Blocking.
void InitNetworkAccess(const base::FilePath& user_data_dir);

// The listen address ("" when network mode is off).
const std::string& ListenAddress();

// The access token ("" when network mode is off).
const std::string& AccessToken();

// Appends ?token=<token> (or &token=) to a ws:// or http:// URL in network
// mode, so the CDP endpoints handed to agents work from any machine.
std::string WithAccessToken(const std::string& url);

}  // namespace shuttle

#endif  // CHROME_BROWSER_SHUTTLE_NETWORK_ACCESS_H_
