// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/base/signin_client.h"


void SigninClient::PreSignOut(
    base::OnceCallback<void(SignoutDecision)> on_signout_decision_reached,
    signin_metrics::ProfileSignout signout_source_metric) {
  // Allow sign out to continue.
  std::move(on_signout_decision_reached)
      .Run(is_clear_primary_account_allowed_for_testing_.value_or(
          SignoutDecision::ALLOW));
}

bool SigninClient::IsClearPrimaryAccountAllowed() const {
  return is_clear_primary_account_allowed_for_testing_.value_or(
             SignoutDecision::ALLOW) == SignoutDecision::ALLOW;
}

bool SigninClient::is_clear_primary_account_allowed_for_testing() const {
  return is_clear_primary_account_allowed_for_testing_ ==
         SignoutDecision::ALLOW;
}

network::mojom::DeviceBoundSessionManager*
SigninClient::GetDeviceBoundSessionManager() const {
  return nullptr;
}
