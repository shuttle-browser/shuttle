#!/bin/bash
# Relaunches the test browser on 9333 (fresh /tmp/ac-profile, log /tmp/ac-crash.log via
# --enable-logging=stderr) with agents alice (2 sessions) + steve, one tab each.
# Usage: scripts/test-browser.sh [close]
. "$(dirname "$0")/lib/paths.sh"
WS=$(curl -s -m 2 http://127.0.0.1:9333/json/version | python3 -c 'import json,sys;print(json.load(sys.stdin)["webSocketDebuggerUrl"])' 2>/dev/null)
[ -n "$WS" ] && node -e 'const ws=new WebSocket(process.argv[1]);ws.onopen=()=>ws.send(JSON.stringify({id:1,method:"Browser.close"}));ws.onmessage=()=>process.exit(0)' "$WS" && sleep 3
[ "$1" = "close" ] && exit 0
rm -rf /tmp/ac-profile; : > /tmp/ac-crash.log
open -g -n --stderr /tmp/ac-crash.log "$SHUTTLE_SRC/out/Sccache/Shuttle.app" --args --user-data-dir=/tmp/ac-profile --remote-debugging-port=9333 --use-mock-keychain --enable-logging=stderr
for i in $(seq 30); do curl -s -m 1 http://127.0.0.1:9333/json/version >/dev/null && break; sleep 0.5; done
M() { node "$(dirname "$0")/mcp-call.mjs" 9333 "$@"; }; sid() { python3 -c 'import json,sys;print(json.load(sys.stdin)["sessionId"])'; }
M create-agent '{"id":"alice","name":"Alice","color":"cyan"}' >/dev/null; M create-agent '{"id":"steve","name":"Steve","color":"blue"}' >/dev/null
A1=$(M open-session '{"agent":"alice"}' | sid); S1=$(M open-session '{"agent":"steve"}' | sid); A2=$(M open-session '{"agent":"alice"}' | sid)
M allocate-tab "{\"sessionId\":\"$A1\",\"url\":\"https://example.com\"}" >/dev/null
M allocate-tab "{\"sessionId\":\"$S1\",\"url\":\"https://www.silkweave.dev\"}" >/dev/null
M allocate-tab "{\"sessionId\":\"$A2\",\"url\":\"https://example.com/\"}" >/dev/null
echo "test browser ready"
