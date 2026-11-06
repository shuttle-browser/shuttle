// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/history/history_ui.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/page_image_service/image_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/cr_components/history/history_util.h"
#include "chrome/browser/ui/webui/favicon_source.h"
#include "chrome/browser/ui/webui/history/browsing_history_handler.h"
#include "chrome/browser/ui/webui/history/navigation_handler.h"
#include "chrome/browser/ui/webui/managed_ui_handler.h"
#include "chrome/browser/ui/webui/metrics_handler.h"
#include "chrome/browser/ui/webui/theme_source.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/history_resources.h"
#include "chrome/grit/history_resources_map.h"
#include "components/grit/components_scaled_resources.h"
#include "components/history/core/browser/features.h"
#include "components/history/core/common/pref_names.h"
#include "components/page_image_service/image_service.h"
#include "components/page_image_service/image_service_handler.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/base/window_open_disposition.h"
#include "ui/webui/tracked_element/tracked_element_handler_document_singleton.h"
#include "ui/webui/webui_util.h"

namespace {

content::WebUIDataSource* CreateAndAddHistoryUIHTMLSource(Profile* profile) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile, chrome::kChromeUIHistoryHost);

  HistoryUtil::PopulateCommonSourceForHistory(source, profile);

  // The history page footer can display messages about other forms of
  // browsing history, linking to Google My Activity (GMA) and/or
  // Gemini Apps Activity (GAA). At most one message is shown, depending on
  // the user's settings.
  source->AddString(
      "sidebarFooterGMAOnly",
      l10n_util::GetStringFUTF16(IDS_HISTORY_OTHER_FORMS_OF_HISTORY_GMA_ONLY,
                                 chrome::kMyActivityUrlInHistory));
  source->AddString(
      "sidebarFooterGAAOnly",
      l10n_util::GetStringFUTF16(IDS_HISTORY_OTHER_FORMS_OF_HISTORY_GAA_ONLY,
                                 chrome::kMyActivityGeminiAppsUrl));
  source->AddString(
      "sidebarFooterGMAAndGAA",
      l10n_util::GetStringFUTF16(
          IDS_HISTORY_OTHER_FORMS_OF_HISTORY_GMA_AND_GAA,
          chrome::kMyActivityUrlInHistory, chrome::kMyActivityGeminiAppsUrl));
  // Links that are used in the messages above.
  source->AddString("sidebarFooterGMALink", chrome::kMyActivityUrlInHistory);
  source->AddString("sidebarFooterGAALink", chrome::kMyActivityGeminiAppsUrl);

  source->AddBoolean("isGlicEnabled", false);
  source->AddBoolean("isGlicWebActuationAvailable", false);

  // Shuttle: no sync device info, so no filtering by device.
  source->AddBoolean("isBrowsingHistoryFilterByDeviceEnabled", false);
  source->AddBoolean("isCriticalActionsEnabled", false);
  source->AddBoolean("isCriticalActionsChatLinkoutsEnabled", false);

  source->AddString("webuiRefresh2026", features::IsWebuiRefresh2026Enabled()
                                            ? "webui-refresh-2026"
                                            : "");

  return source;
}

}  // namespace

HistoryUIConfig::HistoryUIConfig()
    : WebUIConfig(content::kChromeUIScheme, chrome::kChromeUIHistoryHost) {}

HistoryUIConfig::~HistoryUIConfig() = default;

std::unique_ptr<content::WebUIController>
HistoryUIConfig::CreateWebUIController(content::WebUI* web_ui,
                                       const GURL& url) {
  return std::make_unique<HistoryUI>(web_ui);
}

#if BUILDFLAG(IS_ANDROID)
bool HistoryUIConfig::IsWebUIEnabled(content::BrowserContext* browser_context) {
  // LINT.IfChange(AndroidDesktopWebUiHistory)
  return base::android::device_info::is_desktop() &&
         base::FeatureList::IsEnabled(
             chrome::android::kAndroidDesktopWebUiHistory);
  // LINT.ThenChange(//chrome/browser/history/java/src/org/chromium/chrome/browser/history/HistoryFeatures.java:AndroidDesktopWebUiHistory,
  // //chrome/browser/android/ntp/new_tab_page_url_handler.cc:AndroidDesktopWebUiHistory)
}
#endif  // BUILDFLAG(IS_ANDROID)

HistoryUI::HistoryUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui, /*enable_chrome_send=*/true) {
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource* data_source =
      CreateAndAddHistoryUIHTMLSource(profile);
  ManagedUIHandler::Initialize(web_ui, data_source);

  content::URLDataSource::Add(profile, std::make_unique<ThemeSource>(profile));

  web_ui->AddMessageHandler(std::make_unique<webui::NavigationHandler>());
  web_ui->AddMessageHandler(std::make_unique<MetricsHandler>());
}

HistoryUI::~HistoryUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(HistoryUI)

// static
scoped_refptr<base::RefCountedMemory> HistoryUI::GetFaviconResourceBytes(
    ui::ResourceScaleFactor scale_factor) {
  return ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytesForScale(
      IDR_HISTORY_FAVICON, scale_factor);
}

void HistoryUI::BindInterface(
    mojo::PendingReceiver<history::mojom::PageHandler> pending_page_handler) {
  browsing_history_handler_ = std::make_unique<BrowsingHistoryHandler>(
      std::move(pending_page_handler), Profile::FromWebUI(web_ui()),
      web_ui()->GetWebContents());
}

void HistoryUI::BindInterface(
    mojo::PendingReceiver<page_image_service::mojom::PageImageServiceHandler>
        pending_page_handler) {
  base::WeakPtr<page_image_service::ImageService> image_service_weak;
  if (auto* image_service =
          page_image_service::ImageServiceFactory::GetForBrowserContext(
              Profile::FromWebUI(web_ui()))) {
    image_service_weak = image_service->GetWeakPtr();
  }
  image_service_handler_ =
      std::make_unique<page_image_service::ImageServiceHandler>(
          std::move(pending_page_handler), std::move(image_service_weak));
}
