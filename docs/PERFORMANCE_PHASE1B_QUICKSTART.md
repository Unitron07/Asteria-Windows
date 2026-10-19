# Phase 1B: simple 250/500 Mbps pilot

**Exploratory is the default. No metadata editing is required.** Missing workload,
network, power, driver or other extended context produces warnings. Results are
always labeled **EXPLORATORY / NOT CAUSALLY QUALIFIED**, even when all context is
available. This pilot cannot establish a profiling benefit or a causal bottleneck.

Use the updated tooling with Windows Python 3.10+ and the existing qualified
portable ARM64 package, source `00b805e19866155483e6bd27dcd775355087a0ed`.
No new application build is needed. The two supplied configs are ready to use:

```json
{
  "sourceRevision": "00b805e19866155483e6bd27dcd775355087a0ed",
  "profile": "1440p120",
  "context": { "bitrateKbps": 250000, "vsync": true }
}
```

The 500 Mbps file changes only `bitrateKbps` to `500000`. Defaults are profiling
ON, ARM64, PyroWave, native Vulkan fragment, 2560x1440@120, V-sync ON, 10 s warmup
and 60 s capture. The config specifies expected settings; it does not apply them
to Asteria. If using V-sync OFF, change the one `vsync` value in **both** configs.
Use a separate experiment if selecting a different package/source.

## Setup once

In Asteria select PyroWave SDR 8-bit 4:2:0, 1440p120 and V-sync ON. Keep the same
host scene/replay, codec/path, display, route and power setup between streams.
Close Asteria/Moonlight normally before each runner launch. Run from the extracted
toolkit or repository; set `$exe` to your existing qualified package location:

```powershell
$exe = 'C:\Benchmarks\Packages\phase1a-00b805\Asteria.exe'
$pilot = 'C:\Benchmarks\Phase1B\bitrate-exploratory-v1'
if (Test-Path -LiteralPath $pilot) { throw 'Use a NEW directory; preserve old evidence' }
New-Item -ItemType Directory -Path $pilot -ErrorAction Stop | Out-Null
Copy-Item .\docs\benchmark-1440p120-250mbps.template.json "$pilot\250.json"
Copy-Item .\docs\benchmark-1440p120-500mbps.template.json "$pilot\500.json"
python .\scripts\performance.py benchmark-ready "$pilot\250.json" "$pilot\500.json"
if ($LASTEXITCODE) { throw 'Correct essential settings before launching' }
```

`READY` with a context warning is success. No workload hash, host specification,
network inventory, temperature, brightness or driver entry is needed to continue.

## Run the short pilot

Run **one command at a time**. Before each, manually save the stated target bitrate
in Asteria, confirm the fixed settings and close Asteria normally. Follow the
runner prompts: launch, connect one stream, confirm stable video, wait, disconnect,
wait for cleanup and close normally. Wait at least 60 s between commands; single
run sessions cannot enforce cooldown between separate invocations.

```powershell
# A1: save 250 Mbps (250000 Kbps) in Asteria, then close it.
python .\scripts\benchmark_runner.py --exe $exe --config "$pilot\250.json" --session "$pilot\01-250" --single-run
# B1: wait 60 s, save 500 Mbps (500000 Kbps), then close it.
python .\scripts\benchmark_runner.py --exe $exe --config "$pilot\500.json" --session "$pilot\02-500" --single-run
# B2: wait 60 s; keep 500 Mbps, same workload/settings.
python .\scripts\benchmark_runner.py --exe $exe --config "$pilot\500.json" --session "$pilot\03-500" --single-run
# A2: wait 60 s, save 250 Mbps again, then close it.
python .\scripts\benchmark_runner.py --exe $exe --config "$pilot\250.json" --session "$pilot\04-250" --single-run
```

This ABBA pilot has two independent streams per target. Keep every attempt. On a
failure, stop and inspect; after normal client close retry the same command with
`--resume`. Do not edit failed manifests or pool these pilots with full repeats.

## Return evidence

Return the entire `$pilot` directory, including configs, session plans, started
and finished manifests, console/portable logs, capture JSON and summaries. A short
note about the host scene, Wi-Fi/Ethernet route and AC/battery state is useful but
optional. Unknowns remain unknown; bad pacing or high drops are retained.

```powershell
$files = @(Get-ChildItem -LiteralPath $pilot -Recurse -Filter manifest.json | Sort-Object FullName | ForEach-Object FullName)
python .\scripts\performance.py benchmark-validate @files
python .\scripts\performance.py benchmark-report @files --output "$pilot\pilot-report.md"
```

The runner automatically verifies source, package/runtime hashes, PE architecture
and executable SHA-256. It records available Windows OS, PE version, CIM GPU/driver,
display-adapter, battery and brightness observations in `automaticEvidence`.
Selected GPU/backend/path come from the real shutdown summary and must agree with
the capture. CIM observations alone do not establish the selected GPU, active
display or host/network conditions. Unavailable discovery is nonfatal.

Evidence checks remain fatal in both modes: wrong source/binary/runtime pins,
wrong backend/path/bitrate/stream settings, missing/multiple real shutdowns,
partial/overflowed capture, nonzero exit, fatal/timeline errors or failed cleanup.
Reported rates are requested targets, not measured payload. Keep lifetime drop
fractions separate from capture-window metrics; do not pool p99 values.

For a rigorous experiment use the [optional controlled checklist and ten-run
exploratory evidence record](PERFORMANCE_PHASE1B_BITRATE_PILOT.md). Explicitly add
`--qualification controlled` to `benchmark-ready` **and** runner commands, or set
`"qualification": "controlled"` in the config. Only that mode requires detailed
context. Previous full manifests remain readable and are never rewritten or
retroactively qualified. **Stop after the pilot for review. PR #46 stays draft;
no merge, release or production streaming changes.**
