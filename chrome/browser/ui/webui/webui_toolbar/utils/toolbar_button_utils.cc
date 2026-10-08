// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/webui_toolbar/utils/toolbar_button_utils.h"

#include "base/notreached.h"
#include "build/branding_buildflags.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/page_action/action_ids.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_toolbar_actions_ids.h"
#include "components/browser_apis/ui_controllers/toolbar/toolbar_ui_api_data_model.mojom.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/actions/actions.h"
#include "ui/base/ui_base_features.h"
#include "ui/views/vector_icons.h"

namespace webui_toolbar {

DECLARE_ELEMENT_IDENTIFIER_VALUE(kToolbarClearBrowsingDataElementId);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kToolbarCopyUrlElementId);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kToolbarDevToolsElementId);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kToolbarNewIncognitoWindowElementId);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kToolbarPrintElementId);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kToolbarQrCodeGeneratorElementId);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kToolbarRouteMediaElementId);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kToolbarShowAddressesBubbleOrPageElementId);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kToolbarShowDownloadsElementId);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kToolbarShowPasswordsBubbleOrPageElementId);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kToolbarShowPaymentsBubbleOrPageElementId);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kToolbarShowTranslateElementId);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kToolbarTabSearchElementId);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kToolbarTaskManagerElementId);

DEFINE_ELEMENT_IDENTIFIER_VALUE(kToolbarClearBrowsingDataElementId);
DEFINE_ELEMENT_IDENTIFIER_VALUE(kToolbarCopyUrlElementId);
DEFINE_ELEMENT_IDENTIFIER_VALUE(kToolbarDevToolsElementId);
DEFINE_ELEMENT_IDENTIFIER_VALUE(kToolbarNewIncognitoWindowElementId);
DEFINE_ELEMENT_IDENTIFIER_VALUE(kToolbarPrintElementId);
DEFINE_ELEMENT_IDENTIFIER_VALUE(kToolbarQrCodeGeneratorElementId);
DEFINE_ELEMENT_IDENTIFIER_VALUE(kToolbarRouteMediaElementId);
DEFINE_ELEMENT_IDENTIFIER_VALUE(kToolbarShowAddressesBubbleOrPageElementId);
DEFINE_ELEMENT_IDENTIFIER_VALUE(kToolbarShowDownloadsElementId);
DEFINE_ELEMENT_IDENTIFIER_VALUE(kToolbarShowPasswordsBubbleOrPageElementId);
DEFINE_ELEMENT_IDENTIFIER_VALUE(kToolbarShowPaymentsBubbleOrPageElementId);
DEFINE_ELEMENT_IDENTIFIER_VALUE(kToolbarShowTranslateElementId);
DEFINE_ELEMENT_IDENTIFIER_VALUE(kToolbarTabSearchElementId);
DEFINE_ELEMENT_IDENTIFIER_VALUE(kToolbarTaskManagerElementId);

std::vector<ui::ElementIdentifier> GetPinnedToolbarActionElementIds() {
  return {webui_toolbar::kToolbarClearBrowsingDataElementId,
          webui_toolbar::kToolbarCopyUrlElementId,
          webui_toolbar::kToolbarDevToolsElementId,
          webui_toolbar::kToolbarNewIncognitoWindowElementId,
          webui_toolbar::kToolbarPrintElementId,
          webui_toolbar::kToolbarQrCodeGeneratorElementId,
          webui_toolbar::kToolbarRouteMediaElementId,
          webui_toolbar::kToolbarShowAddressesBubbleOrPageElementId,
          webui_toolbar::kToolbarShowDownloadsElementId,
          webui_toolbar::kToolbarShowPasswordsBubbleOrPageElementId,
          webui_toolbar::kToolbarShowPaymentsBubbleOrPageElementId,
          webui_toolbar::kToolbarShowTranslateElementId,
          webui_toolbar::kToolbarTabSearchElementId,
          webui_toolbar::kToolbarTaskManagerElementId};
}

