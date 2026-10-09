// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/chrome_authenticator_request_delegate.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/base/signin_switches.h"
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
#endif
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/navigator/browser_navigator.h"
#include "chrome/browser/ui/navigator/browser_navigator_params.h"
#include "chrome/browser/ui/webauthn/user_actions.h"
#include "chrome/browser/webauthn/authenticator_request_dialog_controller.h"
#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"
#include "chrome/browser/webauthn/immediate_request_rate_limiter_factory.h"
#include "chrome/browser/webauthn/webauthn_metrics_util.h"
#include "chrome/browser/webauthn/webauthn_pref_names.h"
#include "chrome/common/chrome_version.h"
#include "chrome/common/pref_names.h"
#include "components/device_event_log/device_event_log.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/user_prefs/user_prefs.h"
#include "components/webauthn/content/browser/immediate_request_rate_limiter.h"
#include "content/public/browser/authenticator_request_client_delegate.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_authentication_request_proxy.h"
#include "content/public/browser/web_contents.h"
#include "crypto/random.h"
#include "device/fido/authenticator_get_assertion_response.h"
#include "device/fido/cable/v2_constants.h"
#include "device/fido/cable/v2_handshake.h"
#include "device/fido/discoverable_credential_metadata.h"
#include "device/fido/fido_authenticator.h"
#include "device/fido/fido_discovery_base.h"
#include "device/fido/fido_discovery_factory.h"
#include "device/fido/fido_request_handler_base.h"
#include "device/fido/public/features.h"
#include "device/fido/public/fido_constants.h"
#include "device/fido/public/fido_transport_protocol.h"
#include "device/fido/public/fido_types.h"
#include "device/fido/public/public_key_credential_descriptor.h"
#include "device/fido/public/public_key_credential_user_entity.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/url_pattern.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/base/url_util.h"
#include "third_party/blink/public/mojom/credentialmanagement/credential_type_flags.mojom.h"
#include "third_party/icu/source/common/unicode/locid.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/native_ui_types.h"

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/webauthn/chrome_authenticator_request_delegate_mac.h"
#include "device/fido/mac/credential_metadata.h"
#include "ui/views/widget/widget.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "chrome/browser/webauthn/local_credential_management_win.h"
#include "device/fido/win/authenticator.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/components/webauthn/webauthn_request_registrar.h"
#include "ui/aura/window.h"
#endif

using UIPresentation = ChromeAuthenticatorRequestDelegate::UIPresentation;
using TransportAvailabilityInfo =
    device::FidoRequestHandlerBase::TransportAvailabilityInfo;

namespace {

ChromeAuthenticatorRequestDelegate::TestObserver* g_observer = nullptr;

static constexpr char kGoogleRpId[] = "google.com";

// Returns true iff the credential is reported as being present on the platform
// authenticator (i.e. it is not a phone or icloud credential).
bool IsCredentialFromPlatformAuthenticator(
    device::DiscoverableCredentialMetadata cred) {
  return cred.source != device::AuthenticatorType::kICloudKeychain &&
         cred.source != device::AuthenticatorType::kPhone;
}

// Returns true iff |user_id| starts with the prefix reserved for passkeys used
// to authenticate to Google services.
bool UserIdHasGooglePasskeyAuthPrefix(const std::vector<uint8_t>& user_id) {
  static constexpr std::string_view kPrefix = "GOOGLE_ACCOUNT:";
  return user_id.size() >= kPrefix.size() &&
         base::span(user_id).first(kPrefix.size()) == base::span(kPrefix);
}

// Filters |passkeys| to only contain credentials that are used to authenticate
// to Google services.
void FilterGoogleAuthPasskeys(
    std::vector<device::DiscoverableCredentialMetadata>* passkeys) {
  std::erase_if(*passkeys, [](const auto& passkey) {
    return IsCredentialFromPlatformAuthenticator(passkey) &&
           !UserIdHasGooglePasskeyAuthPrefix(passkey.user.id);
  });
}

#if BUILDFLAG(IS_MAC)
const char kWebAuthnTouchIdLastUsed[] = "webauthn.touchid.last_used";

// kMacOsRecentlyUsedMaxDays specifies how recently the macOS profile
// authenticator must have been used (for the current profile) to be considered
// "actively" used. Chrome may default to the profile authenticator in more
// cases if it is being actively used.
const int kMacOsRecentlyUsedMaxDays = 31;
#endif

// CableLinkingEventHandler handles linking information sent by caBLEv2
// authenticators. This linking information can come after the WebAuthn
// operation has resolved and thus after the
// `ChromeAuthenticatorRequestDelegate` has been destroyed. Thus this object is
// owned by the callback itself, and can save linking information until the
// point where the `Profile` itself is destroyed.
class CableLinkingEventHandler : public ProfileObserver {
 public:
  explicit CableLinkingEventHandler(Profile* profile) : profile_(profile) {
    profile_->AddObserver(this);
  }

