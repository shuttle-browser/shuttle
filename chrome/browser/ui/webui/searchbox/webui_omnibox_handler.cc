// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/searchbox/webui_omnibox_handler.h"

#include <memory>
#include <utility>

#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/types/expected.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/omnibox/chrome_omnibox_client.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/browser/ui/omnibox/omnibox_pedal_implementations.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_state_manager.h"
#include "chrome/browser/ui/omnibox/omnibox_tab_helper.h"
#include "chrome/browser/ui/omnibox/omnibox_view.h"
#include "chrome/browser/ui/search/omnibox_utils.h"
#include "chrome/browser/ui/tab_ui_helper.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/omnibox/omnibox_view_views.h"
#include "chrome/browser/ui/webui/cr_components/searchbox/searchbox_handler.h"
#include "chrome/browser/ui/webui/cr_components/searchbox/searchbox_omnibox_client.h"
#include "chrome/browser/ui/webui/metrics_reporter/metrics_reporter.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_ui.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_web_contents_helper.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "components/navigation_metrics/navigation_metrics.h"
#include "components/omnibox/browser/autocomplete_classifier.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/browser/autocomplete_controller_emitter.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/omnibox_client.h"
#include "components/omnibox/browser/omnibox_event_global_tracker.h"
#include "components/omnibox/browser/omnibox_log.h"
#include "components/omnibox/browser/omnibox_prefs.h"
#include "components/omnibox/browser/search_suggestion_parser.h"
#include "components/omnibox/browser/searchbox.mojom-shared.h"
#include "components/omnibox/browser/searchbox_utils.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/prefs/pref_service.h"
#include "components/profile_metrics/browser_profile_type.h"
#include "components/search_engines/template_url_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "net/base/url_util.h"
#include "net/cookies/cookie_util.h"
#include "third_party/metrics_proto/omnibox_focus_type.pb.h"
#include "third_party/omnibox_proto/types.pb.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/webui/resource_path.h"
#include "ui/base/window_open_disposition.h"
#include "ui/base/window_open_disposition_utils.h"
#include "ui/views/widget/widget.h"

namespace {

searchbox::mojom::SelectionLineState ConvertLineState(
    OmniboxPopupSelection::LineState state) {
  switch (state) {
    case OmniboxPopupSelection::LineState::kNormal:
      return searchbox::mojom::SelectionLineState::kNormal;
    case OmniboxPopupSelection::LineState::kKeywordMode:
      return searchbox::mojom::SelectionLineState::kKeywordMode;
    case OmniboxPopupSelection::LineState::kFocusedButtonAction:
      return searchbox::mojom::SelectionLineState::kFocusedButtonAction;
    case OmniboxPopupSelection::LineState::kFocusedButtonRemoveSuggestion:
      return searchbox::mojom::SelectionLineState::
          kFocusedButtonRemoveSuggestion;
    default:
      // WebUI omnibox doesn't support the other UIs and their focus states.
      NOTREACHED() << static_cast<int>(state);
  }
}

bool IsActiveTab(content::WebContents* web_contents,
                 std::optional<int32_t> tab_id) {
  if (!tab_id.has_value()) {
    // If no `tab_id` is specified (e.g. unit tests, transient window/tab
    // lifecycle states, or untagged searchbox contexts), fallback to treating
    // as active.
    return true;
  }
  auto* browser_window_interface =
      webui::GetBrowserWindowInterface(web_contents);
  if (!browser_window_interface) {
    return false;
  }
  tabs::TabInterface* active_tab =
      browser_window_interface->GetActiveTabInterface();
  if (!active_tab) {
    if (auto* tab_list = TabListInterface::From(browser_window_interface)) {
      active_tab = tab_list->GetActiveTab();
    }
  }
  return active_tab && tab_id.value() == active_tab->GetHandle().raw_value();
}

}  // namespace

