// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webdata_services/web_data_service_wrapper.h"

#include <memory>

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "components/search_engines/keyword_table.h"
#include "components/search_engines/keyword_web_data_service.h"
#include "components/signin/public/webdata/token_service_table.h"
#include "components/signin/public/webdata/token_web_data.h"
#include "components/webdata/common/web_database_service.h"
#include "components/webdata/common/webdata_constants.h"

WebDataServiceWrapper::WebDataServiceWrapper() = default;

WebDataServiceWrapper::WebDataServiceWrapper(
    const base::FilePath& context_path,
    const std::string& application_locale,
    const scoped_refptr<base::SequencedTaskRunner>& ui_task_runner,
    const ShowErrorCallback& show_error_callback,
    os_crypt_async::OSCryptAsync* os_crypt) {
  base::FilePath path = context_path.Append(kWebDataFilename);
  auto db_task_runner = base::ThreadPool::CreateSequencedTaskRunnerForResource(
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
      path);
  profile_database_ = base::MakeRefCounted<WebDatabaseService>(
      path, ui_task_runner, db_task_runner);

  // All tables objects that participate in managing the database must
  // be added here.
  profile_database_->AddTable(std::make_unique<KeywordTable>());
  profile_database_->AddTable(std::make_unique<TokenServiceTable>());
  profile_database_->LoadDatabase(os_crypt);

  keyword_web_data_ = base::MakeRefCounted<KeywordWebDataService>(
      profile_database_, ui_task_runner);
  keyword_web_data_->Init(
      base::BindOnce(show_error_callback, ERROR_LOADING_KEYWORD));

  token_web_data_ =
      base::MakeRefCounted<TokenWebData>(profile_database_, ui_task_runner);
  token_web_data_->Init(
      base::BindOnce(show_error_callback, ERROR_LOADING_TOKEN));
}

WebDataServiceWrapper::~WebDataServiceWrapper() = default;

void WebDataServiceWrapper::Shutdown() {
  keyword_web_data_->ShutdownOnUISequence();
  token_web_data_->ShutdownOnUISequence();

  profile_database_->ShutdownDatabase();
}

scoped_refptr<KeywordWebDataService>
WebDataServiceWrapper::GetKeywordWebData() {
  return keyword_web_data_;
}

scoped_refptr<TokenWebData> WebDataServiceWrapper::GetTokenWebData() {
  return token_web_data_;
}

