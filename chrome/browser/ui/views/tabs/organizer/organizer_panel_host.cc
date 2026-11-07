// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/organizer/organizer_panel_host.h"

#include "base/check.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/organizer/organizer_panel_utils.h"
#include "chrome/browser/ui/views/interaction/browser_elements_views.h"
#include "chrome/browser/ui/views/tabs/organizer/organizer_tray_view.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"

namespace {

OrganizerPanelHost* GetOrganizerTrayHost(BrowserWindowInterface& browser) {
  return OrganizerPanelHost::FromView(
      BrowserElementsViews::From(&browser)->GetView(
          OrganizerTrayView::kTrayElementId, /*require_visible=*/false));
}

}  // namespace

// static
OrganizerPanelHost* OrganizerPanelHost::FromView(views::View* view) {
  CHECK(view);
  if (auto* instance = views::AsViewClass<OrganizerPanelHostView>(view)) {
    return instance;
  }
  if (auto* tray = views::AsViewClass<OrganizerTrayView>(view)) {
    return tray;
  }
  if (view->parent()) {
    return FromView(view->parent());
  }
  return nullptr;
}

// static
OrganizerPanelLocation OrganizerPanelHost::GetPreferredLocation(
    BrowserWindowInterface& browser) {
  if (browser.GetType() != BrowserWindowInterface::TYPE_NORMAL ||
      !organizer_panel::IsOrganizerPanelFeatureEnabled()) {
    return OrganizerPanelLocation::kNone;
  }
  return OrganizerPanelLocation::kOrganizerTray;
}

// static
OrganizerPanelHost* OrganizerPanelHost::GetHostForLocation(
    BrowserWindowInterface& browser,
    OrganizerPanelLocation location) {
  switch (location) {
    case OrganizerPanelLocation::kNone:
      return nullptr;
    case OrganizerPanelLocation::kOrganizerTray:
      return GetOrganizerTrayHost(browser);
  }
}

OrganizerPanelHostView::OrganizerPanelHostView() = default;
OrganizerPanelHostView::~OrganizerPanelHostView() = default;

BEGIN_METADATA(OrganizerPanelHostView)
END_METADATA
