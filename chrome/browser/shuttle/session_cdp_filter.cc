// Copyright 2026 Silkweave
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/shuttle/session_cdp_filter.h"

#include <optional>
#include <string_view>
#include <utility>

#include "base/barrier_closure.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/location.h"
#include "base/strings/string_util.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/shuttle/session_manager.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/web_contents.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_inclusion_status.h"
#include "net/cookies/cookie_options.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "url/gurl.h"

namespace shuttle {

namespace {

constexpr int kServerError = -32000;

// Responses start with their id ({"id":N,...}); events with their method.
std::optional<int> LeadingId(std::string_view message) {
  constexpr std::string_view kPrefix = R"({"id":)";
  if (!base::StartsWith(message, kPrefix)) {
    return std::nullopt;
  }
  int64_t value = 0;
  size_t i = kPrefix.size();
  for (; i < message.size() && base::IsAsciiDigit(message[i]); ++i) {
    value = value * 10 + (message[i] - '0');
    if (value > INT32_MAX) {
      return std::nullopt;
    }
  }
  if (i == kPrefix.size()) {
    return std::nullopt;
  }
  return static_cast<int>(value);
}

std::string StringParam(const base::DictValue* params, std::string_view key) {
  const std::string* value = params ? params->FindString(key) : nullptr;
  return value ? *value : std::string();
}

std::string_view SameSiteName(net::CookieSameSite same_site) {
  switch (same_site) {
    case net::CookieSameSite::STRICT_MODE:
      return "Strict";
    case net::CookieSameSite::LAX_MODE:
      return "Lax";
    case net::CookieSameSite::NO_RESTRICTION:
      return "None";
    default:
      return "";
  }
}

net::CookieSameSite ParseSameSite(const std::string& name) {
  if (name == "Strict") {
    return net::CookieSameSite::STRICT_MODE;
  }
  if (name == "Lax") {
    return net::CookieSameSite::LAX_MODE;
  }
  if (name == "None") {
    return net::CookieSameSite::NO_RESTRICTION;
  }
  return net::CookieSameSite::UNSPECIFIED;
}

std::string_view PriorityName(net::CookiePriority priority) {
  switch (priority) {
    case net::COOKIE_PRIORITY_LOW:
      return "Low";
    case net::COOKIE_PRIORITY_HIGH:
      return "High";
    default:
      return "Medium";
  }
}

// A cookie in CDP's Network.Cookie shape.
base::DictValue CookieToProtocol(const net::CanonicalCookie& cookie) {
  base::DictValue dict;
  dict.Set("name", cookie.Name());
  dict.Set("value", cookie.Value());
  dict.Set("domain", cookie.Domain());
  dict.Set("path", cookie.Path());
  dict.Set("expires", cookie.IsPersistent()
                          ? cookie.ExpiryDate().InSecondsFSinceUnixEpoch()
                          : -1.0);
  dict.Set("size",
           static_cast<int>(cookie.Name().size() + cookie.Value().size()));
  dict.Set("httpOnly", cookie.IsHttpOnly());
  dict.Set("secure", cookie.SecureAttribute());
  dict.Set("session", !cookie.IsPersistent());
  std::string_view same_site = SameSiteName(cookie.SameSite());
  if (!same_site.empty()) {
    dict.Set("sameSite", same_site);
  }
  dict.Set("priority", PriorityName(cookie.Priority()));
  dict.Set("sameParty", false);
  dict.Set("sourcePort", cookie.SourcePort());
  return dict;
}

// A cookie from CDP's Network.CookieParam shape, or nullptr if invalid.
std::unique_ptr<net::CanonicalCookie> CookieFromProtocol(
    const base::DictValue& param,
    GURL* source_url) {
  std::string name = StringParam(&param, "name");
  std::string value = StringParam(&param, "value");
  std::string domain = StringParam(&param, "domain");
  std::string path = StringParam(&param, "path");
  bool secure = param.FindBool("secure").value_or(false);
  GURL url(StringParam(&param, "url"));
  if (!url.is_valid()) {
    std::string host(base::TrimString(domain, ".", base::TRIM_LEADING));
    if (host.empty()) {
      return nullptr;
    }
    url = GURL((secure ? "https://" : "http://") + host +
               (path.empty() ? "/" : path));
  }
  if (path.empty()) {
    path = "/";
  }
  base::Time expiration;
  if (std::optional<double> expires = param.FindDouble("expires");
      expires && *expires > 0) {
    expiration = base::Time::FromSecondsSinceUnixEpoch(*expires);
  }
  base::Time now = base::Time::Now();
  net::CookieInclusionStatus status;
  *source_url = url;
  return net::CanonicalCookie::CreateSanitizedCookie(
      url, name, value, domain, path, now, expiration, now, secure,
      param.FindBool("httpOnly").value_or(false),
      ParseSameSite(StringParam(&param, "sameSite")),
      net::COOKIE_PRIORITY_DEFAULT, std::nullopt, &status);
}

}  // namespace

SessionCdpFilter::SessionCdpFilter(base::WeakPtr<SessionManager> manager,
                                   std::string session_id,
                                   bool page_connection)
    : manager_(std::move(manager)),
      session_id_(std::move(session_id)),
      page_connection_(page_connection) {}

SessionCdpFilter::~SessionCdpFilter() = default;

void SessionCdpFilter::Start(SendCallback to_agent, SendCallback to_client) {
  to_agent_ = std::move(to_agent);
  to_client_ = std::move(to_client);
}

bool SessionCdpFilter::Owns(const std::string& target_id,
                            const std::string& opener_id) {
  if (target_id.empty()) {
    return false;
  }
  if (visible_.contains(target_id) || allocating_ ||
      (!opener_id.empty() && visible_.contains(opener_id))) {
    return true;
  }
  scoped_refptr<content::DevToolsAgentHost> host =
      content::DevToolsAgentHost::GetForId(target_id);
  content::WebContents* contents = host ? host->GetWebContents() : nullptr;
  return contents && SessionManager::SessionOf(contents) == session_id_;
}

void SessionCdpFilter::OnClientMessage(std::string message) {
  std::optional<base::DictValue> dict =
      base::JSONReader::ReadDict(message, base::JSON_PARSE_RFC);
  const std::string* method = dict ? dict->FindString("method") : nullptr;
  const base::Value* id = dict ? dict->Find("id") : nullptr;
  if (!method || !id) {
    // Malformed: the agent host answers with the protocol error.
    to_agent_.Run(std::move(message));
    return;
  }
  const std::string* session = dict->FindString("sessionId");
  const base::DictValue* params = dict->FindDict("params");
  std::string_view name = *method;

  if (name == "Target.getTargets") {
    if (id->is_int()) {
      filter_responses_.insert(id->GetInt());
    }
  } else if (name == "Target.attachToTarget" ||
             name == "Target.activateTarget" ||
             name == "Target.closeTarget" ||
             name == "Target.getTargetInfo" ||
             name == "Target.exposeDevToolsProtocol" ||
             name == "Target.autoAttachRelated" ||
             name == "Target.sendMessageToTarget" ||
             name == "Target.detachFromTarget") {
    std::string target_id = StringParam(params, "targetId");
    if (!target_id.empty() && !Owns(target_id, "")) {
      SendError(*id, session,
                "Shuttle: target " + target_id +
                    " is not a tab of this agent session");
      return;
    }
  } else if (name == "Target.attachToBrowserTarget") {
    SendError(*id, session,
              "Shuttle: an agent session can't attach to the browser");
    return;
  } else if (name == "Target.createTarget") {
    CreateTarget(*id, session, params);
    return;
  } else if (name == "Target.createBrowserContext" ||
             name == "Target.disposeBrowserContext") {
    SendError(*id, session,
              "Shuttle: an agent session has one browser context, its "
              "agent's storage (create agents over MCP instead)");
    return;
  } else if (name == "Browser.close") {
    CloseSession(*id, session);
    return;
  } else if (name == "Browser.crash" || name == "Browser.crashGpuProcess") {
    SendError(*id, session, "Shuttle: not allowed for agent sessions");
    return;
  } else if (!page_connection_ && !session &&
             base::StartsWith(name, "Storage.")) {
    // Browser-level storage is the default profile's; answer with the
    // agent's instead.
    if (name == "Storage.getCookies") {
      GetCookies(*id, session);
      return;
    }
    if (name == "Storage.setCookies") {
      SetCookies(*id, session, params);
      return;
    }
    if (name == "Storage.clearCookies") {
      ClearCookies(*id, session);
      return;
    }
  }
  to_agent_.Run(std::move(message));
}

void SessionCdpFilter::OnAgentMessage(std::string message) {
  std::string_view view(message);
  auto from_foreign_session = [&] {
    if (foreign_sessions_.empty()) {
      return false;
    }
    // Messages of child sessions end with "sessionId":"<id>"}.
    std::string_view tail =
        view.substr(view.size() > 80 ? view.size() - 80 : 0);
    for (const std::string& foreign : foreign_sessions_) {
      if (tail.find(foreign) != std::string_view::npos) {
        return true;
      }
    }
    return false;
  };

  if (std::optional<int> id = LeadingId(view)) {
    if (internal_ids_.erase(*id)) {
      return;
    }
    if (filter_responses_.erase(*id)) {
      if (std::optional<base::DictValue> dict =
              base::JSONReader::ReadDict(message, base::JSON_PARSE_RFC)) {
        FilterTargetInfos(std::move(*dict));
        return;
      }
    }
    if (!from_foreign_session()) {
      to_client_.Run(std::move(message));
    }
    return;
  }
  if (base::StartsWith(view, R"({"method":"Target.)")) {
    std::optional<base::DictValue> dict =
        base::JSONReader::ReadDict(message, base::JSON_PARSE_RFC);
    const std::string* method = dict ? dict->FindString("method") : nullptr;
    if (method) {
      std::string name = *method;
      HandleTargetEvent(name, std::move(*dict), std::move(message));
      return;
    }
  }
  if (!from_foreign_session()) {
    to_client_.Run(std::move(message));
  }
}

void SessionCdpFilter::HandleTargetEvent(const std::string& method,
                                         base::DictValue message,
                                         std::string raw) {
  const std::string* parent = message.FindString("sessionId");
  if (parent && foreign_sessions_.contains(*parent)) {
    return;
  }
  base::DictValue* params = message.FindDict("params");
  if (!params) {
    to_client_.Run(std::move(raw));
    return;
  }
  const base::DictValue* info = params->FindDict("targetInfo");
  std::string target_id = StringParam(info, "targetId");
  std::string opener_id = StringParam(info, "openerId");

  if (method == "Target.targetCreated") {
    if (Owns(target_id, opener_id)) {
      visible_.insert(target_id);
      to_client_.Run(std::move(raw));
    }
    return;
  }
  if (method == "Target.targetInfoChanged") {
    if (visible_.contains(target_id)) {
      to_client_.Run(std::move(raw));
    } else if (Owns(target_id, opener_id)) {
      // Leased after it was created (e.g. a popup): announce it now.
      visible_.insert(target_id);
      base::DictValue created;
      created.Set("method", "Target.targetCreated");
      created.Set("params", params->Clone());
      if (parent) {
        created.Set("sessionId", *parent);
      }
      to_client_.Run(base::WriteJson(created).value_or("{}"));
    }
    return;
  }
  if (method == "Target.targetDestroyed" || method == "Target.targetCrashed") {
    std::string id = StringParam(params, "targetId");
    if (visible_.contains(id)) {
      if (method == "Target.targetDestroyed") {
        visible_.erase(id);
      }
      to_client_.Run(std::move(raw));
    }
    return;
  }
  if (method == "Target.attachedToTarget") {
    std::string child = StringParam(params, "sessionId");
    // Nested attachments (frames, workers) belong to their parent target; on
    // page connections so do the root's (the root is the leased page).
    bool own = parent || page_connection_ || Owns(target_id, opener_id);
    if (own) {
      if (!parent && !page_connection_) {
        visible_.insert(target_id);
      }
      to_client_.Run(std::move(raw));
      return;
    }
    if (child.empty()) {
      return;
    }
    foreign_sessions_.insert(child);
    if (params->FindBool("waitingForDebugger").value_or(false)) {
      SendInternal("Runtime.runIfWaitingForDebugger", base::DictValue(),
                   &child);
    }
    SendInternal("Target.detachFromTarget",
                 base::DictValue().Set("sessionId", child), parent);
    return;
  }
  if (method == "Target.detachedFromTarget" ||
      method == "Target.receivedMessageFromTarget") {
    std::string child = StringParam(params, "sessionId");
    if (foreign_sessions_.contains(child)) {
      if (method == "Target.detachedFromTarget") {
        foreign_sessions_.erase(child);
      }
      return;
    }
  }
  to_client_.Run(std::move(raw));
}

void SessionCdpFilter::FilterTargetInfos(base::DictValue message) {
  if (base::DictValue* result = message.FindDict("result")) {
    if (base::ListValue* infos = result->FindList("targetInfos")) {
      infos->EraseIf([this](const base::Value& value) {
        const base::DictValue* info = value.GetIfDict();
        return !info || !Owns(StringParam(info, "targetId"),
                              StringParam(info, "openerId"));
      });
    }
  }
  to_client_.Run(base::WriteJson(message).value_or("{}"));
}

void SessionCdpFilter::CreateTarget(const base::Value& id,
                                    const std::string* session,
                                    const base::DictValue* params) {
  if (!manager_) {
    SendError(id, session, "Shuttle is shutting down");
    return;
  }
  allocating_ = true;
  base::expected<content::WebContents*, std::string> tab =
      manager_->AllocateTab(session_id_, GURL(StringParam(params, "url")),
                            std::nullopt);
  allocating_ = false;
  if (!tab.has_value()) {
    SendError(id, session, tab.error());
    return;
  }
  std::string target_id = SessionManager::TargetIdOf(*tab);
  visible_.insert(target_id);
  SendResult(id, session, base::DictValue().Set("targetId", target_id));
}

void SessionCdpFilter::CloseSession(const base::Value& id,
                                    const std::string* session) {
  SendResult(id, session, base::DictValue());
  // Closing the session's tabs detaches this connection's child sessions;
  // answer first.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(
                     [](base::WeakPtr<SessionManager> manager,
                        std::string session_id) {
                       if (manager) {
                         manager->Close(session_id);
                       }
                     },
                     manager_, session_id_));
}

