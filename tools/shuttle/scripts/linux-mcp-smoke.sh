#!/usr/bin/env bash
# Linux MCP smoke (scripts/linux-mcp-smoke.sh [headless|x11|wayland], default headless) for the current out/Dev:
# fresh profile, scripts/smoke-mcp.mjs, Browser.close + chrome's exit status, relaunch on the SAME profile,
# smoke-mcp.mjs --persist-check, close again; then the fatal grep (same as linux-smoke.sh).
# Log: /tmp/sh-mcp-<mode>.log. Judge by both smoke rcs = 0, both shuttle exits = 0, fatals = 0.
. "$(dirname "$0")/lib/paths.sh"
G='FATAL|Check failed|DUMP_WILL_BE|Trying to access an unregistered pref|Trying to register KeyedService Factory|DanglingRawPtrDetected'
mode=${1:-headless}; H=$(cd "$(dirname "$0")" && pwd); log=/tmp/sh-mcp-$mode.log
cd "$SHUTTLE_SRC" || exit 1
[ -x out/Dev/shuttle ] || { echo "FATAL: $SHUTTLE_ROOT/src/out/Dev/shuttle missing (Shuttle build required; no fallback to chrome)" >&2; exit 2; }
echo "commit $(git log -1 --format=%h), shuttle built $(stat -c %y out/Dev/shuttle | cut -c1-19), mode=$mode"
rm -rf /tmp/sh-profile
launch() {
  args=(--user-data-dir=/tmp/sh-profile --no-first-run about:blank)
  case $mode in
    headless) out/Dev/shuttle --headless "${args[@]}" >> "$log" 2>&1 & ;;
    x11) DISPLAY=:0 out/Dev/shuttle --ozone-platform=x11 "${args[@]}" >> "$log" 2>&1 & ;;
    wayland) XDG_RUNTIME_DIR=/mnt/wslg/runtime-dir WAYLAND_DISPLAY=wayland-0 \
      out/Dev/shuttle --ozone-platform=wayland "${args[@]}" >> "$log" 2>&1 & ;;
    *) echo "unknown mode $mode"; exit 2 ;;
  esac
  pid=$!
  for _ in $(seq 1 30); do curl -s 127.0.0.1:9222/json/version >/dev/null && break; sleep 1; done
}
finish() {
  node "$H/cdp-sandbox-close.mjs" 9222 | tail -1
  ( sleep 60; kill -9 $pid 2>/dev/null ) & killer=$!
  wait $pid; ec=$?; kill $killer 2>/dev/null
  echo "[$1] shuttle exit $ec, fatals=$(grep -aciE "$G" "$log"), still_listening=$(ss -ltn | grep -c ':9222 ')"
}
: > "$log"
launch; timeout 300 node "$H/smoke-mcp.mjs" 9222; echo "[run1] smoke-mcp rc=$?"; finish run1
launch; timeout 120 node "$H/smoke-mcp.mjs" 9222 --persist-check; echo "[run2] persist-check rc=$?"; finish run2
grep -aiE "$G" "$log" | head -3 | cut -c1-250
echo MCP-DONE
