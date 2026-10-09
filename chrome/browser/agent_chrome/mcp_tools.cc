// Copyright 2026 The Agent Chrome Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/agent_chrome/mcp_tools.h"

#include <map>
#include <optional>
#include <utility>

#include "base/base64.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/json/json_writer.h"
#include "base/i18n/time_formatting.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/agent_chrome/agent_chrome_service.h"
#include "chrome/browser/agent_chrome/profile_registry.h"
#include "chrome/browser/agent_chrome/session_manager.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "net/base/net_errors.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

namespace agent_chrome {

namespace {

constexpr base::TimeDelta kNavigationTimeout = base::Seconds(30);
constexpr base::TimeDelta kCaptureTimeout = base::Seconds(10);
constexpr int kJpegQuality = 80;

std::string HostOrDefault(const ToolContext& context) {
  return context.host.empty() ? std::string("127.0.0.1:9222") : context.host;
}

std::string PageWebSocketUrl(const ToolContext& context,
                             const std::string& target_id) {
  return base::StrCat(
      {"ws://", HostOrDefault(context), "/devtools/page/", target_id});
}

const std::string* GetString(const base::DictValue& args,
                             std::string_view key) {
  const std::string* value = args.FindString(key);
  return value && !value->empty() ? value : nullptr;
}

std::string MissingArgument(std::string_view key) {
  return base::StrCat({"Missing argument: ", key});
}

base::DictValue DescribeTab(const ToolContext& context,
                            content::WebContents* contents) {
  std::string target_id = SessionManager::TargetIdOf(contents);
  return base::DictValue()
      .Set("targetId", target_id)
      .Set("url", contents->GetVisibleURL().spec())
      .Set("title", base::UTF16ToUTF8(contents->GetTitle()))
      .Set("cdpUrl", PageWebSocketUrl(context, target_id));
}

base::DictValue DescribeSession(const ToolContext& context,
                                const Session& session) {
  base::ListValue tabs;
  for (content::WebContents* contents :
       context.service->sessions().TabsOf(session.id)) {
    tabs.Append(DescribeTab(context, contents));
  }
  return base::DictValue()
      .Set("sessionId", session.id)
      .Set("agent", session.agent)
      .Set("openedAt", base::TimeFormatAsIso8601(session.opened))
      .Set("tabs", std::move(tabs));
}

// Resolves `targetId` to a tab leased to some session.
content::WebContents* GetLeasedTab(const base::DictValue& args,
                                   std::string* error) {
  const std::string* target_id = GetString(args, "targetId");
  if (!target_id) {
    *error = MissingArgument("targetId");
    return nullptr;
  }
  content::WebContents* contents = SessionManager::FindTab(*target_id);
  if (!contents || SessionManager::SessionOf(contents).empty()) {
    *error = "Unknown or unleased tab: " + *target_id;
    return nullptr;
  }
  return contents;
}

std::optional<gfx::Size> GetViewport(const base::DictValue& args,
                                     std::string* error) {
  std::optional<int> width = args.FindInt("width");
  std::optional<int> height = args.FindInt("height");
  if (!width && !height) {
    return std::nullopt;
  }
  if (!width || !height || *width <= 0 || *height <= 0) {
    *error = "width and height must both be positive integers";
    return std::nullopt;
  }
  return gfx::Size(*width, *height);
}

// Agents --------------------------------------------------------------------

base::DictValue DescribeAgent(const ProfileRegistry::Entry& entry) {
  base::DictValue dict =
      base::DictValue()
          .Set("id", entry.id)
          .Set("name", entry.name)
          .Set("color", ProfileRegistry::ColorName(entry.color))
          .Set("createdAt", base::TimeFormatAsIso8601(entry.created));
  if (!entry.avatar.empty()) {
    dict.Set("avatar", entry.avatar);
  }
  return dict;
}

// Reads name/color/avatar from `args`. Returns false with `error` set if one
// is invalid.
bool GetIdentity(const base::DictValue& args,
                 ProfileRegistry::Identity* identity,
                 std::string* error) {
  if (const std::string* name = args.FindString("name")) {
    *error = ProfileRegistry::CheckName(*name);
    if (!error->empty()) {
      return false;
    }
    identity->name = *name;
  }
  if (const std::string* color = args.FindString("color")) {
    identity->color = ProfileRegistry::ColorFromName(*color);
    if (!identity->color) {
      *error = "Unknown color: " + *color +
               " (grey, blue, red, yellow, green, pink, purple, cyan, orange)";
      return false;
    }
  }
  if (const std::string* avatar = args.FindString("avatar")) {
    *error = ProfileRegistry::CheckAvatar(*avatar);
    if (!error->empty()) {
      return false;
    }
    identity->avatar = *avatar;
  }
  return true;
}

void ListAgents(const ToolContext& context,
                const base::DictValue& args,
                ToolCallback callback) {
  std::map<std::string, int> sessions;
  std::map<std::string, int> tabs;
  SessionManager& manager = context.service->sessions();
  for (const Session* session : manager.List()) {
    sessions[session->agent]++;
    tabs[session->agent] += manager.TabsOf(session->id).size();
  }
  base::ListValue list;
  for (const ProfileRegistry::Entry& entry :
       context.service->profiles().List()) {
    list.Append(DescribeAgent(entry)
                    .Set("sessions", sessions[entry.id])
                    .Set("tabs", tabs[entry.id]));
  }
  std::move(callback).Run(ToolResult::Json(
      base::Value(base::DictValue().Set("agents", std::move(list)))));
}

void CreateAgent(const ToolContext& context,
                 const base::DictValue& args,
                 ToolCallback callback) {
  const std::string* id = GetString(args, "id");
  if (!id) {
    std::move(callback).Run(ToolResult::Error(MissingArgument("id")));
    return;
  }
  if (!ProfileRegistry::IsValidId(*id)) {
    std::move(callback).Run(ToolResult::Error(
        "Invalid agent id (kebab-case, a-z 0-9 and '-', max 64): " + *id));
    return;
  }
  ProfileRegistry::Identity identity;
  std::string error;
  if (!GetIdentity(args, &identity, &error)) {
    std::move(callback).Run(ToolResult::Error(error));
    return;
  }
  if (!context.service->profiles().Create(*id, identity)) {
    std::move(callback).Run(ToolResult::Error("Agent already exists: " + *id));
    return;
  }
  std::move(callback).Run(ToolResult::Json(
      base::Value(DescribeAgent(*context.service->profiles().Get(*id)))));
}

void UpdateAgent(const ToolContext& context,
                 const base::DictValue& args,
                 ToolCallback callback) {
  const std::string* id = GetString(args, "id");
  if (!id) {
    std::move(callback).Run(ToolResult::Error(MissingArgument("id")));
    return;
  }
  ProfileRegistry::Identity identity;
  std::string error;
  if (!GetIdentity(args, &identity, &error)) {
    std::move(callback).Run(ToolResult::Error(error));
    return;
  }
  if (!context.service->profiles().Update(*id, identity)) {
    std::move(callback).Run(ToolResult::Error("Unknown agent: " + *id));
    return;
  }
  context.service->sessions().RefreshGroups(*id);
  std::move(callback).Run(ToolResult::Json(
      base::Value(DescribeAgent(*context.service->profiles().Get(*id)))));
}

void DeleteAgent(const ToolContext& context,
                 const base::DictValue& args,
                 ToolCallback callback) {
  const std::string* id = GetString(args, "id");
  if (!id) {
    std::move(callback).Run(ToolResult::Error(MissingArgument("id")));
    return;
  }
  if (!context.service->profiles().Exists(*id)) {
    std::move(callback).Run(ToolResult::Error("Unknown agent: " + *id));
    return;
  }
  context.service->sessions().CloseProfileSessions(*id);
  context.service->profiles().Delete(
      *id, base::BindOnce(
               [](std::string id, ToolCallback callback) {
                 std::move(callback).Run(ToolResult::Json(base::Value(
                     base::DictValue().Set("id", id).Set("deleted", true))));
               },
               *id, std::move(callback)));
}

// Sessions ------------------------------------------------------------------

void OpenSession(const ToolContext& context,
                 const base::DictValue& args,
                 ToolCallback callback) {
  const std::string* agent = GetString(args, "agent");
  if (!agent) {
    std::move(callback).Run(ToolResult::Error(MissingArgument("agent")));
    return;
  }
  if (!context.service->profiles().Exists(*agent)) {
    std::move(callback).Run(ToolResult::Error(
        "Unknown agent: " + *agent + " (create-agent first)"));
    return;
  }
  const Session& session = context.service->sessions().Open(*agent);
  base::DictValue result = DescribeSession(context, session);
  // A browser-level CDP connection scoped to the session.
  result.Set("cdpEndpoint", base::StrCat({"ws://", HostOrDefault(context),
                                          "/devtools/session/", session.id}));
  std::move(callback).Run(ToolResult::Json(base::Value(std::move(result))));
}

void ListSessions(const ToolContext& context,
                  const base::DictValue& args,
                  ToolCallback callback) {
  base::ListValue list;
  for (const Session* session : context.service->sessions().List()) {
    list.Append(DescribeSession(context, *session));
  }
  std::move(callback).Run(ToolResult::Json(
      base::Value(base::DictValue().Set("sessions", std::move(list)))));
}

void CloseSession(const ToolContext& context,
                  const base::DictValue& args,
                  ToolCallback callback) {
  const std::string* session_id = GetString(args, "sessionId");
  if (!session_id) {
    std::move(callback).Run(ToolResult::Error(MissingArgument("sessionId")));
    return;
  }
  if (!context.service->sessions().Close(*session_id)) {
    std::move(callback).Run(
        ToolResult::Error("Unknown session: " + *session_id));
    return;
  }
  std::move(callback).Run(ToolResult::Json(base::Value(
      base::DictValue().Set("sessionId", *session_id).Set("closed", true))));
}

// Tabs ----------------------------------------------------------------------

void AllocateTab(const ToolContext& context,
                 const base::DictValue& args,
                 ToolCallback callback) {
  const std::string* session_id = GetString(args, "sessionId");
  if (!session_id) {
    std::move(callback).Run(ToolResult::Error(MissingArgument("sessionId")));
    return;
  }
  std::string error;
  std::optional<gfx::Size> viewport = GetViewport(args, &error);
  if (!error.empty()) {
    std::move(callback).Run(ToolResult::Error(error));
    return;
  }
  const std::string* url = args.FindString("url");
  auto contents = context.service->sessions().AllocateTab(
      *session_id, url ? GURL(*url) : GURL(), viewport);
  if (!contents.has_value()) {
    std::move(callback).Run(ToolResult::Error(contents.error()));
    return;
  }
  std::move(callback).Run(
      ToolResult::Json(base::Value(DescribeTab(context, *contents))));
}

void ReleaseTab(const ToolContext& context,
                const base::DictValue& args,
                ToolCallback callback) {
  std::string error;
  content::WebContents* contents = GetLeasedTab(args, &error);
  if (!contents) {
    std::move(callback).Run(ToolResult::Error(error));
    return;
  }
  const std::string* session_id = GetString(args, "sessionId");
  if (session_id && SessionManager::SessionOf(contents) != *session_id) {
    std::move(callback).Run(ToolResult::Error(
        "The tab is not leased to session " + *session_id));
    return;
  }
  std::string target_id = *args.FindString("targetId");
  context.service->sessions().ReleaseTab(contents);
  std::move(callback).Run(ToolResult::Json(base::Value(
      base::DictValue().Set("targetId", target_id).Set("closed", true))));
}

// Waits for the next primary main frame navigation to finish.
class NavigationWaiter : public content::WebContentsObserver {
 public:
  static void Start(content::WebContents* contents, ToolCallback callback) {
    // Owns itself until it replies.
    new NavigationWaiter(contents, std::move(callback));
  }

