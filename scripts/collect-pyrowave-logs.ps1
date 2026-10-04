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
GPU adapter / device-preferred path / CPU fallback path / presentation failure reason:
Incoming / decoded / rendered FPS:
Host processing latency min / max / average (ms):
Network drop percentage / count:
Jitter or presentation drop percentage / count:
RTT / variance (ms):
Decode API time (ms) and GPU submission / CPU decode+readback mode:
Frame queue delay (ms):
Render including V-sync latency (ms):
Native PyroWave GPU timestamp output (ten-second report and shutdown):
Bitrate (Mbps), reassembly / decoder wait / frame assembly / parser preparation (ms):
ARM64 2560x1440/120 Hz comparison to ~3.47 ms decode/readback,
~2.64 ms combined assembly+preparation, ~0.55 ms decoder wait,
~0.08 ms frame queue and ~1.20 ms render; workload was ~109.7 FPS:
Do not infer performance improvement from CI or achieved FPS alone.
'@ | Set-Content (Join-Path $Destination 'OWNER-RESULT.txt') -Encoding utf8
Write-Output "Collected logs in $Destination"
