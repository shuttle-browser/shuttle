// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/login/login_handler.h"

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "build/build_config.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "extensions/buildflags/buildflags.h"
#include "net/base/auth.h"
#include "net/base/host_port_pair.h"
#include "net/base/load_flags.h"
#include "net/http/http_auth_scheme.h"
#include "net/http/http_transaction_factory.h"
#include "net/url_request/url_request_context.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "third_party/blink/public/common/loader/network_utils.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/text_elider.h"
#include "url/origin.h"

using content::BrowserThread;
using content::NavigationController;
using content::WebContents;

namespace {

// All login handlers should be tracked in this global singleton.
using LoginHandlerVector = std::vector<base::WeakPtr<LoginHandler>>;
LoginHandlerVector& GetAllLoginHandlers() {
  static base::NoDestructor<LoginHandlerVector> instance;
  return *instance;
}

}  // namespace

// ----------------------------------------------------------------------------
// LoginHandler

LoginHandler::~LoginHandler() {
  auto& login_handlers = GetAllLoginHandlers();
  for (auto it = login_handlers.begin(); it != login_handlers.end(); ++it) {
    if (it->get() == this) {
      login_handlers.erase(it);
      break;
    }
  }

  if (!WasAuthHandled()) {
    auth_required_callback_.Reset();
  }
}

// static
std::vector<LoginHandler*> LoginHandler::GetAllLoginHandlersForTest() {
  std::vector<LoginHandler*> output;
  for (auto& weak_ptr : GetAllLoginHandlers()) {
    if (weak_ptr) {
      output.push_back(weak_ptr.get());
    }
  }
  return output;
}

void LoginHandler::SetAuth(std::u16string_view username,
                           std::u16string_view password) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (WasAuthHandled()) {
    return;
  }

  net::AuthCredentials credentials{std::u16string(username),
                                   std::u16string(password)};
  content::LoginDelegate::LoginAuthRequiredCallback callback =
      std::move(auth_required_callback_);

  // Calling NotifyAuthSupplied() first allows other LoginHandler instances to
  // call CloseContents() before us. Closing dialogs in the opposite order as
  // they were created avoids races where remaining dialogs in the same tab may
  // be briefly displayed to the user before they are removed.
  if (web_contents_) {
    NotifyAuthSupplied(username, password);
  }
  CloseContents();
  std::move(callback).Run(std::move(credentials));
}

void LoginHandler::CancelAuth(bool notify_others) {
  if (WasAuthHandled()) {
    return;
  }

  content::LoginDelegate::LoginAuthRequiredCallback callback =
      std::move(auth_required_callback_);

  if (notify_others) {
    NotifyAuthCancelled();
  }

  CloseContents();
  std::move(callback).Run(std::nullopt);
}

LoginHandler::LoginHandler(
    const net::AuthChallengeInfo& auth_info,
    content::WebContents* web_contents,
    content::LoginDelegate::LoginAuthRequiredCallback auth_required_callback)
    : web_contents_(web_contents->GetWeakPtr()),
      auth_info_(auth_info),
      auth_required_callback_(std::move(auth_required_callback)) {
  GetAllLoginHandlers().push_back(weak_factory_.GetWeakPtr());
}

void LoginHandler::NotifyAuthNeeded() {
  // Only used by tests. This is being refactored. https://crbug.com/40870289.
}

void LoginHandler::NotifyAuthSupplied(std::u16string_view username,
                                      std::u16string_view password) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(WasAuthHandled());

  // Intentionally make a copy to avoid issues with iterator invalidation.
  LoginHandlerVector vec = GetAllLoginHandlers();
  for (auto& weak_login_handler : vec) {
    if (weak_login_handler && weak_login_handler.get() != this) {
      weak_login_handler->OtherHandlerFinished(/*supplied=*/true, this,
                                               username, password);
    }
  }
}

void LoginHandler::NotifyAuthCancelled() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(WasAuthHandled());

  // Intentionally make a copy to avoid issues with iterator invalidation.
  LoginHandlerVector vec = GetAllLoginHandlers();
  for (auto& weak_login_handler : vec) {
    if (weak_login_handler && weak_login_handler.get() != this) {
      weak_login_handler->OtherHandlerFinished(/*supplied=*/false, this,
                                               /*username=*/{},
                                               /*password=*/{});
    }
  }
}

