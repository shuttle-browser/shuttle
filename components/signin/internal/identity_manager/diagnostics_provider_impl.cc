// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/internal/identity_manager/diagnostics_provider_impl.h"

#include "components/signin/internal/identity_manager/profile_oauth2_token_service.h"
#include "components/signin/internal/identity_manager/profile_oauth2_token_service_delegate.h"

namespace signin {

DiagnosticsProviderImpl::DiagnosticsProviderImpl(
    ProfileOAuth2TokenService* profile_oauth2_token_service)
    : profile_oauth2_token_service_(profile_oauth2_token_service) {
  DCHECK(profile_oauth2_token_service_);
}

DiagnosticsProviderImpl::~DiagnosticsProviderImpl() = default;

signin::LoadCredentialsState
DiagnosticsProviderImpl::GetDetailedStateOfLoadingOfRefreshTokens() const {
  DCHECK(profile_oauth2_token_service_->GetDelegate());
  return profile_oauth2_token_service_->GetDelegate()->load_credentials_state();
}

base::TimeDelta
DiagnosticsProviderImpl::GetDelayBeforeMakingAccessTokenRequests() const {
  const net::BackoffEntry* backoff_entry =
      profile_oauth2_token_service_->GetDelegateBackoffEntry();
  return backoff_entry ? backoff_entry->GetTimeUntilRelease()
                       : base::TimeDelta();
}

base::TimeDelta DiagnosticsProviderImpl::GetDelayBeforeMakingCookieRequests()
    const {
  // Gaia cookie requests are never made.
  return base::TimeDelta();
}

}  // namespace signin
