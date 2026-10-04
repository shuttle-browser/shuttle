// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_PROTOCOL_HANDLERS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_PROTOCOL_HANDLERS_HANDLER_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/values.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"
#include "components/custom_handlers/protocol_handler_registry.h"

class Profile;

////////////////////////////////////////////////////////////////////////////////
// ProtocolHandlersHandler

// Listen for changes to protocol handlers registrations.
// This get triggered whenever a user allows or disallows a specific website or
// application to handle clicks on a link with a specified protocol (i.e.
// mailto: -> Gmail).

namespace settings {

class ProtocolHandlersHandler
    : public SettingsPageUIHandler,
      public custom_handlers::ProtocolHandlerRegistry::Observer {
 public:
  explicit ProtocolHandlersHandler(Profile* profile);

  ProtocolHandlersHandler(const ProtocolHandlersHandler&) = delete;
  ProtocolHandlersHandler& operator=(const ProtocolHandlersHandler&) = delete;

  ~ProtocolHandlersHandler() override;

  // SettingsPageUIHandler:
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;
  void RegisterMessages() override;

  // ProtocolHandlerRegistry::Observer:
  void OnProtocolHandlerRegistryChanged() override;

 private:
  // Called to fetch the state of the protocol handlers. If the full list of
  // handlers is not needed, consider HandleObserveProtocolHandlersEnabledState
  // instead.
  void HandleObserveProtocolHandlers(const base::ListValue& args);

  // Called to begin updates to the handlers enabled status. This is a subset
  // (lighter alternative) of HandleObserveProtocolHandlers. There's no need to
  // call this function if HandleObserveProtocolHandlers is called.
  void HandleObserveProtocolHandlersEnabledState(const base::ListValue& args);

  // Notifies the JS side whether the handlers are enabled or not.
  void SendHandlersEnabledValue();

  // Called when the user toggles whether custom handlers are enabled.
  void HandleSetHandlersEnabled(const base::ListValue& args);

  // Called when the user sets a new default handler for a protocol.
  void HandleSetDefault(const base::ListValue& args);

  // Parses a ProtocolHandler out of the arguments passed back from the view.
  // |args| is a list of [protocol, url].
  custom_handlers::ProtocolHandler ParseHandlerFromArgs(
      const base::ListValue& args) const;

  // Populates a JSON object describing the set of protocol handlers for the
  // given protocol.
  base::DictValue GetHandlersForProtocol(const std::string& protocol);

  // Returns a JSON list of the ignored protocol handlers.
  base::ListValue GetIgnoredHandlers();

  // Called when the JS PasswordManager object is initialized.
  void UpdateHandlerList();

  // Remove a handler.
  // |args| is a list of [protocol, url].
  void HandleRemoveHandler(const base::ListValue& args);

  custom_handlers::ProtocolHandlerRegistry* GetProtocolHandlerRegistry();

  base::ScopedObservation<custom_handlers::ProtocolHandlerRegistry,
                          custom_handlers::ProtocolHandlerRegistry::Observer>
      registry_observation_{this};

  // Called to fetch the state of the app protocol handlers. Web apps are not
  // supported in this build, so the lists are always empty.
  void HandleObserveAppProtocolHandlers(const base::ListValue& args);

  const raw_ptr<Profile> profile_;
};

}  // namespace settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_PROTOCOL_HANDLERS_HANDLER_H_
