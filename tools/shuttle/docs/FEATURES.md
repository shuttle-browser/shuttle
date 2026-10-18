# Feature ledger

What the fork **adds or changes in behavior** on top of Chromium (removals: `REMOVAL-LOG.md`). One row per
feature; keep it current when a fork commit adds, changes or drops a feature. Columns:

- **#**: fork commit number(s) (`REMOVAL-LOG.md` "Fork commits"); **SHA**: commit on `main` that landed it.
- **Upstream touch**: upstream files modified when it landed (excl. `tools/shuttle/`), top areas. These are the
  merge-conflict surface on every upstream merge.
- **Verified by**: the smoke probe (`scripts/smoke-all.sh`) that fails if the feature breaks; `manual` = no probe yet.

Own code lives in `chrome/browser/shuttle/` (MCP server, sessions, agents, page tools, network mode) and
`chrome/browser/resources/agents/` + its WebUI controller (`chrome://agents`).

| # | Feature | SHA | Upstream touch | Verified by | Docs |
| --- | --- | --- | --- | --- | --- |
| 1 | No first-run / default-browser prompt | 452754aa95 | 1: chrome_main_delegate.cc | smoke-cdp (no dialog) | |
| 16 | Always-on CDP: no approval dialog, no automation infobar, `navigator.webdriver` false, policy removed | 48370d6639 | 17: chrome/browser/devtools 8, ui 3, blink 1 | check-automation (opt-in), smoke-cdp | TOOLING |
| 20 | In-browser MCP server `POST /mcp` (agents = storage partitions, sessions, leased tabs); popups keep the opener's partition | 007d52a391 | 12 (+12 new): devtools 3, content devtools 4 | smoke-mcp phase1/2 | MCP.md |
| 21 | Agent identity: name, color, avatar; one tab group per agent, two-way sync, adopt/transfer dragged tabs | a1b652b125 | 8 (own code at the time) | smoke-mcp | MCP.md |
| 22 | No startup window (first agent tab opens it); keep-alive without a window | 00b32091df | 2: devtools, chrome_browser_main.cc | smoke-cdp, close-run1/2 | |
| 23 | `chrome://agents`: agents, sessions, tabs (create/edit/delete, avatar, open/jump) | 707fa803f6 | 9 (+9 new): ui 2, ui/webui/resources 1 | smoke-agents-page | |
| 24 | Per-session CDP endpoints (only the session's tabs + agent storage), agent viewports, CSS-px capture | 58b6b928ab | 12 (+2): content/public/browser 2, devtools 2 | smoke-session-cdp, smoke-mcp | MCP.md |
| 25 | Tab groups are agents: no manual groups, hand/take back tabs, agent bubble, "agent" strings | 01fffd656f | 24: chrome/browser/ui 12, resources 3 | manual (UI) | |
| 26 | Page tools via the agent-browser sidecar (24 tools behind `/mcp`); avatar on the group chip | 9330681592 | 10 (+7): ui 3, resources 1 | manual (needs a signed build; open) | MCP.md |
| 27 | Rebrand to Shuttle (bundle id, internal ids `shuttle`, strings, logos, Linux names) | 8b80db862f | 526: chrome/app/resources 81, ui 33 (strings/assets; `scripts/rebrand.py` re-applies) | smoke-cdp, rebrand.py check | BRANDING.md |
| 28 | Mac release: signing + DMG pipeline, official-build links | 21603b7944 | 2 (+29): framework.order/.exports, installer/mac restored | release-mac.sh | TOOLING |
| 29 | `chrome://agents` is home (Dock reopen), Quick start card with the MCP URL | 3e5d8913b8 | 6: resources 4, app_controller_mac.mm | smoke-agents-page | |
| 32 | Network mode: share one Shuttle across machines (`--shuttle-listen`, access token) | 571c3a4668 | 8 (+2): content devtools 2 | smoke-network (opt-in) | README |
| 36 | Fix: worker use-after-free on session CDP clients | 84718de00b | 4: content devtools 2 | manual (repro open) | REMOVAL-LOG |
| 37 | ImprovedStartupBestEffortDelay on (no 3-min startup fence without a window) | 12988c4c14 | 1: chrome_features.cc | manual | REMOVAL-LOG |
| 38-39 | Linux menu launch opens / reuses `chrome://agents` (`--shuttle-open-agents`) | 963fabcc82, 76e495c1ef | 6 (+1): ui 3, chrome_browser_main.cc | manual (Linux) | LINUX.md |
| 40-42 | Linux/WSL desktop: person launch gets `chrome://agents`; WSL own frame + dark mode; keep running while a session is open | 46479f1e55, b966481b3c, 9586a23162 | 9: ui 3, ui/linux dark_mode 2 | manual (Linux) | LINUX.md |
| 44 | Empty binders for BadgeService / on-device speech (no renderer kill) | bfd55b20e4 | 2: browser_interface_binders.cc, blink 1 | manual (probe open) | REMOVAL-LOG |
| 45 | Agents can't steal focus (no activation on bringToFront/activateTarget) | 9976b54bf0 | 5 (own code) | smoke-focus | REMOVAL-LOG |

Open (decided, not landed): session-cookie persistence for agent partitions (persist + restore, default on,
per-agent opt-out; Toby 2026-10-18).
