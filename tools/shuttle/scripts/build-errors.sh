#!/bin/zsh
# build-errors.sh <substring-of-output> [Dir]: print errors of the failed step(s) matching it (Dir default: $SHUTTLE_BUILD_DIR, out/Dev)
. "$(dirname "$0")/lib/paths.sh"
if [[ -n "${2:-}" ]]; then cd "$SHUTTLE_SRC/out/$2"; else cd "$SHUTTLE_SRC/${SHUTTLE_BUILD_DIR:-out/Dev}"; fi
sed -E 's/\x1b\[[0-9;]*m//g' siso_output | awk -v pat="$1" '/^FAILED: /{p=index($0,pat)>0} p&&/error:/{print}'
