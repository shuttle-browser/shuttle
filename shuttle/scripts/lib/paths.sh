# Sourced by scripts: resolves the Chromium checkout. The tooling lives in the fork at src/shuttle/, so the paths are
# repo-relative: SHUTTLE_SRC = the fork (src/), SHUTTLE_ROOT = its parent (holds .gclient, logs/, release/).
# Machine-local settings (build host, signing identity, ...) go in ~/.shuttle/env (sourced if present).
SHUTTLE_TOOLS="$(cd "$(dirname "${BASH_SOURCE[0]:-$0}")/../.." && pwd -P)"
SHUTTLE_CONF="${SHUTTLE_CONF:-$HOME/.shuttle}"   # sccache client config, toolchain archive, env
[ -f "$SHUTTLE_CONF/env" ] && . "$SHUTTLE_CONF/env"
SHUTTLE_SRC="${SHUTTLE_SRC:-$(cd "$SHUTTLE_TOOLS/.." && pwd -P)}"
SHUTTLE_ROOT="${SHUTTLE_ROOT:-$(cd "$SHUTTLE_SRC/.." && pwd -P)}"
SHUTTLE_LOGS="$SHUTTLE_ROOT/logs"; SHUTTLE_RELEASE="$SHUTTLE_ROOT/release"
export SHUTTLE_TOOLS SHUTTLE_ROOT SHUTTLE_SRC SHUTTLE_LOGS SHUTTLE_RELEASE SHUTTLE_CONF
