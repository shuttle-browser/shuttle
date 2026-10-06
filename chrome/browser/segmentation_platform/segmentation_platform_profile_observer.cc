// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/segmentation_platform/segmentation_platform_profile_observer.h"

#include <vector>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/segmentation_platform/ukm_database_client.h"
#include "components/segmentation_platform/public/segmentation_platform_service.h"

namespace segmentation_platform {

SegmentationPlatformProfileObserver::SegmentationPlatformProfileObserver(
    SegmentationPlatformService* segmentation_platform_service,
    ProfileManager* profile_manager)
    : segmentation_platform_service_(segmentation_platform_service),
      profile_manager_(profile_manager) {
  profile_manager_observation_.Observe(profile_manager);
  for (auto* profile : profile_manager_->GetLoadedProfiles()) {
    OnProfileAdded(profile);
  }
}

SegmentationPlatformProfileObserver::~SegmentationPlatformProfileObserver() =
    default;

void SegmentationPlatformProfileObserver::OnProfileAdded(Profile* profile) {
  // There are no off the record profiles.
  NotifyExistenceOfOTRProfile(false);
}

void SegmentationPlatformProfileObserver::OnProfileManagerDestroying() {
  profile_manager_observation_.Reset();
  profile_manager_ = nullptr;
}

void SegmentationPlatformProfileObserver::NotifyExistenceOfOTRProfile(
    bool has_otr_profiles) {
  if (has_otr_profiles_.has_value() && has_otr_profiles_ == has_otr_profiles)
    return;

  has_otr_profiles_ = has_otr_profiles;
  segmentation_platform_service_->EnableMetrics(!has_otr_profiles_.value());
}

}  // namespace segmentation_platform
