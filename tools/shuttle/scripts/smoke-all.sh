#!/usr/bin/env bash
# smoke-all.sh: the owned smoke run. Launches <binary> with its own temp profile (--remote-debugging-port=0, port read
# from DevToolsActivePort), runs the probes, closes the browser itself (Browser.close, bounded wait, kill -9 only of
# the PID it started), checks the exit status and the log for fatals, removes its temp dir, prints PASS/FAIL/SKIP.
# Usage: scripts/smoke-all.sh <binary|Shuttle.app> [--with network,push,automation] [--mode headless|x11|wayland]
#   Default: cdp, session-cdp, agents-page, isolation (agent storage across blank tabs), focus, sandbox (Linux only: the page is not built on macOS), mcp phase 1,
#   restart on the same profile, mcp phase 2. Opt-in: network (own launch with --shuttle-listen=0.0.0.0, LAN),
#   push (needs SHUTTLE_SMOKE_NET=1, else SKIP), automation (own launch: --enable-automation, no debug port; binds
#   9222 when free, else a free port). x11/wayland use the WSLg defaults (DISPLAY=:0, /mnt/wslg/runtime-dir) unless
#   set. Run it under nice -n 19 on a shared machine. Env: SHUTTLE_SMOKE_READY_S (startup deadline, default 30).
# Exit: 0 all passed, 1 a FAIL, 2 usage, 3 no FAIL but something skipped (open, not green).
set -uo pipefail
here="$(cd "$(dirname "$0")" && pwd -P)"
G='FATAL|Check failed|DUMP_WILL_BE|Trying to access an unregistered pref|Trying to register KeyedService Factory|DanglingRawPtrDetected'
usage() { echo "usage: smoke-all.sh <binary|Shuttle.app> [--with network,push,automation] [--mode headless|x11|wayland]" >&2; exit 2; }
[[ $# -ge 1 ]] || usage
bin=${1%/}; shift; mode=headless; with=,
while [[ $# -gt 0 ]]; do
  [[ $# -ge 2 ]] || usage
  case $1 in --with) with=",$2," ;; --mode) mode=$2 ;; *) usage ;; esac
  shift 2
done
[[ $mode =~ ^(headless|x11|wayland)$ ]] || usage
for w in ${with//,/ }; do [[ $w =~ ^(network|push|automation)$ ]] || usage; done
exe=$bin; [[ $bin == *.app ]] && exe="$bin/Contents/MacOS/$(basename "$bin" .app)"
[[ -f $exe && -x $exe ]] || { echo "not an executable: $exe" >&2; exit 2; }

tmp=$(mktemp -d "${TMPDIR:-/tmp}/shuttle-smoke.XXXXXX") || exit 2
log="$tmp/browser.log"; pid=; port=; ready=; ec=; fail=0; open=0; results=()
trap 'stop; rm -rf "$tmp"' EXIT; trap 'exit 130' INT TERM
note() { results+=("$(printf '%-17s %s' "$1" "$2")"); case $2 in FAIL*) fail=1 ;; SKIP*) open=1 ;; esac; }
to() { if command -v timeout >/dev/null; then timeout "$@"; else perl -e 'alarm shift; exec @ARGV or die "exec: $!"' "$@"; fi; }

launch() {  # launch <profile> [args...]: start the browser in the background, wait until CDP answers
  local prof=$1 a; shift
  rm -f "$prof/DevToolsActivePort"  # left over from the previous launch on the same profile
  a=(--user-data-dir="$prof" --no-first-run --no-default-browser-check --enable-logging=stderr "$@")
  [[ $(uname) == Darwin ]] && a+=(--use-mock-keychain)
  case $mode in
    headless) a=(--headless "${a[@]}") ;;
    x11) a=(--ozone-platform=x11 "${a[@]}"); export DISPLAY=${DISPLAY:-:0} ;;
    wayland) a=(--ozone-platform=wayland "${a[@]}")
             export XDG_RUNTIME_DIR=${XDG_RUNTIME_DIR:-/mnt/wslg/runtime-dir} WAYLAND_DISPLAY=${WAYLAND_DISPLAY:-wayland-0} ;;
  esac
  echo "== launch ${a[*]}" >> "$log"
  "$exe" "${a[@]}" about:blank >> "$log" 2>&1 &
  pid=$!; port=; ready=
  local end=$((SECONDS + ${SHUTTLE_SMOKE_READY_S:-30}))
  while (( SECONDS < end )); do
    kill -0 "$pid" 2>/dev/null || { echo "browser exited during startup"; tail -5 "$log"; return 1; }
    port=$(head -1 "$prof/DevToolsActivePort" 2>/dev/null)
    if [[ $port =~ ^[0-9]+$ ]] && curl -fsS -m 2 "http://127.0.0.1:$port/json/version" >/dev/null 2>&1; then
      ready=1; echo "browser pid $pid, CDP port $port"; return 0
    fi
    sleep 0.5
  done
  echo "browser not ready after ${SHUTTLE_SMOKE_READY_S:-30} s (port '$port')"; tail -5 "$log"; return 1
}
stop() {  # close our own browser (only $pid, never by port), wait at most 30 s, then kill -9; sets ec
  [[ -n $pid ]] || return 0
  if [[ -n $ready ]] && kill -0 "$pid" 2>/dev/null; then
    to 15 node -e 'fetch(`http://127.0.0.1:${process.argv[1]}/json/version`).then((r) => r.json()).then((v) => {
      const w = new WebSocket(v.webSocketDebuggerUrl); w.onopen = () => w.send(JSON.stringify({ id: 1, method: "Browser.close" }));
      w.onmessage = w.onclose = () => process.exit(0); })' "$port" >/dev/null 2>&1
    local end=$((SECONDS + 30))
    while kill -0 "$pid" 2>/dev/null && (( SECONDS < end )); do sleep 0.5; done
  fi
  if kill -0 "$pid" 2>/dev/null; then kill -9 "$pid" 2>/dev/null; wait "$pid" 2>/dev/null; ec=killed
  else wait "$pid" 2>/dev/null; ec=$?; fi
  pid=; ready=
}
closed() { stop; if [[ $ec == 0 ]]; then note "close-$1" PASS; else note "close-$1" "FAIL (exit $ec; 133 = SIGTRAP)"; fi; }
probe() {  # probe <name> <seconds> <script> [args...]: probe deadline + hard timeout as backstop
  local name=$1 s=$2 rc; shift 2
  echo "== $name"
  SHUTTLE_SMOKE_DEADLINE_MS=$((s * 1000)) to $((s + 15)) node "$here/$1" "$port" "${@:2}" 2>&1 | sed 's/^/   /'
  rc=${PIPESTATUS[0]}
  case $rc in 0) note "$name" PASS ;; 3) note "$name" SKIP ;; *) note "$name" "FAIL (rc $rc)" ;; esac
  return "$rc"
}

