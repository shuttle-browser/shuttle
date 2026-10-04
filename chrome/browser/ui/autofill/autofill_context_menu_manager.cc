// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_context_menu_manager.h"

#include <algorithm>
#include <string>

#include "base/containers/fixed_flat_set.h"
#include "base/feature_list.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/notreached.h"
#include "base/values.h"
#include "build/branding_buildflags.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/variations/google_groups_manager_factory.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/password_manager/factories/password_counter_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/passwords/ui_utils.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webauthn/context_menu_helper.h"
#include "chrome/browser/user_education/user_education_service.h"
#include "chrome/grit/generated_resources.h"
#include "components/autofill/content/browser/content_autofill_client.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/core/browser/autofill_feedback_data.h"
#include "components/autofill/core/browser/foundations/autofill_driver.h"
#include "components/autofill/core/browser/foundations/autofill_manager.h"
#include "components/autofill/core/browser/integrators/autofill_ai/autofill_ai_manager.h"
#include "components/autofill/core/common/aliases.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/password_manager/content/browser/content_password_manager_driver.h"
#include "components/password_manager/core/browser/password_autofill_manager.h"
#include "components/password_manager/core/browser/password_counter.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/password_manual_fallback_metrics_recorder.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/renderer_context_menu/render_view_context_menu_base.h"
#include "components/variations/service/variations_service.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/base/ui_base_features.h"
#include "ui/color/color_id.h"
#include "ui/menus/simple_menu_model.h"
#include "url/origin.h"

