// Copyright 2026 The Agent Chrome Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/agents/agents_ui.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/agent_chrome/agent_chrome_service.h"
#include "chrome/browser/agent_chrome/mcp_tools.h"
#include "chrome/browser/agent_chrome/session_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/theme_source.h"
#include "chrome/grit/agents_resources.h"
#include "chrome/grit/agents_resources_map.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "content/public/common/url_constants.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/webui/webui_util.h"
#include "url/gurl.h"

namespace agent_chrome {

namespace {

class AgentsHandler : public content::WebUIMessageHandler {
 public:
  AgentsHandler() = default;
  AgentsHandler(const AgentsHandler&) = delete;
  AgentsHandler& operator=(const AgentsHandler&) = delete;
  ~AgentsHandler() override = default;

  // content::WebUIMessageHandler:
  void RegisterMessages() override {
    web_ui()->RegisterMessageCallback(
        "callTool", base::BindRepeating(&AgentsHandler::HandleCallTool,
                                        base::Unretained(this)));
    web_ui()->RegisterMessageCallback(
        "openTab", base::BindRepeating(&AgentsHandler::HandleOpenTab,
                                       base::Unretained(this)));
    web_ui()->RegisterMessageCallback(
        "showTab", base::BindRepeating(&AgentsHandler::HandleShowTab,
                                       base::Unretained(this)));
  }

 private:
  AgentChromeService* service() {
    return AgentChromeService::Get(Profile::FromWebUI(web_ui()));
  }

  // Rejects with `message` (shown by the page).
  void Reject(const base::Value& callback_id, const std::string& message) {
    RejectJavascriptCallback(callback_id, base::Value(message));
  }

  // [callbackId, toolName, args]: runs an MCP tool; resolves with its
  // structured result, rejects with its error text.
  void HandleCallTool(const base::ListValue& args) {
    AllowJavascript();
    const base::Value& callback_id = args[0];
    const std::string* name = args.size() > 1 ? args[1].GetIfString() : nullptr;
    const base::DictValue* tool_args =
        args.size() > 2 ? args[2].GetIfDict() : nullptr;
    AgentChromeService* agents = service();
    if (!name || !agents) {
      Reject(callback_id, "Agents are not available");
      return;
    }
    for (const Tool& tool : GetTools()) {
      if (tool.name != *name) {
        continue;
      }
      ToolContext context;
      context.service = agents;
      base::DictValue run_args =
          tool_args ? tool_args->Clone() : base::DictValue();
      tool.run(context, run_args,
               base::BindOnce(&AgentsHandler::OnToolDone,
                              weak_factory_.GetWeakPtr(), callback_id.Clone()));
      return;
    }
    Reject(callback_id, "Unknown tool: " + *name);
  }

  void OnToolDone(base::Value callback_id, ToolResult result) {
    if (!IsJavascriptAllowed()) {
      return;
    }
    if (result.is_error) {
      std::string message = "Error";
      if (!result.content.empty() && result.content[0].is_dict()) {
        if (const std::string* text =
                result.content[0].GetDict().FindString("text")) {
          message = *text;
        }
      }
      Reject(callback_id, message);
      return;
    }
    ResolveJavascriptCallback(
        callback_id, result.structured_content
                         ? base::Value(std::move(*result.structured_content))
                         : base::Value());
  }

  // [callbackId, agentId, url]: opens a foreground tab as the agent.
  void HandleOpenTab(const base::ListValue& args) {
    AllowJavascript();
    const base::Value& callback_id = args[0];
    const std::string* agent_id =
        args.size() > 1 ? args[1].GetIfString() : nullptr;
    const std::string* url = args.size() > 2 ? args[2].GetIfString() : nullptr;
    AgentChromeService* agents = service();
    if (!agent_id || !agents) {
      Reject(callback_id, "Agents are not available");
      return;
    }
    GURL gurl;
    if (url && !url->empty()) {
      gurl = GURL(*url);
      if (!gurl.is_valid()) {
        gurl = GURL("https://" + *url);
      }
      if (!gurl.is_valid() || !gurl.SchemeIsHTTPOrHTTPS()) {
        Reject(callback_id, "Enter an http(s) URL");
        return;
      }
    }
    auto contents = agents->sessions().OpenTabForUser(*agent_id, gurl);
    if (!contents.has_value()) {
      Reject(callback_id, contents.error());
      return;
    }
    ResolveJavascriptCallback(callback_id, base::Value());
  }

  // [callbackId, targetId]: brings an agent's tab to the front.
  void HandleShowTab(const base::ListValue& args) {
    AllowJavascript();
    const base::Value& callback_id = args[0];
    const std::string* target_id =
        args.size() > 1 ? args[1].GetIfString() : nullptr;
    content::WebContents* contents =
        target_id ? SessionManager::FindTab(*target_id) : nullptr;
    if (!contents || !SessionManager::ShowTab(contents)) {
      Reject(callback_id, "The tab is gone");
      return;
    }
    ResolveJavascriptCallback(callback_id, base::Value());
  }

  base::WeakPtrFactory<AgentsHandler> weak_factory_{this};
};

}  // namespace

AgentsUIConfig::AgentsUIConfig()
    : DefaultWebUIConfig(content::kChromeUIScheme, kChromeUIAgentsHost) {}

AgentsUI::AgentsUI(content::WebUI* web_ui) : content::WebUIController(web_ui) {
  web_ui->AddMessageHandler(std::make_unique<AgentsHandler>());
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(), kChromeUIAgentsHost);
  webui::SetupWebUIDataSource(source, kAgentsResources, IDR_AGENTS_AGENTS_HTML);
  // The toolbar's product logo (chrome://theme/current-channel-logo).
  content::URLDataSource::Add(Profile::FromWebUI(web_ui),
                              std::make_unique<ThemeSource>(
                                  Profile::FromWebUI(web_ui)));
  // Avatars are https: or data: URLs.
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ImgSrc,
      "img-src chrome://resources chrome://theme data: https:;");
}

AgentsUI::~AgentsUI() = default;

}  // namespace agent_chrome
