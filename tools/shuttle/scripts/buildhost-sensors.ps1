# Build-host hardware sensors via LibreHardwareMonitorLib.dll (no GUI, no web server, no listener).
# Run elevated (PowerShell started from WSL is elevated when the Windows host runs WSL as admin); called by scripts/buildhost-temp.sh.
# Output: one "identifier|value" line per temperature/power/fan sensor, e.g.
#   /intelcpu/0/temperature/8|41   (CPU Package)
$ErrorActionPreference = 'Stop'
$dir = Join-Path $env:LOCALAPPDATA 'Microsoft\WinGet\Packages\LibreHardwareMonitor.LibreHardwareMonitor_Microsoft.Winget.Source_8wekyb3d8bbwe'
Add-Type -Path (Join-Path $dir 'LibreHardwareMonitorLib.dll')
$c = New-Object LibreHardwareMonitor.Hardware.Computer
$c.IsCpuEnabled = $true; $c.IsMotherboardEnabled = $true; $c.IsStorageEnabled = $true
$c.Open()
try {
  foreach ($h in $c.Hardware) {
    $h.Update(); foreach ($s in $h.SubHardware) { $s.Update() }
    foreach ($s in @($h.Sensors) + @($h.SubHardware | ForEach-Object { $_.Sensors })) {
      if ($s.SensorType -in 'Temperature','Power','Fan' -and $null -ne $s.Value) {
        '{0}|{1}' -f $s.Identifier, ([math]::Round([double]$s.Value, 1)).ToString([Globalization.CultureInfo]::InvariantCulture)
      }
    }
  }
} finally { $c.Close() }
