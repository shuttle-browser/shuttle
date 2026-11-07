// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/chrome_content_settings_utils.h"

#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/location_bar/location_bar_override_data.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/web_contents.h"
#endif

namespace content_settings {

void RecordPopupsAction(PopupsAction action) {
  UMA_HISTOGRAM_ENUMERATION("ContentSettings.Popups", action,
                            POPUPS_ACTION_COUNT);
}

void UpdateLocationBarUiForWebContents(content::WebContents* web_contents) {
#if !BUILDFLAG(IS_ANDROID)
  if (!web_contents) {
    return;
  }

  // Secondary UI surfaces (e.g. the Contextual Tasks side panel) host a
  // `WebContents` that is not backed by a tab, so the browser/tab-strip lookup
  // below would not find them. They register their own `LocationBar` via
  // `LocationBarOverrideData`; route the refresh straight to it.
  if (auto* override_data =
          location_bar::LocationBarOverrideData::FromWebContents(
              web_contents)) {
    LocationBar* location_bar = override_data->GetLocationBar();
    if (location_bar && location_bar->GetWebContents() == web_contents) {
      location_bar->UpdateContentSettingsIcons();
    }
    return;
  }

  BrowserWindowInterface* browser =
      GlobalBrowserCollection::GetInstance()->FindBrowserWithTab(web_contents);
  if (!browser) {
    return;
  }

  if (browser->GetTabStripModel()->GetActiveWebContents() != web_contents) {
    return;
  }

  LocationBar* location_bar =
      BrowserWindow::FromBrowser(browser)->GetLocationBar();
  if (location_bar)
    location_bar->UpdateContentSettingsIcons();

#endif
}

}  // namespace content_settings