  ~CableLinkingEventHandler() override {
    if (profile_) {
      profile_->RemoveObserver(this);
      profile_ = nullptr;
    }
  }

  // ProfileObserver:
  void OnProfileWillBeDestroyed(Profile* profile) override {
    DCHECK_EQ(profile, profile_);
    profile_->RemoveObserver(this);
    profile_ = nullptr;
  }

 private:
  raw_ptr<Profile> profile_;
};

}  // namespace

// static
void ChromeAuthenticatorRequestDelegate::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterListPref(prefs::kSecurityKeyPermitAttestation);
#if BUILDFLAG(IS_WIN)
  LocalCredentialManagementWin::RegisterProfilePrefs(registry);
#endif
#if BUILDFLAG(IS_MAC)
  registry->RegisterStringPref(
      webauthn::pref_names::kWebAuthnTouchIdMetadataSecretPrefName,
      std::string());
  registry->RegisterStringPref(kWebAuthnTouchIdLastUsed, std::string());
  // This boolean preference is used as a tristate. If unset, whether or not to
  // default to iCloud is determined based on several factors.
  // (See `ShouldCreateInICloudKeychain`.) If set, then this preference is
  // controlling.
  //
  // The default value of this preference only determines whether the toggle
  // in settings will show as set or not when the preference hasn't been
  // explicitly set. Since the behaviour is actually more complex than can be
  // expressed in a boolean, this is always an approximation.
  registry->RegisterBooleanPref(
      prefs::kCreatePasskeysInICloudKeychain,
      ShouldCreateInICloudKeychain(
          RequestSource::kWebAuthentication,
          // Whether or not the user is actively using the profile authenticator
          // is stored in preferences, which aren't available at this time while
          // we're still registering them. Thus we assume that they are not.
          /*is_active_profile_authenticator_user=*/false,
          IsICloudDriveEnabled(),
          /*request_is_for_google_com=*/false, /*preference=*/std::nullopt));
#endif
}

ChromeAuthenticatorRequestDelegate::ChromeAuthenticatorRequestDelegate(
    content::RenderFrameHost* render_frame_host)
    : render_frame_host_id_(render_frame_host->GetGlobalId()),
      dialog_model_(base::MakeRefCounted<AuthenticatorRequestDialogModel>(
          GetRenderFrameHost())),
      dialog_controller_(std::make_unique<AuthenticatorRequestDialogController>(
          dialog_model_.get(),
          GetRenderFrameHost())) {
  dialog_model_->observers.AddObserver(this);
  if (g_observer) {
    g_observer->Created(this);
  }
}

ChromeAuthenticatorRequestDelegate::~ChromeAuthenticatorRequestDelegate() {
  // Currently, completion of the request is indicated by //content destroying
  // this delegate.
  dialog_model_->OnRequestComplete();
  dialog_model_->observers.RemoveObserver(this);

  if (g_observer) {
    g_observer->OnDestroy(this);
  }

  // Destruction of this class can in some cases be triggered by observers of
  // step transitions in the `AuthenticatorRequestDialogController`. Deferring
  // deletion allows the stack to unwind first.
  // See https://crbug.com/539754136.
  base::SequencedTaskRunner::GetCurrentDefault()->DeleteSoon(
      FROM_HERE, std::move(dialog_controller_));
}

// static
void ChromeAuthenticatorRequestDelegate::SetGlobalObserverForTesting(
    TestObserver* observer) {
  CHECK(!observer || !g_observer);
  g_observer = observer;
}

base::WeakPtr<ChromeAuthenticatorRequestDelegate>
ChromeAuthenticatorRequestDelegate::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void ChromeAuthenticatorRequestDelegate::SetRelyingPartyId(
    const std::string& rp_id) {
  dialog_model_->relying_party_id = rp_id;
}

