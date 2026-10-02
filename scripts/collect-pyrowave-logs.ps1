[CmdletBinding()]
param([string]$Destination = (Join-Path $PSScriptRoot ('logs-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))))
$ErrorActionPreference = 'Stop'
New-Item -ItemType Directory -Force -Path $Destination | Out-Null
$logs = @(Get-ChildItem -LiteralPath $PSScriptRoot -Filter 'Asteria*.log' -File -ErrorAction SilentlyContinue)
if (!$logs.Count) { $logs = @(Get-ChildItem -LiteralPath $PSScriptRoot -Filter '*.log' -File -ErrorAction SilentlyContinue) }
foreach ($log in $logs) { Copy-Item -LiteralPath $log.FullName -Destination $Destination }
if (!$logs.Count) { Write-Warning 'No portable logs found. Use Asteria menu > View logs and copy the current log here.' }
@'
Record: host version/commit, client artifact/run/hash, Windows/driver version,
GPU adapter, resolution/FPS/bitrate, first video/audio/input result, disconnect,
reconnect, standard codec retry and resize/fullscreen. Attach the Asteria log,
and relevant host log excerpts. Do not include pairing credentials/private keys.
PyroWave performance retest:
Presentation mode / fallback reason and compute/fragment path:
Incoming / decoded / rendered FPS:
Host processing latency min / max / average (ms):
Network drop percentage / count:
Jitter or presentation drop percentage / count:
RTT / variance (ms):
Decode API time (ms) and GPU submission / CPU decode+readback mode:
Frame queue delay (ms):
Render including V-sync latency (ms):
Bitrate (Mbps), reassembly / decoder wait / frame preparation (ms):
ARM64 2560x1440/120 comparison to ~93.3 FPS, ~12.45 ms old decode pipeline,
~1.24 ms render (decode definitions differ; do not infer GPU execution time):
'@ | Set-Content (Join-Path $Destination 'OWNER-RESULT.txt') -Encoding utf8
Write-Output "Collected logs in $Destination"