ui::ElementIdentifier ActionIdToElementIdentifier(actions::ActionId action) {
  if (auto id = pinned_toolbar_actions::GetElementIdentifierForAction(action)) {
    return id;
  }
  switch (action) {
    case kActionShowPasswordsBubbleOrPage:
      return kToolbarShowPasswordsBubbleOrPageElementId;
    case kActionShowPaymentsBubbleOrPage:
      return kToolbarShowPaymentsBubbleOrPageElementId;
    case kActionShowAddressesBubbleOrPage:
      return kToolbarShowAddressesBubbleOrPageElementId;
    case kActionShowDownloads:
      return kToolbarShowDownloadsElementId;
    case kActionClearBrowsingData:
      return kToolbarClearBrowsingDataElementId;
    case kActionPrint:
      return kToolbarPrintElementId;
    case kActionShowTranslate:
      return kToolbarShowTranslateElementId;
    case kActionQrCodeGenerator:
      return kToolbarQrCodeGeneratorElementId;
    case kActionRouteMedia:
      return kToolbarRouteMediaElementId;
    case kActionCopyUrl:
      return kToolbarCopyUrlElementId;
    case kActionTaskManager:
      return kToolbarTaskManagerElementId;
    case kActionDevTools:
      return kToolbarDevToolsElementId;
    case kActionTabSearch:
      return kToolbarTabSearchElementId;
    default:
      return ui::ElementIdentifier();
  }
}

std::optional<toolbar_ui_api::mojom::PinnedToolbarAction>
ActionItemToPinnedToolbarAction(actions::ActionItem* item) {
  auto id = item->GetActionId();
  CHECK(id);
  const gfx::VectorIcon* icon = nullptr;
  if (item->GetImage().IsVectorIcon()) {
    icon = item->GetImage().GetVectorIcon().vector_icon();
  }
  if (actions::IsActionClass<actions::StatefulImageActionItem>(item)) {
    auto* stateful_item = static_cast<actions::StatefulImageActionItem*>(item);
    if (stateful_item->GetStatefulImage().IsVectorIcon()) {
      icon = stateful_item->GetStatefulImage().GetVectorIcon().vector_icon();
    }
  }
  switch (*id) {
    case kActionShowPasswordsBubbleOrPage:
      CHECK_EQ(icon, &(features::IsRoundedIconsEnabled()
                           ? vector_icons::kPasswordManagerIcon
                           : vector_icons::kPasswordManagerOldIcon));
      return toolbar_ui_api::mojom::PinnedToolbarAction::
          kShowPasswordsBubbleOrPage;
    case kActionShowPaymentsBubbleOrPage:
      CHECK_EQ(icon, &(features::IsRoundedIconsEnabled()
                           ? kCreditCardIcon
                           : kCreditCardChromeRefreshOldIcon));
      return toolbar_ui_api::mojom::PinnedToolbarAction::
          kShowPaymentsBubbleOrPage;
    case kActionShowAddressesBubbleOrPage:
      CHECK_EQ(icon, &(features::IsRoundedIconsEnabled()
                           ? vector_icons::kLocationOnIcon
                           : vector_icons::kLocationOnChromeRefreshOldIcon));
      return toolbar_ui_api::mojom::PinnedToolbarAction::
          kShowAddressesBubbleOrPage;
    case kActionShowDownloads:
      CHECK_EQ(icon, &(features::IsRoundedIconsEnabled()
                           ? kDownloadIcon
                           : kDownloadToolbarButtonChromeRefreshOldIcon));
      return toolbar_ui_api::mojom::PinnedToolbarAction::kShowDownloads;
    case kActionClearBrowsingData:
      CHECK_EQ(icon,
               &(features::IsRoundedIconsEnabled() ? kDeleteIcon
                                                   : kTrashCanRefreshOldIcon));
      return toolbar_ui_api::mojom::PinnedToolbarAction::kClearBrowsingData;
    case kActionPrint:
      CHECK_EQ(icon, &(features::IsRoundedIconsEnabled() ? kPrintIcon
                                                         : kPrintMenuOldIcon));
      return toolbar_ui_api::mojom::PinnedToolbarAction::kPrint;
    case kActionShowTranslate:
      CHECK_EQ(icon, &vector_icons::kGTranslateIcon);
      return toolbar_ui_api::mojom::PinnedToolbarAction::kShowTranslate;
    case kActionQrCodeGenerator:
      CHECK_EQ(icon, &(features::IsRoundedIconsEnabled()
                           ? kQrCodeIcon
                           : kQrCodeChromeRefreshOldIcon));
      return toolbar_ui_api::mojom::PinnedToolbarAction::kQrCodeGenerator;
    case kActionRouteMedia:
      CHECK(
          icon == &(features::IsRoundedIconsEnabled()
                        ? vector_icons::kCastIcon
                        : vector_icons::kMediaRouterIdleChromeRefreshOldIcon) ||
          icon ==
              &(features::IsRoundedIconsEnabled()
                    ? vector_icons::kCastWarningIcon
                    : vector_icons::kMediaRouterWarningChromeRefreshOldIcon) ||
          icon == &(features::IsRoundedIconsEnabled()
                        ? vector_icons::kCastPauseIcon
                        : vector_icons::kMediaRouterPausedOldIcon) ||
          icon ==
              &(features::IsRoundedIconsEnabled()
                    ? vector_icons::kCastConnectedIcon
                    : vector_icons::kMediaRouterActiveChromeRefreshOldIcon) ||
          icon == &(features::IsRoundedIconsEnabled()
                        ? kCastIcon
                        : kCastChromeRefreshOldIcon));
      return toolbar_ui_api::mojom::PinnedToolbarAction::kRouteMedia;
    case kActionCopyUrl:
      CHECK_EQ(icon, &(features::IsRoundedIconsEnabled()
                           ? vector_icons::kLinkIcon
                           : kLinkChromeRefreshOldIcon));
      return toolbar_ui_api::mojom::PinnedToolbarAction::kCopyUrl;
    case kActionSendTabToSelf:
      CHECK_EQ(icon, &(features::IsRoundedIconsEnabled()
                           ? kDevicesIcon
                           : kDevicesChromeRefreshOldIcon));
      return toolbar_ui_api::mojom::PinnedToolbarAction::kSendTabToSelf;
    case kActionTaskManager:
      CHECK_EQ(icon, &vector_icons::kTableChartIcon);
      return toolbar_ui_api::mojom::PinnedToolbarAction::kTaskManager;
    case kActionDevTools:
      CHECK_EQ(icon,
               &(features::IsRoundedIconsEnabled() ? kCodeIcon
                                                   : kDeveloperToolsOldIcon));
      return toolbar_ui_api::mojom::PinnedToolbarAction::kDevTools;
    case kActionTabSearch:
      CHECK_EQ(icon, &(features::IsRoundedIconsEnabled()
                           ? kManageSearchIcon
                           : kTabSearchTabStripOldIcon));
      return toolbar_ui_api::mojom::PinnedToolbarAction::kTabSearch;
    case kActionSendSharedTabGroupFeedback:
      CHECK_EQ(icon, &(features::IsRoundedIconsEnabled()
                           ? vector_icons::kFeedbackIcon
                           : vector_icons::kFeedbackOldIcon));
      return toolbar_ui_api::mojom::PinnedToolbarAction::
          kSendSharedTabGroupFeedback;
    default:
      return std::nullopt;
  }
}

