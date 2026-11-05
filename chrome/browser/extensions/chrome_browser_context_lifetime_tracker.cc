// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/chrome_browser_context_lifetime_tracker.h"

#include <utility>

#include "base/memory/raw_ref.h"
#include "base/scoped_observation.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_observer.h"

namespace extensions {

// Observes a Profile, forwarding its destruction to a
// BrowserContextLifetimeObserver in terms of content::BrowserContext.
// Shuttle has no incognito profiles, so no off-the-record context is ever
// reported.
class ChromeBrowserContextLifetimeTracker::ProfileWatcher
    : public ProfileObserver {
 public:
  ProfileWatcher(Profile& profile, BrowserContextLifetimeObserver& observer)
      : observer_(observer) {
    profile_observation_.Observe(&profile);
  }

  ProfileWatcher(const ProfileWatcher&) = delete;
  ProfileWatcher& operator=(const ProfileWatcher&) = delete;

  ~ProfileWatcher() override = default;

 private:
  // ProfileObserver:
  void OnProfileWillBeDestroyed(Profile* profile) override {
    if (profile_observation_.IsObservingSource(profile)) {
      profile_observation_.Reset();
      observer_->OnPrimaryBrowserContextDestroyed(*profile);
    }
  }

  const raw_ref<BrowserContextLifetimeObserver> observer_;
  base::ScopedObservation<Profile, ProfileObserver> profile_observation_{this};
};

// static
ChromeBrowserContextLifetimeTracker*
ChromeBrowserContextLifetimeTracker::GetInstance() {
  static base::NoDestructor<ChromeBrowserContextLifetimeTracker> instance;
  return instance.get();
}

ChromeBrowserContextLifetimeTracker::ChromeBrowserContextLifetimeTracker() =
    default;

ChromeBrowserContextLifetimeTracker::~ChromeBrowserContextLifetimeTracker() =
    default;

void ChromeBrowserContextLifetimeTracker::StartObserving(
    content::BrowserContext& context,
    BrowserContextLifetimeObserver& observer) {
  DCHECK(watchers_.find(&observer) == watchers_.end());
  Profile* profile = Profile::FromBrowserContext(&context);
  CHECK(!profile->IsOffTheRecord());
  watchers_[&observer] = std::make_unique<ProfileWatcher>(*profile, observer);
}

void ChromeBrowserContextLifetimeTracker::StopObserving(
    BrowserContextLifetimeObserver& observer) {
  watchers_.erase(&observer);
}

}  // namespace extensions
