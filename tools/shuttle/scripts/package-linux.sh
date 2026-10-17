#!/usr/bin/env bash
# Packages a Linux Shuttle build into shuttle-linux-x64.tar.xz (+ .sha256) with a top-level shuttle/ dir.
# Usage (Linux build host): scripts/package-linux.sh [out-dir] [dest-dir]
#   out-dir:  default $SHUTTLE_ROOT/src/out/Release (non-component build, official)
#   dest-dir: default $SHUTTLE_ROOT/release/v$SHUTTLE_VERSION/linux (SHUTTLE_VERSION = <Chromium major>.<minor>.<patch>)
# Release mode (default): out-dir needs a clean-tree build receipt (lib/receipt.sh), the sidecar is pinned and
# required, an existing tarball is never overwritten. The receipt ships as shuttle/BUILD-RECEIPT.txt; DEST gets
# MANIFEST.txt (receipt, sidecar + tarball sha256; package-deb.sh appends the deb/rpm hashes).
# Env: SHUTTLE_LOCAL_PACKAGE=1 = local test package: no receipt needed, SHUTTLE_NO_SIDECAR=1 allowed.
# File list follows upstream chrome/installer/linux/common/installer.py (pruned from the fork).
set -euo pipefail

. "$(dirname "$0")/lib/paths.sh"
. "$(dirname "$0")/lib/receipt.sh"
LOCAL=${SHUTTLE_LOCAL_PACKAGE:-0}
[ "$LOCAL" = 1 ] || export SHUTTLE_RELEASE_MODE=1
[ "$LOCAL" = 1 ] || [ "${SHUTTLE_NO_SIDECAR:-}" != 1 ] || { echo "FATAL: SHUTTLE_NO_SIDECAR needs SHUTTLE_LOCAL_PACKAGE=1" >&2; exit 2; }
[ "$LOCAL" = 1 ] || unset SHUTTLE_SIDECAR_UNPINNED AGENT_BROWSER_SRC
SRC=$SHUTTLE_SRC
TOOLS=$(cd "$(dirname "$0")/.." && pwd)
OUT=$(realpath "${1:-$SRC/out/Release}")
VERSION=$(. "$SRC/chrome/VERSION"; echo "$MAJOR.$MINOR.$BUILD.$PATCH")
ver=${SHUTTLE_VERSION:-}
if [ "$LOCAL" != 1 ] || [ -n "$ver" ]; then  # release mode: always a valid Shuttle version, explicit dest or not
  [[ $ver =~ ^[0-9]+\.[0-9]+\.[0-9]+$ && ${ver%%.*} == "${VERSION%%.*}" ]] ||
    { echo "FATAL: SHUTTLE_VERSION '$ver' must be ${VERSION%%.*}.<minor>.<patch>" >&2; exit 2; }
fi
if [ -n "${2:-}" ]; then DEST=$(realpath -m "$2")
else
  [ -n "$ver" ] || { echo "FATAL: set SHUTTLE_VERSION (e.g. 155.1.0) or pass a dest-dir" >&2; exit 2; }
  DEST=$(realpath -m "$SHUTTLE_RELEASE/v$ver/linux")
fi
NAME=shuttle-linux-x64

[ -x "$OUT/shuttle" ] || { echo "FATAL: $OUT/shuttle missing" >&2; exit 2; }
grep -q 'is_component_build = true' "$OUT/args.gn" && { echo "FATAL: $OUT is a component build" >&2; exit 2; }
[ ! -e "$DEST/$NAME.tar.xz" ] || { echo "FATAL: $DEST/$NAME.tar.xz exists; never overwritten" >&2; exit 2; }
if [ "$LOCAL" = 1 ]; then receipt="local_package=yes (no receipt check)"
else receipt=$(shuttle_receipt_check "$OUT" "$OUT/shuttle") || exit 2; fi  # identity; file freshness below

STRIP=$SRC/third_party/llvm-build/Release+Asserts/bin/llvm-strip
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT
PKG=$WORK/shuttle
mkdir -p "$PKG/locales"

COPIED=()  # every build output that ships: checked against the receipt before the tarball is written
need() { for f; do [ -e "$OUT/$f" ] || { echo "FATAL: $OUT/$f missing" >&2; exit 2; }; cp -a "$OUT/$f" "$PKG/$f"; COPIED+=("$OUT/$f"); done; }
opt() { for f; do if [ -e "$OUT/$f" ]; then cp -a "$OUT/$f" "$PKG/$f"; COPIED+=("$OUT/$f"); fi; done; }

# Binaries and shared libraries.
need shuttle chrome_crashpad_handler
opt libEGL.so libGLESv2.so libvk_swiftshader.so libvulkan.so.1 vk_swiftshader_icd.json \
    libqt5_shim.so libqt6_shim.so
