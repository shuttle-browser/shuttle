// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/input_delegate/tab_session_source.h"
#include <math.h>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "components/segmentation_platform/embedder/tab_fetcher.h"
#include "components/segmentation_platform/internal/execution/processing/feature_processor_state.h"
#include "components/segmentation_platform/public/types/processed_value.h"
#include "components/sessions/core/session_id.h"
#include "components/sessions/core/session_types.h"
#include "ui/base/page_transition_types.h"

namespace segmentation_platform::processing {

float TabSessionSource::BucketizeExp(int64_t value, int max_buckets) {
  if (value <= 0) {
    return 0;
  }
  int log_val = floor(log2(value));
  if (log_val >= max_buckets) {
    log_val = max_buckets;
  }
  return pow(2, log_val);
}

float TabSessionSource::BucketizeLinear(int64_t value, int max_buckets) {
  if (value <= 0) {
    return 0;
  }
  if (value >= max_buckets) {
    return max_buckets;
  }
  return value;
}

TabSessionSource::TabSessionSource(TabFetcher* tab_fetcher)
    : tab_fetcher_(tab_fetcher) {}

TabSessionSource::~TabSessionSource() = default;

void TabSessionSource::Process(const proto::CustomInput& input,
                               FeatureProcessorState& feature_processor_state,
                               ProcessedCallback callback) {
  auto tab_id_val =
      feature_processor_state.input_context()->GetMetadataArgument("tab_id");
  auto session_tag_val =
      feature_processor_state.input_context()->GetMetadataArgument(
          "session_tag");
  if (!session_tag_val || !tab_id_val) {
    std::move(callback).Run(/*error=*/true, {});
    return;
  }

  CHECK_EQ(tab_id_val->type, ProcessedValue::Type::INT);
  CHECK_EQ(session_tag_val->type, ProcessedValue::STRING);
  SessionID::id_type tab_id = tab_id_val->int_val;
  std::string session_tag = session_tag_val->str_val;

  Tensor inputs(kNumInputs, ProcessedValue(0.0f));

  TabFetcher::Tab tab = tab_fetcher_->FindTab(TabFetcher::TabEntry(
      SessionID::FromSerializedValue(tab_id), session_tag));
  if (tab.webcontents || tab.tab_android) {
    AddLocalTabInfo(tab, feature_processor_state, inputs);
  }

  std::move(callback).Run(false, std::move(inputs));
}

void TabSessionSource::AddLocalTabInfo(
    const TabFetcher::Tab& tab,
    FeatureProcessorState& feature_processor_state,
    Tensor& inputs) {}

}  // namespace segmentation_platform::processing
