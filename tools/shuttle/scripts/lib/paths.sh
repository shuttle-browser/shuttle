# Sourced by scripts: resolves the Chromium checkout. The tooling lives in the fork at src/tools/shuttle/, so the paths
# are repo-relative: SHUTTLE_SRC = the fork (src/), SHUTTLE_ROOT = its parent (holds .gclient, logs/, release/).
# Machine-local settings (build host, signing identity, ...) go in ~/.shuttle/env (sourced if present).
SHUTTLE_TOOLS="$(cd "$(dirname "${BASH_SOURCE[0]:-$0}")/../.." && pwd -P)"
SHUTTLE_CONF="${SHUTTLE_CONF:-$HOME/.shuttle}"   # sccache client config, toolchain archive, env
[ -f "$SHUTTLE_CONF/env" ] && . "$SHUTTLE_CONF/env"
SHUTTLE_SRC="${SHUTTLE_SRC:-$(cd "$SHUTTLE_TOOLS/../.." && pwd -P)}"
SHUTTLE_ROOT="${SHUTTLE_ROOT:-$(cd "$SHUTTLE_SRC/.." && pwd -P)}"
# Old AC_* env names (renamed 2026-10-13) still work for now: copied to SHUTTLE_* unless that is set.
for _v in KIND JOBS CLEAN TARGET SCCACHE SCCACHE_LOCAL SCCACHE_FRESH REMOTE REMOTE_JOBS LOCAL_JOBS REAPI_ADDRESS \
          NO_SIDECAR SYNCHECK_SLOTS SDK_VERSION; do
  if eval "[ -n \"\${AC_$_v+x}\" ] && [ -z \"\${SHUTTLE_$_v+x}\" ]"; then eval "export SHUTTLE_$_v=\"\$AC_$_v\""; fi
done; unset _v
SHUTTLE_LOGS="$SHUTTLE_ROOT/logs"; SHUTTLE_RELEASE="$SHUTTLE_ROOT/release"
export SHUTTLE_TOOLS SHUTTLE_ROOT SHUTTLE_SRC SHUTTLE_LOGS SHUTTLE_RELEASE SHUTTLE_CONF
