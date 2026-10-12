#!/bin/zsh
# Usage: scripts/build-progress.sh [Release|Dev] [step-percent]
# Emits a line every <step-percent> (default 5) with done/total, the rate over
# the last ~15 min and an ETA; also emits on the first FAILED, when the build
# exits, and when the build process disappears without finishing.
. "$(dirname "$0")/lib/paths.sh"
dir="${1:-Release}"; step="${2:-5}"
log="$SHUTTLE_LOGS/build-$dir.log"
last=-1; gone=0; failed=""
typeset -a ts ds   # sample history: epoch seconds / done count
while true; do
  if grep -q "BUILD EXIT" "$log" 2>/dev/null; then grep "BUILD EXIT" "$log"; exit 0; fi
  if ! pgrep -f "autoninja -C out/$dir" >/dev/null; then
    gone=$((gone+1))
    (( gone >= 4 )) && { echo "$dir build: process gone without BUILD EXIT"; tail -c 2000 "$log" | tr '\r' '\n' | tail -5; exit 1; }
  else gone=0; fi
  if [[ -z "$failed" ]] && grep -q "FAILED:" "$log" 2>/dev/null; then
    failed=1; echo "$dir build: step FAILED"; grep -m1 -A4 "FAILED:" "$log"
  fi
  cur=$(tail -c 20000 "$log" 2>/dev/null | tr '\r' '\n' | grep -oE '\[ *[0-9]+/[0-9]+' | tail -1 | tr -d '[ ')
  if [[ -n "$cur" ]]; then
    d=${cur%/*}; t=${cur#*/}; now=$(date +%s)
    ts+=($now); ds+=($d)
    # drop samples older than 15 min (keep at least two)
    while (( ${#ts} > 2 && now - ts[1] > 900 )); do shift ts; shift ds; done
    pct=$(( d * 100 / t )); bucket=$(( pct / step * step ))
    if (( bucket > last )); then
      last=$bucket; span=$(( now - ts[1] )); eta="ETA n/a"
      if (( span >= 120 && d > ds[1] )); then
        rate=$(( (d - ds[1]) * 600 / span ))   # steps per 10 min
        secs=$(( (t - d) * 600 / rate ))
        eta="~${rate} steps/10min, ETA ~$((secs/3600))h$(( (secs%3600)/60 ))m (finish ~$(date -r $((now+secs)) +%H:%M))"
      fi
      echo "$dir build: ${pct}% ($d/$t) at $(date +%H:%M), $eta"
    fi
  fi
  sleep 30
done
