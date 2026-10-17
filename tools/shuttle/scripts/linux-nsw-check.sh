#!/usr/bin/env bash
# Linux no-startup-window check (scripts/linux-nsw-check.sh [x11|wayland|headless], default x11) for the current out/Dev:
# 1. start without URL -> alive, 0 page targets, 0 visible windows; 2. MCP allocate-tab -> window appears,
# close-session -> 0 pages/windows, process alive; 5. second launch (no args) is forwarded and opens a window;
# 3. Browser.close -> exit 0, no fatals; 4. launch WITH a URL arg -> window with that page. Steps 4/5 skip headless.
# Visible windows are counted on X11 only (xwininfo -tree, class shuttle, IsViewable); Wayland: CDP only.
# Note: chrome://omnibox-popup.top-chrome/ counts as a page target once a window exists. Log /tmp/sh-nsw-<mode>.log.
# Manual helper (X11/WSLg). Owns its browsers: each launch uses its own profile and --remote-debugging-port=0, and
# every CDP/MCP call goes to the port in that profile's DevToolsActivePort, so another browser is never touched.
# Exit 1 if a launch, MCP call or Browser.close step failed or fatals logged.
. "$(dirname "$0")/lib/paths.sh"
G='FATAL|Check failed|DUMP_WILL_BE|Trying to access an unregistered pref|Trying to register KeyedService Factory|DanglingRawPtrDetected'
mode=${1:-x11}; S=$(cd "$(dirname "$0")" && pwd); log=/tmp/sh-nsw-$mode.log
cd "$SHUTTLE_SRC" || exit 1
[ -x out/Dev/shuttle ] || { echo "FATAL: $SHUTTLE_ROOT/src/out/Dev/shuttle missing (Shuttle build required; no fallback to chrome)" >&2; exit 2; }
echo "commit $(git log -1 --format=%h), shuttle built $(stat -c %y out/Dev/shuttle | cut -c1-19), mode=$mode"
: > "$log"; bad=0; fails=$(mktemp); profs=("$fails")  # $fails: steps failed inside pipelines/subshells
pid=; port=
trap '[ -n "$pid" ] && kill -9 "$pid" 2>/dev/null && wait "$pid" 2>/dev/null; rm -rf "${profs[@]}"' EXIT
run_chrome() {  # run_chrome [extra args...]: foreground invocation (caller backgrounds if needed)
  local a=(--user-data-dir="$prof" --no-first-run "$@")
  case $mode in
    headless) out/Dev/shuttle --headless "${a[@]}" ;;
    x11) DISPLAY=:0 out/Dev/shuttle --ozone-platform=x11 "${a[@]}" ;;
    wayland) XDG_RUNTIME_DIR=/mnt/wslg/runtime-dir WAYLAND_DISPLAY=wayland-0 out/Dev/shuttle --ozone-platform=wayland "${a[@]}" ;;
    *) echo "unknown mode $mode"; exit 2 ;;
  esac >> "$log" 2>&1
}
launch() {  # own profile + ephemeral port; a launch that isn't ready in 30 s ends the run (its process is killed)
  prof=$(mktemp -d "${TMPDIR:-/tmp}/sh-nsw.XXXXXX"); profs+=("$prof"); run_chrome --remote-debugging-port=0 "$@" & pid=$!
  for _ in $(seq 1 30); do
    port=$(head -1 "$prof/DevToolsActivePort" 2>/dev/null)
    if [[ $port =~ ^[0-9]+$ ]] && curl -s -m 2 "127.0.0.1:$port/json/version" >/dev/null; then sleep 3; return; fi
    kill -0 "$pid" 2>/dev/null || break; sleep 1
  done
  echo "  FAIL: our browser has no CDP endpoint after 30 s"; exit 1; }
pages() { curl -s -m 5 "127.0.0.1:$port/json/list" | python3 -c 'import json,sys;p=[t["url"] for t in json.load(sys.stdin) if t["type"]=="page"];print(len(p),p[:3])' 2>/dev/null || echo "no-cdp"; }
xwins() {  # viewable top-level X11 windows of chrome (WM_CLASS shuttle)
  [ "$mode" = x11 ] || { echo "n/a"; return; }
  local n=0 names=""
  for w in $(DISPLAY=:0 xwininfo -root -tree 2>/dev/null | awk '/\("shuttle/{print $1}'); do
    DISPLAY=:0 xwininfo -id "$w" 2>/dev/null | grep -q 'IsViewable' || continue
    n=$((n+1)); names+="$(DISPLAY=:0 xprop -id "$w" _NET_WM_NAME 2>/dev/null | cut -d= -f2 | cut -c1-50);"
  done; echo "$n $names"; }
alive() { kill -0 $pid 2>/dev/null && echo yes || echo NO; }
state() { echo "  [$1] alive=$(alive) pages=$(pages) x11_windows=$(xwins)"; }
M() { node "$S/mcp-call.mjs" "$port" "$@" || { echo "  FAIL: $1" >&2; echo "$1" >> "$fails"; return 1; }; }
cdpclose() { timeout 15 node -e 'fetch(`http://127.0.0.1:${process.argv[1]}/json/version`).then(r=>r.json()).then(v=>{const ws=new WebSocket(v.webSocketDebuggerUrl);ws.onopen=()=>ws.send(JSON.stringify({id:1,method:"Browser.close"}));ws.onmessage=()=>process.exit(0)})' "$port"; }
finish() { cdpclose; ( sleep 60; kill -9 $pid 2>/dev/null ) & k=$!; wait $pid; ec=$?; pid=; kill $k 2>/dev/null
  local f; f=$(grep -aciE "$G" "$log"); { [ "$ec" = 0 ] && [ "$f" = 0 ]; } || bad=1
  echo "  [$1] Browser.close -> shuttle exit $ec, fatals=$f, still_listening=$(ss -ltn | grep -c ":$port ")"; }

echo "== 1. start without URL"
launch; state start
echo "== 2. MCP allocate-tab / close-session"
M create-agent '{"id":"nsw","name":"NSW"}' >/dev/null
sid=$(M open-session '{"agent":"nsw"}' | python3 -c 'import json,sys;print(json.load(sys.stdin)["sessionId"])')
M allocate-tab "{\"sessionId\":\"$sid\",\"url\":\"chrome://version\"}" | cut -c1-120; sleep 3; state after-allocate
M close-session "{\"sessionId\":\"$sid\"}" | cut -c1-120; sleep 3; state after-close-session
if [ "$mode" != headless ]; then
  echo "== 5. second launch (no args) while running"
  run_chrome; echo "  second launch exit $?"; sleep 4; state after-second-launch
fi
echo "== 3. Browser.close"; finish windowless-run
if [ "$mode" != headless ]; then
  echo "== 4. launch WITH URL arg"
  echo "<title>nsw-urlarg</title>urlarg" > /tmp/nsw-urlarg.html; launch file:///tmp/nsw-urlarg.html; state with-url; finish with-url
fi
grep -aiE "$G" "$log" | head -3 | cut -c1-250
if [ "$bad" = 0 ] && [ ! -s "$fails" ] && [ -n "$sid" ]; then echo NSW-DONE; else echo NSW-FAILED; exit 1; fi
