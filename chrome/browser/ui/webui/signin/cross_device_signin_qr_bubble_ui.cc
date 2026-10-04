// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/cross_device_signin_qr_bubble_ui.h"

#include "base/check.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/escape.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/webui/signin/signin_utils.h"
#include "chrome/browser/ui/webui/util/image_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/signin_resources.h"
#include "chrome/grit/signin_resources_map.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/gfx/image/image.h"
#include "ui/webui/webui_util.h"

CrossDeviceSigninQrBubbleUIConfig::CrossDeviceSigninQrBubbleUIConfig()
    : DefaultWebUIConfig(content::kChromeUIScheme,
                         chrome::kChromeUICrossDeviceSigninQrBubbleHost) {
  // NOLINT(modernize-use-equals-default): DefaultWebUIConfig requires
  // arguments.
}

bool CrossDeviceSigninQrBubbleUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  // The QR code generator is not part of this build.
  return false;
}

namespace {

class CrossDeviceSigninQrBubbleHandler
    : public cross_device_signin::mojom::PageHandler {
 public:
  CrossDeviceSigninQrBubbleHandler(
      mojo::PendingReceiver<cross_device_signin::mojom::PageHandler> receiver,
      Profile* profile)
      : receiver_(this, std::move(receiver)), profile_(profile) {}

  ~CrossDeviceSigninQrBubbleHandler() override = default;

  CrossDeviceSigninQrBubbleHandler(const CrossDeviceSigninQrBubbleHandler&) =
      delete;
  CrossDeviceSigninQrBubbleHandler& operator=(
      const CrossDeviceSigninQrBubbleHandler&) = delete;

  // cross_device_signin::mojom::PageHandler:
  void GetRegistrationData(GetRegistrationDataCallback callback) override {
    auto data =
        cross_device_signin::mojom::CrossDeviceSigninQrBubbleData::New();
    auto* identity_manager = IdentityManagerFactory::GetForProfile(profile_);
    if (!identity_manager) {
      std::move(callback).Run(nullptr);
      return;
    }
    CoreAccountInfo core_info =
        identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
    if (core_info.IsEmpty()) {
      std::move(callback).Run(nullptr);
      return;
    }
    data->email = core_info.email;

    AccountInfo account_info =
        identity_manager->FindExtendedAccountInfo(core_info);
    data->full_name = std::string(account_info.GetFullName().value_or(""));

    // QR code generation is not part of this build, so no QR code data URI is
    // provided.

    std::move(callback).Run(std::move(data));
  }

 private:
  mojo::Receiver<cross_device_signin::mojom::PageHandler> receiver_;
  raw_ptr<Profile> profile_;
};

}  // namespace

CrossDeviceSigninQrBubbleUI::CrossDeviceSigninQrBubbleUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      Profile::FromWebUI(web_ui),
      chrome::kChromeUICrossDeviceSigninQrBubbleHost);

  webui::SetupWebUIDataSource(
      source, base::span(kSigninResources),
      IDR_SIGNIN_CROSS_DEVICE_SIGNIN_QR_BUBBLE_CROSS_DEVICE_SIGNIN_QR_BUBBLE_HTML);

  source->UseStringsJs();

  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"title", IDS_QR_CODE_BUBBLE_SIGNIN_ON_PHONE_TITLE},
      {"subtitle", IDS_QR_CODE_BUBBLE_SIGNIN_ON_PHONE_SUBTITLE},
      {"qrCodeAltText", IDS_QR_CODE_BUBBLE_SIGNIN_ON_PHONE_QR_CODE_ALT_TEXT},
  };
  source->AddLocalizedStrings(kLocalizedStrings);
  source->EnableReplaceI18nInJS();
}

CrossDeviceSigninQrBubbleUI::~CrossDeviceSigninQrBubbleUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(CrossDeviceSigninQrBubbleUI)

void CrossDeviceSigninQrBubbleUI::BindInterface(
    mojo::PendingReceiver<cross_device_signin::mojom::PageHandlerFactory>
        receiver) {
  page_factory_receiver_.reset();
  page_factory_receiver_.Bind(std::move(receiver));
}

void CrossDeviceSigninQrBubbleUI::CreateCrossDeviceSigninQrBubbleHandler(
    mojo::PendingReceiver<cross_device_signin::mojom::PageHandler> receiver) {
  page_handler_ = std::make_unique<CrossDeviceSigninQrBubbleHandler>(
      std::move(receiver), Profile::FromWebUI(web_ui()));
}
