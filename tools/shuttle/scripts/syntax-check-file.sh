#!/bin/zsh
# syntax-check-file.sh [--flags-from <failed-step-substring>] <src.cc>...
# -fsyntax-only each source (path relative to $SHUTTLE_SRC) with its own compile flags from the current ninja graph
# of $SHUTTLE_BUILD_DIR (default out/Sccache). --flags-from borrows the flags of a FAILED step in siso_output instead
# (for files not in the graph yet). Read-only: safe while no gn gen runs. Generated headers come from the last build,
# so edited .mojom files show stale bindings.
# Load control: flags come from a cached compile DB (rebuilt when build.ninja changes), at most
# $SHUTTLE_SYNCHECK_SLOTS (default 2) checks run machine-wide at once, under nice 15.
# Exit: 0 = all clean, 1 = compiler errors or a source without a command, 2 = usage/setup error.
set -u -o pipefail
. "$(dirname "$0")/lib/paths.sh"
export PATH="$PATH:$HOME/depot_tools"
die() { print -u2 "syntax-check-file: $*"; exit 2; }
from=""
if [[ ${1:-} == --flags-from ]]; then (( $# >= 2 )) || die "--flags-from needs a substring"; from=$2; shift 2; fi
(( $# )) || die "usage: syntax-check-file.sh [--flags-from <failed-step-substring>] <src.cc>..."
out="$SHUTTLE_SRC/${SHUTTLE_BUILD_DIR:-out/Sccache}"
cd "$out" 2>/dev/null && [[ -f build.ninja ]] || die "no build.ninja in $out"
key=${out//\//_}
T=${SHUTTLE_SYNCHECK_TMP:-/tmp}   # compile DB + machine-wide slots
DB=$T/sh-compdb$key.tsv   # <src relative to $SHUTTLE_SRC>\t<command>
SLOTS=${SHUTTLE_SYNCHECK_SLOTS:-2}

# mkdir locks with a pid file; a lock whose owner died is taken over.
take() {  # take <dir>: 0 = got it
  if mkdir $1 2>/dev/null; then echo $$ > $1/pid; return 0; fi
  local p; p=$(cat $1/pid 2>/dev/null)
  if [[ -n $p ]] && ! kill -0 $p 2>/dev/null; then rm -rf $1; fi
  return 1
}
held=()
cleanup() { (( ${#held} )) && rm -rf $held; held=(); }
trap cleanup EXIT INT TERM

if [[ -n $from ]]; then
  [[ -f siso_output ]] || die "no siso_output in $out"
  base=$(sed -E 's/\x1b\[[0-9;]*m//g' siso_output | grep -A3 -- "FAILED: .*$from" | grep -m1 'clang++' || true)
  [[ -n $base ]] || die "no failed clang++ step matching '$from' in siso_output"
elif [[ ! -s $DB || build.ninja -nt $DB ]]; then
  until take $DB.lock; do sleep 1; done; held+=($DB.lock)
  if [[ ! -s $DB || build.ninja -nt $DB ]]; then
    nice -n 15 ninja -t compdb cc cxx objc objcxx | nice -n 15 python3 -c '
import json, os, sys
for e in json.load(sys.stdin):
    f = os.path.relpath(os.path.normpath(os.path.join(e["directory"], e["file"])), os.environ["SHUTTLE_SRC"])
    print(f + "\t" + e["command"])' > $DB.tmp && [[ -s $DB.tmp ]] && mv $DB.tmp $DB || { rm -f $DB.tmp; die "compdb failed"; }
  fi
  rm -rf $DB.lock; held=()
fi

slot=""
acquire() { while true; do for i in {1..$SLOTS}; do take $T/sh-synchk.slot$i && { slot=$T/sh-synchk.slot$i; return; }; done; sleep 1; done; }
rc=0
for s in "$@"; do
  if [[ -n $from ]]; then
    c=$(print -r -- "$base" | sed -E "s# -MMD -MF [^ ]+##; s# -o [^ ]+# -fsyntax-only#; s# -c [^ ]+# ../../$s#")
  else
    cmd=$(awk -F'\t' -v s="$s" '$1==s {print $2; exit}' $DB)
    if [[ -z $cmd ]]; then print "== $s: no command (not in build graph?)"; rc=1; continue; fi
    c=$(print -r -- "$cmd" | sed -E "s# -MMD -MF [^ ]+##; s# -o [^ ]+# -fsyntax-only#; s# -c # #")
  fi
  acquire; held+=($slot)
  print "== $s"
  log=$(eval "nice -n 15 $c" 2>&1); crc=$?
  rm -rf $slot; held=(); slot=""
  if (( crc )); then
    rc=1
    errs=$(print -r -- "$log" | sed -E 's/\x1b\[[0-9;]*m//g' | grep -E "error:" | head -20)
    print -r -- "${errs:-$(print -r -- "$log" | tail -20)}"
    print "FAILED (rc $crc)"
  fi
done
exit $rc
