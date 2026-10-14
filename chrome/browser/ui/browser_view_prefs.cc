// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_view_prefs.h"

#include "build/build_config.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"

#if BUILDFLAG(IS_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif

#if BUILDFLAG(IS_LINUX)
#include <sys/utsname.h>

#include "base/strings/string_util.h"
#endif

namespace {

#if BUILDFLAG(IS_LINUX)
// Shuttle: WSL kernels report "...-microsoft-standard-WSL2" as their release.
bool IsRunningUnderWsl() {
  struct utsname name;
  return uname(&name) == 0 &&
         base::ToLowerASCII(name.release).find("microsoft") !=
             std::string::npos;
}

bool GetCustomFramePrefDefault() {
  // Shuttle: WSLg's compositor (Weston) isn't a window manager Chrome knows,
  // so the X11 default falls back to the system title bar: a light Weston
  // frame that ignores dark mode, and a window as tall as the whole screen.
  // Shuttle's own frame fits WSLg.
  if (IsRunningUnderWsl()) {
    return true;
  }
#if BUILDFLAG(IS_OZONE)
  return ui::OzonePlatform::GetInstance()
      ->GetPlatformProperties()
      .custom_frame_pref_default;
#else
  return false;
#endif  // BUILDFLAG(IS_OZONE)
}
#endif

}  // namespace

void RegisterBrowserViewProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
#if BUILDFLAG(IS_LINUX)
  registry->RegisterBooleanPref(prefs::kUseCustomChromeFrame,
                                GetCustomFramePrefDefault());
#endif
}
