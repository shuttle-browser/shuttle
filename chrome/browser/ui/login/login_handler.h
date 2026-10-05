// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LOGIN_LOGIN_HANDLER_H_
#define CHROME_BROWSER_UI_LOGIN_LOGIN_HANDLER_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/login_delegate.h"
#include "content/public/browser/web_contents.h"
#include "net/base/auth.h"

class GURL;

// This is the base implementation for the OS-specific classes that prompt for
// authentication information.
class LoginHandler : public content::LoginDelegate {
 public:
  ~LoginHandler() override;

  // Builds the platform specific LoginHandler. The resulting handler calls
  // auth_required_callback when credentials are available. If destroyed before
  // them, the login request is aborted and the callback will not be called. The
  // callback must remain valid until one of those two events occurs.
  static std::unique_ptr<LoginHandler> Create(
      const net::AuthChallengeInfo& auth_info,
      content::WebContents* web_contents,
      content::LoginDelegate::LoginAuthRequiredCallback auth_required_callback);

  // Call after `Create()` to show the dialog.
  void ShowLoginPrompt(const GURL& request_url);

  // Exposed for testing.
  static std::vector<LoginHandler*> GetAllLoginHandlersForTest();

  // Resend the request with authentication credentials.
  // This function can be called from either thread.
  void SetAuth(std::u16string_view username, std::u16string_view password);

  // Display the error page without asking for credentials again. Setting
  // `notify_others` to `true` will close all other login handlers as well.
  // This function can be called from either thread.
  void CancelAuth(bool notify_others);

  // Who/where/what asked for the authentication.
  const net::AuthChallengeInfo& auth_info() const { return auth_info_; }

  // The WebContents.
  content::WebContents* web_contents() { return web_contents_.get(); }

 protected:
  LoginHandler(
      const net::AuthChallengeInfo& auth_info,
      content::WebContents* web_contents,
      content::LoginDelegate::LoginAuthRequiredCallback auth_required_callback);

  // Implement this to initialize the underlying platform specific view.
  // Returns `false` if the view cannot be built.
  virtual bool BuildViewImpl(const std::u16string& authority,
                             const std::u16string& explanation) = 0;

  // Closes the native dialog.
  virtual void CloseDialog() = 0;

  // Notify observers that authentication is needed.
  virtual void NotifyAuthNeeded();

  // Notify observers that authentication is supplied.
  virtual void NotifyAuthSupplied(std::u16string_view username,
                                  std::u16string_view password);

  // Notify observers that authentication is cancelled.
  virtual void NotifyAuthCancelled();

 private:
  FRIEND_TEST_ALL_PREFIXES(LoginHandlerTest, DialogStringsAndRealm);

  // When any handler finishes, called on every other handler. |username| and
  // |password| are only valid if |supplied| is true. If |supplied| is false
  // then the handler was cancelled. This gives |this| handler the opportunity
  // to dismiss itself if it was waiting for the same authentication.
  void OtherHandlerFinished(bool supplied,
                            LoginHandler* other_handler,
                            std::u16string_view username,
                            std::u16string_view password);

  // Returns whether authentication had been handled (SetAuth or CancelAuth).
  bool WasAuthHandled() const;

  // Closes the view_contents from the UI loop.
  void CloseContents();

  static void GetDialogStrings(const GURL& request_url,
                               const net::AuthChallengeInfo& auth_info,
                               std::u16string* authority,
                               std::u16string* explanation);

  void BuildViewAndNotify(const std::u16string& authority,
                          const std::u16string& explanation);

  base::WeakPtr<content::WebContents> web_contents_;

  // Who/where/what asked for the authentication.
  net::AuthChallengeInfo auth_info_;

  content::LoginDelegate::LoginAuthRequiredCallback auth_required_callback_;

  base::WeakPtrFactory<LoginHandler> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_LOGIN_LOGIN_HANDLER_H_
