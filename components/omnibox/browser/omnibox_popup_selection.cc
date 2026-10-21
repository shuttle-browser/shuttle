// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/omnibox_popup_selection.h"

#include <algorithm>

#include "build/build_config.h"
#include "components/omnibox/browser/actions/omnibox_action.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/autocomplete_result.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/search_engines/template_url_service.h"

constexpr bool kIsDesktop = !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS);

constexpr size_t OmniboxPopupSelection::kNoMatch = static_cast<size_t>(-1);

#if DCHECK_ALWAYS_ON
std::ostream& operator<<(std::ostream& os, const OmniboxPopupSelection& s) {
  os << '{' << s.line << ',' << static_cast<int>(s.state) << ','
     << s.action_index << '}';
  return os;
}

std::ostream& operator<<(std::ostream& os,
                         const std::vector<OmniboxPopupSelection>& ss) {
  os << '[';
  for (const OmniboxPopupSelection& s : ss) {
    os << s;
  }
  os << ']';
  return os;
}
#endif

bool OmniboxPopupSelection::IsChangeToKeyword(
    OmniboxPopupSelection from) const {
  return state == LineState::kKeywordMode &&
         from.state != LineState::kKeywordMode;
}

bool OmniboxPopupSelection::IsButtonFocused() const {
  return state != LineState::kNormal && state != LineState::kKeywordMode;
}

bool OmniboxPopupSelection::IsAction() const {
  return state == LineState::kFocusedButtonAction;
}

bool OmniboxPopupSelection::IsNonMatchSelection() const {
  return line == kNoMatch && state == LineState::kCtrlEnter;
}

bool OmniboxPopupSelection::IsControlPresentOnMatch(
    const AutocompleteResult& result) const {
  if (line >= result.size() || IsNonMatchSelection()) {
    return false;
  }

  const auto& match = result.match_at(line);

  switch (state) {
    case LineState::kNormal: {
      // `NULL_RESULT_MESSAGE` cannot be focused, except for IPH suggestions
      // that contain links (such as the disclaimer or setting promo) which
      // need to be navigable by screen readers.
      return match.type != omnibox::AutocompleteMatchType::kNullResultMessage ||
             (match.IsIphSuggestion() && !match.iph_link_url.is_empty());
    }
    case LineState::kKeywordMode:
      return !match.associated_keyword.empty();
    case LineState::kFocusedButtonAction: {
      // Actions buttons should not be shown in keyword mode.
      return !match.from_keyword && action_index < match.actions.size();
    }
    case LineState::kFocusedButtonThumbsUp:
    case LineState::kFocusedButtonThumbsDown:
      return match.type == omnibox::AutocompleteMatchType::kHistoryEmbeddings;
    case LineState::kFocusedButtonRemoveSuggestion:
      return match.SupportsDeletion();
    case LineState::kFocusedIphLink:
      return match.IsIphSuggestion() && !match.iph_link_url.is_empty();
    case LineState::kCtrlEnter:
      return false;
    default:
      break;
  }
  NOTREACHED();
}

OmniboxPopupSelection OmniboxPopupSelection::GetNextSelection(
    const AutocompleteInput& input,
    const AutocompleteResult& result,
    TemplateURLService* template_url_service,
    Direction direction,
    Step step) const {
  if (result.empty()) {
    return *this;
  }

  // Implementing this was like a Google Interview Problem. It was always a
  // tough problem to handle all the cases, but has gotten much harder since
  // we can now hide whole rows from view by collapsing sections.
  //
  // The only sane thing to do is to first enumerate all available selections.
  // Other approaches I've tried all end up being a jungle of branching code.
  // It's not necessarily optimal to generate this list for each keypress, but
  // in practice it's only something like ~10 elements long, and makes the code
  // easy to reason about.
  std::vector<OmniboxPopupSelection> all_available_selections =
      GetAllAvailableSelectionsSorted(input, result, template_url_service,
                                      step);
  if (all_available_selections.empty()) {
    return *this;
  }

  // Handle the simple case of just getting the first or last element.
  if (step == Step::kAllLines) {
    return direction == Direction::kForward ? all_available_selections.back()
                                            : all_available_selections.front();
  }

  if (direction == Direction::kForward) {
    // To go forward, we want to change to the first selection that's larger
    // than the current selection, and std::upper_bound() does just
    // that.
    const auto next = std::upper_bound(all_available_selections.begin(),
                                       all_available_selections.end(), *this);

    // If we can't find any selections larger than the current
    // selection, wrap.
    if (next == all_available_selections.end())
      return all_available_selections.front();

    // Normal case where we found the next selection.
    return *next;
  } else if (direction == Direction::kBackward) {
    // To go backwards, decrement one from std::lower_bound(), which finds the
    // current selection. I didn't use std::find() here, because
    // std::lower_bound() can gracefully handle the case where
    // selection is no longer within the list of available selections.
    const auto current =
        std::lower_bound(all_available_selections.begin(),
                         all_available_selections.end(), *this);

    // If the current selection is the first one, wrap.
    if (current == all_available_selections.begin()) {
      return all_available_selections.back();
    }

    // Decrement one from the current selection.
    return *(current - 1);
  }

  NOTREACHED();
}

