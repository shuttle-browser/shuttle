// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/language/core/language_model/fluent_language_model.h"

#include <string>

#include "base/strings/string_split.h"
#include "components/language/core/browser/language_prefs.h"
#include "components/prefs/pref_service.h"

namespace language {

FluentLanguageModel::FluentLanguageModel(PrefService* const pref_service)
    : pref_service_(pref_service) {
  DCHECK(pref_service);
}

FluentLanguageModel::~FluentLanguageModel() = default;

std::vector<LanguageModel::LanguageDetails>
FluentLanguageModel::GetLanguages() {
  // Upstream seeded this model from translate's "never translate" list, which
  // is gone with translate. Use the accept languages instead (never empty for a
  // regular profile; callers such as Read Anything rely on that).
  std::vector<std::string> accept_languages;
  LanguagePrefs(pref_service_).GetAcceptLanguagesList(&accept_languages);
  std::vector<LanguageDetails> languages;
  const float step = 1.0f / (accept_languages.size() + 1);
  float score = 1.0f;
  for (const std::string& lang : accept_languages) {
    languages.emplace_back(lang, score);
    score -= step;
  }
  return languages;
}

}  // namespace language
