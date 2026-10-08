// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_BROWSER_COMMAND_BROWSER_COMMAND_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_BROWSER_COMMAND_BROWSER_COMMAND_HANDLER_H_

#include <memory>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/command_updater_delegate.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/window_open_disposition.h"
#include "ui/webui/resources/js/browser_command/browser_command.mojom.h"
#include "url/gurl.h"

class CommandUpdater;
class Profile;


// Handles browser commands send from JS.
class BrowserCommandHandler : public CommandUpdaterDelegate,
                              public browser_command::mojom::CommandHandler {
 public:
  static const char kPromoBrowserCommandHistogramName[];

  BrowserCommandHandler(
      mojo::PendingReceiver<browser_command::mojom::CommandHandler>
          pending_page_handler,
      Profile* profile,
      std::vector<browser_command::mojom::Command> supported_commands,
      content::WebContents* web_contents);
  ~BrowserCommandHandler() override;

  // browser_command::mojom::CommandHandler:
  void CanExecuteCommand(browser_command::mojom::Command command_id,
                         CanExecuteCommandCallback callback) override;
  void ExecuteCommand(browser_command::mojom::Command command_id,
                      browser_command::mojom::ClickInfoPtr click_info,
                      ExecuteCommandCallback callback) override;

  // CommandUpdaterDelegate:
  void HandleCommandWithDisposition(int command_id,
                                    WindowOpenDisposition disposition,
                                    base::TimeTicks time_stamp) override;

 protected:
  void EnableSupportedCommands();

  virtual CommandUpdater* GetCommandUpdater();

  virtual bool BrowserSupportsTabGroups();
  virtual bool DefaultSearchProviderIsGoogle();

 private:
  virtual void NavigateToURL(const GURL& url,
                             WindowOpenDisposition disposition);
  virtual void NavigateToEnhancedProtectionSetting();
  virtual void OpenAISettings();
  virtual void OpenSplitView();
  virtual void EnableVerticalTabs();

  raw_ptr<Profile, DanglingUntriaged> profile_;
  std::vector<browser_command::mojom::Command> supported_commands_;
  std::unique_ptr<CommandUpdater> command_updater_;
  mojo::Receiver<browser_command::mojom::CommandHandler> page_handler_;
  raw_ptr<content::WebContents> web_contents_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_BROWSER_COMMAND_BROWSER_COMMAND_HANDLER_H_
