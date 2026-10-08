// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chrome_untrusted_web_ui_configs.h"

#include "build/android_buildflags.h"
#include "build/build_config.h"
#include "chrome/common/buildflags.h"
#include "content/public/browser/webui_config_map.h"
#include "printing/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
#include "chrome/browser/ui/webui/print_preview/print_preview_ui_untrusted.h"
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ui/webui/ash/config/chrome_untrusted_web_ui_configs_chromeos.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

#if !BUILDFLAG(IS_ANDROID)
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/android/lens/lens_overlay_untrusted_ui_android.h"
#endif  // BUILDFLAG(IS_ANDROID)

void RegisterChromeUntrustedWebUIConfigs() {
  // Don't add calls to `AddUntrustedWebUIConfig()` for ash-specific UIs here.
  // Add them in chrome_untrusted_web_ui_configs_chromeos.cc.
#if BUILDFLAG(IS_CHROMEOS)
  ash::RegisterAshChromeUntrustedWebUIConfigs();
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(ENABLE_PRINT_PREVIEW) || BUILDFLAG(IS_ANDROID)
  auto& map = content::WebUIConfigMap::GetInstance();
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW) || BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
  map.AddUntrustedWebUIConfig(
      std::make_unique<printing::PrintPreviewUIUntrustedConfig>());
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)

#if BUILDFLAG(IS_ANDROID)
  map.AddUntrustedWebUIConfig(
      std::make_unique<lens::LensOverlayUntrustedUIAndroidConfig>());
#endif  // BUILDFLAG(IS_ANDROID)
}
