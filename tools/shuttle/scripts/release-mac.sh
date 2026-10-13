#!/bin/zsh
# Signs, notarizes and packages a Shuttle release DMG from out/Release (_docs/plans/RELEASE.md, "Mac pipeline").
# Usage: scripts/release-mac.sh [--no-notarize]
#   Needs: a green `out/Release` build of `chrome` (SHUTTLE_KIND=release scripts/build-sccache.sh ... chrome),
#   the fork's chrome/installer/mac signing pipeline (fork branch ac/release-signing, merged),
#   SHUTTLE_SIGN_ID   = SHA-1 of the "Developer ID Application: <Org> (<TEAMID>)" identity
#                       (several identities share that name: `security find-identity -v -p codesigning`),
#   SHUTTLE_NOTARY_PROFILE = notarytool keychain profile (`xcrun notarytool store-credentials <name> ...`).
# Output: $SHUTTLE_ROOT/release/<version>/ (signed Shuttle.app + Shuttle-<version>-arm64.dmg). Secrets stay in
# the keychain; nothing is written to the repo.
set -e
. "$(dirname "$0")/lib/paths.sh"
export PATH="$PATH:$HOME/depot_tools"
SRC="$SHUTTLE_SRC"
OUT="$SRC/out/Release"
TOOLS="$(cd "$(dirname "$0")/.." && pwd)"
notarize=staple
[[ "$1" == --no-notarize ]] && notarize=none

: "${SHUTTLE_SIGN_ID:?set SHUTTLE_SIGN_ID to the Developer ID Application SHA-1}"
[[ "$notarize" == none ]] || : "${SHUTTLE_NOTARY_PROFILE:?set SHUTTLE_NOTARY_PROFILE (notarytool keychain profile)}"
[[ -d "$OUT/Shuttle.app" ]] || { echo "FATAL: $OUT/Shuttle.app missing (build out/Release first)" >&2; exit 2; }
"$(dirname "$0")/clean-stale-lproj.sh" "$OUT"   # en-US-only UI: drop old locale dirs left in the out dir
[[ -f "$SRC/chrome/installer/mac/sign_chrome.py" ]] || { echo "FATAL: signing pipeline not in the tree (ac/release-signing)" >&2; exit 2; }

version=$(plutil -extract CFBundleShortVersionString raw "$OUT/Shuttle.app/Contents/Info.plist")
dest="$SHUTTLE_RELEASE/$version"
echo "== Shuttle $version (arm64) -> $dest"

# 1. Packaging dir (signing scripts, entitlements, dmg_tool/hfs_tool).
autoninja -C "$OUT" chrome/installer/mac

# 2. Sidecar into the unsigned app (the pipeline signs it as part 'agent-browser').
"$TOOLS/scripts/fetch-agent-browser.sh" "$OUT/Shuttle.app/Contents/Helpers"
# fetch-agent-browser ad-hoc signs it; the pipeline's codesign has no --force, so strip that signature.
codesign --remove-signature "$OUT/Shuttle.app/Contents/Helpers/agent-browser"

# 3. Sign (+ notarize + staple) the app and the DMG.
rm -rf "$dest"; mkdir -p "$dest"
notary_args=()
[[ "$notarize" == none ]] || notary_args=(--notarize "$notarize" --notary-arg=--keychain-profile "--notary-arg=$SHUTTLE_NOTARY_PROFILE")
python3 "$OUT/Shuttle Packaging/sign_chrome.py" --input "$OUT" --output "$dest" \
  --identity "$SHUTTLE_SIGN_ID" "${notary_args[@]}"

# 4. Verify (the pipeline keeps only the DMG; check the app inside it).
dmg=$(find "$dest" -maxdepth 1 -name '*.dmg' | head -1)
[[ -n "$dmg" ]] || { echo "no DMG in $dest" >&2; exit 1; }
spctl -a -vv -t open --context context:primary-signature "$dmg"
xcrun stapler validate "$dmg"
mnt=$(mktemp -d)
hdiutil attach -nobrowse -readonly -mountpoint "$mnt" "$dmg" >/dev/null
trap 'hdiutil detach -quiet "$mnt"' EXIT
# No --strict: hdiutil makehybrid (upstream pkg-dmg) gives every file a com.apple.FinderInfo, which
# copies keep and --strict rejects. Same for installed Google Chrome; Gatekeeper accepts it.
codesign --verify --deep --verbose=1 "$mnt/Shuttle.app"
spctl -a -vv -t exec "$mnt/Shuttle.app"
echo "== done: $dmg"