void SessionCdpFilter::GetCookies(const base::Value& id,
                                  const std::string* session) {
  network::mojom::CookieManager* cookies =
      manager_ ? manager_->CookieManagerOf(session_id_) : nullptr;
  if (!cookies) {
    SendError(id, session, "Shuttle: no storage for this session");
    return;
  }
  std::optional<std::string> session_copy =
      session ? std::optional<std::string>(*session) : std::nullopt;
  cookies->GetAllCookies(base::BindOnce(
      [](base::WeakPtr<SessionCdpFilter> self, base::Value id,
         std::optional<std::string> session,
         const std::vector<net::CanonicalCookie>& list) {
        if (!self) {
          return;
        }
        base::ListValue result;
        for (const net::CanonicalCookie& cookie : list) {
          result.Append(CookieToProtocol(cookie));
        }
        self->SendResult(id, session ? &*session : nullptr,
                         base::DictValue().Set("cookies", std::move(result)));
      },
      weak_factory_.GetWeakPtr(), id.Clone(), std::move(session_copy)));
}

void SessionCdpFilter::SetCookies(const base::Value& id,
                                  const std::string* session,
                                  const base::DictValue* params) {
  network::mojom::CookieManager* cookies =
      manager_ ? manager_->CookieManagerOf(session_id_) : nullptr;
  const base::ListValue* list = params ? params->FindList("cookies") : nullptr;
  if (!cookies || !list) {
    SendError(id, session, cookies ? "Missing cookies" : "No storage");
    return;
  }
  std::vector<std::pair<std::unique_ptr<net::CanonicalCookie>, GURL>> parsed;
  for (const base::Value& value : *list) {
    const base::DictValue* param = value.GetIfDict();
    GURL url;
    std::unique_ptr<net::CanonicalCookie> cookie =
        param ? CookieFromProtocol(*param, &url) : nullptr;
    if (!cookie) {
      SendError(id, session,
                "Invalid cookie: " + StringParam(param, "name"));
      return;
    }
    parsed.emplace_back(std::move(cookie), url);
  }
  std::optional<std::string> session_copy =
      session ? std::optional<std::string>(*session) : std::nullopt;
  base::RepeatingClosure done = base::BarrierClosure(
      parsed.size(),
      base::BindOnce(
          [](base::WeakPtr<SessionCdpFilter> self, base::Value id,
             std::optional<std::string> session) {
            if (self) {
              self->SendResult(id, session ? &*session : nullptr,
                               base::DictValue());
            }
          },
          weak_factory_.GetWeakPtr(), id.Clone(), std::move(session_copy)));
  for (auto& [cookie, url] : parsed) {
    cookies->SetCanonicalCookie(
        *cookie, url, net::CookieOptions::MakeAllInclusive(),
        base::BindOnce([](base::RepeatingClosure done,
                          net::CookieAccessResult) { done.Run(); },
                       done));
  }
}

