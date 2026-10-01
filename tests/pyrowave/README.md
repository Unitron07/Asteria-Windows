# Offline P0-R: Vibepollo compatibility proof

This standalone parser/Windows runtime harness does not connect to a host.
Normal Asteria builds and release packages stay unchanged; Qt/qmake remains the
application toolchain. Experimental app compilation needs
`CONFIG+=pyrowave_experimental`. No Session hook, codec setting or advertisement.

Active codec: Themaister/pyrowave `186f0393b77f7755953b5ecde994bb1cec2e4155`;
bitstream ID `186f0393`; C API 0.6.0. See [the current contract](../../docs/PYROWAVE_VIBEPOLLO.md)
and [source/patch lock](../../scripts/pyrowave/dependencies.json).
The old `f6fb84...` RTX 4070 Ti/Adreno results are historical. New hardware GPU
qualification on both targets is required before P0.5 presentation.

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
remain failures. ARM64 is not requalified until the new-codec GPU rerun succeeds
on Surface Pro 11 / Snapdragon X Plus / Adreno X1-85; x64 needs its RTX 4070 Ti rerun.

## qmake compile/link

```text
qmake tests/pyrowave/offline.pro CONFIG+=release PYROWAVE_ROOT=C:/absolute/target/install VULKAN_HEADERS=C:/absolute/pinned/Vulkan-Headers/include
nmake
```

Use separate target Qt/MSVC prompts/build directories. The optional CI invokes
`scripts/test-pyrowave-qmake.ps1` and records qmake/import evidence without changing
normal app packaging. Parser policy remains 850,000 total frame bytes and 1,024
compatibility packets; live MTU/FEC/overhead limits require a future review.
