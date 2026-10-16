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
| `assets/` | Logo (SVG), icon generation, Linux desktop entry |
| `docs/` | Tooling guide and gotchas, removal log, Linux build notes, branding, MCP server design, RAM benchmark |

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
scripts/build.sh dev -j 6 -k 0      # Linux: scripts/build-linux.sh dev -j 12 -k 0
```

The fork's history starts at a snapshot of the upstream release tag (branch `chromium`), so there is no Chromium
history to download; `gclient sync` resolves third-party dependencies from `DEPS`.

Run the result headless with a fresh profile and smoke-test it:

```sh
../../out/Dev/Shuttle.app/Contents/MacOS/Shuttle --headless \
  --user-data-dir=/tmp/sh-profile --remote-debugging-port=9222 &     # Linux: ../../out/Dev/shuttle
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
`gn gen out/Dev`, build, and smoke-test. Strategy: `docs/TOOLING.md` "Fork strategy"; workflow and gotchas:
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

`docs/TOOLING.md` (start here), `docs/MCP.md`, `docs/REMOVAL-LOG.md`, `docs/LINUX.md`, `docs/BRANDING.md`,
`docs/BENCHMARK.md`. Coding agents: see `AGENTS.md`. Contributions: `CONTRIBUTING.md`. Security: `../SECURITY.md`.

## License

BSD 3-Clause, Copyright (c) 2026 Silkweave. See `../LICENSE-SHUTTLE`. Chromium and its dependencies keep their own licenses.
