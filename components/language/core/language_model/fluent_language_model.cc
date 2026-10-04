// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/language/core/language_model/fluent_language_model.h"

#include "base/strings/string_split.h"
#include "components/prefs/pref_service.h"

namespace language {

FluentLanguageModel::FluentLanguageModel(PrefService* const pref_service) {
  DCHECK(pref_service);
}

FluentLanguageModel::~FluentLanguageModel() = default;

std::vector<LanguageModel::LanguageDetails>
FluentLanguageModel::GetLanguages() {
  // The "never translate" language list used to seed this model belonged to the
  // translate feature, which has been removed.
  return {};
}

}  // namespace language
