# Rebrand plan (fork commit 19, after B2b)

> **Renamed to Shuttle** in fork commit 27 (2026-10-09). This doc covers the first rebrand (commit 19) and
> the mechanics `scripts/rebrand.py` still uses; current names: `docs/BRANDING.md`.

Agreed 2026-10-08: brand right after B2b lands, as its own fork commit, before the MCP
server (which bakes in profile/user-data paths). One rebuild per platform. Visual identity:
`docs/BRANDING.md` (a2 mark, palette).

## Decisions (2026-10-08)

- **Product name: "Agent Chrome"** (the maintainer's call, with the "Chrome" trademark caveat noted).
  `PRODUCT_FULLNAME`/`PRODUCT_SHORTNAME` = `Agent Chrome` → `Agent Chrome.app`, `Agent Chrome Framework`,
  `Agent Chrome Helper*`; profile dir `~/Library/Application Support/Agent Chrome`, Linux
  `~/.config/agent-chrome`, binary `agent-chrome`; keychain item `Agent Chrome Safe Storage`.
- **Company string: "Silkweave"** (`COMPANY_FULLNAME`/`COMPANY_SHORTNAME`, copyright line). It is
  display-only (Info.plist copyright, About page); it does not need to match the signing identity.
- **Apple signing: Atomic BI team** — `Developer ID Application: Atomic BI PTE LTD`, team ID
  `TT5SR2JM9L` → `MAC_TEAM_ID=TT5SR2JM9L`; bundle ID `<old bundle id>` (matches the team's
  reverse domain). Dev builds stay ad-hoc signed; Developer ID signing + notarization is a release
  step (later, with `out/Release`). `MAC_CREATOR_CODE`: legacy, set to `AgCr`.

## Source surfaces (in `~/chromium/src`, unbranded = `branding_path_component = "chromium"`)

Edit the `chromium` branding in place (no new branding flavour): fewer GN changes and isolated
merge conflicts.

| Surface | File(s) | Notes |
| --- | --- | --- |
| Product/company names, bundle ID | `chrome/app/theme/chromium/BRANDING` | drives `PRODUCT_FULLNAME` → app/framework/helper bundle names, Info.plist |
| User-facing strings | `chrome/app/chromium_strings.grd` (251 "Chromium" hits) | scripted replace; keep "The Chromium Authors" copyright headers and "Chromium" where it means the project |
| Mac profile dir | `chrome/common/chrome_paths_mac.mm` (`"Chromium"` fallback), or `CrProductDirName` in Info.plist | |
| Linux profile dir | `chrome/common/chrome_paths_linux.cc` (`data_dir_basename = "chromium"`) | |
| Keychain / secret-service item | `components/os_crypt/common/keychain_password_mac.mm`, `components/os_crypt/async/browser/freedesktop_secret_key_provider.h` | changing it makes old encrypted data unreadable (fine: no users yet) |
| Mac initial prefs dir | `chrome/browser/mac/initial_prefs.mm` | |
| Mac app icon | `chrome/app/theme/chromium/mac/AppIcon.icon` (Icon Composer bundle, macOS 26), `Assets.car`, `app.icns`, `Assets.xcassets` | macOS 26 reads the compiled `Assets.car` before `app.icns`; regenerate both |
| Product logos | `chrome/app/theme/chromium/product_logo_{16,24,48,64,128,256}.png`, `product_logo_22_mono.png`, `product_logo.svg`, `linux/`, `mac/` PNGs | about page, Linux window icon, tab strip etc. |
| Scaled logo resources | `chrome/app/theme/default_{100,200}_percent/chromium/product_logo_*.png`, `product_logo_name_22*.png` | name wordmark PNGs need the new name |
| Windows (`win/`), ChromeOS dirs | — | unused (Mac + Linux only); leave or delete |

Tooling to update in the same commit: `scripts/smoke-cdp.mjs`, `scripts/check-automation.mjs`
(`Chromium.app` path), smoke instructions in `AGENTS.md`/handoffs, Linux binary/desktop names in
`docs/LINUX.md`.

## Assets to prepare before the commit (no build impact)

1. Vector redraw of a2 → `branding/approved/app-icon-a2.svg`; record exact palette hexes in
   `docs/BRANDING.md`.
2. From the SVG: Icon Composer `AppIcon.icon` (layers for Liquid Glass) + `Assets.car` (`actool`),
   `app.icns` (`iconutil`), PNG set 16/22-mono/24/32/48/64/128/256/512 (+@2x for 100/200% dirs), name
   wordmark PNGs (22 px, dark + white).
3. `scripts/rebrand.py`: copies the asset set into the tree and applies the text edits above
   (idempotent, so it can be rerun after upstream merges). Replaces `spikes/rebrand.sh`, which
   patched a built app.

## Order

1. B2b → fork commit 18 (done first).
2. The maintainer decides name/company; assets generated and reviewed.
3. `rebrand.py --apply`, `gn gen`, one Mac + one Linux build (strings + BRANDING touch widely
   included headers: expect a large rebuild, ~5k+ steps), smoke both (check the new profile path,
   Keychain item, Dock/About/window icon via CDP screenshot + a human check) → fork commit 19.
4. MCP server work starts on top.

## Status / implementation (2026-10-08)

- `scripts/rebrand.py assets` renders everything from `branding/v2/AgentChrome.svg` +
  `branding/make-icons.py` into `branding/build/tree/` (38 files: product logos, Linux icons, 100/200%
  logos, wordmarks, mono tray icon, legacy `AppIcon.appiconset`, `app.icns`, Icon Composer
  `AppIcon.icon`). `scripts/rebrand.py apply [--apply]` copies them into `~/chromium/src`, compiles
  `Assets.car` with Chromium's `tools/mac/icons/compile_car.py` (Xcode 26 `actool`), applies the
  text edits above, and rebrands `chromium_strings.grd` + `settings_chromium_strings.grdp` message
  bodies ("The Chromium Authors" → Silkweave, "Chromium" → Agent Chrome, not ChromiumOS/Updater).
- **Translations:** grit keys translations by a fingerprint of the English text, so changing it
  orphans them. `rebrand.py` parses the grd with grit before/after, remaps the 219 changed ids in
  all 81 `chromium_strings_*.xtb` files and rebrands the translated text (16,119 translations).
- Icon Composer layer SVGs must be standalone valid XML (actool's CoreSVG rejects stray tags).
- **Applied** on fork branch `ac/rebrand` = `033bb9b90b` (pushed to `github`, on top of commit 18):
  Mac `out/Sccache` green (819 steps, ~3 min remote: most outputs restat-skipped), smoke clean,
  `Agent Chrome.app` / `<old bundle id>`; Dock + app switcher icons reviewed by the maintainer after two
  fixes (Icon Composer layers at full-canvas scale 800/1024; document badge `Icon.iconset`).
  Linux (the build host `out/Dev`): green, 4,681 steps, no Linux fixes; smoke clean in headless, WSLg X11
  and Wayland, push OK; profile dir `~/.config/agent-chrome`, window title and `_NET_WM_ICON` v2.
  **Landed as fork commit 19** (`github` `main` = `033bb9b90b`, 2026-10-08).
- Follow-up (Linux): `WM_CLASS` is still `chromium-browser` (taskbar grouping, `.desktop` matching).
- Follow-up: the Settings sidebar "About" entry still shows the old Chromium icon.
- Follow-up: `cr-toolbar` pages (Downloads, History, `chrome://agents`) show Google's Chrome logo in
  dark mode: `ui/webui/resources/images/chrome_logo_dark.svg` (light mode uses the theme's
  current-channel logo, which is ours).
- Follow-up: About page shows "An error occurred while checking for updates: 0" (no updater in this
  build): hide/remove the update status UI (and decide on an update story later).
- Follow-up: Linux binary is still `chrome` (decided name: `agent-chrome`); Developer ID signing +
  notarization with `out/Release`.
- Follow-up: 68 hard-coded "Chromium" mentions in other grd/grdp message bodies (omnibox pedals 33,
  omnibox 12, security interstitials 6, …), mostly unbranded `<if>` variants; same remap approach.
