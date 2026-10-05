// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/containers/fixed_flat_map.h"
#include "base/functional/callback.h"
#include "base/observer_list.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webauthn/authenticator_request_dialog_view_controller.h"
#include "chrome/browser/webauthn/authenticator_transport.h"
#include "chrome/browser/webauthn/webauthn_pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/device_event_log/device_event_log.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "device/fido/discoverable_credential_metadata.h"
#include "device/fido/public/fido_types.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"

namespace {

// StepUiType enumerates the different types of UI that can be displayed.
enum class StepUIType {
  NONE,
  // A Chromium captive dialog.
  DIALOG,
};

StepUIType step_ui_type(AuthenticatorRequestDialogModel::Step step) {
  switch (step) {
    case AuthenticatorRequestDialogModel::Step::kClosed:
    case AuthenticatorRequestDialogModel::Step::kNotStarted:
    case AuthenticatorRequestDialogModel::Step::kPlatformAuthenticator:
      return StepUIType::NONE;

    default:
      return StepUIType::DIALOG;
  }
}

std::optional<content::GlobalRenderFrameHostId> FrameHostIdFromMaybeNull(
    content::RenderFrameHost* render_frame_host) {
  if (render_frame_host == nullptr) {
    return std::nullopt;
  }
  return render_frame_host->GetGlobalId();
}

content::WebContents* GetWebContentsFromFrameHostId(
    std::optional<content::GlobalRenderFrameHostId> frame_host_id) {
  if (!frame_host_id) {
    return nullptr;
  }
  return content::WebContents::FromRenderFrameHost(
      content::RenderFrameHost::FromID(*frame_host_id));
}

}  // namespace

#define AUTHENTICATOR_REQUEST_EVENT_0(name) \
  void AuthenticatorRequestDialogModel::Observer::name() {}
#define AUTHENTICATOR_REQUEST_EVENT_1(name, arg1type) \
  void AuthenticatorRequestDialogModel::Observer::name(arg1type) {}
AUTHENTICATOR_EVENTS
#undef AUTHENTICATOR_REQUEST_EVENT_0
#undef AUTHENTICATOR_REQUEST_EVENT_1

// static
std::u16string AuthenticatorRequestDialogModel::GetMechanismDescription(
    const device::DiscoverableCredentialMetadata& cred,
    UIPresentation ui_presentation) {
  if (cred.provider_name) {
    return base::UTF8ToUTF16(*cred.provider_name);
  }
  int message;
  switch (cred.source) {
    case device::AuthenticatorType::kWinNative:
      message = IDS_WEBAUTHN_SOURCE_WINDOWS_HELLO;
      break;
    case device::AuthenticatorType::kTouchID:
      message = IDS_WEBAUTHN_SOURCE_CHROME_PROFILE;
      break;
    case device::AuthenticatorType::kICloudKeychain:
      message = IDS_WEBAUTHN_SOURCE_ICLOUD_KEYCHAIN;
      break;
    case device::AuthenticatorType::kEnclave:
      message = IDS_WEBAUTHN_SOURCE_GOOGLE_PASSWORD_MANAGER;
      break;
    case device::AuthenticatorType::kOther:
      // "Other" is USB security keys and the virtual authenticator.
      message = IDS_WEBAUTHN_SOURCE_USB_SECURITY_KEY;
      break;
    default:
      message = IDS_WEBAUTHN_SOURCE_USB_SECURITY_KEY;
  }
  return l10n_util::GetStringUTF16(message);
}

AuthenticatorRequestDialogModel::AuthenticatorRequestDialogModel(
    content::RenderFrameHost* render_frame_host)
    : frame_host_id(FrameHostIdFromMaybeNull(render_frame_host)) {}

AuthenticatorRequestDialogModel::~AuthenticatorRequestDialogModel() {
  for (auto& observer : observers) {
    observer.OnModelDestroyed(this);
  }
}

void AuthenticatorRequestDialogModel::AddObserver(
    AuthenticatorRequestDialogModel::Observer* observer) {
  observers.AddObserver(observer);
}

void AuthenticatorRequestDialogModel::RemoveObserver(
    AuthenticatorRequestDialogModel::Observer* observer) {
  observers.RemoveObserver(observer);
}

void AuthenticatorRequestDialogModel::SetStep(Step step) {
  FIDO_LOG(EVENT) << "UI step: " << step;

  const StepUIType previous_ui_type = step_ui_type(step_);
  step_ = step;
  ui_disabled_ = false;

  const StepUIType ui_type = step_ui_type(step_);
  auto* web_contents = GetWebContentsFromFrameHostId(frame_host_id);
  if (ui_type != StepUIType::DIALOG) {
    view_controller_.reset();
  } else if (previous_ui_type != StepUIType::DIALOG && web_contents) {
    view_controller_ =
        AuthenticatorRequestDialogViewController::Create(web_contents, this);
  }

  for (auto& observer : observers) {
    observer.OnStepTransition();
  }
}

bool AuthenticatorRequestDialogModel::should_dialog_be_closed() const {
  return step_ui_type(step_) != StepUIType::DIALOG;
}

#define AUTHENTICATOR_REQUEST_EVENT_0(name)        \
  void AuthenticatorRequestDialogModel::name() {   \
    const int start_generation = this->generation; \
    for (auto& observer : observers) {             \
      if (start_generation != this->generation) {  \
        break;                                     \
      }                                            \
      observer.name();                             \
    }                                              \
  }
