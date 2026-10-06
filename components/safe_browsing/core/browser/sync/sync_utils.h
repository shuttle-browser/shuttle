// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_SYNC_SYNC_UTILS_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_SYNC_SYNC_UTILS_H_

namespace signin {
class IdentityManager;
}

namespace safe_browsing {

// This class implements sync and signin-related safe browsing utilities.
class SyncUtils {
 public:
  SyncUtils() = delete;
  ~SyncUtils() = delete;

  // Returns true if signin is configured such that access token fetches for
  // safe browsing can be enabled. There is no sync, so this requires enhanced
  // protection and a signed-in primary account.
  static bool AreSigninAndSyncSetUpForSafeBrowsingTokenFetches(
      signin::IdentityManager* identity_manager,
      bool user_has_enabled_enhanced_protection);

  // Whether the primary account is signed in. Sync is not required.
  static bool IsPrimaryAccountSignedIn(
      signin::IdentityManager* identity_manager);
};  // class SyncUtils

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_SYNC_SYNC_UTILS_H_
