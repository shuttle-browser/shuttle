#!/bin/bash
# One-line build-host (WSL2) hardware summary, read on demand through LibreHardwareMonitorLib.dll
# (scripts/buildhost-sensors.ps1 via the Windows host's elevated PowerShell; ~1 s per call).
# No GUI, no web server, no network listener. LibreHardwareMonitor 0.9.6 is installed with
# winget on the Windows host; its GUI's web server stays OFF (it binds all interfaces without
# auth), and a firewall rule blocking inbound TCP 8085 is a
# backstop in case it is ever switched on.
#   scripts/buildhost-temp.sh          -> pkg=41C core_max=41C pkg_w=16.6W cpu_fan=497rpm ssd=36C
#   scripts/buildhost-temp.sh --short  -> pkg=41C (for watcher/sampler lines)
# Prints "temp=n/a" (exit 1) when the sensors can't be read.
PS=/mnt/c/Windows/System32/WindowsPowerShell/v1.0/powershell.exe
REPO=$(cd "$(dirname "$0")/.." && pwd)
WIN_PS1=$(wslpath -w "$REPO/scripts/buildhost-sensors.ps1")
out=$(cd /mnt/c && timeout 20 "$PS" -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "$WIN_PS1" 2>/dev/null | tr -d '\r')
awk -F'|' -v short="${1:-}" '
  { v[$1] = $2 }
  function f(name, id, unit, nd) { return (id in v) ? sprintf("%s=%." nd "f%s", name, v[id], unit) : "" }
  END {
    s = f("pkg", "/intelcpu/0/temperature/8", "C", 0)
    if (short != "--short") {
      n = split(f("core_max", "/intelcpu/0/temperature/0", "C", 0) " " f("pkg_w", "/intelcpu/0/power/0", "W", 1) " " \
                f("cpu_fan", "/lpc/nct6687d/0/fan/0", "rpm", 0) " " f("ssd", "/nvme/0/temperature/0", "C", 0), a, " ")
      for (i = 1; i <= n; i++) if (a[i] != "") s = s " " a[i]
    }
    if (s == "") { print "temp=n/a"; exit 1 }
    print s
  }' <<<"$out"
