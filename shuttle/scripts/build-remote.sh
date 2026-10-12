#!/bin/zsh
# Usage: scripts/build-remote.sh [dev|release] [extra autoninja args...]
# Remote build on a NativeLink build host: scripts/build.sh with AC_REMOTE=1 and
# AC_REMOTE_JOBS=8 (override via env). See docs/TOOLING.md "Remote builds".
export AC_REMOTE=1 AC_REMOTE_JOBS="${AC_REMOTE_JOBS:-8}"
exec "$(dirname "$0")/build.sh" "$@"
