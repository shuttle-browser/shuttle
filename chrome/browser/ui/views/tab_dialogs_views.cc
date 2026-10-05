// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tab_dialogs_views.h"

#include <memory>
#include <utility>

#include "build/build_config.h"
#include "chrome/browser/ui/views/hung_renderer_view.h"
#include "chrome/browser/ui/views/site_data/page_specific_site_data_dialog_controller.h"
#include "content/public/browser/web_contents.h"

#if defined(TOOLKIT_VIEWS) && !BUILDFLAG(IS_CHROMEOS)
#endif

// static
void TabDialogs::CreateForWebContents(content::WebContents* contents) {
  DCHECK(contents);
  if (!FromWebContents(contents)) {
    contents->SetUserData(UserDataKey(),
                          std::make_unique<TabDialogsViews>(contents));
  }
}

TabDialogsViews::TabDialogsViews(content::WebContents* contents)
    : web_contents_(contents) {
  DCHECK(contents);
}

TabDialogsViews::~TabDialogsViews() = default;

gfx::NativeView TabDialogsViews::GetDialogParentView() const {
  return web_contents_->GetNativeView();
}

void TabDialogsViews::ShowCollectedCookies() {
  PageSpecificSiteDataDialogController::CreateAndShowForWebContents(
      web_contents_);
}

void TabDialogsViews::ShowHungRendererDialog(
    content::RenderWidgetHost* render_widget_host,
    base::RepeatingClosure hang_monitor_restarter) {
  HungRendererDialogView::Show(web_contents_, render_widget_host,
                               std::move(hang_monitor_restarter));
}

void TabDialogsViews::HideHungRendererDialog(
    content::RenderWidgetHost* render_widget_host) {
  HungRendererDialogView::Hide(web_contents_, render_widget_host);
}

bool TabDialogsViews::IsShowingHungRendererDialog() {
  return HungRendererDialogView::IsShowingForWebContents(web_contents_);
}

void TabDialogsViews::ShowDeprecatedAppsDialog(
    const extensions::ExtensionId& optional_launched_extension_id,
    const std::set<extensions::ExtensionId>& deprecated_app_ids,
    content::WebContents* web_contents) {
  // Deprecated apps dialogs were removed from this build.
}

void TabDialogsViews::ShowForceInstalledDeprecatedAppsDialog(
    const extensions::ExtensionId& app_id,
    content::WebContents* web_contents) {
  // Deprecated apps dialogs were removed from this build.
}

void TabDialogsViews::ShowForceInstalledPreinstalledDeprecatedAppDialog(
    const extensions::ExtensionId& extension_id,
    content::WebContents* web_contents) {
  // Deprecated apps dialogs were removed from this build.
}
