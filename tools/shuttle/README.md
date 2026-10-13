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
| `config/` | GN args (common, dev, release, Linux), curated page-tool list, sccache client config example, sccache-dist host units |
| `scripts/` | Build, prune, fix, rebrand, release (macOS signing, DMG) and smoke-test scripts |
| `prune/` | Manifests consumed by `scripts/prune.py`, one per removal batch |
| `branding/` | Logo (SVG), icon generation, Linux desktop entry |
| `patches/` | Readable export of the first fork commits |
| `docs/` | Tooling guide and gotchas, removal log, Linux build notes, branding, design plans (`docs/plans/`) |

## Requirements

- macOS (Apple Silicon or Intel, current Xcode and SDK) or x86_64 Linux (WSL2 works)
- [`depot_tools`](https://commondatastorage.googleapis.com/chrome-infra-docs/flat/depot_tools/docs/html/depot_tools_tutorial.html) (default location `~/depot_tools`)
- Disk: about 30 GB for the checkout, plus 15-25 GB per build directory
- 16 GB RAM or more (24 GB+ recommended), Node.js 22+ and Python 3 for the scripts
- Several hours for a first build

## Build from source

```sh
# 1. Check out Chromium 154.0.8037.129 with depot_tools
export PATH="$PATH:$HOME/depot_tools"
mkdir -p ~/chromium && cd ~/chromium
fetch --nohooks chromium            # Linux: add target_os = ["linux"] to .gclient
cd src && git checkout 154.0.8037.129

# 2. Add the fork and check out its main branch (brings this tooling along in src/tools/shuttle/)
git remote add github https://github.com/shuttle-browser/shuttle.git
git fetch github && git checkout -B main github/main
gclient sync -D
# Linux only: ./build/install-build-deps.sh

# 3. Build (GN args come from config/, written to out/Dev/args.gn automatically)
cd shuttle
scripts/build.sh dev -j 6 -k 0      # Linux: scripts/build-linux.sh dev -j 12 -k 0
```

The fork's history starts at a snapshot of the upstream tag, so fetching the fork's `main` replaces the
upstream history; `gclient sync` still resolves third-party dependencies from `DEPS`.

Run the result headless with a fresh profile and smoke-test it:

```sh
../out/Dev/Shuttle.app/Contents/MacOS/Shuttle --headless \
  --user-data-dir=/tmp/sh-profile --remote-debugging-port=9222 &     # Linux: out/Dev/shuttle
node scripts/smoke-cdp.mjs 9222
node scripts/smoke-mcp.mjs 9222
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
`gn gen out/Dev`, build, and smoke-test. Strategy: `docs/plans/DEEP-FORK.md`; workflow and gotchas:
`docs/TOOLING.md`.

## Smoke tests

All smokes use a fresh profile, run headless, and end with `Browser.close`.

| Script | What it checks |
| --- | --- |
| `scripts/smoke-cdp.mjs <port>` | WebUI pages load without JS exceptions; browser context |
| `scripts/check-automation.mjs <port>` | no approval prompt, no automation infobar, `navigator.webdriver` false |
| `scripts/smoke-mcp.mjs <port> [--persist-check]` | MCP server: agents, identity, isolation, popups, viewport |
| `scripts/smoke-agents-page.mjs [port]` | `chrome://agents` UI |
| `scripts/smoke-session-cdp.mjs [port]` | per-session CDP endpoint (Playwright, agent-browser) |
| `scripts/smoke-push.mjs` | Web Push subscription |
| `scripts/smoke-network.mjs <port> <udd> [lanIp]` | network mode (`--shuttle-listen`, access token) |
| `scripts/linux-smoke.sh` and friends | Linux wrappers for headless / X11 / Wayland |

## Documentation

`docs/TOOLING.md` (start here), `docs/REMOVAL-LOG.md`, `docs/LINUX.md`, `docs/BRANDING.md`, `docs/TRIAL.md`, and design
notes in `docs/plans/` (MCP server v1/v2, deep-fork strategy, removal candidates, build optimization, rebrand, RAM
benchmark). Coding agents: see `AGENTS.md`. Contributions: `CONTRIBUTING.md`. Security: `../SECURITY.md`.

## License

BSD 3-Clause, Copyright (c) 2026 Silkweave. See `../LICENSE-SHUTTLE`. Chromium and its dependencies keep their own licenses.
