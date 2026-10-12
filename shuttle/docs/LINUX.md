# Building on Linux

Shuttle supports **macOS + Linux**. Windows users run the Linux build in WSL2/WSLg; Windows itself is
removed from the fork.

## Requirements

- x86_64 Linux (Ubuntu 24.04+ tested; WSL2 works). 12+ cores and 24 GB RAM recommended; give a WSL2 VM
  plenty of swap (a `.wslconfig` with `memory=` and `swap=` set generously).
- Disk: ~30 GB for the checkout plus 15-25 GB per build directory. In WSL2, `df /` shows the virtual
  disk size, not the real free space: check the Windows host drive (`df -h /mnt/c`).
- `depot_tools` on `PATH`, and the build dependencies: `build/install-build-deps.sh` from the Chromium
  checkout (works on current Ubuntu releases; add `--unsupported` if it refuses yours).
- WSL2 only: keep the distro alive during long builds (for example a Windows logon task running
  `wsl.exe -d <distro> --exec sleep infinity`). Never run `wsl --shutdown` from inside WSL.

## Checkout

```sh
mkdir -p ~/chromium && cd ~/chromium
cat > .gclient <<'EOT'
solutions = [{
  "name": "src",
  "url": "https://chromium.googlesource.com/chromium/src.git",
  "managed": False,
  "custom_deps": {},
  "custom_vars": {},
}]
target_os = ["linux"]
EOT
fetch --nohooks chromium   # or: gclient sync, then check out 154.0.8037.129
cd src && git remote add github git@github.com:shuttle-browser/shuttle.git
git fetch github && git checkout main
gclient sync -D            # re-run after DEPS changes
./build/install-build-deps.sh
```

See the README for the full "build from source" flow.

## Build

- `scripts/build-linux.sh [dev|release] [autoninja args]`: GN args are `config/args-common.gn` +
  `config/args-<kind>.gn` + `config/args-linux.gn` (VR/OpenXR off) + `config/args-linux-<kind>.gn` if present. dev builds
  `chrome`; release builds `chrome chrome_sandbox` in `out/Release`. Logs to `~/chromium/logs/build-<Dir>.log`, which ends
  with `BUILD EXIT <rc>`. The script unsets `CLAUDECODE`/`AI_AGENT`: with them set, siso runs in agent quiet mode
  (no progress lines) and `build-watch.py` sees nothing.
- Run long builds detached, e.g. `tmux new-session -d -s build "bash scripts/build-linux.sh dev -j 12 -k 0"`.
  `-k 0` keeps going after failures so one pass collects every error.
- Progress / errors from the log:
  `tail -c 20000 log | tr '\r' '\n' | grep -oE '\[ *[0-9]+/[0-9]+'`,
  `tr '\r' '\n' < log | sed 's/\x1b\[[0-9;]*m//g' | grep -E '(fatal )?error:' | sort | uniq -c`,
  or `scripts/build-watch.py Dev`.
- A first full build is roughly 50k steps (about 4 hours at `-j 12`).
- Release (v0.1.0): `config/args-linux-release.gn` overrides the Mac's arm64 with `target_cpu = "x64"`, `is_cfi = false`
  (official x64 Linux defaults to CFI, which asserts ThinLTO, off in `args-release.gn`) and `enable_linux_installer = false`
  (`chrome/installer/linux` is pruned; we ship a tarball). Result: official, non-component, open codecs, no PGO/ThinLTO/CFI.
  A fresh build is ~45k steps (~3.5 h at `-j 12`).
- Official/non-component-only Linux breakages (fixed in fork commit 31): root `BUILD.gn` gn_all pulled
  `service_discovery_sniffer`; `enable_webui_generate_code_cache` (Linux/Windows official only) compiled a stale side panel
  code cache include; the en-US-only permission-element generator emitted an empty `MakeFixedFlatMap({})`. An old out dir
  hides that last one: the stale generated .cc keeps the pre-B3 entries.

## Release tarball

- `scripts/package-linux.sh [out-dir] [dest-dir]` (defaults `out/Release`, `~/chromium/release/<chrome/VERSION>`) →
  `shuttle-linux-x64.tar.xz` + `.sha256`, top-level `shuttle/`. The file list follows upstream
  `chrome/installer/linux/common/installer.py`, plus agent-browser (+ LICENSE/NOTICE) via
  `fetch-agent-browser.sh <dir> linux-x64`. Binaries are stripped, xz -9: ~85 MB packed, ~291 MB unpacked.
- Page tools: the browser looks for `agent-browser` next to the binary (`SHUTTLE_AGENT_BROWSER` overrides).
- Sandbox: Linux uses the user-namespace sandbox (`chrome://sandbox`: "adequately sandboxed"). Ubuntu 23.10+/24.04 block
  unprivileged user namespaces through AppArmor, and Shuttle then fails with "No usable sandbox!". For those systems the
  tarball ships the setuid helper `chrome-sandbox` (`sandbox/linux/suid/*` restored in fork commit 31): run once
  `sudo chown root:root chrome-sandbox && sudo chmod 4755 chrome-sandbox`. Never recommend `--no-sandbox`.
- Compatibility: max `GLIBC_2.25`, no GLIBCXX (libc++ is static).
- Tarball smoke: extract into a fresh dir and run the extracted binary headless: smoke-cdp, smoke-mcp + `--persist-check`,
  smoke-agents-page, page tools, and `smoke-network.mjs <port> <udd> <lanIp>` with `--shuttle-listen=0.0.0.0`.
