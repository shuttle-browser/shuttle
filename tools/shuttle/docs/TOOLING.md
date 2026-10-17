# Tooling and gotchas

The tooling lives in the fork at `src/tools/shuttle/` (scripts, GN args, prune manifests, docs, branding). The scripts are
repo-relative: `scripts/lib/paths.sh` (shell) and `scripts/shuttle_paths.py` (Python) resolve `SHUTTLE_SRC` = the fork
(`src/`) and `SHUTTLE_ROOT` = its parent (the gclient checkout: `logs/`, `release/`, `sccache-cache/`); both can be
overridden by env. Machine-local settings live in `~/.shuttle/`: `env` (sourced by both resolvers: `SHUTTLE_BUILD_HOST`
= ssh alias of the Linux build host, `SHUTTLE_BUILD_HOST_IP` = its sccache-dist scheduler address,
`SHUTTLE_BUILD_HOST_TOOLS` = this directory on the host relative to `~`), `sccache.toml` (client config, see
`config/sccache.example.toml`), the toolchain archive and the default profile. Tree-wide scripts (`prune*.py`,
`fix-gn-refs.py`) skip `tools/shuttle/`; `rebrand.py` only touches its explicit file list.
Team-only notes go in the git-ignored `_docs/`.

**Mac build dirs (2026-10-13):** `out/Sccache` (dev args + sccache-dist) is the dev build; the local-only `out/Dev` was
dropped. Scripts that read a dev build (`prune.py`, `prune-tests.py`, `gn-fix-*.py`, `build-errors.sh`) default to
`out/Sccache` (`SHUTTLE_BUILD_DIR` overrides). Older notes below that say `out/Dev` mean the dev build; Linux keeps `out/Dev`.

## Fork strategy

- **Merge upstream, never rebase.** Branch `chromium` = one snapshot commit per stable tag (no upstream history; the
  full history is ~50 GB): `git fetch --depth=1 origin refs/tags/<new>:refs/tags/<new>`, then on `chromium`
  `git commit-tree <new>^{tree} -p chromium -m "Chromium <new> (snapshot)"`, move `chromium` to it. Diff of two
  snapshots = the upstream change. `main` merges `chromium`; modify/delete conflicts resolve to the deletion.
  `build/util/lastchange.py` falls back to the newest commit (snapshots have no `Change-Id`): keep that edit.
- **Keep the engine close to upstream.** V8, Blink, `content/`, `net/`, `third_party/` stay as unmodified as possible
  (security fixes must merge cheaply; agents browse arbitrary sites). Deep cuts go into the shell (`chrome/`,
  `components/`); new fork code goes into its own dirs (`chrome/browser/shuttle/`) so merges rarely touch it.
- **Two mechanisms:** prune manifests (`prune/*.txt` + `scripts/prune.py`) delete whole areas; code edits are normal
  fork commits. **Remove everything first, then build once** (`-k 0`), with seconds-cheap checks in between
  (`git grep` for dangling symbols, `gn gen`, `gn check`): building per step recompiles the dependents of edited
  shared headers again and again. Commits stay per area (merge conflicts stay per area), not each guaranteed to build.
- **Tests:** tests of removed code are deleted. Only the `chrome` target is guaranteed to build; Chromium's own test
  suites aren't run. Resources (grit, rollup, TS read undeclared inputs) are deleted only with their feature and
  `.grd` entries, never by the "unused" analysis.
- After big prunes do a clean Release build: an incremental build doesn't re-run actions, so a deleted but undeclared
  action input only shows on a clean build.

## GN args rationale

- **Trim the shell, not the engine; prefer runtime defaults over build-time removal.** Every non-default GN arg is an
  untested config: keep one only if it helps measurably, and drop an arg that breaks the build rather than patching
  around it. Args blocked by upstream asserts on Mac are commented in `config/args-common.gn`.
- **Never turn off:** printing (CDP `Page.printToPDF`), `enable_pdf` (agents read PDFs), extensions (DevTools and
  extension users), `enable_swiftshader` (software GPU for headless/VMs), supervised users (asserted on).
