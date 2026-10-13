#!/bin/zsh
# build-errors.sh <substring-of-output> [Dir=Sccache]: print errors of the failed step(s) matching it
. "$(dirname "$0")/lib/paths.sh"
cd "$SHUTTLE_SRC/out/${2:-Sccache}"
sed -E 's/\x1b\[[0-9;]*m//g' siso_output | awk -v pat="$1" '/^FAILED: /{p=index($0,pat)>0} p&&/error:/{print}'