void ChromeAuthenticatorRequestDelegate::SetUIPresentation(
    UIPresentation ui_presentation) {
  dialog_controller_->SetUIPresentation(ui_presentation);
}

bool ChromeAuthenticatorRequestDelegate::DoesBlockRequestOnFailure(
    InterestingFailureReason reason) {
  if (!webauthn_ui_enabled()) {
    return false;
  }

  // If the UI was already in the state where we asked the user to complete the
  // transaction on the other device then any errors are immediately resolved.
  // Very likely the user canceled on the phone and doesn't want to see another
  // error UI on the desktop.
  if (cable_device_ready_) {
    return false;
  }

  switch (reason) {
    case InterestingFailureReason::kTimeout:
      dialog_controller_->OnRequestTimeout();
      break;
    case InterestingFailureReason::kKeyNotRegistered:
      dialog_controller_->OnActivatedKeyNotRegistered();
      break;
    case InterestingFailureReason::kKeyAlreadyRegistered:
      dialog_controller_->OnActivatedKeyAlreadyRegistered();
      break;
    case InterestingFailureReason::kSoftPINBlock:
      dialog_controller_->OnSoftPINBlock();
      break;
    case InterestingFailureReason::kHardPINBlock:
      dialog_controller_->OnHardPINBlock();
      break;
    case InterestingFailureReason::kAuthenticatorRemovedDuringPINEntry:
      dialog_controller_->OnAuthenticatorRemovedDuringPINEntry();
      break;
    case InterestingFailureReason::kAuthenticatorMissingResidentKeys:
      dialog_controller_->OnAuthenticatorMissingResidentKeys();
      break;
    case InterestingFailureReason::kAuthenticatorMissingUserVerification:
      dialog_controller_->OnAuthenticatorMissingUserVerification();
      break;
    case InterestingFailureReason::kAuthenticatorMissingLargeBlob:
      dialog_controller_->OnAuthenticatorMissingLargeBlob();
      break;
    case InterestingFailureReason::kNoCommonAlgorithms:
      dialog_controller_->OnNoCommonAlgorithms();
      break;
    case InterestingFailureReason::kStorageFull:
      dialog_controller_->OnAuthenticatorStorageFull();
      break;
    case InterestingFailureReason::kUserConsentDenied:
      dialog_controller_->OnUserConsentDenied();
      break;
    case InterestingFailureReason::kWinUserCancelled:
      return dialog_controller_->OnWinUserCancelled();
    case InterestingFailureReason::kHybridTransportError:
      return dialog_controller_->OnHybridTransportError();
    case InterestingFailureReason::kNoPasskeys:
      return dialog_controller_->OnNoPasskeys();
    case InterestingFailureReason::kEnclaveError:
      return false;
    case InterestingFailureReason::kEnclaveCancel:
      dialog_model_->CancelAuthenticatorRequest();
      break;
  }
  return true;
}

void ChromeAuthenticatorRequestDelegate::OnTransactionSuccessful(
    RequestSource request_source,
    device::FidoRequestType request_type,
    device::AuthenticatorType authenticator_type) {
  if (request_source != RequestSource::kWebAuthentication) {
    return;
  }
#if BUILDFLAG(IS_MAC)
  if (authenticator_type == device::AuthenticatorType::kTouchID) {
    base::Time::Exploded exploded;
    base::Time::Now().UTCExplode(&exploded);
    profile()->GetPrefs()->SetString(
        kWebAuthnTouchIdLastUsed,
        base::StringPrintf("%04d-%02d-%02d", exploded.year, exploded.month,
                           exploded.day_of_month));
    webauthn::user_actions::RecordChromeProfileSuccess();
  }
  if (authenticator_type == device::AuthenticatorType::kICloudKeychain) {
    webauthn::user_actions::RecordICloudSuccess();
  }

  dialog_controller_->RecordMacOsSuccessHistogram(request_type,
                                                  authenticator_type);
#elif BUILDFLAG(IS_WIN)
  if (authenticator_type == device::AuthenticatorType::kWinNative) {
    webauthn::user_actions::RecordWindowsHelloSuccess();
  }
#endif  // BUILDFLAG(IS_MAC)
}