- **Rejected:** x86 SIMD flags (arm64), `-O3` for third-party / `-ffp-contract=fast` (no gain on Apple silicon;
  fp-contract changes float results vs Chrome), Widevine (needs Google's signing).
- **Privacy Sandbox fully off** (feature defaults, sandbox reported restricted so no consent UI, Topics / Protected
  Audience / Attribution Reporting / Shared Storage / Private Aggregation off). Kept: CHIPS, storage partitioning, the
  default third-party-cookie policy (web compat; agents need working SSO).
- **Release:** official, arm64, open codecs only; no PGO (needs `checkout_pgo_profiles`) and no ThinLTO (its framework
  link needs more than the Mac's 18 GB). Revisit for 1.0 (~10-15 % speed).

## Build

- `config/args-common.gn` + `config/args-{dev,release}.gn` are **the** GN args.
  `scripts/build.sh` writes them into `out/<Dir>/args.gn` and re-runs `gn gen` when they
  change. Six args are commented as "blocked": upstream asserts or unconditional deps on Mac,
  so they need source surgery instead.
- `scripts/build.sh [dev|release] [autoninja args]` runs under `nice` and `caffeinate`, logs to
  `$SHUTTLE_ROOT/logs/build-<Dir>.log`, ends with `BUILD EXIT <rc>`. Run it in a visible workerdeck
  shell (`shell_run`). Use `-j 6` on the Mac (2026-10-06 11:00; the build host `-j 12`) unless told otherwise; add `-k 0` to collect all
  errors in one pass. Incremental B1 rebuild (omnibox headers everywhere) took ~11 min of
  compile at `-j 12` once the machine was awake.
- `scripts/build-watch.py <Dir> [--step 10] [--stall 10]`: **the** build Monitor (macOS + Linux).
  Every stdout line is a notification, so it stays quiet: progress every `--step` % with rate/ETA
  (skips Siso's inflated first total), the first error of each FAILED step deduped by message,
  each new sccache fallback/remote-failure reason, the build host health for `Sccache` (ssh status, only on
  change), "STALLED" after `--stall` min without a finished step, and one summary line at
  `BUILD EXIT` (failure counts by message, sccache remote/local split from
  `$SHUTTLE_ROOT/logs/sccache-stats-Sccache.txt`). Reads only new output unless `--from-start`.
  Arm: Monitor `scripts/build-watch.py Dev` (timeout 30 min; re-arm on expiry).
  Expires after 30 min; re-arm. siso's step total shifts while it schedules, so early ETAs
  are low. Error details: `out/Dev/siso_output`.
- **Run long builds where a workerdeck reload can't kill them.** A workerdeck hot reload/restart
  ends every `shell_run` shell ("server_stopped"); it killed two B2d builds. The maintainer avoids reloads
  during builds; Siso resumes where it stopped, so just restart.

## Remote builds (sccache-dist)

The default Mac build mode: the Mac preprocesses, a Linux build host compiles, links stay local.
- Build host: `scripts/sccache-dist-host.sh install` (systemd units from the templates in `config/sccache/systemd/`,
  filled with the host user, `$HOME` and `SHUTTLE_BUILD_HOST_IP`), `install-sudoers` (passwordless start/stop),
  `status`, `logs`. Scheduler on :10600, two servers on :10501/:10502; configs (with secrets) in `~/sccache/conf/`.
  Never expose an unauthenticated listener: sccache is guarded by the client token and scheduler-signed server JWTs.
- Mac: `scripts/sccache-toolchain.sh` builds the clang toolchain archive; `~/.shuttle/sccache.toml` from
  `config/sccache.example.toml` (one `path_override` pair per out dir, see the gotcha below);
  `nice -n 10 taskpolicy -c utility scripts/build-sccache.sh -local_jobs=<host threads> -k 0` builds `out/Sccache`
  (`SHUTTLE_KIND=release` → `out/Release`).
- **`-local_jobs` is capped by the host's RAM, not its slot count** (2026-10-15): the build host has 12 logical CPUs and
  23 GB; the two servers advertise 24 slots. content/browser TUs need 1.7-2.6 GB each, so 16+ jobs swap hard (550 MB/s
  swap-out, near-OOM). Use `-local_jobs=12` alone, `10` while a host build runs alongside.
- Watch: `scripts/build-watch.py Sccache` (Mac), `scripts/sccache-dist-watch.sh` (quiet watcher), `scripts/buildhost-temp.sh`
  (WSL2 hardware sensors through the Windows host).
- NativeLink/REAPI (siso remote execution) was tried first and dropped: much slower than sccache-dist here
  (removed 2026-10-16; in git history before that).

## Removal workflow (worked for A, B1, sweep 1, B2a, B2c)

1. Scout (one Sonnet agent, scoped dirs, read-only) → confirm borderline items with the user.
2. Manifest `prune/<name>.txt` with `remove` rules → `scripts/prune.py [--apply]`.
3. `git add -A` (not `build/util/MINIBRANCH`) → `scripts/fix-gn-refs.py --apply`.
4. Manual GN fixes, then `gn gen out/Dev` until clean: generated inputs (`$root_gen_dir/...pak`
   in `chrome/chrome_paks.gni`, `.grdp` lists in `ui/webui/resources/BUILD.gn` and
   `chrome/test/data/webui/BUILD.gn`, `mojo_files` lists, json_to_struct/mojom targets left
   with empty sources).
5. `scripts/scan-webui-imports.py` (3s) for TS imports of deleted files.
6. C++/TS surgery (parallel agents need explicit file ownership + a shared brief; template:
   `_docs/agent-briefs/b1-surgery-brief.md`), verify each file with `scripts/syntax-check-file.sh`.
7. One `-k 0` build, fix FAILED steps, GUI smoke test with a fresh `/tmp/sh-profile`, commit.

## Prune and fix scripts

- `scripts/prune.py [--apply] prune/<manifest>.txt`: rule types `remove`, `dir`, `segment`,
  `suffix`, `ext`, `deadtree`, `deadfiles`, `keep`. `remove <path>` deletes unconditionally; a
  manifest with only `remove`/`keep` runs in seconds. Other rules delete only files no build
  config reads ("used" = siso inputs ∪ deps log filtered to live outputs ∪ gn files ∪ files
  referenced by `.grd` ∪ DEPS hooks ∪ a temporary official config). Resource files (images,
  TS/JS/CSS/HTML, JSON, `.xtb`, `.grd`) are only pruned by `dir`/`remove` rules. Run after a
  successful Dev build so the deps log is current.
  **"Used" is the Mac build only:** since Linux is back, a full `--apply` of `platforms.txt`/`deadcode.txt` would
  delete ~3k files Linux builds (ozone, dbus, `*_linux`). Dry-run, intersect with the files you mean to touch (after a
  merge: `git diff --name-only --diff-filter=A main <branch>`), and turn the safe ones into `remove` rules (155: 59 → 11).
- `scripts/fix-gn-refs.py [--apply] [scope ...]`: strips dangling references to paths deleted by
  the *staged* change; reports the rest as `FIX`. 4s for the whole tree.
- `gn check out/Dev --fix` adds direct deps lost with removed `public_deps`. ~1,000 "Source file
  not found" errors and a few ChromeOS include errors predate batch A; ignore.
- `scripts/build-errors.sh <output-substring>`: errors of matching FAILED steps.
- `scripts/syntax-check-file.sh [--flags-from <failed-step-substring>] <src.cc>...`: `-fsyntax-only` with each
  file's own flags from a cached compile DB of `$SHUTTLE_BUILD_DIR` (default `out/Sccache`; `/tmp/sh-compdb<dir>.tsv`,
  rebuilt from `ninja -t compdb` when `build.ninja` changes; ~2.5s/file), or with a failed step's flags from
  `siso_output` (`--flags-from`, for files not in the graph). Exit 1 on compiler errors or a file without a command,
  2 on setup errors. Load-limited since B2d (4 parallel agents pegged the build Mac): at most
  `SHUTTLE_SYNCHECK_SLOTS` (default 2) checks machine-wide, under `nice`. Read-only, safe without a
  build running, but generated headers are from the last build (edited `.mojom` = stale).
  To check against edited mojoms, regenerate bindings into a temp dir and add `-I` (B1 agent 2
  did this in `/tmp/mj`).
- `scripts/add-include.py <file> <header>` (cwd `$SHUTTLE_ROOT/src`): sorted include insert.

## Linux packages (.deb/.rpm)

`scripts/package-deb.sh <shuttle-linux-x64.tar.xz> <version> [dest]` builds `shuttle_<v>_amd64.deb` and
`shuttle-<v>-1.x86_64.rpm` (each with a `.sha256`) from the released tarball. The packages are therefore built from the exact
bytes users download, and the script checks the tarball's `.sha256` first. It uses nfpm with `config/nfpm-shuttle.yaml`,
rendered by envsubst; the maintainer scripts are in `assets/linux/deb/{postinst,postrm}` and are shared by deb and rpm.
A lintian pass at the end is advisory. Each run takes about 2 min (xz). `dest` defaults to the tarball's directory.

- **Needs:** nfpm, installed as the release binary into `~/.local/bin` after checking it against `checksums.txt`
  (2.48.0). Also dpkg-dev, for the dpkg-shlibdeps cross-check. lintian is optional.
  Env: `SHUTTLE_NO_RPM=1` (deb only), `NFPM=<path>`.
- **Version:** use the Shuttle version (`155.1.0`, see "Releases" for the scheme), not the full Chromium version, which
  goes into the description. Test builds use `155.1.0~test1`, because "~" sorts before the release. The deb Version gets
  nfpm's release suffix (`155.1.0-1`), but the file name doesn't. 155.x sorts above 0.1.x, so upgrades from 0.1.4 work.
- **Layout:**
  - `/opt/shuttle/` holds the tarball tree, minus `README-LINUX.txt`. Shared libraries are 0644.
  - `chrome-sandbox` is 4755 root:root inside the archive, and postinst re-asserts it.
  - `/usr/bin/shuttle` is a symlink.
  - `/usr/share/applications/shuttle.desktop` (Exec=/usr/bin/shuttle) and hicolor icons 24/48/64/128/256 named `shuttle.png`.
  - `/usr/share/doc/shuttle/copyright` (DEP-5).
  - `/usr/share` files are listed one per entry, so the rpm doesn't claim system directories like
    `/usr/share/icons/hicolor`.
  - postrm never touches `~/.config/shuttle`.
- **Gotcha: Depends/Requires are curated, and only the package *names* are checked automatically.**
  - dpkg-shlibdeps on a new build host overstates minimum versions:
    - libc6 >= 2.34 comes from the libdl/libpthread stubs, but the real maximum is GLIBC_2.28 (the agent-browser
      sidecar; shuttle itself needs 2.25).
    - libgbm1 >= 21.1 comes from *weak* symbols, but Debian 11 only has 20.3.5.
    - libxcomposite1 >= 0.4.6 comes from the host's shlibs file.
  - It also only knows the host's t64 names.
  - So minimums follow upstream Chrome's expected deps (same sources, same bullseye sysroot), with libc6 >= 2.28, and every
    renamed package gets a t64 alternative (`libasound2t64 | libasound2`).
  - The rpm Requires are sonames (`libX.so.N()(64bit)`, `libc.so.6(GLIBC_2.28)(64bit)`, `libnss3.so(NSS_3.39)(64bit)`).
  - Not listed: the libraries we ship (libEGL, libGLESv2, libvulkan, swiftshader) and the Qt shims' deps. Chrome loads
    `libqt5/6_shim` only when Qt is installed, so they stay optional.
  - **Every build cross-checks the lists:**
    - every package from dpkg-shlibdeps must be in the deb list;
    - every ELF NEEDED soname not shipped by us must be in the rpm list;
    - the maximum `GLIBC_` symbol must be <= 2.28.
  - On a mismatch the build fails with `DEPS: … missing`. After a Chromium merge, add the package (deb, plus a t64
    alternative if its name ends in t64) and the soname (rpm). Raise libc6/GLIBC only when the maximum `GLIBC_` goes
    past 2.28.
- **Recommends:** `libgtk-3-0t64 | libgtk-3-0 | libgtk-4-1`, ca-certificates, fonts-liberation, xdg-utils; the rpm gets
  the same as weak deps. GTK is dlopened. Headless and agent use runs without it: Debian 12 with `--no-install-recommends`
  installs 60 packages instead of 233, and smoke-mcp is clean.
- **Repo source** (build switch, off by default):
  - Enable with `SHUTTLE_PKG_REPO_SOURCE=1 [SHUTTLE_PKG_REPO_URL=…]` (default `https://pkg.shuttlebrowser.com`). The key
    comes from `assets/linux/repo/shuttle.asc` (gpg on the build host dearmors it for the deb).
  - The deb ships it as `/usr/share/keyrings/shuttle-archive-keyring.gpg`, the rpm as `/etc/pki/rpm-gpg/RPM-GPG-KEY-shuttle`.
  - deb, on `configure`: postinst writes `/etc/apt/sources.list.d/shuttle.list` (marker comment,
    `deb [arch=amd64 signed-by=…] <url>/apt stable main`). rpm, on install/upgrade: `/etc/yum.repos.d/shuttle.repo`
    (`<url>/rpm/stable`, `gpgkey=file:///etc/pki/rpm-gpg/…`). Skipped if any source already mentions the URL.
  - apt purge / rpm erase removes the file, but only if it still carries the marker.
  - Enable it only once the repo is live; a source pointing at a dead repo breaks `apt update` / `dnf`.
  - Once it has shipped, **every later package must ship the key too**. Otherwise an upgrade deletes the keyring the list
    points at, and `apt update` fails on the signature.
- **Test, with no Docker:**
  - Use throwaway WSL distros from minimal Docker Hub images, fetched through the registry API; `wsl --import` each one.
  - Install as root, then run Shuttle as a normal user.
  - Run headless smokes (smoke-mcp, smoke-session-cdp) from the build distro on a non-9222 port. WSL distros share one
    network.
  - Exercise the setuid path with `--disable-namespace-sandbox`. Negative control: with mode 0755 Shuttle aborts with the
    FATAL "SUID sandbox helper … not configured correctly".
  - Then upgrade, remove and purge, and check the profile is kept.
  - Unregister each distro afterwards.

## Package repo (pkg.shuttlebrowser.com)

- **Layout** (R2 bucket `shuttle-packages`, Silkweave account, custom domain `pkg.shuttlebrowser.com`):
  - apt at `/apt` (suites `stable` + `testing`, component `main`, amd64; one shared `pool/`), dnf at `/rpm/<suite>`
    (`/rpm/shuttle.repo`, `/rpm/shuttle-testing.repo`).
  - Key at `/shuttle.asc` (armored, for dnf) and `/shuttle.gpg` (binary, for apt `signed-by`); `/index.html` has the
    setup lines.
- **Signing key:** "Shuttle Browser packages <packages@shuttlebrowser.com>", RSA 4096, expires 2031-10-12, fingerprint
  `81D0827DDC5004E5CCCD674943FDBF62DB5D8304`. The public key is `assets/linux/repo/shuttle.asc`. The private key and its
  passphrase live only in the maintainer's Mac keyring (backup: Bitwarden). Before expiry, extend it (`gpg --quick-set-expire`),
  re-export `shuttle.asc` and re-run the publish script with no files, so the metadata and key are re-uploaded.
- **Publish:** `scripts/publish-packages.sh [--suite stable|testing] [--dry-run] <deb> <rpm>` on the Mac.
  - It signs the rpm with `rpmsign` (brew `rpm`; v4 RSA/SHA256 header signature) and builds the metadata on
    `$SHUTTLE_BUILD_HOST` (`apt-ftparchive`, `createrepo_c --general-compress-type=gz`; needs `apt-utils createrepo-c`).
    Then it signs `InRelease`/`Release.gpg`/`repomd.xml.asc` on the Mac and uploads the changed files with wrangler.
  - Upload order: packages and hashed metadata, then signatures, then `InRelease`/`repomd.xml` last.
  - Packages and `repodata/<hash>-*` are cached as immutable; everything else is `no-cache`, so apt never sees a stale
    `Packages` next to a new `InRelease`.
  - The master copy is `$SHUTTLE_RELEASE/pkg-repo` (`.state/`: suite lists, rpm input hashes, upload manifest). R2 is
    the published copy; nothing lists the bucket. Versions are immutable: a file name already published with other bytes
    is refused.
  - Release flow: publish to `testing`, install-test, then publish the same files with `--suite stable`.
  - dnf (`repo_gpgcheck=1`) asks once to import the key. Without `-y` and with no tty it waits forever, e.g. an
    unattended `dnf makecache` after installing a package with the repo switch on. The installer pre-imports the key
    (`rpm --import`). The package can't do that itself: a scriptlet can't run `rpm` (the rpm database is locked).
  - Needs `SHUTTLE_CF_ACCOUNT` in `~/.shuttle/env`, wrangler logged in (run from `$SHUTTLE_ROOT/shuttle-website`), and
    `pinentry-mac` as the gpg-agent pinentry (the passphrase dialog; no tty in agent shells).
- **ssh joins arguments:** `ssh host bash -s -- "$a" "$b c"` sends one command line, so `"$b c"` arrives as two words.
  Pass lists as separate words and rebuild them on the remote side (`shift; LIST="$*"`).
- **Installer** (`curl -fsSL https://get.shuttlebrowser.com | bash`): source `assets/install.sh`, copied to the website's
  `public/install.sh`; the website Worker serves it for the host `get.shuttlebrowser.com`. Copy and deploy the website
  after every change.
  - Counting (2026-10-14): the Worker logs installer fetches, `/dl/<file>` release downloads (302 to the GitHub
    release asset; the installer passes `?via=installer`, the site's buttons `?via=site`) and `/INSTALL.md` /
    `/llms.txt` reads into D1 `shuttle-stats` (no IPs, coarse client class). Checksums still come straight from GitHub.
    Read with the website's `scripts/stats.sh [days]`. New release file names must match `RELEASE_FILE` in
    `worker/index.js`, and `SITE.dmg/deb/rpm` in `src/data/site.ts` carry the version.
  - macOS: the DMG named in the latest release's `SHA256SUMS`, checksum-checked, `ditto` into `/Applications` (or
    `~/Applications`).
  - Linux: the apt/dnf repo with sudo; otherwise the tarball into `~/.local`.
  - At the end: "Start Shuttle now? [Y/n]", read from `/dev/tty` (with `curl | bash` stdin is the script). No terminal
    (CI, agents) → no prompt, no launch. `SHUTTLE_LAUNCH=ask|yes|no` overrides it. The launch passes
    `--shuttle-open-agents` (macOS `open -a … --args`, Linux `setsid -f`, stdin `/dev/null`), so it opens chrome://agents;
    Linux only with `DISPLAY`/`WAYLAND_DISPLAY` and not as root. Test the prompt with `expect`; `script(1)` with piped
    input sends EOF before the answer arrives.
  - Test without touching the real install: `SHUTTLE_APPS_DIR=/tmp/x bash assets/install.sh` (Mac),
    `HOME=/tmp/x SHUTTLE_METHOD=tarball bash install.sh` (Linux); the repo path needs a throwaway distro or VM.
    `curl -fsSL https://get.shuttlebrowser.com | SHUTTLE_CHANNEL=testing bash` installs from `testing`. The variables
    go on the `bash` side: `SHUTTLE_CHANNEL=testing curl … | bash` only sets it for curl and installs `stable`.

## Releases

**Versioning** (from the release after 0.1.4; Toby, 2026-10-16): `<Chromium major>.<minor>.<patch>`, e.g. `155.1.0`.
The major is the Chromium stable major the release is based on; the minor counts Shuttle feature releases on that
base and resets to 0 at each upstream merge (the first release on Chromium 156 is `156.0.0`); the patch is for fixes
and Chromium security patch pickups. 0.1.4 (Chromium 155) counts as 155.0, so the next release is `155.1.0`. The full
Chromium version (`155.0.8059.80`) goes into the release notes and package descriptions; the Mac app's
`CFBundleShortVersionString` stays the Chromium version (`chrome/VERSION`). Breaking changes to the MCP/CDP surface
aren't signaled by the major: call them out in `NOTES.md`. Tags: `v155.1.0`.

1. `SHUTTLE_KIND=release scripts/build-sccache.sh -local_jobs=12 -k 0` (or `scripts/build.sh release`): official
   `out/Release`; `gn gen out/Release` + `scripts/check-ninja-inputs.py out/Release` first. After a Chromium bump it is a
   full rebuild (~7 h via sccache-dist). Smoke it (`smoke-cdp.mjs` against `out/Release`).
2. `SHUTTLE_SIGN_ID=<Developer ID SHA-1> SHUTTLE_NOTARY_PROFILE=<notarytool profile> scripts/release-mac.sh`: builds the
   packaging tools, adds the agent-browser sidecar, signs, notarizes + staples app and DMG, verifies with
   spctl/stapler. Output `$SHUTTLE_ROOT/release/<chromium version>/`; copy the DMG to
   `release/v<X.Y.Z>/Shuttle-<X.Y.Z>-arm64.dmg`. Identity and notary credentials stay in the keychain.
3. Linux: `scripts/build-linux.sh release`, `package-linux.sh` (tarball), `SHUTTLE_PKG_REPO_SOURCE=1 package-deb.sh` (.deb + .rpm), Linux
   smokes and an upgrade test from the previous version (`docs/LINUX.md`).
4. `release/v<X.Y.Z>/`: `NOTES.md` (Chromium base, changes, downloads table, known limits), `SHA256SUMS`, then
   `gh release create v<X.Y.Z> -R shuttle-browser/shuttle --target <full SHA> --title "Shuttle <X.Y.Z>" --notes-file
   NOTES.md --latest <files>` (a short SHA fails with HTTP 422), `scripts/publish-packages.sh` for the apt/dnf repo,
   then the website's version and download links.

## Upstream merges

First merge 154.0.8037.129 → 155.0.8059.80 (2026-10-14): 26,748 upstream files changed; 12,877 modify/delete +
453 content conflicts (1,110 hunks); conflicts resolved in ~20 min (script + 4 Sonnet + 1 Opus), `gn gen` green
~65 min after the merge. Steps: AGENTS.md "After an upstream merge". Gotchas:
- **Rename detection carries upstream edits onto fork files.** A fork file copied from an upstream file the fork
  deleted counts as a rename: `resources/agents/BUILD.gn` received upstream's `assert(is_android)`. After the merge,
  diff every fork-added file (`git diff --no-renames --diff-filter=A <old snapshot> main`) against main.
- **Clean merge hunks land in the wrong place** when the fork deleted their context: a misplaced `} else {` in a
  BUILD.gn, a lost `</message>` in generated_resources.grd (grit fails and blocks most of chrome). Parse every
  changed .grd/.grdp with an XML parser before building.
- **Auto-resolved "ours-deleted" hunks drop upstream's new lines** (≤5 by default): a few were real deps on targets
  upstream moved code into (`views:idle_bubble`, `views/extensions:*_impl`). gn's "Label not in deps" and the link
  show them.
- **Hunk-level "ours" in test BUILD files splices upstream additions** into nonsense: take main's whole file instead.
- **gclient reads DEPS `version_file`s even when the dep's condition is false**: pruning them breaks `gclient sync`
  (prune.py now keeps them). The location-tags hook fails on DIR_METADATA mixins into pruned dirs and `tast_control`
  reads pruned chromeos/ files: both fixed in the fork (they had failed silently since the ChromeOS prune).
- **New prunes leave DIR_METADATA mixins dangling** (155: `metadata/browser_actuator/DIR_METADATA` →
  `components/browser_actuator/COMMON_METADATA`): the location-tags hook fails and gclient skips every later hook.
  After a prune, grep all DIR_METADATA `mixins:` for paths that no longer exist.
- **Removed gitlinks stay on disk** (155: upstream `agents/`): gclient says "no longer part of this client" but
  keeps the dir without `-D`; delete it by hand on each checkout (MAGWIN too).
- **Don't push to MAGWIN's checked-out branch** (`receive.denyCurrentBranch=ignore`): the ref moves under the
  worktree and `git status` shows the whole diff as staged. Push to `incoming/<branch>` and let SH-MagWin fast-forward.
- **Upstream reformats can silently undo fork edits** (155: Lit `.html.ts` re-wrapped by clang-format): the merge
  took upstream's template and the fork's removals came back (history embeddings, synced tabs). After the merge, list
  files where upstream's change is whitespace/quote-only (token-equal) but the result differs from main by tokens;
  restore main's version.
- **Upstream starts using files the fork pruned as unused**: siso aborts before building ("missing and no known rule").
  `scripts/check-ninja-inputs.py` lists them; restore with `git checkout chromium -- <path>`.
- **Restore a "missing input" only if the fork uses it.** check-ninja-inputs lists upstream files a BUILD.gn still
  references; 155's `shadow_overlay_view` and `intent_picker_bubble_view` were never instantiated by the fork and
  pulled in pruned side-panel / link-capturing code. Grep for users first; if none, drop the BUILD.gn entry instead.
- **Agents' BUILD.gn/file edits: run check-ninja-inputs on a scratch out dir**, not just `gn gen` (gn doesn't check
  that sources exist; a deleted file still listed in BUILD.gn blocked a whole Linux round at schedule time).
- **Renamed upstream files re-add pruned features**: 155 renamed `file_system.idl` → `.webidl`, so schema.gni got
  the pruned chrome.fileSystem API back (link error: generated registration without implementation). Compare
  pruned basenames (fork-old vs now) in list files like `api/schema.gni` after a merge.
- **Sweeps that paid off in 155** (each replaced several build rounds): includes new in upstream (absent in the old
  upstream) that the merge dropped and whose header exists (33 restored); `Register*Prefs` calls in browser_prefs.cc
  vs the old fork (a lost one = startup crash "unregistered pref", invisible to compile and link); token-level
  check for upstream reformat-only files that undid fork edits; JSON/XML parse of all merge-touched files.
- **Mac-only code compiles last and only on the Mac**: after Linux was green, the Mac still had 2 errors, an include
  the merge left only inside `#if BUILDFLAG(IS_CHROMEOS)` while 155 uses it under `IS_MAC`, and a new Mac-only
  Keystone promotion infobar (fork: `enable_updater=false`). Sweep: includes top-level in upstream but only inside an
  `#if` block in the merged file.
- **New upstream WebUIs need path mappings and Lit visibility** (`tools/typescript/path_mappings.py`,
  `third_party/lit/v3_0/BUILD.gn`), e.g. `cr_components/cr_lottie`.
- **Gatekeeper flags upstream data** (public CT log keys in `log_list.json`): allow-list with a `rule`, a `pack`
  entry doesn't cover secret findings.

## Gotchas

- **Linux host builds use `scripts/build-linux.sh`** (2026-10-14): `build.sh` is the zsh/Mac script; on the Linux host
  it fails without output (no zsh), and the old `logs/build-<Dir>.log` then looks like a fresh result. Check the
  `Script started on <date>` line of the log before trusting a step count.
- **Dry run after a BUILD.gn change reports 0 steps** (2026-10-14): `-n` can't regenerate the ninja files. Run
  `gn gen out/<dir>` (from the real path) first, then the dry run. Read the count from raw
  `autoninja -C out/<dir> -n --quiet=false chrome`: `build-sccache.sh -n` showed 0 for v0.1.3 where the real
  number was 158, and in agent shells autoninja adds `--quiet`, which hides the step line. A 0-step dry run on
  MAGWIN after a `git checkout` reset (mtimes only) still built 13 steps (v0.1.3): treat "0" as unknown, not
  "nothing to do".
- **`pkill -f` over ssh kills its own shell** when the pattern appears in the ssh command line (`ssh host 'pkill -f X;
  rm -rf /tmp/X'`). Put the commands in a script file, or use a pattern the command line doesn't contain literally
  (`'user-data-dir=/tmp/sh-pro[f]'`).
- **Call the Linux check scripts with an absolute path**: `linux-smoke.sh` and friends `cd` into `src/`, so a relative
  `scripts/…` call breaks their `$(dirname "$0")` lookups (smoke-cdp MODULE_NOT_FOUND, 0 clean pages). Non-interactive
  ssh on the Linux host has no node on PATH: export the nvm bin dir first.
- **chrome:// URLs are rejected as command-line arguments** (`ValidateLaunchUrlWebUnsafe`, except headless opt-in);
  inject the tab in `StartupTabProviderImpl` instead (fork commit 38).
- **`-Wunsafe-buffer-usage`** rejects libc calls like `strtoul` and pointer arithmetic in fork code; use base helpers
  (`base::HexStringToUInt`, `std::string_view`).

- **No `scoped_refptr` in DevTools target notifications** (2026-10-12): worker and tab `DevToolsAgentHost`s start with a zero
  refcount and call `NotifyCreated()` from their constructor, so `Target.targetCreated` reaches fork code (e.g.
  `SessionCdpFilter`) synchronously inside that constructor. A temporary `GetForId()` ref deletes the half-built host
  (dev: DCHECK `lock_impl_posix.cc:85 ... use-after-free`; release: silent UAF). Use `DevToolsAgentHost::GetWebContentsForId()`.
  Crash reports: `~/Library/Logs/DiagnosticReports/Shuttle-*.ips`; the DCHECK text is in the Crashpad `.dmp` (`strings`).
- **Astro scoped CSS vs runtime markup** (website): styles in a component's `<style>` don't reach elements created with
  `innerHTML`/`createElement`; wrap those selectors in `:global(...)` under a static parent (`RobotCheck`, `/pro` resets).
- **README video on GitHub** (2026-10-11): `<video>` only plays for GitHub-hosted uploads (`github.com/user-attachments/...`,
  web-editor drag and drop); a committed MP4 or a CDN URL is stripped or shown as a link. Use a GIF for autoplay
  (`ffmpeg ... palettegen=max_colors=192:stats_mode=diff ... paletteuse=dither=bayer`).
- **Worker custom domains vs existing DNS** (2026-10-11): `wrangler deploy` with `custom_domain: true` fails (code 100117) if the
  hostname already has A/CNAME records; delete them in the dashboard first (the wrangler OAuth token has no DNS scope).
- **Releases `/latest` skips pre-releases**: `releases/latest/download/<file>` links only work for a release not marked
  pre-release, with file names matching the uploaded assets.
- **Website mockups: flared tab corners** (2026-10-11): draw them as 13x12 boxes with a `radial-gradient` quarter ring
  (transparent → group color → toolbar color) laid over the tab's side borders, plus `box-shadow: 0 1px 0 <toolbar>`; the old
  border + offset box-shadow trick leaves notches and seams, worst at fractional scale (hero clip).

- **siso agent mode** (2026-10-11): with `CLAUDECODE`/`AI_AGENT` set, siso runs `--quiet` (no progress lines), so
  `build-watch.py` stays silent. `build.sh` and `build-linux.sh` unset both.
- **Unsigned sidecar in `out/Release` gets SIGKILLed** (2026-10-11): `release-mac.sh` strips agent-browser's ad-hoc
  signature so the pipeline can sign it, so page tools fail ("agent-browser exited unexpectedly", exit 137) when you
  smoke the unsigned `out/Release` app. Smoke the signed app from the DMG (or `out/Sccache`) instead.
- **Network mode smoke** (2026-10-11): `scripts/smoke-network.mjs <port> <udd> [lanIp]` against Shuttle started with
  `--shuttle-listen=0.0.0.0`. Wait for `<udd>/ShuttleAccessToken` before running it (a cold first start can take >5 s).
  The default-profile smokes (smoke-mcp etc.) must still pass without the flag.

- **Don't touch low-level shared headers for cosmetic removals** (2026-10-07): deleting two unused
  ids from `ui/color/color_id.h` (B2b side panel phase) recompiled most of Blink, because
  `ui/native_theme/native_theme.h` includes it and Blink's theme code includes that (~4,500-step
  incremental build instead of a few hundred). Leave dead ids/enum values in `ui/color/color_id.h`,
  `ui/base/**`, `ui/gfx/**`, `base/**`, `content/public/**`, `third_party/blink/public/**` unless
  something else forces an edit; batch such cleanups for an upstream merge (full rebuild anyway).
  Say so in every surgery brief.