std::optional<actions::ActionId> PinnedToolbarActionToActionId(
    toolbar_ui_api::mojom::PinnedToolbarAction action) {
  switch (action) {
    case toolbar_ui_api::mojom::PinnedToolbarAction::kUnspecified:
      return std::nullopt;
    case toolbar_ui_api::mojom::PinnedToolbarAction::kNewIncognitoWindow:
      return std::nullopt;
    case toolbar_ui_api::mojom::PinnedToolbarAction::kShowPasswordsBubbleOrPage:
      return kActionShowPasswordsBubbleOrPage;
    case toolbar_ui_api::mojom::PinnedToolbarAction::kShowPaymentsBubbleOrPage:
      return kActionShowPaymentsBubbleOrPage;
    case toolbar_ui_api::mojom::PinnedToolbarAction::kShowAddressesBubbleOrPage:
      return kActionShowAddressesBubbleOrPage;
    case toolbar_ui_api::mojom::PinnedToolbarAction::kShowDownloads:
      return kActionShowDownloads;
    case toolbar_ui_api::mojom::PinnedToolbarAction::kClearBrowsingData:
      return kActionClearBrowsingData;
    case toolbar_ui_api::mojom::PinnedToolbarAction::kPrint:
      return kActionPrint;
    case toolbar_ui_api::mojom::PinnedToolbarAction::kShowTranslate:
      return kActionShowTranslate;
    case toolbar_ui_api::mojom::PinnedToolbarAction::kQrCodeGenerator:
      return kActionQrCodeGenerator;
    case toolbar_ui_api::mojom::PinnedToolbarAction::kRouteMedia:
      return kActionRouteMedia;
    case toolbar_ui_api::mojom::PinnedToolbarAction::kCopyUrl:
      return kActionCopyUrl;
    case toolbar_ui_api::mojom::PinnedToolbarAction::kSendTabToSelf:
      return kActionSendTabToSelf;
    case toolbar_ui_api::mojom::PinnedToolbarAction::kTaskManager:
      return kActionTaskManager;
    case toolbar_ui_api::mojom::PinnedToolbarAction::kDevTools:
      return kActionDevTools;
    case toolbar_ui_api::mojom::PinnedToolbarAction::kTabSearch:
      return kActionTabSearch;
    case toolbar_ui_api::mojom::PinnedToolbarAction::
        kSendSharedTabGroupFeedback:
      return kActionSendSharedTabGroupFeedback;
    case toolbar_ui_api::mojom::PinnedToolbarAction::kDivider:
      return std::nullopt;
  }
  return std::nullopt;
}

