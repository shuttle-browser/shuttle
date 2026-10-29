# MCP server and agent model

Shuttle's MCP server runs **inside the browser** (C++, `chrome/browser/shuttle/`, so upstream merges rarely touch it).
MCP manages agents, sessions and tabs and offers page tools; CDP stays the transport underneath and an escape hatch.
Many agents share one Shuttle instance. Smoke tests: `scripts/smoke-mcp.mjs`, `smoke-session-cdp.mjs`,
`smoke-agents-page.mjs`, `smoke-network.mjs`.

## Model

| Term | Meaning | Chromium mechanism |
| --- | --- | --- |
| **Agent** | Identity (id = kebab slug `^[a-z0-9]+(-[a-z0-9]+)*$`, ≤64; name, color, optional avatar) + its own persistent storage (cookies, logins, local storage). Created/updated/deleted over MCP or `chrome://agents`. | One persistent, non-default `StoragePartition` per agent (`StoragePartitionConfig::Create(profile, "shuttle-<id>", "", false)`) inside the single Chrome profile; list persisted in the pref `shuttle.profiles` (partition dirs are hashed and can't be listed back). |
| **Session** | One MCP client working as one agent. | In memory only (agents re-open after a restart; session restore isn't relied on). |
| **Tab** | Leased to exactly one session. | `WebContents` created with `SiteInstance::CreateForFixedStoragePartition`; lease data in `AgentTabLease` (`WebContentsUserData`). |
| **Tab group** | The agent's visible identity: one group per agent (all its sessions' tabs), label = name, color = agent color, avatar on the chip. | Upstream tab groups, user-facing strings renamed to "agent"; C++ `TabGroup` ids unchanged for cheap merges. |

- One Chrome profile only; profile-wide state (content settings, history, downloads, extensions) is shared. Accepted.
- Several sessions may share an agent: shared storage (last writer wins), exclusive tab leases.
- No incognito anywhere, CDP included: `Target.createBrowserContext` fails by design.
- Groups exist only as agents: manual group creation (menus, shortcuts, history "open in group") is a no-op; restored
  tabs come back ungrouped. Renaming/recoloring a group writes back to the agent.
- Tab menu "Hand tab to agent ▸" / dragging a tab into an agent's group = adopt (reopened in the agent's storage,
  leased to its latest session); "Take back from agent" reopens it in the user's storage. Dragging out leaves it
  leased but ungrouped (known gap).
- No startup window: Shuttle starts windowless unless it has URLs; the first agent tab or the user creates a window.
  Linux: a person starting it (tty/desktop) gets `chrome://agents`; it keeps running windowless while a session is open.

## Endpoint and protocol

- Same port as CDP (9222, or a free port; see `DevToolsActivePort`), path `/mcp`. Hook in
  `content/browser/devtools/devtools_http_handler.cc` → `DevToolsManagerDelegate::HandleMcpRequest` →
  `ChromeDevToolsManagerDelegate` → `shuttle::ShuttleService` (KeyedService on the default profile).
- JSON-RPC 2.0 over POST, plain `application/json` responses (net::HttpServer sends whole responses: no SSE), stateless
  (no `Mcp-Session-Id`). GET → 405; notifications → 202. Requests with an `Origin` not in `--remote-allow-origins` are
  rejected (web pages can't call `/mcp`); the Host check is upstream's `RequestIsSafeToServe`.
- Default: localhost only, no auth (same trust as the CDP port). Network mode (`--shuttle-listen`, `network_access.cc`)
  requires a token for remote clients; plain HTTP.
- Service pieces: `mcp_tools` (tool table + schemas), `profile_registry` (agents ↔ partitions), `session_manager`
  (sessions, groups, leases, capture handles, viewports), `session_cdp_filter`, `page_tools` + `agent_browser_sidecar`,
  `avatar_cache`.

## Agent tabs

- `allocate-tab`: background tab in the profile's normal window (created if none), in the agent's group.
- **Unthrottled while leased:** the lease holds `IncrementCapturerCount(stay_hidden=false, stay_awake=true)`, so
  background agent tabs report `visible`, run rAF at full rate and don't throttle timers.
- **Viewport:** applied as an emulation override owned by the lease (`Emulation.setDeviceMetricsOverride` through an
  internal DevTools client), so it survives activation and works below Chrome's 500 px window minimum.
- **Opened tabs stay in the partition:** popups, `noopener`/`target=_blank` and Cmd/middle-click tabs from a leased tab
  (upstream leaks the last two to the default partition; patched in `WebContentsImpl::CreateNewWindow` and
  `browser_navigator.cc`). They join the same session and group. CDP-triggered popups need `userGesture: true`.
- **Blank tabs stay in the partition:** a tab allocated without `url`, and `about:blank` along the way, stay in the
  agent's storage (upstream gives a site-less SiteInstance the default partition; patched in `SiteInstanceImpl`,
  fork commit 50). Probe: `scripts/smoke-isolation.mjs`.
