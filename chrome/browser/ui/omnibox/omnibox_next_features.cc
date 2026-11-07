// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/omnibox_next_features.h"

#include <string>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "build/build_config.h"
#include "components/omnibox/common/omnibox_features.h"
#include "ui/accessibility/platform/ax_platform.h"

namespace {
constexpr base::FeatureState DISABLED = base::FEATURE_DISABLED_BY_DEFAULT;
constexpr base::FeatureState ENABLED = base::FEATURE_ENABLED_BY_DEFAULT;
}  // namespace

namespace omnibox {

namespace internal {

// If enabled, shows the omnibox suggestions in the popup in WebUI.
BASE_FEATURE(kWebUIOmniboxPopup, ENABLED);

// If enabled, the Omnibox Popup will enable a different UI state when on a
// webpage.
BASE_FEATURE(kWebUIOmniboxSimplification, ENABLED);

// If enabled, then both the input row and suggestions dropdown (in the Omnibox)
// will be rendered using the WebUI stack (i.e. the cutout for the location bar
// will be removed).
BASE_FEATURE(kWebUIOmniboxFullPopup, DISABLED);

// Safety guard: if enabled, and a screen reader is detected the first time the
// full WebUI omnibox state is queried (i.e. at startup), the full WebUI omnibox
// popup is not used for the remainder of the session, even if
// `kWebUIOmniboxFullPopup` is enabled.
BASE_FEATURE(kDisableFullWebuiOmniboxForScreenreaderUsers, DISABLED);

bool IsWebUIOmniboxFullPopupEnabled() {
  if (!base::FeatureList::IsEnabled(internal::kWebUIOmniboxFullPopup)) {
    return false;
  }
  if (base::FeatureList::IsEnabled(
          internal::kDisableFullWebuiOmniboxForScreenreaderUsers)) {
    // Latched on first use, so that the full WebUI omnibox state stays
    // consistent for the whole session even if a screen reader is started or
    // stopped later on.
    static const bool kScreenReaderActiveAtStartup =
        ui::AXPlatform::GetInstance().IsScreenReaderActive();
    if (kScreenReaderActiveAtStartup) {
      return false;
    }
  }
  return true;
}

}  // namespace internal

// If enabled, disables caret color animation for the WebUI Omnibox popup.
BASE_FEATURE(kWebUIOmniboxDisableCaretColorAnimation, ENABLED);
// Enables the double click mechanism of sending selection set by
// passing click events through the WebView.
BASE_FEATURE(kWebUIOmniboxFullPopupDoubleClick, ENABLED);
// Enables the WebUI for omnibox suggestions without modifying the popup UI.
BASE_FEATURE(kWebUIOmniboxPopupDebug, DISABLED);
// Enables side-by-side comparison omnibox suggestions in WebUI and Views.
const base::FeatureParam<bool> kWebUIOmniboxPopupDebugSxSParam{
    &kWebUIOmniboxPopupDebug, "SxS", false};
// If enabled, the WebUIOmniboxPopup controls its own selection state instead of
// following that of the OmniboxEditModel.
BASE_FEATURE(kWebUIOmniboxPopupSelectionControl, DISABLED);

// If enabled, animates the caret in the omnibox.
BASE_FEATURE(kOmniboxAnimatedCaret, ENABLED);

// If enabled, enables energy effect in the omnibox.
BASE_FEATURE(kEnergyEffectInOmnibox, ENABLED);

// If enabled, prevents closing the popup while file chooser is open.
// Disabled due to focus restoration and popup deactivation issues.
BASE_FEATURE(kOmniboxKeepOpenOnFileSelection, DISABLED);

bool IsWebUIOmniboxPopupEnabled() {
  return base::FeatureList::IsEnabled(internal::kWebUIOmniboxPopup);
}

bool ShouldUseWebUIOmniboxFullHandler() {
  return internal::IsWebUIOmniboxFullPopupEnabled() &&
         base::FeatureList::IsEnabled(
             omnibox::kWebUISearchboxWithoutModelController);
}

bool IsWebUIOmniboxInBrowserViewEnabled() {
  return internal::IsWebUIOmniboxFullPopupEnabled() &&
         kWebUIOmniboxFullPopupUseBrowserView.Get();
}

bool ShouldDrawFullPopupShadowInWebUI() {
  return base::FeatureList::IsEnabled(kOmniboxFullWebUIShadow);
}

const base::FeatureParam<bool> kShowLensSearchChip(
    &internal::kWebUIOmniboxSimplification,
    "Omnibox_ShowLensSearchChip",
    false);
const base::FeatureParam<bool> kWebUIOmniboxFullPopupUseBrowserView{
    &internal::kWebUIOmniboxFullPopup, "Omnibox_UseBrowserView", false};
const base::FeatureParam<bool> kWebUIOmniboxFullPopupMultiline{
    &internal::kWebUIOmniboxFullPopup, "Omnibox_Multiline", false};
const base::FeatureParam<bool> kWebUIOmniboxFullPopupDogfoodFeedback{
    &internal::kWebUIOmniboxFullPopup, "Omnibox_DogfoodFeedback", false};
const base::FeatureParam<int> kWebUIOmniboxFullPopupSnapshotCacheSize{
    &internal::kWebUIOmniboxFullPopup, "Omnibox_SnapshotCacheSize", 10};
}  // namespace omnibox