#define AUTHENTICATOR_REQUEST_EVENT_1(name, arg1type)         \
  void AuthenticatorRequestDialogModel::name(arg1type arg1) { \
    const int start_generation = this->generation;            \
    for (auto& observer : observers) {                        \
      if (start_generation != this->generation) {             \
        break;                                                \
      }                                                       \
      observer.name(arg1);                                    \
    }                                                         \
  }
AUTHENTICATOR_EVENTS
#undef AUTHENTICATOR_REQUEST_EVENT_0
#undef AUTHENTICATOR_REQUEST_EVENT_1

std::ostream& operator<<(std::ostream& os,
                         const AuthenticatorRequestDialogModel::Step& step) {
  using Step = AuthenticatorRequestDialogModel::Step;
  constexpr auto kStepNames = base::MakeFixedFlatMap<Step, std::string_view>({
      {Step::kNotStarted, "kNotStarted"},
      {Step::kMechanismSelection, "kMechanismSelection"},
      {Step::kErrorNoAvailableTransports, "kErrorNoAvailableTransports"},
      {Step::kErrorNoPasskeys, "kErrorNoPasskeys"},
      {Step::kErrorInternalUnrecognized, "kErrorInternalUnrecognized"},
      {Step::kErrorWindowsHelloNotEnabled, "kErrorWindowsHelloNotEnabled"},
      {Step::kTimedOut, "kTimedOut"},
      {Step::kKeyNotRegistered, "kKeyNotRegistered"},
      {Step::kKeyAlreadyRegistered, "kKeyAlreadyRegistered"},
      {Step::kMissingCapability, "kMissingCapability"},
      {Step::kStorageFull, "kStorageFull"},
      {Step::kClosed, "kClosed"},
      {Step::kUsbInsertAndActivate, "kUsbInsertAndActivate"},
      {Step::kBlePowerOnAutomatic, "kBlePowerOnAutomatic"},
      {Step::kBlePowerOnManual, "kBlePowerOnManual"},
      {Step::kBlePermissionMac, "kBlePermissionMac"},
      {Step::kOffTheRecordInterstitial, "kOffTheRecordInterstitial"},
      {Step::kCableV2QRCode, "kCableV2QRCode"},
      {Step::kCableV2Connecting, "kCableV2Connecting"},
      {Step::kCableV2Connected, "kCableV2Connected"},
      {Step::kCableV2Error, "kCableV2Error"},
      {Step::kClientPinChange, "kClientPinChange"},
      {Step::kClientPinEntry, "kClientPinEntry"},
      {Step::kClientPinSetup, "kClientPinSetup"},
      {Step::kClientPinTapAgain, "kClientPinTapAgain"},
      {Step::kClientPinErrorSoftBlock, "kClientPinErrorSoftBlock"},
      {Step::kClientPinErrorHardBlock, "kClientPinErrorHardBlock"},
      {Step::kClientPinErrorAuthenticatorRemoved,
       "kClientPinErrorAuthenticatorRemoved"},
      {Step::kInlineBioEnrollment, "kInlineBioEnrollment"},
      {Step::kRetryInternalUserVerification, "kRetryInternalUserVerification"},
      {Step::kResidentCredentialConfirmation,
       "kResidentCredentialConfirmation"},
      {Step::kSelectAccount, "kSelectAccount"},
      {Step::kPreSelectAccount, "kPreSelectAccount"},
      {Step::kSelectPriorityMechanism, "kSelectPriorityMechanism"},
      {Step::kChromeProfileCreatePasskey, "kChromeProfileCreatePasskey"},
      {Step::kPlatformAuthenticator, "kPlatformAuthenticator"},
  });
  static_assert(Step::kMaxValue == Step::kPlatformAuthenticator &&
                    kStepNames.size() - 1 == static_cast<int>(Step::kMaxValue),
                "implement operator<< overload when adding new Step values");
  return os << kStepNames.at(step);
}

AuthenticatorRequestDialogModel::Mechanism::Mechanism(
    AuthenticatorRequestDialogModel::Mechanism::Type in_type,
    std::u16string in_name,
    const gfx::VectorIcon& in_icon,
    base::RepeatingClosure in_callback,
    std::u16string in_display_name)
    : type(std::move(in_type)),
      name(std::move(in_name)),
      display_name(std::move(in_display_name)),
      icon(in_icon),
      callback(std::move(in_callback)) {}
AuthenticatorRequestDialogModel::Mechanism::~Mechanism() = default;
AuthenticatorRequestDialogModel::Mechanism::Mechanism(Mechanism&&) = default;

AuthenticatorRequestDialogModel::Mechanism::CredentialInfo::CredentialInfo(
    device::AuthenticatorType source_in,
    std::vector<uint8_t> user_id_in,
    std::optional<base::Time> last_used_time_in)
    : source(source_in),
      user_id(std::move(user_id_in)),
      last_used_time(last_used_time_in) {}
AuthenticatorRequestDialogModel::Mechanism::CredentialInfo::CredentialInfo(
    const CredentialInfo&) = default;
AuthenticatorRequestDialogModel::Mechanism::CredentialInfo::~CredentialInfo() =
    default;
bool AuthenticatorRequestDialogModel::Mechanism::CredentialInfo::operator==(
    const CredentialInfo&) const = default;
