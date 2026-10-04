// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/window_feature_controller/window_feature_controller.h"

#include "base/check.h"
#include "base/check_deref.h"
#include "build/build_config.h"
#include "chrome/browser/ui/fullscreen/browser_window_fullscreen_controller.h"

DEFINE_USER_DATA(WindowFeatureController);

WindowFeatureController::WindowFeatureController(
    BrowserWindowFullscreenController* fullscreen_controller,
    BrowserWindowInterface::Type browser_type,
    bool is_trusted_source,
    ui::UnownedUserDataHost& host)
    : fullscreen_controller_(CHECK_DEREF(fullscreen_controller)),
      browser_type_(browser_type),
      is_trusted_source_(is_trusted_source),
      scoped_unowned_user_data_(host, *this) {}

WindowFeatureController::~WindowFeatureController() = default;

// static
WindowFeatureController* WindowFeatureController::From(
    BrowserWindowInterface* browser) {
  return ui::ScopedUnownedUserData<WindowFeatureController>::Get(
      browser->GetUnownedUserDataHost());
}

// static
const WindowFeatureController* WindowFeatureController::From(
    const BrowserWindowInterface* browser) {
  return ui::ScopedUnownedUserData<WindowFeatureController>::Get(
      browser->GetUnownedUserDataHost());
}

bool WindowFeatureController::SupportsWindowFeature(
    WindowFeature feature) const {
  bool supports =
      SupportsWindowFeatureImpl(feature, /*check_can_support=*/false);
  // Supported features imply CanSupportWindowFeature.
  DCHECK(!supports || CanSupportWindowFeature(feature));
  return supports;
}

bool WindowFeatureController::CanSupportWindowFeature(
    WindowFeature feature) const {
  return SupportsWindowFeatureImpl(feature, /*check_can_support=*/true);
}

bool WindowFeatureController::IsTrustedSource() const {
  return is_trusted_source_;
}

#if BUILDFLAG(IS_MAC)
bool WindowFeatureController::UsesImmersiveFullscreenMode() const {
  return CanSupportWindowFeature(WindowFeature::kFeatureTabStrip);
}

bool WindowFeatureController::UsesImmersiveFullscreenTabbedMode() const {
  return CanSupportWindowFeature(WindowFeature::kFeatureTabStrip);
}
#endif

bool WindowFeatureController::NormalBrowserSupportsWindowFeature(
    WindowFeature feature,
    bool check_can_support) const {
  switch (feature) {
    case WindowFeature::kFeatureTabStrip:
    case WindowFeature::kFeatureToolbar:
    case WindowFeature::kFeatureLocationBar:
      return check_can_support || !IsFullscreen();
    case WindowFeature::kFeatureTitleBar:
    case WindowFeature::kFeatureBookmarkBar:
    case WindowFeature::kFeatureNone:
      return false;
  }
}

bool WindowFeatureController::PopupBrowserSupportsWindowFeature(
    WindowFeature feature,
    bool check_can_support) const {
  switch (feature) {
    case WindowFeature::kFeatureTitleBar:
    case WindowFeature::kFeatureLocationBar:
      return check_can_support || (!IsFullscreen() && !is_trusted_source_);
    case WindowFeature::kFeatureTabStrip:
    case WindowFeature::kFeatureToolbar:
    case WindowFeature::kFeatureBookmarkBar:
    case WindowFeature::kFeatureNone:
      return false;
  }
}

bool WindowFeatureController::AppPopupBrowserSupportsWindowFeature(
    WindowFeature feature,
    bool check_can_support) const {
  switch (feature) {
    case WindowFeature::kFeatureTitleBar:
      return check_can_support || !IsFullscreen();
    case WindowFeature::kFeatureLocationBar:
      return false;
    default:
      return PopupBrowserSupportsWindowFeature(feature, check_can_support);
  }
}


bool WindowFeatureController::PictureInPictureBrowserSupportsWindowFeature(
    WindowFeature feature,
    bool check_can_support) const {
  switch (feature) {
    case WindowFeature::kFeatureTitleBar:
      return true;
    case WindowFeature::kFeatureLocationBar:
    case WindowFeature::kFeatureTabStrip:
    case WindowFeature::kFeatureToolbar:
    case WindowFeature::kFeatureBookmarkBar:
    case WindowFeature::kFeatureNone:
      return false;
  }
}

bool WindowFeatureController::SupportsWindowFeatureImpl(
    WindowFeature feature,
    bool check_can_support) const {
  switch (browser_type_) {
    case BrowserWindowInterface::Type::TYPE_NORMAL:
      return NormalBrowserSupportsWindowFeature(feature, check_can_support);
    case BrowserWindowInterface::Type::TYPE_POPUP:
      return PopupBrowserSupportsWindowFeature(feature, check_can_support);
    case BrowserWindowInterface::Type::TYPE_APP:
    case BrowserWindowInterface::Type::TYPE_DEVTOOLS:
    case BrowserWindowInterface::Type::TYPE_APP_POPUP:
      return AppPopupBrowserSupportsWindowFeature(feature, check_can_support);
    case BrowserWindowInterface::Type::TYPE_PICTURE_IN_PICTURE:
      return PictureInPictureBrowserSupportsWindowFeature(feature,
                                                          check_can_support);
  }
}

bool WindowFeatureController::IsFullscreen() const {
  return fullscreen_controller_->ShouldHideUIForFullscreen();
}