WebuiOmniboxHandler::WebuiOmniboxHandler(
    mojo::PendingReceiver<searchbox::mojom::PageHandler> pending_page_handler,
    mojo::PendingRemote<searchbox::mojom::Page> pending_page,
    MetricsReporter* metrics_reporter,
    OmniboxController* omnibox_controller,
    content::WebUI* web_ui)
    : SearchboxHandler(std::move(pending_page_handler),
                       std::move(pending_page),
                       Profile::FromWebUI(web_ui),
                       web_ui->GetWebContents(),
                       /*client=*/nullptr),
      metrics_reporter_(metrics_reporter) {
  // Keep a reference to the OmniboxController instance owned by the
  // `OmniboxView`.
  CHECK(omnibox_controller);
  if (auto* browser_window_interface =
          webui::GetBrowserWindowInterface(web_ui->GetWebContents())) {
    if (auto* tab_list = TabListInterface::From(browser_window_interface)) {
      tab_list_observation_.Observe(tab_list);
    }
  }
  controller_ = omnibox_controller;
  SendAvailableKeywordModels();
  autocomplete_controller_observation_.Observe(autocomplete_controller());
  edit_model_observation_.Observe(omnibox_controller->edit_model());

  if (tabs::TabInterface* tab =
          tabs::TabInterface::MaybeGetFromContents(web_ui->GetWebContents())) {
    tab_will_detach_subscription_ = tab->RegisterWillDetach(base::BindRepeating(
        &WebuiOmniboxHandler::OnTabWillDetach, base::Unretained(this)));
    tab_did_insert_subscription_ = tab->RegisterDidInsert(base::BindRepeating(
        &WebuiOmniboxHandler::OnTabDidInsert, base::Unretained(this)));
  }


  // Ensure the page receives the current autocomplete state on startup.
  // This handles the case where results are generated before the remote is
  // bound and the handler is created and starts observing the
  // AutocompleteController.
  if (base::FeatureList::IsEnabled(
          omnibox::kOmniboxWebUIPopupStabilizeStartupShow)) {
    OnResultChanged(controller_->autocomplete_controller(), false);
  }
}

WebuiOmniboxHandler::~WebuiOmniboxHandler() = default;

void WebuiOmniboxHandler::ActivateKeyword(
    uint8_t line,
    const GURL& url,
    base::TimeTicks match_selection_timestamp,
    bool is_mouse_event) {
  const AutocompleteMatch* match = GetMatchWithUrl(line, url);
  if (!match) {
    // This can happen due to asynchronous updates changing the result while
    // the web UI is referencing a stale match.
    return;
  }
  // The rest of this function mirrors
  // `OmniboxSuggestionButtonRowView::ButtonPressed()`.
  OmniboxPopupSelection selection(
      line, OmniboxPopupSelection::LineState::kKeywordMode);
  // Note: Since keyword mode logic depends on state of the edit model, the
  // selection must first be set to prepare for keyword mode before accepting.
  edit_model()->SetPopupSelection(selection);
  // Don't re-enter keyword mode if already in it. This occurs when the user
  // was in keyword mode and re-clicked the same or a different keyword chip.
  if (edit_model()->is_keyword_hint()) {
    const auto entry_method = is_mouse_event
                                  ? metrics::OmniboxEventProto::CLICK_HINT_VIEW
                                  : metrics::OmniboxEventProto::TAP_HINT_VIEW;
    edit_model()->AcceptKeyword(entry_method);
  }
}

