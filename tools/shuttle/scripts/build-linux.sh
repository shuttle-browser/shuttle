#!/bin/bash
# Usage (on the Linux build host): scripts/build-linux.sh [dev|release] [autoninja args...]
# Linux counterpart of build.sh: builds `chrome` in $SHUTTLE_ROOT/src/out/<Dev|Release>,
# logs to $SHUTTLE_ROOT/logs/build-<Dir>.log, ends with "BUILD EXIT <rc>".
# GN args: config/args-common.gn + config/args-<kind>.gn + config/args-linux.gn
# (+ config/args-linux-<kind>.gn if present: per-kind Linux overrides, e.g. x64 for release).
set -e
# siso switches to a quiet, progress-less log when it sees an AI agent env; build-watch.py needs the progress lines.
. "$(dirname "$0")/lib/paths.sh"
unset CLAUDECODE AI_AGENT
export PATH="$PATH:$HOME/depot_tools"
SRC="$SHUTTLE_SRC"
kind="${1:-dev}"; shift || true
case "$kind" in
  release) dir=Release; targets="chrome chrome_sandbox" ;;
  dev)     dir=Dev; targets=chrome ;;
  *) echo "unknown build kind: $kind (use release|dev)"; exit 2 ;;
esac
cfg="$(cd "$(dirname "$0")/.." && pwd)/config"
files=("$cfg/args-common.gn" "$cfg/args-$kind.gn" "$cfg/args-linux.gn")
[[ -f "$cfg/args-linux-$kind.gn" ]] && files+=("$cfg/args-linux-$kind.gn")
want="$(cat "${files[@]}")"
cd "$SRC"
mkdir -p "out/$dir"
if [[ ! -f "out/$dir/args.gn" || "$(cat "out/$dir/args.gn")" != "$want" ]]; then
  printf '%s\n' "$want" > "out/$dir/args.gn"
  gn gen "out/$dir" || exit 1
fi
mkdir -p "$SHUTTLE_LOGS"
log="$SHUTTLE_LOGS/build-$dir.log"
: > "$log"
start=$(date +%s)
set +e
script -q -f -e -c "nice -n ${NICE:-10} autoninja -C out/$dir $targets $*" "$log"
rc=$?
echo "BUILD EXIT $rc after $(( ($(date +%s) - start) / 60 ))m" | tee -a "$log"
exit $rc
