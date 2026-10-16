# RAM benchmark: chrome-devtools-mcp vs Shuttle (2026-10-12)

Script: `scripts/bench-ram.mjs` (`node scripts/bench-ram.mjs cdm <n> <url>` /
`node scripts/bench-ram.mjs shuttle <n> <url> <Shuttle binary> <port>`).

## Setup

- Mac (Apple silicon), macOS 26. Metric: `phys_footprint` summed over the whole process tree (the value Activity Monitor
  shows as "Memory"), read with `footprint`.
- **chrome-devtools-mcp 1.10.1** (`npx -y chrome-devtools-mcp@1.10.1 --isolated --headless --no-usage-statistics`), one
  per agent, each launching its own **Google Chrome 155.0.8059.40**. That's the default per-client setup: the default
  persistent profile can only be used by one browser at a time, so parallel agents need `--isolated`. Usage statistics
  were off (the default turns them on and adds a ~15 MB watchdog process per instance, so CDM's real default is a little
  higher).
- **Shuttle 0.1.1** (signed release app), one browser, its built-in MCP server, one agent + session per client.
- Both headless. Two points per run, each after a 15 s settle:
  - **idle**: connected and ready. CDM after its first tool call (`list_pages`, which launches Chrome with its blank
    page); Shuttle with N agents and N open sessions, no tabs yet.
  - **page**: every agent opened https://en.wikipedia.org/wiki/Space_Shuttle and took an accessibility snapshot. CDM:
    `new_page` + `take_snapshot` (its initial blank tab stays open). Shuttle: `allocate-tab` + `snapshot` (page tools run
    on the agent-browser sidecar, which is included).

## Results

| Agents | Setup | Idle | 1 Wikipedia page each + snapshot |
| --- | --- | --- | --- |
| 3 | chrome-devtools-mcp ×3 + Chrome ×3 | 1.42 GB (24 processes) | **1.81 GB** (29-33 processes) |
| 3 | Shuttle (one browser, one MCP) | 0.09 GB (4 processes) | **0.55 GB** (12 processes) |
| 10 | chrome-devtools-mcp ×10 + Chrome ×10 | 4.70 GB (80 processes) | **5.96 GB** (110 processes) |
| 10 | Shuttle | 0.09 GB (4 processes) | **1.44 GB** (26 processes) |

3 agents: 3 CDM runs (1.80/1.81/1.83 GB page) and 2 Shuttle runs (550/553 MB); 10 agents: one run each.

**Shuttle uses ~3.3× less RAM with 3 agents and ~4.1× less with 10** (1.26 GB and 4.5 GB saved).

Where it goes:
- Each CDM instance is two Node processes before any browser: `npm exec` ~114-130 MB + `chrome-devtools-mcp`
  ~150 MB (≈ 270-285 MB). With 3 agents that's ~0.85 GB of Node alone.
- Each extra Chrome brings its own browser process (~62-69 MB), GPU/network/utility helpers (~40 MB each) and a blank
  tab's renderer.
- The page itself costs the same in both (a Wikipedia renderer is ~103-104 MB in Chrome and in Shuttle). Shuttle's
  per-agent cost is that renderer plus a ~7 MB agent-browser sidecar; everything else is shared.

## Browser vs MCP-server split (second round, 2026-10-12, used on the site's /why chart)

"MCP" = Node processes for chrome-devtools-mcp (`npm exec` + `chrome-devtools-mcp`), the agent-browser page-tools sidecars
for Shuttle (its MCP server itself runs inside the browser process). One run each, page state.

| Agents | chrome-devtools-mcp: browser / MCP / total | Shuttle: browser / MCP / total | Ratio |
| --- | --- | --- | --- |
| 1 | 337 MB (9) / 274 MB (2) / **611 MB** | 304 MB (7) / 7 MB (1) / **311 MB** | 2.0× |
| 3 | 1,019 MB (27) / 817 MB (6) / **1.84 GB** | 536 MB (9) / 21 MB (3) / **557 MB** | 3.3× |
| 10 | 3,399 MB (70) / 2,425 MB (20) / **5.82 GB** | 1,353 MB (16) / 72 MB (10) / **1.42 GB** | 4.1× |

(process counts in parentheses). The 10-agent CDM total varies between runs (5.82-5.96 GB).

## Caveats (say them on the comparison page)

- Headless; a headed window adds some memory to both.
- CDM keeps its initial blank tab open after `new_page` (its normal behavior), worth ~20-30 MB per instance.
- CDM can share one Chrome via `--browser-url`/`--autoConnect`; that saves the extra browsers but not the Node
  processes, and brings the shared-profile issues (one profile for all agents, autoConnect dialog + banner). This
  benchmark is the default isolated setup.
- One machine, one page. Rerun with the script before quoting new numbers.

## Suggested page wording

> Three agents, one Wikipedia page each: **1.8 GB** with three chrome-devtools-mcp servers and their Chrome instances,
> **0.55 GB** with Shuttle. With ten agents: **6.0 GB vs 1.4 GB**. (Measured on macOS, headless, chrome-devtools-mcp
> 1.10.1 + Chrome 155 vs Shuttle 0.1.1. Script in the repo.)
