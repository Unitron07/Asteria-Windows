# M1B PyroWave source diff and implementation plan

**2026-09-25: source-diff phase complete; conditional go for an offline prototype, no-go for advertising support yet.** This change is documentation only. No decoder, preference, constants, dependencies or build flags are enabled. Runtime-loading, fallback and Windows ARM64 gates make a speculative scaffold premature. M0A remains complete; neither Windows architecture is qualified for PyroWave. Use Moonlight's existing statistics for M1A; do not change frame pacing or add a telemetry subsystem.

## Exact comparison boundary

| Source | Immutable revision |
| --- | --- |
| Asteria main inspected | [`a0233b79b707e5e79a6ad8aa1e963d496496696c`](https://github.com/Unitron07/Asteria-Windows/tree/a0233b79b707e5e79a6ad8aa1e963d496496696c) |
| Asteria common protocol gitlink | [`62e066388f1a1b133e0bee947b9a374311a3354b`](https://github.com/moonlight-stream/moonlight-common-c/tree/62e066388f1a1b133e0bee947b9a374311a3354b), at `moonlight-common-c/moonlight-common-c` |
| Reference client | [`92a4d936986b28cad70a443a67b22bd79a723661`](https://github.com/joemossjr16/moonlight-qt-pyrowave/tree/92a4d936986b28cad70a443a67b22bd79a723661) |
| Reference host | [`86a462feddadb0dcbe166fc61c36053e5441bdbf`](https://github.com/joemossjr16/pyrollo/tree/86a462feddadb0dcbe166fc61c36053e5441bdbf) |
| Supplemental handoff | [`7e19a940f69448e1c613b4a10e1eaa96acbe4ddb`](https://github.com/joemossjr16/pyrowave-streaming/tree/7e19a940f69448e1c613b4a10e1eaa96acbe4ddb); reported Windows host/WSL/Android results are reference-author evidence, not Asteria tests |

The client reference **vendors** common protocol sources; there is no independent common-c gitlink to substitute for Asteria's pin. Comparing the two `src/` trees gives exactly four changed files, **30 insertions and four deletions**. The remaining source files, including RTP/FEC, audio, input and connection control, are identical. Do not import the separate Android common fork, its receive-buffer changes, or the entire client.

Reproduce with clean checkouts at the revisions above:

```text
git -C Asteria-Windows ls-tree HEAD moonlight-common-c/moonlight-common-c
git diff --no-index common/src reference-client/moonlight-common-c/moonlight-common-c/src
git diff --no-index Asteria-Windows/app/streaming/session.cpp reference-client/app/streaming/session.cpp
git diff --no-index Asteria-Windows/app/streaming/session.h reference-client/app/streaming/session.h
```

`git diff --no-index` exits 1 for differences. Full `app/` comparison also includes Asteria branding/update-policy differences; these are not codec prerequisites. PyroWave changes are settings/UI, Session selection and format mapping, optional qmake integration, and new `pyrowave.cpp`/`pyrowave_decoder.h`. Existing decoder implementations are unchanged in that app diff.

## Protocol delta and collision audit

Paths below are relative to the common protocol root; links point to the pinned reference client.

| File / symbol | Minimum change and compatibility result |
| --- | --- |
| [`src/Limelight.h`](https://github.com/joemossjr16/moonlight-qt-pyrowave/blob/92a4d936986b28cad70a443a67b22bd79a723661/moonlight-common-c/moonlight-common-c/src/Limelight.h), formats | Add `VIDEO_FORMAT_PYROWAVE=0x10000`, `VIDEO_FORMAT_PYROWAVE_444=0x20000`, mask `0x30000`. Existing codec masks `0x000F`, `0x0F00`, `0xF000` OR to `0xFF0F`: zero intersection. Extend YUV444 mask `0xCC04` to `0x2CC04`; retain 10-bit mask `0xAA00`. |
| Same file, server modes | Add `SCM_PYROWAVE=0x00800000`, `SCM_PYROWAVE_444=0x01000000`, mask `0x01800000`; extend `SCM_MASK_YUV444` only. Existing defined capability bits OR to `0x007F0301`: zero intersection. Both fit current signed 32-bit fields. VIDEO_FORMAT and SCM are separate namespaces; equal numbers across them are not collisions. |
| [`src/RtspConnection.c`](https://github.com/joemossjr16/moonlight-qt-pyrowave/blob/92a4d936986b28cad70a443a67b22bd79a723661/moonlight-common-c/moonlight-common-c/src/RtspConnection.c), `performRtspHandshake()` | Before AV1, require a client PyroWave format, server **base** SCM_PYROWAVE, and `PYROWAVE/90000` in DESCRIBE SDP. Choose 444 only with both 444 bits, otherwise base. A 444-only server bit is insufficient. Harden base selection to require the client's base bit too: the reference can select base from a 444-only client mask. |
| [`src/SdpGenerator.c`](https://github.com/joemossjr16/moonlight-qt-pyrowave/blob/92a4d936986b28cad70a443a67b22bd79a723661/moonlight-common-c/moonlight-common-c/src/SdpGenerator.c), `getAttributesList()` | Emit `x-nv-vqos[0].bitStreamFormat=3`; existing values are H.264=0, HEVC=1, AV1=2. Existing `x-ss-video[0].chromaSamplingType` uses the expanded 444 mask. No new chroma field or HDR bit. |
| [`src/VideoDepacketizer.c`](https://github.com/joemossjr16/moonlight-qt-pyrowave/blob/92a4d936986b28cad70a443a67b22bd79a723661/moonlight-common-c/moonlight-common-c/src/VideoDepacketizer.c), `validateDecodeUnitForPlayback()` | Add an opaque BUFFER_TYPE_PICDATA assertion branch. Existing non-H.264/non-HEVC paths already bypass NAL parsing and consume frame metadata. Common-c does **not** parse PYRW; the codec adapter does. Preserve loss/FEC/queue handling. |

No collision was found **at these pins**. These are private extension values, not an upstream allocation guarantee; repeat checks after common-c updates. Maintain the four-file delta as a small protocol patch/fork commit on Asteria's pin, then update its gitlink in an implementation PR; do not flatten the submodule. Its nested pins remain ENet `aca87840b57f045a1f7f9299e4b1b9b8e2a5e2f1` and nanors `b1e3c22ca0cdc0bb83e3cd6ed1a2fc77869ed99a`.

### Host contract and PYRW bounds

Pinned [nvhttp.cpp](https://github.com/joemossjr16/pyrollo/blob/86a462feddadb0dcbe166fc61c36053e5441bdbf/src/nvhttp.cpp) advertises both bits through `video::pyrowave_advertised()`. [rtsp.cpp](https://github.com/joemossjr16/pyrollo/blob/86a462feddadb0dcbe166fc61c36053e5441bdbf/src/rtsp.cpp) offers RTP map 99 PYROWAVE/90000, rejects unavailable PyroWave ANNOUNCE, forces 8-bit SDR, accepts chromaSamplingType 0/1 and checks `fits_rate_controller()`. Older host prose saying it forces 420 is stale: **the pinned source supports 444 SDR too**. Ordinary Apollo/Nonary Vibepollo builds do not thereby gain support.

[`video::pyrowave::make_frame_payload()`](https://github.com/joemossjr16/pyrollo/blob/86a462feddadb0dcbe166fc61c36053e5441bdbf/src/pyrowave.cpp) serializes:

```text
bytes 0..3: ASCII PYRW
byte 4: version 1
bytes 5..6: big-endian uint16 packet count (1..65535)
byte 7: reserved zero
repeat count times: big-endian uint32 length (>0), then packet bytes
no trailing bytes
```

This private frame container is distinct from the upstream codec bitstream. The serializer checks count, nonempty lengths, uint32 length range and size_t overflow. `pyrowave_encode_session_t::MAX_FRAME_BYTES` in [video.cpp](https://github.com/joemossjr16/pyrollo/blob/86a462feddadb0dcbe166fc61c36053e5441bdbf/src/video.cpp) caps the encoder budget at **850,000 bytes**, with a 16 KiB minimum. Its rationale assumes four 255-shard FEC blocks, 20% FEC and a 1024-byte minimum network packet. This is a host budget, not a universal bound for arbitrary FEC/MTU settings; account for container/transport overhead in tests. Every host PyroWave frame is IDR/intra-only.

Reference `PyrowaveVideoDecoder::decodeFrame()` concatenates decode-unit buffers, validates magic/version/reserved/count/length/trailing data, pushes packets, calls `decode_is_ready(..., true)`, and synchronously decodes CPU planes. It clears on most parser/decode failures and returns DR_NEED_IDR. It reserves `du->fullLength` before an application-level cap and does not check the sum of entries equals that length. Asteria must bound total bytes/count/dimensions and allocation arithmetic **before allocation/GPU work**, validate entry sums and clear partial state on every rejected frame, including not-ready. Test malformed containers without a GPU. Partial-codec-packet recovery does not recover missing RTP fragments: common-c must first deliver a complete reassembled decode unit. Retain its FEC/drop policy.

## Dependency lock for the first build experiment

The client consumes an external PYROWAVE_ROOT; neither client nor host commit locks that directory. The handoff links the codec fork below. **This is our explicit source selection for reproducibility, not a recovered build ID for the author's binaries.** A reproducible Windows build remains unverified until the toolchain and resulting binaries are recorded.

| Component | Repository and exact revision | Role |
| --- | --- | --- |
| PyroWave C API 0.6.0 | [joemossjr16/pyrowave](https://github.com/joemossjr16/pyrowave/tree/f6fb84eb0d8538f43f6f54e58d2040d101c8676c), `f6fb84eb0d8538f43f6f54e58d2040d101c8676c` | Snapshot `89ba6fa42f89e6dd3c69d0683c04d956a87dd5ac` plus the 444 encoder payload-sizing fix. Do not select the experimental Adreno branch. Upstream provenance: [Themaister/pyrowave](https://github.com/Themaister/pyrowave); do not substitute moving HEAD. |
| Granite | [Themaister/Granite](https://github.com/Themaister/Granite/tree/b6cffd5ce81f540f0855e6778428483e14763d9b), `b6cffd5ce81f540f0855e6778428483e14763d9b` | Exact GRANITE_COMMIT in codec `checkout_granite.sh`. Fetched by script, not a codec gitlink. |
| volk | [zeux/volk](https://github.com/zeux/volk/tree/47cddf7ed97b94118a08aacb548a411188e016cc), `47cddf7ed97b94118a08aacb548a411188e016cc` | Granite `third_party/volk` gitlink; static granite-volk loads Vulkan dynamically. |
| Vulkan-Headers | [KhronosGroup/Vulkan-Headers](https://github.com/KhronosGroup/Vulkan-Headers/tree/6802bb4733b63ed5efd3adb308a6c885ef180ea1), `6802bb4733b63ed5efd3adb308a6c885ef180ea1` | Granite `third_party/khronos/vulkan-headers` gitlink; headers only. |
| Embedded shaders | Codec `shaders/slangmosh.hpp` and `slangmosh_scaler.hpp` at its pin | Use committed generated shaders. Minimal standalone build needs no runtime Slang, shaderc, glslang or SPIRV-Cross compiler. |

Use PYROWAVE_DEVEL=OFF, PYROWAVE_UTILS=OFF, GRANITE_SHARED=OFF, GRANITE_TARGET_NATIVE=OFF. Codec CMake selects null platform/shipping mode and disables Granite renderer/FFmpeg/Fossilize/runtime shader compiler/SPIRV-Cross/system handles. Its checkout script initializes only volk and Vulkan-Headers. Development-tool dependencies are outside this boundary. Retain defaults PYROWAVE_FP32_STORAGE=OFF and PYROWAVE_FP32_MATH=ON initially; record precision changes.

Build the dependency separately with **CMake 3.27 or newer on Windows**, MSVC Release, separate x64/ARM64 build/install directories; keep the app's Qt/qmake harness. Proposed experiment: Visual Studio generator with `-A x64` or `-A ARM64`, options above, then build target `pyrowave-shared`. This is not a successful build record. Pin actual CMake package/version/hash and MSVC/SDK versions alongside archive SHA-256s in the future dependency manifest; PyroWave toolchain/archive pins do not yet exist. Keep Asteria's existing Qt 6.11.2 and architecture-specific baseline dependencies.

### Runtime inventory and ARM64 evidence

| Dependency | Windows integration / ARM64 assessment |
| --- | --- |
| `libpyrowave-shared-0.dll` | New runtime. Reference qmake links `pyrowave-shared.lib` and post-copies the DLL from `build-msvc/output/bin`. Build per target. The C API is explicitly ABI-unstable before 1.0: resolve required exports and check API 0.6.0 before creating a device; version alone cannot prove identical ABI, so bind packages to source/hash. |
| Granite / volk / math/util libraries | Static in this configuration, no separate DLLs. SIMD has guarded x86 paths and portable alternatives, but `Granite/util/bitops.hpp` uses MSVC __popcnt/__popcnt64 under generic _MSC_VER/_WIN64. Verify on MSVC ARM64; a narrow intrinsic guard may be needed. NEON selection uses GCC-style macros; Android/Linux evidence does not establish MSVC ARM64 support. |
| `vulkan-1.dll` and GPU ICD | volk uses LoadLibraryA("vulkan-1.dll"). Both loader and usable GPU driver must be target-native. Reference qmake does not pin/ship them. Initially treat these as system-driver prerequisites; absence must disable PyroWave without preventing app launch. Record driver version per hardware test. |
| MSVC CRT / UCRT | Inspect new DLL normal/delay imports and deploy target runtime through existing packaging. Do not assume the current CRT set suffices. Exact added CRT filenames must come from the built import closure; never ship an x64 CRT in ARM64. |
| SDL2 / FFmpeg libswscale | Already in Asteria. Reference uploads 420 to SDL IYUV; 444 converts to BGRA with libswscale. No major dependency upgrade is necessary. |
| Reference artifacts/tools | Host MSYS2 UCRT64 and SteamOS x86_64 AppImage are not native Windows ARM64 inputs. Codec `build_aarch64.sh` is a Linux cross-build recipe. Host-only D3D11/D3D12/DXGI interop tests are not client runtime dependencies. |

No inherent x64-only requirement was established for the minimal design, but **the complete graph has not been shown to build or run on Windows ARM64**. MSVC intrinsics, PE import closure, Vulkan feature support and presentation remain gates. The codec API documents subgroup arithmetic/shuffle/shuffle-relative/vote/ballot/basic operations and subgroup-size control (Vulkan 1.3 core). A loader/version string is insufficient: device and decoder creation must succeed on the actual adapter. Do not qualify software Vulkan as a hardware decoder.

Preserve PyroWave/Granite MIT notices, volk's MIT notice and Vulkan-Headers' applicable licenses, plus existing Moonlight GPL obligations. Include exact sources/generated shaders and build inputs in corresponding-source/evidence artifacts. Final ZIP PE architecture checks and contamination tests remain mandatory for every added runtime.

## Integration and fallback contract

1. Explicit experimental persisted setting, default false, separate from the codec enum; build inclusion also defaults off. Avoid reference auto-enabling via pkg-config or the environment override (presence alone enables it, even a value of 0).
2. Forced H.264/HEVC/AV1 remains authoritative. Offer PyroWave only in Auto with opt-in, SDR and compatible decoder policy. Keep HDR disabled. Start with 420; map/reserve 444 but enable only after its own decode/color tests.
3. Isolate runtime loading through QLibrary with an absolute application-directory path or equivalently restricted Windows loading. Avoid a hard DLL import: reference direct linkage can fail app startup before fallback runs. Check exports/API, device, dimensions, decoder, output allocations **and presentation** before advertisement. Reference testOnly skips SDL renderer/texture creation and does not satisfy full presentation preflight.
4. Filter PyroWave against authenticated host capabilities even with 444 off. Asteria's general host-mask filtering is inside the 444 branch of validateLaunch(), and initialize() locks **only m_SupportedVideoFormats.front()** into supportedVideoFormats. Prepending PyroWave does not retain the standard offer set. Reference common fallback can choose H.264 if SDP omits PyroWave while decoder properties were prepared for PyroWave. Retain a validated standard-codec candidate/configuration; abort a mismatched PyroWave handshake cleanly before reinitializing for that candidate. Do not blindly OR all formats without checking per-decoder colorspace/range/callback/capability properties.
5. Missing DLL/export/driver/feature, failed preflight, disabled opt-in, HDR, forced standard codec or unsupported host: remove only PyroWave and run normal codec selection. Log one useful reason for opted-in failure. Never feed PyroWave bytes to FFmpeg as AV1/H.264; chooseDecoder() must fail if the selected PyroWave adapter cannot initialize.
6. After ANNOUNCE/stream start, host/decoder failure cannot change codec in place. Stop/release resources and offer a fresh standard-codec session, suppressing PyroWave for that retry. Preserve session/app identity; never automatically quit/relaunch the host app or replay side-effecting commands. Reset suppression on a deliberate later attempt; bound any future automatic retry.
7. Preserve pairing/audio/input/ordinary decoder paths and pacing. Reference rendering uses synchronous GPU-to-CPU readback, SDL upload and latest-frame events outside the FFmpeg pacer: useful interoperability evidence, not a production latency design. Do not import its pacing/telemetry wholesale. Use existing overlay/log infrastructure.
8. Keep baseline bitrate on fallback. Reference startConnectionAsync() applies its BPP override before final RTSP selection; do not copy this into the minimal integration.

## Concrete implementation map (future small PRs)

| Files | Functions/symbols and changes | Risks / acceptance boundary |
| --- | --- | --- |
| Common-c four files above; parent gitlink/.gitmodules if maintained fork used | Constants/masks, performRtspHandshake(), getAttributesList(), validateDecodeUnitForPlayback() | Minimal patch on 62e0663; fixtures for missing SDP, absent/partial bits, 444-only offers, SDR and unchanged standard codecs. |
| `app/settings/streamingpreferences.{h,cpp}`, `app/gui/SettingsView.qml` | enablePyroWave, read-only build availability, reload()/save(), experimental checkbox | Default false, unavailable builds cannot offer it. Preserve codec enum/settings; defer BPP UI. |
| `app/streaming/session.h` | SupportedVideoFormatList::maskByServerCodecModes() maps both new bits | Separate SCM/format namespaces; filter base/444 independently. |
| `app/streaming/session.cpp` | initialize(), validateLaunch(), getDecoderAvailability(), populateDecoderProperties(), chooseDecoder(), drSetup(), startConnectionAsync() | Preflight before offer, selected-format properties, dedicated decoder, safe retry. Preserve forced codec/HDR behavior. |
| New `app/streaming/video/pyrowave_runtime.{h,cpp}` | RAII module/export table and capability probe | Wrong/missing runtime is recoverable; retain module through all calls, release after worker shutdown. |
| New `app/streaming/video/pyrowave_frame.{h,cpp}` | Bounded container parser independent of Vulkan | Reject bad count/size/entry sum/truncation/trailing bytes. Fixtures and fuzz/sanitizer coverage; no unchecked allocations. |
| New `app/streaming/video/pyrowave_decoder.h`, `pyrowave.cpp` | IVideoDecoder initialize, submit/decode, render, cleanup and SDR contract | Reference is not drop-in. Test resource lifetime, resize/reconnect/device loss, 420 then 444 patterns. Preserve pacing. |
| `app/app.pro` | Explicit off-by-default CONFIG+=pyrowave / HAVE_PYROWAVE, target headers/runtime adapter | No mandatory DLL import, mixed target paths or app build migration. |
| New dependency lock/build helper under `scripts/`; `scripts/setup-baseline-deps.ps1`, `build-baseline.ps1`, packaging hooks | Fetch pins, build/archive per architecture, install runtime/notices/source, collect imports/hashes | Retain existing Qt/dependency and ZIP architecture guards; no reuse of x64 output for ARM64. |
| `.github/workflows/build.yml`, reusable Windows workflow when feature builds exist | Keep upstream/candidate x64/ARM64 matrix; add opt-in dependency/prototype jobs | Required baseline jobs remain. Feature-on cross-build alone is not hardware support. |
| New offline prototype/parser fixtures under `tests/` (harness chosen with implementation) | Load runtime, decode bounded known PYRW frame and present SDR | No host advertisement needed for first proof. Record architecture, driver, exports, output and cleanup. |

## First testable milestone and blockers

**P0: offline dependency/decode/present proof on x64 and native ARM64.** Build the locked minimal C API, inventory imports/PE types, test missing/wrong runtime, reject malformed frames without GPU work, and decode/present a known 1080p 420 SDR frame from a pinned host or codec roundtrip. Record exact compiler/SDK/CMake versions, hashes, Windows/driver/GPU, color/output and stop/recreate results. An ARM64 cross-build is only a build result until run natively on the target GPU. Add 444 as the next fixture.

Before offering the real streaming prototype:

- Prove Windows ARM64 dependency builds (especially Granite MSVC intrinsics), native driver features and presentation. No ARM64 PyroWave support claim yet.
- Implement optional runtime loading/full preflight; verify ABI/exports and package imports. The reference client/host pins do not lock Windows binaries/toolchains.
- Resolve single-format Session selection versus RTSP fallback, stale capabilities/SDP, decoder properties, bitrate and safe reconnect without host-app side effects.
- Establish allocation limits and test large frames/FEC/MTU/loss/malformed input. The host budget is not a protocol-wide allocation policy.
- Qualify SDR BT.709 limited range, 420/444, actual resolution limits and cleanup. Keep HDR excluded; defer GPU presentation optimization/pacing changes.

P1 then adds runtime-gated opt-in 420 against pinned Pyrollo. Negative cases: ordinary Apollo, absent SCM, absent SDP, base-only host, malformed 444-only offer, missing/wrong-architecture DLL, export/API mismatch, unavailable features, forced software, HDR, forced standard codecs, setup failure after successful probe, host encoder failure, reconnect and device loss. Re-run H.264/HEVC/AV1 lifecycle/audio/input checks and existing-statistics comparisons on both available architectures. Hosted runners are not GPU/Apollo interoperability tests.

## Validation of this documentation change

Local `scripts/test-baseline-preflight.ps1`, `scripts/test-package-architecture-tests.ps1` and `scripts/test-arm64-package-repair.ps1` passed on 2026-09-25, including mixed-architecture rejection. No application or workflow code changed. The focused PR records the exact candidate SHA and hosted upstream/candidate x64/ARM64 results; those baseline builds do not build or qualify PyroWave. P0 dependency builds and hardware/interoperability tests remain outstanding.
