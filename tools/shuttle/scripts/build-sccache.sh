#!/bin/zsh
# Usage: scripts/build-sccache.sh [extra autoninja args...]
# distcc-style spike: out/Sccache with cc_wrapper=sccache; sccache preprocesses on the Mac and
# compiles on a remote sccache-dist build host (scheduler URL in your sccache config,
# see config/sccache.example.toml), links stay local.
# Default -j 24 (override SHUTTLE_JOBS). SHUTTLE_SCCACHE_LOCAL=1 disables distribution (local baseline).
# The local cache ($SHUTTLE_ROOT/sccache-cache, max 10 GiB) persists across runs; SHUTTLE_SCCACHE_FRESH=1 wipes it
# first (for timing comparisons). Dev and release flags differ, so they don't share entries.
# SHUTTLE_CLEAN=1 runs `gn clean out/Sccache` first (full rebuild of the target).
# SHUTTLE_KIND=release builds out/Release (official beta args) the same way.
# Setup: docs/TOOLING.md "Remote builds", config/sccache.example.toml.
. "$(dirname "$0")/lib/paths.sh"
kind="${SHUTTLE_KIND:-sccache}"
[[ "$kind" == release ]] && export SHUTTLE_SCCACHE=1
export SCCACHE_CONF="${SCCACHE_CONF:-$SHUTTLE_CONF/sccache.toml}"
export SCCACHE_DIR="${SCCACHE_DIR:-$SHUTTLE_ROOT/sccache-cache}"
export SCCACHE_ERROR_LOG="$SHUTTLE_LOGS/sccache.log" SCCACHE_LOG="${SCCACHE_LOG:-warn}"
# sccache has no env switch for dist; local mode = an empty config (no [dist] section).
[[ "${SHUTTLE_SCCACHE_LOCAL:-0}" == 1 ]] && export SCCACHE_CONF="$SHUTTLE_CONF/sccache-local.toml"
[[ "${SHUTTLE_CLEAN:-0}" == 1 && -d "$SHUTTLE_SRC/out/Sccache" ]] && \
  (cd "$SHUTTLE_SRC" && PATH="$PATH:$HOME/depot_tools" gn clean out/Sccache)
sccache --stop-server >/dev/null 2>&1
[[ "${SHUTTLE_SCCACHE_FRESH:-0}" == 1 ]] && rm -rf "$SCCACHE_DIR"
sccache --start-server || exit 1
sccache --dist-status
"$(dirname "$0")/build.sh" "$kind" -j "${SHUTTLE_JOBS:-24}" "$@"
rc=$?
sccache --show-stats | tee "$SHUTTLE_LOGS/sccache-stats-Sccache.txt"
sccache --stop-server >/dev/null 2>&1
exit $rc
