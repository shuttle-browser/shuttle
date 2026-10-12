# Removal candidates (scout results, 2026-10-04)

## Decided 2026-10-04: next removal batch

Everything below goes, together with bookmarks and commerce:
- **Tier 1:** all four bundles. These are the AI side features, promos and consumer UX, sharing/Cast/Drive, and telemetry/internals.
- **Tier 2 Google AI:** glic and contextual_tasks; Lens and Compose; the on-device AI stack (on_device_model, screen_ai, on-device translation, passage embeddings, page annotations, segmentation, plus optimization_guide model delivery as a stub).
- **Tier 2 identity/shell:** sign-in and sync, the NTP (replaced by a minimal blank NTP), IPH/user education, and the side panel entries.
- **Ask items:** payments and wallet, PWAs and apps, and translate (all resolved, see below).
- **Deferred:** `chrome/browser/actor`. Study it first as a possible base for the MCP server.

Removal mechanics: never prune resources by the "unused" analysis. grit, rollup and TS read undeclared inputs. Delete feature resources together with their `.grd` entries.


These come from 4 read-only Sonnet scouts. The fifth scout (`components/`) was stopped because its full-tree `git grep`s were hogging the CPU. File and includer counts are rough. "Includers" means the number of files outside the directory that include its headers. Already done or decided:
- the platform prune
- dead code
- the GN args in `config/args-common.gn`
- bookmarks, commerce and Privacy Sandbox

## Tier 1: small, low coupling (delete the directory, then fix the few includers)

AI and Google services:
- `chrome/browser/{notebooks, manta, private_ai, personal_context, passage_embeddings, skills, ttc, history_embeddings, image_descriptions, content_extraction, contextual_cueing, companion, gsa, auxiliary_search}`
- `chrome/browser/ui/webui/{skills, ai_overlay_dialog, context_hub, organizer_panel}`
- the `ui/views/{mahi, search_ai_mode, indigo, dictation}` bubbles

Promos and consumer UX:
- `chrome/browser/{educational_tip, tips, safety_promo, desktop_to_mobile_promos, magic_stack, default_browser, privacy_guide, quick_delete, tab_group_suggestion, image_editor, long_screenshots, readaloud, record_replay, share, sharing_hub, feed, data_saver}`
- What's New, tab search, Safety Hub, performance controls (Memory/Energy Saver), global media controls, HaTS surveys
- the session-restore infobar
- the intro page and search engine choice. They are already bypassed at runtime, so the code can go.

Telemetry:
- `chrome/browser/{breadcrumbs, error_reporting, crash_upload_list, diagnostics, assist_ranker, complex_tasks}`
- feedback, after its includers are gone

Sharing and sync-adjacent:
- send tab to self
- data_sharing, collaboration
- nearby share
- QR code generator
- consent auditor

Cast: the media router UI and the access-code cast dialog.

PDF/Drive: save to Drive (`ui/views/save_to_drive`, `drive_picker_host`).

Debug pages: the `chrome://*-internals` pages of removed features.

## Tier 2: tight coupling (stub the service first, then delete)

Google AI:
- glic (Gemini in Chrome): 611 files, 142 includers
- contextual_tasks: 161 files
- Lens: about 200 files. The GN flags are blocked by asserts.
- compose, screen_ai, on-device translation. GN-blocked.
- `services/on_device_model`
- optimization_guide: 114 includers
- page_content_annotations
- segmentation_platform

Identity:
- sign-in and DICE: 373 includers
- sync: 180 includers
- trusted_vault
- tab_group_sync: 47 includers
- account/sync settings UI

Other:
- NTP: replace with a minimal blank NTP. `enable_webui_ntp` exists but has asserts.
- user_education / IPH: 94 includers. Stub the registry.
- side panel entries. Keep the framework (106 includers) until its entries are gone.
- enterprise DLP. Several flags are GN-blocked.
- supervised users: the GN flag is asserted true in `chrome/browser/BUILD.gn`.

## Ask: resolved (all decided; last ones in B3, fork commit 30)

- **`chrome/browser/actor`**: studied (B3 scout), not reusable for the MCP server → **removed in B3**.
- **Password manager / autofill:** removed in B2c (fork commit 12).
- **Translate:** removed in Batch A (fork commit 8).
- **Web APIs:** WebNN, Web Speech, FedCM, Shape Detection **kept** (web compatibility); on-device speech backends removed in B3.
- **Apps:** PWAs/web apps removed in Batch A; platform/hosted apps (Chrome Apps) **removed in B3**.
- **Profiles:** picker/UI removed in B2a; avatar images and theme colors **removed in B3** (minimal profiles stay).
- **GCM:** `chrome.gcm`/`instanceID` removed in B2b; GCM driver + Web Push **kept**.
- **Experimental frames:** `webui_browser` removed in B2b; waap and `webui_toolbar` **removed in B3**.
- **Safe Browsing:** B3 = safe cuts in mode 1 (incident reporting, extension telemetry, Mac signature evaluator) and
  **SB off by default**. `safe_browsing_mode` 0 is a possible future spike.

## Keep

The engine and web-platform backends, DevTools/CDP, extensions, printing/PDF, downloads, tracing, themes (too coupled), and devices (bluetooth, HID, USB, serial).
