// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/ui/password_undo_helper.h"

#include <utility>

#include "base/check.h"
#include "base/location.h"
#include "components/password_manager/core/browser/password_store/password_form_converters.h"
#include "components/password_manager/core/browser/password_store/password_store_interface.h"
#include "components/password_manager/core/browser/password_store/stored_credential.h"

namespace password_manager {

PasswordUndoHelper::PasswordUndoHelper(PasswordStoreInterface* profile_store,
                                       PasswordStoreInterface* account_store)
    : profile_store_(profile_store), account_store_(account_store) {}

PasswordUndoHelper::~PasswordUndoHelper() = default;

void PasswordUndoHelper::PasswordRemoved(StoredCredential credential) {
  AddOperation({OperationType::kAdd, std::move(credential)});
}

void PasswordUndoHelper::BackupPasswordRemoved(
    StoredCredential credential_with_backup) {
  AddOperation({OperationType::kUpdate, std::move(credential_with_backup)});
}

void PasswordUndoHelper::Undo() {
  if (undo_groups_.empty()) {
    return;
  }
  std::vector<Operation> group = std::move(undo_groups_.back());
  undo_groups_.pop_back();
  // Revert in the reverse order of recording.
  for (auto it = group.rbegin(); it != group.rend(); ++it) {
    const StoredCredential& credential = it->credential;
    switch (it->type) {
      case OperationType::kAdd:
        if (credential.IsUsingAccountStore()) {
          account_store_->AddLogin(CloneStoredCredential(credential));
        }
        if (credential.IsUsingProfileStore()) {
          profile_store_->AddLogin(CloneStoredCredential(credential));
        }
        break;
      case OperationType::kUpdate:
        if (credential.IsUsingAccountStore()) {
          account_store_->UpdateLogin(CloneStoredCredential(credential));
        }
        if (credential.IsUsingProfileStore()) {
          profile_store_->UpdateLogin(CloneStoredCredential(credential));
        }
        break;
    }
  }
}

void PasswordUndoHelper::StartGroupingActions() {
  ++group_actions_count_;
}

void PasswordUndoHelper::EndGroupingActions() {
  CHECK_GT(group_actions_count_, 0);
  if (--group_actions_count_ == 0 && !pending_group_.empty()) {
    undo_groups_.push_back(std::move(pending_group_));
    pending_group_.clear();
  }
}

void PasswordUndoHelper::AddOperation(Operation operation) {
  if (group_actions_count_ > 0) {
    pending_group_.push_back(std::move(operation));
    return;
  }
  std::vector<Operation> group;
  group.push_back(std::move(operation));
  undo_groups_.push_back(std::move(group));
}

}  // namespace password_manager