void WebuiOmniboxHandler::QueryAutocomplete(
    int32_t query_id,
    std::optional<int32_t> tab_id,
    const std::u16string& input,
    bool prevent_inline_autocomplete,
    uint32_t cursor_position,
    bool is_on_focus,
    const std::string& keyword,
    searchbox::mojom::InputMethod input_method) {
  if (!omnibox::internal::IsWebUIOmniboxFullPopupEnabled()) {
    DCHECK(!tab_id.has_value())
        << "QueryAutocomplete with tab_id is only supported when WebUI Omnibox "
           "full popup is enabled.";
    SearchboxHandler::QueryAutocomplete(
        query_id, /*tab_id=*/std::nullopt, input, prevent_inline_autocomplete,
        cursor_position, is_on_focus, keyword, input_method);
    return;
  }

  if (!IsActiveTab(web_contents_.get(), tab_id)) {
    // Query IPC arrived for a background tab (e.g., user switched tabs while
    // IPC was in flight). Update that tab's saved draft text without querying
    // autocomplete.
    tabs::TabInterface* const target_tab =
        tab_id.has_value() ? tabs::TabHandle(tab_id.value()).Get() : nullptr;
    auto* const current_window =
        webui::GetBrowserWindowInterface(web_contents_.get());
    // Ensure `target_tab` exists, belongs to the same window/profile,
    // and has a valid WebContents before mutating draft text.
    const bool belongs_to_same_window =
        target_tab &&
        (!current_window ||
         target_tab->GetBrowserWindowInterface() == current_window);
    if (belongs_to_same_window && target_tab->GetContents()) {
      OmniboxViewViews::SetUserTextForTab(target_tab->GetContents(), input,
                                          cursor_position);
    }
    return;
  }

  SearchboxHandler::QueryAutocomplete(
      query_id, /*tab_id=*/std::nullopt, input, prevent_inline_autocomplete,
      cursor_position, is_on_focus, keyword, input_method);

  // Ensure the `OmniboxEditModel` reflects the current input state.
  if (auto* view = edit_model()->view()) {
    view->SetWindowTextAndCaretPos(input, cursor_position,
                                   /*update_popup=*/false,
                                   /*notify_text_changed=*/false);
  }
}

void WebuiOmniboxHandler::StepSelection(
    OmniboxPopupSelection::Direction direction,
    OmniboxPopupSelection::Step step) {
  searchbox::mojom::SelectionStep mojom_step;
  switch (step) {
    case OmniboxPopupSelection::Step::kWholeLine: {
      mojom_step = searchbox::mojom::SelectionStep::kWholeLine;
      break;
    }
    case OmniboxPopupSelection::Step::kStateOrLine: {
      mojom_step = searchbox::mojom::SelectionStep::kStateOrLine;
      break;
    }
    case OmniboxPopupSelection::Step::kAllLines: {
      mojom_step = searchbox::mojom::SelectionStep::kAllLines;
      break;
    }
    default: {
      NOTREACHED();
    }
  }
  page_->StepSelection(direction == OmniboxPopupSelection::Direction::kForward
                           ? searchbox::mojom::SelectionDirection::kForward
                           : searchbox::mojom::SelectionDirection::kBackward,
                       mojom_step);
}

void WebuiOmniboxHandler::OpenCurrentSelection(
    WindowOpenDisposition disposition) {
  page_->OpenCurrentSelection(disposition);
}

WindowOpenDisposition WebuiOmniboxHandler::ComputeWindowOpenDisposition(
    uint8_t mouse_button,
    bool alt_key,
    bool ctrl_key,
    bool meta_key,
    bool shift_key,
    bool via_keyboard) {
  return via_keyboard
             ? searchbox::ComputeOpenDispositionFromModifiersAndLogToUma(
                   shift_key, ctrl_key, alt_key, meta_key)
             : ui::DispositionFromClick(
                   /*middle_button=*/mouse_button == 1, alt_key, ctrl_key,
                   meta_key, shift_key);
}


bool WebuiOmniboxHandler::SupportsKeywordMode() const {
  return true;
}

void WebuiOmniboxHandler::OverrideIconPaths(
    const AutocompleteMatch& match,
    searchbox::mojom::AutocompleteMatch* mojom_match) const {
  SearchboxHandler::OverrideIconPaths(match, mojom_match);
}

void WebuiOmniboxHandler::OnFocusChanged(bool focused) {
  if (focused) {
    if (omnibox::internal::IsWebUIOmniboxFullPopupEnabled()) {
      if (omnibox_controller() &&
          omnibox_controller()->popup_state_manager()->popup_state() ==
              OmniboxPopupState::kNone) {
        return;
      }
    }
    edit_model()->OnSetFocus(/*control_down=*/false);
  } else {
    edit_model()->OnWillKillFocus();
    // Delay killing focus for full popup until state is properly synced in
    // omnibox_popup_view_full_webui's `OnTabChanged`
    if (!omnibox::internal::IsWebUIOmniboxFullPopupEnabled()) {
      // Kill focus on focus loss to properly terminate the edit session and
      // reset popup and keyword state.
      edit_model()->OnKillFocus();
    }
  }
}

