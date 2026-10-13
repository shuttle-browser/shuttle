#!/bin/zsh
# syntax-check-file.sh <src.cc>...: -fsyntax-only each source (path relative to $SHUTTLE_ROOT/src)
# with its own compile flags from the current ninja graph. Read-only: safe while no gn gen runs.
# Generated headers come from the last build, so edited .mojom files show stale bindings.
# Load control: flags come from a cached compile DB (rebuilt when build.ninja changes), at most
# $SHUTTLE_SYNCHECK_SLOTS (default 2) checks run machine-wide at once, at background QoS (E-cores) + nice.
. "$(dirname "$0")/lib/paths.sh"
export PATH="$PATH:$HOME/depot_tools"
cd "$SHUTTLE_SRC/out/Dev"
DB=/tmp/sh-compdb.tsv   # <src relative to $SHUTTLE_ROOT/src>\t<command>
SLOTS=${SHUTTLE_SYNCHECK_SLOTS:-2}

lock_db() { until mkdir /tmp/sh-compdb.lock 2>/dev/null; do sleep 1; done; }
if [[ ! -s $DB || build.ninja -nt $DB ]]; then
  lock_db
  if [[ ! -s $DB || build.ninja -nt $DB ]]; then
    nice -n 15 ninja -t compdb cc cxx objc objcxx 2>/dev/null | nice -n 15 python3 -c '
import json, os, sys
for e in json.load(sys.stdin):
    f = os.path.relpath(os.path.normpath(os.path.join(e["directory"], e["file"])), os.environ["SHUTTLE_SRC"])
    print(f + "\t" + e["command"])' > $DB.tmp && mv $DB.tmp $DB
  fi
  rmdir /tmp/sh-compdb.lock
fi

slot=""
acquire() {
  while true; do
    for i in $(seq 1 $SLOTS); do
      d=/tmp/sh-synchk.slot$i
      if mkdir $d 2>/dev/null; then echo $$ > $d/pid; slot=$d; return; fi
      p=$(cat $d/pid 2>/dev/null); [[ -n $p ]] && ! kill -0 $p 2>/dev/null && rm -rf $d
    done
    sleep 1
  done
}
release() { [[ -n $slot ]] && rm -rf $slot; slot=""; }
trap release EXIT INT TERM

for s in "$@"; do
  cmd=$(awk -F'\t' -v s="$s" '$1==s {print $2; exit}' $DB)
  [[ -z $cmd ]] && { echo "== $s: no command (not in build graph?)"; continue; }
  c=$(echo "$cmd" | sed -E "s# -MMD -MF [^ ]+##; s# -o [^ ]+# -fsyntax-only#; s# -c # #")
  acquire
  echo "== $s"; eval "taskpolicy -c background nice -n 15 $c" 2>&1 | sed -E 's/\x1b\[[0-9;]*m//g' | grep -E "error:" | head -20
  release
done
