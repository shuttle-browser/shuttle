// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/desktop_session_duration/desktop_profile_session_durations_service.h"

#include "components/signin/core/browser/signin_status_metrics_provider_helpers.h"

namespace metrics {

DesktopProfileSessionDurationsService::DesktopProfileSessionDurationsService(
    PrefService* pref_service,
    DesktopSessionDurationTracker* tracker)
    : msbb_metrics_recorder_(
          std::make_unique<
              unified_consent::MsbbSessionDurationsMetricsRecorder>(
              pref_service)) {
  session_duration_observation_.Observe(tracker);
  if (tracker->in_session()) {
    // The session was started before this service was created. Let's start
    // tracking now.
    OnSessionStarted(base::TimeTicks::Now());
  }
}

DesktopProfileSessionDurationsService::
    ~DesktopProfileSessionDurationsService() = default;

void DesktopProfileSessionDurationsService::Shutdown() {
  // The Profile is being destroyed, e.g. because the
  // DestroyProfileOnBrowserClose flag is enabled. Recorders expect every call
  // to OnSessionStarted() to have a corresponding OnSessionEnded().
  //
  // Use a |session_length| of zero, so each recorder can infer the duration
  // based on their internal state.
  OnSessionEnded(base::TimeDelta(), base::TimeTicks::Now());

  msbb_metrics_recorder_.reset();
}

signin_metrics::SingleProfileSigninStatus
DesktopProfileSessionDurationsService::GetSigninStatus() const {
  // There is no sign-in UI in this browser.
  return signin_metrics::SingleProfileSigninStatus::kSignedOut;
}

bool DesktopProfileSessionDurationsService::IsSyncing() const {
  return false;
}

void DesktopProfileSessionDurationsService::OnSessionStarted(
    base::TimeTicks session_start) {
  msbb_metrics_recorder_->OnSessionStarted(session_start);
}

void DesktopProfileSessionDurationsService::OnSessionEnded(
    base::TimeDelta session_length,
    base::TimeTicks session_end) {
  msbb_metrics_recorder_->OnSessionEnded(session_length);
}

}  // namespace metrics
