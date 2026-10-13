# Removal log

What each removal batch deleted, how the build was fixed, and what it left behind.
Decisions: `docs/plans/REMOVAL-CANDIDATES.md`. Open work: `_docs/NEXT-SESSION.md`.

## Fork commits (`git log chromium..main`)

1. Skip first-run and default-browser prompt (forced in `ChromeMainDelegate::BasicStartupComplete`).
2. Remove the missing-API-keys infobar.
3. Prune non-macOS platforms (94k files).
4. Prune dead directories (20.5k files).
5. Prune dead single files (3.9k files).
6. Restore resources with undeclared build inputs (~7.3k files back).
7. Fix the PDF viewer TS build with `enable_pdf_ink2=false`.
8. Batch A: bookmarks, commerce, Google AI stack, PWAs/apps, translate, Cast, sharing,
   payments UI, feedback/HaTS (~10.9k files).
9. Batch B1: AI mode, composebox, Omnibox Everywhere, WebUI NTP, Customize Chrome
   (1,067 files deleted + ~250 edited; 197k lines removed).
10. Cleanup sweep 1: leftovers of A and B1 (Instant stack, NTP backgrounds, …; 323 files,
    25k lines).
11. Batch B2a: sign-in, sync service, profile UI, incognito product, saved tab groups
    (2,014 files deleted + 353 edited; 424k lines removed).
12. Batch B2c: password manager, autofill, passkeys/GPM, SB password reuse, affiliations,
    actor login/form tools, data importer (5,572 files deleted + 299 edited; 1.18M lines removed).

13. Remove test targets wholesale (`scripts/prune-tests.py`; 2,301 testonly targets, 37,368 files,
    6.07M lines). Commit `6a6b76ceb7`.
14. Cleanup sweep 2: 924 policies, 13,918 IDS strings + 34 grdps, WebUI/C++ leftovers, two
    runtime crash fixes (1,309 files changed, 90k lines removed). Commit `b54b7c7b9c`.
15. Restore Linux: 2,166 files back, 27 edited, 1 deleted (441k lines added), build fixes, the
    `ExtensionConfigMapFactory` registration fix. Commit `9866fcd92b` (post-rewrite history, on
    `main`; squash of branch `ac/linux`).
16. Always-on remote debugging, no approval dialog, no automation infobar, `navigator.webdriver`
    always false, `RemoteDebuggingAllowed` policy removed. Commit `48370d6639`.
17. Batch B2d: sync, reading list, trusted vault, OTR/guest/system profiles. Commit `aaeb882acf`.
18. Batch B2b: user education/IPH, side panel (entries, `chrome.sidePanel`, framework), experimental
    WebUI browser, `chrome.gcm`/`instanceID` + invalidation, on-device AI consumers (GCM + Web Push
    kept). Commit `3482a77af5` (squash of `ac/b2b`).
19. First rebrand (interim name, replaced in commit 27; Silkweave; the signing team; `<old bundle id>`; logo v2),
    applied by `scripts/rebrand.py`. Commit `033bb9b90b` (single commit, fast-forwarded). Mac + Linux
    green, smoke clean. Details: `docs/plans/REBRAND.md`.
20. In-browser MCP server at `POST /mcp` on the CDP port (now `chrome/browser/shuttle/`, 11 tools:
    agent profiles = storage partitions, sessions = tab groups, leased tabs) + new tabs/noopener windows
    keep the opener's storage partition. Commit `007d52a391` (squash of `ac/mcp`). Mac + Linux green,
    `smoke-mcp.mjs` 27/27 + persist-check; the first live MCP test 2026-10-08 passed. Design:
    `docs/plans/MCP-V1.md`, `docs/plans/MCP-SERVER.md`.
21. Agent identity: an agent (= profile) has a name, group color and optional avatar; one tab group per
    agent (shared by its sessions), two-way sync with the tab strip (rename/recolor; an emptied title is
    restored); tabs the user drags into an agent's group are adopted (reopened in its storage, leased to
    its latest session), moved between agents = transfer. Commit `a1b652b125` (squash of
    `ac/agent-identity`, 3 commits). Mac + Linux green, smoke-mcp + persist-check, 3-mode smoke clean.
    Behavior changes: MCP tools `*-profile` renamed to `list/create/update/delete-agent` (`update-agent`
    new), `open-session` takes only the agent id; session groups show the agent's name (was
    `<profile> · <agent>`). Design: `docs/plans/MCP-SERVER.md` "Agent identity".
22. No startup window: `--no-startup-window` is the default for the process's own launch (unless URLs or
    `--restore-last-session`); the first agent tab creates the window. The remote-debugging keep-alive no
    longer needs a `--remote-debugging-*` switch (Linux/headless stay alive without windows); not taken
    for headful macOS (AppController keeps the app alive and releases on Quit, so Cmd-Q works). Commit
    `00b32091df` (squash of `ac/no-startup-window`, 2 commits). Mac + Linux green, smokes clean,
    `scripts/linux-nsw-check.sh` OK on X11/Wayland/headless; confirmed Dock click + Cmd-Q.
23. `chrome://agents`: the user's view of agents, sessions and tabs (Settings-style cr elements; create, edit,
    delete, avatar, open a tab as the agent, jump to a tab). Commit `707fa803f6` (squash of `ac/agents-page`).
