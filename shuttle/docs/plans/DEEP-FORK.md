# Plan: Deep fork (removing what agents don't need)

Status: started 2026-10-04. Decision: Shuttle becomes a full fork of Chromium, optimized deeply for agents on macOS. Code that the use case doesn't need is removed physically, not just hidden.

## Strategy

- **Fork branch, merge upstream.** The fork lives on long-lived branches in the fork checkout (trunk `main`, topic branches `sh/*`). Upstream stable tags get **merged** in, not rebased onto. A tag-to-tag rebase of thousands of deletions is impractical; a merge keeps the conflicts local.
- **Keep the engine close to upstream.** V8, Blink, `content/`, `net/` and `third_party/` stay as unmodified as possible. Security fixes land there, and Shuttle visits arbitrary sites, so merging fixes must stay cheap. The deep cuts go into the browser shell (`chrome/`, `components/`).
- **Two mechanisms:**
  - **Prune manifests** (`prune/*.txt` + `scripts/prune.py`) delete whole areas. A file is deleted only if it matches a rule and no build config we use reads it. "Used" means the siso inputs, the full deps log, or gn's loaded files, for `out/Dev` plus a temporary official config. After an upstream merge, re-run the prune; modify/delete conflicts resolve to the deletion.
  - **Code edits** are normal commits on the fork branch. The `patches/` export is a readable record, not the source of truth.
- **Tests:** tests of removed code are deleted. Only the `chrome` target is guaranteed to build; we don't run Chromium's own test suites.
- **Remote (since 2026-10-06):** private `git@github.com:shuttle-browser/shuttle.git` (remote `github`).
  History starts at a snapshot commit: branch `chromium` = one commit with the tree of upstream tag
  154.0.8037.129 (`8690039f`, no upstream ancestry; full history is ~50 GB). `main` = the fork. Locally:
  `chromium` tracks `github/chromium`, `main` tracks `github/main`. `origin` stays googlesource (gclient).

### Upstream merge workflow (per stable release)

1. `git fetch --depth=1 origin refs/tags/<new>:refs/tags/<new>`; on branch `chromium`, commit a new
   snapshot: `git commit-tree <new>^{tree} -p chromium -m "Chromium <new> (snapshot)"`, move
   `chromium` to it, push. Diff of snapshots = upstream change.
2. `git merge chromium` into the fork branch (merge base = previous snapshot). Resolve conflicts:
   modify/delete means `git rm` the file.
3. Build `out/Dev` (Mac) and Linux, run `scripts/prune.py --apply` + `scripts/prune-tests.py`,
   rebuild (should be a no-op), commit, push.
4. `build/util/lastchange.py` is patched to fall back to the newest commit (snapshot commits have no
   `Change-Id`); keep the patch when merging.

## Phase A: Platform and dead-code prune (done 2026-10-04)

`prune/platforms.txt` covers:
- iOS and Android apps, Android WebView, Cronet
- ChromeOS / Ash, Chromecast, Fuchsia
- Windows- and Linux-only products such as installers, `ui/ozone` and `chrome/elf`
- platform-named files and directories anywhere (`android/`, `*_win.cc`, …) and Java/Kotlin

It never touches `third_party`, `build`, `buildtools`, `tools`, `testing` or `v8`. `remoting` is skipped until `enable_remoting=false` lands.

Dry run: **about 94,400 files to delete, about 19% of the tracked tree.** 93 matching files are kept because the Mac build reads them.

Lesson learned: `siso query inputs` misses discovered headers. The first attempt deleted `components/zucchini/type_win_pe.h`, which the Mac build includes. The script now also reads the full deps log.

Verification: after the prune, `gn gen` gives an identical `build.ninja`.

Results, as fork commits on `main`:
1. **Platforms:** 94,368 files.
2. **Dead directories:** 20,513 files (`prune/deadcode.txt`, `deadtree` rules). These are whole directories under `chrome`, `components`, `services`, `ui`, `device` and `extensions` that no build reads. Test-named directories are never deleted as a whole. Files that `.grd` files reference, such as `.xtb` translations, are protected because grit doesn't declare them as inputs.
3. **GN args round:** `config/args-common.gn`. `scripts/build.sh` now writes `args.gn` from `config/` and reruns gn gen when the args change. 32 args are flipped. 11 are blocked by upstream asserts or unconditional deps on Mac and are marked in the file: lens, compose, screen_ai, on-device translation, the speech service, background mode, captive portal, and 4 enterprise DLP args.
4. **Dead files:** 3,890 files (`deadfiles` rules). These are individual unread files. Build metadata, `.py`/`.sh` scripts and test-named files are kept.

Tracked files went from 502,677 to 383,907 (−24%).

Caveats:
- An incremental build doesn't re-run actions, so an action input that was never declared and got deleted only shows up on a clean build. Do a clean Release build after big prunes.
- Stale deps-log entries, for targets that are no longer built, are filtered against the current ninja outputs.

## Phase B: Bookmarks, full removal (next)

In scope, decided 2026-10-04:
- bookmark UI: the bar, star button, menus, side panel, `chrome://bookmarks`, keyboard shortcuts and context menu items
- the settings, the model (`components/bookmarks`, `chrome/browser/bookmarks`) and the omnibox bookmark provider
- **commerce / shopping and price tracking**, which store products as bookmarks
- the **bookmark sync type**
- the **`chrome.bookmarks` extension API**, plus `ManagedBookmarks` and the other bookmark policies
- **bookmark import**

Scale: about 175 non-test files use `BookmarkModel`, and 121 GN targets depend on `//components/bookmarks`.

Approach: **remove everything first, then build once.** Building after each step would recompile the dependents of edited shared headers again and again. One pass:
- Make all the edits.
- Run the cheap static checks, which take seconds:
  - `git grep` for dangling includes and symbols of the deleted code
  - `gn gen`
  - `gn check` for include/dependency mismatches
- Run a single Dev build with `-k 0` so every compile error shows up in one pass.
- Fix the errors in batches, each followed by an incremental build.

Commits stay per area for readability, but individual commits aren't guaranteed to build.

Work areas, from leaves inward:
1. Map dependents with `gn refs out/Dev //components/bookmarks/browser --all`, grouped by area.
2. Remove the UI surfaces: views, Cocoa menus, WebUI, the side panel and commands.
3. Remove the integrations: omnibox, commerce, sync type, extension API and policies, importer, NTP/history hooks, and power bookmarks.
4. Delete `chrome/browser/bookmarks`, `components/bookmarks` and `components/commerce`. Remove their prefs, strings (`.grd`/`.grdp`, then their `.xtb` translations) and histograms.
5. Final sweep: `git grep -i bookmark` outside third_party; anything that's left gets a documented reason.

Each area becomes one commit, so a later upstream merge conflicts per area.

## Phase C: Candidates

Scout results, tiered: `REMOVAL-CANDIDATES.md`.


Remoting (via the GN arg), the Google AI features group (`BUILD-OPTIMIZATION.md` group A), sync as a whole, Google account sign-in, translate, the reading list, payments/autofill server parts, and the Windows/Linux branches inside shared files.