namespace autofill {

namespace {

using ::password_manager::ContentPasswordManagerDriver;

bool ShouldShowAutofillContextMenu(const content::ContextMenuParams& params) {
  if (params.is_content_editable_for_autofill) {
    return true;
  }
  if (!params.form_control_type) {
    return false;
  }
  // Return true (only) on text fields.
  //
  // Note that this switch is over `blink::mojom::FormControlType`, not
  // `autofill::FormControlType`. Standard form controls are handled by this
  // switch, while `contenteditable` elements are handled by the
  // `is_content_editable_for_autofill` check above.
  //
  // TODO(crbug.com/40285492): Unify with functions from form_autofill_util.cc.
  switch (*params.form_control_type) {
    case blink::mojom::FormControlType::kInputEmail:
    case blink::mojom::FormControlType::kInputMonth:
    case blink::mojom::FormControlType::kInputNumber:
    case blink::mojom::FormControlType::kInputPassword:
    case blink::mojom::FormControlType::kInputSearch:
    case blink::mojom::FormControlType::kInputTelephone:
    case blink::mojom::FormControlType::kInputText:
    case blink::mojom::FormControlType::kInputUrl:
    case blink::mojom::FormControlType::kTextArea:
      return true;
    case blink::mojom::FormControlType::kButtonButton:
    case blink::mojom::FormControlType::kButtonSubmit:
    case blink::mojom::FormControlType::kButtonReset:
    case blink::mojom::FormControlType::kButtonPopover:
    case blink::mojom::FormControlType::kFieldset:
    case blink::mojom::FormControlType::kInputButton:
    case blink::mojom::FormControlType::kInputCheckbox:
    case blink::mojom::FormControlType::kInputColor:
    case blink::mojom::FormControlType::kInputDate:
    case blink::mojom::FormControlType::kInputDatetimeLocal:
    case blink::mojom::FormControlType::kInputFile:
    case blink::mojom::FormControlType::kInputHidden:
    case blink::mojom::FormControlType::kInputImage:
    case blink::mojom::FormControlType::kInputRadio:
    case blink::mojom::FormControlType::kInputRange:
    case blink::mojom::FormControlType::kInputReset:
    case blink::mojom::FormControlType::kInputSubmit:
    case blink::mojom::FormControlType::kInputTime:
    case blink::mojom::FormControlType::kInputWeek:
    case blink::mojom::FormControlType::kOutput:
    case blink::mojom::FormControlType::kSelectOne:
    case blink::mojom::FormControlType::kSelectMultiple:
      return false;
  }
  NOTREACHED();
}

// Returns true if the given id is one generated for autofill context menu.
bool IsAutofillCustomCommandId(
    AutofillContextMenuManager::CommandId command_id) {
  static constexpr auto kAutofillCommands = base::MakeFixedFlatSet<int>({
      IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PASSWORDS_SELECT_PASSWORD,
      IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PASSWORDS_IMPORT_PASSWORDS,
      IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PASSWORDS_SUGGEST_PASSWORD,
      IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PASSWORDS_USE_PASSKEY_FROM_ANOTHER_DEVICE,
  });
  return kAutofillCommands.contains(command_id.value());
}

// Returns true if the field is a username or password field.
bool IsPasswordFormField(ContentPasswordManagerDriver& password_manager_driver,
                         const content::ContextMenuParams& params) {
  const FieldRendererId current_field_renderer_id(
      params.field_renderer_id.value());
  return password_manager_driver.GetPasswordManager()
      ->GetPasswordFormCache()
      ->GetPasswordForm(&password_manager_driver, current_field_renderer_id);
}

}  // namespace

AutofillContextMenuManager::AutofillContextMenuManager(
    RenderViewContextMenuBase* delegate,
    ui::SimpleMenuModel* menu_model)
    : menu_model_(menu_model), delegate_(delegate) {
  DCHECK(delegate_);
  params_ = delegate_->params();
}

AutofillContextMenuManager::~AutofillContextMenuManager() = default;

void AutofillContextMenuManager::AppendItems() {
  if (params_.is_content_editable_for_autofill) {
    return;
  }

  MaybeAddAutofillManualFallbackItems();
}

bool AutofillContextMenuManager::IsCommandIdSupported(int command_id) {
  return IsAutofillCustomCommandId(CommandId(command_id));
}

bool AutofillContextMenuManager::IsCommandIdEnabled(int command_id) {
  return true;
}

void AutofillContextMenuManager::ExecuteCommand(int command_id) {
  content::RenderFrameHost* rfh = delegate_->GetRenderFrameHost();
  if (!rfh) {
    return;
  }
  if (command_id ==
      IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PASSWORDS_USE_PASSKEY_FROM_ANOTHER_DEVICE) {
    webauthn::OnPasskeyFromAnotherDeviceContextMenuItemSelected(rfh);
    return;
  }
  ContentAutofillDriver* autofill_driver =
      ContentAutofillDriver::GetForRenderFrameHost(rfh);
  if (!autofill_driver) {
    return;
  }
  CHECK(IsAutofillCustomCommandId(CommandId(command_id)));

  if (command_id ==
      IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PASSWORDS_SELECT_PASSWORD) {
    ExecuteFallbackForSelectPasswordCommand(*autofill_driver);
    return;
  }

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(rfh);
  if (command_id ==
      IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PASSWORDS_IMPORT_PASSWORDS) {
    // This function also records metrics.
    NavigateToManagePasswordsPage(
        GlobalBrowserCollection::GetInstance()->FindBrowserWithTab(
            web_contents),
        password_manager::ManagePasswordsReferrer::kPasswordContextMenu);
    return;
  }

  if (command_id ==
      IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PASSWORDS_SUGGEST_PASSWORD) {
    // This function also records metrics.
    password_manager_util::UserTriggeredManualGenerationFromContextMenu(
        ChromePasswordManagerClient::FromWebContents(web_contents),
        ContentAutofillClient::FromWebContents(web_contents));
    return;
  }
}

void AutofillContextMenuManager::MaybeAddAutofillManualFallbackItems() {
  if (!ShouldShowAutofillContextMenu(params_)) {
    // Autofill entries are only available in input or text area fields
    return;
  }

  content::RenderFrameHost* rfh = delegate_->GetRenderFrameHost();
  if (!rfh) {
    return;
  }

  ContentPasswordManagerDriver* password_manager_driver =
      ContentPasswordManagerDriver::GetForRenderFrameHost(rfh);

  bool add_passwords_fallback = false;

  // Do not show password manager context menu options for input fields that
  // cannot be filled by the driver. See crbug.com/40061116.
  if (password_manager_driver) {
    add_passwords_fallback =
        ShouldAddPasswordsManualFallbackItem(*password_manager_driver);
  }

  if (add_passwords_fallback) {
    Profile* profile = Profile::FromBrowserContext(rfh->GetBrowserContext());
    password_manager::PasswordCounter* counter =
        PasswordCounterFactory::GetForProfile(profile);
    const bool select_passwords_option_shown =
        counter && counter->autofillable_passwords() > 0;
    AddPasswordsManualFallbackItems(*password_manager_driver,
                                    select_passwords_option_shown);

    if (select_passwords_option_shown) {
      LogSelectPasswordManualFallbackContextMenuEntryShown(
          CHECK_DEREF(password_manager_driver));
    }
  }

  if (add_passwords_fallback) {
    menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);
  }
}

