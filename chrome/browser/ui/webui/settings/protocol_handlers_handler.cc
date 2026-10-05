// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/protocol_handlers_handler.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/url_identity.h"
#include "chrome/common/url_constants.h"
#include "components/custom_handlers/protocol_handler.h"
#include "components/google/core/common/google_util.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_ui.h"

namespace settings {

namespace {

base::ListValue GetHandlersAsListValue(
    const custom_handlers::ProtocolHandlerRegistry* registry,
    const custom_handlers::ProtocolHandlerRegistry::ProtocolHandlerList&
        handlers) {
  base::ListValue handler_list;
  for (const auto& handler : handlers) {
    base::DictValue handler_value;
    handler_value.Set("protocol_display_name",
                      handler.GetProtocolDisplayName());
    handler_value.Set("protocol", handler.protocol());
    handler_value.Set("spec", handler.url().spec());
    handler_value.Set("host", handler.url().GetHost());
    if (registry) {
      handler_value.Set("is_default", registry->IsDefault(handler));
    }

    handler_list.Append(std::move(handler_value));
  }
  return handler_list;
}

}  // namespace

ProtocolHandlersHandler::ProtocolHandlersHandler(Profile* profile)
    : profile_(profile) {}

ProtocolHandlersHandler::~ProtocolHandlersHandler() = default;

void ProtocolHandlersHandler::OnJavascriptAllowed() {
  registry_observation_.Observe(GetProtocolHandlerRegistry());
}

void ProtocolHandlersHandler::OnJavascriptDisallowed() {
  registry_observation_.Reset();
}

void ProtocolHandlersHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "observeProtocolHandlers",
      base::BindRepeating(
          &ProtocolHandlersHandler::HandleObserveProtocolHandlers,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "observeProtocolHandlersEnabledState",
      base::BindRepeating(
          &ProtocolHandlersHandler::HandleObserveProtocolHandlersEnabledState,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "removeHandler",
      base::BindRepeating(&ProtocolHandlersHandler::HandleRemoveHandler,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "setHandlersEnabled",
      base::BindRepeating(&ProtocolHandlersHandler::HandleSetHandlersEnabled,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "setDefault",
      base::BindRepeating(&ProtocolHandlersHandler::HandleSetDefault,
                          base::Unretained(this)));
}

void ProtocolHandlersHandler::OnProtocolHandlerRegistryChanged() {
  SendHandlersEnabledValue();
  UpdateHandlerList();
}

base::DictValue ProtocolHandlersHandler::GetHandlersForProtocol(
    const std::string& protocol) {
  base::DictValue handlers_value;
  custom_handlers::ProtocolHandlerRegistry* registry =
      GetProtocolHandlerRegistry();
  handlers_value.Set(
      "protocol_display_name",
      custom_handlers::ProtocolHandler::GetProtocolDisplayName(protocol));
  handlers_value.Set("protocol", protocol);

  base::ListValue handlers_list = GetHandlersAsListValue(
      registry, registry->GetHandlersFor(protocol));
  handlers_value.Set("handlers", std::move(handlers_list));
  return handlers_value;
}

base::ListValue ProtocolHandlersHandler::GetIgnoredHandlers() {
  custom_handlers::ProtocolHandlerRegistry* registry =
      GetProtocolHandlerRegistry();
  custom_handlers::ProtocolHandlerRegistry::ProtocolHandlerList
      ignored_handlers = registry->GetIgnoredHandlers();
  return GetHandlersAsListValue(registry, ignored_handlers);
}

void ProtocolHandlersHandler::UpdateHandlerList() {
  custom_handlers::ProtocolHandlerRegistry* registry =
      GetProtocolHandlerRegistry();
  std::vector<std::string> protocols;
  registry->GetRegisteredProtocols(&protocols);

  base::ListValue handlers;
  for (auto& protocol : protocols) {
    handlers.Append(GetHandlersForProtocol(protocol));
  }

  FireWebUIListener("setProtocolHandlers", handlers);
  FireWebUIListener("setIgnoredProtocolHandlers", GetIgnoredHandlers());
}

void ProtocolHandlersHandler::HandleObserveProtocolHandlers(
    const base::ListValue& args) {
  AllowJavascript();
  SendHandlersEnabledValue();
  UpdateHandlerList();
}

void ProtocolHandlersHandler::HandleObserveProtocolHandlersEnabledState(
    const base::ListValue& args) {
  AllowJavascript();
  SendHandlersEnabledValue();
}

void ProtocolHandlersHandler::SendHandlersEnabledValue() {
  FireWebUIListener("setHandlersEnabled",
                    base::Value(GetProtocolHandlerRegistry()->enabled()));
}

void ProtocolHandlersHandler::HandleRemoveHandler(const base::ListValue& args) {
  custom_handlers::ProtocolHandler handler(ParseHandlerFromArgs(args));
  CHECK(!handler.IsEmpty());
  GetProtocolHandlerRegistry()->RemoveHandler(handler);

  // No need to call UpdateHandlerList() - we should receive a notification
  // that the ProtocolHandlerRegistry has changed and we will update the view
  // then.
}

void ProtocolHandlersHandler::HandleSetHandlersEnabled(
    const base::ListValue& args) {
  bool enabled = true;
  CHECK(args[0].is_bool());
  enabled = args[0].GetBool();
  if (enabled) {
    GetProtocolHandlerRegistry()->Enable();
  } else {
    GetProtocolHandlerRegistry()->Disable();
  }
}

void ProtocolHandlersHandler::HandleSetDefault(const base::ListValue& args) {
  const custom_handlers::ProtocolHandler& handler(ParseHandlerFromArgs(args));
  CHECK(!handler.IsEmpty());
  GetProtocolHandlerRegistry()->OnAcceptRegisterProtocolHandler(handler);
}

custom_handlers::ProtocolHandler ProtocolHandlersHandler::ParseHandlerFromArgs(
    const base::ListValue& args) const {
  bool ok = args.size() >= 2u && args[0].is_string() && args[1].is_string();
  if (!ok) {
    return custom_handlers::ProtocolHandler::EmptyProtocolHandler();
  }
  const std::string& protocol = args[0].GetString();
  const std::string& url = args[1].GetString();
  return custom_handlers::ProtocolHandler::CreateProtocolHandler(protocol,
                                                                 GURL(url));
}

custom_handlers::ProtocolHandlerRegistry*
ProtocolHandlersHandler::GetProtocolHandlerRegistry() {
  return ProtocolHandlerRegistryFactory::GetForBrowserContext(profile_);
}

}  // namespace settings