- **macOS icons** (2026-10-08, rebrand): (1) in an Icon Composer `.icon` the whole 1024 canvas is
  the tile (the system masks/insets it), unlike legacy `.icns`/appiconset art drawn in an 824 px
  squircle; layers drawn on the legacy grid look small in the Dock. (2) The asset catalog's `Icon`
  (`Assets.xcassets/Icon.iconset`, document badge) also shows up in the app switcher: rebrand it too.
  (3) Layer SVGs must be standalone valid XML (actool/CoreSVG). (4) Compile with Chromium's
  `tools/mac/icons/compile_car.py` (Xcode 26 actool works; it warns about icns). (5) Renaming the
  product leaves stale `Chromium*.app` bundles in the out dir; delete them.
- **Changing English strings orphans translations** (2026-10-08): grit keys `.xtb` entries by a
  fingerprint of the English message, so edited messages silently fall back to English.
  `scripts/rebrand.py` shows the fix: parse the grd with `grit.grd_reader` before/after, remap the
  ids in every xtb, and edit the translated text the same way.
- **String prunes rebuild a lot too** (2026-10-08): `prune-strings.py --apply` on 6 grd/grdp files
  regenerates `generated_resources.h` etc. (siso showed ~5,250 steps, ~500 after restat skips;
  ~25 min remote on the Mac, ~40 min on Linux). Run it once per batch, right before the final build,
  and pause the Linux build while the Mac rebuilds.