bool AutofillContextMenuManager::ShouldAddPasswordsManualFallbackItem(
    ContentPasswordManagerDriver& driver) {
  if (!driver.CanShowAutofillUi()) {
    return false;
  }
  // Password suggestions should not be triggered on text areas.
  if (params_.form_control_type == blink::mojom::FormControlType::kTextArea) {
    return false;
  }

  if (base::FeatureList::IsEnabled(
          password_manager::features::kPasswordManualFallbackSecurityChecks) &&
      (!driver.HasValidURL(/*may_kill_renderer*/ false) ||
       !driver.IsRenderFrameHostSupported())) {
    return false;
  }
  return driver.GetPasswordManager()->GetClient()->IsFillingEnabled(
      driver.GetLastCommittedOrigin(), driver.GetLastCommittedURL());
}

void AutofillContextMenuManager::AddPasswordsManualFallbackItems(
    ContentPasswordManagerDriver& password_manager_driver,
    bool add_select_password_option) {
  const bool add_password_generation_option =
      password_manager_util::ManualPasswordGenerationEnabled(
          &password_manager_driver) &&
      password_manager_driver.IsPasswordFieldForPasswordManager(
          FieldRendererId(params_.field_renderer_id.value()),
          params_.form_control_type);
  const bool add_passkey_from_another_device_option =
      webauthn::IsPasskeyFromAnotherDeviceContextMenuEnabled(
          delegate_->GetRenderFrameHost(), params_.form_renderer_id.value(),
          params_.field_renderer_id.value());
  const bool add_import_passwords_option = !add_select_password_option;

  if (add_select_password_option) {
    menu_model_->AddItemWithStringId(
        IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PASSWORDS_SELECT_PASSWORD,
        IDS_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PASSWORDS_SELECT_PASSWORD);

    if (::features::IsMenuSimplificationEnabled()) {
      menu_model_->SetIconForCommandId(
          IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PASSWORDS_SELECT_PASSWORD,
          ui::ImageModel::FromVectorIcon(
              ::features::IsRoundedIconsEnabled()
                  ? vector_icons::kPasswordManagerIcon
                  : vector_icons::kPasswordManagerOldIcon,
              ui::kColorMenuIcon, ui::SimpleMenuModel::kDefaultIconSize));
    }
  }
  if (add_password_generation_option) {
    menu_model_->AddItemWithStringId(
        IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PASSWORDS_SUGGEST_PASSWORD,
        IDS_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PASSWORDS_SUGGEST_PASSWORD);
  }
  if (add_passkey_from_another_device_option) {
    menu_model_->AddItemWithStringId(
        IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PASSWORDS_USE_PASSKEY_FROM_ANOTHER_DEVICE,
        IDS_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PASSWORDS_USE_PASSKEY_FROM_ANOTHER_DEVICE);
  }
  if (add_import_passwords_option) {
    menu_model_->AddItemWithStringId(
        IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PASSWORDS_IMPORT_PASSWORDS,
        IDS_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PASSWORDS_IMPORT_PASSWORDS);
  }
}

void AutofillContextMenuManager::
    LogSelectPasswordManualFallbackContextMenuEntryShown(
        ContentPasswordManagerDriver& password_manager_driver) {
  password_manager_driver.GetPasswordAutofillManager()
      ->GetPasswordManualFallbackMetricsRecorder()
      .ContextMenuEntryShown(
          /*classified_as_target_filling_password=*/
          IsPasswordFormField(password_manager_driver, params_));
}

void AutofillContextMenuManager::
    LogSelectPasswordManualFallbackContextMenuEntryAccepted() {
  content::RenderFrameHost* rfh = delegate_->GetRenderFrameHost();
  ContentPasswordManagerDriver* password_manager_driver =
      rfh ? ContentPasswordManagerDriver::GetForRenderFrameHost(rfh) : nullptr;

  if (password_manager_driver) {
    password_manager_driver->GetPasswordAutofillManager()
        ->GetPasswordManualFallbackMetricsRecorder()
        .ContextMenuEntryAccepted(/*classified_as_target_filling_password=*/
                                  IsPasswordFormField(*password_manager_driver,
                                                      params_));
  }
}

void AutofillContextMenuManager::ExecuteFallbackForSelectPasswordCommand(
    AutofillDriver& autofill_driver) {
  autofill_driver.RendererShouldTriggerSuggestions(
      /*field_id=*/{autofill_driver.GetFrameToken(),
                    FieldRendererId(params_.field_renderer_id.value())},
      AutofillSuggestionTriggerSource::kManualFallbackPasswords);

  LogSelectPasswordManualFallbackContextMenuEntryAccepted();
}

void AutofillContextMenuManager::MaybeMarkLastItemAsNewFeature(
    const base::Feature& feature) {
  menu_model_->SetIsNewFeatureAt(menu_model_->GetItemCount() - 1,
                                 UserEducationService::MaybeShowNewBadge(
                                     delegate_->GetBrowserContext(), feature));
}

}  // namespace autofill