void ChromeAuthenticatorRequestDelegate::RegisterActionCallbacks(
    base::OnceClosure cancel_callback,
    base::OnceClosure immediate_not_found_callback,
    base::RepeatingClosure start_over_callback,
    AccountPreselectedCallback account_preselected_callback,
    PasswordSelectedCallback password_selected_callback,
    device::FidoRequestHandlerBase::RequestCallback request_callback,
    base::OnceClosure cancel_ui_timeout_callback,
    base::RepeatingClosure bluetooth_adapter_power_on_callback,
    base::RepeatingCallback<
        void(device::FidoRequestHandlerBase::BlePermissionCallback)>
        request_ble_permission_callback) {
  cancel_callback_ = std::move(cancel_callback);
  immediate_not_found_callback_ = std::move(immediate_not_found_callback);
  start_over_callback_ = std::move(start_over_callback);
  account_preselected_callback_ = std::move(account_preselected_callback);
  request_callback_ = request_callback;
  cancel_ui_timeout_callback_ = std::move(cancel_ui_timeout_callback);

  dialog_controller_->SetRequestCallback(request_callback);
  dialog_controller_->SetAccountPreselectedCallback(
      account_preselected_callback_);
  dialog_controller_->SetBluetoothAdapterPowerOnCallback(
      bluetooth_adapter_power_on_callback);
  dialog_controller_->SetRequestBlePermissionCallback(
      request_ble_permission_callback);
}

void ChromeAuthenticatorRequestDelegate::ConfigureDiscoveries(
    const url::Origin& origin,
    const std::string& rp_id,
    RequestSource request_source,
    device::FidoRequestType request_type,
    std::optional<device::ResidentKeyRequirement> resident_key_requirement,
    device::UserVerificationRequirement user_verification_requirement,
    bool cmtg_key_requested,
    std::optional<std::string_view> user_name,
    bool browser_provided_passkeys_available,
    device::FidoDiscoveryFactory* discovery_factory) {
  DCHECK(request_type == device::FidoRequestType::kGetAssertion ||
         resident_key_requirement.has_value());

  // Without the UI enabled, discoveries like caBLE, Android AOA, iCloud
  // keychain, and the enclave, don't make sense.
  if (!webauthn_ui_enabled()) {
    return;
  }

  // Shuttle: password credentials in WebAuthn requests are not
  // supported (no password manager).
  if (!discovery_factory) {
    return;
  }

  std::array<uint8_t, device::cablev2::kQRKeySize> qr_generator_key;
  crypto::RandBytes(qr_generator_key);
  std::string qr_string =
      device::cablev2::qr::Encode(qr_generator_key, request_type);

  auto linking_handler = std::make_unique<CableLinkingEventHandler>(profile());
  discovery_factory->set_cable_event_callback(
      base::BindRepeating(&ChromeAuthenticatorRequestDelegate::OnCableEvent,
                          weak_ptr_factory_.GetWeakPtr()));

  dialog_controller_->set_cable_transport_info(qr_string);

  discovery_factory->set_cable_data(request_type, qr_generator_key);

  if (SystemNetworkContextManager::GetInstance()) {
    // caBLE and the enclave depend on the network context factory.
    // TODO(nsatragno): this should probably use a storage partition network
    // context instead. See the SystemNetworkContextManager class comments.
    discovery_factory->set_network_context_factory(base::BindRepeating([]() {
      return SystemNetworkContextManager::GetInstance()->GetContext();
    }));
  }

#if BUILDFLAG(IS_MAC)
  ConfigureNSWindow(discovery_factory);
#endif

  dialog_controller_->set_is_non_webauthn_request(
      request_source != RequestSource::kWebAuthentication);

#if BUILDFLAG(IS_MAC)
  ConfigureICloudKeychain(request_source, rp_id);
#endif
}

void ChromeAuthenticatorRequestDelegate::SetHints(
    const AuthenticatorRequestClientDelegate::Hints& hints) {
  if (g_observer) {
    g_observer->HintsSet(hints);
  }
  dialog_controller_->SetHints(hints);
}

void ChromeAuthenticatorRequestDelegate::SelectAccount(
    std::vector<device::AuthenticatorGetAssertionResponse> responses,
    base::OnceCallback<void(device::AuthenticatorGetAssertionResponse)>
        callback) {
  if (!webauthn_ui_enabled()) {
    // Requests with UI disabled should never reach account selection.
    DCHECK(IsVirtualEnvironmentEnabled());

    // The browser is being automated. Select the first credential to support
    // automation of discoverable credentials.
    // TODO(crbug.com/40639383): Provide a way to determine which account gets
    // picked.
    std::move(callback).Run(std::move(responses.at(0)));
    return;
  }

  if (g_observer) {
    g_observer->AccountSelectorShown(responses);
    std::move(callback).Run(std::move(responses.at(0)));
    return;
  }

  dialog_controller_->SelectAccount(std::move(responses), std::move(callback));
}

