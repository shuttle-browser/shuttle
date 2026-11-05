// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/chrome_supervised_user_service_platform_delegate_base.h"

#include "base/check_deref.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/profile_metrics_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/channel_info.h"
#include "components/variations/service/variations_service.h"
#include "content/public/browser/storage_partition.h"

ChromeSupervisedUserServicePlatformDelegateBase::
    ChromeSupervisedUserServicePlatformDelegateBase(Profile& profile)
    : profile_(profile) {
}

ChromeSupervisedUserServicePlatformDelegateBase::
    ~ChromeSupervisedUserServicePlatformDelegateBase() = default;

std::string ChromeSupervisedUserServicePlatformDelegateBase::GetCountryCode()
    const {
  std::string country;
  variations::VariationsService* variations_service =
      g_browser_process->variations_service();
  if (variations_service) {
    country = variations_service->GetStoredPermanentCountry();
    if (country.empty()) {
      country = variations_service->GetLatestCountry();
    }
  }
  return country;
}

version_info::Channel
ChromeSupervisedUserServicePlatformDelegateBase::GetChannel() const {
  return chrome::GetChannel();
}
