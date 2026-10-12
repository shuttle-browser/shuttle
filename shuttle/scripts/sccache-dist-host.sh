#!/bin/bash
# Build-host side of the sccache-dist remote build (Mac client -> Linux build host), run by systemd.
#
#   scripts/sccache-dist-host.sh install    # once, needs the sudo password: copies the units,
#                                             # enables + starts everything
#   scripts/sccache-dist-host.sh install-sudoers  # once (sudo): passwordless start/stop/restart
#                                             # of exactly these 3 units (config/sccache/systemd/)
#   scripts/sccache-dist-host.sh status     # unit states, listeners, scheduler counters
#   scripts/sccache-dist-host.sh start|stop|restart [scheduler|1|2]   # default: all
#   scripts/sccache-dist-host.sh logs [unit-suffix]                   # journalctl -f
#   scripts/sccache-dist-host.sh uninstall  # stop, disable, remove units + sudoers (sudo)
#
# Units (config/sccache/systemd/): sccache-dist-scheduler.service (User=$SHUTTLE_BUILD_USER, :10600),
# sccache-dist-server@1|2.service (root, :10501/:10502, state /var/lib/sccache-dist/s1|s2).
# They are system units, independent of any terminal session: closing it leaves them up,
# and Restart=always brings them back after a crash or a late host IP.
# start/stop/restart need no password once the sudoers drop-in is installed.
# Configs ~/sccache/conf/{scheduler,server1,server2}.conf (mode 600, secrets), binaries ~/sccache/bin.
set -euo pipefail
REPO=$(cd "$(dirname "$0")/.." && pwd)
SRC=$REPO/config/sccache/systemd
. "$(dirname "$0")/lib/paths.sh"
IP=${SHUTTLE_BUILD_HOST_IP:?set SHUTTLE_BUILD_HOST_IP (scheduler address) in ~/.shuttle/env}
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
tmpl() { sed -e "s|@USER@|$(id -un)|g" -e "s|@HOME@|$HOME|g" -e "s|@IP@|$IP|g" "$1"; }   # fill unit/sudoers templates
ALL=(sccache-dist-scheduler.service sccache-dist-server@1.service sccache-dist-server@2.service)

units() {  # map args (scheduler|1|2, none = all) to unit names
  [ $# -eq 0 ] && { printf '%s\n' "${ALL[@]}"; return; }
  for a; do case $a in
    scheduler) echo sccache-dist-scheduler.service ;;
    1|2) echo "sccache-dist-server@$a.service" ;;
    *) echo "unknown unit: $a (scheduler|1|2)" >&2; exit 2 ;;
  esac; done
}

status() {  # exit 1 if a unit is not active; LAST line is always "scheduler: ..." (stable one-liner)
  local rc=0 st line
  for u in "${ALL[@]}"; do
    st=$(systemctl is-active "$u" 2>/dev/null || true); [ "$st" = active ] || rc=1
    printf '%-34s %s\n' "$u" "$st"
  done
  ss -ltn | grep -E "$IP:(10600|1050[0-9])\b" | awk '{print "listening", $4}' || echo "no listeners"
  # bincode SchedulerStatus: u64 num_servers, u64 num_cpus, u64 in_progress (little endian)
  line=$(curl -s -m 5 "http://$IP:10600/api/v1/scheduler/status" | od -An -tu8 -w24 |
    awk 'NF==3{printf "%s servers, %s cpus, %s in progress", $1, $2, $3}')
  echo "scheduler: ${line:-unreachable}"
  [ -n "$line" ] || rc=1
  return $rc
}

case "${1:-}" in
  install-sudoers)
    tmpl "$SRC/sccache-dist.sudoers" > "$TMP/sccache-dist.sudoers"
    /usr/sbin/visudo -cf "$TMP/sccache-dist.sudoers"
    sudo install -m 0440 "$TMP/sccache-dist.sudoers" /etc/sudoers.d/sccache-dist
    sudo /usr/sbin/visudo -c >/dev/null && echo "sudoers drop-in installed: /etc/sudoers.d/sccache-dist" ;;
  install)
    if pgrep -x sccache-dist >/dev/null && ! systemctl is-active -q sccache-dist-scheduler.service; then
      echo "sccache-dist runs outside systemd (old manual shells?): stop it first" >&2; exit 1
    fi
    for u in sccache-dist-scheduler.service sccache-dist-server@.service; do tmpl "$SRC/$u" > "$TMP/$u"; done
    sudo install -m 0644 "$TMP/sccache-dist-scheduler.service" "$TMP/sccache-dist-server@.service" /etc/systemd/system/
    sudo install -d -m 0755 /var/lib/sccache-dist
    sudo systemctl daemon-reload
    sudo systemctl enable --now "${ALL[@]}"
    sleep 3; status ;;
  uninstall)
    sudo systemctl disable --now "${ALL[@]}" || true
    sudo rm -f /etc/systemd/system/sccache-dist-scheduler.service /etc/systemd/system/sccache-dist-server@.service \
      /etc/sudoers.d/sccache-dist
    sudo systemctl daemon-reload ;;
  start|stop|restart)
    verb=$1; shift
    list=$(units "$@")
    [ "$verb" = stop ] && list=$(echo "$list" | tac)   # servers before the scheduler
    for u in $list; do sudo /usr/bin/systemctl "$verb" "$u"; done   # no prompt once install-sudoers ran
    sleep 2; status ;;
  status) status ;;
  logs)
    shift; if [ $# -gt 0 ]; then journalctl -f -o short-iso -u "$(units "$1")"
    else journalctl -f -o short-iso -u 'sccache-dist-*'; fi ;;
  *) sed -n '2,19p' "$0"; exit 2 ;;
esac
