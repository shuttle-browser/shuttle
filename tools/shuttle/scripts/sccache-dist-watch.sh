#!/bin/bash
# Quiet watcher for the build host's sccache-dist units, meant to run under a Monitor (every stdout
# line = one notification). Usage: scripts/sccache-dist-watch.sh [minutes=2] [idle_seconds=60]
#
# Prints:
#   - one start line;
#   - every N minutes, ONLY while jobs run: jobs done/failed in the interval, in progress,
#     running clang, CPU%, CPU/IO pressure;
#   - immediately, once per NEW kind of problem: job failure reason (job ids, hashes, numbers
#     and paths stripped for dedup), unit crash/restart/stop, scheduler/server WARN/ERROR;
#   - while active, "HOT: CPU package NC" once per 10 min if it reaches HOT (env, default 90) C
#     (LibreHardwareMonitor via scripts/buildhost-temp.sh; interval lines also carry pkg=NC);
#   - when activity stops (no job events for idle_seconds and 0 in progress): one summary line
#     "idle after X jobs, Y failed (duration)", then quiet again until the next build.
# Sources: journalctl -f -u 'sccache-dist-*', the scheduler status endpoint, /proc.
set -uo pipefail
MIN=${1:-2}; IDLE=${2:-60}
. "$(dirname "$0")/lib/paths.sh"
IP=${SHUTTLE_BUILD_HOST_IP:?set SHUTTLE_BUILD_HOST_IP in ~/.shuttle/env}
EVERY=$((MIN * 60))

in_progress() {
  curl -s -m 3 "http://$IP:10600/api/v1/scheduler/status" | od -An -tu8 -w24 | awk '{print $3+0}'
}
cpu_sample() { awk '/^cpu /{print $2+$3+$4+$5+$6+$7+$8+$9, $5+$6}' /proc/stat; }
psi() { awk '/^some/{split($2,a,"="); print a[2]}' "/proc/pressure/$1"; }
norm() {  # dedup key for an error message
  sed -E 's/^.*(WARN|ERROR) +//; s/Res [0-9]+ error: //; s/[0-9a-f]{16,}(-[0-9]+)?/H/g;
          s#/[^ `'"'"'"]*#P#g; s/[0-9]+/N/g' <<<"$1" | cut -c1-220
}

declare -A seen
active=0 start=0 last_evt=0 tot_done=0 tot_fail=0 int_done=0 int_fail=0 next_tick=0
read -r c_tot c_idle < <(cpu_sample)
echo "sccache-dist-watch: watching (report every ${MIN} min while active, idle after ${IDLE}s); $(in_progress) in progress"

exec 3< <(journalctl -f -n 0 -o cat -u 'sccache-dist-*' 2>&1)
while true; do
  now=$(date +%s)
  if IFS= read -r -t 2 line <&3; then
    case $line in
      *"updated state to Complete"*) ((tot_done++, int_done++)); last_evt=$now ;;
      *"updated state to"*) last_evt=$now ;;
      *" WARN "*|*" ERROR "*|*"error:"*)
        case $line in *"Res "*" error"*) ((tot_fail++, int_fail++)); last_evt=$now ;; esac
        k=$(norm "$line")
        if [ -z "${seen[$k]:-}" ]; then seen[$k]=1; echo "NEW ERROR: $(cut -c1-400 <<<"$line")"; fi ;;
      *"Main process exited"*|*"Failed with result"*|*"Scheduled restart job"*|*"Stopped "*|*"Started "*|*"Stopping "*)
        # unit events repeat legitimately (later crash/restart): same text at most once per 10 min
        k="unit:$(norm "$line")"
        if [ $((now - ${seen[$k]:-0})) -ge 600 ]; then seen[$k]=$now; echo "$(date +%T) UNIT: $line"; fi ;;
    esac
    if [ $active -eq 0 ] && [ $last_evt -eq $now ]; then
      active=1 start=$now next_tick=$((now + EVERY)) tot_done=0 tot_fail=0 int_done=0 int_fail=0
      read -r c_tot c_idle < <(cpu_sample)
    fi
  fi
  now=$(date +%s)
  # heat alert (LibreHardwareMonitor on the Windows host): CPU package >= HOT C, checked every
  # 30 s while active, reported at most once per 10 min
  if [ $active -eq 1 ] && [ $now -ge ${next_heat:-0} ]; then
    next_heat=$((now + 30)); t=$("$(dirname "$0")"/buildhost-temp.sh --short); t=${t#pkg=}; t=${t%C}
    if [[ $t =~ ^[0-9]+$ ]] && [ "$t" -ge "${HOT:-90}" ] && [ $((now - ${last_hot:-0})) -ge 600 ]; then
      last_hot=$now; echo "$(date +%T) HOT: CPU package ${t}C (>= ${HOT:-90}C; TjMax 100C)"
    fi
  fi
  if [ $active -eq 1 ] && [ $now -ge $next_tick ]; then
    read -r t i < <(cpu_sample); dt=$((t - c_tot)); cpu=$(( dt > 0 ? 100 * (dt - (i - c_idle)) / dt : 0 ))
    c_tot=$t c_idle=$i
    echo "$(date +%T) active: +${int_done} done, +${int_fail} failed in ${MIN}m (total ${tot_done}/${tot_fail}), $(in_progress) in progress, clang=$(pgrep -xc 'clang(\+\+)?') cpu=${cpu}% cpu_psi=$(psi cpu) io_psi=$(psi io) $("$(dirname "$0")"/buildhost-temp.sh --short)"
    int_done=0 int_fail=0 next_tick=$((now + EVERY))
  fi
  if [ $active -eq 1 ] && [ $((now - last_evt)) -ge $IDLE ] && [ "$(in_progress)" = 0 ]; then
    d=$((last_evt - start))
    echo "$(date +%T) idle after ${tot_done} jobs, ${tot_fail} failed ($((d / 60))m$((d % 60))s of activity)"
    active=0
  fi
done