- **Deleting a target can orphan a still-used source_set** (2026-10-08): `views:find_bar_owner` was
  linked into the browser only via `ui/webui_browser`'s deps; removing webui_browser compiled fine and
  failed at the final `libchrome_dll.dylib` link (undefined `FindBarOwner::*`). After deleting a
  target, `git grep` its deps list for targets with no other dependent and add them where the
  sources are still used.
- **Git load on the 334k-file checkout** (2026-10-07): `git status`/`git diff` stat every file
  and `git grep` uses all cores (200–460% CPU bursts). `$SHUTTLE_ROOT/src` now sets
  `core.fsmonitor=true` (built-in watcher daemon, `git fsmonitor--daemon status`),
  `core.untrackedCache=true` and `grep.threads=4`: `git status` 1.46 s → 0.07 s. Re-apply on a
  fresh checkout. Syntax checks are separately capped (`SHUTTLE_SYNCHECK_SLOTS`, default 2).
- **`[unsafe-buffers] file '...' bad test against path '/ac/src/'`** in `out/Sccache` logs:
  harmless. Remote compiles see the sccache-dist path `/ac/src/`, the plugin can't relate it to
  its source root for some third_party/gen files and skips the check (no error).
- **Don't build while sources are being edited** (surgery agents): a file edited during its
  compile can leave an object that ninja/siso considers up to date. Stop the build first; the
  finished objects stay valid.
