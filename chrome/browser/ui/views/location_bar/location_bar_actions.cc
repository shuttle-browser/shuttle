// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/location_bar_actions.h"

#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/omnibox/clipboard_utils.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "ui/actions/actions.h"

namespace content {

class WebContents;

}  // namespace content

namespace {

LocationBar* GetLocationBarForActions(BrowserWindowInterface* browser) {
  BrowserWindow* browser_window = BrowserWindow::FromBrowser(browser);
  return browser_window ? browser_window->GetLocationBar() : nullptr;
}

void ExecutePasteAndGo(BrowserWindowInterface* browser,
                       actions::ActionItem* item,
                       actions::ActionInvocationContext context) {
  LocationBar* const location_bar = GetLocationBarForActions(browser);
  if (!location_bar || !location_bar->GetOmniboxView()) {
    return;
  }
  GetClipboardText(
      /*notify_if_restricted=*/true,
      base::BindOnce(
          [](base::WeakPtr<BrowserWindowInterface> bwi, std::u16string text) {
            if (!bwi) {
              return;
            }
            LocationBar* location_bar = GetLocationBarForActions(bwi.get());
            if (location_bar && location_bar->GetOmniboxView()) {
              if (auto* controller = location_bar->GetOmniboxController()) {
                controller->edit_model()->PasteAndGo(text);
              }
            }
          },
          browser->GetWeakPtr()));
}

}  // namespace

void RegisterOmniboxActions(
    BrowserWindowInterface* browser) {
  if (!browser) {
    return;
  }

  auto* browser_actions = BrowserActions::From(browser);
  if (!browser_actions) {
    return;
  }

  browser_actions->RegisterAction(
      actions::ActionItem::Builder(
          base::BindRepeating(&ExecutePasteAndGo, base::Unretained(browser)))
          .SetActionId(kActionPasteAndGo)
          .Build());
}
