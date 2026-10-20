#!/usr/bin/env bash
# Fetches the pinned agent-browser release (npm tarball), verifies its version
# and installs the binary + license into an app bundle's Helpers directory.
# The browser spawns it as the page-tools sidecar (docs/MCP.md "Page tools").
#
# Usage: scripts/fetch-agent-browser.sh <Helpers-dir> [platform]
#   e.g. scripts/fetch-agent-browser.sh "$SHUTTLE_SRC/out/Dev/Shuttle.app/Contents/Helpers"
#   platform: darwin-arm64 (default on Apple Silicon), darwin-x64, linux-x64, linux-arm64
# Env: AGENT_BROWSER_SRC=<dir of an unpacked npm package> skips the download
#      (e.g. /tmp/ab-spike/node_modules/agent-browser); its binary must still match the pin.
#      SHUTTLE_SIDECAR_UNPINNED=1 accepts a binary that doesn't match (dev only; refused with SHUTTLE_RELEASE_MODE=1).
# Integrity: the npm tarball is checked against its pinned sha512 and the platform binary against its pinned sha256
# before it is copied or executed. Bumping VERSION = new pins (README of this script: `npm view
# agent-browser@<v> dist.integrity`, then `shasum -a 256 package/bin/agent-browser-*` of the unpacked tarball).
#
# Code signing: the binary must be signed with the app for distribution
# (hardened runtime, same team). For dev, ad-hoc is enough:
#   codesign --force --sign - "<Helpers>/agent-browser"
# Then re-sign the app (codesign --force --deep --sign - "<app>") if you
# modified a signed bundle. Do not edit signed bundles while the app runs.
set -euo pipefail

VERSION=0.38.2
TARBALL_SHA512=sha512-b/7hvo2RrqocvybjB9kwpX12kGkxkFvtdJ4XPDoott5TLK2n3aPX+lYg7ALd+2YvPi2LrTKGzQYu/rxZC2Y7ZA==
pin() {  # sha256 of bin/agent-browser-<platform> in agent-browser@0.38.2
  case $1 in
    darwin-arm64) echo 8168b86ab5d94be8f670992dfe4fe1445016518a864b48bda105e64142e7cbf9 ;;
    darwin-x64)   echo 787cb40e086a188d0bb13ff29a99a0b2380aff3aa5e8600b8f8131a0b98ca69c ;;
    linux-x64)    echo a54b765192db774666f0513fa8b545a298753b6f29e73bcdf4a1e78f18e7c0e1 ;;
    linux-arm64)  echo 690c02d952de8497bba4f8cc58b59acbf27dc27b346755869b518f4b411c7f40 ;;
  esac
}
sha256() { if command -v sha256sum >/dev/null; then sha256sum "$1" | cut -d' ' -f1; else shasum -a 256 "$1" | cut -d' ' -f1; fi; }
DEST=${1:?usage: $0 <Helpers-dir> [platform]}
case "$(uname -s)-$(uname -m)" in
  Darwin-arm64) DEFAULT=darwin-arm64 ;; Darwin-x86_64) DEFAULT=darwin-x64 ;;
  Linux-x86_64) DEFAULT=linux-x64 ;; Linux-aarch64) DEFAULT=linux-arm64 ;;
  *) DEFAULT= ;;
esac
PLATFORM=${2:-$DEFAULT}
[ -n "$PLATFORM" ] || { echo "unknown platform; pass one" >&2; exit 2; }

WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT
if [ "${SHUTTLE_SIDECAR_UNPINNED:-}" = 1 ] && [ "${SHUTTLE_RELEASE_MODE:-}" = 1 ]; then
  echo "SHUTTLE_SIDECAR_UNPINNED is not allowed in a release" >&2; exit 2
fi
if [ -n "${AGENT_BROWSER_SRC:-}" ]; then
  PKG=$AGENT_BROWSER_SRC
else
  # Fresh empty dir; nothing from the tarball is executed except the binary we verify.
  TGZ=$WORK/agent-browser-$VERSION.tgz
  if command -v npm >/dev/null; then
    (cd "$WORK" && npm pack "agent-browser@$VERSION" --silent >/dev/null)
  else  # no npm (e.g. a minimal Linux host): the registry tarball directly (same bytes, checked against the same pin below)
    curl -fsSL -o "$TGZ" "https://registry.npmjs.org/agent-browser/-/agent-browser-$VERSION.tgz"
  fi
  [ -f "$TGZ" ] || { echo "npm pack produced no $TGZ" >&2; exit 1; }
  GOT="sha512-$(openssl dgst -sha512 -binary "$TGZ" | base64 | tr -d '\n')"
  [ "$GOT" = "$TARBALL_SHA512" ] || { echo "agent-browser tarball integrity mismatch: $GOT" >&2; exit 1; }
  mkdir "$WORK/pkg" && tar -xzf "$TGZ" -C "$WORK/pkg"
  PKG=$WORK/pkg/package
fi

BIN=$PKG/bin/agent-browser-$PLATFORM
[ -f "$BIN" ] || { echo "missing $BIN" >&2; exit 1; }
grep -q "\"version\": \"$VERSION\"" "$PKG/package.json" || { echo "package version is not $VERSION" >&2; exit 1; }
WANT=$(pin "$PLATFORM"); BIN_SHA=$(sha256 "$BIN")
if [ "$BIN_SHA" != "$WANT" ]; then
  if [ "${SHUTTLE_SIDECAR_UNPINNED:-}" = 1 ]; then echo "warning: unpinned agent-browser binary ($BIN_SHA)" >&2
  else echo "agent-browser-$PLATFORM sha256 $BIN_SHA, pinned ${WANT:-none}: refusing" >&2; exit 1; fi
fi

mkdir -p "$DEST"
cp "$BIN" "$DEST/agent-browser"
chmod 755 "$DEST/agent-browser"
# License files: in a macOS bundle they go to Contents/Resources (codesign rejects
# non-code files in Contents/Helpers); elsewhere next to the binary.
DOCS=$DEST
case "$DEST" in */Contents/Helpers|*/Contents/Helpers/) DOCS=${DEST%/}; DOCS=${DOCS%/Helpers}/Resources ;; esac
rm -f "$DEST/agent-browser.LICENSE" "$DEST/agent-browser.NOTICE"
mkdir -p "$DOCS"
cp "$PKG/LICENSE" "$DOCS/agent-browser.LICENSE"
cat > "$DOCS/agent-browser.NOTICE" <<NOTICE
agent-browser $VERSION (https://github.com/vercel-labs/agent-browser), Apache License 2.0.
Bundled unmodified as the page-tools sidecar of Shuttle. See agent-browser.LICENSE.
NOTICE

# Verify only when it runs on this machine.
if [ "$PLATFORM" = "$DEFAULT" ]; then
  GOT=$("$DEST/agent-browser" --version | awk '{print $NF}')
  [ "$GOT" = "$VERSION" ] || { echo "binary reports $GOT, expected $VERSION" >&2; exit 1; }
fi
if [ "$(uname -s)" = Darwin ]; then
  xattr -dr com.apple.quarantine "$DEST/agent-browser" 2>/dev/null || true
  codesign --force --sign - "$DEST/agent-browser" 2>&1 | tail -1 || true
fi
echo "agent-browser $VERSION ($PLATFORM) -> $DEST/agent-browser"
echo "sidecar_sha256=$BIN_SHA (upstream binary, before ad-hoc signing)"
