# MCP v1: design sketch

**Update 2026-10-08 (fork branch `ac/agent-identity`, `72c9342e41` + `b6cdbdddeb`):** profiles became
**agents** with an identity. Tools `list/create/update/delete-agent` (create/update take `name`, `color`,
`avatar`); `open-session` takes only `{agent}`; one tab group **per agent** (shared by its sessions), labelled
with the agent's name in its color, synced both ways with the tab strip. Tables below show the v1 shape
(fork commit 20); `docs/plans/MCP-SERVER.md` "Agent identity" has the current model.

Status 2026-10-08: landed as fork commit 20 (`007d52a391`, squash of branch `ac/mcp` (`8134822d3f` partition inheritance, `53d4d1d02f`
MCP server). Mac `out/Sccache` green; `scripts/smoke-mcp.mjs` all OK
(protocol, Origin 403/GET 405, profile isolation, background tab `visible`, popup/`noopener`/`target=_blank`
tabs join the session in the same partition, 390x844 viewport, capture, navigate, cleanup), cookies survive a
restart (`--persist-check`), a profile deleted while loaded is wiped from disk at the next launch. smoke-cdp +
check-automation clean, no FATAL. Linux green, smoke-mcp 27/27. Note: CDP-triggered popups need
`Runtime.evaluate` with `userGesture: true` (popup blocker).
Reference: chromatrix `mcp-tools.md`, `tabs.md`, `agents.md` (same split: MCP provisions, CDP browses).

## Shape

```
agent ── POST http://127.0.0.1:<port>/mcp ──▶ DevToolsHttpHandler (content, handler thread)
                                               │  new: /mcp → UI thread
                                               ▼
                         ChromeDevToolsManagerDelegate::HandleMcpRequest
                                               ▼
                    agent_chrome::AgentChromeService (KeyedService, default profile)
                     ├─ McpServer         JSON-RPC: initialize, tools/list, tools/call, ping
                     ├─ ProfileRegistry   agent profiles ↔ storage partitions, persisted in prefs
                     └─ SessionManager    sessions ↔ tab groups, tab leases, capture, viewports
agent ── ws://127.0.0.1:<port>/devtools/page/<targetId> ──▶ normal CDP (shared, cooperative in v1)
```

Port: same as CDP (9222, or a free port, see `DevToolsActivePort`). MCP client config:
`claude mcp add --transport http agent-chrome http://127.0.0.1:9222/mcp`.

## content/ patch (the only one for MCP)

- `ServerWrapper::OnHttpRequest`: `info.path == "/mcp"` → post `DevToolsHttpHandler::OnMcpRequest`
  to the UI thread with method, headers and body.
- New virtual `DevToolsManagerDelegate::HandleMcpRequest(method, headers, body, callback(status,
  content_type, body))`; the default answers 404.
- Rules: POST only (GET → 405, since there is no SSE stream). Reject any request with an `Origin` header
  that is not in `--remote-allow-origins` (stops web pages calling /mcp; the Host check is already
  done by `RequestIsSafeToServe`). Notifications answer 202 with an empty body.

## MCP protocol (stateless)

- JSON-RPC 2.0 over POST, `application/json` responses only. No `Mcp-Session-Id` (stateless);
  echoes the client's `protocolVersion` if supported (2025-03-26, 2025-06-18), otherwise the latest.
- `initialize` → `serverInfo {name: "agent-chrome", version: <chrome version>}`, `capabilities.tools`.
- `tools/call` errors: tool failures → `isError: true` with a text message; bad JSON-RPC → error object.
- JSON parsing via `base::JSONReader` and `base::DictValue`; input schemas are hand-written JSON
  literals next to each tool.

## Tools

| Tool | Input | Result |
| --- | --- | --- |
| `list-profiles` | – | `[{id, createdAt, sessions, tabs}]` |
| `create-profile` | `id` (kebab slug, ≤64) | `{id}`; error if it exists |
| `delete-profile` | `id` | closes its sessions/tabs, flushes, obliterates the partition |
| `open-session` | `profile`, `agent` | `{sessionId, groupColor, cdpUrl}` (browser-level ws URL) |
| `list-sessions` | – | `[{sessionId, profile, agent, openedAt, tabs: [{targetId, url, title}]}]` |
| `close-session` | `sessionId` | closes its tabs, flushes cookies |
| `allocate-tab` | `sessionId`, `url?`, `width?`, `height?` | `{targetId, cdpUrl}` (page-level ws URL) |
| `release-tab` | `sessionId`, `targetId` | closes the tab |
| `navigate-tab` | `targetId`, `url` | `{url}` after commit (or `loading: true` on timeout) |
| `set-tab-viewport` | `targetId`, `width`, `height` | the size actually applied |
| `capture-tab` | `targetId`, `format?` (jpeg/png) | MCP `image` content block |

`agent` is a free-form label in v1 (no auth); it becomes the credential subject in v2.

## ProfileRegistry

- Profile id → `StoragePartitionConfig::Create(profile, "ac-<id>", "", /*in_memory=*/false)`. One
  partition domain per agent profile (the spike used one shared domain `agentchrome`), so that
  `BrowserContext::AsyncObliterateStoragePartition("ac-<id>")` wipes exactly one profile.
- The list is persisted in a profile pref `agent_chrome.profiles` (dict id → createdAt), because
  on-disk partition directories are hashed and can't be listed back as names.
- Delete: close the tabs → flush the cookie store → obliterate (its "GC required" path covers a
  partition that is still loaded; it is retried at next launch) → remove the pref entry.

## SessionManager

- **Session** = `{id, profile, agent, TabGroupId, openedAt}`, in memory only (agents re-open after
  a restart; session restore is not relied on).
- **Window:** the profile's single normal `Browser` (create one if none). All agent tabs live there.
- **Tab group:** created on the first `allocate-tab` (a group can't be empty), title
  `<profile> · <agent>`, colour = hash(profile) over `TabGroupColorId` (same profile = same colour).
- **allocate-tab:** `WebContents::Create` with `SiteInstance::CreateForFixedStoragePartition(url,
  config)`, appended to the tab strip in the background (not activated), added to the session group,
  then navigated. Lease data lives in a `WebContentsUserData` (`AgentTabLease`: session id, capture
  handle, requested size).
- **Unthrottled background tabs:** the lease holds `IncrementCapturerCount(size, stay_hidden=false,
  stay_awake=true)`, as in the spike (visible, rAF at full rate, timers unthrottled).
- **Viewport:** `WebContents::Resize()` while hidden. A `TabStripModelObserver` re-applies the size
  when a leased tab is deactivated (activation snaps it to the window size). Making `outerWidth/Height`
  report the tab size, so they match the resized viewport, is follow-up work (MCP-SERVER item 4).
- **Opened tabs:** popups, `noopener` and Cmd-click tabs from a leased tab stay in its partition
  (`8134822d3f`). The observer leases them to the same session and moves them into its group.
- **User/CDP-closed tabs:** the observer drops their lease; an empty session stays open (no group
  until its next tab).
- **Cookie flush:** `GetCookieManagerForBrowserProcess()->FlushCookieStore()` on close-session,
  delete-profile and on `chrome::OnClosingAllBrowsers` (the spike saw cookies lost on `Browser.close`).

## Code layout

`chrome/browser/agent_chrome/`: `agent_chrome_service{,_factory}.{h,cc}`, `mcp_server.{h,cc}`,
`mcp_tools.{h,cc}`, `profile_registry.{h,cc}`, `session_manager.{h,cc}`, `agent_tab_lease.{h,cc}`,
`BUILD.gn` (source_set, deps from `//chrome/browser` + `//chrome/browser/ui`), `DEPS`.
Touches outside: the content/ hook (3 files), `ChromeDevToolsManagerDelegate` (forward to the
service), pref registration, factory registration in `ChromeBrowserMainExtraParts` service init.

## Not in v1

Scoped per-agent CDP (Playwright `context.newPage()`/`cookies()` still hit the default partition;
agents use `allocate-tab` + page URLs), auth, `chrome://agents`, stale-session cleanup (idle
timeout or dashboard), takeover/screencast, `Runtime.enable` fidelity mitigations.

## Test plan

`scripts/smoke-mcp.mjs <port>`: initialize → tools/list → create-profile ×2 → open-session ×2 →
allocate-tab ×2 (local server sets a cookie) → CDP check that cookies are isolated and stay with a
`target=_blank` popup → capture-tab → set-tab-viewport 390x844 → close-session → restart →
cookies persisted → delete-profile → partition gone. Plus the existing smoke-cdp/check-automation.

## Decided (2026-10-08)

- Port: keep the free-port fallback; log the actual MCP URL (it is also in `DevToolsActivePort`).
- `release-tab` closes the tab.
- Stale sessions: not in v1; they live until `close-session`, `delete-profile` or browser exit.
