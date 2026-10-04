// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/profiles/profile_error_dialog.h"

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_functions.h"
#include "base/notimplemented.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/ui/simple_message_box.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

}  // namespace

void ShowProfileErrorDialog(ProfileErrorType type,
                            int message_id,
                            const std::string& diagnostics) {
#if BUILDFLAG(IS_ANDROID)
  NOTIMPLEMENTED();
#else  // BUILDFLAG(IS_ANDROID)
  base::UmaHistogramEnumeration("Profile.ProfileError2", type);
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kNoErrorDialogs)) {
    return;
  }

  chrome::ShowWarningMessageBoxAsync(
      gfx::NativeWindow(),
      l10n_util::GetStringUTF16(IDS_PROFILE_ERROR_DIALOG_TITLE),
      l10n_util::GetStringUTF16(message_id));
#endif  // BUILDFLAG(IS_ANDROID)
}