 private:
  NavigationWaiter(content::WebContents* contents, ToolCallback callback)
      : content::WebContentsObserver(contents), callback_(std::move(callback)) {
    timer_.Start(FROM_HERE, kNavigationTimeout,
                 base::BindOnce(&NavigationWaiter::OnTimeout,
                                base::Unretained(this)));
  }
  ~NavigationWaiter() override = default;

  void DidFinishNavigation(content::NavigationHandle* handle) override {
    if (!handle->IsInPrimaryMainFrame() || handle->IsSameDocument()) {
      return;
    }
    base::DictValue result;
    result.Set("url", handle->GetURL().spec());
    result.Set("committed", handle->HasCommitted());
    if (handle->GetNetErrorCode() != net::OK) {
      result.Set("error", net::ErrorToShortString(handle->GetNetErrorCode()));
    }
    Reply(ToolResult::Json(base::Value(std::move(result))));
  }

  void WebContentsDestroyed() override {
    Reply(ToolResult::Error("The tab was closed"));
  }

  void OnTimeout() {
    Reply(ToolResult::Json(base::Value(
        base::DictValue()
            .Set("url", web_contents()->GetVisibleURL().spec())
            .Set("loading", true))));
  }

  void Reply(ToolResult result) {
    std::move(callback_).Run(std::move(result));
    delete this;
  }

