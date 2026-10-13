#!/bin/bash
# Launches the local Shuttle build for interactive use with its MCP server.
# Usage: scripts/launch-shuttle.sh [port] [user-data-dir] [extra chrome args...]
#   port           CDP + MCP port (default 9222, the one .mcp.json uses). Shuttle is
#                  meant to own 9222: don't run other browsers with remote debugging on it.
#   user-data-dir  default ~/.shuttle/profile (persistent; agent profiles live in it).
# Opens in the background (no focus stealing). Stop it with CDP Browser.close or Cmd-Q.
set -euo pipefail
. "$(dirname "$0")/lib/paths.sh"
PORT="${1:-9222}"
DATA_DIR="${2:-$HOME/.shuttle/profile}"
shift $(( $# > 2 ? 2 : $# )) || true
APP="${SHUTTLE_APP:-$SHUTTLE_SRC/out/Sccache/Shuttle.app}"

mcp_ping() {
  curl -fsS -X POST "http://127.0.0.1:$PORT/mcp" -H 'Content-Type: application/json' \
    -d '{"jsonrpc":"2.0","id":1,"method":"ping"}' >/dev/null 2>&1
}
if mcp_ping; then
  echo "Shuttle already running: MCP http://127.0.0.1:$PORT/mcp"; exit 0
fi
if lsof -nP -iTCP:"$PORT" -sTCP:LISTEN >/dev/null 2>&1; then
  echo "Port $PORT is taken by another process (not Shuttle):" >&2
  lsof -nP -iTCP:"$PORT" -sTCP:LISTEN >&2
  echo "Quit it (e.g. Google Chrome started with --remote-debugging-port=$PORT) and retry." >&2
  exit 1
fi
mkdir -p "$DATA_DIR"
open -g -n "$APP" --args --user-data-dir="$DATA_DIR" --remote-debugging-port="$PORT" \
  --use-mock-keychain --enable-logging --log-file="$DATA_DIR/shuttle.log" "$@"
for _ in $(seq 60); do
  curl -fsS "http://127.0.0.1:$PORT/json/version" >/dev/null 2>&1 && break
  sleep 0.5
done
mcp_ping \
  && echo "Shuttle up: MCP http://127.0.0.1:$PORT/mcp, CDP http://127.0.0.1:$PORT" \
  || { echo "Shuttle did not answer on port $PORT" >&2; exit 1; }
