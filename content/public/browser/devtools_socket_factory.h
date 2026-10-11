// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_DEVTOOLS_SOCKET_FACTORY_H_
#define CONTENT_PUBLIC_BROWSER_DEVTOOLS_SOCKET_FACTORY_H_

#include <string>

#include "net/socket/server_socket.h"

namespace content {

// Factory of net::ServerSocket. This is to separate instantiating dev tools
// and instantiating server sockets.
// All methods including destructor are called on a separate thread
// different from any BrowserThread instance.
class DevToolsSocketFactory {
 public:
  virtual ~DevToolsSocketFactory() {}

  // Returns a new instance of ServerSocket or nullptr if an error occurred.
  virtual std::unique_ptr<net::ServerSocket> CreateForHttpServer() = 0;

  // Creates a named socket for reversed tethering implementation (used with
  // remote debugging, primarily for mobile).
  virtual std::unique_ptr<net::ServerSocket> CreateForTethering(
      std::string* out_name) = 0;

  // Shuttle: network mode. When non-empty, clients that don't connect over
  // loopback must present this token (Authorization: Bearer <token> or a
  // token=<token> query parameter). Called once, on the handler thread.
  virtual std::string GetAccessToken();
  };

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_DEVTOOLS_SOCKET_FACTORY_H_
