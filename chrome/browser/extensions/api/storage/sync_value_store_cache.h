// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_STORAGE_SYNC_VALUE_STORE_CACHE_H_
#define CHROME_BROWSER_EXTENSIONS_API_STORAGE_SYNC_VALUE_STORE_CACHE_H_

#include <map>
#include <memory>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "extensions/browser/api/storage/settings_storage_quota_enforcer.h"
#include "extensions/browser/api/storage/value_store_cache.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension_id.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace value_store {
class ValueStore;
class ValueStoreFactory;
}  // namespace value_store

namespace extensions {

// ValueStoreCache for the SYNC namespace. Agent Chrome has no sync service, so
// chrome.storage.sync is served from a plain on-disk store (like the LOCAL
// namespace) in its own directory, with the sync quotas enforced.
class SyncValueStoreCache : public ValueStoreCache {
 public:
  explicit SyncValueStoreCache(
      scoped_refptr<value_store::ValueStoreFactory> factory);

  SyncValueStoreCache(const SyncValueStoreCache&) = delete;
  SyncValueStoreCache& operator=(const SyncValueStoreCache&) = delete;

  ~SyncValueStoreCache() override;

  // ValueStoreCache implementation:
  void RunWithValueStoreForExtension(
      StorageCallback callback,
      scoped_refptr<const Extension> extension) override;
  void DeleteStorageSoon(const ExtensionId& extension_id) override;

 private:
  using StorageMap =
      std::map<ExtensionId, std::unique_ptr<value_store::ValueStore>>;

  value_store::ValueStore* GetStorage(const Extension* extension);

  // The Factory to use for creating new ValueStores.
  const scoped_refptr<value_store::ValueStoreFactory> storage_factory_;

  // Quota limits (see SettingsStorageQuotaEnforcer).
  const SettingsStorageQuotaEnforcer::Limits quota_;

  // The collection of ValueStores for sync storage.
  StorageMap storage_map_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_STORAGE_SYNC_VALUE_STORE_CACHE_H_