void SessionCdpFilter::ClearCookies(const base::Value& id,
                                    const std::string* session) {
  network::mojom::CookieManager* cookies =
      manager_ ? manager_->CookieManagerOf(session_id_) : nullptr;
  if (!cookies) {
    SendError(id, session, "Shuttle: no storage for this session");
    return;
  }
  std::optional<std::string> session_copy =
      session ? std::optional<std::string>(*session) : std::nullopt;
  cookies->DeleteCookies(
      network::mojom::CookieDeletionFilter::New(),
      base::BindOnce(
          [](base::WeakPtr<SessionCdpFilter> self, base::Value id,
             std::optional<std::string> session, uint32_t) {
            if (self) {
              self->SendResult(id, session ? &*session : nullptr,
                               base::DictValue());
            }
          },
          weak_factory_.GetWeakPtr(), id.Clone(), std::move(session_copy)));
}

void SessionCdpFilter::SendResult(const base::Value& id,
                                  const std::string* session,
                                  base::DictValue result) {
  base::DictValue message;
  message.Set("id", id.Clone());
  message.Set("result", std::move(result));
  if (session) {
    message.Set("sessionId", *session);
  }
  to_client_.Run(base::WriteJson(message).value_or("{}"));
}

void SessionCdpFilter::SendError(const base::Value& id,
                                 const std::string* session,
                                 const std::string& error) {
  base::DictValue message;
  message.Set("id", id.Clone());
  message.Set("error", base::DictValue()
                           .Set("code", kServerError)
                           .Set("message", error));
  if (session) {
    message.Set("sessionId", *session);
  }
  to_client_.Run(base::WriteJson(message).value_or("{}"));
}

void SessionCdpFilter::SendInternal(const std::string& method,
                                    base::DictValue params,
                                    const std::string* session) {
  int id = next_internal_id_++;
  internal_ids_.insert(id);
  base::DictValue message;
  message.Set("id", id);
  message.Set("method", method);
  message.Set("params", std::move(params));
  if (session) {
    message.Set("sessionId", *session);
  }
  // Not re-entrantly: this runs while the agent host dispatches an event
  // (e.g. inside auto-attach), which must finish first.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](base::WeakPtr<SessionCdpFilter> self, std::string json) {
            if (self) {
              self->to_agent_.Run(std::move(json));
            }
          },
          weak_factory_.GetWeakPtr(),
          base::WriteJson(message).value_or("{}")));
}

}  // namespace shuttle
