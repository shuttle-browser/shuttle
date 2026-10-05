// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_PROCESS_STATE_H_
#define CHROME_RENDERER_PROCESS_STATE_H_

#include "build/build_config.h"

namespace process_state {

// Returns true if this renderer process is incognito.
bool IsIncognitoProcess();

// Sets whether this renderer process is an incognito process.
void SetIsIncognitoProcess(bool is_incognito_process);

}  // namespace process_state

#endif  // CHROME_RENDERER_PROCESS_STATE_H_
