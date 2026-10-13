#!/bin/zsh
# Usage: scripts/build-remote.sh [dev|release] [extra autoninja args...]
# Remote build on a NativeLink build host: scripts/build.sh with SHUTTLE_REMOTE=1 and
# SHUTTLE_REMOTE_JOBS=8 (override via env). See docs/TOOLING.md "Remote builds".
export SHUTTLE_REMOTE=1 SHUTTLE_REMOTE_JOBS="${SHUTTLE_REMOTE_JOBS:-${AC_REMOTE_JOBS:-8}}"
exec "$(dirname "$0")/build.sh" "$@"
