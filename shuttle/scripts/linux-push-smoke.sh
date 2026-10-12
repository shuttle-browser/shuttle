#!/usr/bin/env bash
# Linux push smoke (scripts/linux-push-smoke.sh [headless|x11|wayland], default headless): fresh profile,
# GCM vmodule logging + net-log (/tmp/ac-push-netlog.json), scripts/smoke-push.mjs, Browser.close,
# then chrome's exit status and the fatal grep (same as linux-smoke.sh). Log: /tmp/ac-push-<mode>.log.
. "$(dirname "$0")/lib/paths.sh"
G='FATAL|Check failed|DUMP_WILL_BE|Trying to access an unregistered pref|Trying to register KeyedService Factory|DanglingRawPtrDetected'
mode=${1:-headless}; H=$(cd "$(dirname "$0")" && pwd)
cd "$SHUTTLE_SRC" || exit 1
[ -x out/Dev/shuttle ] || { echo "FATAL: $SHUTTLE_ROOT/src/out/Dev/shuttle missing (Shuttle build required; no fallback to chrome)" >&2; exit 2; }
rm -rf /tmp/ac-profile; args=(--user-data-dir=/tmp/ac-profile --no-first-run --vmodule="checkin_request=1,registration_request=1,gcm_*=1,mcs_client=1,push_messaging*=1" --log-net-log=/tmp/ac-push-netlog.json about:blank)
case $mode in
  headless) out/Dev/shuttle --headless "${args[@]}" > /tmp/ac-push-$mode.log 2>&1 & ;;
  x11) DISPLAY=:0 out/Dev/shuttle --ozone-platform=x11 "${args[@]}" > /tmp/ac-push-$mode.log 2>&1 & ;;
  wayland) XDG_RUNTIME_DIR=/mnt/wslg/runtime-dir WAYLAND_DISPLAY=wayland-0 \
    out/Dev/shuttle --ozone-platform=wayland "${args[@]}" > /tmp/ac-push-$mode.log 2>&1 & ;;
esac
pid=$!
for i in $(seq 1 30); do curl -s 127.0.0.1:9222/json/version >/dev/null && break; sleep 1; done
timeout 180 node "$H/smoke-push.mjs" 9222; echo "[push-$mode] script rc=$?"
node "$H/cdp-sandbox-close.mjs" 9222 | tail -2
( sleep 60; kill -9 $pid 2>/dev/null ) & killer=$!
wait $pid; ec=$?; kill $killer 2>/dev/null
echo "[push-$mode] shuttle exit $ec, fatals=$(grep -aciE "$G" /tmp/ac-push-$mode.log)"
grep -aiE "$G" /tmp/ac-push-$mode.log | head -3 | cut -c1-250
echo PUSH-DONE
