// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/storage/sync_value_store_cache.h"

#include <stddef.h>

#include <utility>

#include "components/value_store/value_store_factory.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/api/storage/backend_task_runner.h"
#include "extensions/browser/api/storage/value_store_util.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/api/storage.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

using content::BrowserThread;

namespace extensions {

namespace {

// Returns the quota limit for sync storage, taken from the schema in
// extensions/common/api/storage.json.
SettingsStorageQuotaEnforcer::Limits GetSyncQuotaLimits() {
  SettingsStorageQuotaEnforcer::Limits limits = {
      static_cast<size_t>(api::storage::sync::QUOTA_BYTES),
      static_cast<size_t>(api::storage::sync::QUOTA_BYTES_PER_ITEM),
      static_cast<size_t>(api::storage::sync::MAX_ITEMS)};
  return limits;
}

}  // namespace

SyncValueStoreCache::SyncValueStoreCache(
    scoped_refptr<value_store::ValueStoreFactory> factory)
    : storage_factory_(std::move(factory)), quota_(GetSyncQuotaLimits()) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

SyncValueStoreCache::~SyncValueStoreCache() {
  DCHECK(IsOnBackendSequence());
}

void SyncValueStoreCache::RunWithValueStoreForExtension(
    StorageCallback callback,
    scoped_refptr<const Extension> extension) {
  DCHECK(IsOnBackendSequence());
  std::move(callback).Run(GetStorage(extension.get()));
}

void SyncValueStoreCache::DeleteStorageSoon(const ExtensionId& extension_id) {
  DCHECK(IsOnBackendSequence());
  storage_map_.erase(extension_id);

  value_store_util::DeleteValueStore(settings_namespace::SYNC,
                                     value_store_util::ModelType::APP,
                                     extension_id, storage_factory_);
  value_store_util::DeleteValueStore(settings_namespace::SYNC,
                                     value_store_util::ModelType::EXTENSION,
                                     extension_id, storage_factory_);
}

value_store::ValueStore* SyncValueStoreCache::GetStorage(
    const Extension* extension) {
  auto iter = storage_map_.find(extension->id());
  if (iter != storage_map_.end()) {
    return iter->second.get();
  }

  value_store_util::ModelType model_type =
      extension->is_app() ? value_store_util::ModelType::APP
                          : value_store_util::ModelType::EXTENSION;
  std::unique_ptr<value_store::ValueStore> store =
      value_store_util::CreateSettingsStore(settings_namespace::SYNC,
                                            model_type, extension->id(),
                                            storage_factory_);
  auto storage =
      std::make_unique<SettingsStorageQuotaEnforcer>(quota_, std::move(store));
  value_store::ValueStore* storage_ptr = storage.get();
  storage_map_[extension->id()] = std::move(storage);
  return storage_ptr;
}

}  // namespace extensions