bool ChromeAuthenticatorRequestDelegate::webauthn_ui_enabled() const {
  return dialog_controller_->ui_presentation() != UIPresentation::kDisabled;
}

void ChromeAuthenticatorRequestDelegate::SetCredentialTypes(
    int credential_type_flags) {
  credential_types_ = credential_type_flags;
}

void ChromeAuthenticatorRequestDelegate::SetCredentialIdFilter(
    std::vector<device::PublicKeyCredentialDescriptor> credential_list) {
  credential_filter_ = std::move(credential_list);
}

void ChromeAuthenticatorRequestDelegate::SetUserEntityForMakeCredentialRequest(
    const device::PublicKeyCredentialUserEntity& user_entity) {
  dialog_model_->user_entity = user_entity;
}

void ChromeAuthenticatorRequestDelegate::StartObserving(
    device::FidoRequestHandlerBase* request_handler) {
  request_handler_observation_.Observe(request_handler);
}

void ChromeAuthenticatorRequestDelegate::StopObserving(
    device::FidoRequestHandlerBase* request_handler) {
  request_handler_observation_.Reset();
}

void ChromeAuthenticatorRequestDelegate::OnTransportAvailabilityEnumerated(
    TransportAvailabilityInfo data) {
  if (g_observer) {
    g_observer->OnPreTransportAvailabilityEnumerated(this);
  }

  if (!webauthn_ui_enabled()) {
    return;
  }

  ShowUI(std::move(data));
}

bool ChromeAuthenticatorRequestDelegate::EmbedderControlsAuthenticatorDispatch(
    const device::FidoAuthenticator& authenticator) {
  // Decide whether the //device/fido code should dispatch the current
  // request to an authenticator immediately after it has been
  // discovered, or whether the embedder/UI takes charge of that by
  // invoking its RequestCallback.
  if (!webauthn_ui_enabled()) {
    // There is no UI to handle request dispatch.
    return false;
  }
  if (dialog_controller_->ui_presentation() == UIPresentation::kAutofill &&
      dialog_model_->step() ==
          AuthenticatorRequestDialogModel::Step::kNotStarted) {
    // If the inlined QR code suggestion feature is enabled, we do not want to
    // control (block) dispatch for hybrid authenticators during conditional UI,
    // but only on official Chrome Sign-in pages. This starts the background
    // hybrid handshake advertising immediately on page load to make the QR
    // string payload instantly available for rendering inside the Autofill
    // popup, while avoiding unnecessary Bluetooth advertising on other pages.
    // There is an active conditional request that is not showing any UI. The UI
    // will dispatch to any plugged in authenticators after the user selects an
    // option.
    return true;
  }
  auto transport = authenticator.AuthenticatorTransport();
  return !transport ||  // Windows
         *transport == device::FidoTransportProtocol::kInternal;
}

void ChromeAuthenticatorRequestDelegate::FidoAuthenticatorAdded(
    const device::FidoAuthenticator& authenticator) {
  if (!webauthn_ui_enabled()) {
    return;
  }

  dialog_controller_->AddAuthenticator(authenticator);
}

void ChromeAuthenticatorRequestDelegate::FidoAuthenticatorRemoved(
    std::string_view authenticator_id) {
  if (!webauthn_ui_enabled()) {
    return;
  }

  dialog_controller_->RemoveAuthenticator(authenticator_id);
}

void ChromeAuthenticatorRequestDelegate::BluetoothAdapterStatusChanged(
    device::FidoRequestHandlerBase::BleStatus ble_status) {
  dialog_controller_->BluetoothAdapterStatusChanged(ble_status);
}

bool ChromeAuthenticatorRequestDelegate::SupportsPIN() const {
  return true;
}

void ChromeAuthenticatorRequestDelegate::CollectPIN(
    CollectPINOptions options,
    base::OnceCallback<void(std::u16string)> provide_pin_cb) {
  dialog_controller_->CollectPIN(options.reason, options.error,
                                 options.min_pin_length, options.attempts,
                                 std::move(provide_pin_cb));
}