- **Sub-agent temp files:** give each agent its own prefix (`/tmp/<ID>-*`); a shared
  `/tmp/b2b/ed.py` got overwritten by a parallel agent in B2b.

- **Lid closed on battery = no build.** `caffeinate -i` can't prevent clamshell sleep on
  battery; the Mac only dark-wakes every ~15 min (~15 compiles each). Keep it on power with the
  lid open, or `sudo pmset -a disablesleep 1` (undo with `0`).
- **Never send keystrokes to the GUI** (osascript/System Events): the user works in parallel and
  input lands in their apps. Drive the fork via CDP (`--remote-debugging-port=9333`, Node 24 has
  a global `WebSocket`; Python lacks `websockets`), and ask the user for omnibox/keyboard tests.
- **Sub-agents:** Sonnet 5.5 context is >200k (an agent ran to 290k without compacting); watch
  usage from the transcript JSONL. Long agents occasionally stall on the API stream watchdog;
  resuming via SendMessage stalled again — finishing the remainder directly was faster.
- **zsh:** `$V:r` is a filename modifier, write `${V}`; no bash-style array joins; `//chrome/*`
  globs fail unquoted (`gn check out/Dev '//chrome/*'`).
- **googlesource 429s:** rerun gclient with `-j 4`.
- **Metal Toolchain** for ANGLE: `xcodebuild -downloadComponent MetalToolchain` (done).
- **GN args:** `enable_nacl` no longer exists; don't set `use_lld=false`; official build needs PGO
  profiles or `chrome_pgo_phase=0`.
- **Throttling:** never `taskpolicy -c background` (efficiency cores only, ~14× slower).
- **Restoring files rewrites mtimes** and forces dependent rebuilds; prefer dry runs.
- **Full-tree `git grep`:** ~2s over `chrome components ui`; fine sequentially, never several in
  parallel from scouts.
