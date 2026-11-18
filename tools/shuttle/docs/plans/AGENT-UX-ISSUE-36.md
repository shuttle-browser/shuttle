# Agent management UX from issue #36 (design, 2026-11-18)

Status: implemented as fork commit 63 on `sh/agent-ux` (2026-11-18; smoke 18/18; cleanup flow and page reviewed by
the maintainer). Changes against this plan: `delete-idle-agents` pins a preview with `ids` (no server-side
`previewId`); chrome://agents was redesigned to the Settings look (full-width list like chrome://settings/content,
expandable rows with tabs and favicons, search and sort in the toolbar, Help dialog for Quick start, toolbar "New
agent" + ⋮ menu with Task Manager / Clean up, totals footer). The five items were proposed by an outside contributor
in issue #36 (Jeremiah Church, branch `JeremiahChurch/shuttle sh/agent-ux-fixes`, e3d9838ce7). We first built our
own version locally to inform the reply; the maintainer then decided to submit it as one PR crediting them
(`Co-authored-by`, PR text links their commits) and to ask for their review. Review of their branch: SH-Review,
private `_docs/reviews/REVIEW-issue-36.md` (B = blocker, S = should-fix below).

Branch: `sh/agent-ux` = `sh/fix-zero-suggest-factory` (fork commit 62, its own PR: the "+" crash) + fork commit 63 +
this docs commit. Exercise history (their commits merged underneath, per-step commits): local `sh/agent-ux-36`.

## 1. Agents don't switch the tab the user is looking at (their b5f2a90688)

Take as is. `SessionManager::SelectTab` (CDP `Page.bringToFront` / `Target.activateTarget`) already refused while the
window is active; now it also refuses when the shown tab is leased to another session. A user tab or one of the same
session is still replaced. Verify: two sessions, user watches session A's tab with the window inactive, session B
calls `bringToFront` on its tab: the shown tab stays.

## 2. Last active, keep, sort (their 536cd3b8bc, fixed)

- `last_active` per persistent agent in `shuttle.profiles` (as theirs), shown on chrome://agents ("Active 3 days ago",
  "Active now" with an open session), in `list-agents` as `lastActiveAt`.
- **B1 fix**: write it while a session is in use, not only at open/close. `SessionManager::Touch` calls
  `ProfileRegistry::RecordActivity` (already throttled to 1/min per agent, so a busy agent costs one pref write a
  minute). `SessionManager` destructor / `ShuttleService::Shutdown` records the last activity of every open session.
  A crash loses at most a minute.
- Migration: agents without `last_active` get "now" at first start (conservative: nothing looks old right after the
  upgrade). The cleanup dialog says so.