  ToolCallback callback_;
  base::OneShotTimer timer_;
};

void NavigateTab(const ToolContext& context,
                 const base::DictValue& args,
                 ToolCallback callback) {
  std::string error;
  content::WebContents* contents = GetLeasedTab(args, &error);
  const std::string* url_string = GetString(args, "url");
  if (!contents || !url_string) {
    std::move(callback).Run(
        ToolResult::Error(contents ? MissingArgument("url") : error));
    return;
  }
  GURL url(*url_string);
  if (!url.is_valid()) {
    std::move(callback).Run(ToolResult::Error("Invalid URL: " + *url_string));
    return;
  }
  NavigationWaiter::Start(contents, std::move(callback));
  content::NavigationController::LoadURLParams params(url);
  params.transition_type = ui::PAGE_TRANSITION_TYPED;
  contents->GetController().LoadURLWithParams(params);
}

void SetTabViewport(const ToolContext& context,
                    const base::DictValue& args,
                    ToolCallback callback) {
  std::string error;
  content::WebContents* contents = GetLeasedTab(args, &error);
  std::optional<gfx::Size> size;
  if (contents) {
    size = GetViewport(args, &error);
    if (!size && error.empty()) {
      error = MissingArgument("width/height");
    }
  }
  if (!error.empty()) {
    std::move(callback).Run(ToolResult::Error(error));
    return;
  }
  gfx::Size applied = context.service->sessions().SetViewport(contents, *size);
  std::move(callback).Run(ToolResult::Json(base::Value(
      base::DictValue()
          .Set("width", applied.width())
          .Set("height", applied.height()))));
}

void CaptureTab(const ToolContext& context,
                const base::DictValue& args,
                ToolCallback callback) {
  std::string error;
  content::WebContents* contents = GetLeasedTab(args, &error);
  if (!contents) {
    std::move(callback).Run(ToolResult::Error(error));
    return;
  }
  const std::string* format = args.FindString("format");
  bool png = format && *format == "png";
  if (format && !png && *format != "jpeg") {
    std::move(callback).Run(ToolResult::Error("format must be jpeg or png"));
    return;
  }
  content::RenderWidgetHostView* view = contents->GetRenderWidgetHostView();
  if (!view) {
    std::move(callback).Run(ToolResult::Error("The tab has no view yet"));
    return;
  }
  // CSS pixels (not device pixels): a quarter of the data on 2x screens.
  // Crop to the agent's viewport, which may be smaller than the view of the
  // active tab.
  gfx::Rect source(view->GetViewBounds().size());
  if (std::optional<gfx::Size> viewport =
          SessionManager::ViewportOf(contents)) {
    source.Intersect(gfx::Rect(*viewport));
  }
  view->CopyFromSurface(
      source, source.size(), kCaptureTimeout,
      base::BindOnce(
          [](bool png, ToolCallback callback,
             const content::CopyFromSurfaceResult& result) {
            if (!result.has_value() || result->bitmap.drawsNothing()) {
              std::move(callback).Run(
                  ToolResult::Error("Could not capture the tab"));
              return;
            }
            std::optional<std::vector<uint8_t>> encoded =
                png ? gfx::PNGCodec::EncodeBGRASkBitmap(
                          result->bitmap, /*discard_transparency=*/true)
                    : gfx::JPEGCodec::Encode(result->bitmap, kJpegQuality);
            if (!encoded) {
              std::move(callback).Run(
                  ToolResult::Error("Could not encode the capture"));
              return;
            }
            std::move(callback).Run(
                ToolResult::Image(base::Base64Encode(*encoded),
                                  png ? "image/png" : "image/jpeg"));
          },
          png, std::move(callback)));
}

// Tool table ----------------------------------------------------------------

constexpr char kNoArgs[] = R"({"type":"object","properties":{}})";

constexpr char kAgentIdArgs[] = R"({"type":"object","properties":{
  "id":{"type":"string","description":"Agent id: kebab-case slug, max 64 chars."}},
  "required":["id"]})";