# setuid sandbox helper, only used where unprivileged user namespaces are unavailable.
[ -e "$OUT/chrome_sandbox" ] || { echo "FATAL: $OUT/chrome_sandbox missing (build-linux.sh release builds it)" >&2; exit 2; }
cp "$OUT/chrome_sandbox" "$PKG/chrome-sandbox"; COPIED+=("$OUT/chrome_sandbox")
# Resources.
need resources.pak icudtl.dat locales/en-US.pak
opt chrome_100_percent.pak chrome_200_percent.pak theme_resources_100_percent.pak ui_resources_100_percent.pak
if [ -e "$OUT/v8_context_snapshot.bin" ]; then need v8_context_snapshot.bin; else need snapshot_blob.bin; fi
for d in PrivacySandboxAttestationsPreloaded MEIPreload; do if [ -d "$OUT/$d" ]; then cp -a "$OUT/$d" "$PKG/"; COPIED+=("$OUT/$d"); fi; done
# Icons (installer/theme in upstream; the fork pruned that target).
for s in 24 48 64 128 256; do
  cp "$SRC/chrome/app/theme/chromium/linux/product_logo_$s.png" "$PKG/product_logo_$s.png"
done
# Legal.
cp "$SRC/LICENSE" "$PKG/LICENSE"
CREDITS=$OUT/gen/components/resources/about_credits.html
[ -f "$CREDITS" ] || { echo "FATAL: $CREDITS missing" >&2; exit 2; }
cp "$CREDITS" "$PKG/about_credits.html"; COPIED+=("$CREDITS")
if [ "$LOCAL" != 1 ]; then shuttle_receipt_fresh "$OUT" "${COPIED[@]}" || exit 2; fi

# Page-tools sidecar (agent-browser, Apache-2.0): the browser looks for it next to the binary.
sidecar=none
if [ "${SHUTTLE_NO_SIDECAR:-}" != 1 ]; then
  sidecar=$("$TOOLS/scripts/fetch-agent-browser.sh" "$PKG" linux-x64 | tee /dev/stderr | sed -n 's/^sidecar_sha256=\([0-9a-f]*\).*/\1/p')
  [ -n "$sidecar" ] || { echo "FATAL: no sidecar hash" >&2; exit 2; }
fi
printf '%s\n' "$receipt" > "$PKG/BUILD-RECEIPT.txt"

for f in shuttle chrome_crashpad_handler chrome-sandbox libEGL.so libGLESv2.so libvk_swiftshader.so \
         libvulkan.so.1 libqt5_shim.so libqt6_shim.so; do
  if [ -f "$PKG/$f" ]; then "$STRIP" --strip-unneeded "$PKG/$f"; fi
done

cat > "$PKG/README-LINUX.txt" <<EOF
Shuttle $VERSION for Linux x86_64 (https://www.shuttlebrowser.com)

First install two libraries that minimal systems (and a fresh WSL2 Ubuntu) don't have:
  Ubuntu 24.04+ / Debian 13:  sudo apt install libnss3 libasound2t64
  Ubuntu 22.04 / Debian 12:   sudo apt install libnss3 libasound2
  Fedora:                     sudo dnf install nss alsa-lib
(Without them: "error while loading shared libraries: libnspr4.so".)

Run:   ./shuttle            (DevTools/CDP on 127.0.0.1:9222, MCP at http://127.0.0.1:9222/mcp)
       ./shuttle --headless (no window)
No window opens at startup: Shuttle is running once it prints
"DevTools listening on ws://127.0.0.1:9222/...". The first agent tab opens a window.
If 9222 is taken it picks a free port and prints that one instead.
Windows: run it in WSL2 (WSLg shows the window). dbus/MESA/glx lines on WSL are harmless.

Sandbox: Shuttle uses the user-namespace sandbox. If startup fails with "No usable sandbox!"
(e.g. Ubuntu 23.10+ restricts unprivileged user namespaces through AppArmor), use the setuid
helper that ships here (as root, once):
  sudo chown root:root chrome-sandbox && sudo chmod 4755 chrome-sandbox
Shuttle finds it next to the binary. Do not run with --no-sandbox.

Licenses: LICENSE (BSD-3-Clause), about_credits.html (third-party), agent-browser.LICENSE.
EOF

mkdir -p "$DEST"
TAR=$DEST/$NAME.tar.xz
( set -o noclobber; : > "$TAR.tmp" ) 2>/dev/null || { echo "FATAL: $TAR.tmp exists (another run?)" >&2; exit 2; }
tar -C "$WORK" --owner=0 --group=0 --numeric-owner -cf - shuttle | xz -T0 -9 >> "$TAR.tmp"
ln "$TAR.tmp" "$TAR" 2>/dev/null || { rm -f "$TAR.tmp"; echo "FATAL: $TAR appeared meanwhile; not overwritten" >&2; exit 2; }
rm -f "$TAR.tmp"
(cd "$DEST" && sha256sum "$NAME.tar.xz" > "$NAME.tar.xz.sha256")
{
  echo "shuttle_version=${ver:-unset}"
  echo "platform=linux-x64"
  printf '%s\n' "$receipt"
  echo "sidecar_sha256=$sidecar"
  echo "artifact=$NAME.tar.xz"
  echo "artifact_sha256=$(cut -d' ' -f1 "$TAR.sha256")"
  echo "packaged_at=$(date -u +%Y-%m-%dT%H:%M:%SZ)"
} > "$DEST/MANIFEST.txt"
echo "unpacked: $(du -sh "$PKG" | cut -f1), files: $(find "$PKG" -type f | wc -l)"
echo "$TAR: $(du -h "$TAR" | cut -f1)"
cat "$TAR.sha256"
