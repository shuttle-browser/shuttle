#!/usr/bin/env bash
# Installs branding/linux/shuttle.desktop + hicolor icons for the current user (Linux / WSLg), with
# Exec pointing at a Shuttle build. Usage: linux-install-desktop.sh [out-dir]   (default $SHUTTLE_ROOT/src/out/Dev)
#                                         linux-install-desktop.sh --uninstall
# Matches the browser's desktop id (GetDesktopName() = shuttle.desktop, WM_CLASS / Wayland app_id = shuttle).
set -euo pipefail
. "$(dirname "$0")/lib/paths.sh"
src="$SHUTTLE_SRC"; apps=~/.local/share/applications; icons=~/.local/share/icons/hicolor
tools=$(cd "$(dirname "$0")/.." && pwd); sizes="24 48 64 128 256"
if [ "${1:-}" = --uninstall ]; then
  rm -f "$apps/shuttle.desktop"; for s in $sizes; do rm -f "$icons/${s}x${s}/apps/shuttle.png"; done
  echo "removed shuttle.desktop + icons"; exit 0
fi
out=$(realpath -m "${1:-$src/out/Dev}")
[ -x "$out/shuttle" ] || { echo "FATAL: $out/shuttle missing (Shuttle build required; no fallback to chrome)" >&2; exit 2; }
for s in $sizes; do
  [ -f "$src/chrome/app/theme/chromium/linux/product_logo_$s.png" ] || { echo "FATAL: icon product_logo_$s.png missing" >&2; exit 2; }
done
mkdir -p "$apps"
sed "s|@EXEC@|$out/shuttle|g" "$tools/branding/linux/shuttle.desktop" > "$apps/shuttle.desktop"
for s in $sizes; do
  install -Dm644 "$src/chrome/app/theme/chromium/linux/product_logo_$s.png" "$icons/${s}x${s}/apps/shuttle.png"
done
command -v update-desktop-database >/dev/null && update-desktop-database "$apps" || true
command -v gtk-update-icon-cache >/dev/null && gtk-update-icon-cache -q -t "$icons" || true
echo "installed $apps/shuttle.desktop (Exec=$out/shuttle) + icons ($sizes)"