- **Checking a failed grit/TS step without the build:** copy the command from
  `out/Dev/siso_output` (or the live ninja graph via `subninja`; stale `obj/**/*.ninja` mislead).
  For grit redirect `-o`, `--depfile`, `--depdir` to `/tmp` and drop `--write-only-new`,
  `--depend-on-stamp`, `--assert-output-file-list`. `.grd` entries may use backslash paths.
- **Dev builds crash on WebUI JS errors** and `DUMP_WILL_BE_NOTREACHED`. A WebUI page whose Mojo
  handler was removed fails at startup; disable the feature that loads it.
  `--disable-crash-on-webui-js-error` helps triage.
- **Static include-fix agents over-delete** generated `buildflags.h` includes and headers of
  still-existing classes; check before building.
- **`deadfiles` vs. GN args:** code behind a disabled arg can be pruned while still called; link
  errors reveal it; restore with `git checkout chromium -- <files>`.
- **Orphaned sources:** deleting a GN target can drop `.cc` files that other code still needs
  (B1: `ChromeColorsFactory` compiled only in the deleted `new_tab_page:impl`). Shows as a link
  error.
- **Deleted tab helpers / services can create others as a side effect** (SearchTabHelper created
  `OmniboxTabHelper`; ToastController CHECKs it at startup). Grep a deleted class's constructor
  for `CreateForWebContents`/`GetForProfile` before removing its attach site.
- **`build/util/MINIBRANCH`** is untracked noise; never commit it.
- **`pgrep -f` self-match:** a monitor's own command line matches its pattern.
- **macOS:** the checkout (`~/projects/shuttle-browser/shuttle`, was `~/chromium`) is excluded from Time Machine (sticky, survived the
  move) and Spotlight (path-based Privacy list: re-add after a move); XProtect scans each new
  binary (~0.5s).
- Old profiles from pre-batch builds trip migration DCHECKs; always use a fresh
  `/tmp/sh-profile`.
- **Stubbed KeyedService factories (B2a):** a factory's `DependsOn()` calls also *register* the
  factories it depends on. Stub or delete one and those others hit "Trying to register
  KeyedService Factory … after EnsureBrowserContextKeyedServiceFactoriesBuilt" at startup. Diff
  the old `DependsOn` list (`git show HEAD:<factory>.cc`) against
  `chrome_browser_main_extra_parts_profiles.cc` and register the survivors explicitly.
- **`-k 0` stops early** when a generator (grit, mojom, ts) or the link fails ("cannot make
  progress"). Before the build: scan `.grd`/`.grdp` `<part>`/`<include>` files and
  `siso query inputs -C out/Dev chrome` for missing sources.
- **Frameworks linked by deleted targets:** macOS frameworks (e.g. SystemConfiguration) may have
  reached the link only through a deleted target; add `frameworks = [...]` to the real users.
- **Agent sizing:** B2a's 3 ownership areas each needed 3 agent waves (~140k context per agent).
  Split big batches into 5–6 areas and ask the user for the agent cap up front.
- **`*.grd.gritdeps` (B2c):** `components/components_strings.grd.gritdeps` and
  `chrome/app/generated_resources.grd.gritdeps` are checked-in input lists for grit. When a
  `.grdp` is deleted, delete its line there too, or siso fails at schedule time with
  "`<file>.grdp`, needed by `…__strings_grit.d.stamp`, missing and no known rule". (This, not
  kept code, is why B2a had to restore `saved_tab_groups_strings.grdp`.)
- **Never break `gn gen` while agents run (B2c):** `syntax-check-file.sh` calls `gn outputs`;
  one bad BUILD.gn dep makes it print "no object" for *every* file, so all agents silently skip
  verification. After any BUILD.gn edit during surgery, run `gn outputs` on one file.
- **Agent sizing (B2c):** 6 areas, 6 Sonnet agents: 4 finished, 2 stopped at ~70% within 5–10
  min (WebAuthn + Safe Browsing, actor + misc). Heavy-reading areas (big dialog
  controllers/models) need half the scope of mechanical areas.
- **Windows firewall block rules are no protection on the build host (2026-10-07):** a block rule did not
  stop inbound to an http.sys (System, PID 4) listener; it stayed reachable over VPN and LAN.
  Prefer no listener, or bind to one address. Always verify exposure from the Mac with `curl`/`nc`.
- **Smoke test must end with `Browser.close` (B2c):** late KeyedService registrations
  (`AppSessionServiceFactory`) only fire at shutdown. Grep the browser log for FATAL afterwards.
- **Removed `IDS_*` pass syntax checks (B2c):** `out/Dev/gen/**/grit/*.h` are stale until grit
  reruns. Before the build, grep kept code for the removed grdps' message names.
- **Don't steal focus:** smoke tests run `--headless` (`"…/Shuttle.app/Contents/MacOS/Shuttle" --headless … &`):
  since commit 22 the first agent tab creates a window, which macOS orders in front even with
  `open -g` (2026-10-08). Windowed: `open -g -n -a …/Chromium.app --args …`;
  `smoke-cdp.mjs` opens targets with `background: true`. `open --stdout` appends to the log.
- **Per-session CDP (2026-10-08, `ac/session-cdp`):** `open-session` returns
  `ws://<host>/devtools/session/<id>`; `chrome/browser/shuttle/session_cdp_filter.cc` filters it (and page
  connections to leased tabs). `scripts/smoke-session-cdp.mjs [port]` (env `PLAYWRIGHT_CORE`, `AGENT_BROWSER` for
  the optional checks). Gotchas: commands a filter sends from inside an agent-host event must be **posted**
  (re-entrant `Target.detachFromTarget` during auto-attach crashed `TargetHandler::AutoAttach`); a
  `DevToolsAgentHostClient::GetTypeForMetrics()` must be DevTools/Extension/RemoteDebugger/Other (DCHECK in
  `chrome_devtools_session_base.cc`); a client that auto-attached with `waitForDebuggerOnStart` holds new
  navigations until it **disconnects**, even after `setAutoAttach false` (upstream). Emulation on a tab without a
  view crashed the browser upstream (fixed in `WebContentsImpl::SetDeviceEmulationSize`).
- **Page tools / agent-browser sidecar (2026-10-08, `ac/agent-browser`):** `scripts/fetch-agent-browser.sh
  <app>/Contents/Helpers [platform]` installs the pinned agent-browser (npm tarball, version check, LICENSE +
  NOTICE, ad-hoc codesign; `AGENT_BROWSER_SRC=<unpacked package>` skips the download). The browser finds it in
  `Contents/Helpers/agent-browser`, or `SHUTTLE_AGENT_BROWSER=<binary>` for dev runs. Relinking `chrome` keeps
  the Helpers copy. The page tool list lives in `config/agent-browser-tools.json`; after edits run
  `node scripts/gen-page-tools.mjs` and commit the regenerated fork file `chrome/browser/shuttle/page_tools_data.inc`.
  smoke-mcp expects
  36 tools (12 + 24). Gotchas: `AGENT_BROWSER_PIN_TAB=1` makes a fresh daemon open a stray tab; the daemon
  outlives the MCP process (send `close`, idle timeout as fallback).
- **zsh in the Bash tool:** `$VAR` holding several words does **not** split (`scripts/x.sh $FILES` passes one
  argument). Pass arguments literally or use `${=VAR}`.
- **Smoke order:** `smoke-agents-page.mjs` expects a fresh profile; after `smoke-mcp.mjs` in the same profile
  it fails on leftover agents (not a product bug).
- **`build-sccache.sh` needs a PTY** (`script`): run it via `shell_run`; from a plain Bash call it exits 1 after 0
  steps with `tcgetattr ... not supported`.
- **Moving the checkout loses siso's state unless you rewrite it first** (2026-10-13, `~/chromium` →
  `~/projects/shuttle-browser/shuttle`): `.siso_fs_state` (zstd, so `strings`/`grep` show nothing) is keyed by
  absolute path; the first siso run at the new path, even `-n`, drops every old entry → "command line not found in
  log", full rebuild (~48k steps). `.ninja_log` is truncated each run, no help. Before ANY siso run after the move:
  `siso fs export -C out/<Dir> > state.json` per out dir (and back up `.siso_fs_state*`, `.siso_deps`,
  `siso_explain*`), `mv`, rewrite old→new path in the JSON, `siso fs import -C out/<Dir> < state.json`, `gn gen`
  (Xcode paths like gperf are stored relative, so their `../` depth changes). Keep an old→new symlink:
  `.siso_deps` holds absolute depfile paths from some generators (v8 inspector_protocol → ~155 Blink files
  recompile otherwise). They are not plain text there, and an entry only changes when its action reruns, so neither a
  0-step dry run nor a real build proves it's gone (the Linux host dropped its symlink after real builds of every out
  dir and still rebuilt ~220 steps per dir: regenerated inspector headers, mostly unchanged). Keep the symlink until you're ready to pay that rebuild. Done this way on 2026-10-13 (`shuttle/src` → `src` one level up): dry runs matched (503 and
  6,174 steps); only 12 Blink gperf actions reran (the relative Xcode gperf path changes with the depth), 4 outputs
  changed. Linux build host, same day (`~/chromium` → `~/projects/shuttle-browser`): dry runs matched (Dev 1 → 1,
  Release 0 → 1: `about_credits.html` lists `build.ninja` as an input, so any `gn gen` makes it stale). Build only via the real path: the scripts resolve it (`pwd -P` / `realpath`), so a symlinked cwd is safe for
  them, but raw `gn`/`siso` from a symlinked cwd would register a second path. If the state is already lost: the siso "adopt" patch + pre-move explain logs
  (`_docs/siso-adopt/`) brought out/Sccache from 47,948 to ~1,060 steps. sccache's local cache did not help
  (0 hits; it never held a full build).