constexpr char kCreateAgentArgs[] = R"({"type":"object","properties":{
  "id":{"type":"string","description":"Agent id: kebab-case slug, max 64 chars."},
  "name":{"type":"string","description":"Friendly name, shown on the tab group (max 64 chars; default: the id)."},
  "color":{"type":"string","enum":["grey","blue","red","yellow","green","pink","purple","cyan","orange"],"description":"Tab group color (default: picked from the id)."},
  "avatar":{"type":"string","description":"Optional image: https: or data:image/ URL (max 256 KB); empty removes it."}},
  "required":["id"]})";

constexpr char kUpdateAgentArgs[] = R"({"type":"object","properties":{
  "id":{"type":"string"},
  "name":{"type":"string","description":"Friendly name, shown on the tab group (max 64 chars; default: the id)."},
  "color":{"type":"string","enum":["grey","blue","red","yellow","green","pink","purple","cyan","orange"],"description":"Tab group color (default: picked from the id)."},
  "avatar":{"type":"string","description":"Optional image: https: or data:image/ URL (max 256 KB); empty removes it."}},
  "required":["id"]})";

constexpr char kOpenSessionArgs[] = R"({"type":"object","properties":{
  "agent":{"type":"string","description":"Agent id."}},
  "required":["agent"]})";

