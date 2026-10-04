// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/actions/omnibox_action_client_delegator.h"

OmniboxActionClientDelegator::OmniboxActionClientDelegator(
    OmniboxAction::Client& delegate)
    : delegate_(&delegate) {}

OmniboxActionClientDelegator::~OmniboxActionClientDelegator() = default;

void OmniboxActionClientDelegator::OpenSharingHub() {
  delegate_->OpenSharingHub();
}

void OmniboxActionClientDelegator::NewIncognitoWindow() {
  delegate_->NewIncognitoWindow();
}

void OmniboxActionClientDelegator::OpenIncognitoClearBrowsingDataDialog() {
  delegate_->OpenIncognitoClearBrowsingDataDialog();
}

void OmniboxActionClientDelegator::CloseIncognitoWindows() {
  delegate_->CloseIncognitoWindows();
}

void OmniboxActionClientDelegator::PromptPageTranslation() {
  delegate_->PromptPageTranslation();
}

bool OmniboxActionClientDelegator::OpenJourneys(const std::string& query) {
  return delegate_->OpenJourneys(query);
}

bool OmniboxActionClientDelegator::ShouldOpenCoBrowsePanel() const {
  return delegate_->ShouldOpenCoBrowsePanel();
}

void OmniboxActionClientDelegator::OpenCoBrowsePanel() {
  delegate_->OpenCoBrowsePanel();
}