24. Per-session CDP endpoints (`open-session` → `ws://…/devtools/session/<id>`: only the session's tabs and the
    agent's storage; page connections to leased tabs filtered too), agent viewports via device emulation,
    CSS-px capture, `WebContentsImpl::SetDeviceEmulationSize` null-view crash fix. Commit `58b6b928ab`
    (squash of `ac/session-cdp`). Design: `docs/plans/MCP-V2.md`.
25. Tab groups are agents: no manual groups; "Hand tab to agent" / "Take back from agent"; agent bubble;
    user-facing "group" strings say "agent"; restored tabs come back ungrouped. Commit `01fffd656f` (squash
    of `ac/agent-groups`).
26. Page tools via an agent-browser sidecar (24 curated kebab-case tools behind `/mcp`, one child per session,
    binary in `Contents/Helpers`, clean error without it); agent avatar on the group chip; dragging a tab out
    of its agent's group takes it back; `list-agents` summarizes uploaded avatars. Commit `9330681592`
    (squash of `ac/next`'s rest). Mac + Linux green on 23-26's final tree (3-mode smokes clean).
27. **Rebrand to Shuttle** (2026-10-09: `Shuttle.app`, `dev.silkweave.shuttle`, internal ids `shuttle`,
    logins reset): own code moved to `chrome/browser/shuttle` (namespace, prefs `shuttle.*`,
    partitions `shuttle-<id>`, MCP `serverInfo` `shuttle`), BRANDING/profile dirs/keychain, all remaining
    "Chromium" UI strings (translations remapped; the credit link keeps "Chromium"), help links →
    shuttlebrowser.com, dark toolbar logo + Settings "About" glyph, Linux binary/WM_CLASS/app_id/desktop
    name `shuttle`, About page without the update-check error (basic updater on macOS). Commit `8b80db862f`
    (squash of `ac/rebrand-shuttle` + `ac/rebrand-shuttle-linux`). Mac green + all smokes; Linux green
    (3 modes + windowed brand checks). Tooling: `scripts/rebrand.py` (parameters + earlier-brand rename),
    a one-off rename script (since removed). Behavior changes: existing agents/logins not migrated (fresh
    `~/.shuttle/profile`); MCP server key `shuttle` (tools `mcp__shuttle__*`).
    Behavior changes: no window or tab at launch (headless too); a second launch or URL args still open
    a window; on Linux closing the last window no longer quits (Browser.close or kill; accepted).
28. **Mac release: signing + DMG pipeline, official build links** (2026-10-11): `chrome/installer/mac/signing`,
    `sign_chrome.py`, `pkg-dmg`, entitlements back (parts: agent-browser sidecar; no app_mode_loader/shortcut
    copier); `chrome/app/app.exports` restored; removed symbols out of `framework.exports`/`framework.order`; no
    pre-notarization spctl (macOS 26), `import subprocess` fix. Commit `21603b7944` (squash of `ac/release-mac`).
    First notarized DMG built from it (`scripts/release-mac.sh`). Mac release + dev green, Linux green.
29. **`chrome://agents` is home** (2026-10-11): Dock reopen (no windows) opens `chrome://agents`, not an empty New
    Tab; Quick start card (Claude Code/Codex `mcp add` commands, MCP URL with the real port from
    `DevToolsActivePort` via `getMcpUrl`, example prompt, copy buttons; open while no agents, choice remembered).
    Commit `3e5d8913b8`. Mac + Linux green, smokes clean (Linux headless: cdp, mcp + persist, agents-page).
30. **Batch B3** (2026-10-11): en-US-only UI, Google actor framework, on-device/remote model execution (B2b 4b),
    Chrome Apps, experimental frames (waap/InitialWebUI, WebUI toolbar), profile leftovers, Safe Browsing cuts +
    SB off by default, on-device speech, test/string sweep (4,774 files changed, 4,366 deleted incl. 3,027 `.xtb`;
    −2.11M lines). Commit `c97374a5fc` (squash of `ac/prune-b3` + `ac/prune-b3-linux`). Mac release + dev and Linux green, smokes clean.

History rewrite (2026-10-06, for GitHub): the 15 commits were replayed unchanged onto a snapshot
root commit (`8690039f`); hashes above are the pre-rewrite ones (pre-rewrite branch, since deleted).
New hashes: commit 13 = `c96f7470bd`, commit 14 = `72b994abe1`.

Since the rewrite, `main` on `github` is the fork's trunk; topic branches are `sh/*`.

Tree: 502,677 → 334,159 tracked files at commit 14 (335,244 at commit 13 (before: 371,543). Before B2c: 377,115; before B2a: 379,129; text lines 64.6M → ~48.1M; Chromium's own code
(excl. vendored third_party, incl. Blink) −31.5%.

## Batch A (fork commit 8, 2026-10-04) — manifest `prune/features.txt`

Removed bookmarks, commerce, the four Tier 1 bundles, the Google AI stack (glic,
contextual_tasks, Lens, Compose, `services/{on_device_model,passage_embeddings,screen_ai}`,
on-device translation), payments/wallet UI, PWAs/apps, translate, media router/Cast,
sharing/send-tab-to-self/data_sharing/collaboration, feedback and HaTS.

Fix patterns from the 48 compile failures: missing includes that came through deleted
headers; removed generated `buildflags.h` includes where the flag is still used; stale
members/locals/unused functions (`-Werror`); `#if`/`#endif` mismatches; literal `false`
conditions (`-Wunreachable-code`); strings/overrides only present under a disabled buildflag
(screen_ai/OCR); upstream `#error` on Mac for `enable_browser_speech_service=false`.

Link/runtime fixes: unexportable-key sources restored and wired to
`enable_device_bound_sessions`; `UnexportableKeyServiceFactory` registered;
`kWebUIOmniboxAimPopup` default DISABLED (superseded by B1, which deleted it).

Behavior changes (agreed): no bookmark bar, web apps or Cast; Sad Tab shows only Reload;
`--app`, `--app-id`, `--load-apps` removed; `chrome://crashes`, `chrome://system`,
support_tool deleted; no HaTS; passkey QR codes don't render; File System Access extended
permissions need an explicit grant.

Deferred out of A: autofill credit-card payments (`components/autofill/**/payments`),
`content/browser/payments`, `services/on_device_model` (with the B optimization_guide stub),
`components/manta` (`services/image_annotation` needs it), `chrome/browser/performance_manager`.

## Batch B1 (fork commit 9, 2026-10-05) — manifest `prune/ai-mode.txt`

Decisions (user, 2026-10-04): Omnibox Everywhere deleted (hard-depends on AIM);
`searchbox.mojom` pruned in the same pass; WebUI NTP replaced by a blank page now; Customize
Chrome side panel deleted with the NTP; `components/touch_to_search` left for a dead-code
batch (Android-era legacy, not AIM).

Deleted: AIM eligibility/button services, AIM popup (views + WebUI), AI-mode page action and
toolbar toggle, `chrome://omnibox` AIM pages, AIM eligibility component extension;
composebox (`cr_components/composebox`, `components/omnibox/composebox`,
`composebox_features`), fusebox, `components/contextual_search`,
`chrome/browser/ui/contextual_search`; Omnibox Everywhere (all); WebUI NTP
(`new_tab_page{,_third_party}`, footer, new_tab_shared, customize_buttons, NTP modules,
one_google_bar, microsoft_auth, most_visited, ntp_tiles_internals UI, NTP page-load metrics,
NTP preload, browser_ntp_promos, `chrome/browser/ntp_customization` Android res); Customize
Chrome side panel; wallpaper search; `ChromeColorsService`/factory (only the NTP used it).

Kept on purpose: classic omnibox + WebUI popup, `components/ntp_tiles` +
`chrome/browser/ntp_tiles` (zero-suggest), `chrome/browser/search` (InstantService),
`chrome/browser/new_tab_page/{chrome_colors,prefs}`, `cr_components/{theme_color_picker,
customize_color_scheme_mode}` (Settings), NTP color mixers, `RealboxHandler` (WebUIBrowserUI
uses it), `chrome/browser/resources/new_tab_page_instant`.

Blank NTP: `NewTabURLDetails::ForProfile` (`chrome/browser/search/search.cc`) returns an
invalid URL for every profile, so `chrome://newtab` is not rewritten and `NewTabUI` serves it;
`NTPResourceCache` returns an empty page for `NORMAL` windows. Incognito/guest NTPs unchanged.

Surgery highlights:
- `searchbox.mojom`: only the classic surface remains (no attachments, tool/model modes, input
  state, AIM button, `kFocusedButtonAim`, `SuggestInventory`, contextual flags).
- `OmniboxPopupSelection::FOCUSED_BUTTON_AIM`, `OmniboxPopupState::kAim`,
  `OmniboxClient::{GetAiModeButtonService,IsAimPopupEnabled,IsOmniboxNextAimPopupEnabled}`,
  `AutocompleteProviderClient::{GetAimEligibilityService,GetAiModeButtonService}`,
  `OmniboxView::{ApplyFocusRingToAimButton,AimButtonVisible}` deleted.
- `omnibox_next_features` keeps classic/full WebUI popup, simplification, caret, energy flags.
- ~50 AIM/composebox/AskG features removed from `omnibox_features`; ~40 about:flags entries.
- `GetDisabledStarterPackIds` lost its AIM parameter; `@aimode` starter-pack data removed
  (enum id kept). `kShowAiModeOmniboxButton` pref and its synced-pref entry removed.
- Toolbar mojom: `LocationBarFlags.force_aim_button_focus_ring` removed.

Behavior changes: blank NTP; no Customize Chrome panel (themes via Settings, pinning via
context menus); omnibox placeholder not shown while focused; popup placeholder text no longer
cycles; AIM server suggestions always dropped.

How it went: scout (1 Sonnet) → manifest → `prune.py --apply` → `fix-gn-refs.py` → manual
GN/pak/grdp fixes → `gn gen` → 3 parallel Sonnet surgery agents with file ownership and a
shared brief (verified with `scripts/syntax-check-file.sh`) → one `-k 0` build: every compile
passed, one link error (orphaned `ChromeColorsFactory`). GUI smoke test passed (omnibox,
suggestions, keyword mode, blank NTP, Settings).

## Cleanup sweep 1 (fork commit 10, 2026-10-05) — manifest `prune/cleanup-1.txt`

Deleted: Instant/embedded search (InstantService, BrowserInstantController, InstantController,
SearchTabHelper, SearchIPCRouter, ChromeSearchNavigationThrottle, most_visited_iframe_source,
search_engine_base_url_tracker, `chrome/common/search`, `chrome/renderer/searchbox`, Instant
process model, `chrome-search://` scheme, new_tab_page_instant); NTP custom background services
and policy handler; `chrome://new-tab-page*` constants; ~60 ntp_features + 33 NTP flags; Omnibox
Everywhere virtual focus + Lens element ID; `kAimEligibilityServiceName`; ntp_tiles internals
WebUI; `cr_components/search`; `components/touch_to_search` + IPH; `components/apps`; shopping
segmentation model; `bookmark_bar.mojom`; about_sys/support_tool resources; dead
`ENABLE_COMPOSE` blocks; dead Settings/History load-time keys; translate-pref reads in Settings.

Kept: `search.cc` basics (`IsNTPURL`, `IsNTPOrRelatedURL` = `chrome://newtab`,
`GetNewTabPageURL` only for tests), `ui/search/omnibox_utils`, NTP color mixers (extension themes,
blank NTP, searchbox CSS), composebox paths in zero-suggest (shared helpers), NTP pref names theme
sync reads (now registered by ThemeService), policy templates.

How it went: manifest (172 files) → `fix-gn-refs.py` + 6 manual GN fixes (empty proto target,
empty source_set, 3 paks, a grdp) → 3 parallel Sonnet agents (brief:
`_docs/agent-briefs/cleanup1-surgery-brief.md`; agent 3 reused for agent 1's out-of-area list)
→ one `-k 0` build: green on the first pass. Smoke test found one runtime CHECK:
`OmniboxTabHelper` was created only by SearchTabHelper (ToastController CHECKs it); now attached
in TabHelpers. Lesson: deleted tab helpers can create other helpers as a side effect; grep their
constructors for `CreateForWebContents`.

Behavior changes: blank NTP has a localized title; theme color picker never shows a background
image; `NTPCustomBackgroundEnabled` policy is inert; the `SimpleMenuModel::AddSeparator`
NOTIMPLEMENTED log is upstream behavior on Mac (spacing separator in the app menu), not ours.

## Batch B2a (fork commit 11, 2026-10-05) — manifest `prune/b2a.txt`

Decisions (user): remove incognito entirely; password manager and autofill: full removal (batch
B2c); sync model/protocol layer: clean removal, but in the next batch together with B2c and the
OTR Profile type (CDP `Target.createBrowserContext` may break until the MCP service exists);
B2c extras accepted (delete Safe Browsing password reuse, `components/affiliations`; add a
Credential Management stub binder). `trusted_vault` and the sync engine moved to the next batch
(entangled with WebAuthn/passwords, which B2c deletes). Import Data dialog dropped with the
People page.

Deleted: sign-in UI/services (DICE, AccountReconcilor, investigator, account preview, intercept
bubbles, promos, all signin WebUIs, signin-internals, inline_login, gaia_auth_host,
`chrome/browser/enterprise/signin`, signin/Mirror header helpers, Google Accounts private API);
`chrome/browser/sync` except stub `SyncServiceFactory` (always null),
`DataTypeStoreServiceFactory`, `sync_start_util`, `chrome_syncable_prefs_database`;
`ui/sync`, sync-internals, `browser_sync`, `sync_sessions`, `sync_device_info`,
`sync_user_events`, `sync_tab_context`, `cross_device_pref_tracker`, cross-device theme
tracker; profile UI (`ui/views/profiles`, ProfilePicker, FirstRunService, customization,
profile_internals, guest pages, GAIAInfoUpdateService, downloader/statistics/shortcuts, Cocoa
profile menu); incognito product (menus, dock, Cmd-Shift-N, context-menu items, clear-data
dialog, enterprise incognito, connectability); saved/shared tab groups (+ Mac Tab Groups menu,
deletion dialog, comments and tabs-from-other-devices side panels, omnibox provider, IPHs);
Settings People/sync/manage profile/sign-out/account card/Import Data; History foreign sessions
and promos; feature_showcase; `chrome.syncFileSystem` + `sync_file_system`.

Kept (next batch): `components/sync/**`, `sync_preferences` core, `trusted_vault`,
`IdentityManager` (always signed out), OTR Profile type (CDP contexts), `IncognitoModePrefs`
(reports disabled), password manager, autofill, `extensions/sync`, policy YAMLs. Strings
(`saved_tab_groups_strings.grdp` etc.) stay.

How it went: 2 Sonnet scouts (B2a + B2c, reports in `_docs/agent-briefs/b2{a,c}-scout-report.md`)
→ manifest (1,737 files) → `fix-gn-refs.py` + manual GN/pak/grdp fixes (Chrome Apps generated
schemas made conditional because `sync_file_system.idl` was the last one) → surgery in 3 waves
of 3 agents (W/P/U ownership; Sonnet, then Opus, then Sonnet again with the user raising the
Sonnet cap to 8). Every agent hit ~70% context before finishing its area; handoff reports with
"Remaining work" lists made the relay work. Coordinator finished the rest (settings TS/C++,
extensions, tab-strip views). `-k 0` build: 3 compile errors, 2 grit/GN leftovers, 2 link
errors (W3 had deleted `BrowserLiveTabContext` methods by accident; SystemConfiguration
framework had only been linked via `sync_device_info`). Smoke test: one startup DCHECK —
factories that were only registered through `SyncServiceFactory::DependsOn()`
(TrustedVault, PasskeyModel, password sharing, SecurityEventRecorder, GCM, FamilyLink,
IdentityManager) are now registered explicitly. Then: all WebUI pages load without JS
exceptions, deleted pages 404, `Target.createBrowserContext` works, Settings menu has no
"You and Google".

Lessons: (1) stubbing a factory drops its `DependsOn()` edges, which also registered other
factories — diff the old DependsOn list against the registry; (2) agents sized for "one area"
need ~2 waves for a batch this big — split ownership finer (5–6 areas) next time; (3) `-k 0`
stops at generator/link failures ("cannot make progress"), so check grd/GN inputs
(`siso query inputs` for missing sources, grd `<part>`/`<include>` scans) before building.

Behavior changes: startup always opens a browser window (no picker); closing/ungrouping tab
groups never prompts; `sessions.getDevices` is empty and foreign restore fails; extensions'
`chrome.identity` sign-in flows fail with SigninFailed; the dock "last profile locked/guest"
path just opens a window; idle-timeout "show profile picker" action does nothing; policies
BrowserGuestModeEnabled, ProfileReauthPrompt, OIDC/token managed sign-in are inert.

## Batch B2c (fork commit 12, 2026-10-05) — manifest `prune/b2c.txt`

Decisions (user, B2a session): full removal of password manager and autofill, delete Safe
Browsing password-reuse protection and `components/affiliations`, Credential Management stub.
This session: delete the data importer entirely (its only UI, Settings Import Data, went in
B2a); 6 Sonnet agents, no relays. Coordinator: `components/trusted_vault` +
`chrome/browser/trusted_vault` stay until B2d (16 files in `components/sync/service` use them).

Deleted: `components/{autofill,password_manager (except credential_manager_types +
credential_manager_mojom_traits),affiliations,device_reauth,credential_management,
one_time_tokens,sensitive_content,user_data_importer}`; `chrome/browser/{password_manager,
autofill,affiliations,device_reauth,touch_to_fill,facilitated_payments,importer}`,
`ui/{autofill,passwords}`, `ui/views/{autofill,passwords,importer}`, password-manager /
autofill-internals / autofill-ml-internals / reset-password WebUIs, Settings `autofill_page`,
`passwordsPrivate`/`autofillPrivate` APIs, CDP `Autofill` domain, renderer password/autofill
agents, `chrome/{common,utility}/importer`; GPM/enclave/passkey model/CMTG/passkey import in
`components/webauthn` and `chrome/browser/webauthn` (+ GPM sheets, PIN views, recovery window,
create-user sheet, Touch ID GPM view, `unexportable_key_utils`, `ui_readiness_barrier`); SB
password protection (core + content + chrome service, telemetry signal, modal, page-info
change-password UI); actor `attempt_login` / `attempt_form_filling` / `attempt_otp_filling`;
amount-extraction + zxcvbn component installers; autofill/password grdps.

Kept: core WebAuthn (security keys, hybrid/caBLE, Touch ID / iCloud Keychain, request dialog,
Settings security keys, `passkey_detail_view` for the Touch ID create sheet); CSD phishing
detection (minus autofill observation); `components/webdata` (keyword + token tables);
`BrowsingDataType` enum values; `ToastId`/command/action IDs; policy YAMLs (handlers removed).

How it went: manifest generated by script from the B2c scout report (5,402 + 145 importer
files) → `fix-gn-refs.py` + a gn-gen loop that strips unresolved deps (`/tmp/b2c/fixdeps.py`)
→ pak/grdp/grd/d.ts fixes → 6 parallel Sonnet surgery agents (W wiring/renderer, P
profiles/prefs/flags/extensions, U browser UI/WebUI wiring, S Settings, A WebAuthn/SB/page info,
X actor/login/context menu). W, P, U, S finished; A and X stopped at 70% within 5–10 min and the
coordinator finished their areas. Build blockers: `components_strings.grd.gritdeps` still
listed the deleted grdps (siso schedule failure); one stale `IDS_` use (syntax check passed on
the stale grit header). Smoke-test fixes: clear-browsing-data handler DCHECK on null counters
(passwords/form data), Credential stub returned no `CredentialInfo` (blink DCHECK; now an
EMPTY credential → `null`), `AppSessionServiceFactory` registered late at shutdown (its
explicit registration went in batch A; a deleted factory's `DependsOn` had covered it).

Lessons: (1) `*.grd.gritdeps` must follow grdp deletions; (2) never leave `gn gen` broken while
agents run (`syntax-check-file.sh` silently reports "no object"); (3) syntax checks pass on stale
generated grit/mojom headers — grep kept code for removed `IDS_*` IDs before building;
(4) smoke tests must include a clean `Browser.close` (late factory registrations show up at
shutdown); (5) heavy-reading areas (WebAuthn dialog model/controller) need half the scope.

Behavior changes: no password saving/filling or autofill of any kind; `navigator.credentials.get`
(password/federated) resolves `null`, `store()`/`preventSilentAccess()` succeed; WebAuthn
`isConditionalMediationAvailable()` is false and conditional/ambient/passkey-upgrade requests
show no UI (they stay pending until aborted); WebAuthn Signal API is a no-op; GPM passkeys and
password credentials never appear in the WebAuthn dialog; CDP `Autofill` domain is gone; HTTP
auth dialog never pre-fills or saves; no "Import bookmarks and settings" (menus, first run,
initial-preferences import keys ignored); actor login/form-filling/OTP actions return
unsupported and critical-action logging is a no-op; FedCM auto-reauthn is gated only by its
content setting; Clear browsing data's passwords/autofill checkboxes have no counters; Gemini
anti-scam metadata no longer reports form-field types.

## Test target removal (fork commit 13, 2026-10-05) — `scripts/prune-tests.py`

User decision: remove tests wholesale (DEEP-FORK: only `chrome` must build). Why wholesale: about
1,600 test files were already broken. Only ~400 of those broke from feature removal; the other
~1,000 broke because the dead-files prune (commit 5) deleted test helpers the chrome target never
reads, such as `extension_builder.h` (used by 217 tests).

Scope: chrome, components, content, extensions, services, ui, device. Kept: 123 testonly targets
that blink/cc/net/headless test targets still use, and files the chrome build reads.

Manual fixes: unused GN variables; content_shell's Mac bundle block; the
`web_ui_mojo_test_resources` grit; root `gn_all` deps.

Mistakes caught on the way:
- Deleted `.gitignore` files un-ignored CIPD dmg test data, which `git add -A` then staged.
- Submodule gitlinks were deleted.
- A read-only CIPD `BUILD.gn` was overwritten (reconstructed by hand; `gclient sync` restores it).

The build was green after rebuilding mtime-touched files; the smoke test passed.

## Cleanup sweep 2 (fork commit 14, 2026-10-06) — `prune/policies-sweep2.txt` + scripts

Reports: `_docs/agent-briefs/sweep2-{scout-report,surgery-brief,report-C,report-W}.md`.

Removed:
- 924 policy YAMLs for removed features (`scripts/prune-policies.py`; ids blanked as `N: ''`)
  and their C++ handlers/refs.
- 13,918 unused `IDS_*` messages and 34 emptied grdps (`scripts/prune-strings.py`), incl.
  platform-off ones; `<part>` and `.gritdeps` lines fixed.
- Dead `webui_url_constants`, 11 empty GN targets, `ui/webui/profile_helper`, StrikeDatabase,
  the cross-device theme flag, 237 flag descriptions, 456 stale flag-metadata entries, the AI
  settings page + `experimentalAiData` API, `kDeletePasswords`/`kDeleteFormData` prefs, dead
  loadTimeData keys and icons (settings, history, side panels).

Build: one incremental `-k 0` build at `-j 12` (~2.9k steps, 11 min), 0 FAILED.

Runtime fixes (both bugs came from earlier batches; the extended smoke test found them):
- `FluentLanguageModel::GetLanguages()` was stubbed to `{}` in batch A (it read translate's
  never-translate list). Read Anything calls `.front()` on it and aborted (libc++ hardening).
  It now returns the accept languages.
- `ExtensionMojoBinderRegistryFactory` was only registered via `DependsOn()` from the
  contextual-tasks (A) and AIM-eligibility (B1) bridge factories. Extension service worker
  startup hit the late-registration DCHECK. Now registered in
  `extensions/browser/core_browser_context_keyed_service_factories.cc`.

Smoke test: 49 pages without JS errors (list extended with site settings, search engines,
languages, performance, reset, history/grouped, reading list, Read Anything, policy,
management, components), `Target.createBrowserContext` works, clean `Browser.close`, no FATAL.

## Linux restore (fork commit 15, 2026-10-06) — `scripts/linux-*.py`, `docs/LINUX.md`

Squash of `ac/linux` (7 WIP commits; the Mac session on the Mac, the Linux session on the build host). Workflow and
scripts: `docs/LINUX.md`.

- 2,160 files a Linux chrome build reads, minus removed features; gn gen fixes; restored test
  targets removed again. Build-driven: headers via generated code / typemaps / other-platform
  `#if`s, ~110 Linux strings under their upstream `<if>` stacks (`restore-strings.py`,
  `grd-if-check.py`), Floss fake clients, `ui/gtk/log_noop.h`, `apps/ui/views`, chrome://sandbox.
- Linux remnants of removed features dropped (web-app shell integration, Aura bookmark drag,
  create-app-shortcut dialog, appmenu profiles submenu, SpellcheckLanguage policy handler, removed
  side panels in the code-cache map).

Build: Linux first `-k 0` build ~53k steps, 234 min at `-j 12`, 67 FAILED → fixed in 3 incremental
rounds. macOS incremental at `-j 6`.

Runtime fix: `ExtensionConfigMapFactory` was only registered via `DependsOn()` from the removed
contextual-tasks/AIM bridges (same class as sweep 2's `ExtensionMojoBinderRegistryFactory`, which
had been missed); the first extension frame FATALed. Now registered in the core factories. Both
were already broken on macOS at commit 14 (the commit-14 smoke test didn't load an extension frame
early enough to hit it).

Smoke: macOS 50/50, Linux headless + WSLg X11 + Wayland 50/50, sandbox adequate, clean close.
Not covered: GTK/portal file dialogs (manual), dbus appmenu (no registrar in WSLg), custom frame on
GNOME/KDE. Upstream bug noticed: `read-anything-side-panel` loaded as a plain tab segfaults.

## Agent control without prompts (fork commit 16, 2026-10-06) — `scripts/check-automation.mjs`

The goal: agents (e.g. chrome-devtools-mcp) connect without any approval and the browser never
shows "Chrome is being controlled by automated test software"; the built-in MCP server's
authentication will be the safety measure later.

- `RemoteDebuggingServer::Start()` (was `GetInstance()`): HTTP server always on, localhost only,
  `--remote-debugging-port` or 9222 with a free-port fallback; `DevToolsActivePort` always written
  to the user data dir; pipe handler with `--remote-debugging-pipe`. No policy / default
  user-data-dir gate. WebSocket origin check (`--remote-allow-origins`) unchanged.
- Approval mode gone: `kDevToolsAcceptDebuggingConnections` disabled (chrome://inspect toggle
  hidden), `DevToolsConnectionDialog` + `DevToolsRemoteServerInfobarDelegate` deleted,
  `ChromeDevToolsManagerDelegate::AcceptDebugging`/`SetActiveWebSocketConnections` overrides
  removed (content keeps its defaults).
- Infobar gone: `AutomationInfoBarDelegate`, its migrated-infobar spec, the startup trigger;
  `Emulation.setAutomationOverride` is a no-op. `--enable-automation` still suppresses the other
  startup infobars.
- `Navigator::webdriver()` returns false (content's AutomationControlled mappings untouched).
- `RemoteDebuggingAllowed` policy deleted (`prune/policies-automation.txt`), its pref stays
  registered (default true) for inspect_ui/devtools metrics readers.

Verified: macOS + Linux builds green; `check-automation.mjs` (no flags → port answers,
DevToolsActivePort, no approval, webdriver false before/after override, no infobar by UI height,
chrome://inspect clean); second instance falls back to a free port; Linux WSLg X11 window capture
shows no infobar; smoke 50/50 both platforms.
Gotcha: a running Google Chrome with remote debugging enabled holds 9222; the fork then picks a
free port, so read `DevToolsActivePort` instead of assuming 9222.

## Batch B2d (fork commit 17, 2026-10-07) — manifests `prune/b2d.txt` + `prune/b2d-policies.txt`

Decisions (B2d scout Q&A): no Google account sync, no incognito, no guest or system
profile; agents get isolated contexts from storage partitions instead (`docs/plans/MCP-SERVER.md`).
`chrome.storage.sync` stays as a local-only area. 4 Sonnet surgery agents (A1 profile core /
prefs / IdentityManager / wiring, A2 components + non-UI sync consumers, A3 extensions /
enterprise / OTR-loop services, A4 browser UI / policy / WebUI). 1,554 files changed, −151k lines;
335,326 tracked files.

Deleted: `components/sync` (except `syncer::StringOrdinal`), `components/sync_preferences`,
`chrome/browser/sync`, all data-type bridges (history + journeys sync, extension sync,
storage.sync processors, theme syncable service, security events, account settings, desks
storage, family link controller, SB sync observer); reading list everywhere (components,
chrome, extension API, side panel, `chrome://read-later`, menus); `WebHistoryService`, tailored
security, `gapis`, `UnifiedConsentService`; trusted vault (components, chrome, renderer
extension) and `device/fido/enclave`; OTR profile impl, `IncognitoModePrefs`, incognito policy
handlers, incognito/guest NTP; guest/system profile APIs; IdentityManager
`GaiaCookieManagerService`, `AccountsCookieMutator`, OAuth multilogin, token binding;
11 policies (SyncDisabled, SyncTypesListDisabled, RoamingProfile*, Incognito*,
MandatoryExtensionsForIncognitoNavigation, BrowserGuestModeEnforced, BrowserAddPersonEnabled);
17 flags.

Kept: `Profile::IsOffTheRecord()`/`IsIncognitoProfile()` (return false; 650+ callers) and the
`ProfileSelections` builder API; `IdentityManager` core (primary account, token service, account
tracker), `GetAccountsInCookieJar()` (empty, no network); pref-based URL-keyed consent;
`PrefRegistrySyncable` + `SYNCABLE_*` flags (profile prefs are a plain `PrefService`);
command/action/side-panel IDs and histogram enums.

How it went: scout → the decisions → manifests → `prune.py` + `prune-policies.py` →
`fix-gn-refs.py` → 4 agents with disjoint ownership and a shared brief → Linux build first
(green on `203c0e29b2`), Mac build later (local, after the sccache spike). Fixes after surgery:
Linux-only build errors (`android_rsa` include, sync pin reason, upgrade detector guest check),
`url_keyed_anonymized_data_collection.enabled` must stay registered (startup crash), and a
**shutdown `DanglingRawPtrDetected` trap** (pre-existing since commit 16):
`VisitedURLRankingServiceFactory` passed `TemplateURLService` to `SearchEngineURLStripHandler`
without `DependsOn(TemplateURLServiceFactory)`, so shutdown order freed it first (the ordering had
come transitively through a pruned factory).

Lessons: (1) when deleting factories, check every remaining factory that hands another service's
raw pointer to a member: missing `DependsOn` shows up only at shutdown, so the smoke test must
check the exit status (SIGTRAP = 133), not just the log; (2) Siso's first step count after a start
is before its up-to-date scan (34,689 → 1,902); wait ~30 s before judging a resume.

Behavior changes: `Target.createBrowserContext` errors (default context only, until the MCP
server's partitions); no incognito window anywhere (menus, AppleScript, `--incognito` ignored);
no guest/add-person; no reading list; Google sign-in may remain but never syncs; app menu has no
Bookmarks/Reading-list submenu; Manage Sync / Launch Incognito omnibox pedals gone; UKM consent is
driven by the MSBB pref only (no sync gate); extension `incognito` manifest key and APIs remain
but there is never an incognito profile; `chrome.storage.sync` is local-only.

Verified: macOS + Linux Dev builds green; CDP smoke clean on macOS and Linux headless / WSLg X11 /
Wayland (48/48), `check-automation.mjs` 7/7, clean `Browser.close` exit on both platforms.

## Batch B2b (fork commit 18, 2026-10-08) — manifests `prune/b2b.txt` + `prune/b2b-sp.txt`

Decisions (B2b scout Q&A, `_docs/agent-briefs/b2b-scout-report.md` section 10): delete user
education and feature engagement outright (no shim); remove `chrome.sidePanel` and then the whole side
panel framework; remove invalidation but **keep GCM + Web Push** (GCM starts lazily); remove the
experimental WebUI browser (off by default); delete on-device AI consumers; the orphaned
model-execution stack (`prune/b2b-4b.txt`) is deferred. Phase 1: 4 Sonnet agents (A1 profile/prefs/
binders/flags, A2 WebUI TS + WebUI C++, A3 components + non-UI services), then B1/B2 (views frame/
toolbar/tabs, rest of the UI shell); phase 2: S1 (views layout, browser window, color) and S2 (settings,
WebUI, devtools, webui_browser). 1,557 files changed (1,142 deleted), −178.6k lines; 334,184 tracked files.

Deleted: `components/user_education`, `components/feature_engagement` and every chrome/ UI consumer
(IPH, help bubbles, new badges, product-messaging queue in chip selector, `chrome://user-education-
internals`); side panel entries (history, history clusters, Read Anything + Read Aloud + reading mode
services, phrase segmentation, WASM TTS installer, About This Site, extension side panels) and
`chrome.sidePanel`; the side panel framework (`ui/side_panel`, `ui/views/side_panel`, loading
voter/policy, animations, material side panel color mixer, `web_view_side_panel_*`) and the
experimental WebUI browser (`ui/webui_browser`, its resources, renderer part, pak, devtools BrowserUI
target type); `chrome.gcm` + `chrome.instanceID` + `chrome://gcm-internals`; `components/invalidation`
and every policy/remote-commands invalidator + FM token uploader, SB notification telemetry, device
identity provider; on-device AI consumers (`chrome/browser/ai`, `on_device_ai`, Finds +
`chrome://finds-internals`, Gemini anti-scam, CSD intelligent scan, notification content detection);
24+ flags (IPH, read-anything, finds, antiscam, side panel flyover, …); 26 orphaned `IDS_*`.

Kept: GCM driver + Web Push (`PushManager.subscribe()` works, FCM endpoint; GCM checks in only when a
site subscribes), PDF viewer's in-page side panel, pinned toolbar actions, `ViewType::kExtensionSidePanel`
and histogram/menu enums, `IDS_SIDE_PANEL_TOOLBAR_BUTTON_CXMENU_PIN/UNPIN`, `components/surface_embed`,
`components/browser_apis/tab_strip`.

How it went: phase 1 built green on the Mac after two fixes (gcm custom bindings resource entry,
`ExtensionsToolbarDesktop::GetViewForId` restored). Phase 2 needed four leftovers (flyover flag
entries, `kSidePanelInset` case, webui_browser pak line, unused `inSidePanel_` TS6133) and one link
fix: `//chrome/browser/ui/views:find_bar_owner` had been linked only through webui_browser's deps.

Lessons: (1) removing ids from a low-level shared header (S1 deleted 2 entries from
`ui/color/color_id.h`) rebuilds most of Blink and content/ (~2 h remote); queue such edits with other
big changes or leave dead ids; (2) a `.grd` string prune regenerates `generated_resources.h` (~5k steps,
much of it restat-skipped); (3) when deleting a target, check whether it was the only dep path to a
still-used source_set (link errors show up only at the final link).

Behavior changes: no IPH/promos anywhere; no side panel (window contents use the full width; Settings
> Appearance has no side panel position, persisted `side_panel.*` prefs are ignored); no Read
Anything/Reading mode; no About This Site in page info; extensions can't use `chrome.sidePanel`,
`chrome.gcm`, `chrome.instanceID`; no policy invalidation (cloud policy refreshes on its timer only);
persistent web notifications skip the on-device content check; SB real-time lookups report
NOT_SUPPORTED intelligent scan; page action chips show immediately (no messaging queue); `BindAIManager`
drops the receiver (built-in AI APIs unavailable); no `chrome://webui-browser`.

Verified: macOS Dev build green (`out/Sccache`); CDP smoke clean (51 pages, no JS exceptions),
`check-automation.mjs` 7/7, push subscribe OK, net-log without GCM/invalidation traffic at startup,
clean `Browser.close`. Linux (the build host `out/Dev`, no Linux-only fixes): headless / WSLg X11 /
Wayland smoke clean (48 pages each), push subscribe resolves (~30 s after launch), exit 0. Linux
net-log shows two startup `c2dm/register3` calls (PHONE_REGISTRATION_ERROR) + MCS login 401 before any
page subscribes: follow-up for the lean-startup goal.

## Batch B3 (fork commit 30, 2026-10-11) — manifests `prune/b3-{actor,frames-apps,sb}.txt` + `scripts/prune-locales.py`

Decisions (2026-10-11, scouts `_docs/agent-briefs/b3-scout-{actor,ask}.md`): UI locales en-US only; remove the
orphaned model-execution stack (`prune/b2b-4b.txt`), the Google `actor` framework (Shuttle's MCP doesn't use it), Chrome
Apps, the experimental frames, profile avatar/theme leftovers; Safe Browsing: only the cuts that are safe in mode 1, and
**SB defaults to off**. Sub-agents: 2 Sonnet scouts + 2 Sonnet surgery (S1 actor + shared hot files, S2 frames/apps/
profiles); the Mac session did locales, model execution, speech, SB, the sweep and the build fixes.

Deleted:
- **Locales**: `build/config/locales.gni` is en-US only; per-locale grit outputs and translations stripped, 3,027 `.xtb`
  files and the per-language Google ToS files deleted (`scripts/prune-locales.py`, rerun after each upstream merge,
  before `scripts/rebrand.py`; rebrand.py no longer remaps xtb ids). `scripts/clean-stale-lproj.sh` removes the stale
  `.lproj` dirs from existing out dirs (release-mac.sh calls it).
- **Actor**: `chrome/browser/actor`, `components/actor`, renderer tool executor/journal/page-stability monitor
  (`ChromeRenderFrame` tool methods), `chrome://actor-internals`, actor overlay + border + tab UI controllers, actor
  safety-list component, ScriptToolHost binder, 13+ actor flags, 9 `kColorActorUi*` color ids.
- **Model execution** (`build_with_model_execution=false`, `use_on_device_model_service=false`): on-device model
  service clients, model broker, model quality logging, model validator, on-device speech recognition (Gemini Nano /
  small-expert ASR installer; SODA stays).
- **Chrome Apps** (`enable_platform_apps=false`, `enable_hosted_apps=false`): `apps/ui/views`, `app.window`,
  `app.runtime`, `app.currentWindowInternal`, `fileSystem` APIs, app-window desktop capture, hosted-apps counter.
- **Experimental frames**: waap (InitialWebUI manager, window metrics, prewarm), WebUI toolbar (+ its code-cache pak,
  WebUI page-action/permission-chip views, `browser_apis` toolbar/controls), reload-button metrics.
- **Profiles**: avatar icon images, high-res avatar downloads, `ProfileThemeColors` API.
- **Safe Browsing**: `incident_reporting`, `extension_telemetry` (+ tabs/cookies API telemetry hooks), Mac signature
  evaluator, SB Android leftovers.
- **Sweep**: `prune-tests.py` (219 files), `prune-strings.py` (76 messages).

Kept on purpose: `services/on_device_model` (Blink's AI mojoms import its public mojoms; the service is unbuilt);
`chrome/browser/optimization_guide/model_execution/optimization_guide_global_state.*` (prediction manager for TFLite
models; on-device parts cut); `feature_registry/feature_registration.*` reduced to the GenAI policy/settings prefs
(`RegisterGenAiFeatures` from browser_prefs.cc); `chrome/browser/safe_browsing/suspicious_site_warnings` (wired into
page info); the `FeatureOptInState` enum (multistep filter); `TabAlert::kActor*` enum values (strings now Glic's);
the `ExtensionWebRequestReporter` binder (registered, drops the receiver: renderers still request it). Left dead for a
later sweep: ~60 `kGlicActor*`/`kActor*` and ~15 `kWebUI*`/`kInitialWebUI*` features in `chrome_features.{h,cc}`,
their flag descriptions, `theme_service.h` friend decls.

How it went: one Mac Dev `-k 0` build after surgery + fix rounds; Linux-only gn error
(`webui_toolbar_code_cache_resources.pak` in `chrome_paks.gni` under `enable_webui_generate_code_cache`). Gotchas
added to `docs/TOOLING.md` (prune-locales, what the on-device stack still feeds, renderer-requested interfaces,
fix-gn-refs on partly deleted dirs).

Behavior changes: UI is en-US only (`navigator.language` en-US); Safe Browsing off by default
(`safebrowsing.enabled=false`; can be enabled in Settings); no Chrome Apps / app windows; no actor (all "actor acting"
checks false; `window.open` never converts to a same-tab navigation); profile avatar index always 0, no profile theme
colors; Views toolbar only; fewer `chrome://flags` entries; extension telemetry and SERP FCP observer are no-ops; built-in
on-device AI / speech APIs unavailable.

Linux fix (the Linux session, `ac/prune-b3-linux`): the test sweep had deleted 78 Linux-only stub/fake sources that production
Linux targets compile (`ui/ozone` stub input controller/pixmap factory/overlay manager, x11 `hit_test_x11`, stub keyboard
layout engine, `fake_input_method_context`, `device/bluetooth/{dbus,floss}/fake_*`): restored. `prune-tests.py` now skips
files an enclosing BUILD.gn still names (its scan only sees the Mac's targets).

Verified: Mac release build (`out/Release`, official) green; Mac Dev (`out/Sccache`) green with the final args; fresh headless
profile: smoke-cdp (only the known createBrowserContext / Autofill.enable errors), check-automation 6/6, smoke-mcp ALL OK +
persist-check, smoke-session-cdp, smoke-agents-page 14/14, `navigator.language` en-US, no FATAL, clean `Browser.close`.
Linux (the build host `out/Dev`, headless): smoke-cdp 48 clean pages, smoke-mcp + persist-check ALL OK, smoke-agents-page 14/14,
en-US, no FATAL, clean exit.


## Fork commits 31-33 (2026-10-11): Linux release, open source, network mode

- **31 `612634a758`** Linux official release build (the Linux session, `ac/linux-release` squashed): `gn_all` skips
  `service_discovery_sniffer` without service discovery; `generate_permission_element_grd.py` handles zero translations
  (en-US-only UI makes an empty `MakeFixedFlatMap`, which can't deduce N; hits the Mac too on a fresh regen); drop the
  stale side panel code cache include in `webui_util_desktop.cc` (only `enable_webui_generate_code_cache` builds compile
  it); restore `sandbox/linux/suid/*` so `chrome_sandbox` ships as `chrome-sandbox` (Ubuntu 23.10+ AppArmor blocks
  unprivileged user namespaces).
- **32 `1f9522401f`** Open source: Shuttle README (Chromium's moved to README.chromium.md), LICENSE-SHUTTLE (BSD-3),
  SECURITY.md (GitHub private advisories).
- **33 `571c3a4668`** Network mode: `--shuttle-listen=<ip>` binds the DevTools/MCP server there
  (`chrome/browser/devtools/remote_debugging_server.cc` socket factories). Non-loopback peers must present the access
  token (`Authorization: Bearer` or `?token=`, stripped from the path before the handlers), checked in
  `ServerWrapper::CheckAccess` (`content/browser/devtools/devtools_http_handler.cc`); for them the token replaces the
  Host check, the Origin checks stay. Token: `chrome/browser/shuttle/network_access.{h,cc}` (`<udd>/ShuttleAccessToken`,
  0600, created on first use in `BrowserProcessImpl::CreateDevToolsProtocolHandler` under ScopedAllowBlocking, or
  `--shuttle-access-token`), handed to content via a new `DevToolsSocketFactory::GetAccessToken()` (no command-line switch,
  so it never shows in chrome://version). MCP `cdpEndpoint`/`cdpUrl` and the page-tools sidecar endpoint carry `?token=`.
  Behavior change: none without the flag.

Verified: Mac `out/Sccache` + official `out/Release` green; `scripts/smoke-network.mjs` 18/18 on the Dev build and on the
notarized DMG app; smoke-mcp ALL OK and smoke-cdp clean without the flag. Linux (the build host official, from the tarball):
smoke-cdp 48 pages, smoke-mcp + persist ALL OK, smoke-agents-page 14/14, page tools, smoke-network 18/18, "adequately
sandboxed".

## Fork commits 34-35 (2026-10-11): v0.1.0 release docs

- **34 `0dcd561917`** README/SECURITY.md: 0.1.0 is a regular release, not a beta . Tag `v0.1.0` points here
  (binaries were built from 33; 34 is docs only).
- **35 `ae2d19c8bf`** README demo: the website hero clip as a GIF (`docs/shuttle/hero.gif`, 1280 px, 20 fps, 5.8 MB),
  replacing `demo.gif`. GitHub strips `<video>` with external `src`, so an inline player needs a GitHub user-attachments upload.

## Fork commit 36 (2026-10-12): v0.1.1 worker use-after-free fix

- **36 `84718de00b`** Fix a use-after-free when a session CDP client discovers a new worker (`ac/worker-uaf`,
  fast-forwarded). Worker and tab `DevToolsAgentHost`s start at refcount 0 and call `NotifyCreated()` from their
  constructor; with `Target.setDiscoverTargets` on a per-session CDP connection, `Target.targetCreated` reached
  `SessionCdpFilter::Owns()` synchronously, which took and dropped a temporary `scoped_refptr` via `GetForId()` and so
  deleted the half-built host (`MakeRefCounted` then AddRef'd freed memory). Crashed a user's Shuttle on shared workers
  and the reddit.com service worker. New `DevToolsAgentHost::GetWebContentsForId()` (no reference taken), used in
  `SessionCdpFilter::Owns()` and `SessionManager::FindTab()`. Rule: TOOLING gotcha "No `scoped_refptr` in DevTools
  target notifications". Behavior change: none.

Verified: Mac `out/Sccache` green; repro `/tmp/site-tools/uaf-repro.mjs` no longer crashes; smoke-mcp, smoke-session-cdp,
smoke-cdp clean.

## Fork commit 37 (2026-10-12): no 3-minute startup fence

- **37 `12988c4c14`** Enable `kImprovedStartupBestEffortDelay` by default (`chrome/common/chrome_features.cc`, "// Shuttle:"
  comment; root cause found by the Linux session). BEST_EFFORT and after-startup tasks are fenced until "startup complete", which
  Chrome marks when a visible tab reaches loaded-idle. Shuttle has no startup window (fork commit 22), so only the 3-minute
  failsafe released it: the first `delete-agent` after every launch took ~180 s (its reply waits on a BEST_EFFORT
  `BlockingObliteratePath`). The upstream feature stops waiting after 5 s without a visible tab (meant for the profile
  picker / Mac zero-window mode).
  Behavior change: work that used to wait ~3 min after launch now runs after ~5 s (or once the first visible tab is
  idle): pending agent deletes from the last run clear their partition dirs; StoragePartition GC/obliterate; Chrome's
  after-startup tasks (print backend service, media permission refresh, prerender prewarm, old blob dir removal, push
  messaging cleanup, downgrade cleanup); BEST_EFFORT UI/IO queues and ThreadPool jobs (history/DB cleanup). No UI change.

Verified: Mac `out/Release` (7 steps): first delete-agent 1.4 s after launch (was ~180 s), later ones 1-35 ms;
smoke-mcp ALL OK, smoke-session-cdp all ok, check-automation 7/7, worker UAF repro alive. Linux `out/Dev` (the Linux session):
first delete 179 s → 4.6 s, smoke-mcp, smoke-session-cdp, smoke-agents-page 14/14, smoke-cdp 48/48, UAF repro alive.
