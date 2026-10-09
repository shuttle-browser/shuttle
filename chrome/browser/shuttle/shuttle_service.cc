// Copyright 2026 Silkweave
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/shuttle/shuttle_service.h"

#include <algorithm>
#include <optional>
#include <string_view>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "base/version_info/version_info.h"
#include "chrome/browser/shuttle/shuttle_service_factory.h"
#include "chrome/browser/shuttle/avatar_cache.h"
#include "chrome/browser/shuttle/mcp_tools.h"
#include "chrome/browser/shuttle/page_tools.h"
#include "chrome/browser/shuttle/profile_registry.h"
#include "chrome/browser/shuttle/session_cdp_filter.h"
#include "chrome/browser/shuttle/session_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_contents.h"

namespace shuttle {

namespace {

// Newest first; the client's version is echoed if supported.
constexpr std::string_view kProtocolVersions[] = {"2025-06-18", "2025-03-26",
                                                  "2024-11-05"};

constexpr char kInstructions[] =
    "Shuttle runs agents in tabs. An agent has an identity (name, "
    "color, avatar: shown on its tabs) and its own storage (logins "
    "persist). Flow: create-agent (once) -> open-session (agent id) -> "
    "allocate-tab -> snapshot / click / fill ... (page tools, take the "
    "targetId from allocate-tab; refs are @eN from snapshot) -> release-tab / "
    "close-session when done. Or drive the tab yourself through the session's "
    "cdpEndpoint (Playwright connectOverCDP(cdpEndpoint), agent-browser --cdp "
    "<cdpEndpoint>): it only sees the session's tabs and the agent's storage. "
    "Don't use the browser-wide endpoint (/json/version) for agent work.";

// JSON-RPC error codes.
constexpr int kParseError = -32700;
constexpr int kInvalidRequest = -32600;
constexpr int kMethodNotFound = -32601;
constexpr int kInvalidParams = -32602;

constexpr int kHttpOk = 200;
constexpr int kHttpAccepted = 202;
constexpr int kHttpBadRequest = 400;
constexpr char kJson[] = "application/json";

std::string Serialize(const base::DictValue& message) {
  return base::WriteJson(message).value_or("{}");
}

std::string ResultMessage(const base::Value& id, base::DictValue result) {
  return Serialize(base::DictValue()
                       .Set("jsonrpc", "2.0")
                       .Set("id", id.Clone())
                       .Set("result", std::move(result)));
}

std::string ErrorMessage(const base::Value& id,
                         int code,
                         std::string_view message) {
  return Serialize(
      base::DictValue()
          .Set("jsonrpc", "2.0")
          .Set("id", id.Clone())
          .Set("error",
               base::DictValue().Set("code", code).Set("message", message)));
}

base::DictValue InitializeResult(const base::DictValue* params) {
  std::string_view version = kProtocolVersions[0];
  if (params) {
    if (const std::string* requested = params->FindString("protocolVersion")) {
      for (std::string_view supported : kProtocolVersions) {
        if (*requested == supported) {
          version = supported;
        }
      }
    }
  }
  return base::DictValue()
      .Set("protocolVersion", version)
      .Set("capabilities",
           base::DictValue().Set(
               "tools", base::DictValue().Set("listChanged", false)))
      .Set("serverInfo",
           base::DictValue()
               .Set("name", "shuttle")
               .Set("version", version_info::GetVersionNumber()))
      .Set("instructions", kInstructions);
}

base::DictValue ToolsListResult() {
  base::ListValue tools;
  for (const Tool& tool : GetTools()) {
    std::optional<base::DictValue> schema =
        base::JSONReader::ReadDict(tool.input_schema, base::JSON_PARSE_RFC);
    CHECK(schema) << "Bad input schema for " << tool.name;
    tools.Append(base::DictValue()
                     .Set("name", tool.name)
                     .Set("description", tool.description)
                     .Set("inputSchema", std::move(*schema)));
  }
  for (base::Value& tool : PageToolManager::ListTools()) {
    tools.Append(std::move(tool));
  }
  return base::DictValue().Set("tools", std::move(tools));
}

}  // namespace

ShuttleService::ShuttleService(Profile* profile)
    : profiles_(std::make_unique<ProfileRegistry>(profile)),
      sessions_(std::make_unique<SessionManager>(profile, profiles_.get())),
      avatars_(std::make_unique<AvatarCache>(
          profile,
          profiles_.get(),
          base::BindRepeating(&SessionManager::RefreshGroups,
                              base::Unretained(sessions_.get())))),
      page_tools_(std::make_unique<PageToolManager>(sessions_.get(),
                                                    profile->GetPath())) {
  sessions_->SetSessionClosedCallback(base::BindRepeating(
      &PageToolManager::OnSessionClosed, base::Unretained(page_tools_.get())));
}

ShuttleService::~ShuttleService() = default;

// static
ShuttleService* ShuttleService::Get(Profile* profile) {
  return ShuttleServiceFactory::GetForProfile(profile);
}

void ShuttleService::HandleMcpRequest(const std::string& host,
                                          const std::string& body,
                                          HttpResponseCallback callback) {
  const base::Value null_id;
  std::optional<base::Value> parsed =
      base::JSONReader::Read(body, base::JSON_PARSE_RFC);
  if (!parsed) {
    std::move(callback).Run(kHttpBadRequest, kJson,
                            ErrorMessage(null_id, kParseError, "Parse error"));
    return;
  }
  if (!parsed->is_dict()) {
    // JSON-RPC batches were removed from MCP in 2025-06-18.
    std::move(callback).Run(
        kHttpBadRequest, kJson,
        ErrorMessage(null_id, kInvalidRequest, "Expected a JSON-RPC object"));
    return;
  }
  const base::DictValue& message = parsed->GetDict();
  const std::string* method = message.FindString("method");
  const base::Value* id = message.Find("id");
  if (!method) {
    // A response or malformed message; nothing to answer.
    std::move(callback).Run(kHttpAccepted, kJson, std::string());
    return;
  }
  if (!id) {
    // Notifications (notifications/initialized, cancelled, ...).
    std::move(callback).Run(kHttpAccepted, kJson, std::string());
    return;
  }
  const base::DictValue* params = message.FindDict("params");

  if (*method == "initialize") {
    std::move(callback).Run(kHttpOk, kJson,
                            ResultMessage(*id, InitializeResult(params)));
    return;
  }
  if (*method == "ping") {
    std::move(callback).Run(kHttpOk, kJson,
                            ResultMessage(*id, base::DictValue()));
    return;
  }
  if (*method == "tools/list") {
    std::move(callback).Run(kHttpOk, kJson,
                            ResultMessage(*id, ToolsListResult()));
    return;
  }
  if (*method != "tools/call") {
    std::move(callback).Run(
        kHttpOk, kJson,
        ErrorMessage(*id, kMethodNotFound, "Method not found: " + *method));
    return;
  }

  const std::string* name = params ? params->FindString("name") : nullptr;
  const base::DictValue* arguments =
      params ? params->FindDict("arguments") : nullptr;
  base::DictValue empty_arguments;
  ToolContext context{this, host};
  const Tool* tool = nullptr;
  if (name) {
    for (const Tool& candidate : GetTools()) {
      if (candidate.name == *name) {
        tool = &candidate;
      }
    }
  }
  if (!tool && name && PageToolManager::IsPageTool(*name)) {
    // Page tools answer asynchronously (the sidecar round trip).
    page_tools_->Call(
        *name, context, arguments ? *arguments : empty_arguments,
        base::BindOnce(
            [](base::Value id, HttpResponseCallback callback,
               ToolResult result) {
              std::move(callback).Run(
                  kHttpOk, kJson,
                  ResultMessage(id, std::move(result).ToValue()));
            },
            id->Clone(), std::move(callback)));
    return;
  }
  if (!tool) {
    std::move(callback).Run(
        kHttpOk, kJson,
        ErrorMessage(*id, kInvalidParams,
                     base::StrCat({"Unknown tool: ", name ? *name : ""})));
    return;
  }
  tool->run(context, arguments ? *arguments : empty_arguments,
            base::BindOnce(
                [](base::Value id, HttpResponseCallback callback,
                   ToolResult result) {
                  std::move(callback).Run(
                      kHttpOk, kJson,
                      ResultMessage(id, std::move(result).ToValue()));
                },
                id->Clone(), std::move(callback)));
}

std::vector<ShuttleService::AgentSummary> ShuttleService::ListAgents()
    const {
  std::vector<AgentSummary> agents;
  for (const ProfileRegistry::Entry& entry : profiles_->List()) {
    agents.push_back({entry.id, entry.name});
  }
  std::ranges::sort(agents, [](const AgentSummary& a, const AgentSummary& b) {
    return base::ToLowerASCII(a.name) < base::ToLowerASCII(b.name);
  });
  return agents;
}

std::string ShuttleService::AgentOfTab(
    content::WebContents* contents) const {
  return sessions_->AgentOf(contents);
}

std::string ShuttleService::AgentOfGroup(
    const tab_groups::TabGroupId& group) const {
  return sessions_->AgentOfGroup(group);
}

gfx::ImageSkia ShuttleService::AvatarOfGroup(
    const tab_groups::TabGroupId& group) {
  std::string agent_id = sessions_->AgentOfGroup(group);
  return agent_id.empty() ? gfx::ImageSkia() : avatars_->Get(agent_id);
}

void ShuttleService::HandTabToAgent(content::WebContents* contents,
                                        const std::string& agent_id) {
  sessions_->HandTabToAgent(contents, agent_id);
}

void ShuttleService::TakeBackTab(content::WebContents* contents) {
  sessions_->TakeBackTab(contents);
}

void ShuttleService::Shutdown() {
  // Cookies set shortly before exit are otherwise lost (seen in the
  // partition spike with Browser.close).
  profiles_->FlushAllCookies();
  page_tools_->Shutdown();
}

std::unique_ptr<content::DevToolsManagerDelegate::ConnectionFilter>
ShuttleService::CreateConnectionFilter(const std::string& session_id,
                                           const std::string& target_id) {
  if (!session_id.empty()) {
    if (!sessions_->Get(session_id)) {
      return nullptr;
    }
    return std::make_unique<SessionCdpFilter>(sessions_->GetWeakPtr(),
                                              session_id,
                                              /*page_connection=*/false);
  }
  content::WebContents* contents = SessionManager::FindTab(target_id);
  std::string owner = contents ? SessionManager::SessionOf(contents) : "";
  if (owner.empty()) {
    return nullptr;
  }
  return std::make_unique<SessionCdpFilter>(sessions_->GetWeakPtr(), owner,
                                            /*page_connection=*/true);
}

}  // namespace shuttle
