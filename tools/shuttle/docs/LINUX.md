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
- Moving an existing checkout: follow the "Moving the checkout" gotcha in `docs/TOOLING.md` (`siso fs export` per out dir, mv, rewrite paths,
  `siso fs import`, temporary old→new symlink, `gn gen`, dry run must match) or every out dir rebuilds from scratch.
- WSL2 only: keep the distro alive during long builds (for example a Windows logon task running
  `wsl.exe -d <distro> --exec sleep infinity`). Never run `wsl --shutdown` from inside WSL.

## Checkout

The gclient root can live anywhere; the scripts find it two levels above `src/tools/shuttle` (override with
`SHUTTLE_ROOT`). Our hosts use `~/projects/shuttle-browser` (gclient root: `.gclient`, `src/`, `logs/`, `release/`).

Checkout: README "Build from source" (add `target_os = ["linux"]` to `.gclient`, then
`src/build/install-build-deps.sh`). Pushing over ssh needs the remote URL `git@github.com:shuttle-browser/shuttle.git`.


## Build

- `scripts/build-linux.sh [dev|release] [autoninja args]`: GN args are `config/args-common.gn` +
  `config/args-<kind>.gn` + `config/args-linux.gn` (VR/OpenXR off) + `config/args-linux-<kind>.gn` if present. dev builds
  `chrome`; release builds `chrome chrome_sandbox` in `out/Release`. Logs to `$SHUTTLE_ROOT/logs/build-<Dir>.log`, which ends
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

- `scripts/package-linux.sh [out-dir] [dest-dir]` (defaults `out/Release`, `$SHUTTLE_ROOT/release/<chrome/VERSION>`) →
  `shuttle-linux-x64.tar.xz` + `.sha256`, top-level `shuttle/`. The file list follows upstream
  `chrome/installer/linux/common/installer.py`, plus agent-browser (+ LICENSE/NOTICE) via
  `fetch-agent-browser.sh <dir> linux-x64`. Binaries are stripped, xz -9: ~85 MB packed, ~291 MB unpacked.
- Page tools: the browser looks for `agent-browser` next to the binary (`SHUTTLE_AGENT_BROWSER` overrides).
- Sandbox: Linux uses the user-namespace sandbox (`chrome://sandbox`: "adequately sandboxed"). Ubuntu 23.10+/24.04 block
  unprivileged user namespaces through AppArmor, and Shuttle then fails with "No usable sandbox!". For those systems the
  tarball ships the setuid helper `chrome-sandbox` (`sandbox/linux/suid/*` restored in fork commit 31): run once
  `sudo chown root:root chrome-sandbox && sudo chmod 4755 chrome-sandbox`. Never recommend `--no-sandbox`. The .deb/.rpm
  install it 4755 root:root (`/opt/shuttle/chrome-sandbox`); check the setuid path with `--disable-namespace-sandbox`
  (with mode 0755 that aborts with "The SUID sandbox helper binary was found, but is not configured correctly").
- Compatibility: max `GLIBC_2.25`, no GLIBCXX (libc++ is static).
- Tarball smoke: extract into a fresh dir and run the extracted binary headless: smoke-cdp, smoke-mcp + `--persist-check`,
  smoke-agents-page, page tools, and `smoke-network.mjs <port> <udd> <lanIp>` with `--shuttle-listen=0.0.0.0`.
- WSL2 (mirrored networking): Windows clients reach `http://127.0.0.1:9222/json/version` and `/mcp`; a foreign `Origin`
  gets 403. NAT mode not verified.
- **Packages:** after `scripts/package-linux.sh`, run
  `SHUTTLE_PKG_REPO_SOURCE=1 scripts/package-deb.sh $SHUTTLE_RELEASE/<dir>/shuttle-linux-x64.tar.xz X $SHUTTLE_RELEASE/<dir>`.
  Release packages need `SHUTTLE_PKG_REPO_SOURCE=1`: without it the deb lacks the repo keyring and an upgrade breaks
  `apt update` (`NO_PUBKEY`), the rpm lacks key and repo file (caught in the 0.1.4 upgrade test).
  This builds `shuttle_X_amd64.deb` and `shuttle-X-1.x86_64.rpm`; details are in TOOLING "Linux packages".
- **Supported, all tested 2026-10-13:**
  - Ubuntu 20.04, 22.04 and 24.04;
  - Debian 11, 12 and 13. Debian 11 is EOL: its security pool 404s on deb.debian.org, so use archive.debian.org;
  - Fedora 44 (rpm).
  - The floor is glibc 2.28, set by the agent-browser sidecar.
