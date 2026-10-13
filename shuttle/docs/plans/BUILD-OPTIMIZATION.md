# Plan: Build and runtime optimization

Status: proposed (2026-10-03). Base: Chromium 154.0.8037.129, branch `chromium`.

## Goal

Make Shuttle a lean, fast browser for AI agents: less build time, less idle resource use, and predictable runtime behavior. Do this without breaking web compatibility.

## Principles

- **Measure before trimming.** Every change is compared against a stock baseline.
- **Trim the browser shell, not the engine.** Blink, V8, and legacy web compatibility (quirks mode, old APIs, encodings) stay intact. They are not separable modules, and agents browse the real, messy web.
- **Prefer runtime defaults over build-time removal.** They give larger gains, carry lower risk, and survive rebases better.
- **Every non-default GN arg is an untested config.** Expect occasional compile breaks and rebase maintenance. Keep a flag only if it measurably helps.

## Steps

### 1. Baseline (stock Dev build)

After the first `out/Dev` build succeeds, record:

- the build step count and wall time
- app bundle size
- cold-start time to first page
- idle memory with N tabs (browser + renderer + GPU processes)
- background network traffic at idle (first 10 minutes after launch)

### 2. Runtime defaults (patch series)

Disable by default in the fork, via small patches or a bundled policy file:

- background networking
- component updater
- variations / field trials (Finch)
- sync
- translate
- optimization guide and on-device model downloads (multi-GB)

Open decision: Safe Browsing (`safe_browsing_mode=0` or runtime off). Agents visit arbitrary sites, so phishing and malware protection still has value. Decide explicitly.

**Privacy Sandbox: fully disabled (decided 2026-10-04).** For an agent browser, the gains are no ad-API tracking surface, no "Ad privacy" consent dialog that would block automation, and less background work. The downside is small: sites feature-detect these APIs and fall back. Ship it as its own patch, `disable-privacy-sandbox`, written against 154 and modelled on Thorium's `other/disable-privacy-sandbox.patch` (BSD-3, credit in the patch header):

- `components/privacy_sandbox/privacy_sandbox_features.cc`: flip the sandbox feature defaults to `DISABLED_BY_DEFAULT`.
- `privacy_sandbox_settings_impl.cc`: always report the sandbox as restricted (`IsPrivacySandboxRestricted()` → true, API status → `kRestricted`). This also suppresses the consent and notice UI.
- `privacy_sandbox_prefs.cc`: `kPrivacySandboxRelatedWebsiteSetsEnabled` defaults to false.
- Ad-API features off by default: Topics (`kBrowsingTopics`), Protected Audience (`kInterestGroupStorage`, FLEDGE), Attribution Reporting (`kConversionMeasurement`), Shared Storage, Private Aggregation (`kPrivateAggregationApi`).
- Keep: CHIPS / partitioned cookies, storage partitioning, and the default third-party-cookie policy. These are web-compat and privacy baseline, not ad APIs, and agents need working SSO.
- Verify: `chrome://settings/adPrivacy` shows everything off, `document.browsingTopics` is undefined or rejects, and no privacy dialog appears on a fresh profile.

### 2b. Adopted GN args (from the Thorium review, decided 2026-10-04)

Add these to `common` in `scripts/build.sh` after the baseline is recorded. Changing them on `out/Dev` triggers a large rebuild, so batch them with the first trim round.

| Arg | Why |
|---|---|
| `proprietary_codecs=true`, `ffmpeg_branding="Chrome"` | H.264/AAC playback. Without them, plain Chromium fails on many video sites. |
| `disable_fieldtrial_testing_config=true` | No baked-in field-trial configs. Predictable behavior, matching the Finch-off runtime default. |
| `enable_reporting=false` | Drops the Reporting API / NEL uploads. |

Considered and rejected: the x86 SIMD flags (irrelevant on arm64); `-O3` for third-party code and `-ffp-contract=fast` (no measured gain on Apple Silicon, and fp-contract changes float results versus Chrome); Widevine (needs Google's signing); and Thorium's way of copying whole files over the tree (we keep a patch series). Thorium's arm64 speed comes from `is_official_build` (PGO + ThinLTO), which step 4 already covers.

### 3. GN trimming (one group at a time)

Candidate group A (low risk):

| Area | Args → `false` |
|---|---|
| Google AI features | `enable_compose`, `enable_lens_desktop`, `enable_lens_overlay_backend`, `use_on_device_model_service`, `enable_screen_ai_service`, `enable_on_device_translation` |
| Remote and casting | `enable_remoting`, `enable_media_remoting`, `enable_media_remoting_rpc` |
| Local network discovery | `enable_service_discovery`, `enable_mdns` (likely also removes the macOS local-network permission prompt) |
| Speech | `enable_speech_service`, `enable_browser_speech_service` |
| Misc shell | `enable_background_mode`, `enable_paint_preview`, `enable_captive_portal_detection`, `enable_pdf_save_to_drive`, `enable_pdf_translation`, `enable_pdf_ink2` |

Process: add the group to `scripts/build.sh`, rebuild, then compare against the baseline. If a flag breaks the build, drop it rather than patching around it.

Skip:

- `enable_supervised_users=false`: a rarely built desktop config that is likely to break.
- printing: CDP `Page.printToPDF` depends on it.
- `enable_pdf`: agents read PDFs.
- extensions: the companion extension and DevTools depend on them.
- `enable_swiftshader`: the software GPU fallback for headless runs and VMs.

Expected impact: roughly 5–15% less build time, and a small runtime gain. Code that isn't built mostly isn't loaded at runtime anyway.

### 4. Shipping Release build

`out/Release` with `is_official_build=true` (PGO + ThinLTO). This is typically worth 10%+ runtime performance, more than any feature trimming. It builds much slower, so run it overnight and use it for distributable builds only. Day-to-day iteration stays on `out/Dev` (a component build).

### 5. Faster builds (optional, later)

- Remote build execution (RBE) for siso can cut full rebuilds and rebases from hours to minutes. It needs a backend, either Google's or a self-hosted one.
- Incremental iteration already relies on the component build (`out/Dev`), which matters more than any GN trimming.

## Ordering relative to other fork work

Branding and the infobar removal can proceed in parallel as soon as the Dev build exists. They give the fastest visible progress. This plan's steps 2–3 follow the baseline.

## References

- `scripts/build.sh`: builds `out/Dev` (component build) and `out/Release`.
- `scripts/build-progress.sh`: progress, ETA and failure events from the build log.
- All args: `gn args out/Dev --list --short`
