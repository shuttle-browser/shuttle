// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_process_platform_part_mac.h"

#include "base/apple/foundation_util.h"
#include "base/time/time.h"
#import "chrome/browser/app_controller_mac.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_browser_application_mac.h"
#include "chrome/browser/lifetime/application_lifetime_desktop.h"
#include "services/device/public/cpp/geolocation/system_geolocation_source_apple.h"

BrowserProcessPlatformPart::BrowserProcessPlatformPart() = default;

BrowserProcessPlatformPart::~BrowserProcessPlatformPart() = default;

void BrowserProcessPlatformPart::StartTearDown() {}

void BrowserProcessPlatformPart::AttemptExit(bool try_to_quit_application) {
  // On the Mac, the application continues to run once all windows are closed.
  // Terminate will result in a CloseAllBrowsers() call, and once (and if)
  // that is done, will cause the application to exit cleanly.
  //
  // This function is called for two types of attempted exits: URL requests
  // (chrome://quit or chrome://restart), and a keyboard menu invocations of
  // command-Q. (Interestingly, selecting the Quit command with the mouse don't
  // come down this code path at all.) URL requests to exit have
  // |try_to_quit_application| set to true; keyboard menu invocations have it
  // set to false.

  if (!try_to_quit_application) {
    // A keyboard menu invocation.
    if ([AppController.sharedController confirmQuitIfNeeded] ==
        ConfirmQuitResultAborted) {
      chrome::OnClosingAllBrowsers(false);
      return;
    }
  }

  chrome_browser_application_mac::Terminate();
}

void BrowserProcessPlatformPart::PreMainMessageLoopRun() {
  // Workaround for https://crbug.com/40155239: This needs to be created at
  // browser startup.
  if (!device::GeolocationSystemPermissionManager::GetInstance()) {
    device::GeolocationSystemPermissionManager::SetInstance(
        device::SystemGeolocationSourceApple::
            CreateGeolocationSystemPermissionManager());
  }
}

void BrowserProcessPlatformPart::PostDestroyThreads() {}
