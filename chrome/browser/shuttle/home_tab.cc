// Copyright 2026 Silkweave
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/shuttle/home_tab.h"

#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"
#include "content/public/common/url_constants.h"
#include "url/gurl.h"

namespace shuttle {

namespace {

class HomeTabMarker : public content::WebContentsUserData<HomeTabMarker> {
 public:
  ~HomeTabMarker() override = default;

 private:
  friend class content::WebContentsUserData<HomeTabMarker>;
  explicit HomeTabMarker(content::WebContents* contents)
      : content::WebContentsUserData<HomeTabMarker>(*contents) {}

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
