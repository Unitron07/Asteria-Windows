# Offline P0-R proof and P0.5 SDL presentation

## P1a live implementation tests and owner artifacts

Run `./scripts/apply-common-c-p1a.ps1` after recursive checkout and before the
CMake commands below. The qmake application build applies this maintained patch
automatically against the checked gitlink. New GPU-free negotiation tests use
the actual RTSP policy; live-frame tests use the production decode-unit assembly
and parser, test arbitrary fragment cuts, malformed/truncated/oversized inputs,
HDR/444/range rejection, recovery and a structurally valid 3,546,016-byte frame
with 1,501 codec packets (larger than both old offline limits). Existing parser,
legacy, loader/export/API, Granite and P0.5 presentation suites are retained.

The optional workflow also builds the full experimental app on x64/native ARM64.
`scripts/build-pyrowave-live.ps1` invokes the established Qt/MSVC build with
`CONFIG+=pyrowave_experimental`, verifies metadata, then stages the verified
portable client plus `pyrowave/` runtime in a separate experimental ZIP. Normal
CI does not build the runtime. No Vulkan/PyroWave startup imports are permitted.
The restricted loader's CRT closure is staged in the runtime directory; all
packaged PE types, notices and hashes are checked. See
[LIVE-OWNER-TEST.md](LIVE-OWNER-TEST.md) for extracting, launching, selection,
logging, manual retry and live qualification criteria. Live owner qualification
remains PENDING; passing CI only establishes build/API/test evidence.


This standalone parser/Windows runtime harness does not connect to a host.
Normal Asteria builds and release packages stay unchanged; Qt/qmake remains the
application toolchain. Experimental app compilation needs
`CONFIG+=pyrowave_experimental`. No Session hook, codec setting or advertisement.

