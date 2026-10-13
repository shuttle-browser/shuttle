#!/bin/zsh
# syntax-check.sh <failed-output-substring> <src.cc>...: -fsyntax-only each src with the failed step's flags
. "$(dirname "$0")/lib/paths.sh"
cd "$SHUTTLE_SRC/out/Dev"
pat=$1; shift
cmd=$(sed -E 's/\x1b\[[0-9;]*m//g' siso_output | grep -A3 "FAILED: .*$pat" | grep -m1 'clang++')
[[ -z $cmd ]] && { echo "no command for $pat"; exit 1; }
for s in "$@"; do
  c=$(echo "$cmd" | sed -E "s# -MMD -MF [^ ]+##; s# -o [^ ]+# -fsyntax-only#; s# -c [^ ]+# ../../$s#")
  echo "== $s"; eval "$c" 2>&1 | sed -E 's/\x1b\[[0-9;]*m//g' | grep -E "error:" | head -20
done