void LoginHandler::OtherHandlerFinished(bool supplied,
                                        LoginHandler* other_handler,
                                        std::u16string_view username,
                                        std::u16string_view password) {
  // Break out early if we aren't interested in the notification.
  if (!web_contents_ || WasAuthHandled()) {
    return;
  }

  // Only listen to notifications within a single BrowserContext.
  if (other_handler->web_contents() &&
      other_handler->web_contents()->GetBrowserContext() !=
          web_contents_->GetBrowserContext()) {
    return;
  }

  // We should never dispatch to self.
  DCHECK(other_handler != this);

  // Only handle notification for the identical auth info. When comparing
  // AuthChallengeInfos, ignore path because the same credentials can be used
  // for different paths.
  if (!auth_info().MatchesExceptPath(other_handler->auth_info())) {
    return;
  }

  // Ignore login notification events from other StoragePartitions.
  // TODO(crbug.com/40202416): Getting the StoragePartition from the WebContents
  // is fine for now, but we'll need to plumb frame information to LoginHandler
  // as part of removing the multi-WebContents architecture.
  content::StoragePartition* source_partition =
      other_handler->web_contents() ? other_handler->web_contents()
                                          ->GetPrimaryMainFrame()
                                          ->GetStoragePartition()
                                    : nullptr;
  content::StoragePartition* partition =
      web_contents()->GetPrimaryMainFrame()->GetStoragePartition();
  if (!source_partition || source_partition != partition) {
    return;
  }

  // Set or cancel the auth in this handler. Defer an event loop iteration to
  // avoid potential reentrancy issues.
  if (supplied) {
    SetAuth(username, password);
  } else {
    CancelAuth(/*notify_others=*/true);
  }
}

// Returns whether authentication had been handled (SetAuth or CancelAuth).
bool LoginHandler::WasAuthHandled() const {
  return auth_required_callback_.is_null();
}

// Closes the view_contents from the UI loop.
void LoginHandler::CloseContents() {
  CloseDialog();
}

// static
void LoginHandler::GetDialogStrings(const GURL& request_url,
                                    const net::AuthChallengeInfo& auth_info,
                                    std::u16string* authority,
                                    std::u16string* explanation) {
  GURL authority_url;

  if (auth_info.is_proxy) {
    *authority = l10n_util::GetStringFUTF16(
        IDS_LOGIN_DIALOG_PROXY_AUTHORITY,
        url_formatter::FormatUrlForSecurityDisplay(
            auth_info.challenger.GetURL(), url_formatter::SchemeDisplay::SHOW));
    authority_url = auth_info.challenger.GetURL();
  } else {
    *authority = url_formatter::FormatUrlForSecurityDisplay(request_url);
#if BUILDFLAG(IS_ANDROID)
    // Android concatenates with a space rather than displaying on two separate
    // lines, so it needs some surrounding text.
    *authority =
        l10n_util::GetStringFUTF16(IDS_LOGIN_DIALOG_AUTHORITY, *authority);
#endif
    authority_url = request_url;
  }

  if (!network::IsUrlPotentiallyTrustworthy(authority_url)) {
    // TODO(asanka): The string should be different for proxies and servers.
    // http://crbug.com/40473278
    *explanation = l10n_util::GetStringUTF16(IDS_LOGIN_DIALOG_NOT_PRIVATE);
  } else {
    explanation->clear();
  }
}

void LoginHandler::ShowLoginPrompt(const GURL& request_url) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  std::u16string authority;
  std::u16string explanation;
  GetDialogStrings(request_url, auth_info(), &authority, &explanation);

  BuildViewAndNotify(authority, explanation);
}

void LoginHandler::BuildViewAndNotify(const std::u16string& authority,
                                      const std::u16string& explanation) {
  base::WeakPtr<LoginHandler> weak_this = weak_factory_.GetWeakPtr();
  bool success = BuildViewImpl(authority, explanation);
  if (!weak_this) {
    return;
  }
  if (success) {
    NotifyAuthNeeded();
  } else {
    // CancelAuth results in synchronous destruction of `this`. As building the
    // view happens synchronously, we dispatch the cancellation to avoid
    // re-entrancy into the calling code.
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&LoginHandler::CancelAuth, weak_this,
                                  /*notify_others=*/false));
  }
}
