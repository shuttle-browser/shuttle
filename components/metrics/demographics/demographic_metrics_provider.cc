// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/demographics/demographic_metrics_provider.h"

#include <optional>

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "third_party/metrics_proto/ukm/report.pb.h"

namespace metrics {

// static
BASE_FEATURE(kDemographicMetricsReporting,
             base::FEATURE_ENABLED_BY_DEFAULT);

DemographicMetricsProvider::DemographicMetricsProvider(
    std::unique_ptr<ProfileClient> profile_client,
    MetricsLogUploader::MetricServiceType metrics_service_type)
    : profile_client_(std::move(profile_client)),
      metrics_service_type_(metrics_service_type) {
  DCHECK(profile_client_);
}

DemographicMetricsProvider::~DemographicMetricsProvider() = default;

std::optional<UserDemographics>
DemographicMetricsProvider::ProvideSyncedUserNoisedBirthYearAndGender() {
  // Skip if feature disabled.
  if (!base::FeatureList::IsEnabled(kDemographicMetricsReporting))
    return std::nullopt;

#if !BUILDFLAG(IS_CHROMEOS)
  // Skip if not exactly one Profile on disk. Having more than one Profile that
  // is using the browser can make demographics less relevant. This approach
  // cannot determine if there is more than 1 distinct user using the Profile.

  // ChromeOS almost always has more than one profile on disk, so this check
  // doesn't work. We have a profile selection strategy for ChromeOS, so skip
  // this check for ChromeOS.
  if (profile_client_->GetNumberOfProfilesOnDisk() != 1) {
    LogUserDemographicsStatusInHistogram(
        UserDemographicsStatus::kMoreThanOneProfile);
    return std::nullopt;
  }
#endif  // !BUILDFLAG(IS_CHROMEOS)

  // Demographics are only reported for users with an upload-capable sync
  // account. There is no sync service in this browser, so they are never
  // reported.
  LogUserDemographicsStatusInHistogram(UserDemographicsStatus::kNoSyncService);
  return std::nullopt;
}

void DemographicMetricsProvider::ProvideCurrentSessionData(
    ChromeUserMetricsExtension* uma_proto) {
  ProvideSyncedUserNoisedBirthYearAndGender(uma_proto);
}

void DemographicMetricsProvider::
    ProvideSyncedUserNoisedBirthYearAndGenderToReport(ukm::Report* report) {
  ProvideSyncedUserNoisedBirthYearAndGender(report);
}

void DemographicMetricsProvider::LogUserDemographicsStatusInHistogram(
    UserDemographicsStatus status) {
  switch (metrics_service_type_) {
    case MetricsLogUploader::MetricServiceType::UMA:
      base::UmaHistogramEnumeration("UMA.UserDemographics.Status", status);
      // If the user demographics data was retrieved successfully, then the user
      // must be between the ages of |kUserDemographicsMinAgeInYears|+1=21 and
      // |kUserDemographicsMaxAgeInYears|=85, so the user is not a minor.
      base::UmaHistogramBoolean("UMA.UserDemographics.IsNoisedAgeOver21Under85",
                                status == UserDemographicsStatus::kSuccess);
      return;
    case MetricsLogUploader::MetricServiceType::UKM:
      // UKM Metrics doesn't have demographic metrics.
      return;
    case MetricsLogUploader::MetricServiceType::STRUCTURED_METRICS:
      // Structured Metrics doesn't have demographic metrics.
      return;
    case MetricsLogUploader::MetricServiceType::DWA:
      // DWA doesn't have demographic metrics.
      return;
    case MetricsLogUploader::MetricServiceType::PRIVATE_METRICS:
      // Private Metrics doesn't have demographic metrics.
      return;
  }
  NOTREACHED();
}

}  // namespace metrics