extra=(); [[ $with == *,push,* ]] && extra=(--vmodule="checkin_request=1,registration_request=1,gcm_*=1,mcs_client=1,push_messaging*=1")
# windowed focus check: macOS windowed and X11 only (Wayland has no foreign-window query; xprop sees XWayland only)
focus=(); [[ $mode == x11 || ($mode != headless && $(uname) == Darwin) ]] && focus=(windowed)
if launch "$tmp/profile" --remote-debugging-port=0 ${extra[@]+"${extra[@]}"}; then
  probe cdp 300 smoke-cdp.mjs
  probe session-cdp 120 smoke-session-cdp.mjs
  probe agents-page 120 smoke-agents-page.mjs
  probe isolation 60 smoke-isolation.mjs
  probe focus 60 smoke-focus.mjs ${focus[@]+"${focus[@]}"}
  [[ $(uname) == Linux ]] && probe sandbox 60 smoke-sandbox.mjs
  [[ $with == *,push,* ]] && probe push 150 smoke-push.mjs
  probe mcp-phase1 120 smoke-mcp.mjs --phase1; p1=$?
  closed run1
  if [[ $p1 != 0 ]]; then note mcp-phase2 "SKIP (phase 1 failed)"
  elif launch "$tmp/profile" --remote-debugging-port=0; then probe mcp-phase2 60 smoke-mcp.mjs --phase2; closed run2
  else note relaunch FAIL; stop; fi
else note launch FAIL; stop; fi
if [[ $with == *,network,* ]]; then
  if launch "$tmp/net" --remote-debugging-port=0 --shuttle-listen=0.0.0.0; then
    probe network 60 smoke-network.mjs "$tmp/net"; closed network
  else note launch-network FAIL; stop; fi
fi
if [[ $with == *,automation,* ]]; then
  if launch "$tmp/auto" --enable-automation; then probe automation 60 check-automation.mjs "$tmp/auto"; closed automation
  else note launch-auto FAIL; stop; fi
fi
n=$(grep -acE "$G" "$log"); if [[ $n == 0 ]]; then note log-fatals PASS; else note log-fatals "FAIL ($n lines)"; fi
grep -aE "$G" "$log" | head -5 | cut -c1-250
echo "== summary ($mode, $exe)"; printf '  %s\n' "${results[@]}"
(( fail )) && exit 1; (( open )) && exit 3; exit 0