using MojomPageActionId = toolbar_ui_api::mojom::PageActionId;

actions::ActionId MojomPageActionIdToActionId(
    MojomPageActionId page_action_id) {
  switch (page_action_id) {
    case MojomPageActionId::kActionIndigo:
      return kActionIndigo;
    case MojomPageActionId::kActionMultistepFilter:
      return kActionMultistepFilter;
    case MojomPageActionId::kActionLensOverlayHomework:
      return kActionLensOverlayHomework;
    case MojomPageActionId::kActionShowTranslate:
      return kActionShowTranslate;
    case MojomPageActionId::kActionShowMemorySaverChip:
      return kActionShowMemorySaverChip;
    case MojomPageActionId::kActionShowJsOptimizationsIcon:
      return kActionShowJsOptimizationsIcon;
    case MojomPageActionId::kActionRecordReplay:
      return kActionRecordReplay;
    case MojomPageActionId::kActionShowIntentPicker:
      return kActionShowIntentPicker;
    case MojomPageActionId::kActionShowZoomBubble:
      return kActionShowZoomBubble;
    case MojomPageActionId::kActionOffersAndRewardsForPage:
      return kActionOffersAndRewardsForPage;
    case MojomPageActionId::kActionShowFileSystemAccess:
      return kActionShowFileSystemAccess;
    case MojomPageActionId::kActionInstallPwa:
      return kActionInstallPwa;
    case MojomPageActionId::kActionCommercePriceInsights:
      return kActionCommercePriceInsights;
    case MojomPageActionId::kActionCommerceDiscounts:
      return kActionCommerceDiscounts;
    case MojomPageActionId::kActionShowPasswordsBubbleOrPage:
      return kActionShowPasswordsBubbleOrPage;
    case MojomPageActionId::kActionShowCollaborationRecentActivity:
      return kActionShowCollaborationRecentActivity;
    case MojomPageActionId::kActionAutofillMandatoryReauth:
      return kActionAutofillMandatoryReauth;
    case MojomPageActionId::kActionFind:
      return kActionFind;
    case MojomPageActionId::kActionShowCookieControls:
      return kActionShowCookieControls;
    case MojomPageActionId::kActionShowAddressesBubbleOrPage:
      return kActionShowAddressesBubbleOrPage;
    case MojomPageActionId::kActionVirtualCardEnroll:
      return kActionVirtualCardEnroll;
    case MojomPageActionId::kActionFilledCardInformation:
      return kActionFilledCardInformation;
    case MojomPageActionId::kActionShowPaymentsBubbleOrPage:
      return kActionShowPaymentsBubbleOrPage;
    case MojomPageActionId::kActionBookmarkThisTab:
      return kActionBookmarkThisTab;
    case MojomPageActionId::kActionFederation:
      return kActionFederation;
    case MojomPageActionId::kActionGlicContextualCueing:
      return kActionGlicContextualCueing;
    case MojomPageActionId::kActionAnchoredContextualCue:
      return kActionAnchoredContextualCue;
    case MojomPageActionId::kActionWebAuthnAmbientSignin:
      return kActionWebAuthnAmbientSignin;
    case MojomPageActionId::kActionAutofillPayment:
      return kActionAutofillPayment;
    case MojomPageActionId::kActionShowPaymentsChurnedUsersBubble:
      return kActionShowPaymentsChurnedUsersBubble;
    case MojomPageActionId::kActionFakePageActionForDebug:
      return kActionFakePageActionForDebug;
    case MojomPageActionId::kActionWalletReminderNotice:
      return kActionWalletReminderNotice;
  }
  NOTREACHED();
}

