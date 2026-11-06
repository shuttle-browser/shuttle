// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/download_protection/download_feedback.h"

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/task/task_runner.h"
#include "base/task/thread_pool.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/net_errors.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace safe_browsing {

namespace {

// Formerly uploaded a single downloaded binary to the safebrowsing download
// feedback service. Shuttle: the multipart uploader lived in the removed
// enterprise connectors code, so feedback finishes without uploading and the
// copied file is deleted.
class DownloadFeedbackImpl : public DownloadFeedback {
 public:
  DownloadFeedbackImpl(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const base::FilePath& file_path,
      uint64_t file_size,
      const std::string& ping_request,
      const std::string& ping_response);

  DownloadFeedbackImpl(const DownloadFeedbackImpl&) = delete;
  DownloadFeedbackImpl& operator=(const DownloadFeedbackImpl&) = delete;

  ~DownloadFeedbackImpl() override;

  void Start(base::OnceClosure finish_callback) override;

  const std::string& GetPingRequestForTesting() const override {
    return ping_request_;
  }

  const std::string& GetPingResponseForTesting() const override {
    return ping_response_;
  }

 private:
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  const base::FilePath file_path_;

  // The safebrowsing request and response of checking that this binary is
  // unsafe.
  std::string ping_request_;
  std::string ping_response_;
};

DownloadFeedbackImpl::DownloadFeedbackImpl(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const base::FilePath& file_path,
    uint64_t file_size,
    const std::string& ping_request,
    const std::string& ping_response)
    : url_loader_factory_(url_loader_factory),
      file_path_(file_path),
      ping_request_(ping_request),
      ping_response_(ping_response) {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI, base::NotFatalUntil::M162);
  DVLOG(1) << "DownloadFeedback constructed " << this << " for "
           << file_path.AsUTF8Unsafe();
}

DownloadFeedbackImpl::~DownloadFeedbackImpl() {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI, base::NotFatalUntil::M162);
  DVLOG(1) << "DownloadFeedback destructed " << this;

  base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT})
      ->PostTask(FROM_HERE, base::GetDeleteFileCallback(file_path_));
}

void DownloadFeedbackImpl::Start(base::OnceClosure finish_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  std::move(finish_callback).Run();
  // We may be deleted here.
}

}  // namespace

// static
const int64_t DownloadFeedback::kMaxUploadSize = 50 * 1024 * 1024;

// static
const char DownloadFeedback::kSbFeedbackURL[] =
    "https://safebrowsing.google.com/safebrowsing/uploads/chrome";

// static
DownloadFeedbackFactory* DownloadFeedback::factory_ = nullptr;

// static
std::unique_ptr<DownloadFeedback> DownloadFeedback::Create(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const base::FilePath& file_path,
    uint64_t file_size,
    const std::string& ping_request,
    const std::string& ping_response) {
  if (!factory_) {
    return base::WrapUnique(new DownloadFeedbackImpl(
        url_loader_factory, file_path, file_size, ping_request, ping_response));
  }
  return DownloadFeedback::factory_->CreateDownloadFeedback(
      url_loader_factory, file_path, file_size, ping_request, ping_response);
}

}  // namespace safe_browsing