Active codec: Themaister/pyrowave `186f0393b77f7755953b5ecde994bb1cec2e4155`;
bitstream ID `186f0393`; C API 0.6.0. See [the current contract](../../docs/PYROWAVE_VIBEPOLLO.md)
and [source/patch lock](../../scripts/pyrowave/dependencies.json).
**P0-R is COMPLETE** on RTX 4070 Ti x64 and native Surface Pro 11 / Snapdragon
X Plus / Adreno X1-85 ARM64 with this codec/ID/API, both framing modes, three
decoder lifetimes, expected I420 planes and malformed rejection/recovery. See
[the owner hardware record](../../docs/VALIDATION.md#m1b-p0-r-vibepollo-validation).
Old `f6fb84...` evidence remains historical. **P0.5 is COMPLETE** after PR #20
merged and owner visual qualification passed on both named targets. See the
[P0.5 hardware record and limits](../../docs/VALIDATION.md#m1b-p05-offline-sdl-qualification).
P1a live SDR 4:2:0 integration is the next future milestone, not implemented here.

## Dependencies and GPU-free parser tests

Use PowerShell, Git, MSVC x64/ARM64 tools, Windows SDK and CMake >=3.27. The
helper builds separate sources/caches/install/evidence per architecture and
fails exact revision/API/patch hash mismatch. Use a fresh root after a patched
build. ARM64 keeps the isolated Granite portable math patch; `-UnpatchedArm64`
reproduces the known historical compile blockers.

```powershell
./scripts/build-pyrowave-deps.ps1
cmake -S tests/pyrowave -B build/parser-x64 -A x64
cmake --build build/parser-x64 --config Release
ctest --test-dir build/parser-x64 -C Release --output-on-failure
```

Use `-A ARM64`, an ARM64 build directory and native ARM64 execution for its parser
results. Cross-compiling is not native execution. Both parser executables need no
Vulkan/GPU. The main suite tests LE compatibility and full record framing,
count/size/allocation bounds, sequence/context/block invariants, padding,
straddling/unaligned bytes, truncations and 20,000 deterministic mutations.
The legacy suite retains old `PYRW` regression cases through the explicitly
named `parseLegacyOfflineFrame`; it is never selected by host framing detection.

## Runtime and both framing modes

```powershell
$deps = (Resolve-Path build/pyrowave/x64).Path
cmake -S tests/pyrowave -B build/probe-x64 -A x64 -DPYROWAVE_EXPERIMENTAL=ON "-DPYROWAVE_ROOT=$deps/install" "-DVULKAN_HEADERS=$deps/source/Granite/third_party/khronos/vulkan-headers/include"
cmake --build build/probe-x64 --config Release
ctest --test-dir build/probe-x64 -C Release --output-on-failure
./build/probe-x64/Release/pyrowave-offline-proof.exe --load "$deps/install/bin"
./build/probe-x64/Release/pyrowave-offline-proof.exe --roundtrip "$deps/install/bin" "$deps/evidence/roundtrip"
# Individual modes:
./build/probe-x64/Release/pyrowave-offline-proof.exe --roundtrip-compatibility "$deps/install/bin"
./build/probe-x64/Release/pyrowave-offline-proof.exe --roundtrip-records "$deps/install/bin"
```

Substitute ARM64 consistently. The wrapper loads an absolute canonical DLL path
via `LoadLibraryExW`, searches dependencies only in its directory/System32,
checks API 0.6.0 and required exports, and preloads the native System32 Vulkan
loader before device creation. No codec/Vulkan startup import is linked.
CTest exercises absent/wrong-version/missing-export runtimes, relative paths
and repeated rejected loads. `--load` tests real DLL unload/reload without a GPU.
The printed bitstream ID is the expected build metadata, not source identity
extracted from the API. Use the dependency artifact inventory to bind the DLL.

`--roundtrip` generates the deterministic 1920x1080 8-bit SDR 4:2:0 luma ramp
(16..235), neutral chroma (128), with an 800,000-byte budget and Vibepollo's
1024-byte codec packetizer target. It wraps the same codec packets as LE
compatibility framing and complete record framing (including padding). Both
formats must yield identical I420 planes and mean absolute sample error <=8,
through three decoder lifetimes. Each malformed/truncated frame must reject
with empty output and recover on the following complete frame. The runtime
clears the decoder before and after every frame and requires full readiness.

Optional output files include both generated `.bin` fixtures, both `.i420`
outputs and codec metadata. Compression may vary by GPU/driver; fixtures are
local roundtrip evidence, not captured host data or network interoperability.
Plane sizes are 2,073,600 / 518,400 / 518,400 bytes. No SDL window, display color,
pacing, production latency, 4:4:4 or HDR support is qualified here.

## Owner rerun from CI artifacts

From an extracted target artifact, use the matching runtime location. ARM64's
patched root is used when CI reproduced and repaired the known Granite blocker:

```powershell
.\probe-x64\Release\pyrowave-offline-proof.exe --load ".\pyrowave\x64\install\bin"
.\probe-x64\Release\pyrowave-offline-proof.exe --roundtrip ".\pyrowave\x64\install\bin" ".\roundtrip-output"
.\probe-arm64\Release\pyrowave-offline-proof.exe --load ".\pyrowave-patched\arm64\install\bin"
.\probe-arm64\Release\pyrowave-offline-proof.exe --roundtrip ".\pyrowave-patched\arm64\install\bin" ".\roundtrip-output"
Get-ChildItem .\roundtrip-output -File | Get-FileHash -Algorithm SHA256
```

The CLI resolves paths to absolute paths. Save all logs, runtime/output hashes,
Windows build and display-driver version. Confirm native ARM64 process execution.
Unavailable Vulkan is exit 77 in CI, not a pass on hardware. Other decode failures
remain failures. The owner has completed these new-codec P0-R commands on both
targets. The completed P0.5 presentation qualification is reproducible below.

## qmake compile/link

```text
qmake tests/pyrowave/offline.pro CONFIG+=release PYROWAVE_ROOT=C:/absolute/target/install VULKAN_HEADERS=C:/absolute/pinned/Vulkan-Headers/include
nmake
```

Use separate target Qt/MSVC prompts/build directories. The optional CI invokes
`scripts/test-pyrowave-qmake.ps1` and records qmake/import evidence without changing
normal app packaging. Parser policy remains 850,000 total frame bytes and 1,024
compatibility packets; live MTU/FEC/overhead limits require a future review.

## P0.5 offline SDL presentation

The harness is isolated from Session and the normal application build. Initial
contract: **1920x1080 SDR, 8-bit I420, BT.709 limited range, centered 4:2:0**.
4:4:4 and HDR are excluded. P0-R remains hardware-qualified; **P0.5 is COMPLETE**
on x64 RTX 4070 Ti and native Surface Pro 11 / Snapdragon X Plus / Adreno X1-85
ARM64. Owner inspection passed all five raw/compatibility/record patterns,
fit/resize, nearest/linear, fullscreen/maximize/restore and renderer/texture
recreation. Ten lifecycles, both SDL reset recovery paths, synthetic full
recreation and 30-second 60 FPS pacing sanity loops passed. Exact chroma siting,
physical GPU loss, full pacing/production latency and live host/network
interoperability remain unqualified. Commands below remain reusable for reruns.

Optional CI artifacts include `presentation-x64/` or `presentation-arm64/` with
the executable, matching SDL2.dll/SDL3.dll, `runtime/bin/`, source notices and
`RUN-ME.txt`. Use the matching native architecture with the MSVC runtime installed,
as for the existing P0-R probe. Codec modes additionally need the native Vulkan
loader and a supported driver. Run from that presentation directory:

```powershell
.\pyrowave-offline-proof.exe --present-raw-i420 - .\evidence-raw
.\pyrowave-offline-proof.exe --present-compatibility .\runtime\bin .\evidence-compat
.\pyrowave-offline-proof.exe --present-records .\runtime\bin .\evidence-records
.\pyrowave-offline-proof.exe --present-recreate-test .\runtime\bin .\evidence-recreate
.\pyrowave-offline-proof.exe --present-loop .\runtime\bin .\evidence-loop --seconds 30
```

Inspection windows stay open until Esc. Loop defaults to ten seconds. Options
`--pattern 1..5` and `--seconds 1..3600` select initial pattern and duration.
Raw mode accepts `-` without loading PyroWave/Vulkan; toggling to a codec source
with T requires a real DLL directory (supply `./runtime/bin` even in raw mode if
you want this comparison). Encoded modes always decode the *same encoded packet
stream* in compatibility and record containers and require byte-identical I420.
They report MAE/max sample error versus raw; sharp-edge compression differences
are observations, not the old gray-ramp <=8 gate. Both formats are offline fixtures.

| Key | Action |
| --- | --- |
| 1 / 2 / 3 / 4 / 5 | Range / BT.709 bars / chroma / geometry / gradient |
| T | Cycle raw I420 / compatibility / records for the selected pattern |
| F | Desktop fullscreen toggle |
| R | Destroy/recreate renderer+texture, unload/reload runtime and recreate decoder |
| S | Explicit nearest/linear SDL texture scaling; initial nearest |
| N | Request native 1920x1080 window |
| W | Request 960x540, 2560x1440, then 1000x1000 window |
| Esc | Clean shutdown |

Resize with the mouse, maximize/restore repeatedly and toggle fullscreen. Windows
may constrain requested window dimensions to the desktop; inspect logged window
and output pixels. Source-to-output fit always preserves aspect ratio and adds
black bars; a minimized zero-size output is skipped. Mixed DPI can make window
coordinates differ from output pixels. Native nearest inspection needs a
**1920x1080 renderer output**, not merely a requested window size.

### Expected visual results and color controls

- Range: vertical strips contain Y **0,8,15,16,17,32,64,128,192,234,235,236,247,255**,
  U/V 128. Under limited-range conversion, <=16 clip black and >=235 clip white;
  17 and 234 are near the endpoints. Out-of-range strips are clipping references,
  not full-range content. Compare direct raw against codec output.
- Bars, left to right: **white, yellow, cyan, green, magenta, red, blue, black**.
  Rounded BT.709 limited Y/U/V triplets are `(235,128,128)`, `(219,16,138)`,
  `(188,154,16)`, `(173,42,26)`, `(78,214,230)`, `(63,102,240)`, `(32,240,118)`,
  `(16,128,128)`. Look for U/V swaps, wrong hue, range washout or clipping.
- Chroma: upper broad contrasting regions change on even 2x2 luma boundaries,
  with a bright even-coordinate grid. Lower areas alternate one and two chroma
  samples (2x2/4x4 luma quads). Each sample is authored at the center of its 2x2
  quad. At native nearest, inspect symmetric boundaries relative to the grid;
  compare raw/codec and both formats. Linear filtering should soften transitions.
  Visual evidence may reveal offsets; this test does not measure fractional
  chroma phase or prove a backend preserves exact sample-center coordinates.
- Geometry: upper-left 1px and lower-left 2px checker regions, 120px grid, center crosshair,
  border/corners and a centered 400x400 square. Square stays square, all corners
  remain visible and square windows add bars. Fractional scaling can alias fine
  checks; nearest has sharp steps, linear smooths. Codec ringing is distinguishable
  by comparing raw. Dense checks are localized to keep the fixture within the
  unchanged frame/packet caps. Gradient uses Y 16..235 with neutral chroma.

The existing verified v15 archive supplies **SDL2 API 2.32.70 through sdl2-compat**,
backed by **SDL3 3.4.16** (ARM64 inventory in DEPENDENCIES_WINDOWS.md). No new major
dependency is introduced. Probe reports compiled/runtime SDL2 version and revision,
renderer name/flags, advertised IYUV support and actual texture, source/window/output
dimensions and scaling. Backend advertisement can omit IYUV even when SDL emulates
it; successful texture creation/query/upload is reported separately.

`SDL_SetYUVConversionMode(SDL_YUV_CONVERSION_BT709)` is set **before every texture
creation**. The [pinned compatibility source](https://github.com/libsdl-org/sdl2-compat/blob/a53b6ad90ecd2d0ccfe01d5cfd2059793acf8c12/src/sdl2_compat.c)
maps this to `SDL_COLORSPACE_BT709_LIMITED` in `GetColorspaceForFormatAndSize` and
sets the SDL3 texture colorspace property in `SDL_CreateTexture`. This matches
the explicit conversion used by Asteria's existing SDL renderer. The SDL2 API has
no independent chroma-siting control. The [pinned SDL3 colorspace definition](https://github.com/libsdl-org/SDL/blob/fa2c02bb6e21974a89ea9824bc53c9932abe5f9c/include/SDL3/SDL_pixels.h)
defines `SDL_COLORSPACE_BT709_LIMITED` with **LEFT chroma-location metadata**, while
Vibepollo's source pattern is **CENTER**. This is a concrete metadata mismatch;
the probe logs it and does not compensate or claim centered alignment is preserved.
Owner inspection found no visible chroma anomaly on either tested device, but
exact CENTER-versus-LEFT sampling behavior remains **formally unqualified**;
visual agreement does not mathematically prove exact center-sample preservation.
A visible offset on a future backend should be recorded as a qualification failure
requiring a presentation-path decision. No display calibration or fractional
chroma-phase measurement was supplied. Full-range BT.709 and HDR are not qualified. See [SDL conversion API](https://wiki.libsdl.org/SDL2/SDL_SetYUVConversionMode).

### Lifecycle, reset, timing and evidence limits

`--present-recreate-test` runs **ten** cycles across all five patterns and both
codec formats: window/renderer/texture create, runtime load/device/decoder create,
encode/decode/upload/present, synthetic target/device-reset recovery and full
renderer+decoder+runtime recreation, destruction and SDL shutdown/reinit. It
exercises API lifetime/recovery without proving leak freedom. `--present-smoke`
uses raw patterns for hosted window/lifecycle checks. `--hidden` hides only these
test windows; `--software` explicitly requests software rendering for diagnostics,
and must not be recorded as GPU renderer qualification.

Actual `SDL_RENDER_DEVICE_RESET` and `SDL_RENDER_TARGETS_RESET` events are logged,
then the texture is recreated and the retained CPU frame reuploaded. Full R
recreation replaces renderer/decoder/runtime too. [SDL reset events](https://wiki.libsdl.org/SDL2/SDL_EventType)
do not provide a portable real GPU loss injector. Synthetic calls exercise the
same recovery function; **true device loss remains manual/unproven**, and a
failed recovery exits nonzero. The pinned compatibility layer translates target
and device reset events, but drops SDL3's separate `SDL_EVENT_RENDER_DEVICE_LOST`
event, which has no SDL2 equivalent. Checked render/upload failures still exit
nonzero; the SDL2 API cannot provide complete fatal-loss observation, especially
through its void present call. No device-loss or leak guarantee is inferred.

The optional loop requests 60 FPS using a monotonic deadline, measures submission
intervals, duration, mean/min/max and nominal missed intervals (10% jitter
tolerance). Interactive decode/recreation stalls count. Catch-up bursts are
avoided. This observes catastrophic pacing only; SDL2 `SDL_RenderPresent` has
no return value and timing does not measure visible refresh, production latency
or end-to-end pacing. Normal Asteria pacing is untouched.

Evidence directories contain `presentation.txt` (mode/pattern, runtime directory,
expected codec/bitstream/API, Vulkan adapter when used, SDL/backend/dimensions,
scaling, reset results, sample error, timing and qualification limits) plus selected
I420 files and their **SHA-256 of concatenated Y/U/V bytes** in the log. Revisited
pattern files are replaced; logs retain visits. Save console stderr as well for
runtime failures. Runtime binary/source hashes come from the accompanying CI
inventory; SDL/API cannot recover a DLL's source commit. Add Windows build,
display-driver version, native process confirmation, observations and optional
manual screenshots/photos. Screenshots are not captured automatically.

Exit 0 proves successful checked upload/render calls and completion; visual output
is always marked pending for owner review in the per-run log. That reminder is
not repository milestone status: the owner's completed inspection records qualify
P0.5 on the two named devices. Exit 77 explicitly reports unavailable
SDL window/renderer/Vulkan before first submission; losing availability afterward
or failing upload/decode/recovery is a failure. Hosted CI cannot visually qualify
color/range/chroma, scaling or true hardware loss. Log the renderer separately
for each future rerun; the completed RTX 4070 Ti x64 and Adreno X1-85 ARM64
records both identify Direct3D11 with SDL compiled/runtime 2.32.70 and IYUV.

### Building the presentation probe

Use a fresh target checkout and the existing verified baseline dependency helper:

```powershell
./scripts/setup-baseline-deps.ps1 -Architecture x64
$deps = (Resolve-Path build/pyrowave/x64).Path
$sdl = (Resolve-Path libs/windows).Path
cmake -S tests/pyrowave -B build/probe-x64 -A x64 -DPYROWAVE_EXPERIMENTAL=ON -DPYROWAVE_SDL_PRESENTATION=ON "-DSDL_ROOT=$sdl" -DSDL_ARCH=x64 "-DPYROWAVE_ROOT=$deps/install" "-DVULKAN_HEADERS=$deps/source/Granite/third_party/khronos/vulkan-headers/include"
cmake --build build/probe-x64 --config Release
ctest --test-dir build/probe-x64 -C Release --output-on-failure
```

For ARM64 use `-Architecture arm64`, `-A ARM64`, `-DSDL_ARCH=arm64`, the ARM64 probe
directory and matching dependency root. Use patched ARM64 root if required.
Parser-only CMake also runs headless deterministic patterns, pixel sizes,
aspect-fit, reset policy and timing tests without SDL/GPU. Optional CI builds and
runs both native targets, retains P0-R regressions, attempts hidden raw lifecycles
and codec presentation, and stages experimental binaries only. Unavailable
presentation is explicitly recorded; hosted CI does not replace owner inspection.
The completed hardware visual record is separate from those CI submissions.

qmake presentation opt-in adds `CONFIG+=pyrowave_sdl SDL_ROOT=... SDL_ARCH=x64`
(or arm64) to the existing offline command. The CI helper validates both target
qmake builds. SDL2.dll and SDL3.dll must be beside that executable. Application
qmake files, Session/common-c, H.264/HEVC/AV1 and normal packaging are unchanged.
