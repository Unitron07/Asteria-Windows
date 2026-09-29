# Offline P0 experiment

This directory builds only the parser and optional Windows runtime probe. It
does not connect to any host. The app keeps Qt/qmake; CMake here is a standalone
dependency/test harness. Ordinary release builds neither compile nor ship the
runtime experiment. The new `app.pro` include is inert without
`CONFIG+=pyrowave_experimental`.

## Dependency and GPU-free tests

Run in PowerShell with Git, MSVC (x64 and ARM64 tools), Windows SDK and CMake
3.27 or newer. Default helper builds **both** architectures with independent
sources/caches/build/install/evidence paths. A target mismatch or dirty pinned
source fails; use a fresh output root for a patched retry. The isolated portable
math compatibility patch is applied for ARM64 by default; `-UnpatchedArm64`
reproduces the original compilation blockers and does not change x64.

```powershell
./scripts/build-pyrowave-deps.ps1
# Select just one: -Architecture x64 or -Architecture arm64
# Override the installed Visual Studio generator with -Generator if needed.
cmake -S tests/pyrowave -B build/parser-x64 -A x64
cmake --build build/parser-x64 --config Release
ctest --test-dir build/parser-x64 -C Release --output-on-failure
```

For native ARM64 parser execution, use `-A ARM64` and a different build directory,
then run CTest on a Windows ARM64 machine. Cross-building alone does not run it.
The optional workflow uses separate x64 and native ARM64 hosted runners.

## Runtime/decode probe

```powershell
$deps = (Resolve-Path build/pyrowave/x64).Path
cmake -S tests/pyrowave -B build/probe-x64 -A x64 -DPYROWAVE_EXPERIMENTAL=ON "-DPYROWAVE_ROOT=$deps/install" "-DVULKAN_HEADERS=$deps/source/Granite/third_party/khronos/vulkan-headers/include"
cmake --build build/probe-x64 --config Release
ctest --test-dir build/probe-x64 -C Release --output-on-failure
./build/probe-x64/Release/pyrowave-offline-proof.exe --load "$deps/install/bin"
./build/probe-x64/Release/pyrowave-offline-proof.exe --roundtrip "$deps/install/bin" "$deps/evidence/roundtrip"
```

Substitute ARM64 consistently for the target, build directory, and dependency
directory. The wrapper accepts only an absolute explicit dependency directory,
loads the exact DLL filename with `LoadLibraryExW`, and searches its dependencies
only in that directory and Windows System32. Before device creation it preloads
`vulkan-1.dll` from System32, constraining volk's indirect loader lookup. It checks API 0.6.0 before resolving
the remaining decoder exports. No import library or Vulkan library is linked.
CTest covers absent DLLs, incompatible API, missing exports, repeated rejected
loads, relative-path rejection and empty output after failed decode, without a
GPU. `--load` additionally exercises unload/reload of the real built DLL.

The roundtrip needs a native Vulkan loader/ICD and the codec's GPU features. It
generates 1920x1080 8-bit planar 420 with a deterministic gray luma ramp (16..235)
and neutral U/V (128), using encoder exports resolved lazily from the same pinned
codec DLL. Encoder budget is 800,000 bytes with 1,200-byte packet boundaries.
The resulting private PYRW frame is parsed and decoded to a tightly packed I420
buffer; plane sizes are 2,073,600 / 518,400 / 518,400 bytes. Mean absolute sample
error must be <= 8, a bring-up tolerance, not a quality claim. The test decodes the
same frame through three decoder lifetimes and verifies recovery after malformed
input. Device/decoder destruction completes before the module is unloaded.

Optional output files record the generated frame and decoded CPU planes. These
are generated fixtures with pinned-codec provenance, not captured host streams;
GPU/driver-dependent compression means they are not promised byte-identical
across adapters. I420 is SDL IYUV-compatible; this milestone copies into a known
pixel buffer and does **not** qualify an SDL window, color interpretation, GPU
presentation performance, frame pacing, or live decode latency.

## Qt/qmake experimental compile

From the usual target Qt/MSVC developer prompt, the same runtime source can be
compiled with the existing qmake toolchain:

```text
qmake tests/pyrowave/offline.pro CONFIG+=release PYROWAVE_ROOT=C:/absolute/target/install VULKAN_HEADERS=C:/absolute/pinned/Vulkan-Headers/include
nmake
```

Use a separate working/build directory for each architecture. To compile the
experiment into Asteria itself, pass `CONFIG+=pyrowave_experimental` and those
two paths to the existing root `moonlight-qt.pro` invocation. There is no runtime
copy, startup probe or active streaming hook. Keep it out of preview packages.

## Parser policy

The full PYRW container, including length fields, is capped at **850,000 bytes**
and **1,024 packets**. These are P0 application safety choices inspired by the
pinned host's encoder budget, not universal codec/protocol limits. A host frame
using the full 850,000-byte payload budget may exceed this conservative container
cap and be rejected. Review overhead/FEC/MTU policy before any live integration.

Parsing validates caller/reassembly length, magic/version/reserved/count, every
length, exact end position and payload sum before allocating offsets. It does
not copy packet data or allocate from a raw packet length. Arithmetic is bounded
by remaining bytes using subtraction; rejected input clears all partial state.
The caller retains immutable input while using packet offsets. GPU/device/output
allocation is a separate fixed 1080p contract in the P0 wrapper.

The parser suite includes valid one/multiple packets, every header/payload
truncation, unsupported header fields, zero/oversized counts and lengths,
count/sum/trailing mismatches, exact/oversized frame boundaries, UINT32_MAX and
SIZE_MAX cases, null input, offset correctness and 10,000 deterministic mutations.