- `keep`: a flag on persistent agents (`update-agent { keep: true }`, a toggle in the agent's edit dialog), "Kept"
  badge on the card. Only cleanup honors it; `delete-agent` still deletes.
- Sort on chrome://agents: Name, Recent, Memory, CPU (Memory/CPU from item 5). Shared Chromium select style
  (`md-select`, `md_select_lit.css`), choice kept in localStorage.

## 3. Clean up idle agents (their 536cd3b8bc, reworked)

Scope: persistent agents (throwaway agents expire on their own).

- Eligible: persistent, not kept, no open session, `last_active` older than N days (1-365; 0 is refused: "idle for no
  time" means everything without a session, use `delete-agent` for that).
- Two steps, never one query: **preview** returns the candidates (ids, names, last active) and a `previewId`;
  **confirm** takes the `previewId` and deletes only those ids, each re-checked at that moment (still eligible:
  not kept, no session, still idle). A candidate that stopped qualifying is skipped and reported. The set never
  grows at confirm (B3). Previews expire after 10 minutes and are kept in memory only.
- MCP: `delete-idle-agents { olderThanDays, exclude?: ["glob"], dryRun? }` stays as the agent-facing tool: `dryRun:
  true` = preview, otherwise preview + delete in one call (an agent can't sit on a dialog). `dryRun` present and not
  a boolean = error (B2); same strict check for `olderThanDays` (integer in range) and `exclude` (array of strings).
- chrome://agents: "Clean up" opens a dialog: days input with visible validation (S1), the candidate list from the
  latest preview (stale responses dropped by a request counter), "Delete N agents" disabled until the current preview
  is in. Confirm sends the `previewId`.
- Docs: `tools/shuttle/docs/MCP.md` gets `delete-idle-agents`, `keep`, `lastActiveAt`, the eligibility rules, the
  preview/confirm contract and the migration note (S5).

## 4. New tab in an agent (their d73cf0ba06, fixed)

- "+" while an agent's tab is shown opens a tab for that agent (its storage, its latest session, in its group).
  Ctrl/Cmd+T stays a plain new tab. Right-click on "+": "New tab" + "New tab in <agent>" for agents with open
  sessions (shown agent first), the rest under "New tab in another agent".
- **S2 fix**: no interception in the generic `AddAndReturnTabAt`. Instead the two group commands of the tab context
  menu ("New tab to the right", "New tab in group") check for an agent group in their own handlers and call
  `ShuttleService::OpenNewTabInAgent(agent_id, {browser, index, foreground})`, which returns the created
  `WebContents*` and inserts at the requested index in the given browser.
- Blank agent tab URL: the same new-tab URL as a user tab. The zero-suggest crash this exposed is fixed by fork
  commit 62.
- Remove the now-unused `AddNewTabInGroupItem`.

## 5. Per-agent resource usage (replaces their Browser card)

### Attribution
Agents are storage partitions of the one profile ("shuttle-<id>"). Chromium never puts two storage partitions in one
renderer process, so every renderer process (main frames, out-of-process iframes, dedicated/shared/service workers)
belongs to exactly one agent or to the user's default storage. Per-agent usage is a sum over whole processes, measured,
not estimated.

- `RenderProcessHost::FromID(task's child process id)->GetStoragePartition()->GetConfig().partition_domain()` ->
  `shuttle-<id>` -> agent id. Default partition -> "You". Everything else (browser, GPU, network, utility, spare
  renderer) -> "Browser (shared)".
- Memory footprint and CPU are per process: count each PID once. Network is per task: sum the tasks.

### `AgentUsageService` (`chrome/browser/shuttle/agent_usage.{h,cc}`)
- Owned by `ShuttleService`. A `TaskManagerObserver` (refresh: memory footprint, CPU, network; 2 s interval) that
  publishes `{agent id -> memory, cpu, network, processes, tabs}` plus "You" and "Browser (shared)" after each refresh.
- Runs only on demand: `AcquireUsage()` returns a handle; sampling stops when the last handle goes (fixes S3). Holders:
  the Task Manager's Agents tab, a visible chrome://agents page (Page Visibility: hidden releases), a pending MCP call.

### Task Manager: "Agents" tab
- A fourth tab next to "Tabs & extensions", "Browser", "All tasks". One row per agent, "You", "Browser (shared)";
  columns Agent (avatar + color + name), Memory footprint, CPU, Network, Processes, Tabs; sortable like the other tabs
  (`ui::TableModel` sorting).
- Implemented as a second table model (`AgentUsageTableModel`, `chrome/browser/ui/task_manager/`) that
  `TaskManagerView` swaps into its `TableView` when the Agents tab is selected; the task model and its column settings
  stay untouched. The process grouper is off on this tab. "End process" becomes "Close sessions" for an agent row
  (closes the agent's sessions, keeps its storage), disabled for "You" and "Browser (shared)".
- Double-click an agent row: "All tasks" filtered to that agent (search matches the agent name on agent tasks).
- Saved tab (local state) accepts the new category; `chrome://agents` link opens the Task Manager on it.

### chrome://agents
- No Browser card. Each agent card shows its memory and CPU while sampling (secondary line, after sessions/tabs).
- Sort by Memory / CPU uses the same numbers; until the first sample (2 s) the order falls back to Name.
- Header action "Task Manager" opens the Task Manager on the Agents tab.

### MCP (small)
- `list-agents { usage?: true }` adds `memoryBytes`, `cpuPercent`, `processes` per agent (waits for one sample, at
  most ~3 s). Without `usage` nothing changes (no sampling cost). Documented in MCP.md.

## Order and estimate
1. Branch + zero-suggest fix + their commits (done when the preview builds and "+" no longer crashes).
2. Item 1 check, item 2 fixes, item 3 rework, item 4 fixes (one commit each).
3. Item 5: service, Task Manager tab, chrome://agents, MCP (one commit each).
4. One Mac build after each step, your browser pass, SH-Review over our commits, then the issue reply.

Conventionally about 6-8 engineering days; for us about 1 day of sessions. On top, not sped up: your browser passes
(the Task Manager tab and the cleanup dialog), Mac builds (each ~1-10 min incremental), SH-Review.

## Decided (maintainer, 2026-11-18)
- Submit as one PR (stacked on fork 62's PR), credited to the issue's author with `Co-authored-by`; ask them for
  review and whether they agree with us opening it. Contributors list in the README and on the website.
- Task Manager agent rows: "Close sessions" (frees the agent's processes, keeps its storage).
- `delete-idle-agents` without `dryRun`: one call previews and deletes (the set is fixed inside the call).
