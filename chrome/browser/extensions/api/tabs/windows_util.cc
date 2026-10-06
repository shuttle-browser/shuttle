// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/tabs/windows_util.h"

#include <string>
#include <vector>

#include "base/strings/string_number_conversions.h"
#include "chrome/browser/extensions/api/tabs/tabs_constants.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/window_controller.h"
#include "chrome/browser/extensions/window_controller_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/navigator/browser_navigator.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_function_dispatcher.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/constants.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension.h"
#include "url/gurl.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace windows_util {

bool GetControllerFromWindowID(ExtensionFunction* function,
                               int window_id,
                               extensions::WindowController::TypeFilter filter,
                               extensions::WindowController** out_controller,
                               std::string* error) {
  DCHECK(out_controller);
  DCHECK(error);

  *out_controller = nullptr;
  if (window_id == extension_misc::kCurrentWindowId) {
    // If there is a window controller associated with this extension, use that.
    if (extensions::WindowController* window_controller =
            function->dispatcher()->GetExtensionWindowController()) {
      *out_controller = window_controller;
      return true;
    }

    // Otherwise get the focused or most recently added window.
    if (extensions::WindowController* window_controller =
            extensions::WindowControllerList::GetInstance()
                ->CurrentWindowForFunctionWithFilter(function, filter)) {
      *out_controller = window_controller;
      return true;
    }

    *error = extensions::ExtensionTabUtil::kNoCurrentWindowError;
    return false;
  } else {
    if (extensions::WindowController* window_controller =
            extensions::WindowControllerList::GetInstance()
                ->FindWindowForFunctionByIdWithFilter(function, window_id,
                                                      filter)) {
      *out_controller = window_controller;
      return true;
    }

    *error = extensions::ErrorUtils::FormatErrorMessage(
        extensions::ExtensionTabUtil::kWindowNotFoundError,
        base::NumberToString(window_id));
    return false;
  }
}

bool CanOperateOnWindow(const ExtensionFunction* function,
                        const extensions::WindowController* controller,
                        extensions::WindowController::TypeFilter filter) {
  if (filter && !controller->MatchesFilter(filter)) {
    return false;
  }

  // TODO(crbug.com/41367902): Remove this.
  bool allow_dev_tools_windows = !!filter;
  if (function->extension() &&
      !controller->IsVisibleToTabsAPIForExtension(function->extension(),
                                                  allow_dev_tools_windows)) {
    return false;
  }

  if (function->browser_context() == controller->profile()) {
    return true;
  }

  if (!function->include_incognito_information()) {
    return false;
  }

  // There is no off the record profile.
  return false;
}

IncognitoResult ShouldOpenIncognitoWindow(Profile* profile,
                                          std::optional<bool> incognito,
                                          std::vector<GURL>* urls,
                                          std::string* error) {
  // Incognito is permanently unavailable.
  if (incognito.value_or(false)) {
    *error = extensions::tabs_constants::kIncognitoModeIsDisabled;
    return IncognitoResult::kError;
  }
  return IncognitoResult::kRegular;
}

}  // namespace windows_util
