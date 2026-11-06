// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_CRITICAL_USER_JOURNEYS_FEATURES_H_
#define CHROME_BROWSER_METRICS_CRITICAL_USER_JOURNEYS_FEATURES_H_

#include "base/feature_list.h"

namespace metrics {

BASE_DECLARE_FEATURE(kCriticalUserJourneyService);

// Dedicated feature flags for each journey.
BASE_DECLARE_FEATURE(kClearBrowsingHistoryJourney);
BASE_DECLARE_FEATURE(kViewDownloadedFileJourney);
BASE_DECLARE_FEATURE(kViewDownloadedFileFromAppMenuJourney);
BASE_DECLARE_FEATURE(kPinExtensionJourney);
BASE_DECLARE_FEATURE(kSettingsGlowupJourneys);
BASE_DECLARE_FEATURE(kSettingsAppearanceThemesJourney);
BASE_DECLARE_FEATURE(kSettingsClearBrowsingDataJourney);
BASE_DECLARE_FEATURE(kSettingsNavIAJourney);
BASE_DECLARE_FEATURE(kSettingsSearchEngineJourney);
BASE_DECLARE_FEATURE(kSettingsSitesDashboardJourney);
BASE_DECLARE_FEATURE(kSettingsSuperchargedSearchJourney);
BASE_DECLARE_FEATURE(kCustomizeChromeJourney);
BASE_DECLARE_FEATURE(kReadAnythingJourney);
BASE_DECLARE_FEATURE(kTabSearchJourney);

}  // namespace metrics

#endif  // CHROME_BROWSER_METRICS_CRITICAL_USER_JOURNEYS_FEATURES_H_