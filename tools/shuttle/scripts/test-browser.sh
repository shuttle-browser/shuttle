#!/bin/bash
# Relaunches the test browser on 9333 (fresh mktemp profile, log <profile>/shuttle.log via --enable-logging=stderr)
# with agents alice (2 sessions) + steve, one tab each. Manual helper; hits the internet. App: $SHUTTLE_APP or
# out/Release/Shuttle.app. Closes a Shuttle already on 9333 (MCP answers); refuses when another process owns 9333.
# Usage: scripts/test-browser.sh [close]
. "$(dirname "$0")/lib/paths.sh"
APP="${SHUTTLE_APP:-$SHUTTLE_SRC/out/Release/Shuttle.app}"
if ! curl -fsS -m 2 -X POST http://127.0.0.1:9333/mcp -H 'Content-Type: application/json' \
    -d '{"jsonrpc":"2.0","id":1,"method":"ping"}' >/dev/null 2>&1 && lsof -nP -iTCP:9333 -sTCP:LISTEN >/dev/null 2>&1; then
  echo "Port 9333 is taken by another process (not Shuttle): refusing." >&2; lsof -nP -iTCP:9333 -sTCP:LISTEN >&2; exit 1
fi
WS=$(curl -s -m 2 http://127.0.0.1:9333/json/version | python3 -c 'import json,sys;print(json.load(sys.stdin)["webSocketDebuggerUrl"])' 2>/dev/null)
[ -n "$WS" ] && node -e 'const ws=new WebSocket(process.argv[1]);ws.onopen=()=>ws.send(JSON.stringify({id:1,method:"Browser.close"}));ws.onmessage=()=>process.exit(0)' "$WS" && sleep 3
[ "$1" = "close" ] && exit 0
PROFILE=$(mktemp -d "${TMPDIR:-/tmp}/sh-test-browser.XXXXXX") || exit 1
open -g -n --stderr "$PROFILE/shuttle.log" "$APP" --args --user-data-dir="$PROFILE" --remote-debugging-port=9333 --use-mock-keychain --enable-logging=stderr
for _ in $(seq 30); do curl -s -m 1 http://127.0.0.1:9333/json/version >/dev/null && break; sleep 0.5; done
M() { node "$(dirname "$0")/mcp-call.mjs" 9333 "$@"; }; sid() { python3 -c 'import json,sys;print(json.load(sys.stdin)["sessionId"])'; }
M create-agent '{"id":"alice","name":"Alice","color":"cyan"}' >/dev/null; M create-agent '{"id":"steve","name":"Steve","color":"blue"}' >/dev/null
A1=$(M open-session '{"agent":"alice"}' | sid); S1=$(M open-session '{"agent":"steve"}' | sid); A2=$(M open-session '{"agent":"alice"}' | sid)
M allocate-tab "{\"sessionId\":\"$A1\",\"url\":\"https://example.com\"}" >/dev/null
M allocate-tab "{\"sessionId\":\"$S1\",\"url\":\"https://www.silkweave.dev\"}" >/dev/null
M allocate-tab "{\"sessionId\":\"$A2\",\"url\":\"https://example.com/\"}" >/dev/null
echo "test browser ready (profile + log: $PROFILE)"
