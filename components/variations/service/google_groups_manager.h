// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_SERVICE_GOOGLE_GROUPS_MANAGER_H_
#define COMPONENTS_VARIATIONS_SERVICE_GOOGLE_GROUPS_MANAGER_H_

#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/scoped_observation.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/variations/service/google_groups_manager_prefs.h"

namespace user_prefs {
class PrefRegistrySyncable;
}

// Service responsible for one-way synchronization of Google group information
// from per-profile sync data to local-state.
class GoogleGroupsManager : public KeyedService {
 public:
  explicit GoogleGroupsManager(PrefService& target_prefs,
                                      const std::string& key,
                                      PrefService& source_prefs);

  GoogleGroupsManager(const GoogleGroupsManager&) = delete;
  GoogleGroupsManager& operator=(const GoogleGroupsManager&) =
      delete;

  ~GoogleGroupsManager() override;

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Returns whether `feature` is group controlled, i.e. whether the study that
  // is connected to this feature has a non-empty `google_groups` filter. This
  // will always return `false` if called before experiment parameters are
  // registered.
  static bool IsFeatureGroupControlled(const base::Feature& feature);

  // Returns whether `feature` is enabled and, if the study that is connected to
  // this feature has a non-empty `google_groups` filter, whether the
  // BrowserContext associated with `this` KeyedService is in at least one of
  // those Google Groups. Note that the Google Groups for the BrowserContext are
  // only populated if the user is syncing their preferences. If the user is not
  // syncing their preferences, this function therefore falls back to
  // `base::FeatureList::IsEnabled`.
  bool IsFeatureEnabledForProfile(const base::Feature& feature) const;

  // KeyedService overrides.
  void Shutdown() override;

 private:
  // Update the group memberships in `target_prefs_`.
  // Called when `source_prefs_` have been initialized or modified.
  void UpdateGoogleGroups();

  // The preferences to write to. These are the local-state prefs.
  // Preferences are guaranteed to outlive keyed services, so this reference
  // will stay valid for the lifetime of this service.
  const raw_ref<PrefService> target_prefs_;

  // The key to use in the `target_prefs_` dictionary.
  // This key is immutable (it will not change for eg. a given profile, even
  // across Chrome restarts).
  const std::string key_;

  // The preferences to read from. These are the profile prefs.
  // Preferences are guaranteed to outlive keyed services, so this reference
  // will stay valid for the lifetime of this service.
  const raw_ref<PrefService> source_prefs_;

  PrefChangeRegistrar pref_change_registrar_;

  // A cached version of the (variations-related) Google Groups that the profile
  // is a part of.
  base::flat_set<std::string> google_group_ids_;

};

#endif  // COMPONENTS_VARIATIONS_SERVICE_GOOGLE_GROUPS_MANAGER_H_
