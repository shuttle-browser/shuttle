// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_SIGNIN_UTIL_H_
#define CHROME_BROWSER_SIGNIN_SIGNIN_UTIL_H_

#include <optional>
#include <string>

#include "base/containers/enum_set.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/supports_user_data.h"
#include "build/build_config.h"
#include "components/policy/core/browser/signin/profile_separation_policies.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#include "components/signin/public/identity_manager/tribool.h"
#include "net/base/schemeful_site.h"
#include "ui/base/interaction/element_identifier.h"

class GaiaId;
class Profile;
class BrowserWindowInterface;

namespace signin {
class IdentityManager;
}

namespace signin_util {

DECLARE_ELEMENT_IDENTIFIER_VALUE(kSigninErrorDialogId);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kSigninErrorDialogOkButtonId);

enum class ProfileSeparationPolicyState {
  kEnforcedByExistingProfile,
  kEnforcedByInterceptedAccount,
  kEnforcedOnMachineLevel,
  kKeepsBrowsingData,
  kMaxValue = kKeepsBrowsingData
};

// Enum used to share the sign in state with the WebUI.
enum class SignedInState {
  kSignedOut = 0,
  kSignedIn = 1,
  kSyncing = 2,
  kSignInPending = 3,
  kWebOnlySignedIn = 4,
  kSyncPaused = 5,
};

using ProfileSeparationPolicyStateSet =
    base::EnumSet<ProfileSeparationPolicyState,
                  ProfileSeparationPolicyState::kEnforcedByExistingProfile,
                  ProfileSeparationPolicyState::kMaxValue>;

using PrimaryAccountError = signin::PrimaryAccountMutator::PrimaryAccountError;

// This class calls ResetForceSigninForTesting when destroyed, so that
// ForcedSigning doesn't leak across tests.
class ScopedForceSigninSetterForTesting {
 public:
  explicit ScopedForceSigninSetterForTesting(bool enable);
  ~ScopedForceSigninSetterForTesting();
  ScopedForceSigninSetterForTesting(const ScopedForceSigninSetterForTesting&) =
      delete;
  ScopedForceSigninSetterForTesting& operator=(
      const ScopedForceSigninSetterForTesting&) = delete;
};

// Return whether the force sign in policy is enabled or not.
// The state of this policy will not be changed without relaunch Chrome.
bool IsForceSigninEnabled();

// Enable or disable force sign in for testing. Please use
// ScopedForceSigninSetterForTesting instead, if possible. If not, make sure
// ResetForceSigninForTesting is called before the test finishes.
void SetForceSigninForTesting(bool enable);

// Reset force sign in to uninitialized state for testing.
void ResetForceSigninForTesting();

// Returns true if profile deletion is allowed.
bool IsProfileDeletionAllowed(Profile* profile);

#if !BUILDFLAG(IS_ANDROID)
#if !BUILDFLAG(IS_CHROMEOS)

// Returns true if managed accounts signin are required to create a new profile
// by policies set in `profile`. This will check the by default check the
// ManagedAccountsSigninRestriction policy.
// The optional `intercepted_account_email` will trigger a check to the
// ProfileSeparationDomainExceptionList policy. Unless
// `intercepted_account_email` is not available, it should always be passed.
bool IsProfileSeparationEnforcedByProfile(
    Profile* profile,
    std::string_view intercepted_account_email);

// Returns true if profile separation is enforced by
// `intercepted_account_separation_policies`.
bool IsProfileSeparationEnforcedByPolicies(
    const policy::ProfileSeparationPolicies&
        intercepted_profile_separation_policies);

bool ProfileSeparationAllowsKeepingUnmanagedBrowsingDataInManagedProfile(
    Profile* profile,
    const policy::ProfileSeparationPolicies&
        intercepted_profile_separation_policies);

bool IsAccountExemptedFromEnterpriseProfileSeparation(Profile* profile,
                                                      std::string_view email);
#endif  // !BUILDFLAG(IS_CHROMEOS)
// Records a UMA metric if the user accepts or not to create an enterprise
// profile.
void RecordEnterpriseProfileCreationUserChoice(bool enforced_by_policy,
                                               bool created);
#endif  // !BUILDFLAG(IS_ANDROID)

// TODO(b/339214136): Add a standalone unit for this function.
// Add an account with `user_email` and `gaia_id` to `profile`, and then set it
// as the primary account. A invalid refresh token will be set to mimic the
// behavior of a signed-out user. It is expected that the user is not tracked
// yet.
PrimaryAccountError SetPrimaryAccountWithInvalidToken(
    Profile* profile,
    const std::string& user_email,
    const GaiaId& gaia_id,
    bool is_under_advanced_protection,
    signin_metrics::AccessPoint access_point,
    signin_metrics::SourceForRefreshTokenOperation source);

// Returns true if the Chrome is signed into with an account that is in
// persistent error state. Always return false for Syncing users, even if in
// error state.
bool IsSigninPending(signin::IdentityManager* identity_manager);

// Returns the current state of the primary account that is used in Chrome.
SignedInState GetSignedInState(const signin::IdentityManager* identity_manager);

// Returns a string representation of `SignedInState`.
std::string SignedInStateToString(SignedInState state);

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
// Show a simple error message with an "OK" button to the user, displaying
// `error_message_id`.
void ShowErrorDialogWithMessage(BrowserWindowInterface* browser,
                                int error_message_id);

#endif  // BUILDFLAG(IS_LINUX) ||  BUILDFLAG(IS_MAC) ||  BUILDFLAG(IS_WIN)

// Returns the list of sites that register Device Bound Sessions that
// are of interest to signin code.
std::vector<net::SchemefulSite> GetDeviceBoundSessionRestrictedSites();

}  // namespace signin_util

#endif  // CHROME_BROWSER_SIGNIN_SIGNIN_UTIL_H_