// static
std::vector<OmniboxPopupSelection>
OmniboxPopupSelection::GetAllAvailableSelectionsSorted(
    const AutocompleteInput& input,
    const AutocompleteResult& result,
    TemplateURLService* template_url_service,
    Step step) {
  // First enumerate all the accessible states based on `direction` and `step`,
  // as well as enabled feature flags. This doesn't mean each match will have
  // all of these states - just that it's possible to get there, if available.
  std::vector<LineState> all_states;
  switch (step) {
    case Step::kWholeLine:
    case Step::kAllLines:
      all_states.push_back(LineState::kNormal);
      // Whole line stepping can go straight into keyword mode.
      all_states.push_back(LineState::kKeywordMode);
      break;
    case Step::kStateOrLine:
      all_states.push_back(LineState::kNormal);
      all_states.push_back(LineState::kKeywordMode);
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
      all_states.push_back(LineState::kFocusedButtonAction);
#endif
      all_states.push_back(LineState::kFocusedButtonThumbsUp);
      all_states.push_back(LineState::kFocusedButtonThumbsDown);
      all_states.push_back(LineState::kFocusedButtonRemoveSuggestion);
      all_states.push_back(LineState::kFocusedIphLink);
      break;
  }
  DCHECK(std::is_sorted(all_states.begin(), all_states.end()))
      << "Line states must be added in sorted order for the algorithm to work.";

  std::vector<OmniboxPopupSelection> available_selections;
  // Now, for each accessible line, add all the available line states to a list.
  for (size_t line_number = 0; line_number < result.size(); ++line_number) {
    for (LineState line_state : all_states) {
      if (line_state == LineState::kFocusedButtonAction) {
        constexpr size_t kMaxActionCount = 8;
        for (size_t i = 0; i < kMaxActionCount; i++) {
          OmniboxPopupSelection selection(line_number, line_state, i);
          if (selection.IsControlPresentOnMatch(result)) {
            available_selections.push_back(selection);
          } else {
            // Break early when there are no more actions. Note, this
            // implies that a match takeover action should be last
            // to allow other actions on the match to be included.
            break;
          }
        }
      } else if (line_state == LineState::kKeywordMode && kIsDesktop) {
        OmniboxPopupSelection selection(line_number, line_state);
        if (selection.IsControlPresentOnMatch(result)) {
          if (result.match_at(line_number)
                  .HasInstantKeyword(template_url_service)) {
            if (available_selections.size() > 0 &&
                available_selections.back().line == line_number &&
                available_selections.back().state == LineState::kNormal) {
              // Remove the preceding normal state selection so that keyword
              // mode will be entered immediately when the user arrows down
              // to this keyword line.
              available_selections.pop_back();
            }
            available_selections.push_back(selection);
          } else if (step == Step::kStateOrLine) {
            available_selections.push_back(selection);
          }
        }
      } else {
        OmniboxPopupSelection selection(line_number, line_state);
        if (selection.IsControlPresentOnMatch(result)) {
          available_selections.push_back(selection);
        }
      }
    }
  }
  DCHECK(
      std::is_sorted(available_selections.begin(), available_selections.end()))
      << "This algorithm depends on a sorted list of available selections.";
  return available_selections;
}
