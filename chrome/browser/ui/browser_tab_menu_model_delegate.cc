// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_tab_menu_model_delegate.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tab_strip_model_delegate.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"

namespace chrome {

BrowserTabMenuModelDelegate::BrowserTabMenuModelDelegate(
    SessionID session_id,
    const Profile* profile,
    ui::UnownedUserDataHost& host)
    : TabMenuModelDelegate(host),
      session_id_(session_id),
      profile_(profile) {}

BrowserTabMenuModelDelegate::~BrowserTabMenuModelDelegate() = default;

std::vector<BrowserWindowInterface*>
BrowserTabMenuModelDelegate::GetOtherBrowserWindows(bool is_app) {
  std::vector<BrowserWindowInterface*> browsers;

  ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
      [&](BrowserWindowInterface* browser) {
        // We can only move into a tabbed view of the same profile, and not the
        // same window we're currently in.
        if (browser->GetSessionID() != session_id_ &&
            browser->GetProfile() == profile_) {
          if (!is_app &&
              browser->GetType() == BrowserWindowInterface::TYPE_NORMAL) {
            browsers.push_back(browser);
          }
        }
        return true;  // continue iterating
      });
  return browsers;
}

}  // namespace chrome