constexpr char kSessionIdArgs[] = R"({"type":"object","properties":{
  "sessionId":{"type":"string"}},"required":["sessionId"]})";

constexpr char kAllocateTabArgs[] = R"({"type":"object","properties":{
  "sessionId":{"type":"string"},
  "url":{"type":"string","description":"URL to open (default about:blank)."},
  "width":{"type":"integer","description":"Viewport width in CSS px."},
  "height":{"type":"integer","description":"Viewport height in CSS px."}},
  "required":["sessionId"]})";

constexpr char kReleaseTabArgs[] = R"({"type":"object","properties":{
  "targetId":{"type":"string"},
  "sessionId":{"type":"string","description":"Optional; checked if given."}},
  "required":["targetId"]})";

constexpr char kNavigateTabArgs[] = R"({"type":"object","properties":{
  "targetId":{"type":"string"},"url":{"type":"string"}},
  "required":["targetId","url"]})";

constexpr char kViewportArgs[] = R"({"type":"object","properties":{
  "targetId":{"type":"string"},
  "width":{"type":"integer"},"height":{"type":"integer"}},
  "required":["targetId","width","height"]})";

constexpr char kCaptureArgs[] = R"({"type":"object","properties":{
  "targetId":{"type":"string"},
  "format":{"type":"string","enum":["jpeg","png"],"description":"Default jpeg."}},
  "required":["targetId"]})";

