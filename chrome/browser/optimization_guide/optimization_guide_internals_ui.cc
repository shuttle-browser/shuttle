// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/optimization_guide_internals_ui.h"

#include <cstdint>
#include <memory>
#include <vector>

#include "base/hash/hash.h"
#include "base/i18n/time_formatting.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/grit/optimization_guide_internals_resources.h"
#include "components/grit/optimization_guide_internals_resources_map.h"
#include "components/optimization_guide/core/delivery/prediction_manager.h"
#include "components/optimization_guide/core/optimization_guide_prefs.h"
#include "components/optimization_guide/optimization_guide_internals/webui/optimization_guide_internals.mojom.h"
#include "components/optimization_guide/optimization_guide_internals/webui/optimization_guide_internals_page_handler_impl.h"
#include "components/optimization_guide/proto/model_execution.pb.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_data_source.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"
#include "ui/webui/webui_util.h"

bool OptimizationGuideInternalsUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  auto* service = OptimizationGuideKeyedServiceFactory::GetForProfile(profile);
  return service != nullptr;
}

OptimizationGuideInternalsUI::OptimizationGuideInternalsUI(
    content::WebUI* web_ui)
    : MojoWebUIController(web_ui, /*enable_chrome_send=*/true) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(),
      optimization_guide_internals::kChromeUIOptimizationGuideInternalsHost);
  webui::SetupWebUIDataSource(
      source, kOptimizationGuideInternalsResources,
      IDR_OPTIMIZATION_GUIDE_INTERNALS_OPTIMIZATION_GUIDE_INTERNALS_HTML);
}

OptimizationGuideInternalsUI::~OptimizationGuideInternalsUI() = default;

void OptimizationGuideInternalsUI::BindInterface(
    mojo::PendingReceiver<
        optimization_guide_internals::mojom::PageHandlerFactory> receiver) {
  // TODO(crbug.com/40215132): Remove the reset which is needed now since
  // |this| is reused on internals page reloads.
  optimization_guide_internals_page_factory_receiver_.reset();
  optimization_guide_internals_page_factory_receiver_.Bind(std::move(receiver));
}

void OptimizationGuideInternalsUI::CreatePageHandler(
    mojo::PendingRemote<optimization_guide_internals::mojom::Page> page) {
  Profile* profile = Profile::FromWebUI(web_ui());
  auto* service = OptimizationGuideKeyedServiceFactory::GetForProfile(profile);
  DCHECK(service);
  OptimizationGuideLogger* optimization_guide_logger =
      service->GetOptimizationGuideLogger();
  optimization_guide_internals_page_handler_ =
      std::make_unique<OptimizationGuideInternalsPageHandlerImpl>(
          std::move(page), optimization_guide_logger);
}

void OptimizationGuideInternalsUI::RequestDownloadedModelsInfo(
    RequestDownloadedModelsInfoCallback callback) {
  Profile* profile = Profile::FromWebUI(web_ui());
  auto* service = OptimizationGuideKeyedServiceFactory::GetForProfile(profile);
  optimization_guide::PredictionManager* prediction_manager =
      service->GetPredictionManager();
  std::vector<optimization_guide_internals::mojom::DownloadedModelInfoPtr>
      downloaded_models_info =
          prediction_manager->GetDownloadedModelsInfoForWebUI();
  std::move(callback).Run(std::move(downloaded_models_info));
}

void OptimizationGuideInternalsUI::RequestLoggedModelQualityClientIds(
    RequestLoggedModelQualityClientIdsCallback callback) {
  // Shuttle: model quality logging is removed.
  std::move(callback).Run({});
}

void OptimizationGuideInternalsUI::RequestMqlsLogs(
    RequestMqlsLogsCallback callback) {
  // Shuttle: model quality logging is removed.
  std::move(callback).Run({});
}

WEB_UI_CONTROLLER_TYPE_IMPL(OptimizationGuideInternalsUI)
