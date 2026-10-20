#!/bin/bash
# Usage (on the Linux build host): scripts/build-linux.sh [dev|release] [autoninja args...]
# Linux counterpart of build.sh: builds `chrome` in $SHUTTLE_ROOT/src/out/<Dev|Release>,
# logs to $SHUTTLE_ROOT/logs/build-<Dir>.log, ends with "BUILD EXIT <rc>".
# GN args: config/args-common.gn + config/args-<kind>.gn + config/args-linux.gn
# (+ config/args-linux-<kind>.gn if present: per-kind Linux overrides, e.g. x64 for release).
# Safety (lib/build-common.sh): lock per out dir, SHUTTLE_MIN_FREE_GB (20) free, nice -n ${NICE:-19}, `-local_jobs`
# always set (default 10: the host's RAM, not its CPU count, is the limit; SHUTTLE_LOCAL_JOBS or `-local_jobs N`).
# SHUTTLE_KEEP_ARGS=1 refuses to change args.gn. SHUTTLE_OUT_DIR=<Dir> overrides the out dir name (e.g. a second
# release config next to out/Release). A successful build writes out/<Dir>/shuttle-build-receipt.txt.
set -eo pipefail
# siso switches to a quiet, progress-less log when it sees an AI agent env; build-watch.py needs the progress lines.
. "$(dirname "$0")/lib/paths.sh"
. "$(dirname "$0")/lib/build-common.sh"
. "$(dirname "$0")/lib/receipt.sh"
unset CLAUDECODE AI_AGENT
export PATH="$PATH:$HOME/depot_tools"
SRC="$SHUTTLE_SRC"
kind="${1:-dev}"; shift || true
case "$kind" in
  release) dir=${SHUTTLE_OUT_DIR:-Release}; targets=(chrome chrome_sandbox) ;;
  dev)     dir=${SHUTTLE_OUT_DIR:-Dev}; targets=(chrome) ;;
  *) echo "unknown build kind: $kind (use release|dev)" >&2; exit 2 ;;
esac
shuttle_build_args 10 "$@" || exit 2
shuttle_build_prefix
cfg="$(cd "$(dirname "$0")/.." && pwd)/config"
files=("$cfg/args-common.gn" "$cfg/args-$kind.gn" "$cfg/args-linux.gn")
[[ -f "$cfg/args-linux-$kind.gn" ]] && files+=("$cfg/args-linux-$kind.gn")
want="$(cat "${files[@]}")"
if [[ "$kind" == release ]]; then shuttle_build_salt "$SRC" "$cfg" || exit 2; fi
cd "$SRC"
shuttle_build_lock "$SRC/out/$dir" || exit 2
trap shuttle_build_unlock EXIT; trap 'exit 130' INT; trap 'exit 143' TERM  # traps run after the foreground build exits
shuttle_build_disk "$SRC/out" || exit 2
mkdir -p "out/$dir"
if [[ ! -f "out/$dir/args.gn" || "$(cat "out/$dir/args.gn")" != "$want" ]]; then
  [[ "${SHUTTLE_KEEP_ARGS:-0}" != 1 ]] || { echo "out/$dir/args.gn differs (SHUTTLE_KEEP_ARGS=1)" >&2; exit 2; }
  printf '%s\n' "$want" > "out/$dir/args.gn"
  gn gen "out/$dir" || exit 1
fi
mkdir -p "$SHUTTLE_LOGS"
log="$SHUTTLE_LOGS/build-$dir.log"
: > "$log"
start=$(date +%s)
# script(1) -c takes one shell string: quote every word so args with spaces/metacharacters survive.
cmd=$(printf '%q ' "${SHUTTLE_BUILD_PREFIX[@]}" autoninja -C "out/$dir" "${SHUTTLE_BUILD_ARGS[@]}" "${targets[@]}")
echo "build: $cmd"
shuttle_receipt_begin "$SRC/out/$dir"
set +e
script -q -f -e -c "$cmd" "$log"
rc=$?
shuttle_receipt_end "$SRC/out/$dir" $rc "$SRC/out/$dir/shuttle"
echo "BUILD EXIT $rc after $(( ($(date +%s) - start) / 60 ))m" | tee -a "$log"
exit $rc
