#!/usr/bin/env bash
# Linux smoke (scripts/linux-smoke.sh) for the current out/Dev: headless, WSLg X11, WSLg Wayland (default: all three).
# Fresh profile, no debug flags (DevTools is always on at 9222), CDP smoke, Browser.close, then
# check the log for fatals AND chrome's exit status (SIGTRAP on shutdown = exit 133, e.g.
# DanglingRawPtrDetected). Usage: ac-wslg-smoke.sh [headless] [x11] [wayland]
. "$(dirname "$0")/lib/paths.sh"
G='FATAL|Check failed|DUMP_WILL_BE|Trying to access an unregistered pref|Trying to register KeyedService Factory|DanglingRawPtrDetected'
H=$(cd "$(dirname "$0")" && pwd)
cd "$SHUTTLE_SRC" || exit 1
[ -x out/Dev/shuttle ] || { echo "FATAL: $SHUTTLE_ROOT/src/out/Dev/shuttle missing (Shuttle build required; no fallback to chrome)" >&2; exit 2; }
echo "commit $(git log -1 --format=%h), shuttle built $(stat -c %y out/Dev/shuttle | cut -c1-19)"
modes=${*:-headless x11 wayland}
for mode in $modes; do
  rm -rf /tmp/ac-profile
  args=(--user-data-dir=/tmp/ac-profile --no-first-run about:blank)
  case $mode in
    headless) out/Dev/shuttle --headless "${args[@]}" > /tmp/ac-run-$mode.log 2>&1 & ;;
    x11) DISPLAY=:0 out/Dev/shuttle --ozone-platform=x11 "${args[@]}" > /tmp/ac-run-$mode.log 2>&1 & ;;
    wayland) XDG_RUNTIME_DIR=/mnt/wslg/runtime-dir WAYLAND_DISPLAY=wayland-0 \
      out/Dev/shuttle --ozone-platform=wayland "${args[@]}" > /tmp/ac-run-$mode.log 2>&1 & ;;
  esac
  pid=$!
  timeout 600 node "$(dirname "$0")/smoke-cdp.mjs" 9222 > /tmp/ac-smoke-$mode.out 2>&1; rc=$?
  clean=$(grep -c 'exceptions":\[\],"errors":\[\]' /tmp/ac-smoke-$mode.out)
  other=$(grep -v 'exceptions":\[\],"errors":\[\]' /tmp/ac-smoke-$mode.out | grep -vE 'createBrowserContext|Autofill.enable|Browser.getVersion')
  echo "[$mode] smoke rc=$rc clean_pages=$clean"
  [ -n "$other" ] && echo "[$mode] unexpected: $other" | cut -c1-300
  node "$H/cdp-sandbox-close.mjs" 9222 | tail -2
  ( sleep 60; kill -9 $pid 2>/dev/null ) & killer=$!
  wait $pid; ec=$?; kill $killer 2>/dev/null
  case $ec in 0) st="exit 0" ;; 133) st="SIGTRAP (133)" ;; 137) st="KILLED after 60 s (hang?)" ;; *) st="exit $ec" ;; esac
  echo "[$mode] shuttle $st, fatals=$(grep -aciE "$G" /tmp/ac-run-$mode.log) still_listening=$(ss -ltn | grep -c ':9222 ')"
  grep -aiE "$G" /tmp/ac-run-$mode.log | head -3 | cut -c1-250
done
echo SMOKE-DONE
