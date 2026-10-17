# Shuttle tooling (instructions for coding agents)

**Shuttle** (Shuttle Browser, by Silkweave) is a deep fork of Chromium specialized for AI agents, for macOS and
Linux: everything agents don't need is physically removed, an MCP server is built in, and tabs show which agent
controls them. Website: https://www.shuttlebrowser.com. The browser source is the fork
(`github.com/shuttle-browser/shuttle`); **this directory (`src/tools/shuttle/`) holds the tooling around it**: GN args,
build/prune/rebrand/smoke scripts, prune manifests, branding assets, docs. Scripts operate on the fork they live in
(`scripts/lib/paths.sh`, `scripts/shuttle_paths.py`: `SHUTTLE_SRC` = `src/`, `SHUTTLE_ROOT` = the gclient checkout).
Nothing here is part of the GN build; tree-wide scripts skip `tools/shuttle/`.

The fork is **public**: no machine names, IPs, user names, absolute home paths or credentials in tracked files.
Machine-local settings go in `~/.shuttle/env`; team notes, handoffs and briefs in the git-ignored `_docs/` (if it
exists, read `_docs/AGENTS-TEAM.md` and your handoff there first).

## Layout

| Path | Contents |
| --- | --- |
| `config/` | GN args (`args-common.gn`, `args-{dev,release,linux,linux-release}.gn`), `agent-browser-tools.json` (curated page tools), `sccache.example.toml`, `sccache/systemd/` (build-host unit templates) |
| `scripts/` | Build, prune, fix, rebrand, release and smoke-test scripts (see below and `docs/TOOLING.md`) |
| `prune/` | Manifests for `scripts/prune.py` (one per removal batch) |
| `fixtures/` | Small plain-bash tests for the safety gates in `scripts/`, run by `scripts/check-scripts.sh` |
| `assets/` | Logo and icon sources, icon generation, Linux desktop entry |
| `docs/` | `TOOLING.md` (workflow + all gotchas), `REMOVAL-LOG.md` (fork commits and what each removed), `MCP.md`, `LINUX.md`, `BRANDING.md`, `BENCHMARK.md` |

