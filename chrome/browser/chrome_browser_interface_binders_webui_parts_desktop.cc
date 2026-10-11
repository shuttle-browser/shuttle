// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "chrome/browser/chrome_browser_interface_binders_webui_parts.h"
#include "chrome/browser/history_clusters/history_clusters_service_factory.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/color_pipeline_internals/color_pipeline_internals_ui.h"
#include "chrome/browser/ui/webui/downloads/downloads.mojom.h"
#include "chrome/browser/ui/webui/downloads/downloads_ui.h"
#include "chrome/browser/ui/webui/history/history_ui.h"
#include "chrome/browser/ui/webui/infobar_internals/infobar_internals.mojom.h"
#include "chrome/browser/ui/webui/infobar_internals/infobar_internals_ui.h"
#include "chrome/browser/ui/webui/metrics_reporter/metrics_reporter_service.h"
#include "chrome/browser/ui/webui/multistep_filter_internals/multistep_filter_internals.mojom.h"
#include "chrome/browser/ui/webui/multistep_filter_internals/multistep_filter_internals_ui.h"
#include "chrome/browser/ui/webui/omnibox/logging/logs.mojom.h"
#include "chrome/browser/ui/webui/omnibox/omnibox_ui.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_ui.h"
#include "chrome/browser/ui/webui/page_action_internals/page_action_internals.mojom.h"
#include "chrome/browser/ui/webui/page_action_internals/page_action_internals_ui.h"
#include "chrome/browser/ui/webui/settings/settings_ui.h"
#include "chrome/browser/ui/webui/webui_gallery/webui_gallery_ui.h"
#include "chrome/common/chrome_features.h"
#include "components/browser_apis/tab_drag/tab_drag_api.mojom.h"
#include "components/history_clusters/core/history_clusters_service.h"
#include "components/multistep_filter/core/features.h"
#include "components/omnibox/browser/searchbox.mojom.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/page_image_service/mojom/page_image_service.mojom.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_browser_interface_broker_registry.h"
#include "content/public/browser/web_ui_controller_interface_binder.h"
#include "mojo/public/cpp/bindings/binder_map.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "ui/webui/color_change_listener/color_change_handler.h"
#include "ui/webui/resources/cr_components/app_management/app_management.mojom.h"
#include "ui/webui/resources/cr_components/color_change_listener/color_change_listener.mojom.h"
#include "ui/webui/resources/cr_components/customize_color_scheme_mode/customize_color_scheme_mode.mojom.h"
#include "ui/webui/resources/cr_components/history/history.mojom.h"
#include "ui/webui/resources/cr_components/history_clusters/history_clusters.mojom.h"
#include "ui/webui/resources/cr_components/theme_color_picker/theme_color_picker.mojom.h"
#include "ui/webui/resources/js/browser_command/browser_command.mojom.h"

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
// The intro mojom targets exist on ChromeOS but these headers are only
// compiled on Win/Mac/Linux; gn check is static, so suppress it on ChromeOS.
#include "chrome/browser/ui/webui/updater/updater_ui.h"
#include "chrome/browser/ui/webui/updater/updater_ui.mojom.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/ui/webui/unexportable_keys_internals/unexportable_keys_internals.mojom.h"
#include "chrome/browser/ui/webui/unexportable_keys_internals/unexportable_keys_internals_ui.h"
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/webui/diagnostics_ui/diagnostics_ui.h"
#include "ash/webui/firmware_update_ui/firmware_update_app_ui.h"
#include "ash/webui/graduation/graduation_ui.h"
#include "ash/webui/os_feedback_ui/os_feedback_ui.h"
#include "ash/webui/personalization_app/personalization_app_ui.h"
#include "ash/webui/print_management/print_management_ui.h"
#include "ash/webui/sanitize_ui/sanitize_ui.h"
#include "ash/webui/scanning/scanning_ui.h"
#include "ash/webui/shortcut_customization_ui/shortcut_customization_app_ui.h"
#include "ash/webui/vc_background_ui/vc_background_ui.h"
#include "chrome/browser/ui/webui/ash/app_install/app_install_dialog.h"
#include "chrome/browser/ui/webui/ash/bluetooth/bluetooth_pairing_dialog.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_ui.h"
#include "chrome/browser/ui/webui/ash/curtain_ui/remote_maintenance_curtain_ui.h"
#include "chrome/browser/ui/webui/ash/emoji/emoji_ui.h"
#include "chrome/browser/ui/webui/ash/extended_updates/extended_updates_ui.h"
#include "chrome/browser/ui/webui/ash/internet/internet_config_dialog.h"
#include "chrome/browser/ui/webui/ash/internet/internet_detail_dialog.h"
#include "chrome/browser/ui/webui/ash/login/oobe_ui.h"
#include "chrome/browser/ui/webui/ash/multidevice_setup/multidevice_setup_dialog.h"
#include "chrome/browser/ui/webui/ash/office_fallback/office_fallback_ui.h"
#include "chrome/browser/ui/webui/ash/parent_access/parent_access_ui.h"
#include "chrome/browser/ui/webui/ash/set_time/set_time_ui.h"
#include "chrome/browser/ui/webui/ash/settings/os_settings_ui.h"
#include "chrome/browser/ui/webui/ash/skyvault/local_files_migration_ui.h"
#include "chrome/browser/ui/webui/nearby_share/nearby_share_dialog_ui.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

