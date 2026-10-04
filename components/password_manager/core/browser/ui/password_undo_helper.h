// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_PASSWORD_UNDO_HELPER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_PASSWORD_UNDO_HELPER_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "components/password_manager/core/browser/password_store/stored_credential.h"

namespace password_manager {

class PasswordStoreInterface;

// Helper class to revert deletion of a saved passwords or password exception
// entries.
class PasswordUndoHelper {
 public:
  explicit PasswordUndoHelper(PasswordStoreInterface* profile_store,
                              PasswordStoreInterface* account_store);
  PasswordUndoHelper(const PasswordUndoHelper&) = delete;
  PasswordUndoHelper& operator=(const PasswordUndoHelper&) = delete;
  ~PasswordUndoHelper();

  // Adds password to the undo action.
  void PasswordRemoved(StoredCredential credential);
  // Adds backup password to the undo action.
  void BackupPasswordRemoved(StoredCredential credential);

  // Reverts last grouped deletion.
  void Undo();

  // Starts grouping multiple password deletions under single undo action.
  // Should be called before first call to 'PasswordRemoved()'.
  void StartGroupingActions();

  // Ends grouping multiple password deletions under single undo action.
  // Should be called after last call to 'PasswordRemoved()'.
  void EndGroupingActions();

 private:
  // Which store operation reverts a recorded removal.
  enum class OperationType {
    // The credential was removed: add it back.
    kAdd,
    // The credential had a backup password removed: restore the credential.
    kUpdate,
  };

  struct Operation {
    OperationType type;
    StoredCredential credential;
  };

  // Records `operation` in the group being built or, outside of grouping, as a
  // group of its own.
  void AddOperation(Operation operation);

  // Reverted operations are popped from the back, one group per `Undo()`.
  std::vector<std::vector<Operation>> undo_groups_;
  std::vector<Operation> pending_group_;
  int group_actions_count_ = 0;

  raw_ptr<PasswordStoreInterface> profile_store_;
  raw_ptr<PasswordStoreInterface> account_store_;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_PASSWORD_UNDO_HELPER_H_
