// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_placeholder_util.h"

#include <string>

#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/grit/generated_resources.h"
#include "components/omnibox/browser/omnibox_client.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "ui/base/l10n/l10n_util.h"

namespace omnibox {

bool ShouldUseAskPlaceholderText(
    bool is_aim_popup_enabled,
    const TemplateURLService* template_url_service) {
  // Shuttle: the AI mode popup is removed, so the "Ask" placeholder is unused.
  return false;
}

void ComputePlaceholderText(
    OmniboxController& controller,
    std::u16string& out_placeholder_text,
    std::optional<std::u16string>& out_a11y_placeholder) {
  out_a11y_placeholder = std::nullopt;
  out_placeholder_text = std::u16string();
  OmniboxClient* client = controller.client();
  if (!controller.edit_model()->keyword_placeholder().empty()) {
    // If `keyword_placeholder()` is set, then the user is in a keyword mode
    // that has placeholder text, so display that.
    out_placeholder_text = controller.edit_model()->keyword_placeholder();
  } else if (ShouldInstallContextualTasksPlaceholderText(controller)) {
    // For Contextual Tasks page, use the page title as placeholder text.
    out_placeholder_text = client->GetTitle();
  } else if (const auto* default_provider =
                 client->GetTemplateURLService()->GetDefaultSearchProvider()) {
    // If a DSE is set, use the DSE placeholder text.
    out_placeholder_text = l10n_util::GetStringFUTF16(
        IDS_OMNIBOX_PLACEHOLDER_TEXT, default_provider->short_name());
  }
}

bool ShouldShowPlaceholderText(const OmniboxController& controller) {
  // If there's keyword placeholder to show, always show it, regardless of
  // whether the omnibox is focused, because users won't enter keyword mode,
  // blur the omnibox, read the placeholder text, refocus the omnibox, and begin
  // typing.
  if (!controller.edit_model()->keyword_placeholder().empty()) {
    return true;
  }

  // If the omnibox is blurred, only show the DSE placeholder if there is no
  // keyword selected.
  if (!controller.edit_model()->is_caret_visible()) {
    return !controller.edit_model()->is_keyword_selected();
  }

  // If the omnibox is focused, no placeholder is shown.
  return false;
}

bool ShouldUseDimPlaceholderColor(OmniboxController& controller) {
  // Contextual tasks placeholder text and keyword
  // placeholders are dim to differentiate from user input. DSE placeholders are
  // not dim to draw attention to the omnibox and because the omnibox is
  // unfocused so there's less risk of confusion with user input.
  bool dse_placeholder_installed =
      controller.edit_model()->keyword_placeholder().empty() &&
      !ShouldInstallContextualTasksPlaceholderText(controller);
  return !dse_placeholder_installed;
}

bool ShouldInstallContextualTasksPlaceholderText(
    const OmniboxController& controller) {
  return false;
}

}  // namespace omnibox
