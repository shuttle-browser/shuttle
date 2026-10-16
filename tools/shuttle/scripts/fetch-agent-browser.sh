#!/usr/bin/env bash
# Fetches the pinned agent-browser release (npm tarball), verifies its version
# and installs the binary + license into an app bundle's Helpers directory.
# The browser spawns it as the page-tools sidecar (docs/MCP.md "Page tools").
#
# Usage: scripts/fetch-agent-browser.sh <Helpers-dir> [platform]
#   e.g. scripts/fetch-agent-browser.sh "$SHUTTLE_SRC/out/Sccache/Shuttle.app/Contents/Helpers"
#   platform: darwin-arm64 (default on Apple Silicon), darwin-x64, linux-x64, linux-arm64
# Env: AGENT_BROWSER_SRC=<dir of an unpacked npm package> skips the download
#      (e.g. /tmp/ab-spike/node_modules/agent-browser).
#
# Code signing: the binary must be signed with the app for distribution
# (hardened runtime, same team). For dev, ad-hoc is enough:
#   codesign --force --sign - "<Helpers>/agent-browser"
# Then re-sign the app (codesign --force --deep --sign - "<app>") if you
# modified a signed bundle. Do not edit signed bundles while the app runs.
set -euo pipefail

VERSION=0.38.2
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
if [ -n "${AGENT_BROWSER_SRC:-}" ]; then
  PKG=$AGENT_BROWSER_SRC
else
  # Fresh empty dir; nothing from the tarball is executed except the binary we verify.
  (cd "$WORK" && npm pack "agent-browser@$VERSION" --silent >/dev/null && mkdir pkg && tar -xzf agent-browser-*.tgz -C pkg)
  PKG=$WORK/pkg/package
fi

BIN=$PKG/bin/agent-browser-$PLATFORM
[ -f "$BIN" ] || { echo "missing $BIN" >&2; exit 1; }
grep -q "\"version\": \"$VERSION\"" "$PKG/package.json" || { echo "package version is not $VERSION" >&2; exit 1; }

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
