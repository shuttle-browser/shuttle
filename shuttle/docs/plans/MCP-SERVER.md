# MCP server and session model

Decided 2026-10-05. Reference for intent: chromatrix (`~/projects/ai/chromatrix`, docs in
`apps/docs/src/content/docs/{identities,tabs,agents,mcp-tools}.md`). Shuttle eventually
replaces chromatrix's role but is not combined with it; chromatrix is the reference, not a
dependency.

## Goals

- One application: the MCP server runs **inside the browser** (C++, own directory such as
  `chrome/browser/shuttle/`, so upstream merges rarely touch it). No sidecar process.
- MCP's primary job is managing **profiles and tabs**. Agents drive their tabs over CDP
  (Playwright, agent-browser). MCP provisions; CDP browses (same split as chromatrix).
- Many agents share one Shuttle instance.
- **One window.** Everything is a tab; tab groups make sessions visible.
- Fully agent-controlled: user-facing UI/UX can go (profile picker, profile menu, avatar button,
  multi-profile and guest UI, sign-in).
- No incognito anywhere, including CDP.

## Model

| Term | Meaning | Chromium mechanism |
| --- | --- | --- |
| **Profile** | Named, persistent storage (cookies, logins, local storage). Id is a kebab slug (`^[a-z0-9]+(-[a-z0-9]+)*$`, max 64). Created, listed and deleted by agents over MCP. | One persistent, non-default `StoragePartition` inside the single Chrome profile. |
| **Agent** | An MCP/CDP client, identified by agent id. | Per-agent credential (derived token, as chromatrix: `HMAC(token, profile ‖ agentId)`). |
| **Session** | One agent working in one profile. | One **tab group**, label `<profile> · <agent>`, colour per profile. |
| **Tab** | Leased to exactly one session. | `WebContents` created with `SiteInstance::CreateForFixedStoragePartition`. |

- One Chrome profile only. Profile-wide state (content settings, history, autofill, passwords,
  downloads, extensions) is shared across profiles; accepted, none of it matters for agents.
- Several sessions may share a profile: shared context, exclusive tab leasing (chromatrix's proven
  concurrency model). Shared storage is last-writer-wins.
- Profile lifecycle is create / list / delete. No start/stop: a partition loads when its first tab
  opens. Delete closes the profile's tabs and wipes the partition.

## CDP

- Per-agent CDP endpoint built into the browser: the agent sees and attaches only its own
  session's tabs (replaces chromatrix's mux ACL).
- CDP browser contexts map to **profiles**: `browserContextId` = profile id.
  `Target.createBrowserContext` never creates an off-the-record profile (rejected, or creates a
  named persistent profile). `DevToolsBrowserContextManager`'s OTR path goes away.
- Must keep Playwright `connectOverCDP` working (`contexts()`, `pages()`, `newPage()`, detach and
  reconnect). Puppeteer is a non-goal, as in chromatrix.

## MCP surface (first cut, modelled on chromatrix `mcp-tools.md`)

- Profiles: `create-profile`, `list-profiles`, `delete-profile`.
- Sessions/tabs: `open-session` (agent + profile → tab group + scoped CDP URL), `allocate-tab`,
  `navigate-tab`, `release-tab`, `close-session`, `list-sessions`, `capture-tab`.
- Transport: streamable HTTP on localhost, one access token (Bearer), gating `tools/list` too.

## Spike results (2026-10-05, branch `ac/spike-partitions`, commit 70b7cc7cba)

Throwaway hook: CDP `Target.createTarget` with `browserContextId: "ac:<name>[:cap][:WxH]"` opens
a normal tab whose `WebContents` uses `SiteInstance::CreateForFixedStoragePartition` with
`StoragePartitionConfig::Create(profile, "<domain>", <name>, /*in_memory=*/false)`. Test
scripts: `spikes/partitions/*.mjs` (`server.mjs` on :8765, browser on CDP :9333).