- **Session cookies persist:** an agent's session cookies (no Expires/Max-Age) survive a browser restart, like
  Chrome's main partition with "Continue where you left off" (upstream drops them in every non-default partition).
  Per-agent opt-out: `persistSessionCookies: false` on `create-agent`/`update-agent` (`list-agents` shows it;
  toggle in the `chrome://agents` edit dialog), stored as `persist_session_cookies` in the agent's `shuttle.profiles`
  entry (missing = on). A partition's network context is configured once, so a change applies at the next start;
  opting out deletes nothing (the cookie store drops the agent's session cookies at its next load). Hook:
  `shuttle::ShouldPersistSessionCookies` (`agent_partition.cc`), called from `ProfileNetworkContextService`; it
  maps the partition path (`Storage/ext/shuttle-<id>/def`) back to the agent.
- **Cookie flush** (`FlushCookieStore`) on close-session, agent deletion and `OnClosingAllBrowsers`: cookies set right
  before `Browser.close` were otherwise lost in every partition.
- Deleting an agent closes its tabs, flushes, then `AsyncObliterateStoragePartition` (a still-loaded partition is
  wiped at the next launch).

## Per-session CDP endpoint

`open-session` returns `cdpEndpoint = ws://127.0.0.1:<port>/devtools/session/<sessionId>`: a browser-level DevTools
connection filtered to the session (content hook `DevToolsManagerDelegate::CreateConnectionFilter`;
`session_cdp_filter.cc`). Playwright `connectOverCDP(cdpEndpoint)` and `agent-browser --cdp` see only the agent's world:

- `Target.getTargets`/discovery/auto-attach: only the session's tabs; attach/close/activate on foreign targets → error.
- `Target.createTarget` → a leased tab; `Browser.close` → close-session (never quits the browser).
- Browser contexts/cookies (`Storage.*`) map to the agent's partition.
- Page endpoints (`/devtools/page/<id>`) can't reach other targets. The browser-wide endpoint stays open for tooling
  and DevTools; agents are told to use their session endpoint.
- Gotchas: internal commands sent from inside an agent-host event must be posted (re-entrant detach crashed
  `TargetHandler::AutoAttach`); a DevTools client's `GetTypeForMetrics()` must be DevTools/Extension/RemoteDebugger/Other;
  a client that auto-attached with `waitForDebuggerOnStart` holds new navigations until it disconnects (upstream).

## Page tools (agent-browser sidecar)

[agent-browser](https://github.com/vercel-labs/agent-browser) (Rust, Apache-2.0, bundled unmodified with LICENSE +
NOTICE) supplies the page tools, proxied through `/mcp`.

- `tools/list` = the agent/session/tab tools + ~24 curated page tools (`snapshot click fill type press select check
  uncheck scroll wait-for eval get-text screenshot back forward reload console errors state-save state-load hover
  dblclick find open`), ~7.6 KB of schema instead of agent-browser's 63 KB `core` set. Kebab-case names; every page
  tool takes a required `targetId` (the session is derived from the tab). `wait-for` merges the five `wait_*` tools.
  Hidden: tabs, close, profile/auth/launch, `path`/`extraArgs`, downloads/uploads.
- Definitions: `config/agent-browser-tools.json` → `scripts/gen-page-tools.mjs` → `page_tools_data.inc` (C++ raw
  string; regenerate and commit together). Args are whitelisted against each tool's schema; unknown args are an error.
  Results drop agent-browser's `structuredContent`/`lifecycle` envelope (`click` → `{"clicked":"@e3"}`, no data → `ok`).
- `PageToolManager` keeps one `Sidecar` per session (`agent-browser mcp --tools all`, env `AGENT_BROWSER_CDP=<session
  endpoint>`, `AGENT_BROWSER_SESSION`, short `AGENT_BROWSER_SOCKET_DIR`: Mac socket paths max ~100 chars). Calls are
  serialized per sidecar; a `tab_switch {tab:<targetId>}` precedes calls for a different tab. Never set
  `AGENT_BROWSER_PIN_TAB` (a fresh daemon opens a stray tab).
- Each sidecar starts a daemon that outlives the MCP process: on session close send the `close` tool (3 s), then
  kill; `AGENT_BROWSER_IDLE_TIMEOUT_MS` is the fallback. Per-job timeout 60 s (wait + 15 s; 20 s cold start), then the
  process is killed and restarted on the next call. Cold first call ~0.5 s, later calls 25-50 ms.
- Binary: `$SHUTTLE_AGENT_BROWSER`, else `Contents/Helpers/agent-browser` (Mac) or next to the executable (Linux).
  `scripts/fetch-agent-browser.sh <Helpers-dir>` installs the pinned version; `release-mac.sh` signs it with the app.
- Later, data-driven: port the hot-path tools (snapshot, click, fill, press, wait-for, eval) to in-browser C++ with the
  same names, refs and output; the sidecar keeps the long tail.

## `chrome://agents`

Lit WebUI (`chrome/browser/resources/agents`, `chrome/browser/ui/webui/agents`) from cr elements and cr vars only
(Chromium Settings look, no custom brand colors). Lists agents with avatar, sessions and tabs (2 s refresh while
visible); create, edit name/color/avatar (file → center crop 256 px data: URL) and "Keep session cookies after
restart", delete (storage wiped), end session,
focus a tab, "Open tab" (foreground tab in the agent's storage, e.g. to sign in). Calls the MCP tools via
`chrome.send('callTool')` (allowlisted in `build_webui.gni` `legacy_chrome_send_users`).

## Open

- `outerWidth/outerHeight` report the window, not the resized tab viewport (detectable mismatch).
- Empty sessions (all tabs closed by hand) stay open until `close-session`; no idle timeout yet.
- Fidelity mitigations (`Runtime.enable` suppression) and remote takeover/screencast: not in the browser.
