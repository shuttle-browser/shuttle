#!/bin/zsh
# Signs, notarizes and packages a Shuttle release DMG from out/Release (docs/TOOLING.md "Releases").
# Usage: SHUTTLE_VERSION=155.1.0 scripts/release-mac.sh [--no-notarize]
#   SHUTTLE_VERSION   = the Shuttle version, <Chromium major>.<minor>.<patch> (major must match chrome/VERSION).
#   Needs: a green `out/Release` build of `chrome` made by build.sh from a clean tree (same SHUTTLE_GN_EXTRA, if any; its build
#   receipt, lib/receipt.sh), the fork's chrome/installer/mac signing pipeline,
#   SHUTTLE_SIGN_ID   = SHA-1 of the "Developer ID Application: <Org> (<TEAMID>)" identity
#                       (several identities share that name: `security find-identity -v -p codesigning`),
#   SHUTTLE_NOTARY_PROFILE = notarytool keychain profile (`xcrun notarytool store-credentials <name> ...`).
#   --no-notarize: sign only (local test); skips the Gatekeeper/stapler checks, which need notarization.
# Output: $SHUTTLE_ROOT/release/v<version>/mac/ (Shuttle-<chromium version>-arm64.dmg + MANIFEST.txt: receipt,
# sidecar and DMG sha256). Refuses an existing destination. Secrets stay in the keychain; nothing is written to the repo.
set -e
. "$(dirname "$0")/lib/paths.sh"
. "$(dirname "$0")/lib/receipt.sh"
export PATH="$PATH:$HOME/depot_tools" SHUTTLE_RELEASE_MODE=1
unset SHUTTLE_SIDECAR_UNPINNED AGENT_BROWSER_SRC
SRC="$SHUTTLE_SRC"
OUT="$SRC/out/Release"
TOOLS="$(cd "$(dirname "$0")/.." && pwd)"
notarize=staple
[[ "${1:-}" == --no-notarize ]] && notarize=none

: "${SHUTTLE_SIGN_ID:?set SHUTTLE_SIGN_ID to the Developer ID Application SHA-1}"
[[ "$notarize" == none ]] || : "${SHUTTLE_NOTARY_PROFILE:?set SHUTTLE_NOTARY_PROFILE (notarytool keychain profile)}"
ver="${SHUTTLE_VERSION:-}"
[[ -n "$ver" ]] || { echo "FATAL: set SHUTTLE_VERSION (e.g. 155.1.0)" >&2; exit 2; }
[[ "$ver" =~ '^[0-9]+\.[0-9]+\.[0-9]+$' ]] || { echo "FATAL: SHUTTLE_VERSION '$ver' is not <major>.<minor>.<patch>" >&2; exit 2; }
major=$(. "$SRC/chrome/VERSION"; echo "$MAJOR")
[[ "${ver%%.*}" == "$major" ]] || { echo "FATAL: SHUTTLE_VERSION $ver doesn't start with Chromium major $major" >&2; exit 2; }
[[ -d "$OUT/Shuttle.app" ]] || { echo "FATAL: $OUT/Shuttle.app missing (build out/Release first)" >&2; exit 2; }
[[ -f "$SRC/chrome/installer/mac/sign_chrome.py" ]] || { echo "FATAL: signing pipeline not in the tree" >&2; exit 2; }
dest="$SHUTTLE_RELEASE/v$ver/mac"
[[ ! -e "$dest" ]] || { echo "FATAL: $dest exists; never overwritten (pick a new version or move it away)" >&2; exit 2; }
receipt=$(shuttle_receipt_check "$OUT" "$OUT/Shuttle.app") || exit 2
mkdir -p "$(dirname "$dest")" && mkdir "$dest" 2>/dev/null || { echo "FATAL: $dest exists (another run?)" >&2; exit 2; }  # atomic reservation
chromium=$(plutil -extract CFBundleShortVersionString raw "$OUT/Shuttle.app/Contents/Info.plist")
echo "== Shuttle $ver (Chromium $chromium, arm64) -> $dest"

# 1. Packaging dir (signing scripts, entitlements, dmg_tool/hfs_tool), through build.sh with the args unchanged.
SHUTTLE_TARGET=chrome/installer/mac SHUTTLE_KEEP_ARGS=1 "$(dirname "$0")/build.sh" release
shuttle_receipt_check "$OUT" "$OUT/Shuttle.app" >/dev/null || { echo "FATAL: building the packaging target rebuilt chrome" >&2; exit 2; }
"$(dirname "$0")/clean-stale-lproj.sh" "$OUT"   # en-US-only UI: drop old locale dirs left in the out dir

# 2. Sidecar into the unsigned app (the pipeline signs it as part 'agent-browser'), pinned hash.
sidecar=$("$TOOLS/scripts/fetch-agent-browser.sh" "$OUT/Shuttle.app/Contents/Helpers" | tee /dev/stderr | sed -n 's/^sidecar_sha256=\([0-9a-f]*\).*/\1/p')
[[ -n "$sidecar" ]] || { echo "FATAL: no sidecar hash" >&2; exit 2; }
# fetch-agent-browser ad-hoc signs it; the pipeline's codesign has no --force, so strip that signature.
codesign --remove-signature "$OUT/Shuttle.app/Contents/Helpers/agent-browser"

# 3. Sign (+ notarize + staple) the app and the DMG.
notary_args=()
[[ "$notarize" == none ]] || notary_args=(--notarize "$notarize" --notary-arg=--keychain-profile "--notary-arg=$SHUTTLE_NOTARY_PROFILE")
python3 "$OUT/Shuttle Packaging/sign_chrome.py" --input "$OUT" --output "$dest" \
  --identity "$SHUTTLE_SIGN_ID" "${notary_args[@]}"

# 4. Verify (the pipeline keeps only the DMG; check the app inside it).
dmg=$(find "$dest" -maxdepth 1 -name '*.dmg' | head -1)
[[ -n "$dmg" ]] || { echo "no DMG in $dest" >&2; exit 1; }
if [[ "$notarize" != none ]]; then
  spctl -a -vv -t open --context context:primary-signature "$dmg"
  xcrun stapler validate "$dmg"
fi
mnt=$(mktemp -d)
hdiutil attach -nobrowse -readonly -mountpoint "$mnt" "$dmg" >/dev/null
trap 'hdiutil detach -quiet "$mnt"' EXIT
# No --strict: hdiutil makehybrid (upstream pkg-dmg) gives every file a com.apple.FinderInfo, which
# copies keep and --strict rejects. Same for installed Google Chrome; Gatekeeper accepts it.
codesign --verify --deep --verbose=1 "$mnt/Shuttle.app"
[[ "$notarize" == none ]] || spctl -a -vv -t exec "$mnt/Shuttle.app"

# 5. Release manifest (outside the artifact: it holds the artifact's hash).
{
  echo "shuttle_version=$ver"
  echo "platform=mac-arm64"
  echo "notarized=$([[ "$notarize" == none ]] && echo no || echo yes)"
  print -r -- "$receipt"
  echo "sidecar_sha256=$sidecar"
  echo "artifact=$(basename "$dmg")"
  echo "artifact_sha256=$(shuttle_sha256 "$dmg")"
  echo "packaged_at=$(date -u +%Y-%m-%dT%H:%M:%SZ)"
} > "$dest/MANIFEST.txt"
echo "== done: $dmg"
cat "$dest/MANIFEST.txt"