void ChromeAuthenticatorRequestDelegate::StartBioEnrollment(
    base::OnceClosure next_callback) {
  dialog_controller_->StartInlineBioEnrollment(std::move(next_callback));
}

void ChromeAuthenticatorRequestDelegate::OnSampleCollected(
    int bio_samples_remaining) {
  dialog_controller_->OnSampleCollected(bio_samples_remaining);
}

void ChromeAuthenticatorRequestDelegate::FinishCollectToken() {
  dialog_controller_->FinishCollectToken();
}

void ChromeAuthenticatorRequestDelegate::OnRetryUserVerification(int attempts) {
  dialog_controller_->OnRetryUserVerification(attempts);
}

void ChromeAuthenticatorRequestDelegate::OnStartOver() {
  DCHECK(start_over_callback_);
  dialog_model_->generation++;
  if (g_observer) {
    g_observer->PreStartOver();
  }
  start_over_callback_.Run();
}

void ChromeAuthenticatorRequestDelegate::OnModelDestroyed(
    AuthenticatorRequestDialogModel* model) {
  DCHECK_EQ(model, dialog_model_.get());
}

void ChromeAuthenticatorRequestDelegate::OnCancelRequest() {
  // |cancel_callback_| must be invoked at most once as invocation of
  // |cancel_callback_| will destroy |this|.
  DCHECK(cancel_callback_);
  std::move(cancel_callback_).Run();
}

content::RenderFrameHost*
ChromeAuthenticatorRequestDelegate::GetRenderFrameHost() const {
  content::RenderFrameHost* ret =
      content::RenderFrameHost::FromID(render_frame_host_id_);
  DCHECK(ret);
  return ret;
}

content::BrowserContext* ChromeAuthenticatorRequestDelegate::GetBrowserContext()
    const {
  return GetRenderFrameHost()->GetBrowserContext();
}

Profile* ChromeAuthenticatorRequestDelegate::profile() const {
  return Profile::FromBrowserContext(GetRenderFrameHost()->GetBrowserContext());
}

bool ChromeAuthenticatorRequestDelegate::MaybeHandleImmediateMediation(
    const TransportAvailabilityInfo& data) {
  if (data.request_type != device::FidoRequestType::kGetAssertion ||
      dialog_controller_->ui_presentation() !=
          UIPresentation::kModalImmediate) {
    return false;
  }

  // Always return not allowed immediate in incognito.
  if (profile()->IsOffTheRecord()) {
    base::UmaHistogramEnumeration(
        "WebAuthentication.GetAssertion.Immediate.RejectionReason",
        content::ImmediateMediationRejectionReason::kIncognito);
    return true;
  }

  if (auto* rate_limiter =
          ImmediateRequestRateLimiterFactory::GetForProfile(profile())) {
    if (!rate_limiter->IsRequestAllowed(*GetRenderFrameHost())) {
      FIDO_LOG(ERROR) << "Immediate request rate limit exceeded for the main "
                         "frame's origin.";
      base::UmaHistogramEnumeration(
          "WebAuthentication.GetAssertion.Immediate.RejectionReason",
          content::ImmediateMediationRejectionReason::kRateLimited);
      return true;
    }
  }

  if (data.recognized_credentials.empty()) {
    base::UmaHistogramEnumeration(
        "WebAuthentication.GetAssertion.Immediate.RejectionReason",
        content::ImmediateMediationRejectionReason::kNoCredentials);
    return true;
  }

  return false;
}

void ChromeAuthenticatorRequestDelegate::ShowUI(TransportAvailabilityInfo tai) {
  // Shuttle: conditional (autofill), ambient and passkey-upgrade requests
  // have no UI without the password manager/autofill. They stay pending until
  // the page aborts them.
  switch (dialog_controller_->ui_presentation()) {
    case UIPresentation::kAutofill:
    case UIPresentation::kAmbient:
    case UIPresentation::kPasskeyUpgrade:
    case UIPresentation::kDisabled:
      return;
    default:
      break;
  }

  FilterRecognizedCredentials(&tai);

  if (MaybeHandleImmediateMediation(tai)) {
    std::move(immediate_not_found_callback_).Run();
    return;
  }

  if (!cancel_ui_timeout_callback_.is_null()) {
    std::move(cancel_ui_timeout_callback_).Run();
  }

  if (g_observer) {
    g_observer->OnTransportAvailabilityEnumerated(this, &tai);
  }

  if (dialog_model_->step() !=
      AuthenticatorRequestDialogModel::Step::kNotStarted) {
    return;
  }

  dialog_controller_->SetCredentialTypes(credential_types_);
  UpdateModelForTransportAvailability(tai);

  dialog_controller_->StartFlow(std::move(tai));

  if (g_observer) {
    g_observer->UIShown(this);
  }
}