Docs index: `docs/TOOLING.md` (read first: fork strategy, GN args rationale, build, merges, releases, gotchas),
`docs/MCP.md` (in-browser MCP server, agent model, session CDP, page tools), `docs/REMOVAL-LOG.md` (what each fork
commit removed, what is kept on purpose), `docs/LINUX.md`, `docs/BRANDING.md` (incl. rebrand mechanics),
`docs/BENCHMARK.md` (RAM benchmark behind the website's numbers).

## Fork basics

- Fork checkout (`src/` of a gclient checkout): base branch `chromium` = stable Chromium **155.0.8059.80** snapshot (merged 2026-10-16; 154.0.8037.129 before); fork work
  lives on `main`, work in progress on topic branches. Upstream releases are *merged* in, not rebased; afterwards
  rerun the prune and rebrand scripts (see "After an upstream merge").
- Mac and Linux must both build before a removal batch is committed. Windows is removed (WSL2 runs the Linux build).
- Internal code ids, prefs, storage and the Linux binary are named `shuttle`; the MCP server is `shuttle`
  (`.mcp.json` points an MCP client at `http://127.0.0.1:9222/mcp`).
- DevTools is always on (9222 or a free port, see `DevToolsActivePort`), with no approval dialog, no automation
  infobar and `navigator.webdriver` false. `scripts/check-automation.mjs` verifies it (`smoke-all.sh --with automation`).
- `depot_tools` is expected at `~/depot_tools` but not necessarily on `PATH`:
  `export PATH="$PATH:$HOME/depot_tools"`.

## Build

- `config/args-common.gn` + `config/args-{dev,release}.gn` are **the** GN args (Linux adds `args-linux.gn`).
- `scripts/build.sh [dev|release|sccache] [autoninja args]`: writes the args, runs `gn gen` when they change,
  builds the `chrome` target, logs to `$SHUTTLE_ROOT/logs/build-<Dir>.log`, ends with `BUILD EXIT <rc>`.
  The build scripts enforce the caps themselves (`scripts/lib/build-common.sh`): nice 19, `-local_jobs` 8 with sccache-dist / 4 local / 10 on the Linux host unless given (`SHUTTLE_LOCAL_JOBS`), one build per out dir (lock), ≥ 20 GB free (`SHUTTLE_MIN_FREE_GB`); `SHUTTLE_BG=1` adds `taskpolicy -c background`.
  `-local_jobs N` is siso's real cap (`-j` above the core count is ignored); add `-k 0` to collect every error in one
  pass. `SHUTTLE_KEEP_ARGS=1` refuses to change `args.gn`. `scripts/build-linux.sh` is the Linux counterpart (`docs/LINUX.md`).
- `scripts/build-sccache.sh` (optional): distributed compiles on a Linux build host running sccache-dist
  (`config/sccache.example.toml`, `scripts/sccache-toolchain.sh`, host side `scripts/sccache-dist-host.sh`; see `docs/TOOLING.md` "Remote builds"). `-local_jobs` is the real cap.
- `scripts/build-watch.py <Dir>`: quiet build monitor (progress, first errors, stall detection).
  `scripts/build-errors.sh` lists failed steps while a build runs.
- `SHUTTLE_VERSION=<X.Y.Z> scripts/release-mac.sh`: sign, notarize and package a DMG from `out/Release` (needs its
  clean-tree build receipt; writes `release/v<X.Y.Z>/mac/` + `MANIFEST.txt`). Identity and notary profile come from
  `SHUTTLE_SIGN_ID` and `SHUTTLE_NOTARY_PROFILE`; never commit identities or credentials.

## Removal workflow

Scout, then confirm borderline items, then manifest, then `scripts/prune.py --apply`, then `git add -A`, then
`scripts/fix-gn-refs.py`, then manual GN/pak/grdp fixes until `gn gen` passes, then `scripts/scan-webui-imports.py`,
then surgery (parallel workers need file ownership and a shared brief), then one `-k 0` build, fix, a fresh-profile
smoke test, and one commit. Details and every gotcha: `docs/TOOLING.md`.

Prune and fix tools: `prune.py` (manifest-driven deletion; manifests in `prune/`), `prune-tests.py` (testonly GN
targets), `prune-policies.py`, `prune-strings.py`, `prune-locales.py` (en-US-only UI), `fix-gn-refs.py`,
`gn-fix-loop.py`, `delete-classes.py`, `scan-webui-imports.py`, `syntax-check-file.sh`,
`add-include.py`, `restore-includes.py`, `restore-strings.py`.

## After an upstream merge

Worked for 154 → 155 (first merge, 2026-10-14; log in TOOLING "Upstream merges"):

1. Conflicts: modify/delete → `git rm`; `git checkout --conflict=zdiff3 -- <UU files>`, then
   `scripts/merge-autoresolve.py stats|apply ours-deleted,ours-subset <list>` (fork-only deletions; hunks where
   upstream added lines stay conflicted: `show ours-deleted-new,ours-subset-new` and review them), the rest by
   sub-agents per area with a shared brief. Keep DEPS `version_file`s (gclient reads them even when unused).
2. `gclient sync -j 4` from the gclient root (default -j hits googlesource HTTP 429); new clang ⇒ toolchain archive.
3. Delete upstream additions in directories the fork deleted entirely; judge brand-new dirs; rerun `prune/m155.txt`
   and other remove-only manifests.
4. `scripts/prune-locales.py --apply`, `scripts/rebrand.py apply --apply` (keep main's rendered assets if only bytes differ).
5. `scripts/gn-fix-loop.py` (dry run), then `--apply` until `gn gen` passes; then `scripts/prune-tests.py --apply`, `fix-gn-refs.py --apply`.
6. `scripts/check-ninja-inputs.py` (missing inputs on chrome's graph; restore from `chromium`), then a `-k 0` build.

## Smoke tests

`scripts/smoke-all.sh <binary|Shuttle.app> [--with network,push,automation] [--mode headless|x11|wayland]` owns the
browser: own temp profile, `--remote-debugging-port=0` (port from `DevToolsActivePort`), runs the probes, closes the
browser, checks its exit status and the log for fatals, removes the temp dir, prints PASS/FAIL/SKIP. Exit 0 all
passed, 1 a FAIL, 3 something skipped (open, not green). Mac: `nice -n 19 scripts/smoke-all.sh ../../out/Release/Shuttle.app`.
Probes (`node <probe> <port>`, `scripts/lib/smoke.mjs`): exit 0/1/2 usage/3 skipped, clean up their agents and tabs,
never close the browser, local pages only (anything needing the internet skips unless `SHUTTLE_SMOKE_NET=1`).

| Probe | Checks |
| --- | --- |
| `smoke-cdp.mjs` | WebUI pages load, no JS exceptions/errors outside its `EXPECTED` list, browser context, credentials stub |
| `smoke-mcp.mjs --phase1\|--phase2` | MCP server: agents/identity, isolation, popups, viewport; phase 2 after a restart: persistence |
| `smoke-session-cdp.mjs` | per-session CDP endpoint (optional Playwright / agent-browser via `PLAYWRIGHT_CORE`, `AGENT_BROWSER`) |
| `smoke-agents-page.mjs [shotDir]` | `chrome://agents` UI |
| `smoke-focus.mjs [windowed]` | fork 45: bringToFront/activateTarget work; `windowed` (manual run, macOS `lsappinfo`): Shuttle not frontmost |
| `smoke-sandbox.mjs` | `chrome://sandbox/` adequately sandboxed (Linux; not built on macOS) |
| `smoke-network.mjs <udd> [lanIp]` | opt-in `network`: network mode (`--shuttle-listen`, access token) |
| `smoke-push.mjs` | opt-in `push`: Web Push subscribe (needs `SHUTTLE_SMOKE_NET=1`, 30-90 s) |
| `check-automation.mjs [udd]` | opt-in `automation` (own launch, `--enable-automation`): no prompt, no infobar, `navigator.webdriver` false |

`scripts/linux-nsw-check.sh` stays a manual Linux helper (`docs/LINUX.md`).

Helpers: `scripts/test-browser.sh [close]` (manual test browser on 9333 with two agents, mktemp profile), `scripts/mcp-call.mjs <port> <tool> '<json>'`,
`scripts/launch-shuttle.sh` (interactive run on 9222),
`scripts/fetch-agent-browser.sh <Helpers-dir>` + `scripts/gen-page-tools.mjs` (page-tool sidecar and generated tool list),
`scripts/linux-install-desktop.sh` (per-user desktop entry), `scripts/package-linux.sh` (Linux tarball), `scripts/package-deb.sh` (.deb + .rpm from the tarball),
`scripts/bench-ram.mjs` (RAM benchmark, `docs/BENCHMARK.md`).

## Rules and top gotchas (full list in `docs/TOOLING.md`)

- Smoke-test with `scripts/smoke-all.sh` (it owns the temp profile, the launch and `Browser.close`; probes never
  close the browser). Run smokes **`--headless`**: agent tabs create windows that pop in front of whatever the user is doing. Windowed runs only for
  window behavior.
- Never send keystrokes or AppleScript to the desktop GUI; drive the browser over CDP
  (`--remote-debugging-port`, Node's global `WebSocket`) and ask a human for keyboard tests.
- Never commit `build/util/MINIBRANCH` in the fork.
- Don't run full-tree `git grep` in parallel; scope searches to directories.
- Dev builds crash on WebUI JS errors and `DUMP_WILL_BE_NOTREACHED`; a WebUI page whose Mojo handler was removed
  fails at startup.
- Official (release) builds link with export lists and can break where dev builds don't: check
  `chrome/app/app.exports` and `framework.exports` after removals.
- Keep the Mac on power with the lid open during long builds (lid closed on battery stops them).
- Never load the Mac to full CPU: it is the maintainer's workstation and a full-load build cuts them off (no input,
  no messages). `build-sccache.sh` refuses to start when the build host has no servers (the build would compile
  locally); `SHUTTLE_ALLOW_LOCAL=1` only after asking. The scripts set nice 19 + `-local_jobs` (8 dist / 4 local);
  don't add `taskpolicy -c background` (E-cores only, ~14× slower) unless asked. Run builds in a shell the maintainer
  can watch; stop if the load climbs. sccache can still fall back to local compiles if the host drops out mid-build. Don't kill a build early
  to read its step count: killed runs leave regenerated outputs (protoc, mojom) that cascade.
- Run `scripts/check-scripts.sh` before every tooling commit (syntax per interpreter, shellcheck, `fixtures/`). A
  changed safety gate gets one fixture that fails on the old bug and passes after the fix; no test framework.
- Keep branded surfaces consistent with `docs/BRANDING.md`.
- No secrets in tracked files: API keys go in the environment or `_docs/.env`, signing material stays in the keychain.
- Before pushing the fork (it is public): secret scan of `tools/shuttle/` and the new commits (gitleaks, low priority:
  `nice -n 19`, `GOMAXPROCS=2`, scoped to `tools/shuttle/` + `github/main..HEAD`, never the full Chromium history).