// TODO(crbug.com/469098088): Use something other than
//   `AutocompleteController::Observer::OnStart()` to reduce the IPC overhead
//   due to the fact that `AutocompleteController::Start()` gets invoked on
//   *every* keystroke in the Omnibox.
void WebuiOmniboxHandler::OnStart(AutocompleteController* controller,
                                  const AutocompleteInput& input) {
  // Lens was removed, so the Lens entrypoint is never eligible.
  page_->UpdateLensSearchEligibility(false);
}

void WebuiOmniboxHandler::OnResultChanged(AutocompleteController* controller,
                                          bool default_match_changed) {
  // TODO: (crbug.com/506266490) - Clean up these metrics since it is no longer
  // relevant to track after moving to factory pattern for searchbox::mojom.
  if (metrics_reporter_ && !metrics_reporter_->HasLocalMark("FirstAccess")) {
    metrics_reporter_->Mark("FirstAccess");
    base::UmaHistogramBoolean(
        "Omnibox.Popup.WebUI.PageRemoteIsBoundOnFirstCall", true);
  }

  if (metrics_reporter_ && !metrics_reporter_->HasLocalMark("ResultChanged")) {
    metrics_reporter_->Mark("ResultChanged");
  }

  SearchboxHandler::OnResultChanged(controller, default_match_changed);
}

void WebuiOmniboxHandler::SetPopupSelection(
    searchbox::mojom::OmniboxPopupSelectionPtr selection) {
  SearchboxHandler::SetPopupSelection(std::move(selection));
}

void WebuiOmniboxHandler::OnSelectionChanged(
    OmniboxPopupSelection old_selection,
    OmniboxPopupSelection selection) {
  page_->UpdateSelection(
      searchbox::mojom::OmniboxPopupSelection::New(
          old_selection.line, ConvertLineState(old_selection.state),
          old_selection.action_index),
      searchbox::mojom::OmniboxPopupSelection::New(
          selection.line, ConvertLineState(selection.state),
          selection.action_index));
}

void WebuiOmniboxHandler::OnCharTyped(base::TimeTicks timestamp) {
  if (metrics_reporter_ && !metrics_reporter_->HasLocalMark("CharTyped")) {
    metrics_reporter_->Mark("CharTyped", timestamp);
  }
}

void WebuiOmniboxHandler::OnActiveTabChanged(TabListInterface& tab_list,
                                             tabs::TabInterface* tab) {
}

void WebuiOmniboxHandler::OnTabWillDetach(
    tabs::TabInterface* tab,
    tabs::TabInterface::DetachReason reason) {
  edit_model_observation_.Reset();
  autocomplete_controller_observation_.Reset();
  controller_ = nullptr;
  UpdateTabListObservation(nullptr);
}

void WebuiOmniboxHandler::OnTabDidInsert(tabs::TabInterface* tab) {
  if (auto* browser_window_interface = tab->GetBrowserWindowInterface()) {
    // The window can be absent in unit tests that stub out the browser.
    BrowserWindow* const browser_window =
        BrowserWindow::FromBrowser(browser_window_interface);
    if (auto* location_bar =
            browser_window ? browser_window->GetLocationBar() : nullptr) {
      if (auto* omnibox_controller = location_bar->GetOmniboxController()) {
        edit_model_observation_.Reset();
        autocomplete_controller_observation_.Reset();
        controller_ = omnibox_controller;
        autocomplete_controller_observation_.Observe(autocomplete_controller());
        edit_model_observation_.Observe(omnibox_controller->edit_model());
        if (auto* helper = OmniboxPopupWebContentsHelper::FromWebContents(
                web_contents_.get())) {
          helper->set_omnibox_controller(omnibox_controller);
        }
        UpdateTabListObservation(
            TabListInterface::From(browser_window_interface));
      }
    }
  }
}

void WebuiOmniboxHandler::UpdateTabListObservation(TabListInterface* tab_list) {
  tab_list_observation_.Reset();
  if (tab_list) {
    tab_list_observation_.Observe(tab_list);
  }
}
