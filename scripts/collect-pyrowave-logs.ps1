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
ARM64 2560x1440/120 Hz: preferred/GPU fragment, fallback compute expected.
Compare latest fragment ~4.31 ms decode/readback to earlier compute ~3.47 ms.
Latest fragment: ~0.97 ms assembly, ~1.76 ms preparation, ~2.19 ms decoder wait,
~1.50 ms render, ~101.7 FPS; native iDWT fragment ~1.98 and Dequant ~0.83 ms/frame:
Conditions differed; do not require exact targets as gameplay/network load vary.
Do not infer performance improvement from CI or achieved FPS alone.
'@ | Set-Content (Join-Path $Destination 'OWNER-RESULT.txt') -Encoding utf8
Write-Output "Collected logs in $Destination"
