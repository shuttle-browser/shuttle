# MCP v2: agents drive the browser through MCP

Status 2026-10-08: plan + spike (the Mac session). Decided: **sidecar first, then native**. agent-browser
(vercel-labs, Rust, Apache-2.0) supplies the page tools: bundled as a sidecar and proxied through our
`/mcp`. Later, the hot-path tools get ported to in-browser C++ with the same names and ref format.

Goal: an agent is pointed at one MCP server (`http://127.0.0.1:9222/mcp`) and drives the browser
entirely through it: agents/sessions/tabs (v1) + snapshot/act/wait/read/logs (v2). CDP stays the
transport underneath and remains an escape hatch.

## Why (first external trial, 2026-10-08)

- v1 has no page tools: every click/read/check needed a hand-written Playwright/CDP script.
- Playwright `contexts()[0]` is the default profile, not the agent: `cookies()`, `newPage()`,
  `storageState()` hit default storage.
- Isolation is by convention (see spike), and viewport/emulation set by a client dies with its connection.

## Spike results (agent-browser 0.38.2 vs test browser 9333, agents alice x2 + steve)

| Check | Result |
| --- | --- |
| `--cdp ws://…/devtools/page/<steve's tab>` | Works, **but sees all 3 tabs** (`tab` lists alice's too) and drove alice's tab (the active one). |
| Per-tab `cdpUrl` isolation | **None.** A page-level CDP session can use the Target domain browser-wide. v1 told agents this was the safe path: wrong. |
| `--cdp 9333 --pin-tab open <url>` | New tab via `Target.createTarget`: **outside any session**, default storage, no group. |
| Snapshot | `- link "Login" [ref=e14]`, `-i` interactive-only; compact and useful. |
| `agent-browser mcp` | stdio. `core` = 29 tools / **66 KB schema (~16k tokens)**, `all` = 64 tools / 145 KB. Every tool repeats 13 common props (`allowedDomains`, `caCert`, `restore*`, `session`, `namespace`…). |

Consequences: (1) we need a **per-session CDP endpoint** that is the agent's world, and page endpoints
must lose cross-target access; (2) the proxy must **curate and slim** agent-browser's schemas, not pass them through.

## Phases

### 1. Per-session CDP endpoint (fork commit, prerequisite for everything)

**Status 2026-10-08 (fork branch `ac/session-cdp`, `80db4a8192`, pushed; stacked on `ac/agents-page`):** implemented; Mac `out/Sccache` green;
`scripts/smoke-session-cdp.mjs` 30/30 (raw CDP + Playwright + agent-browser), smoke-mcp, agents-page and
check-automation green (all headless). Pieces: content hook `DevToolsManagerDelegate::CreateConnectionFilter`
+ `/devtools/session/<id>` in `devtools_http_handler.cc`; `shuttle/session_cdp_filter.{h,cc}`;
viewport via an internal DevTools client (`Emulation.setDeviceMetricsOverride`) owned by the tab lease;
capture-tab in CSS px cropped to the viewport; upstream crash fix (null view in
`WebContentsImpl::SetDeviceEmulationSize`, crashed 9222 at 16:59); `*.top-chrome` WebUI reported as type
`other`. Findings: internal commands sent from inside an agent-host event must be posted (re-entrant
detach during auto-attach crashed `TargetHandler::AutoAttach`); a DevTools client's
`GetTypeForMetrics()` must be one of DevTools/Extension/RemoteDebugger/Other (DCHECK); a client that
auto-attached with `waitForDebuggerOnStart` holds new navigations until it disconnects (upstream).
The maintainer's 9222 instance runs it. Not yet: Linux build (Mac only for now), landing on `main`.

- `open-session` returns `cdpEndpoint = ws://127.0.0.1:<port>/devtools/session/<sessionId>`: a
  browser-level DevTools session with a filter (in `chrome/browser/shuttle/`, hook in
  `ChromeDevToolsManagerDelegate` / content's browser handler):
  - `Target.getTargets` / `setDiscoverTargets` / `setAutoAttach` events: only the session's tabs.
  - `Target.createTarget` → `allocate-tab` (leased, agent storage, agent group).
  - `attachToTarget` / `closeTarget` / `activateTarget` on foreign targets → error.
  - `Target.getBrowserContexts` / `browserContextId`: present the agent as the (only) context;
    `Storage.getCookies`/`setCookies`/`clearCookies` → the agent's partition.
  - `Browser.close` → `close-session` (never quits the browser).
- Page endpoints (`/devtools/page/<id>`): Target domain limited to the page's own session.
- The browser-wide endpoint (`/devtools/browser/…`) stays for tooling/DevTools (open question below).
- Viewport fix rides along: `SetViewport` applies to the active tab too (emulation-level override owned by
  the lease), returns the effective size; `allocate-tab` width/height go the same path.
- Acceptance: Playwright `connectOverCDP(cdpEndpoint)` sees only the agent's tabs; `newPage()` creates a
  leased tab; `context.cookies()` = agent cookies; agent-browser with `--cdp <cdpEndpoint>` sees only the
  session; smoke-mcp extended (cross-agent attach rejected).

### 2. Sidecar: agent-browser behind `/mcp` (fork commit + tooling)

**Status 2026-10-08 (evening):** built and tested on Mac (`out/Sccache`, fork branch `ac/agent-browser`, combined
with the agent groups work on `ac/next`; the maintainer's 9222 instance runs it). End-to-end through `/mcp`: snapshot, fill, click,
check, wait-for, get-text, eval, screenshot, open, state-save (name validation), console, close-session; smoke-mcp
ALL OK (36 tools), smoke-session-cdp all ok. Fixes after the prototype: `Write()` uses `base::WriteFileDescriptor`
(unsafe-buffer error), **no `AGENT_BROWSER_PIN_TAB`** (pinned, a fresh daemon opens a stray tab of its own; every
call switches to its `targetId` first anyway). Binary installed with `scripts/fetch-agent-browser.sh <Helpers>`
(npm download path tested, ad-hoc signed); dev override `SHUTTLE_AGENT_BROWSER=<binary>`. Pieces:
`agent_browser_sidecar.{h,cc}` (`Sidecar`), `page_tools.{h,cc}` (`PageToolManager`), `page_tools_data.inc`
(generated), hooks in `shuttle_service.cc` and `session_manager.{h,cc}`; tooling: `config/agent-browser-tools.json`,
`scripts/gen-page-tools.mjs`, `scripts/fetch-agent-browser.sh`, `scripts/ab-mcp-client.mjs` (stdio client for
measurements). Not yet: Linux, release signing, `find`/`hover`/`dblclick` and `state-load` with real cookies untested,
token benchmark vs v1 + Playwright.

**Measured (agent-browser 0.38.2, darwin-arm64, headless test browser):**

- `agent-browser mcp --tools <profiles>`: newline-delimited JSON-RPC, protocol 2025-11-25. `core` = 29 tools
  (63 KB), `all` = 156 tools (328 KB; the older "64 tools / 145 KB" figure was a different count). Core names:
  `snapshot click fill type press check uncheck select scroll wait_ms wait_for_selector wait_for_text wait_for_load
  screenshot get_text get_url get_title eval close back forward reload tab_new tab_list tab_switch tab_close open read
  tools_profiles`. Not in core: `console errors` (profile `debug`), `state_save/load/list` (`state`), `wait_for_url`,
  `hover`, `dblclick`, `find`... (only in `all`; we start with `--tools all` and list only ours).
- 13 config props on every tool (`allowedDomains caCert clearCaCert extraArgs idleTimeout namespace restore*
  session timeoutMs`); `extraArgs` is an escape hatch to any CLI flag, so we whitelist args per tool.
- **`--cdp` via env**, no CLI flags needed: `AGENT_BROWSER_CDP=<ws session endpoint>`, `AGENT_BROWSER_SESSION=<sid>`,
  (`AGENT_BROWSER_PIN_TAB=1` tested but dropped, see status), `AGENT_BROWSER_SOCKET_DIR`, `AGENT_BROWSER_SCREENSHOT_DIR`,
  `AGENT_BROWSER_IDLE_TIMEOUT_MS`. Connects to the session endpoint fine.
- **Targeting a tab:** the session is bound to one tab at a time; a fresh sidecar is bound to none
  (`tab_list` shows `active:false`). `tab_switch {tab:<CDP targetId>}` binds (also with pin-tab) and returns the
  title. So `targetId` -> `tab_switch` before the call when it differs from the last bound tab. Calls must be
  serialized per sidecar.
- **Isolation holds** against the session endpoint: `tab_list` shows only session tabs; `tab_switch` to another
  session's targetId -> "No tab with target id ..."; `tab_new` creates a leased session tab; `close` does not close
  tabs or the session (the Browser.close filter is not even reached).
- **Daemon:** every sidecar starts a per-session background daemon (`agent-browser-darwin-arm64`, own process,
  unix socket in `AGENT_BROWSER_SOCKET_DIR`) that **outlives the MCP process**. Fix: send the `close` tool before
  killing (stops the daemon, verified), plus `AGENT_BROWSER_IDLE_TIMEOUT_MS` as fallback (daemon exited ~3 s after
  the configured idle time, verified with 3000). Mac socket paths max ~100 chars: keep the dir short.
- **Timing:** spawn -> initialize 30-40 ms; first call (daemon start + CDP connect) ~490 ms; later calls 25-50 ms;
  `back`/`forward` ~530 ms, `wait_for_text` as long as the page takes.
- **Output:** results are `content[0].text` plus a large `structuredContent` (envelope with a `lifecycle` block of
  ~500 B on each call). Text is plain for `snapshot get_text eval get_url get_title back forward reload`;
  other tools return a JSON envelope `{"data":{...,"lifecycle":{...}},"error":null,"success":true}`. We drop
  `structuredContent` and `lifecycle`: `click` -> `{"clicked":"@e3"}`, no data -> `ok`. `screenshot` returns the saved
  file path text + an inline `image/png` part (we keep the image only; ~15 KB b64 for a small page).
  Snapshot: `- textbox "Name" [ref=e6]`, refs used as `@e6`. Errors: `isError:true`, text like `Unknown ref: e99`,
  `Wait timed out after 25000ms` (default wait 25 s; `wait_for_selector` needs a *visible* element).
  `console` -> `{"messages":[{"type":"log","text":..,"args":[..]}]}`, `errors` -> `{"errors":[]}`.
- `state_save` writes to any `path` (we map `name` to `<profile>/shuttle/states/<agent>/<name>.json`);
  output `{"cookies":[],"origins":[]}`. **Not verified with real cookies** (needs an http page).
- Binary: npm tarball ships `bin/agent-browser-darwin-arm64` (15.6 MB, arm64 only; also x64, linux x64/arm64/musl,
  win32). Apache-2.0, bundled unmodified with LICENSE + NOTICE.

**Design as built:**

- `tools/list` = v1 tools + 24 page tools (**7.6 KB / ~1.9k tokens** vs 63 KB core): `snapshot click fill type press
  select check uncheck scroll wait-for eval get-text screenshot back forward reload console errors state-save
  state-load hover dblclick find open`. agent-browser names with **kebab-case** like the v1 tools (only our
  proxy maps names; `open` = navigate + wait for load; `wait-for` merges its 5 `wait_*` tools: first of
  text/selector/url/load/ms present picks the variant); every tool takes a required `targetId` (leased tab) and
  no `sessionId`: the session is derived from the tab. Hidden: open/navigation (use `navigate-tab`), tabs, close,
  profile, auth, launch, `path`/`extraArgs`/`human`/`newTab`, downloads/uploads. Definitions live in
  `config/agent-browser-tools.json`; `scripts/gen-page-tools.mjs` compiles them into `page_tools_data.inc`, a C++
  raw string (not a .grd resource: no pak plumbing, parsed once; regenerate and commit with the config).
- `PageToolManager` (owned by `ShuttleService`) keeps one `Sidecar` per session, created on the first page-tool
  call with `--cdp ws://<Host>/devtools/session/<sid>`. Args are whitelisted against the tool's schema and
  mapped (`map`, `fixed`, `special: state_path`); unknown args are an error.
- The request path was already async (`ToolCallback`/`HttpResponseCallback`, as for `capture-tab`); page tools
  just reply from the sidecar's response. `Sidecar` (UI thread) queues jobs, one at a time: `[tab_switch]` +
  `tools/call`; spawn/stdin writes/kill on a `BLOCK_SHUTDOWN` blocking sequence (`base::SequenceBound<Io>`),
  stdout read by a separate blocking task posting lines back to the UI thread. Per-job timeout (60 s, or wait +
  15 s; 20 s for the cold start): on expiry the process is killed and the next call restarts it.
- `SessionManager::SetSessionClosedCallback` -> `close-session`/agent deletion call `Sidecar::Close` (`close` tool,
  3 s, then kill); `Shutdown()` kills all immediately (daemon idle timeout 60 s as fallback).
- Binary: `$SHUTTLE_AGENT_BROWSER`, else `<app>/Contents/Helpers/agent-browser` (macOS) or next to the
  executable (Linux, later). `scripts/fetch-agent-browser.sh <Helpers-dir>` installs the pinned version + LICENSE +
  NOTICE and ad-hoc signs it; release signing with the app is still to do.
- Server instructions mention the page tools flow; still to measure: tokens per task vs v1 + Playwright.

### 3. Native port of the hot path (later, data-driven)

Port the tools phase 2 shows agents actually use (expected: `snapshot`, `click`, `fill`, `press`,
`wait-for`, `eval`) to C++: snapshot from the browser's own accessibility tree, input over an in-process
DevTools session, same names/refs/output so agents notice nothing. The sidecar keeps the long tail.

## Decided (2026-10-08)

1. Browser-wide CDP endpoint stays open (tooling, DevTools, smoke scripts); agents are told to use their
   session endpoint. Gating revisited later.
2. Sidecar ships **Mac first**; Linux after the trial confirms the tool set.
3. Overlapping agent-browser features: **expose some** (e.g. `state save/load` as advanced tools); profile,
   auth vault, launch/close stay hidden. Exact list decided with the phase 2 tool set.
4. Phase 1 started: fork branch `ac/session-cdp`.