#if !defined(OFFICIAL_BUILD)
#endif  // defined(OFFICIAL_BUILD)

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)


#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)
#endif

#if BUILDFLAG(IS_WIN)
#include "chrome/browser/ui/webui/default_browser/visual_guided_setter_ui.h"
#endif

namespace chrome::internal {

using content::RegisterWebUIControllerInterfaceBinder;

namespace {

void BindMetricsReporterService(
    content::RenderFrameHost* frame_host,
    mojo::PendingReceiver<metrics_reporter::mojom::PageMetricsHost> receiver) {
  auto* web_contents = content::WebContents::FromRenderFrameHost(frame_host);
  if (!web_contents) {
    return;
  }

  // MetricsReporterService is only intended for use by WebUI.
  if (!frame_host->GetWebUI()) {
    return;
  }

  MetricsReporterService* service =
      MetricsReporterService::GetFromWebContents(web_contents);
  service->BindReceiver(std::move(receiver));
}

}  // namespace

void PopulateChromeWebUIFrameBindersPartsDesktop(
    mojo::BinderMapWithContext<content::RenderFrameHost*>* map,
    content::RenderFrameHost* render_frame_host) {
  RegisterWebUIControllerInterfaceBinder<downloads::mojom::PageHandlerFactory,
                                         DownloadsUI>(map);

  RegisterWebUIControllerInterfaceBinder<history::mojom::PageHandler,
                                         HistoryUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      infobar_internals::mojom::PageHandlerFactory, InfoBarInternalsUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      page_action_internals::mojom::PageHandlerFactory, PageActionInternalsUI>(
      map);

  auto* history_clusters_service =
      HistoryClustersServiceFactory::GetForBrowserContext(
          render_frame_host->GetProcess()->GetBrowserContext());
  if (history_clusters_service &&
      history_clusters_service->is_journeys_feature_flag_enabled()) {
    RegisterWebUIControllerInterfaceBinder<
        history_clusters::mojom::PageHandlerFactory, HistoryUI>(map);
  }
  RegisterWebUIControllerInterfaceBinder<
      page_image_service::mojom::PageImageServiceHandler, HistoryUI>(map);

  RegisterWebUIControllerInterfaceBinder<searchbox::mojom::PageHandlerFactory,
                                         OmniboxPopupUI>(map);

  // TODO(crbug.com/398926117): Create a generic mechanism for these interfaces.
  map->Add<metrics_reporter::mojom::PageMetricsHost>(
      &BindMetricsReporterService);

  if (base::FeatureList::IsEnabled(multistep_filter::kMultistepFilter)) {
    RegisterWebUIControllerInterfaceBinder<
        multistep_filter_internals::mojom::PageHandlerFactory,
        multistep_filter_internals::MultistepFilterInternalsUI>(map);
  }

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  RegisterWebUIControllerInterfaceBinder<updater_ui::mojom::PageHandlerFactory,
                                         UpdaterUI>(map);
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_MAC)
  RegisterWebUIControllerInterfaceBinder<
      unexportable_keys_internals::mojom::PageHandlerFactory,
      UnexportableKeysInternalsUI>(map);
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_WIN)
  RegisterWebUIControllerInterfaceBinder<
      visual_guided_setter::mojom::PageHandlerFactory, VisualGuidedSetterUI>(
      map);
#endif
}

void PopulateChromeWebUIFrameInterfaceBrokersTrustedPartsDesktop(
    content::WebUIBrowserInterfaceBrokerRegistry& registry) {
  // Note: The MetricsReporterService & ColorChangeListener are available to all
  // WebUIs in the registry
  registry.AddGlobal<metrics_reporter::mojom::PageMetricsHost>(
      base::BindRepeating(&BindMetricsReporterService));

  registry
      .ForWebUI<settings::SettingsUI>()
#if !BUILDFLAG(IS_CHROMEOS)
      .Add<theme_color_picker::mojom::ThemeColorPickerHandlerFactory>()
#endif  // !BUILDFLAG(IS_CHROMEOS)
      .Add<customize_color_scheme_mode::mojom::
               CustomizeColorSchemeModeHandlerFactory>();

  // TODO(crbug.com/452983498): Migrate all remaining
  // RegisterWebUIControllerInterfaceBinder calls to registry.ForWebUI().Add()
  // calls.
}

void PopulateChromeWebUIFrameInterfaceBrokersUntrustedPartsDesktop(
    content::WebUIBrowserInterfaceBrokerRegistry& registry) {
}

}  // namespace chrome::internal
