// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/identifiers/device_client_id.h"

#include <algorithm>
#include <string>
#include <string_view>

#include "base/command_line.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "components/enterprise/browser/enterprise_switches.h"
#include "components/policy/core/common/policy_switches.h"

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#elif BUILDFLAG(IS_LINUX)
#include <stdio.h>

#include <array>

#include "base/base64url.h"
#include "base/files/scoped_file.h"
#include "crypto/obsolete/sha1.h"
#endif

#if BUILDFLAG(IS_LINUX)
namespace policy {

// Friend of crypto::obsolete::Sha1 (see crypto/obsolete/sha1.h); kept under
// its upstream name so the machine-id hash stays byte-identical.
std::array<uint8_t, crypto::obsolete::kSha1Size> Sha1ForMachineId(
    std::string_view input) {
  return crypto::obsolete::Sha1::Hash(input);
}

}  // namespace policy
#endif  // BUILDFLAG(IS_LINUX)

namespace enterprise {

namespace {

// Same condition as the removed ChromeBrowserCloudManagementController::
// IsEnabled(): without it, BrowserDMTokenStorage never initialised the client
// id and returned an empty string.
bool IsClientIdEnabled() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return true;
#else
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  return command_line->HasSwitch(
             ::switches::kEnableChromeBrowserCloudManagement) ||
         command_line->HasSwitch(policy::switches::kUseFakeDmserver);
#endif
}

std::string ReadPlatformClientId() {
#if BUILDFLAG(IS_MAC)
  // BrowserDMTokenStorageMac::InitClientId().
  return base::mac::GetPlatformSerialNumber();
#elif BUILDFLAG(IS_LINUX)
  // BrowserDMTokenStorageLinux::InitClientId(): the client id is derived from
  // /etc/machine-id, hashed and base64url-encoded (the raw id must not leave
  // the machine).
  constexpr size_t kMachineIdSize = 32;
  // A tiny file read once per process; plain stdio so a first call on the UI
  // thread does not trip the blocking-call assertions of base/files.
  std::string machine_id;
  {
    base::ScopedFILE file(fopen("/etc/machine-id", "r"));
    if (!file) {
      return std::string();
    }
    char buffer[128];
    size_t read = fread(buffer, 1, sizeof(buffer), file.get());
    machine_id.assign(buffer, read);
  }
  std::string_view machine_id_trimmed =
      base::TrimWhitespaceASCII(machine_id, base::TRIM_TRAILING);
  if (machine_id_trimmed.size() != kMachineIdSize) {
    return std::string();
  }
  std::string machine_id_base64;
  base::Base64UrlEncodeEarlyStartup(
      policy::Sha1ForMachineId(machine_id_trimmed),
      base::Base64UrlEncodePolicy::OMIT_PADDING, &machine_id_base64);
  return machine_id_base64;
#else
  return std::string();
#endif
}

std::string ComputeDeviceClientId() {
  if (!IsClientIdEnabled()) {
    return std::string();
  }
  std::string client_id = ReadPlatformClientId();
  // Validation from BrowserDMTokenStorage::InitIfNeeded().
  if (client_id.length() > 64 ||
      std::ranges::any_of(client_id, [](char ch) {
        return ch == ' ' || !base::IsAsciiPrintable(ch);
      })) {
    return std::string();
  }
  return client_id;
}

}  // namespace

std::string GetDeviceClientId() {
  static const base::NoDestructor<std::string> client_id(
      ComputeDeviceClientId());
  return *client_id;
}

}  // namespace enterprise
