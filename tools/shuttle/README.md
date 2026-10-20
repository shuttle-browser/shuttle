# Shuttle tooling (`src/tools/shuttle/`)

**Shuttle** is a deep fork of Chromium built for AI agents: everything agents don't need is removed, an MCP server
is built in, and every agent gets its own identity and persistent storage, visible on its tabs. macOS and Linux.

- Browser source (the fork): https://github.com/shuttle-browser/shuttle
- Website: https://www.shuttlebrowser.com

This directory of the fork holds the **tooling around the browser source**: GN args, build, prune, rebrand, release
and smoke-test scripts, the prune manifests that define what was removed, branding assets, and design docs. (Until
2026-10 it lived in the separate `shuttle-tools` repo.) Nothing here is part of the GN build; the tree-wide prune
scripts skip this directory.

## What is in here

| Path | Contents |
| --- | --- |
| `config/` | GN args (common, dev, release, Linux, opt-in ThinLTO + PGO), curated page-tool list, Linux package config |
| `scripts/` | Build, prune, fix, rebrand, release (macOS signing, DMG) and smoke-test scripts |
| `prune/` | Manifests consumed by `scripts/prune.py`, one per removal batch |
| `assets/` | Logo (SVG), icon generation, Linux desktop entry |
| `docs/` | Tooling guide and gotchas, removal log, Linux build notes, branding, MCP server design, RAM and speed benchmarks |

## Requirements

- macOS (Apple Silicon or Intel, current Xcode and SDK) or x86_64 Linux (WSL2 works)
- [`depot_tools`](https://commondatastorage.googleapis.com/chrome-infra-docs/flat/depot_tools/docs/html/depot_tools_tutorial.html) (default location `~/depot_tools`)
- Disk: about 30 GB for the checkout, plus 15-25 GB per build directory
- 16 GB RAM or more (24 GB+ recommended), Node.js 22+ and Python 3 for the scripts
- Several hours for a first build

## Build from source

```sh
# 1. Check out the fork with depot_tools (gclient clones it as src/ and syncs its DEPS)
export PATH="$PATH:$HOME/depot_tools"
mkdir -p ~/shuttle-browser && cd ~/shuttle-browser
cat > .gclient <<'EOT'
solutions = [{
  "name": "src",
  "url": "https://github.com/shuttle-browser/shuttle.git",
  "managed": False,
  "custom_deps": {},
  "custom_vars": {},
}]
EOT
# Linux: also add the line  target_os = ["linux"]  to .gclient
gclient sync -j 4                   # default -j hits googlesource rate limits (HTTP 429); re-run after DEPS changes
# Linux only: src/build/install-build-deps.sh

# 2. Build (GN args come from config/, written to out/<Dir>/args.gn automatically)
cd src/tools/shuttle
scripts/build.sh dev -k 0           # Linux: scripts/build-linux.sh dev -k 0 (caps: -local_jobs, nice 19)
```

The fork's history starts at a snapshot of the upstream release tag (branch `chromium`), so there is no Chromium
history to download; `gclient sync` resolves third-party dependencies from `DEPS`.

Smoke-test the result (headless, own temp profile):

```sh
scripts/smoke-all.sh ../../out/Release/Shuttle.app     # Linux: ../../out/Dev/shuttle
```

Release builds (`config/args-release.gn`) are signed and packaged with `scripts/release-mac.sh`
(`SHUTTLE_SIGN_ID`, `SHUTTLE_NOTARY_PROFILE`). Linux specifics: `docs/LINUX.md`.

## After an upstream Chromium merge

Upstream releases are merged into the fork, not rebased. Afterwards, from `src/tools/shuttle/`:

```sh
scripts/prune-tests.py --apply      # drop testonly GN targets and their files again
scripts/prune-locales.py --apply    # en-US-only UI (run before the rebrand)
scripts/rebrand.py apply --apply    # re-apply Shuttle branding to the source tree
```

Then rerun the manifests for areas that came back (`scripts/prune.py --apply prune/<manifest>.txt`), run
`gn gen out/Dev`, build, and smoke-test. Strategy: `docs/TOOLING.md` "Fork strategy"; workflow and gotchas:
`docs/TOOLING.md`.

## Smoke tests

`scripts/smoke-all.sh <binary|Shuttle.app> [--with network,push,automation] [--mode headless|x11|wayland]` launches
the browser headless with its own temp profile, runs the probes, closes the browser, checks its exit status and log,
and prints PASS/FAIL/SKIP per probe (exit 0 only if all passed).

| Probe | What it checks |
| --- | --- |
| `scripts/smoke-cdp.mjs` | WebUI pages load without unexpected JS exceptions; browser context |
| `scripts/smoke-mcp.mjs --phase1\|--phase2` | MCP server: agents, identity, isolation, popups, viewport; persistence across a restart |
| `scripts/smoke-session-cdp.mjs` | per-session CDP endpoint (Playwright, agent-browser) |
| `scripts/smoke-agents-page.mjs` | `chrome://agents` UI |
| `scripts/smoke-focus.mjs` | agents can't steal focus (bringToFront / activateTarget) |
| `scripts/smoke-sandbox.mjs` | `chrome://sandbox/` status (Linux) |
| `scripts/check-automation.mjs` | opt-in: no approval prompt, no automation infobar, `navigator.webdriver` false |
| `scripts/smoke-push.mjs` | opt-in: Web Push subscription (needs `SHUTTLE_SMOKE_NET=1`) |
| `scripts/smoke-network.mjs` | opt-in: network mode (`--shuttle-listen`, access token) |

## Documentation

`docs/TOOLING.md` (start here), `docs/MCP.md`, `docs/REMOVAL-LOG.md`, `docs/LINUX.md`, `docs/BRANDING.md`,
`docs/BENCHMARK.md`. Coding agents: see `AGENTS.md`. Contributions: `CONTRIBUTING.md`. Security: `../SECURITY.md`.

## License

BSD 3-Clause, Copyright (c) 2026 Silkweave. See `../LICENSE-SHUTTLE`. Chromium and its dependencies keep their own licenses.
