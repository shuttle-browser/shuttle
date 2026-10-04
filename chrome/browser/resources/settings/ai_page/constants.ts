// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// LINT.IfChange(ModelExecutionEnterprisePolicyValue)
export enum ModelExecutionEnterprisePolicyValue {
  ALLOW = 0,
  ALLOW_WITHOUT_LOGGING = 1,
  DISABLE = 2,
}
// LINT.ThenChange(/components/optimization_guide/core/model_execution/model_execution_prefs.h:ModelExecutionEnterprisePolicyValue)

// Exporting enterprise pref names so that they can be referenced by tests.
export enum AiEnterpriseFeaturePrefName {
  AUTOFILL_AI =
      'optimization_guide.model_execution.autofill_prediction_improvements_enterprise_policy_allowed',
}