| Question | Result |
| --- | --- |
| Isolation | Cookies and localStorage fully separate between `ac:alpha`, `ac:beta` and the default partition, all tabs in one window. On disk: `Default/Storage/ext/<domain>/<hash>/`. |
| Navigation in a tab | Stays in the partition (content keeps `IsFixedStoragePartition` across BrowsingInstance swaps). |
| `window.open` with opener | Inherits (same SiteInstance). No patch needed. |
| `noopener` / `target=_blank` | **Leaked to the default partition upstream.** Patched in `WebContentsImpl::CreateNewWindow`: a fixed-partition opener creates the new window with `CreateForFixedStoragePartition`. Inherits after patch. |
| Cmd-click / middle-click / browser-opened tabs | **Leaked upstream** (`CreateTargetContents` uses `tab_util::GetSiteInstanceForNewTab`). Patched in `browser_navigator.cc`: a non-default `source_site_instance` partition is kept. Inherits after patch. |
| Partitioned tabs as normal tabs | No CHECKs or errors: tab helpers, omnibox, DevTools attach, screenshots all fine. Upstream has no non-test caller of `CreateForFixedStoragePartition`, so we own this path. |
| Background throttling | Plain background tab: `visibilityState=hidden`, 0 rAF, ~4 timers/s, `outerWidth/Height = 0`. With `IncrementCapturerCount(stay_hidden=false)`: `visible`, 120 fps rAF, unthrottled timers, same as foreground. **Agent tabs should hold a capture handle while leased.** |
| Screenshots | `Page.captureScreenshot` works for foreground, hidden and captured-hidden tabs (~50 ms). |
| Per-tab viewport | `WebContents::Resize()` on a hidden tab gives it its own size, including phone sizes below Chrome's 500 px window minimum (390x844 worked, screenshot 780x1688 @2x). Caveats: `outerWidth/outerHeight` still report the window (detectable mismatch; fork should derive them from the tab size); activating the tab snaps it to the window size and it keeps that size after deactivation (MCP must re-apply on hide). `screen.*` is the real display. |
| Playwright `connectOverCDP` | One context (default); partition tabs appear as its pages. Page-level APIs work (`evaluate`, `goto`, `click`, popups stay in the partition). Context-level APIs hit the default partition: `context.cookies()` returns default cookies, `context.newPage()` opens in default. The per-agent CDP endpoint must map the context to the session's profile (`Storage.*` cookies, `Target.createTarget`). |
| Persistence | Partition data survives restart. Cookies set right before `Browser.close` are lost in **every** partition, including default (no shutdown flush on that path). MCP must `FlushCookieStore` on profile/session close and shutdown. |
| Session restore | Restored no tabs in the test; agent sessions should not rely on it (and needn't: profiles persist, sessions are re-opened by agents). |

Other observations: a GCM registration request goes out at startup (`DEPRECATED_ENDPOINT`), a
removal candidate (push messaging/GCM). `GetLastUsedProfile()` from a CDP command before the
profile has loaded hits a blocking DCHECK; the real server must start after profile init.

Decision: the design holds. Production work needs (1) the two inheritance patches, (2) a
profile/session service owning partitions, tab groups, capture handles and per-tab sizes,
(3) the per-agent CDP endpoint with context mapping, (4) outer-size spoof-free consistency for
resized tabs.

## v1 decisions (2026-10-08)

- **Endpoint:** same port as CDP, path `/mcp` (9222 or the port in `DevToolsActivePort`). A small
  hook in `content/browser/devtools/devtools_http_handler.cc` (which 404s unknown paths today)
  routes `/mcp` to `chrome/browser/shuttle/`. Its `net::HttpServer` sends whole responses
  only, so MCP over HTTP returns plain JSON responses, with no SSE.
- **CDP scoping later:** v1 manages profiles, sessions and tabs over MCP. Agents use the shared
  CDP endpoint and attach by targetId, cooperatively. The per-agent scoped CDP endpoint (filtering
  per connection, mapping `browserContextId` to the profile) is v2.
- **Auth:** none in v1; localhost only, the same trust model as the CDP port (Host check). A
  Bearer token comes later.
- **Dashboard `chrome://agents`** (fork branch `ac/agents-page`: `b94a87cbfe` page + `b342e2436a`
  Settings-style redesign, 2026-10-08; not on `main` yet): Lit WebUI in
  `chrome/browser/resources/agents` + `chrome/browser/ui/webui/agents`, built from cr elements
  (cr-toolbar with search, Settings cards, cr-action-menu Edit/Delete, cr-dialog, cr-input), cr vars only
  (no custom brand colors, no colored left borders, Chromium Settings look). Lists agents with
  avatar, sessions and tabs (refresh every 2 s while visible); create (id follows the name), edit
  name/color/avatar, delete (confirm dialog, storage wiped), end session, click a tab to bring it to the
  front, **Open tab** = "Go to Profile" (foreground tab in the agent's storage + group, e.g. to sign in;
  `SessionManager::OpenTabForUser`). The page calls the MCP tools via `chrome.send('callTool')`
  (allowlisted in `build_webui.gni`'s `legacy_chrome_send_users`; Mojo would allow push updates).
  `scripts/smoke-agents-page.mjs` 14/14 on Mac; Linux: `b94a87cbfe` green + smokes, redesign check
  paused. Open: entry point (tab group menu / app menu "Manage agents"), a design sign-off.
- **Agent identity (later, 2026-10-08, after first live MCP test):** the tab group label
  `steve · Steve` (profile · agent) is a placeholder. Re-purpose the tab group feature as the
  agent's visible identity. **Agent = profile** (the terms are interchangeable; one entity), with
  its storage plus an **ID**, a
  **Friendly Name** (the group label), a **Color** (the group color) and an optional
  **Avatar/Image** (shown on the group chip). Managed in `chrome://agents` and
  via MCP tools.
  - **Two-way sync:** editing the group's name or color in the tab strip writes back to the
    profile (observe tab group visual-data changes). `open-session` then just picks a profile;
    its free-text `agent` name goes away.
  - **One group per agent** : all tabs of an agent form one tab group, whichever session
    leased them; sessions still own their tabs (`close-session` closes only its own).
  - **Drag-in = adopt** : a tab the user moves into an agent's group becomes the agent's: leased to
    its latest session (or a new one) and reopened in the agent's storage (a page can't change storage;
    browser pages like the New Tab page become about:blank). Moving a tab from one agent's group to
    another's transfers it the same way.
  - **Status:** fork branch `ac/agent-identity` (`72c9342e41` identity, `b6cdbdddeb` group per agent,
    `9d65fbea15` adopt/transfer): checked labels, rename/recolor sync, drag-in adopt live. Not yet
    eyeballed: agent-to-agent transfer, emptied name restore. Open: avatar on the group chip (views work),
    "Go to Profile" (needs `chrome://agents`); default color is a fixed hash of the id (may collide).
  - **"Go to Profile"** entry on the tab group (context menu / group editor bubble) opens its
    record in `chrome://agents` (page exists on `ac/agents-page`; the menu entry doesn't yet).
- **Tab groups are agents (2026-10-08 evening; fork branch `ac/agent-groups` `74d613e3bb`, avatar chip on
  `ac/next` `4692376d2f` + `e274bca5bf`; Mac green, checked it live):** groups exist only as agents (made over
  MCP / chrome://agents), never by hand. Rename depth: **user-facing strings only** (C++ `TabGroup` ids stay for
  cheap merges; MCP tool descriptions still say "tab group").
  - Tab context menu: **"Hand tab to agent ▸"** (agents list; same adoption as drag-in, starts the agent's group in
    that window if needed: `SessionManager::HandTabToAgent`) and **"Take back from agent"** (page reopens in the
    user's storage as an ordinary tab next to it, the agent's tab closes: `TakeBackTab`; routed through
    `CommandRemoveFromGroup`). The UI reaches the impl only through `ShuttleService`'s public header
    (`ListAgents`, `AgentOfTab`, `AgentOfGroup`, `HandTabToAgent`, `TakeBackTab`, `AvatarOfGroup`).
  - **Agent bubble** (group header bubble): name + color (sync to the agent as before), "New tab for agent",
    "Manage agents" (opens chrome://agents; the old Home button), "Close agent tabs". Gone: ungroup, delete, move
    to new window, focus.
  - No group creation: "New tab group" (new-tab button menu), Ctrl/Cmd+Shift+P, Mac Tab menu "Group Tab",
    `GroupTab`/`GroupAllUngroupedTabs`/`CreateNewTabGroup` are no-ops; history "Open all in new tab group" opens
    plain tabs; tab/session restore brings tabs back **ungrouped** (CHECKs on missing groups turned into returns).
  - Strings: ~30 messages say "agent" (menus, bubble, a11y, reopen/restore); unused group strings remain until the
    next `prune-strings.py`.
  - **Avatar on the chip:** `AvatarCache` (data: parsed, https: fetched without credentials, decoded in the
    sandboxed data decoder, round 48 px) in the header's unused sync-icon slot, concentric in the chip start
    (inset = vertical gap, 5 DIP before the name). `RefreshGroups` always repaints (avatar changes).
  - chrome://agents: **avatar from a file** ("Choose image…": center crop, 256 px, PNG or JPEG data: URL, Remove
    button). Native file dialog not explicitly confirmed yet.
  - Known gaps: dragging a tab out of an agent's group leaves it leased but ungrouped (only "Take back" returns it;
    automating the drag-out risked replacing WebContents mid-drag).
- **No startup window (2026-10-08):** Shuttle starts without a window (no stray New Tab);
  the first agent tab, or the user (Dock click / Cmd-N), creates it. Fork commit 22
  (`00b32091df`): `--no-startup-window` is appended to the process's own command line unless it has URLs or
  `--restore-last-session` (a second launch still opens a window); the remote-debugging keep-alive no
  longer needs a `--remote-debugging-*` switch (not taken for headful macOS, where AppController keeps the
  app alive and releases on Quit). So the browser keeps running with no windows on all platforms, until `Browser.close` / quit: **accepted on Linux too** (2026-10-08: Shuttle is
  meant to be controlled via MCP). Smoke scripts create their own targets, unchanged.
- **Open bug:** an "Aw, Snap!" renderer crash in an agent tab (example.com, 2nd Alice session)
  during a visual test; not reproduced with logging (`--enable-logging=stderr`) on Mac or Linux.
- **Empty sessions (seen in the same test):** closing a session's tabs by hand leaves the session
  open with no tabs and no visible group (state stays consistent; next `allocate-tab` makes a new
  group). Show idle sessions in `chrome://agents`; consider an idle timeout.
- **Order:** (1) inheritance patches (branch `ac/mcp`, done) → (2) profile/session service in
  `chrome/browser/shuttle/` → (3) `/mcp` endpoint + tools → (4) scoped CDP → (5) dashboard.

## Effects on pruning

- B2 can remove profile picker/menu, avatar button, multi-profile and guest UI, alongside
  sign-in/sync.
- Keep tab groups (they are the agents); saved/synced tab groups can go.
- Keep `chrome/browser/actor` until studied as a base.
- Later decision: whether chromatrix's fidelity mitigations (`Runtime.enable` suppression) and
  remote takeover (screencast) move into the browser.
