// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/profile_helper.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/delete_profile_helper.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/ui/startup/startup_types.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"

namespace webui {

void OpenNewWindowForProfile(Profile* profile) {
  profiles::FindOrCreateNewWindowForProfile(
      profile, chrome::startup::IsProcessStartup::kYes,
      chrome::startup::IsFirstRun::kYes, false);
}

void DeleteProfileAtPath(base::FilePath file_path,
                         ProfileMetrics::ProfileDelete deletion_source) {
  if (!profiles::IsMultipleProfilesEnabled()) {
    return;
  }
  g_browser_process->profile_manager()
      ->GetDeleteProfileHelper()
      .MaybeScheduleProfileForDeletion(
          file_path, base::BindOnce(&OpenNewWindowForProfile), deletion_source);
}

}  // namespace webui
