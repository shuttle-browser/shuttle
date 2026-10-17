#!/bin/zsh
# Usage: scripts/build.sh [release|dev|sccache] [extra autoninja args...]
# Builds the `chrome` target (override: SHUTTLE_TARGET=base) in $SHUTTLE_SRC/out/<Release|Dev|Sccache>.
# `sccache` = dev args + cc_wrapper=sccache + use_clang_modules=false in out/Sccache (client config
# ~/.shuttle/sccache.toml, see docs/TOOLING.md and config/sccache.example.toml); run it via build-sccache.sh.
# Runs in a PTY via `script` so the live status line stays visible, while the
# raw output is also logged to $SHUTTLE_ROOT/logs/build-<dir>.log (for monitoring).
# Safety (lib/build-common.sh): one build per out dir (lock taken before args.gn/gn gen/log), at least
# SHUTTLE_MIN_FREE_GB (20) free, nice -n ${NICE:-19}, `-local_jobs` always set (default 8 with sccache-dist, 4
# local; SHUTTLE_LOCAL_JOBS or `-local_jobs N` override), SHUTTLE_BG=1 adds taskpolicy -c background.
# GN args come from config/args-common.gn + config/args-<kind>.gn (re-generated on change);
# SHUTTLE_KEEP_ARGS=1 refuses to change args.gn instead (e.g. a packaging step must not reconfigure a build).
# A successful `chrome` build writes out/<dir>/shuttle-build-receipt.txt (lib/receipt.sh; packaging requires it).
# SHUTTLE_CLEAN=1 runs `gn clean` first (full rebuild). `-n` (dry run) still takes the lock and may run gn gen.
set -e
. "$(dirname "$0")/lib/paths.sh"
. "$(dirname "$0")/lib/build-common.sh"
. "$(dirname "$0")/lib/receipt.sh"
# siso switches to a quiet agent mode under these (no progress lines, build-watch.py stays silent)
unset CLAUDECODE AI_AGENT
export PATH="$PATH:$HOME/depot_tools"
SRC="$SHUTTLE_SRC"
kind="${1:-release}"; shift || true

case "$kind" in
  release) dir=Release ;;
  dev)     dir=Dev ;;
  sccache) dir=Sccache ;;
  *) echo "unknown build kind: $kind (use release|dev|sccache)" >&2; exit 2 ;;
esac

cfg="$(cd "$(dirname "$0")/.." && pwd)/config"
if [[ "$kind" == sccache ]]; then
  want="$(cat "$cfg/args-common.gn" "$cfg/args-dev.gn")"
else
  want="$(cat "$cfg/args-common.gn" "$cfg/args-$kind.gn")"
fi
# sccache-dist (build-host compiles): always for `sccache`, opt-in for release with SHUTTLE_SCCACHE=1.
jobs_default=4
if [[ "$kind" == sccache || "${SHUTTLE_SCCACHE:-0}" == 1 ]]; then
  want+=$'\n''cc_wrapper = "sccache"'$'\n''use_clang_modules = false'
  export SCCACHE_CONF="${SCCACHE_CONF:-$SHUTTLE_CONF/sccache.toml}"
  jobs_default=8
  if [[ "${SHUTTLE_SCCACHE_PREFLIGHT:-0}" != 1 && "${SHUTTLE_ALLOW_LOCAL:-0}" != 1 ]]; then
    echo "sccache builds go through scripts/build-sccache.sh (build-host preflight); SHUTTLE_ALLOW_LOCAL=1 skips it" >&2
    exit 2
  fi
fi
shuttle_build_args $jobs_default "$@" || exit 2
shuttle_build_prefix
cd "$SRC"
out="$SRC/out/$dir"
shuttle_build_lock "$out" || exit 2
trap shuttle_build_unlock EXIT; trap 'exit 130' INT; trap 'exit 143' TERM  # traps run after the foreground build exits
shuttle_build_disk "$SRC/out" || exit 2
mkdir -p "out/$dir"
[[ "${SHUTTLE_CLEAN:-0}" == 1 && -f "out/$dir/build.ninja" ]] && gn clean "out/$dir"
if [[ ! -f "out/$dir/args.gn" || "$(cat "out/$dir/args.gn")" != "$want" ]]; then
  if [[ "${SHUTTLE_KEEP_ARGS:-0}" == 1 ]]; then
    echo "out/$dir/args.gn differs from config/ (SHUTTLE_KEEP_ARGS=1): refusing to reconfigure" >&2; exit 2
  fi
  print -r -- "$want" > "out/$dir/args.gn"
  gn gen "out/$dir" || exit 1
fi

mkdir -p "$SHUTTLE_LOGS"
log="$SHUTTLE_LOGS/build-$dir.log"
: > "$log"
start=$(date +%s)
receipt=0; [[ -z "${SHUTTLE_TARGET:-}" || "$SHUTTLE_TARGET" == chrome ]] && receipt=1
(( receipt )) && shuttle_receipt_begin "$out"
set +e
# macOS script(1) aborts ("tcgetattr/ioctl: Operation not supported on socket") when stdin is a socket, e.g. a build
# started by an agent harness; /dev/null works.
[[ -t 0 ]] || exec </dev/null
echo "build: out/$dir ${SHUTTLE_BUILD_ARGS[*]} ${SHUTTLE_TARGET:-chrome} under ${SHUTTLE_BUILD_PREFIX[*]}"
script -q -F "$log" "${SHUTTLE_BUILD_PREFIX[@]}" caffeinate -i autoninja -C "out/$dir" "${SHUTTLE_BUILD_ARGS[@]}" "${SHUTTLE_TARGET:-chrome}"
rc=$?
(( receipt )) && shuttle_receipt_end "$out" $rc "$out/Shuttle.app/Contents/MacOS/Shuttle"
mins=$(( ($(date +%s) - start) / 60 ))
echo "BUILD EXIT $rc after ${mins}m" | tee -a "$log"
exit $rc
