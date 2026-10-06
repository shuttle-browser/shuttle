// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/sync/sync_utils.h"

#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"

namespace safe_browsing {

// static
bool SyncUtils::IsPrimaryAccountSignedIn(
    signin::IdentityManager* identity_manager) {
  CoreAccountInfo primary_account_info =
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  return !primary_account_info.account_id.empty();
}

// static
bool SyncUtils::AreSigninAndSyncSetUpForSafeBrowsingTokenFetches(
    signin::IdentityManager* identity_manager,
    bool user_has_enabled_enhanced_protection) {
  return user_has_enabled_enhanced_protection &&
         IsPrimaryAccountSignedIn(identity_manager);
}

}  // namespace safe_browsing
