// Copyright 2026 The Agent Chrome Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/agent_chrome/agent_chrome_service.h"

#include <optional>
#include <string_view>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/strings/strcat.h"
#include "base/values.h"
#include "base/version_info/version_info.h"
#include "chrome/browser/agent_chrome/agent_chrome_service_factory.h"
#include "chrome/browser/agent_chrome/mcp_tools.h"
#include "chrome/browser/agent_chrome/profile_registry.h"
#include "chrome/browser/agent_chrome/session_manager.h"
#include "chrome/browser/profiles/profile.h"

namespace agent_chrome {

namespace {

// Newest first; the client's version is echoed if supported.
constexpr std::string_view kProtocolVersions[] = {"2025-06-18", "2025-03-26",
                                                  "2024-11-05"};

constexpr char kInstructions[] =
    "Agent Chrome runs agents in tabs. An agent has an identity (name, "
    "color, avatar: shown on its tab group) and its own storage (logins "
    "persist). Flow: create-agent (once) -> open-session (agent id) -> "
    "allocate-tab -> drive the tab over CDP with the returned cdpUrl (or "
    "Playwright connectOverCDP(cdpEndpoint) and pick the page by targetId) "
    "-> release-tab / close-session when done.";

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
               .Set("name", "agent-chrome")
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
  return base::DictValue().Set("tools", std::move(tools));
}

}  // namespace

AgentChromeService::AgentChromeService(Profile* profile)
    : profiles_(std::make_unique<ProfileRegistry>(profile)),
      sessions_(std::make_unique<SessionManager>(profile, profiles_.get())) {}

AgentChromeService::~AgentChromeService() = default;

// static
AgentChromeService* AgentChromeService::Get(Profile* profile) {
  return AgentChromeServiceFactory::GetForProfile(profile);
}

void AgentChromeService::HandleMcpRequest(const std::string& host,
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
  const Tool* tool = nullptr;
  if (name) {
    for (const Tool& candidate : GetTools()) {
      if (candidate.name == *name) {
        tool = &candidate;
      }
    }
  }
  if (!tool) {
    std::move(callback).Run(
        kHttpOk, kJson,
        ErrorMessage(*id, kInvalidParams,
                     base::StrCat({"Unknown tool: ", name ? *name : ""})));
    return;
  }
  const base::DictValue* arguments = params->FindDict("arguments");
  base::DictValue empty_arguments;
  ToolContext context{this, host};
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

void AgentChromeService::Shutdown() {
  // Cookies set shortly before exit are otherwise lost (seen in the
  // partition spike with Browser.close).
  profiles_->FlushAllCookies();
}

}  // namespace agent_chrome
