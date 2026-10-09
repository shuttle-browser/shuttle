// Copyright 2026 Silkweave
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/shuttle/page_tools.h"

#include <algorithm>
#include <optional>
#include <set>
#include <string_view>
#include <utility>

#include "base/command_line.h"
#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "build/build_config.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/shuttle/agent_browser_sidecar.h"
#include "chrome/browser/shuttle/session_manager.h"
#include "content/public/browser/web_contents.h"

namespace shuttle {

namespace {

// Compiled from config/agent-browser-tools.json (scripts/gen-page-tools.mjs).
// A C++ raw string rather than a .grd resource: no resource plumbing, and the
// list is parsed once at first use.
constexpr char kPageToolsJson[] =
#include "chrome/browser/shuttle/page_tools_data.inc"
    ;

constexpr char kBinaryEnv[] = "SHUTTLE_AGENT_BROWSER";
constexpr base::TimeDelta kDefaultTimeout = base::Seconds(60);
constexpr base::TimeDelta kTimeoutSlack = base::Seconds(15);

const base::ListValue& Tools() {
  static const base::NoDestructor<base::ListValue> tools([] {
    std::optional<base::ListValue> parsed =
        base::JSONReader::ReadList(kPageToolsJson, base::JSON_PARSE_RFC);
    CHECK(parsed) << "Bad page_tools_data.inc";
    return std::move(*parsed);
  }());
  return *tools;
}

const base::DictValue* FindTool(std::string_view name) {
  for (const base::Value& value : Tools()) {
    const base::DictValue& tool = value.GetDict();
    const std::string* tool_name = tool.FindString("name");
    if (tool_name && *tool_name == name) {
      return &tool;
    }
  }
  return nullptr;
}

bool IsSafeName(const std::string& name) {
  if (name.empty() || name.size() > 64) {
    return false;
  }
  return std::ranges::all_of(name, [](char c) {
    return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-';
  });
}

struct Translated {
  std::string ab_tool;
  base::DictValue args;
  base::TimeDelta timeout = kDefaultTimeout;
};

// Maps our arguments to agent-browser's. Returns an error message or "".
std::string Translate(const base::DictValue& tool,
                      const base::DictValue& args,
                      const base::FilePath& states_dir,
                      Translated* out) {
  const base::DictValue* ab = tool.FindDict("ab");
  const base::DictValue* properties =
      tool.FindDict("inputSchema")->FindDict("properties");

  const base::DictValue* variant_map = nullptr;
  std::vector<std::string> variant_keys;
  std::string chosen_key;
  if (const std::string* name = ab->FindString("tool")) {
    out->ab_tool = *name;
  } else {
    // The first variant whose key is present wins.
    std::string choices;
    for (const base::Value& value : *ab->FindList("variants")) {
      const base::DictValue& variant = value.GetDict();
      const std::string& key = *variant.FindString("if");
      variant_keys.push_back(key);
      base::StrAppend(&choices, {choices.empty() ? "" : ", ", key});
      if (chosen_key.empty() && args.contains(key)) {
        chosen_key = key;
        out->ab_tool = *variant.FindString("ab");
        variant_map = variant.FindDict("map");
      }
    }
    if (chosen_key.empty()) {
      return "Give one of: " + choices;
    }
  }

  const base::DictValue* tool_map = ab->FindDict("map");
  for (const auto [key, value] : args) {
    if (key == "targetId") {
      continue;
    }
    if (!properties->contains(key)) {
      return "Unknown argument: " + std::string(key);
    }
    // Of the variants' selector arguments only the chosen one is sent on.
    if (key != chosen_key && std::ranges::find(variant_keys, key) !=
                                 variant_keys.end()) {
      continue;
    }
    std::string dest(key);
    if (variant_map && variant_map->FindString(key)) {
      dest = *variant_map->FindString(key);
    } else if (tool_map && tool_map->FindString(key)) {
      dest = *tool_map->FindString(key);
    }
    if (dest.empty()) {
      continue;  // Mapped to "": not applicable to this variant.
    }
    out->args.Set(dest, value.Clone());
  }

  if (const base::DictValue* fixed = ab->FindDict("fixed")) {
    for (const auto [key, value] : *fixed) {
      out->args.Set(key, value.Clone());
    }
  }

  const std::string* special = ab->FindString("special");
  if (special && *special == "state_path") {
    const std::string* name = args.FindString("name");
    if (!name || !IsSafeName(*name)) {
      return "name must be 1-64 characters of a-z 0-9 -";
    }
    out->args.Remove("name");
    out->args.Set("path", states_dir.AppendASCII(*name + ".json").AsUTF8Unsafe());
  }

  // Waits can legitimately outlast the default.
  for (const char* key : {"timeoutMs", "ms"}) {
    if (std::optional<int> ms = args.FindInt(key)) {
      out->timeout =
          std::max(out->timeout, base::Milliseconds(*ms) + kTimeoutSlack);
    }
  }
  return std::string();
}

// agent-browser wraps most results as {"data": {...}, "success": true,
// "error": null} with a large "lifecycle" block in data. Returns what the
// agent needs: compact data, "ok" if nothing is left, or `text` unchanged
// for plain-text results (snapshot, eval, get_text, urls).
// agent-browser 0.38 repeats parts of a snapshot scoped to an element with a
// role (`<div role=dialog><div>...`): the same refs, emitted again. Drops a
// line whose ref was already printed, with its (more indented) children.
std::string DedupeSnapshot(const std::string& text) {
  std::set<std::string_view> seen;
  std::string out;
  size_t skip_indent = std::string::npos;
  bool dropped = false;
  for (std::string_view line : base::SplitStringPiece(
           text, "\n", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL)) {
    size_t indent = line.find_first_not_of(' ');
    if (indent == std::string_view::npos) {
      indent = line.size();
    }
    if (skip_indent != std::string::npos) {
      if (indent > skip_indent) {
        continue;
      }
      skip_indent = std::string::npos;
    }
    size_t ref = line.find("ref=e");
    if (ref != std::string_view::npos) {
      size_t end = line.find_first_not_of("0123456789", ref + 5);
      std::string_view id = line.substr(ref, end == std::string_view::npos
                                                 ? std::string_view::npos
                                                 : end - ref);
      if (!seen.insert(id).second) {
        skip_indent = indent;
        dropped = true;
        continue;
      }
    }
    out.append(line);
    out.push_back('\n');
  }
  if (!dropped) {
    return text;
  }
  while (!out.empty() && out.back() == '\n') {
    out.pop_back();
  }
  return out;
}

std::string SlimText(const std::string& text) {
  if (base::StartsWith(text, "- ")) {
    return DedupeSnapshot(text);  // An accessibility snapshot.
  }
  if (text.empty() || text.front() != '{') {
    return text;
  }
  std::optional<base::Value> parsed =
      base::JSONReader::Read(text, base::JSON_PARSE_RFC);
  if (!parsed || !parsed->is_dict()) {
    return text;
  }
  base::DictValue& dict = parsed->GetDict();
  // The success flag tells the envelope apart from an eval'd {data: ...}.
  base::DictValue* data = dict.FindDict("data");
  if (!data || !dict.FindBool("success")) {
    return text;
  }
  data->Remove("lifecycle");
  if (data->empty()) {
    return "ok";
  }
  return base::WriteJson(*data).value_or(text);
}

ToolResult ToToolResult(base::expected<base::DictValue, std::string> response) {
  if (!response.has_value()) {
    return ToolResult::Error(std::move(response.error()));
  }
  const base::DictValue& result = response.value();
  const bool is_error = result.FindBool("isError").value_or(false);
  const base::ListValue* content = result.FindList("content");
  ToolResult out;
  out.is_error = is_error;
  if (!content) {
    out.content.Append(
        base::DictValue().Set("type", "text").Set("text", "ok"));
    return out;
  }
  bool has_image = false;
  for (const base::Value& part : *content) {
    const std::string* type = part.GetDict().FindString("type");
    has_image |= type && *type == "image";
  }
  for (const base::Value& value : *content) {
    const base::DictValue& part = value.GetDict();
    const std::string* type = part.FindString("type");
    if (type && *type == "text") {
      // A screenshot's text part is only the temp file path.
      if (has_image) {
        continue;
      }
      const std::string* text = part.FindString("text");
      out.content.Append(base::DictValue().Set(
                             "type", "text")
                             .Set("text", SlimText(text ? *text : "")));
    } else {
      out.content.Append(part.Clone());
    }
  }
  return out;
}

bool IsSafeAgentId(const std::string& id) {
  return !id.empty() && id.find_first_of("/\\") == std::string::npos &&
         id != "." && id != "..";
}

}  // namespace

PageToolManager::PageToolManager(SessionManager* sessions,
                                 base::FilePath profile_path)
    : sessions_(sessions), profile_path_(std::move(profile_path)) {}

PageToolManager::~PageToolManager() = default;

// static
base::ListValue PageToolManager::ListTools() {
  base::ListValue list;
  for (const base::Value& value : Tools()) {
    const base::DictValue& tool = value.GetDict();
    list.Append(base::DictValue()
                    .Set("name", *tool.FindString("name"))
                    .Set("description", *tool.FindString("description"))
                    .Set("inputSchema", tool.FindDict("inputSchema")->Clone()));
  }
  return list;
}

// static
bool PageToolManager::IsPageTool(std::string_view name) {
  return FindTool(name) != nullptr;
}

// static
base::FilePath PageToolManager::FindBinary() {
  std::unique_ptr<base::Environment> env = base::Environment::Create();
  if (std::optional<std::string> path = env->GetVar(kBinaryEnv);
      path && !path->empty()) {
    return base::FilePath(*path);
  }
  base::FilePath exe;
  base::PathService::Get(base::FILE_EXE, &exe);
#if BUILDFLAG(IS_MAC)
  // <app>/Contents/MacOS/Shuttle -> <app>/Contents/Helpers/agent-browser.
  // (Helper processes run from nested bundles, but the browser process
  // spawns the sidecar, so FILE_EXE is the main executable.)
  return exe.DirName().DirName().Append("Helpers").Append("agent-browser");
#else
  return exe.DirName().Append("agent-browser");
#endif
}

void PageToolManager::Call(std::string_view name,
                           const ToolContext& context,
                           const base::DictValue& args,
                           ToolCallback callback) {
  const base::DictValue* tool = FindTool(name);
  if (!tool) {
    std::move(callback).Run(ToolResult::Error("Unknown tool"));
    return;
  }
  const std::string* target_id = args.FindString("targetId");
  if (!target_id || target_id->empty()) {
    std::move(callback).Run(ToolResult::Error("Missing argument: targetId"));
    return;
  }
  content::WebContents* contents = SessionManager::FindTab(*target_id);
  std::string session_id =
      contents ? SessionManager::SessionOf(contents) : std::string();
  const Session* session = session_id.empty() ? nullptr : sessions_->Get(session_id);
  if (!session || !IsSafeAgentId(session->agent)) {
    std::move(callback).Run(ToolResult::Error(
        "Not a leased tab of an open session: " + *target_id));
    return;
  }

  base::FilePath states_dir =
      profile_path_.Append("shuttle").Append("states").Append(session->agent);
  Translated translated;
  std::string error = Translate(*tool, args, states_dir, &translated);
  if (!error.empty()) {
    std::move(callback).Run(ToolResult::Error(std::move(error)));
    return;
  }
  GetOrCreateSidecar(session_id, context.host, states_dir)
      ->CallTool(*target_id, std::move(translated.ab_tool),
                 std::move(translated.args), translated.timeout,
                 base::BindOnce(
                     [](ToolCallback callback,
                        base::expected<base::DictValue, std::string> result) {
                       std::move(callback).Run(ToToolResult(std::move(result)));
                     },
                     std::move(callback)));
}

Sidecar* PageToolManager::GetOrCreateSidecar(const std::string& session_id,
                                             const std::string& host,
                                             const base::FilePath& states_dir) {
  auto it = sidecars_.find(session_id);
  if (it != sidecars_.end()) {
    return it->second.get();
  }
  base::FilePath temp;
  if (!base::GetTempDir(&temp)) {
    temp = base::FilePath("/tmp");
  }
  // Short: daemon sockets live here (unix socket paths max ~100 chars).
  base::FilePath dir = temp.Append("acab").AppendASCII(session_id);
  Sidecar::Config config;
  config.binary = FindBinary();
  config.socket_dir = dir;
  config.screenshot_dir = dir.Append("shots");
  config.extra_dirs = {states_dir};
  config.cdp_endpoint =
      base::StrCat({"ws://", host, "/devtools/session/", session_id});
  config.session_id = session_id;
  return sidecars_.emplace(session_id, std::make_unique<Sidecar>(config))
      .first->second.get();
}

void PageToolManager::OnSessionClosed(const std::string& session_id) {
  auto it = sidecars_.find(session_id);
  if (it == sidecars_.end()) {
    return;
  }
  Sidecar* sidecar = it->second.get();
  retiring_.push_back(std::move(it->second));
  sidecars_.erase(it);
  sidecar->Close(base::BindOnce(&PageToolManager::OnSidecarClosed,
                                weak_factory_.GetWeakPtr(), sidecar));
}

void PageToolManager::OnSidecarClosed(Sidecar* sidecar) {
  // Called from inside the sidecar: delete it after the call returns.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](base::WeakPtr<PageToolManager> self, Sidecar* sidecar) {
            if (!self) {
              return;
            }
            std::erase_if(self->retiring_,
                          [sidecar](const std::unique_ptr<Sidecar>& s) {
                            return s.get() == sidecar;
                          });
          },
          weak_factory_.GetWeakPtr(), sidecar));
}

void PageToolManager::Shutdown() {
  for (auto& [id, sidecar] : sidecars_) {
    sidecar->Kill();
  }
  for (auto& sidecar : retiring_) {
    sidecar->Kill();
  }
  sidecars_.clear();
  retiring_.clear();
}

}  // namespace shuttle
