// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/model_execution/optimization_guide_global_state.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/optimization_guide/prediction/chrome_profile_download_service_tracker.h"
#include "chrome/common/chrome_paths.h"
#include "components/component_updater/installer_policies/prediction_model_component_installer.h"
#include "components/optimization_guide/core/delivery/optimization_guide_model_provider.h"
#include "components/optimization_guide/core/delivery/prediction_manager.h"
#include "components/optimization_guide/core/delivery/prediction_model_store.h"
#include "components/services/unzip/content/unzip_service.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace optimization_guide {

namespace {

base::WeakPtr<OptimizationGuideGlobalState>& GetInstance() {
  static base::NoDestructor<base::WeakPtr<OptimizationGuideGlobalState>>
      instance;
  return *instance.get();
}

base::FilePath GetBaseStoreDir() {
  base::FilePath model_downloads_dir;
  base::PathService::Get(chrome::DIR_USER_DATA, &model_downloads_dir);
  model_downloads_dir = model_downloads_dir.Append(
      optimization_guide::kOptimizationGuideModelStoreDirPrefix);
  return model_downloads_dir;
}

}  // namespace

ChromePredictionManager::ChromePredictionManager()
    : prediction_model_store_(*g_browser_process->local_state()),
      prediction_manager_(&prediction_model_store_,
                          g_browser_process->shared_url_loader_factory(),
                          g_browser_process->local_state(),
                          g_browser_process->GetApplicationLocale(),
                          OptimizationGuideLogger::GetInstance(),
                          base::BindRepeating(&unzip::LaunchUnzipper)) {
  prediction_model_store_.Initialize(GetBaseStoreDir());
  prediction_manager_.MaybeInitializeModelDownloads(
      profile_download_service_tracker_, g_browser_process->local_state());
}
ChromePredictionManager::~ChromePredictionManager() = default;

OptimizationGuideGlobalState::OptimizationGuideGlobalState() = default;

void RegisterPredictionModelComponent(
    proto::OptimizationTarget target,
    base::WeakPtr<PredictionModelComponentUpdateListener> listener) {
  auto* cus = g_browser_process->component_updater();
  if (!cus) {
    return;
  }
  component_updater::RegisterPredictionModelComponent(cus, target, listener);
}

OptimizationGuideGlobalState::~OptimizationGuideGlobalState() = default;

scoped_refptr<OptimizationGuideGlobalState>
OptimizationGuideGlobalState::CreateOrGet() {
  base::WeakPtr<OptimizationGuideGlobalState>& instance = GetInstance();
  if (!instance) {
    auto new_instance =
        base::WrapRefCounted(new OptimizationGuideGlobalState());
    instance = new_instance->weak_ptr_factory_.GetWeakPtr();
    return new_instance;
  }
  return scoped_refptr<OptimizationGuideGlobalState>(instance.get());
}

OptimizationGuideGlobalFeature::OptimizationGuideGlobalFeature() = default;

OptimizationGuideGlobalFeature::~OptimizationGuideGlobalFeature() = default;

OptimizationGuideGlobalState& OptimizationGuideGlobalFeature::Get() {
  if (!global_state_) {
    global_state_ = OptimizationGuideGlobalState::CreateOrGet();
  }
  return *global_state_;
}

OptimizationGuideModelProvider&
OptimizationGuideGlobalFeature::GetModelProvider() {
  return Get().model_provider();
}

}  // namespace optimization_guide
