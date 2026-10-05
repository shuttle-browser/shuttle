// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOGIN_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_LOGIN_VIEW_H_

#include <string>
#include <string_view>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/login/login_handler.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace views {
class Textfield;
}

// This class is responsible for displaying the contents of a login window
// for HTTP/FTP authentication.
class LoginView : public views::View {
  METADATA_HEADER(LoginView, views::View)

 public:
  LoginView(const std::u16string& authority,
            const std::u16string& explanation);
  LoginView(const LoginView&) = delete;
  LoginView& operator=(const LoginView&) = delete;
  ~LoginView() override;

  // Access the data in the username/password text fields.
  std::u16string_view GetUsername() const;
  std::u16string_view GetPassword() const;

  // Used by LoginHandlerViews to set the initial focus.
  views::View* GetInitiallyFocusedView();

 private:
  // Non-owning refs to the input text fields.
  raw_ptr<views::Textfield> username_field_;
  raw_ptr<views::Textfield> password_field_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOGIN_VIEW_H_
