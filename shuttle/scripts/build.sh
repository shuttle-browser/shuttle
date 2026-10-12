#!/bin/zsh
# Usage: scripts/build.sh [release|dev|sccache] [extra autoninja args...]
# Builds the `chrome` target (override: AC_TARGET=base) in $SHUTTLE_ROOT/src/out/<Release|Dev|Sccache>.
# `sccache` = dev args + cc_wrapper=sccache + use_clang_modules=false in out/Sccache (distcc-style
# spike; client config ~/.shuttle/sccache.toml, see docs/TOOLING.md and config/sccache.example.toml).
# Runs in a PTY via `script` so the live status line stays visible, while the
# raw output is also logged to $SHUTTLE_ROOT/logs/build-<dir>.log (for monitoring).
# Throttle with e.g. `scripts/build.sh dev -j 6` (parallel jobs); runs under nice (NICE=10 default).
# GN args come from config/args-common.gn + config/args-<kind>.gn (re-generated on change).
set -e
# siso switches to a quiet agent mode under these (no progress lines, build-watch.py stays silent)
. "$(dirname "$0")/lib/paths.sh"
unset CLAUDECODE AI_AGENT
export PATH="$PATH:$HOME/depot_tools"
SRC="$SHUTTLE_SRC"
kind="${1:-release}"; shift || true

case "$kind" in
  release) dir=Release ;;
  dev)     dir=Dev ;;
  sccache) dir=Sccache ;;
  *) echo "unknown build kind: $kind (use release|dev|sccache)"; exit 2 ;;
esac

cfg="$(cd "$(dirname "$0")/.." && pwd)/config"
if [[ "$kind" == sccache ]]; then
  want="$(cat "$cfg/args-common.gn" "$cfg/args-dev.gn")"
else
  want="$(cat "$cfg/args-common.gn" "$cfg/args-$kind.gn")"
fi
# sccache-dist (build-host compiles): always for `sccache`, opt-in for release with AC_SCCACHE=1.
if [[ "$kind" == sccache || "${AC_SCCACHE:-0}" == 1 ]]; then
  want+=$'\n''cc_wrapper = "sccache"'$'\n''use_clang_modules = false'
  export SCCACHE_CONF="${SCCACHE_CONF:-$SHUTTLE_CONF/sccache.toml}"
fi
# AC_REMOTE=1: compile C++ remotely on a NativeLink build host (Mac only; see docs/TOOLING.md
# "Remote builds"). Mac-only steps and links stay local. Needs the rewrapper cfg + wrapper
# from config/remote/ in buildtools/reclient_cfgs/chromium-browser-clang/ (copied below).
remote_args=()
if [[ "${AC_REMOTE:-0}" == 1 ]]; then
  want+=$'\n'"use_remoteexec = true"
  mkdir -p "$SRC/buildtools/reclient_cfgs/chromium-browser-clang"
  cp "$cfg/remote/rewrapper_mac.cfg" "$cfg/remote/clang_remote_wrapper" \
     "$SRC/buildtools/reclient_cfgs/chromium-browser-clang/"
  export SISO_PROJECT=shuttle SISO_REAPI_INSTANCE=main RBE_service_no_security=true
  remote_args=(-reapi_address="${AC_REAPI_ADDRESS:-${SHUTTLE_BUILD_HOST_IP:?set SHUTTLE_BUILD_HOST_IP or AC_REAPI_ADDRESS}:50051}" -reapi_instance=main -reapi_insecure
               -local_jobs="${AC_LOCAL_JOBS:-4}" -remote_jobs="${AC_REMOTE_JOBS:-12}")
fi
cd "$SRC"
mkdir -p "out/$dir"
if [[ ! -f "out/$dir/args.gn" || "$(cat "out/$dir/args.gn")" != "$want" ]]; then
  print -r -- "$want" > "out/$dir/args.gn"
  gn gen "out/$dir" || exit 1
fi

mkdir -p "$SHUTTLE_LOGS"
log="$SHUTTLE_LOGS/build-$dir.log"
: > "$log"
start=$(date +%s)
set +e
script -q -F "$log" nice -n "${NICE:-10}" caffeinate -i autoninja -C "out/$dir" "${AC_TARGET:-chrome}" "${remote_args[@]}" "$@"
rc=$?
mins=$(( ($(date +%s) - start) / 60 ))
echo "BUILD EXIT $rc after ${mins}m" | tee -a "$log"
exit $rc
