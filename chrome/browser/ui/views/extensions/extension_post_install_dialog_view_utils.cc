// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extension_post_install_dialog_view_utils.h"

namespace extensions {

// Agent Chrome: sign-in promos were removed, so there is never a footnote.
// TODO(agent-chrome): drop the caller in extension_post_install_dialog.cc and
// delete this file.
void MaybeAddSigninPromoFootnoteView(
    Profile* profile,
    content::WebContents* web_contents,
    const extensions::Extension& extension,
    ui::DialogModel::Builder& dialog_model_builder) {}

}  // namespace extensions
