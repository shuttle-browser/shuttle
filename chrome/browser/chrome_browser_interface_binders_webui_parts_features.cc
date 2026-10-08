// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/android_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_browser_interface_binders_webui_parts.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_features.h"
#include "components/enterprise/buildflags/buildflags.h"
#include "components/safe_browsing/buildflags.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_ui_browser_interface_broker_registry.h"
#include "content/public/browser/web_ui_controller_interface_binder.h"
#include "extensions/buildflags/buildflags.h"
#include "mojo/public/cpp/bindings/binder_map.h"

#if BUILDFLAG(CHROME_ROOT_STORE_CERT_MANAGEMENT_UI)
#include "chrome/browser/resources/certificate_manager/certificate_manager.mojom.h"
#include "chrome/browser/ui/webui/certificate_manager/certificate_manager_ui.h"
#endif  // BUILDFLAG(CHROME_ROOT_STORE_CERT_MANAGEMENT_UI)

#if BUILDFLAG(ENABLE_EXTENSIONS)
#endif

#if BUILDFLAG(ENABLE_SESSION_SERVICE)
#include "chrome/browser/ui/webui/tab_strip_internals/tab_strip_internals_ui.h"
#endif

#if BUILDFLAG(ENTERPRISE_WATERMARK)
#include "chrome/browser/ui/webui/watermark/watermark_ui.h"
#endif

#if BUILDFLAG(FULL_SAFE_BROWSING)
#endif  // BUILDFLAG(FULL_SAFE_BROWSING)

namespace chrome::internal {

using content::RegisterWebUIControllerInterfaceBinder;

void PopulateChromeWebUIFrameBindersPartsFeatures(
    mojo::BinderMapWithContext<content::RenderFrameHost*>* map,
    content::RenderFrameHost* render_frame_host) {
#if BUILDFLAG(CHROME_ROOT_STORE_CERT_MANAGEMENT_UI)
  RegisterWebUIControllerInterfaceBinder<
      certificate_manager::mojom::CertificateManagerPageHandlerFactory,
      CertificateManagerUI>(map);
#endif  // BUILDFLAG(CHROME_ROOT_STORE_CERT_MANAGEMENT_UI)

#if BUILDFLAG(ENABLE_SESSION_SERVICE)
  RegisterWebUIControllerInterfaceBinder<
      tab_strip_internals::mojom::PageHandlerFactory, TabStripInternalsUI>(map);
#endif

#if BUILDFLAG(ENTERPRISE_WATERMARK)
  RegisterWebUIControllerInterfaceBinder<watermark::mojom::PageHandlerFactory,
                                         WatermarkUI>(map);
#endif
}

void PopulateChromeWebUIFrameInterfaceBrokersUntrustedPartsFeatures(
    content::WebUIBrowserInterfaceBrokerRegistry& registry) {
}

}  // namespace chrome::internal