MojomPageActionId ActionIdToMojomPageActionId(actions::ActionId action_id) {
  switch (action_id) {
    case kActionIndigo:
      return MojomPageActionId::kActionIndigo;
    case kActionMultistepFilter:
      return MojomPageActionId::kActionMultistepFilter;
    case kActionLensOverlayHomework:
      return MojomPageActionId::kActionLensOverlayHomework;
    case kActionShowTranslate:
      return MojomPageActionId::kActionShowTranslate;
    case kActionShowMemorySaverChip:
      return MojomPageActionId::kActionShowMemorySaverChip;
    case kActionShowJsOptimizationsIcon:
      return MojomPageActionId::kActionShowJsOptimizationsIcon;
    case kActionRecordReplay:
      return MojomPageActionId::kActionRecordReplay;
    case kActionShowIntentPicker:
      return MojomPageActionId::kActionShowIntentPicker;
    case kActionShowZoomBubble:
      return MojomPageActionId::kActionShowZoomBubble;
    case kActionOffersAndRewardsForPage:
      return MojomPageActionId::kActionOffersAndRewardsForPage;
    case kActionShowFileSystemAccess:
      return MojomPageActionId::kActionShowFileSystemAccess;
    case kActionInstallPwa:
      return MojomPageActionId::kActionInstallPwa;
    case kActionCommercePriceInsights:
      return MojomPageActionId::kActionCommercePriceInsights;
    case kActionCommerceDiscounts:
      return MojomPageActionId::kActionCommerceDiscounts;
    case kActionShowPasswordsBubbleOrPage:
      return MojomPageActionId::kActionShowPasswordsBubbleOrPage;
    case kActionShowCollaborationRecentActivity:
      return MojomPageActionId::kActionShowCollaborationRecentActivity;
    case kActionAutofillMandatoryReauth:
      return MojomPageActionId::kActionAutofillMandatoryReauth;
    case kActionFind:
      return MojomPageActionId::kActionFind;
    case kActionShowCookieControls:
      return MojomPageActionId::kActionShowCookieControls;
    case kActionShowAddressesBubbleOrPage:
      return MojomPageActionId::kActionShowAddressesBubbleOrPage;
    case kActionVirtualCardEnroll:
      return MojomPageActionId::kActionVirtualCardEnroll;
    case kActionFilledCardInformation:
      return MojomPageActionId::kActionFilledCardInformation;
    case kActionShowPaymentsBubbleOrPage:
      return MojomPageActionId::kActionShowPaymentsBubbleOrPage;
    case kActionBookmarkThisTab:
      return MojomPageActionId::kActionBookmarkThisTab;
    case kActionFederation:
      return MojomPageActionId::kActionFederation;
    case kActionGlicContextualCueing:
      return MojomPageActionId::kActionGlicContextualCueing;
    case kActionAnchoredContextualCue:
      return MojomPageActionId::kActionAnchoredContextualCue;
    case kActionWebAuthnAmbientSignin:
      return MojomPageActionId::kActionWebAuthnAmbientSignin;
    case kActionAutofillPayment:
      return MojomPageActionId::kActionAutofillPayment;
    case kActionShowPaymentsChurnedUsersBubble:
      return MojomPageActionId::kActionShowPaymentsChurnedUsersBubble;
    case kActionFakePageActionForDebug:
      return MojomPageActionId::kActionFakePageActionForDebug;
    case kActionWalletReminderNotice:
      return MojomPageActionId::kActionWalletReminderNotice;
  }
  NOTREACHED();
}

}  // namespace webui_toolbar
