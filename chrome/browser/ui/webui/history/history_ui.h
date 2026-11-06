// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_HISTORY_HISTORY_UI_H_
#define CHROME_BROWSER_UI_WEBUI_HISTORY_HISTORY_UI_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "components/page_image_service/mojom/page_image_service.mojom.h"
#include "content/public/browser/webui_config.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/resource/resource_scale_factor.h"
#include "ui/webui/mojo_web_ui_controller.h"
#include "ui/webui/resources/cr_components/history/history.mojom-forward.h"

namespace base {
class RefCountedMemory;
}

class BrowsingHistoryHandler;

namespace page_image_service {
class ImageServiceHandler;
}

class HistoryUIConfig : public content::WebUIConfig {
 public:
  HistoryUIConfig();
  ~HistoryUIConfig() override;

  // content::WebUIConfig:
  std::unique_ptr<content::WebUIController> CreateWebUIController(
      content::WebUI* web_ui,
      const GURL& url) override;
#if BUILDFLAG(IS_ANDROID)
  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
#endif  // BUILDFLAG(IS_ANDROID)
};

class HistoryUI : public ui::MojoWebUIController {
 public:
  explicit HistoryUI(content::WebUI* web_ui);
  HistoryUI(const HistoryUI&) = delete;
  HistoryUI& operator=(const HistoryUI&) = delete;
  ~HistoryUI() override;

  static scoped_refptr<base::RefCountedMemory> GetFaviconResourceBytes(
      ui::ResourceScaleFactor scale_factor);

  // Instantiates the implementors of mojom interfaces.
  void BindInterface(
      mojo::PendingReceiver<history::mojom::PageHandler> pending_page_handler);
  void BindInterface(
      mojo::PendingReceiver<page_image_service::mojom::PageImageServiceHandler>
          pending_page_handler);

  // For testing only.
  BrowsingHistoryHandler* GetBrowsingHistoryHandlerForTesting() {
    return browsing_history_handler_.get();
  }

 private:
  std::unique_ptr<BrowsingHistoryHandler> browsing_history_handler_;
  std::unique_ptr<page_image_service::ImageServiceHandler>
      image_service_handler_;
  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_HISTORY_HISTORY_UI_H_
