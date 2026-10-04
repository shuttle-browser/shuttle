// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OMNIBOX_POPUP_OMNIBOX_POPUP_UI_H_
#define CHROME_BROWSER_UI_WEBUI_OMNIBOX_POPUP_OMNIBOX_POPUP_UI_H_

#include <memory>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/ui/webui/omnibox_popup/mojom/omnibox_popup.mojom.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_web_ui_controller.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_webui_config.h"
#include "chrome/common/webui_url_constants.h"
#include "components/omnibox/browser/searchbox.mojom.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/url_constants.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"

class Profile;

class OmniboxPopupHandler;
class OmniboxPopupPresenterBase;
class OmniboxPopupUI;
class SearchboxHandler;

class OmniboxPopupUIConfig
    : public DefaultTopChromeWebUIConfig<OmniboxPopupUI> {
 public:
  OmniboxPopupUIConfig()
      : DefaultTopChromeWebUIConfig(content::kChromeUIScheme,
                                    chrome::kChromeUIOmniboxPopupHost) {}

  // content::WebUIConfig:
  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
  bool ShouldCrashOnJavascriptErrorInDevelopmentBuild() const override;
};

// The Web UI controller for the chrome://omnibox-popup.top-chrome.
class OmniboxPopupUI : public TopChromeWebUIController,
                       public omnibox_popup::mojom::PageHandlerFactory,
                       public searchbox::mojom::PageHandlerFactory {
 public:
  explicit OmniboxPopupUI(content::WebUI* web_ui);
  OmniboxPopupUI(const OmniboxPopupUI&) = delete;
  OmniboxPopupUI& operator=(const OmniboxPopupUI&) = delete;
  ~OmniboxPopupUI() override;

  // Instantiates the implementor of the searchbox::mojom::PageHandlerFactory
  // mojo interface passing the pending receiver that will be internally bound.
  void BindInterface(content::RenderFrameHost* host,
                     mojo::PendingReceiver<searchbox::mojom::PageHandlerFactory>
                         pending_page_handler);
  SearchboxHandler* omnibox_handler() {
    return omnibox_handler_.get();
  }

  // omnibox_popup::mojom::PageHandlerFactory:
  void BindInterface(
      mojo::PendingReceiver<omnibox_popup::mojom::PageHandlerFactory> receiver);
  void CreatePageHandler(
      mojo::PendingRemote<omnibox_popup::mojom::Page> page,
      mojo::PendingReceiver<omnibox_popup::mojom::PageHandler> receiver)
      override;

  OmniboxPopupHandler* popup_handler() {
    return const_cast<OmniboxPopupHandler*>(
        std::as_const(*this).popup_handler());
  }
  const OmniboxPopupHandler* popup_handler() const {
    return popup_handler_.get();
  }

  // searchbox::mojom::PageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingRemote<searchbox::mojom::Page> page,
      mojo::PendingReceiver<searchbox::mojom::PageHandler> handler) override;

  static constexpr std::string_view GetWebUIName() { return "OmniboxPopup"; }

  void SetPresenterDelegate(OmniboxPopupPresenterBase* delegate);

 private:
  raw_ptr<Profile> profile_;

  std::unique_ptr<SearchboxHandler> omnibox_handler_;

  std::unique_ptr<OmniboxPopupHandler> popup_handler_;
  mojo::Receiver<omnibox_popup::mojom::PageHandlerFactory>
      popup_page_factory_receiver_{this};

  mojo::Receiver<searchbox::mojom::PageHandlerFactory>
      searchbox_page_factory_receiver_{this};

  raw_ptr<OmniboxPopupPresenterBase> presenter_delegate_ = nullptr;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_OMNIBOX_POPUP_OMNIBOX_POPUP_UI_H_
