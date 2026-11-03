// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry_factory.h"
#include "chrome/browser/devtools/protocol/page_handler.h"
#include "chrome/browser/shuttle/home_tab.h"
#include "components/custom_handlers/protocol_handler_registry.h"
#include "components/subresource_filter/content/browser/devtools_interaction_tracker.h"
#include "content/public/browser/web_contents.h"

PageHandler::PageHandler(scoped_refptr<content::DevToolsAgentHost> agent_host,
                         content::WebContents* web_contents,
                         protocol::UberDispatcher* dispatcher,
                         bool is_trusted)
    : agent_host_(agent_host),
      web_contents_(web_contents->GetWeakPtr()),
      is_trusted_(is_trusted) {
  protocol::Page::Dispatcher::wire(dispatcher, this);
}

PageHandler::~PageHandler() {
  Disable();
}

void PageHandler::ToggleAdBlocking(bool enabled) {
  if (!web_contents_) {
    return;
  }

  // Create the DevtoolsInteractionTracker lazily (note that this call is a
  // no-op if the object was already created).
  subresource_filter::DevtoolsInteractionTracker::CreateForWebContents(
      web_contents_.get());

  subresource_filter::DevtoolsInteractionTracker::FromWebContents(
      web_contents_.get())
      ->ToggleForceActivation(enabled);
}

protocol::Response PageHandler::Enable(
    std::optional<bool> enable_file_chooser_opened_event) {
  enabled_ = true;
  // Do not mark the command as handled. Let it fall through instead, so that
  // the handler in content gets a chance to process the command.
  return protocol::Response::FallThrough();
}

protocol::Response PageHandler::Disable() {
  enabled_ = false;
  ToggleAdBlocking(false /* enable */);
  // Do not mark the command as handled. Let it fall through instead, so that
  // the handler in content gets a chance to process the command.
  return protocol::Response::FallThrough();
}

protocol::Response PageHandler::Close() {
  if (shuttle::IsHomeTab(web_contents_.get())) {
    return protocol::Response::ServerError(
        "The home tab closes only with its window");
  }
  return protocol::Response::FallThrough();
}

protocol::Response PageHandler::SetAdBlockingEnabled(bool enabled) {
  if (!enabled_) {
    return protocol::Response::ServerError("Page domain is disabled.");
  }
  ToggleAdBlocking(enabled);
  return protocol::Response::Success();
}

protocol::Response PageHandler::SetRPHRegistrationMode(
    const protocol::String& mode) {
  if (!web_contents_) {
    return protocol::Response::ServerError("No web contents to host a dialog.");
  }
  if (!is_trusted_) {
    return protocol::Response::ServerError(
        "Permission denied: Page.setRPHRegistrationMode requires a trusted "
        "client");
  }

  custom_handlers::RphRegistrationMode rph_mode =
      custom_handlers::RphRegistrationMode::kNone;
  if (mode == protocol::Page::SetRPHRegistrationMode::ModeEnum::AutoAccept) {
    rph_mode = custom_handlers::RphRegistrationMode::kAutoAccept;
  } else if (mode ==
             protocol::Page::SetRPHRegistrationMode::ModeEnum::AutoReject) {
    rph_mode = custom_handlers::RphRegistrationMode::kAutoReject;
  } else if (mode != protocol::Page::SetRPHRegistrationMode::ModeEnum::None) {
    return protocol::Response::ServerError("Unrecognized mode value");
  }

  custom_handlers::ProtocolHandlerRegistry* registry =
      ProtocolHandlerRegistryFactory::GetForBrowserContext(
          web_contents_->GetBrowserContext());
  registry->SetRphRegistrationMode(rph_mode);
  return protocol::Response::Success();
}
