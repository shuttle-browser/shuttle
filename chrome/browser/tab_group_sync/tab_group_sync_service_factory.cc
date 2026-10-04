// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_group_sync/tab_group_sync_service_factory.h"

#include <memory>

#include "base/functional/callback.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/data_type_store_service_factory.h"
#include "chrome/browser/sync/device_info_sync_service_factory.h"
#include "chrome/browser/tab_group_sync/feature_utils.h"
#include "chrome/browser/tab_group_sync/tab_group_trial.h"
#include "chrome/common/channel_info.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/saved_tab_groups/delegate/empty_tab_group_sync_delegate.h"
#include "components/saved_tab_groups/delegate/tab_group_sync_delegate.h"
#include "components/saved_tab_groups/public/collaboration_finder.h"
#include "components/saved_tab_groups/public/features.h"
#include "components/saved_tab_groups/public/synthetic_field_trial_helper.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/saved_tab_groups/public/tab_group_sync_service_factory_helper.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/report_unrecoverable_error.h"
#include "components/sync/model/client_tag_based_data_type_processor.h"
#include "components/sync/model/data_type_store_service.h"
#include "components/sync_device_info/device_info_sync_service.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "chrome/browser/tab_group_sync/android/tab_group_sync_delegate_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/TabGroupSyncDepsProvider_jni.h"
#else
#include "chrome/browser/ui/tabs/saved_tab_groups/tab_group_sync_delegate_desktop.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace tab_groups {

namespace {

// Shared tab groups (collaborations) are not supported, so no collaboration
// ever becomes available.
class NoopCollaborationFinder : public CollaborationFinder {
 public:
  NoopCollaborationFinder() = default;
  ~NoopCollaborationFinder() override = default;

  // CollaborationFinder:
  void SetClient(Client* client) override {}
  bool IsCollaborationAvailable(
      const syncer::CollaborationId& collaboration_id) override {
    return false;
  }
};

}  // namespace

// static
TabGroupSyncServiceFactory* TabGroupSyncServiceFactory::GetInstance() {
  static base::NoDestructor<TabGroupSyncServiceFactory> instance;
  return instance.get();
}

// static
TabGroupSyncService* TabGroupSyncServiceFactory::GetForProfile(
    Profile* profile) {
  CHECK(profile);
  return static_cast<TabGroupSyncService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

TabGroupSyncServiceFactory::TabGroupSyncServiceFactory()
    : ProfileKeyedServiceFactory(
          "TabGroupSyncService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .Build()),
      synthetic_field_trial_helper_(std::make_unique<SyntheticFieldTrialHelper>(
          base::BindRepeating(&TabGroupTrial::OnHadSyncedTabGroup),
          base::BindRepeating(&TabGroupTrial::OnHadSharedTabGroup))) {
  DependsOn(DataTypeStoreServiceFactory::GetInstance());
  DependsOn(DeviceInfoSyncServiceFactory::GetInstance());
  DependsOn(OptimizationGuideKeyedServiceFactory::GetInstance());
  // The dependency on IdentityManager is only for the purpose of recording "on
  // signin" metrics.
  DependsOn(IdentityManagerFactory::GetInstance());
}

TabGroupSyncServiceFactory::~TabGroupSyncServiceFactory() = default;

std::unique_ptr<KeyedService>
TabGroupSyncServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  DCHECK(context);
  Profile* profile = static_cast<Profile*>(context);
  auto* pref_service = profile->GetPrefs();

  if (!IsTabGroupSyncEnabled(pref_service)) {
    return nullptr;
  }

  auto service = CreateTabGroupSyncService(
      chrome::GetChannel(), DataTypeStoreServiceFactory::GetForProfile(profile),
      pref_service,
      DeviceInfoSyncServiceFactory::GetForProfile(profile)
          ->GetDeviceInfoTracker(),
      OptimizationGuideKeyedServiceFactory::GetForProfile(profile),
      IdentityManagerFactory::GetForProfile(profile),
      std::make_unique<NoopCollaborationFinder>(),
      synthetic_field_trial_helper_.get());

  std::unique_ptr<TabGroupSyncDelegate> delegate;
#if BUILDFLAG(IS_ANDROID)
  if (IsTabGroupSyncDelegateAndroidEnabled()) {
    auto j_delegate_deps = Java_TabGroupSyncDepsProvider_createDeps(
        base::android::AttachCurrentThread());
    delegate = std::make_unique<TabGroupSyncDelegateAndroid>(service.get(),
                                                             j_delegate_deps);
  } else {
    delegate = std::make_unique<EmptyTabGroupSyncDelegate>();
  }
#else
  delegate =
      std::make_unique<TabGroupSyncDelegateDesktop>(service.get(), profile);
#endif  // BUILDFLAG(IS_ANDROID)

  service->SetTabGroupSyncDelegate(std::move(delegate));
  return std::move(service);
}

}  // namespace tab_groups

#if BUILDFLAG(IS_ANDROID)
DEFINE_JNI(TabGroupSyncDepsProvider)
#endif
