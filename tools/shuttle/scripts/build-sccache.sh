#!/bin/zsh
# Usage: scripts/build-sccache.sh [extra autoninja args...]
# out/Sccache with cc_wrapper=sccache: sccache preprocesses on the Mac and compiles on a remote sccache-dist build
# host (scheduler URL in your sccache config, see config/sccache.example.toml), links stay local.
# Caps come from build.sh (nice 19, -local_jobs 8 unless given, lock, disk check). SHUTTLE_KIND=release builds
# out/Release (official args) the same way. SHUTTLE_CLEAN=1 = `gn clean` first (full rebuild).
# Preflight: refuses to start when the scheduler reports no build servers (the build would compile everything on
# the Mac) unless SHUTTLE_ALLOW_LOCAL=1. Limit: sccache 0.18 has no "never compile locally" switch, so a scheduler
# that drops out mid-build still falls back to local compiles; -local_jobs is the cap that keeps the Mac usable.
# The sccache server is shared (dev + release builds): started if absent, never stopped or restarted here.
# SHUTTLE_SCCACHE_LOCAL=1 (local baseline, empty config) and SHUTTLE_SCCACHE_FRESH=1 (wipe the cache) need a fresh
# server, so they refuse while one runs: stop it yourself (`sccache --stop-server`) when no build uses it.
# The local cache ($SHUTTLE_ROOT/sccache-cache, max 10 GiB) persists across runs.
# Setup: docs/TOOLING.md "Remote builds", config/sccache.example.toml.
. "$(dirname "$0")/lib/paths.sh"
kind="${SHUTTLE_KIND:-sccache}"
[[ "$kind" == release ]] && export SHUTTLE_SCCACHE=1
export SCCACHE_CONF="${SCCACHE_CONF:-$SHUTTLE_CONF/sccache.toml}"
export SCCACHE_DIR="${SCCACHE_DIR:-$SHUTTLE_ROOT/sccache-cache}"
export SCCACHE_ERROR_LOG="$SHUTTLE_LOGS/sccache.log" SCCACHE_LOG="${SCCACHE_LOG:-warn}"
# sccache has no env switch for dist; local mode = an empty config (no [dist] section).
[[ "${SHUTTLE_SCCACHE_LOCAL:-0}" == 1 ]] && export SCCACHE_CONF="$SHUTTLE_CONF/sccache-local.toml"
if pgrep -qx sccache; then
  if [[ "${SHUTTLE_SCCACHE_LOCAL:-0}" == 1 || "${SHUTTLE_SCCACHE_FRESH:-0}" == 1 ]]; then
    echo "an sccache server already runs (maybe for another build): SHUTTLE_SCCACHE_LOCAL/FRESH need a fresh one" >&2
    exit 2
  fi
else
  [[ "${SHUTTLE_SCCACHE_FRESH:-0}" == 1 ]] && rm -rf "$SCCACHE_DIR"
  sccache --start-server || exit 1
fi
st=$(sccache --dist-status 2>&1); echo "$st"
if [[ "${SHUTTLE_SCCACHE_LOCAL:-0}" != 1 && "${SHUTTLE_ALLOW_LOCAL:-0}" != 1 ]] &&
   ! print -r -- "$st" | grep -q '"SchedulerStatus".*"num_servers":[1-9]'; then
  echo "sccache-dist: no build servers reachable; the build would compile locally. Start the build host, or" \
       "set SHUTTLE_ALLOW_LOCAL=1 (ask the maintainer first)." >&2
  exit 3
fi
sccache --zero-stats >/dev/null 2>&1   # shared server: the stats are approximate when two builds overlap
SHUTTLE_SCCACHE_PREFLIGHT=1 "$(dirname "$0")/build.sh" "$kind" "$@"
rc=$?
sccache --show-stats | tee "$SHUTTLE_LOGS/sccache-stats-Sccache.txt"
exit $rc
