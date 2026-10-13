# Branding

Approved 2026-10-06. Keep every branded surface (app icon, logo, welcome / control-center
tab, agent-control indicators, docs) consistent with this.

## Name and identity (2026-10-09; fork commit 27 renamed the first brand)

Product **Shuttle** (short: app, Dock, menus, most UI) / **Shuttle Browser** (long: website, DMG, Linux
`.desktop` Name, README titles); company string **Silkweave**; website `https://www.shuttlebrowser.com`;
`Shuttle.app`, bundle ID `dev.silkweave.shuttle`, creator code `Shtl`; signed by the Atomic BI Apple
developer team (`TT5SR2JM9L`). Internal code ids, prefs, storage and binary: `shuttle`. Keep the
`Chromium` brand in the UA string and `navigator.userAgentData` (sites key on it); say "built on
Chromium". Details: `docs/plans/REBRAND.md` (mechanics), `scripts/rebrand.py` (parameters).

## Logo / app icon (v2, locked in 2026-10-08)

- **Mark:** `assets/v2/Shuttle.svg` (+ `Shuttle@4x.png`): flat vector of the a2 mark: three
  swept segments (amber right, violet left, teal top) around an open centre with a cyan ring. Used
  as is for every product-logo surface (about page, Linux icons, `product_logo_*`, small sizes).
- **macOS app icon:** `assets/make-icons.py` → v2 mark on the graphite squircle (Apple 824/1024
  grid) with a dark glass lens and glowing cyan pupil in the centre, plus per-element Icon Composer
  layers (amber, violet, teal, shine, lens, eye) for macOS 26. Output in git-ignored `assets/build/`.
- **Palette (hex):** amber `#FFD24A` → `#FA9D0E` → `#CD4D07`; violet `#9B6BFF` → `#4514D6` → `#1E078E`;
  teal `#3FF5EA` → `#12E7DD` → `#0192A6`; ring/glow `#19D8F5`; pupil `#8BFCFD`; lens `#031E2D` / edge
  `#00131F`; squircle `#1C2430` → `#0B1017`.

### Original concept (a2)

- **Approved:** `assets/approved/app-icon-a2.png` (concept "a2 aperture squircle", OpenAI
  `gpt-image-2.5-flare`, prompt in `app-icon-a2.json`).
- Mark: a three-segment swept ring around a central disc (a nod to the Chrome logo's structure,
  never its colours) in a macOS squircle. The centre is a dark glass lens with a glowing cyan pupil:
  an attentive machine eye, i.e. the agent.
- The PNG is a raster concept. Before shipping: a flat vector redraw (SVG) of the same mark, then the
  icon sets (`.icns`, Linux PNGs 16-512, favicon) from the SVG. Small sizes (≤ 32 px) may drop the
  glass depth but keep the three colours and the eye.

## Palette (approved)

| Role | Colour | Notes |
| --- | --- | --- |
| Segment 1 | electric teal | primary brand colour |
| Segment 2 | deep violet | |
| Segment 3 | warm amber | accent, warnings/highlights |
| Eye / pupil | glowing cyan | "agent active" signal, focus rings |
| Lens | dark glass navy-black | |
| Background | deep graphite | icon squircle, dark UI surfaces |

Exact hex values come with the vector redraw (sample them from the approved PNG and record them
here). Never use Google's red / yellow / green / blue together in a ring (trademark distance).

## Related concepts kept for reference (not approved as separate marks)

`_docs/branding/concepts/` (private, regenerate from `_docs/branding/concepts-*.json`): c2 (cursor +
sparkle, possible agent-control badge), d2 (liquid chrome, possible welcome-page hero), e2 (flat
small-size variant).

## Tooling

`node _docs/branding/generate.mjs <concepts.json> <outDir> [model] [quality] [size]` — key from the
private `_docs/.env`; the image API allows 5 requests/min, the script skips existing outputs, so
just rerun after a rate-limit failure.
