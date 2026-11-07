// Copyright 2026 Silkweave
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/shuttle/page_tools.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <optional>
#include <set>
#include <string_view>
#include <utility>

#include "base/command_line.h"
#include "base/containers/span.h"
#include "base/environment.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/posix/eintr_wrapper.h"
#include "base/rand_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/shuttle/agent_browser_sidecar.h"
#include "chrome/browser/shuttle/network_access.h"
#include "chrome/browser/shuttle/profile_registry.h"
#include "chrome/browser/shuttle/session_manager.h"
#include "content/public/browser/web_contents.h"
#include "crypto/sha2.h"
#include "third_party/re2/src/re2/re2.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"

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

re2::RE2::Options PatternOptions() {
  re2::RE2::Options options;
  options.set_case_sensitive(false);
  options.set_log_errors(false);
  return options;
}

// Tools that retry once after scrolling the element to the viewport centre
// when another element covers it (sticky headers/trays near the edges).
bool RetriesWhenCovered(std::string_view tool) {
  return tool == "click" || tool == "dblclick" || tool == "hover";
}

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
    if (key != chosen_key &&
        std::ranges::find(variant_keys, key) != variant_keys.end()) {
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
    out->args.Set("path",
                  states_dir.AppendASCII(*name + ".json").AsUTF8Unsafe());
  }
  if (special && *special == "save_text") {
    const std::string* to = args.FindString("to");
    if (!to || *to == "file") {
      const std::string* name = args.FindString("name");
      if (!name || !IsSafeName(*name)) {
        return "to=file needs a name of 1-64 characters of a-z 0-9 -";
      }
    } else if (*to != "clipboard") {
      return "to must be file or clipboard";
    }
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

// Keeps the lines of an accessibility snapshot that match `pattern`, with
// their ancestors (for context) and their children (a table row's cells and
// buttons, say). Lines nest by indentation.
std::string FilterSnapshot(const std::string& text, const re2::RE2& pattern) {
  std::vector<std::string_view> lines = base::SplitStringPiece(
      text, "\n", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  std::vector<size_t> indent(lines.size());
  for (size_t i = 0; i < lines.size(); ++i) {
    size_t first = lines[i].find_first_not_of(' ');
    indent[i] = first == std::string_view::npos ? lines[i].size() : first;
  }
  std::vector<bool> keep(lines.size(), false);
  bool any = false;
  for (size_t i = 0; i < lines.size(); ++i) {
    if (!re2::RE2::PartialMatch(lines[i], pattern)) {
      continue;
    }
    any = true;
    keep[i] = true;
    size_t level = indent[i];
    for (size_t j = i; j-- > 0 && level > 0;) {
      if (indent[j] < level) {
        keep[j] = true;
        level = indent[j];
      }
    }
    for (size_t j = i + 1; j < lines.size() && indent[j] > indent[i]; ++j) {
      keep[j] = true;
    }
  }
  if (!any) {
    return "No snapshot lines match the filter.";
  }
  std::string out;
  for (size_t i = 0; i < lines.size(); ++i) {
    if (keep[i]) {
      base::StrAppend(&out, {lines[i], "\n"});
    }
  }
  out.pop_back();
  return out;
}

// The first text part of a raw agent-browser result.
std::string RawText(const base::DictValue& result) {
  if (const base::ListValue* content = result.FindList("content")) {
    for (const base::Value& part : *content) {
      const std::string* type = part.GetDict().FindString("type");
      const std::string* text = part.GetDict().FindString("text");
      if (type && *type == "text" && text) {
        return *text;
      }
    }
  }
  return std::string();
}

struct SavedText {
  std::string error;
  std::string path;
};

// Opens directory `name` in `parent_fd` (creating it with mode 0700 if missing)
// without following a link, and makes sure it is a directory owned by this
// user with mode 0700. Returns an invalid fd on any failure.
base::ScopedFD OpenPrivateDir(int parent_fd, const std::string& name) {
  if (mkdirat(parent_fd, name.c_str(), 0700) != 0 && errno != EEXIST) {
    return base::ScopedFD();
  }
  base::ScopedFD fd(HANDLE_EINTR(openat(
      parent_fd, name.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC)));
  if (!fd.is_valid()) {
    return base::ScopedFD();
  }
  struct stat st;
  if (fstat(fd.get(), &st) != 0 || !S_ISDIR(st.st_mode) ||
      st.st_uid != geteuid()) {
    return base::ScopedFD();
  }
  if ((st.st_mode & 07777) != 0700 && fchmod(fd.get(), 0700) != 0) {
    return base::ScopedFD();
  }
  return fd;
}

// Makes sure `states_dir` (<profile>/shuttle/states/<agent>) exists: the three
// components below the profile directory are opened (created if missing)
// without following links and must be private directories of this user, as
// for secret files. State files hold cookies and are written there by
// agent-browser. Returns false on any failure.
bool EnsureStatesDir(base::FilePath states_dir) {
  const base::FilePath shuttle_dir = states_dir.DirName().DirName();
  base::ScopedFD profile_fd(HANDLE_EINTR(
      open(shuttle_dir.DirName().value().c_str(),
           O_RDONLY | O_DIRECTORY | O_CLOEXEC)));
  if (!profile_fd.is_valid()) {
    return false;
  }
  base::ScopedFD base_fd =
      OpenPrivateDir(profile_fd.get(), shuttle_dir.BaseName().value());
  if (!base_fd.is_valid()) {
    return false;
  }
  base::ScopedFD parent_fd = OpenPrivateDir(base_fd.get(), "states");
  if (!parent_fd.is_valid()) {
    return false;
  }
  return OpenPrivateDir(parent_fd.get(), states_dir.BaseName().value())
      .is_valid();
}

// Writes `value` to `secrets_dir`/`name`.txt, readable only by the user.
// `secrets_dir` is <profile>/shuttle/secrets/<agent>: the three components
// below the profile directory are opened without following links (and must be
// private directories of this user), the value goes into a new exclusively
// created 0600 file that then replaces the destination entry atomically. An
// existing file (or link) at the destination is replaced, never written
// through.
SavedText WriteSecretFile(base::FilePath secrets_dir,
                          std::string name,
                          std::string value) {
  SavedText out;
  const base::FilePath agent_dir = secrets_dir.DirName().DirName();
  const std::string fail = "Could not write the secret file under " +
                           secrets_dir.AsUTF8Unsafe() +
                           " (it must be a private directory of this user)";
  // The profile directory is the trusted anchor; every component below it
  // (shuttle/secrets/<agent>) is opened without following links and must be a
  // private directory of this user.
  base::ScopedFD profile_fd(HANDLE_EINTR(
      open(agent_dir.DirName().value().c_str(),
           O_RDONLY | O_DIRECTORY | O_CLOEXEC)));
  if (!profile_fd.is_valid()) {
    out.error = fail;
    return out;
  }
  base::ScopedFD base_fd =
      OpenPrivateDir(profile_fd.get(), agent_dir.BaseName().value());
  if (!base_fd.is_valid()) {
    out.error = fail;
    return out;
  }
  base::ScopedFD secrets_fd = OpenPrivateDir(base_fd.get(), "secrets");
  if (!secrets_fd.is_valid()) {
    out.error = fail;
    return out;
  }
  base::ScopedFD dir_fd =
      OpenPrivateDir(secrets_fd.get(), secrets_dir.BaseName().value());
  if (!dir_fd.is_valid()) {
    out.error = fail;
    return out;
  }
  const std::string final_name = name + ".txt";
  const std::string temp_name =
      "." + name + ".tmp-" + base::HexEncodeLower(base::RandBytesAsVector(8));
  base::ScopedFD file(HANDLE_EINTR(
      openat(dir_fd.get(), temp_name.c_str(),
             O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600)));
  if (!file.is_valid()) {
    out.error = fail;
    return out;
  }
  struct stat st;
  const bool ok =
      fchmod(file.get(), 0600) == 0 && fstat(file.get(), &st) == 0 &&
      S_ISREG(st.st_mode) && st.st_uid == geteuid() &&
      (st.st_mode & 07777) == 0600 &&
      base::WriteFileDescriptor(file.get(), value) &&
      HANDLE_EINTR(fsync(file.get())) == 0;
  file.reset();
  if (!ok || renameat(dir_fd.get(), temp_name.c_str(), dir_fd.get(),
                      final_name.c_str()) != 0) {
    unlinkat(dir_fd.get(), temp_name.c_str(), 0);
    out.error = fail;
    return out;
  }
  out.path = secrets_dir.AppendASCII(final_name).AsUTF8Unsafe();
  return out;
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
    out.content.Append(base::DictValue().Set("type", "text").Set("text", "ok"));
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
      out.content.Append(base::DictValue()
                             .Set("type", "text")
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

struct PageToolManager::PendingCall {
  std::string tool;
  std::string session_id;
  std::string target_id;
  std::string ab_tool;
  base::DictValue ab_args;
  base::TimeDelta timeout;
  bool retried = false;
  // snapshot: regex the lines must match.
  std::string filter;
  // save-text.
  bool save_text = false;
  std::string save_to;
  std::string save_name;
  std::string save_regex;
  base::FilePath secrets_dir;
};

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
  const Session* session =
      session_id.empty() ? nullptr : sessions_->Get(session_id);
  if (!session || !IsSafeAgentId(session->agent)) {
    std::move(callback).Run(ToolResult::Error(
        "Not a leased tab of an open session: " + *target_id));
    return;
  }

  if (const std::string* ab_special =
          tool->FindDict("ab")->FindString("special");
      ab_special && *ab_special == "state_path" &&
      ProfileRegistry::IsThrowawayId(session->agent)) {
    // A saved state is cookies on disk; throwaway agents keep nothing there.
    std::move(callback).Run(ToolResult::Error(
        "state-save / state-load aren't available to throwaway agents"));
    return;
  }

  base::FilePath agent_dir = profile_path_.Append("shuttle");
  base::FilePath states_dir = agent_dir.Append("states").Append(session->agent);
  Translated translated;
  std::string error = Translate(*tool, args, states_dir, &translated);
  if (!error.empty()) {
    std::move(callback).Run(ToolResult::Error(std::move(error)));
    return;
  }

  PendingCall call;
  call.tool = std::string(name);
  call.session_id = session_id;
  call.target_id = *target_id;
  call.ab_tool = std::move(translated.ab_tool);
  call.ab_args = std::move(translated.args);
  call.timeout = translated.timeout;
  if (const std::string* filter = args.FindString("filter");
      filter && !filter->empty()) {
    re2::RE2 pattern(*filter, PatternOptions());
    if (!pattern.ok()) {
      std::move(callback).Run(
          ToolResult::Error("Bad filter regex: " + pattern.error()));
      return;
    }
    call.filter = *filter;
    // agent-browser snapshots default to interactive elements only; a filter
    // searches page text (e.g. "API key"), so default to the full tree.
    if (!args.Find("interactive")) {
      call.ab_args.Set("interactive", false);
    }
  }
  const std::string* special = tool->FindDict("ab")->FindString("special");
  if (special && *special == "save_text") {
    call.save_text = true;
    const std::string* to = args.FindString("to");
    call.save_to = to ? *to : "file";
    if (const std::string* save_name = args.FindString("name")) {
      call.save_name = *save_name;
    }
    if (const std::string* regex = args.FindString("regex");
        regex && !regex->empty()) {
      re2::RE2 pattern(*regex, re2::RE2::Options(re2::RE2::Quiet));
      if (!pattern.ok()) {
        std::move(callback).Run(
            ToolResult::Error("Bad regex: " + pattern.error()));
        return;
      }
      call.save_regex = *regex;
    }
    call.secrets_dir = agent_dir.Append("secrets").Append(session->agent);
  }
  GetOrCreateSidecar(session_id, context.host);
  if (special && *special == "state_path") {
    // state-save / state-load: the states directory is checked (and created)
    // on every call, never through a link.
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE,
        {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
         base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
        base::BindOnce(&EnsureStatesDir, states_dir),
        base::BindOnce(&PageToolManager::SendIfStatesDir,
                       weak_factory_.GetWeakPtr(), states_dir, std::move(call),
                       std::move(callback)));
    return;
  }
  Send(std::move(call), std::move(callback));
}

void PageToolManager::SendIfStatesDir(const base::FilePath& states_dir,
                                      PendingCall call,
                                      ToolCallback callback,
                                      bool ok) {
  if (!ok) {
    std::move(callback).Run(ToolResult::Error(
        "Could not use the states directory " + states_dir.AsUTF8Unsafe() +
        " (it must be a private directory of this user)"));
    return;
  }
  Send(std::move(call), std::move(callback));
}

void PageToolManager::Send(PendingCall call, ToolCallback callback) {
  auto it = sidecars_.find(call.session_id);
  if (it == sidecars_.end()) {
    std::move(callback).Run(ToolResult::Error("The session was closed."));
    return;
  }
  std::string target_id = call.target_id;
  std::string ab_tool = call.ab_tool;
  base::DictValue ab_args = call.ab_args.Clone();
  base::TimeDelta timeout = call.timeout;
  it->second->CallTool(
      std::move(target_id), std::move(ab_tool), std::move(ab_args), timeout,
      base::BindOnce(&PageToolManager::OnResult, weak_factory_.GetWeakPtr(),
                     std::move(call), std::move(callback)));
}

void PageToolManager::OnResult(
    PendingCall call,
    ToolCallback callback,
    base::expected<base::DictValue, std::string> result) {
  const bool tool_error =
      result.has_value() && result->FindBool("isError").value_or(false);

  // Covered by a sticky header/tray: scroll to the centre, retry once.
  if (tool_error && !call.retried && RetriesWhenCovered(call.tool) &&
      RawText(*result).find("is covered by") != std::string::npos) {
    auto it = sidecars_.find(call.session_id);
    const std::string* selector = call.ab_args.FindString("selector");
    if (it != sidecars_.end() && selector) {
      call.retried = true;
      std::string target_id = call.target_id;
      base::DictValue scroll_args =
          base::DictValue().Set("selector", *selector);
      base::TimeDelta timeout = call.timeout;
      it->second->CallTool(
          std::move(target_id), "scroll_into_view", std::move(scroll_args),
          timeout,
          base::BindOnce(
              [](base::WeakPtr<PageToolManager> self, PendingCall call,
                 ToolCallback callback,
                 base::expected<base::DictValue, std::string>) {
                if (!self) {
                  std::move(callback).Run(
                      ToolResult::Error("The session was closed."));
                  return;
                }
                self->Send(std::move(call), std::move(callback));
              },
              weak_factory_.GetWeakPtr(), std::move(call),
              std::move(callback)));
      return;
    }
  }

  if (call.save_text) {
    if (!result.has_value() || tool_error) {
      // Never forward the sidecar's error text: it can carry page content
      // (e.g. a cached accessible name of the secret's element).
      std::move(callback).Run(ToolResult::Error(
          "Could not read the element's text (a stale @ref? take a new "
          "snapshot)."));
      return;
    }
    SaveText(std::move(call), std::move(callback), RawText(*result));
    return;
  }

  ToolResult out = ToToolResult(std::move(result));
  if (!out.is_error && (!call.filter.empty() || call.retried)) {
    for (base::Value& part : out.content) {
      std::string* text = part.GetDict().FindString("text");
      if (!text) {
        continue;
      }
      if (!call.filter.empty()) {
        *text = FilterSnapshot(*text, re2::RE2(call.filter, PatternOptions()));
      }
      if (call.retried) {
        *text = "(Was covered: scrolled into view and retried.) " + *text;
      }
      break;
    }
  }
  std::move(callback).Run(std::move(out));
}

void PageToolManager::SaveText(PendingCall call,
                               ToolCallback callback,
                               std::string text) {
  std::string value(base::TrimWhitespaceASCII(text, base::TRIM_ALL));
  if (!call.save_regex.empty()) {
    re2::RE2 pattern(call.save_regex, re2::RE2::Options(re2::RE2::Quiet));
    std::string match;
    const bool found = pattern.NumberOfCapturingGroups() > 0
                           ? re2::RE2::PartialMatch(value, pattern, &match)
                           : re2::RE2::PartialMatch(
                                 value,
                                 re2::RE2("(" + call.save_regex + ")",
                                          re2::RE2::Options(re2::RE2::Quiet)),
                                 &match);
    if (!found) {
      // Never echo the text: it may be the secret.
      std::move(callback).Run(
          ToolResult::Error("The regex does not match the element's text."));
      return;
    }
    value = std::move(match);
  }
  if (value.empty()) {
    std::move(callback).Run(ToolResult::Error("The element has no text."));
    return;
  }
  base::DictValue info;
  info.Set("length", static_cast<int>(value.size()));
  info.Set("sha256",
           base::HexEncodeLower(crypto::SHA256HashString(value)).substr(0, 12));
  if (call.save_to == "clipboard") {
    {
      // Commits when it goes out of scope: before success is reported.
      ui::ScopedClipboardWriter writer(ui::ClipboardBuffer::kCopyPaste);
      writer.WriteText(base::UTF8ToUTF16(value));
      // Honoured on macOS (current host only, no clipboard history); Linux
      // (Ozone) ignores it, see the tool description.
      writer.MarkAsConfidential();
    }
    info.Set("saved", "clipboard");
    std::move(callback).Run(ToolResult::Json(base::Value(std::move(info))));
    return;
  }
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
      base::BindOnce(&WriteSecretFile, call.secrets_dir, call.save_name,
                     std::move(value)),
      base::BindOnce(
          [](base::DictValue info, ToolCallback callback, SavedText saved) {
            if (!saved.error.empty()) {
              std::move(callback).Run(ToolResult::Error(saved.error));
              return;
            }
            info.Set("saved", "file");
            info.Set("path", saved.path);
            std::move(callback).Run(
                ToolResult::Json(base::Value(std::move(info))));
          },
          std::move(info), std::move(callback)));
}

Sidecar* PageToolManager::GetOrCreateSidecar(const std::string& session_id,
                                             const std::string& host) {
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
  config.cdp_endpoint = WithAccessToken(
      base::StrCat({"ws://", host, "/devtools/session/", session_id}));
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
      FROM_HERE, base::BindOnce(
                     [](base::WeakPtr<PageToolManager> self, Sidecar* sidecar) {
                       if (!self) {
                         return;
                       }
                       std::erase_if(
                           self->retiring_,
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