void ChromeAuthenticatorRequestDelegate::OnCableEvent(
    device::cablev2::Event event) {
  if (event == device::cablev2::Event::kReady) {
    cable_device_ready_ = true;
  }

  dialog_controller_->OnCableEvent(event);
}

void ChromeAuthenticatorRequestDelegate::FilterRecognizedCredentials(
    TransportAvailabilityInfo* tai) {
  if (dialog_model()->relying_party_id == kGoogleRpId &&
      tai->has_empty_allow_list &&
      std::ranges::any_of(tai->recognized_credentials,
                          IsCredentialFromPlatformAuthenticator)) {
    // Regrettably, Chrome will create webauthn credentials for things other
    // than authentication (e.g. credit card autofill auth) under the rp id
    // "google.com". To differentiate those credentials from actual passkeys you
    // can use to sign in, Google adds a prefix to the user id.
    // This code filter passkeys that do not match that prefix.
    FilterGoogleAuthPasskeys(&tai->recognized_credentials);
    if (tai->has_platform_authenticator_credential ==
            device::FidoRequestHandlerBase::RecognizedCredential::
                kHasRecognizedCredential &&
        std::ranges::none_of(tai->recognized_credentials,
                             IsCredentialFromPlatformAuthenticator)) {
      tai->has_platform_authenticator_credential = device::
          FidoRequestHandlerBase::RecognizedCredential::kNoRecognizedCredential;
    }
  }

  if (!credential_filter_.empty()) {
    std::vector<device::DiscoverableCredentialMetadata> filtered_list;
    for (auto& platform_credential : tai->recognized_credentials) {
      for (auto& filter_credential : credential_filter_) {
        if (platform_credential.cred_id == filter_credential.id) {
          filtered_list.push_back(platform_credential);
          break;
        }
      }
    }
    tai->recognized_credentials = std::move(filtered_list);
  }

  const auto kImmediateTypes =
      std::unordered_set{device::AuthenticatorType::kICloudKeychain,
                         device::AuthenticatorType::kWinNative,
                         device::AuthenticatorType::kChromeOS,
                         device::AuthenticatorType::kTouchID};
  if (dialog_controller_->ui_presentation() ==
      UIPresentation::kModalImmediate) {
    std::erase_if(tai->recognized_credentials,
                  [&kImmediateTypes](const auto& passkey) {
                    return !kImmediateTypes.contains(passkey.source);
                  });
  }
}

