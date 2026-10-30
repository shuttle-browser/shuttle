// Copyright 2026 Silkweave
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/shuttle/home_tab.h"

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/single_thread_task_runner.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "content/public/common/referrer.h"
#include "ui/base/page_transition_types.h"
#include "content/public/common/url_constants.h"
#include "url/gurl.h"

namespace shuttle {

namespace {

// Also keeps the tab on chrome://agents: Navigate() sends other URLs to a new
// tab, but a navigation that bypasses it (CDP Page.navigate from the
// browser-wide endpoint) is sent back.
class HomeTabMarker : public content::WebContentsUserData<HomeTabMarker>,
                      public content::WebContentsObserver {
 public:
  ~HomeTabMarker() override = default;

  // content::WebContentsObserver:
  void DidFinishNavigation(content::NavigationHandle* handle) override {
    if (!handle->IsInPrimaryMainFrame() || !handle->HasCommitted() ||
        handle->IsSameDocument() || IsAgentsURL(handle->GetURL())) {
      return;
    }
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(
                       [](base::WeakPtr<content::WebContents> contents) {
                         if (contents && IsHomeTab(contents.get()) &&
                             !IsAgentsURL(contents->GetLastCommittedURL())) {
                           contents->GetController().LoadURL(
                               GURL(kHomeTabURL), content::Referrer(),
                               ui::PAGE_TRANSITION_AUTO_TOPLEVEL,
                               std::string());
                         }
                       },
                       web_contents()->GetWeakPtr()));
  }

 private:
  friend class content::WebContentsUserData<HomeTabMarker>;
  explicit HomeTabMarker(content::WebContents* contents)
      : content::WebContentsUserData<HomeTabMarker>(*contents),
        content::WebContentsObserver(contents) {}

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

WEB_CONTENTS_USER_DATA_KEY_IMPL(HomeTabMarker);

}  // namespace

bool IsHomeTab(const content::WebContents* contents) {
  return contents && HomeTabMarker::FromWebContents(
                         const_cast<content::WebContents*>(contents));
}

void MarkHomeTab(content::WebContents* contents) {
  HomeTabMarker::CreateForWebContents(contents);
}

bool IsAgentsURL(const GURL& url) {
  return url.SchemeIs(content::kChromeUIScheme) && url.host() == "agents";
}

}  // namespace shuttle
