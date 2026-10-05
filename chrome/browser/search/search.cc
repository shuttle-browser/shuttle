// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/search.h"

#include <stddef.h>

#include <string>

#include "base/check_deref.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/search_engines/ui_thread_search_terms_data.h"
#include "chrome/browser/supervised_user/supervised_user_url_filtering_service_factory.h"
#include "chrome/common/url_constants.h"
#include "components/google/core/common/google_util.h"
#include "components/search/ntp_features.h"
#include "components/search/search.h"
#include "components/search_engines/search_engine_type.h"
#include "components/search_engines/template_url_service.h"
#include "components/supervised_user/core/browser/supervised_user_preferences.h"
#include "components/supervised_user/core/browser/supervised_user_url_filtering_service.h"
#include "components/supervised_user/core/browser/supervised_user_utils.h"
#include "components/supervised_user/core/common/buildflags.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/security_principal.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/device_info.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/common/webui_url_constants.h"
#endif

namespace search {

bool DefaultSearchProviderIsGoogle(Profile* profile) {
  return DefaultSearchProviderIsGoogle(
      TemplateURLServiceFactory::GetForProfile(profile));
}

bool IsNTPOrRelatedURL(const GURL& url, Profile* profile) {
  // Agent Chrome: the only New Tab Page is the blank chrome://newtab.
  return url.is_valid() && url == chrome::ChromeUINewTabURLAsGURL();
}

bool IsNTPURL(const GURL& url) {
#if BUILDFLAG(IS_ANDROID)
  return (url.SchemeIs(chrome::kChromeNativeScheme) &&
          url.host() == chrome::kChromeUINewTabHost);
#else
  return url.SchemeIs(content::kChromeUIScheme) &&
         url.host() == chrome::kChromeUINewTabHost;
#endif
}

bool IsWebUiNtpEnabledForDesktopAndroid() {
#if BUILDFLAG(IS_ANDROID)
  return base::android::device_info::is_desktop() &&
         base::FeatureList::IsEnabled(chrome::android::kUseWebUiNtpAndroid);
#else
  return false;
#endif
}

bool IsSplitViewNewTabPage(const GURL& url) {
  return url.spec() == chrome::kChromeUISplitViewNewTabPageURL;
}

const void* const kIsNTPProcessKey = &kIsNTPProcessKey;

}  // namespace search
