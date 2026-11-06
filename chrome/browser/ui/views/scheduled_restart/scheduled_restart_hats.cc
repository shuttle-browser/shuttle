// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/scheduled_restart/scheduled_restart_hats.h"

// Shuttle: HaTS (Google surveys) is removed; the scheduled-restart surveys are
// no-ops.

namespace scheduled_restart {

void MaybeLaunchHatsSurvey(
    ScheduledRestartBubbleView::ScheduledRestartDialogChoice choice,
    BrowserWindowInterface* browser,
    bool show_active,
    base::TimeDelta duration) {}

void MaybeLaunchPendingHatsSurvey(Profile* profile) {}

}  // namespace scheduled_restart