- **WSL test distros:**
  - Create one with `wsl.exe --import <name> C:\…\distros\<name> <rootfs.tar.gz>`. The `distros` directory must exist
    first.
  - Minimal images lack sudo and su. On Fedora, install util-linux. Use `-u root` for the install, then `su <user>` to run.
  - Pass scripts as files on `/mnt/c`, because `$` variables get mangled through `wsl.exe bash -c`. Strip NULs from
    wsl.exe output (`tr -d '\0'`).
  - Ports are shared across distros, so never run two browsers on the same port.
  - `wsl.exe --unregister <name>` afterwards.

## Binary, WM_CLASS, desktop entry

The Linux binary is `out/Dev/shuttle` (the GN target is still `chrome`; the `linux-*.sh` smokes exit 2
if `shuttle` is missing). WM_CLASS (`shuttle`, `Shuttle`), Wayland app_id `shuttle`, desktop id
`shuttle.desktop` (`$CHROME_DESKTOP` still overrides), secret service app `shuttle` / KWallet
"Shuttle Keys", profile `~/.config/shuttle`. `scripts/linux-install-desktop.sh [out-dir|--uninstall]`
installs a per-user desktop entry (`assets/linux/shuttle.desktop`) plus hicolor icons for
WSLg/Linux desktops.

## Linux smoke tests

All smokes use a fresh profile (`/tmp/sh-profile`), no debug flags (DevTools is always on at 9222),
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
  `/tmp/sh-mcp-<mode>.log`).
- No startup window: `scripts/linux-nsw-check.sh [x11|wayland|headless]` checks a windowless start,
  the allocate-tab window, keep-alive after close-session, second-launch forwarding, launch with a URL
  argument, and a clean `Browser.close` (log `/tmp/sh-nsw-<mode>.log`).
- Push: `scripts/linux-push-smoke.sh [headless|x11|wayland]` runs `scripts/smoke-push.mjs` (Web Push
  subscribe, 90 s timeout; expect an FCM endpoint after about 30 s).
- Not covered by WSLg: GTK/portal file dialogs, the dbus appmenu, caption-button themes
  (need a GNOME/KDE session).
- Desktop behavior (fork commits 40-42, check on WSLg with `winshot` on the WSL box):
  - A `shuttle` started from a terminal (stdin is a tty) with a display, or from the menu entry, opens chrome://agents;
    started by a program (no tty) it stays windowless. A second launch focuses the agents tab; `--new-window` opens a
    stock window.
  - WSL defaults: Shuttle's own frame (not Weston's light title bar) and the Windows dark/light app mode (`reg.exe`,
    rechecked every minute). Test with a fresh profile: an explicit choice in chrome://settings/appearance wins.
  - Closing the last window keeps Shuttle running while an agent session is open; menu Exit always quits.

## Optional: distributed compiles

`scripts/build-sccache.sh` can compile on a remote Linux box through sccache-dist; the client config
template is `config/sccache.example.toml`, the toolchain archive comes from
`scripts/sccache-toolchain.sh`, and the gotchas are in `docs/TOOLING.md`. Running the scheduler and
build servers (systemd units, `bubblewrap`, firewalling the scheduler port) is up to you: they listen
without authentication on the network unless you restrict them. Our units are templates in
`config/sccache/systemd/` (`scripts/sccache-dist-host.sh install|status|restart|logs`); the script reads
`SHUTTLE_BUILD_HOST_IP` (the address the scheduler binds) from `~/.shuttle/env`. Size `-local_jobs` to the build host's
real core count and RAM; more jobs than it can hold thrash swap on heavy Blink/content files.

## How the Linux files came back (fork branch `ac/linux`)

The fork originally pruned Linux along with the other platforms; Linux was restored from the base tree:

1. Base read set: `gn gen --ide=json out/Base` on the *base* tree (branch `chromium`) with the Linux
   args, then `scripts/linux-read-set.py out/Base` (sources/inputs/public/data of targets reachable from
   //chrome:chrome + .gn/.gni + grd refs).
2. `scripts/linux-restore-set.py`: missing-in-fork ∩ Linux-read, minus removed features (manifest
   dir/remove rules, gone dirs with feature deletions, same-stem siblings of feature-deleted files);
   then `git checkout <base> --pathspec-from-file=...`.
3. GN: `scripts/gn-fix-loop.py --apply` (deletes single-entry deps on targets/dirs the fork removed, only lines GN
   evaluates, only inside the reported target; refuses anything not removed vs `chromium`). `fix-gn-refs.py --base chromium` exists but on the whole
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

AppImage/Flatpak (not planned; the .deb/.rpm cover the main distros), the signed apt repo (phase 2), a windowed
X11/Wayland/WSLg run of the packages (desktop entry, start menu), the setuid sandbox on a real (non-WSL) Ubuntu 24.04
with AppArmor's userns restriction; the third-party/web-tests prune must keep what Linux reads.
