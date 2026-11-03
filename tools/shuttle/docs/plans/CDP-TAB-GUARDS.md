# Plan: guards against leftover agent tabs and sessions (backlog item 3)

Status: agreed with Toby (2026-11-03: popups stay with the session; user tab switches count). Implemented on
`sh/cdp-tab-guards` (not built yet). BOX-Lead answered (2026-11-03, see below). Defaults agreed with Toby (2026-11-02):
30 min idle timeout, no tab cap yet.

## Problem

Agents leave tabs and sessions behind. A CDP client (Playwright `connectOverCDP`) creates tabs, then crashes or
disconnects without closing them. MCP sessions have no lifetime: an agent that never calls `close-session` keeps its
session, its tabs (unthrottled while leased) and its agent-browser sidecar until the browser quits. MCP is stateless
HTTP, so there's no connection whose end we could watch.

## Design

### 1. Tabs a CDP connection created close with that connection

- Scope: session endpoints only (`/devtools/session/<id>` and page connections to leased tabs, i.e. every
  `SessionCdpFilter`). The browser-wide endpoint keeps upstream behaviour (tooling, DevTools).
- `SessionCdpFilter::CreateTarget` records the target ids it allocated. When the connection ends (the filter is
  destroyed), a posted task releases and closes those of them still leased to the same session.
- Not closed: tabs from MCP `allocate-tab`, tabs the user handed to the agent, tabs another connection created, and
  popups the page opened. Popups join the session through `OnTabOpened`; they follow their opener only if the
  opener was CDP-created. Open question: should they?
- The session itself stays open. Ending it is `Browser.close` / `close-session` / the idle timeout.
- The agent-browser sidecar connects to the session endpoint too. It never creates targets (it switches between
  existing tabs), so killing or restarting it closes nothing. Verify in the probe.

### 2. Idle timeout: 30 min, per session

- A session is closed (`SessionManager::Close`: tabs closed, cookies flushed, sidecar stopped) after 30 min without
  activity.
- Activity = any MCP tool call naming the session or one of its tabs (incl. page tools), any client message on one
  of its CDP connections, and the user focusing one of its tabs (a person watching or taking over counts).
- An open CDP connection by itself doesn't count, only messages. Open question: should it? A connected but silent
  Playwright client is the hung-client case (2026-10-22), which is the thing to clean up.
- Settings:
  - browser-wide default by switch `--shuttle-session-idle-timeout=<minutes>` (0 = off), default 30;
  - per session: `open-session { idleTimeoutMinutes }` (0 = off), for long human handoffs.
- One repeating timer in `SessionManager` (every minute) checks `last_activity` per session. Closing logs one line
  with the session id and the idle time.
- Visibility: `list-sessions` and `chrome://agents` show `lastActivityAt`. Optional: `idleClosesAt`.

### 3. `open-session` warns when the agent already has open sessions

- The result gains `warning` ("agent X already has N open sessions …") and `openSessions` (id, openedAt,
  lastActivityAt, tab count). The new session still opens, because several sessions per agent are allowed.
- The tool description tells agents to reuse or close sessions.

### BOX-Lead's flows (2026-11-03)

- Box core: `open-session` → `allocate-tab` → `connectOverCDP(cdpEndpoint)`, then `ctx.pages()[0]` (the allocated
  tab) or `ctx.newPage()` (Target.createTarget) for more tabs. The leftover tabs (2026-10-10) came from repeated
  `newPage()`/`page.close()` cycles on one session endpoint. They never use the browser-wide endpoint.
- `detach(browser, { keepTab: true })` drops the CDP connection and leaves the session open so a person can finish
  a sign-in in the allocated tab. Rule 1 keeps that tab. No known flow needs a `newPage()` tab to outlive its
  connection. Document in MCP.md: "tabs you create over CDP close with the connection; use `allocate-tab` for a tab
  that must stay".
- The sign-in handoff can take much longer than 30 min, so `idleTimeoutMinutes` on `open-session` (0 = off) is needed.
  The Box passes it only for keepTab leases; its own sweep closes kept sessions older than a day. Everything else
  uses the 30 min default.
- The Box only knows about keepTab at detach time, so add `update-session { sessionId, idleTimeoutMinutes }`
  (0 = off). `detach` calls it before dropping the connection.
- Keep guaranteed: the `allocate-tab` tab is in `ctx.pages()` at connect time (redditLogin uses
  `ctx.pages()[0] ?? ctx.newPage()`). The probe checks it.
- Tab switches by a person count as activity (BOX-Lead agrees).

### Not now

- Tab cap per session (Toby: not yet).
- The browser-wide endpoint: closing tabs on disconnect there would break DevTools and tooling.

## Code

- `session_manager.{h,cc}`: `Session` gets `last_activity`, `idle_timeout`; `Touch(session_id)`; the idle timer;
  `OpenSessionsOf(agent)`.
- `session_cdp_filter.{h,cc}`: `created_` target ids; close them on destruction; `Touch` on client messages.
- `mcp_tools.cc`: `open-session` args and warning; `Touch` in tool dispatch (session from args or from the tab);
  `list-sessions` / `DescribeSession` field.
- Focus: a `TabStripModelObserver` activation hook (SessionManager already observes the strips).
- Own code only (chrome/browser/shuttle), no upstream touch expected. Not a numbered fork commit unless an upstream
  file is touched; it gets SH-Review either way.

## Probe

Extend `smoke-session-cdp.mjs` (or a new `smoke-tab-guards.mjs`):
- A session-endpoint connection creates a tab, disconnects, and the tab is gone. A tab from `allocate-tab` in the
  same session stays.
- A sidecar-style connection without createTarget closes nothing.
- Idle timeout with a test override (`idleTimeoutMinutes` doesn't go below 1, so a hidden test switch in seconds, or
  run the check only in a long mode): the session closes; MCP activity resets the timer.
- `update-session` changes the timeout; `idleTimeoutMinutes: 0` keeps an idle session open.
- An `allocate-tab` tab is listed by a fresh session-endpoint connection (Playwright `pages()`).
- `open-session` twice for one agent: the second result has `warning` and lists the first session.
- MCP.md: replace "Open: empty sessions stay open … no idle timeout yet".