#if BUILDFLAG(IS_MAC)
// static
std::optional<int> ChromeAuthenticatorRequestDelegate::DaysSinceDate(
    const std::string& formatted_date,
    const base::Time now) {
  std::vector<std::string> parts = base::SplitString(
      formatted_date, "-", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  if (parts.size() != 3) {
    return std::nullopt;
  }

  int year, month, day_of_month;
  if (!base::StringToInt(parts[0], &year) ||
      !base::StringToInt(parts[1], &month) ||
      !base::StringToInt(parts[2], &day_of_month)) {
    return std::nullopt;
  }

  const base::Time::Exploded exploded = {
      .year = year, .month = month, .day_of_month = day_of_month};

  base::Time t;
  if (!base::Time::FromUTCExploded(exploded, &t) || now < t) {
    return std::nullopt;
  }

  const base::TimeDelta difference = now - t;
  return difference.InDays();
}

// static
std::optional<bool> ChromeAuthenticatorRequestDelegate::GetICloudKeychainPref(
    const PrefService* prefs) {
  const PrefService::Preference* pref =
      prefs->FindPreference(prefs::kCreatePasskeysInICloudKeychain);
  if (pref->IsDefaultValue()) {
    return std::nullopt;
  }
  return pref->GetValue()->GetBool();
}

// static
bool ChromeAuthenticatorRequestDelegate::IsActiveProfileAuthenticatorUser(
    const PrefService* prefs) {
  const std::string& last_used = prefs->GetString(kWebAuthnTouchIdLastUsed);
  if (last_used.empty()) {
    return false;
  }
  const std::optional<int> days = DaysSinceDate(last_used, base::Time::Now());
  return days.has_value() && days.value() <= kMacOsRecentlyUsedMaxDays;
}

// static
bool ChromeAuthenticatorRequestDelegate::ShouldCreateInICloudKeychain(
    RequestSource request_source,
    bool is_active_profile_authenticator_user,
    bool has_icloud_drive_enabled,
    bool request_is_for_google_com,
    std::optional<bool> preference) {
  // Secure Payment Confirmation and credit-card autofill continue to use
  // the profile authenticator.
  if (request_source != RequestSource::kWebAuthentication) {
    return false;
  }
  if (preference.has_value()) {
    return *preference;
  }
  const base::Feature* feature;
  if (request_is_for_google_com) {
    feature = &device::kWebAuthnICloudKeychainForGoogle;
  } else {
    if (is_active_profile_authenticator_user) {
      if (has_icloud_drive_enabled) {
        feature = &device::kWebAuthnICloudKeychainForActiveWithDrive;
      } else {
        feature = &device::kWebAuthnICloudKeychainForActiveWithoutDrive;
      }
    } else {
      if (has_icloud_drive_enabled) {
        feature = &device::kWebAuthnICloudKeychainForInactiveWithDrive;
      } else {
        feature = &device::kWebAuthnICloudKeychainForInactiveWithoutDrive;
      }
    }
  }

  return base::FeatureList::IsEnabled(*feature);
}

void ChromeAuthenticatorRequestDelegate::ConfigureNSWindow(
    device::FidoDiscoveryFactory* discovery_factory) {
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(GetRenderFrameHost());
  BrowserWindowInterface* browser =
      GlobalBrowserCollection::GetInstance()->FindBrowserWithTab(web_contents);
  if (browser && browser->GetType() == BrowserWindowInterface::TYPE_APP) {
    // PWAs render the UI in an out-of-process window, thus there is no valid
    // NSWindow* available in the browser process.
    // TODO: crbug.com/364926914 - potentially do iCloud Keychain operations out
    // of process so that they can work in PWAs.
    return;
  }

  // Not all contexts in which this code runs have a BrowserWindow.
  // Notably the dialog containing a WebContents that is used for signing
  // into a new profile does not. Thus the NSWindow is fetched more directly.
  const views::Widget* widget = views::Widget::GetTopLevelWidgetForNativeView(
      web_contents->GetNativeView());
  if (widget) {
    const gfx::NativeWindow window = widget->GetNativeWindow();
    if (window) {
      discovery_factory->set_nswindow(window);
    }
  }
}
void ChromeAuthenticatorRequestDelegate::ConfigureICloudKeychain(
    RequestSource request_source,
    const std::string& rp_id) {
  const PrefService* prefs = profile()->GetPrefs();
  const bool is_icloud_drive_enabled = IsICloudDriveEnabled();
  const bool is_active_profile_authenticator_user =
      IsActiveProfileAuthenticatorUser(prefs);
  dialog_controller_->set_allow_icloud_keychain(
      request_source == RequestSource::kWebAuthentication);
  dialog_controller_->set_has_icloud_drive_enabled(is_icloud_drive_enabled);
  dialog_controller_->set_should_create_in_icloud_keychain(
      ShouldCreateInICloudKeychain(
          request_source, is_active_profile_authenticator_user,
          is_icloud_drive_enabled, rp_id == "google.com",
          GetICloudKeychainPref(prefs)));
}

#endif

void ChromeAuthenticatorRequestDelegate::UpdateModelForTransportAvailability(
    const TransportAvailabilityInfo& tai) {
  dialog_model_->request_type = tai.request_type;
  dialog_model_->resident_key_requirement = tai.resident_key_requirement;
  dialog_model_->attestation_conveyance_preference =
      tai.attestation_conveyance_preference;
  dialog_model_->ble_adapter_is_powered =
      tai.ble_status == device::FidoRequestHandlerBase::BleStatus::kOn;
  dialog_model_->show_security_key_on_qr_sheet =
      tai.available_transports.contains(
          device::FidoTransportProtocol::kUsbHumanInterfaceDevice);
  dialog_model_->is_off_the_record = GetBrowserContext()->IsOffTheRecord();
  dialog_model_->platform_has_biometrics = tai.platform_has_biometrics;
}