- WSL2 (mirrored networking): Windows clients reach `http://127.0.0.1:9222/json/version` and `/mcp`; a foreign `Origin`
  gets 403. NAT mode not verified.

## Binary, WM_CLASS, desktop entry

The Linux binary is `out/Dev/shuttle` (the GN target is still `chrome`; the `linux-*.sh` smokes exit 2
if `shuttle` is missing). WM_CLASS (`shuttle`, `Shuttle`), Wayland app_id `shuttle`, desktop id
`shuttle.desktop` (`$CHROME_DESKTOP` still overrides), secret service app `shuttle` / KWallet
"Shuttle Keys", profile `~/.config/shuttle`. `scripts/linux-install-desktop.sh [out-dir|--uninstall]`
installs a per-user desktop entry (`branding/linux/shuttle.desktop`) plus hicolor icons for
WSLg/Linux desktops.

## Linux smoke tests

All smokes use a fresh profile (`/tmp/ac-profile`), no debug flags (DevTools is always on at 9222),
and end with `Browser.close`.

- `scripts/linux-smoke.sh [headless] [x11] [wayland]` (default all three) for the current `out/Dev`:
  runs `scripts/smoke-cdp.mjs 9222`, then `scripts/cdp-sandbox-close.mjs 9222` (prints the
  `chrome://sandbox` status, sends `Browser.close`). WSLg: X11 via `DISPLAY=:0`, Wayland via
  `XDG_RUNTIME_DIR=/mnt/wslg/runtime-dir WAYLAND_DISPLAY=wayland-0`.
- Per mode it reports clean pages, unexpected smoke lines (expected: `createBrowserContext` fails
  since OTR is gone, `Autofill.enable` not found, `Browser.getVersion`), **the browser's exit status**
  (0 = clean, 133 = SIGTRAP, killed after 60 s = shutdown hang) and log hits for
  `FATAL|Check failed|DUMP_WILL_BE|unregistered pref|KeyedService Factory|DanglingRawPtrDetected`.
  Shutdown crashes only show in the exit status and the log, never in the smoke output.
- MCP: `scripts/linux-mcp-smoke.sh [headless|x11|wayland]` runs smoke-mcp, restarts on the same
  profile, runs `--persist-check`, and checks exit status and fatals each time (log
  `/tmp/ac-mcp-<mode>.log`).
- No startup window: `scripts/linux-nsw-check.sh [x11|wayland|headless]` checks a windowless start,
  the allocate-tab window, keep-alive after close-session, second-launch forwarding, launch with a URL
  argument, and a clean `Browser.close` (log `/tmp/ac-nsw-<mode>.log`).
- Push: `scripts/linux-push-smoke.sh [headless|x11|wayland]` runs `scripts/smoke-push.mjs` (Web Push
  subscribe, 90 s timeout; expect an FCM endpoint after about 30 s).
- Not covered by WSLg: GTK/portal file dialogs, the dbus appmenu, custom frame and caption buttons
  (need a GNOME/KDE session).

## Optional: distributed compiles

`scripts/build-sccache.sh` can compile on a remote Linux box through sccache-dist; the client config
template is `config/sccache.example.toml`, the toolchain archive comes from
`scripts/sccache-toolchain.sh`, and the gotchas are in `docs/TOOLING.md`. Running the scheduler and
build servers (systemd units, `bubblewrap`, firewalling the scheduler port) is up to you: they listen
without authentication on the network unless you restrict them. Size `-local_jobs` to the build host's
real core count and RAM; more jobs than it can hold thrash swap on heavy Blink/content files.

## How the Linux files came back (fork branch `ac/linux`)

The fork originally pruned Linux along with the other platforms; Linux was restored from the base tree:

1. Base read set: `gn gen --ide=json out/Base` on the *base* tree (branch `chromium`) with the Linux
   args, then `scripts/linux-read-set.py out/Base` (sources/inputs/public/data of targets reachable from
   //chrome:chrome + .gn/.gni + grd refs).
2. `scripts/linux-restore-set.py`: missing-in-fork ∩ Linux-read, minus removed features (manifest
   dir/remove rules, gone dirs with feature deletions, same-stem siblings of feature-deleted files);
   then `git checkout <base> --pathspec-from-file=...`.
3. GN: `scripts/gn-fix-loads.py` (deletes single-entry deps on unloadable dirs, only lines GN
   evaluates) + `scripts/gn-fix-unresolved.py`. `fix-gn-refs.py --base chromium` exists but on the whole
   tree it touches ~11k lines of other-platform branches (merge churn): don't use it there.
4. Restored BUILD.gn files bring their test targets back: `prune-tests.py --apply`, then **revert every
   change outside the restore set** (on Linux its "used" set lacks a deps log and it would delete
   Mac-kept files). Then fix leftovers (ui/ozone test vars, root gn_all test deps).
5. Build-driven: missing headers (included via generated code, mojom typemaps, or unconditionally under
   other-platform `#if`s): `scripts/restore-includes.py <path>...` (restores transitively); removed
   strings: `scripts/restore-strings.py IDS_...` (re-inserts `<message>` from the base grd).
6. Never touch a kept file except in Linux-only blocks; check `gn gen out/Dev` on macOS after pulling
   Linux fixes.

## Later

`.deb`/AppImage packaging (`chrome/installer/linux`, not restored), a windowed X11/Wayland run of the tarball, the setuid
sandbox path on a real Ubuntu 24.04; the third-party/web-tests prune must keep what Linux reads.
