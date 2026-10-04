// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>
#include <vector>

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry_factory.h"
#include "chrome/browser/extensions/chrome_content_browser_client_extensions_part.h"
#include "chrome/browser/extensions/chrome_extensions_browser_client.h"
#include "chrome/browser/extensions/chrome_process_manager_delegate.h"
#include "chrome/browser/extensions/error_console/error_console.h"
#include "chrome/browser/extensions/menu_manager.h"
#include "chrome/browser/extensions/user_script_listener.h"
#include "components/safe_browsing/buildflags.h"
#include "extensions/browser/safe_browsing_delegate.h"
#include "ipc/constants.mojom.h"

#if BUILDFLAG(FULL_SAFE_BROWSING)
#include "chrome/browser/extensions/chrome_safe_browsing_delegate.h"
#endif

namespace extensions {

void ChromeExtensionsBrowserClient::Init() {
  process_manager_delegate_ = std::make_unique<ChromeProcessManagerDelegate>();

  // Must occur after g_browser_process is initialized.
  user_script_listener_ = std::make_unique<UserScriptListener>();

#if BUILDFLAG(FULL_SAFE_BROWSING)
  // Full safe browsing is supported so use the Chrome delegate.
  safe_browsing_delegate_ = std::make_unique<ChromeSafeBrowsingDelegate>();
#else
  // Safe browsing is not available, use a noop delegate.
  safe_browsing_delegate_ = std::make_unique<SafeBrowsingDelegate>();
#endif
}

void ChromeExtensionsBrowserClient::StartTearDown() {
  user_script_listener_->StartTearDown();
  process_manager_delegate_->StartTearDown();
}

ProcessManagerDelegate*
ChromeExtensionsBrowserClient::GetProcessManagerDelegate() const {
  return process_manager_delegate_.get();
}

mojo::PendingRemote<network::mojom::URLLoaderFactory>
ChromeExtensionsBrowserClient::GetControlledFrameEmbedderURLLoader(
    const url::Origin& app_origin,
    content::FrameTreeNodeId frame_tree_node_id,
    content::BrowserContext* browser_context) {
  // ControlledFrame is only available to Isolated Web Apps, which are removed.
  return mojo::PendingRemote<network::mojom::URLLoaderFactory>();
}

void ChromeExtensionsBrowserClient::ReportError(
    content::BrowserContext* context,
    std::unique_ptr<ExtensionError> error) {
  ErrorConsole::Get(context)->ReportError(std::move(error));
}

void ChromeExtensionsBrowserClient::CleanUpWebView(
    content::BrowserContext* browser_context,
    int embedder_process_id,
    int view_instance_id) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  if (ChromeContentBrowserClientExtensionsPart::AreExtensionsDisabledForProfile(
          profile)) {
    return;
  }

  // Clean up context menus for the WebView.
  auto* menu_manager = MenuManager::Get(profile);
  DCHECK(menu_manager);
  // The |webview_embedder_frame_id| parameter of ExtensionKey is not used to
  // identify the context menu items that belong to a WebView so it is OK for it
  // to be |IPC::mojom::kRoutingIdNone| here.
  menu_manager->RemoveAllContextItems(MenuItem::ExtensionKey(
      "", embedder_process_id,
      /*webview_embedder_frame_id=*/IPC::mojom::kRoutingIdNone,
      view_instance_id));
}

custom_handlers::ProtocolHandlerRegistry*
ChromeExtensionsBrowserClient::GetProtocolHandlerRegistry(
    content::BrowserContext* context) {
  return ProtocolHandlerRegistryFactory::GetForBrowserContext(context);
}

}  // namespace extensions