constexpr Tool kTools[] = {
    {"list-agents",
     "List agents: identity (name, color, avatar), open sessions and tabs.",
     kNoArgs, &ListAgents},
    {"create-agent",
     "Create an agent: an identity shown on its tab groups, and its own "
     "cookies, logins and storage, kept across restarts.",
     kCreateAgentArgs, &CreateAgent},
    {"update-agent",
     "Change an agent's name, color or avatar; its open tab groups follow.",
     kUpdateAgentArgs, &UpdateAgent},
    {"delete-agent",
     "Close the agent's sessions and tabs and wipe its storage.",
     kAgentIdArgs, &DeleteAgent},
    {"open-session",
     "Start working as an agent. Returns a sessionId and cdpEndpoint; tabs "
     "you allocate form one tab group with the agent's name and color. "
     "cdpEndpoint is a browser-level CDP WebSocket that only sees this "
     "session's tabs: use it with Playwright connectOverCDP or agent-browser "
     "--cdp. There, the default browser context is the agent's (cookies, "
     "storage), new pages (Target.createTarget, Playwright newPage) become "
     "session tabs, and Browser.close ends the session. Don't create browser "
     "contexts.",
     kOpenSessionArgs, &OpenSession},
    {"list-sessions", "List open sessions and their tabs.", kNoArgs,
     &ListSessions},
    {"close-session", "Close the session's tabs and end it.", kSessionIdArgs,
     &CloseSession},
    {"allocate-tab",
     "Open a background tab leased to the session, in its agent's storage. "
     "Returns targetId (in Playwright: the page whose CDP session reports "
     "it via Target.getTargetInfo) and cdpUrl (this tab's own page "
     "WebSocket). Tabs it opens (popups, target=_blank) join the session. "
     "width/height set its viewport (see set-tab-viewport).",
     kAllocateTabArgs, &AllocateTab},
    {"release-tab", "Close a leased tab.", kReleaseTabArgs, &ReleaseTab},
    {"navigate-tab",
     "Navigate a leased tab and wait until the navigation commits (not until "
     "the page has loaded or rendered: wait for your own readiness signal, "
     "e.g. a selector, on single-page apps).",
     kNavigateTabArgs, &NavigateTab},
    {"set-tab-viewport",
     "Set a leased tab's viewport in CSS px (device emulation: works for "
     "the active tab and below the window minimum, e.g. 390x844). Kept "
     "across navigations. Returns the size applied.",
     kViewportArgs, &SetTabViewport},
    {"capture-tab",
     "Screenshot of a leased tab's viewport in CSS px (also in the "
     "background).",
     kCaptureArgs, &CaptureTab},
};

}  // namespace

ToolResult::ToolResult() = default;
ToolResult::ToolResult(ToolResult&&) = default;
ToolResult& ToolResult::operator=(ToolResult&&) = default;
ToolResult::~ToolResult() = default;

// static
ToolResult ToolResult::Json(base::Value value) {
  ToolResult result;
  result.content.Append(
      base::DictValue()
          .Set("type", "text")
          .Set("text", base::WriteJson(value).value_or("null")));
  if (value.is_dict()) {
    result.structured_content = std::move(value).TakeDict();
  }
  return result;
}

// static
ToolResult ToolResult::Error(std::string message) {
  ToolResult result;
  result.is_error = true;
  result.content.Append(
      base::DictValue().Set("type", "text").Set("text", std::move(message)));
  return result;
}

// static
ToolResult ToolResult::Image(std::string base64_data, std::string mime_type) {
  ToolResult result;
  result.content.Append(base::DictValue()
                            .Set("type", "image")
                            .Set("data", std::move(base64_data))
                            .Set("mimeType", std::move(mime_type)));
  return result;
}

base::DictValue ToolResult::ToValue() && {
  base::DictValue value;
  value.Set("content", std::move(content));
  if (structured_content) {
    value.Set("structuredContent", std::move(*structured_content));
  }
  if (is_error) {
    value.Set("isError", true);
  }
  return value;
}

base::span<const Tool> GetTools() {
  return kTools;
}

}  // namespace agent_chrome
