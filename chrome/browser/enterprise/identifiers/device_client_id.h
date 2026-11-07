// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_IDENTIFIERS_DEVICE_CLIENT_ID_H_
#define CHROME_BROWSER_ENTERPRISE_IDENTIFIERS_DEVICE_CLIENT_ID_H_

#include <string>

namespace enterprise {

// Returns the device client id that ProfileIdService mixes into profile ids.
// Shuttle replacement for the removed `BrowserDMTokenStorage::RetrieveClientId()`
// and returns the same value so existing profile ids stay stable:
// - empty unless cloud management was enabled (Google Chrome branding, or the
//   --enable-chrome-browser-cloud-management / --use-fake-dmserver switches),
//   exactly like the old storage, which skipped client id initialisation;
// - Mac: the IOKit platform serial number;
// - Linux: base64url(SHA-1(/etc/machine-id)) without padding;
// - empty if the value is longer than 64 characters or contains a space or a
//   non-printable character.
// The value is computed once and cached.
std::string GetDeviceClientId();

}  // namespace enterprise

#endif  // CHROME_BROWSER_ENTERPRISE_IDENTIFIERS_DEVICE_CLIENT_ID_H_