- **sccache-dist needs a toolchain entry per build dir:** `~/.shuttle/sccache.toml` `path_override`
  entries match the compiler path literally (`out/<Dir>/../../third_party/llvm-build/...`). A dir without its
  own clang/clang++ entries compiles everything on the Mac ("Could not package toolchain: Automatic packaging
  not supported"; `sccache --show-stats` "Failed distributed compilations"). `out/Release` had none until
  2026-10-10. siso labels sccache compiles `[local]` either way; check the stats.
- **`-local_jobs` is the real parallelism for sccache builds:** siso sees sccache-wrapped compiles as local
  actions, so `-local_jobs=N` caps compiles in flight on the build host at N; `-j`/`SHUTTLE_JOBS` doesn't raise it
  (2026-10-10: -j 24 with -local_jobs=6 kept the build host at 5-6 compiles, ~35% busy).
  It is also the only Mac-side cap: siso ignores `-j` above the core count ("Ignoring -j 24"), and until 2026-10-16
  `build.sh` put extra args after the target, where siso never read them. Use `nice -n 19 taskpolicy -c background
  scripts/build-sccache.sh -local_jobs 8`; unthrottled, a local build loaded the Mac to 58 and cut the maintainer off.
- **Official builds link with export lists** (`chrome/app/app.exports` for the app + helpers,
  `chrome/app/framework.exports` for the framework); dev builds don't use them, so pruning can break only the
  release link. After removals, check both lists for deleted files/symbols (2026-10-11 release failure).
- **Agent workerdeck shells die with the Claude session:** a long build started via `shell_run` is killed when
  the session ends (no `BUILD EXIT` in the log). Keep the session open or have the maintainer start the shell.
- **sccache local cache persists** (since 2026-10-10); `SHUTTLE_SCCACHE_FRESH=1 scripts/build-sccache.sh` wipes it.
- **The build host disk:** WSL2's `df /` shows the virtual disk size (~1 TB), not real space. Real free space is the
  Windows host drive: `ssh <build-host> df -h /mnt/c` (the WSL disk image `ext4.vhdx` lives on C:).
- **MCP server (2026-10-08, `ac/mcp`):** `scripts/launch-shuttle.sh [port] [dir]` starts the
  `out/Sccache` build on **9222** (persistent `~/.shuttle/profile`) and refuses if another browser
  holds the port (Shuttle owns 9222 by design; don't run Google Chrome with remote debugging).
  The repo's `.mcp.json` points Claude Code at `http://127.0.0.1:9222/mcp` (approve it once at session
  start). `scripts/smoke-mcp.mjs <port>` tests the server; popups triggered over CDP need
  `Runtime.evaluate` with `userGesture: true` (popup blocker). Official SDK check:
  `npx @modelcontextprotocol/inspector --cli http://127.0.0.1:9222/mcp --transport http --method tools/list`
  (run from an empty temp dir).
  `scripts/smoke-agents-page.mjs [port] [shotDir]` drives `chrome://agents` (fresh profile; create, edit +
  avatar, open tab, show tab, end session, delete, live refresh, search; screenshots optional). CDP gotchas
  for WebUI tests: pages opened with `background: true` are hidden, so a `cr-dialog` 'close' event can
  arrive late (after a reopen); type into `crInput.inputElement` + dispatch `input` to fire `value-changed`.
  Live-test findings (2026-10-08): the Claude Code MCP client caches tool schemas, so after
  renaming tools (`ac/agent-identity`) relaunch Shuttle with the new build and run `/mcp`
  reconnect. Tab groups aren't visible over CDP: group label/color/sync checks need a human check.
  Google's `/sorry` reCAPTCHA on our network is IP-level (plain `curl` gets it too), not an
  Shuttle fingerprint; the agent profile keeps the cookie once he solves it.
  Test-browser loop: close it before relinking `out/Sccache` (new renderers would load the new dylibs);
  launch with `open -g -n --stderr <log> ... --enable-logging=stderr` to catch renderer crashes; a dead
  (Aw-Snap) tab makes CDP `Runtime.evaluate` hang, so probe tabs with `timeout 5`.
- **Scripts added in sweep 2:**
  - `scripts/prune-tests.py`: computes the delete set from `gn gen --ide=json` (written to `out/TestScan`). Targets that test
    targets outside scope still need (blink, cc, net, headless) stay pinned. It skips `.gitignore`
    files: deleting them un-ignores CIPD test data, which `git add -A` then stages. It also skips
    submodule gitlinks and untracked (CIPD) `BUILD.gn` files. Afterwards run `fix-gn-refs.py`, then
    `gn-fix-unresolved.py` (since replaced by `gn-fix-loop.py`), then fix leftovers by hand:
    "Assignment had no effect" for unused GN variables, and `get_target_outputs` on removed
    targets (content_shell's Mac bundle).
  - `scripts/prune-policies.py`: upstream marks a deleted policy as `N: ''` in `policies.yaml`.
    A `policy_atomic_groups.yaml` file left empty must be deleted, because pyyaml loads it as
    None. Leave the device proto maps alone (not validated). On Mac only policies supported on Mac
    generate `key::k*` constants, so deleting non-Mac policies needs no Mac C++ change. Validate
    with `autoninja -C out/Dev components/policy:policy_code_generate`.
  - `scripts/prune-strings.py`: `.gritdeps` paths are repo-relative. Components grdps are also
    listed in `components/strings/BUILD.gn`, so run `fix-gn-refs.py` afterwards.
  - `git grep -E` has no `\w`: use `[A-Za-z0-9_]`.
  - A big `-w -E` alternation over the tree is very slow (minutes). Grep `-o` a broad pattern once,
    then intersect sets in Python.
- **Linux / two-machine tooling (2026-10-06, details in `docs/LINUX.md`):**
  `scripts/build-linux.sh`, `config/args-linux.gn`, `scripts/linux-read-set.py` (base read set on
  the Linux host), `scripts/linux-restore-set.py`, `scripts/gn-fix-loop.py`,
  `scripts/restore-includes.py`, `scripts/restore-strings.py`, `fix-gn-refs.py --base REF`.
  `restore-strings.py` wraps each restored message in its base `<if>` stack (an earlier version
  anchored by neighbour and put 43 Linux strings under `is_macosx`); afterwards run
  `scripts/grd-if-check.py` (messages whose `<if>` stack differs from branch `chromium`; expects no
  output). Stale `gen/` headers in `out/Dev` can hide includes of deleted mojoms on Mac.
  Every removal batch from now on must build on macOS *and* Linux before its commit.
- **Git:** fork remote `github` (private). Snapshot commits have no `Change-Id`; `lastchange.py`
  is patched to fall back to the newest commit (otherwise LASTCHANGE = 0, build year 1970).
- **Sweep 2 runtime gotchas:**
  - A factory may be registered *only* through `DependsOn()` from another factory. Deleting the
    dependent factory leaves it unregistered, and the first lazy `GetInstance()` hits the
    late-registration DCHECK in `dependency_manager.cc`. Before deleting a factory, check
    whether its `DependsOn` targets are registered anywhere else.
  - Replacing a function body with `return {}` can break callers that assume a non-empty
    result (`.front()`): `FluentLanguageModel` crashed Read Anything. Grep the callers.
  - Capture the browser log with `open -g -n --stdout /tmp/sh-smoke.log --stderr
    /tmp/sh-smoke.log -a …`. Read Anything is `chrome-untrusted://read-anything-side-panel.top-chrome/`.
  - `scripts/prune.py` `grd_refs()` protects every file a used `.grd`/`.grdp` references
    (grit reads `.xtb` files and some parts without declaring them as inputs).
- **Helpers added in B2c:** `scripts/gn-fix-unresolved.py` (since replaced by `gn-fix-loop.py`; gn-gen loop that strips deps on
  removed targets; run after `fix-gn-refs.py`), `scripts/delete-classes.py` (remove whole classes
  from a .h/.cc pair, handles split return types and `= default` dtors). Manifest generation for
  big batches: script it from the scout report with `git ls-files` + regex (see `prune/b2c.txt`),
  since `remove` rules match exact paths or directories only.
- **`git checkout` of an unchanged file still triggers a rebuild (B2d):** Siso decides by mtime.
  Reverting an agent's edit to `base/threading/thread_restrictions.h` with `git checkout` gave it
  a new mtime and scheduled ~30k steps instead of ~9k. Fix: `touch -t <old time>` on files whose
  content equals HEAD. Never edit `base/` headers for a batch (A4 did for one friend
  declaration; an overload in the caller avoids it).
  Same after `git checkout main && git merge --ff-only` from a branch (2026-10-16): 11,284 files of the merge diff
  got a new mtime, `out/Sccache` scheduled 58k steps. Recovery: files with that mtime minus those changed since the
  last green build → `touch -t <before the build>`; the rest → `touch -t` their last commit time
  (`git log -1 --format=%cd --date=format-local:%Y%m%d%H%M.%S`). `out/Release` went back to a 5 min build. Don't
  start (and kill) a build before the restore: those runs relinked `protoc` and regenerated headers, so
  `out/Sccache` still needs ~44k steps. Move branches with `git branch -f`.
  Testing a change on the other host before it's pushed: send it as a patch, `git apply` it there (uncommitted).
  After the push, `git merge --ff-only` refuses ("local changes would be overwritten") even when the content is
  identical: `git add` those files first (index = target), then the fast-forward leaves them and their mtimes alone
  (fork 45 on Linux, 2026-10-17: dry run 0 steps). Never stash or checkout them.
- **Deleted `.grdp` also lives in the `.grd` itself (B2d):** remove the `<part file=...>` line in
  `components/components_strings.grd` as well as the `.gritdeps` entry.
- **Checked-in TS API definitions (B2d):** `tools/typescript/definitions/*.d.ts` mirror the
  extension IDL/webidl; when an agent removes a member from the webidl, drop it there too, or the
  WebUI `build_ts` step fails (TS2741).
- **B2d sizing:** 4 Sonnet agents on 80-120 files each all finished in one pass (~35 min each,
  250-460k context). A file-disjoint ownership by directory plus per-agent file lists from
  `git grep` of removed symbols worked; cross-agent requests in the reports were few (~10).
- **Official-only build inputs (2026-10-11, first release build):** removals that dev builds never notice break
  only `is_official_build`: `chrome/app/app.exports` (helpers link with it), exported symbols in
  `chrome/app/framework.exports`, and the symbol list in `chrome/app/framework.order` (checked by
  `verify_chrome_framework_order`). When a removal deletes an entry point, grep `chrome/app/*.exports`,
  `*.order` and the signing parts (`chrome/installer/mac/signing/parts.py`) for it.
- **Release signing (2026-10-11):** `scripts/release-mac.sh` runs Chromium's `sign_chrome.py`; it calls `codesign`
  without `--force` (so the sidecar's ad-hoc signature is stripped first), rejects non-code files in
  `Contents/Helpers` (sidecar license files live in `Contents/Resources`), and its pre-notarization `spctl` check is
  off (macOS 26 rejects an unnotarized Developer ID app). It keeps only the DMG; the verify step mounts it.
  `codesign --strict` fails on any copy of the app from the DMG (`hdiutil makehybrid` sets
  `com.apple.FinderInfo`; installed Google Chrome fails the same way); Gatekeeper accepts it, so verify without
  `--strict`. Timing: small change ~45 s release build + ~5 min sign/notarize (two Apple round trips).
- **`smoke-mcp.mjs` timing flake (2026-10-11):** on a fresh profile the first one or two runs can fail "cookies
  isolated" / "opened tabs join the session" (fixed 1.5-2 s sleeps); reruns pass. Poll instead of sleeping.
- **en-US-only UI (B3, 2026-10-11):** `scripts/prune-locales.py` keeps `lang="en-US"` and `lang="en"` grit elements
  (`lang="en"` marks untranslated data packs and the English omnibox pedal synonyms xtb: dropping them breaks the
  build), deletes every other `<output>`/`<file lang=...>`, per-language `<if expr="'xx' == lang">` includes (Google ToS)
  and the `.xtb` files, and empties `<if>` blocks left behind. Hardcoded locale lists that GN can't subtract from an
  en-US-only `all_chrome_locales` (`Item not found`) were in `components/strings` (locale settings),
  `components/omnibox/resources`, `base/win/embedded_i18n` and `components/policy` (`policy_templates.gni`); after a
  merge, `gn gen` points at new ones. `scripts/rebrand.py` no longer remaps xtb ids.
- **What the on-device model stack still feeds (B3):** `third_party/blink/public/mojom/ai/*.mojom` import
  `services/on_device_model/public/mojom`, so `services/on_device_model` stays (unbuilt service:
  `use_on_device_model_service=false`, `build_with_model_execution=false`). `OptimizationGuideGlobalState` also owns the
  prediction manager for the TFLite models (omnibox scoring, language detection, permissions), so it stays with its
  on-device parts cut. `feature_registration.*` stays for the GenAI policy/settings prefs (`kGeminiSettings` etc.).
- **Renderer-requested interfaces:** when removing a browser-side implementation whose interface renderers still request
  (`safe_browsing::mojom::ExtensionWebRequestReporter`), keep the binder registered and drop the receiver instead of
  unregistering it.
- **`fix-gn-refs.py` and partly deleted dirs:** deleting only some files of a directory whose `BUILD.gn` is already gone
  (here `.xtb` files under `ui/android`) makes it strip refs to that dir's targets everywhere (dead `is_android` deps,
  harmless). Review the `del` lines when a batch also deletes data files.
- **`prune-tests.py` and other platforms' sources (B3):** the scan runs `gn gen` with the Mac's args, so Linux-only
  production sources named like tests (`stub_*`, `fake_*`: ozone stubs, `fake_input_method_context`, bluetooth
  dbus/floss fakes) looked unreferenced and were deleted (78 files, Linux build broke). It now skips any file that an
  enclosing `BUILD.gn` still names. Any Mac-only scan that deletes files needs the same guard, or a Linux `gn gen` check.
- **Agent avatars are PNG/JPEG only (2026-10-11):** `create-agent`/`update-agent` accept any `data:image/` URL, but the
  sandboxed image decoder doesn't decode SVG, so an SVG avatar leaves the tab-group chip and `chrome://agents` empty.
- **`allocate-tab` width/height = device emulation:** in a visible window the page renders at that size and leaves a grey
  band; `set-tab-viewport` has no reset (release the tab and allocate a new one). Leave them off for screenshots/footage.
- **Open codecs in release builds:** Shuttle release builds can't play H.264/AAC; web content we ship (site videos) needs
  a WebM (VP9/AV1, Opus) source first.
- **Window footage on macOS without input:** get the window id via `CGWindowListCopyWindowInfo` (owner "Shuttle", layer 0),
  then `screencapture -x -o -l<id> shot.png` or `screencapture -v -V <seconds> -l<id> clip.mov`; drive the page with MCP
  page tools / CDP (never keystrokes).
