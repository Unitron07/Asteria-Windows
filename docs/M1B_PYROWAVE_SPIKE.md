# M1B PyroWave status, source diff, and validation

**Current status: source-diff groundwork and P0 offline dependency/runtime/parser/decode proof are complete.** [PR #16](https://github.com/Unitron07/Asteria-Windows/pull/16) is merged; its final P0 head was `a02902fb58ac66ae4820373dd3978d3087de7d24`. Offline GPU decode is validated on Windows x64 RTX 4070 Ti and native Windows ARM64 Surface Pro 11th Edition / Snapdragon X Plus / Adreno X1-85. PyroWave remains experimental and off by default; live host negotiation is not implemented, and normal releases do not ship active PyroWave streaming support.

**Next: P0.5 presentation qualification, then a separate P1 live opt-in change.** Real SDL presentation, SDR display color/range/chroma, pacing, device-loss/recovery, 4:4:4, runtime deployment policy, and production latency/performance remain unqualified. HDR is excluded. No Apollo/Vibepollo end-to-end PyroWave stream has been validated. See [NEXT_STEP.md](NEXT_STEP.md) and the [validation record](VALIDATION.md#m1b-p0-offline-pyrowave-validation).

The PR #14 source-diff findings below remain the design basis. Their comparison boundary is historical; P0 build/runtime results later in this document supersede the initial build concerns. No Session, capability, RTSP/SDP, preference, bitrate, pacing, or normal preview-package behavior changed. M0A remains complete; use Moonlight's existing statistics for M1A.

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

## Pinned dependency lock

The client consumes an external PYROWAVE_ROOT; neither client nor host commit locks that directory. The handoff links the codec fork below. **This is our explicit source selection for reproducibility, not a recovered build ID for the author's binaries.** The P0 evidence below records the actual Windows toolchain, source archives, and resulting binaries.

| Component | Repository and exact revision | Role |
| --- | --- | --- |
| PyroWave C API 0.6.0 | [joemossjr16/pyrowave](https://github.com/joemossjr16/pyrowave/tree/f6fb84eb0d8538f43f6f54e58d2040d101c8676c), `f6fb84eb0d8538f43f6f54e58d2040d101c8676c` | Snapshot `89ba6fa42f89e6dd3c69d0683c04d956a87dd5ac` plus the 444 encoder payload-sizing fix. Do not select the experimental Adreno branch. Upstream provenance: [Themaister/pyrowave](https://github.com/Themaister/pyrowave); do not substitute moving HEAD. |
| Granite | [Themaister/Granite](https://github.com/Themaister/Granite/tree/b6cffd5ce81f540f0855e6778428483e14763d9b), `b6cffd5ce81f540f0855e6778428483e14763d9b` | Exact GRANITE_COMMIT in codec `checkout_granite.sh`. Fetched by script, not a codec gitlink. |
| volk | [zeux/volk](https://github.com/zeux/volk/tree/47cddf7ed97b94118a08aacb548a411188e016cc), `47cddf7ed97b94118a08aacb548a411188e016cc` | Granite `third_party/volk` gitlink; static granite-volk loads Vulkan dynamically. |
| Vulkan-Headers | [KhronosGroup/Vulkan-Headers](https://github.com/KhronosGroup/Vulkan-Headers/tree/6802bb4733b63ed5efd3adb308a6c885ef180ea1), `6802bb4733b63ed5efd3adb308a6c885ef180ea1` | Granite `third_party/khronos/vulkan-headers` gitlink; headers only. |
| Embedded shaders | Codec `shaders/slangmosh.hpp` and `slangmosh_scaler.hpp` at its pin | Use committed generated shaders. Minimal standalone build needs no runtime Slang, shaderc, glslang or SPIRV-Cross compiler. |

Use PYROWAVE_DEVEL=OFF, PYROWAVE_UTILS=OFF, GRANITE_SHARED=OFF, GRANITE_TARGET_NATIVE=OFF. Codec CMake selects null platform/shipping mode and disables Granite renderer/FFmpeg/Fossilize/runtime shader compiler/SPIRV-Cross/system handles. Its checkout script initializes only volk and Vulkan-Headers. Development-tool dependencies are outside this boundary. Retain defaults PYROWAVE_FP32_STORAGE=OFF and PYROWAVE_FP32_MATH=ON initially; record precision changes.

Build the dependency separately with **CMake 3.27 or newer on Windows**, MSVC Release, separate x64/ARM64 build/install directories; keep the app's Qt/qmake harness. The implemented helper uses the Visual Studio generator with `-A x64` or `-A ARM64`, the options above, and target `pyrowave-shared`. Actual CMake executable/version/hash, MSVC/SDK versions, source archive hashes, and runtime inventory are recorded in the P0 evidence below; future builds must retain their own evidence. Keep Asteria's existing Qt 6.11.2 and architecture-specific baseline dependencies.

### Runtime inventory and ARM64 evidence

| Dependency | Windows integration / ARM64 assessment |
| --- | --- |
| `libpyrowave-shared-0.dll` | New runtime. Reference qmake links `pyrowave-shared.lib` and post-copies the DLL from `build-msvc/output/bin`. Build per target. The C API is explicitly ABI-unstable before 1.0: resolve required exports and check API 0.6.0 before creating a device; version alone cannot prove identical ABI, so bind packages to source/hash. |
| Granite / volk / math/util libraries | Static in this configuration, no separate DLLs. SIMD has guarded x86 paths and portable alternatives, but `Granite/util/bitops.hpp` uses MSVC __popcnt/__popcnt64 under generic _MSC_VER/_WIN64. P0 native MSVC ARM64 execution verified these intrinsics without a bitops patch. The observed portable-math fallback defects and their isolated fix are recorded below. NEON selection uses GCC-style macros; Android/Linux evidence alone did not establish MSVC ARM64 support. |
| `vulkan-1.dll` and GPU ICD | volk uses LoadLibraryA("vulkan-1.dll"). Both loader and usable GPU driver must be target-native. Reference qmake does not pin/ship them. Initially treat these as system-driver prerequisites; absence must disable PyroWave without preventing app launch. Record driver version per hardware test. |
| MSVC CRT / UCRT | Inspect new DLL normal/delay imports and deploy target runtime through existing packaging. Do not assume the current CRT set suffices. Exact added CRT filenames must come from the built import closure; never ship an x64 CRT in ARM64. |
| SDL2 / FFmpeg libswscale | Already in Asteria. Reference uploads 420 to SDL IYUV; 444 converts to BGRA with libswscale. No major dependency upgrade is necessary. |
| Reference artifacts/tools | Host MSYS2 UCRT64 and SteamOS x86_64 AppImage are not native Windows ARM64 inputs. Codec `build_aarch64.sh` is a Linux cross-build recipe. Host-only D3D11/D3D12/DXGI interop tests are not client runtime dependencies. |

The source audit did not establish an inherent x64-only requirement. **The later P0 record below supersedes the original unverified ARM64 build assessment.** Native Vulkan device/decode success is now recorded on Adreno X1-85; presentation and device-loss behavior remain hardware gates. The codec API documents subgroup arithmetic/shuffle/shuffle-relative/vote/ballot/basic operations and subgroup-size control (Vulkan 1.3 core). A loader/version string is insufficient: device and decoder creation must succeed on the actual adapter. Do not qualify software Vulkan as a hardware decoder.

Preserve PyroWave/Granite MIT notices, volk's MIT notice and Vulkan-Headers' applicable licenses, plus existing Moonlight GPL obligations. Include exact sources/generated shaders and build inputs in corresponding-source/evidence artifacts. Final ZIP PE architecture checks and contamination tests remain mandatory for every added runtime.

## Future P1 integration and fallback contract (not implemented)

1. Explicit experimental persisted setting, default false, separate from the codec enum; build inclusion also defaults off. Avoid reference auto-enabling via pkg-config or the environment override (presence alone enables it, even a value of 0).
2. Forced H.264/HEVC/AV1 remains authoritative. Offer PyroWave only in Auto with opt-in, SDR and compatible decoder policy. Keep HDR disabled. Start with 420; map/reserve 444 but enable only after its own decode/color tests.
3. Isolate runtime loading through QLibrary with an absolute application-directory path or equivalently restricted Windows loading. Avoid a hard DLL import: reference direct linkage can fail app startup before fallback runs. Check exports/API, device, dimensions, decoder, output allocations **and presentation** before advertisement. Reference testOnly skips SDL renderer/texture creation and does not satisfy full presentation preflight.
4. Filter PyroWave against authenticated host capabilities even with 444 off. Asteria's general host-mask filtering is inside the 444 branch of validateLaunch(), and initialize() locks **only m_SupportedVideoFormats.front()** into supportedVideoFormats. Prepending PyroWave does not retain the standard offer set. Reference common fallback can choose H.264 if SDP omits PyroWave while decoder properties were prepared for PyroWave. Retain a validated standard-codec candidate/configuration; abort a mismatched PyroWave handshake cleanly before reinitializing for that candidate. Do not blindly OR all formats without checking per-decoder colorspace/range/callback/capability properties.
5. Missing DLL/export/driver/feature, failed preflight, disabled opt-in, HDR, forced standard codec or unsupported host: remove only PyroWave and run normal codec selection. Log one useful reason for opted-in failure. Never feed PyroWave bytes to FFmpeg as AV1/H.264; chooseDecoder() must fail if the selected PyroWave adapter cannot initialize.
6. After ANNOUNCE/stream start, host/decoder failure cannot change codec in place. Stop/release resources and offer a fresh standard-codec session, suppressing PyroWave for that retry. Preserve session/app identity; never automatically quit/relaunch the host app or replay side-effecting commands. Reset suppression on a deliberate later attempt; bound any future automatic retry.
7. Preserve pairing/audio/input/ordinary decoder paths and pacing. Reference rendering uses synchronous GPU-to-CPU readback, SDL upload and latest-frame events outside the FFmpeg pacer: useful interoperability evidence, not a production latency design. Do not import its pacing/telemetry wholesale. Use existing overlay/log infrastructure.
8. Keep baseline bitrate on fallback. Reference startConnectionAsync() applies its BPP override before final RTSP selection; do not copy this into the minimal integration.

## Source integration map: implemented P0 and future P1

P0 completed the dependency helper, parser/runtime sources, experimental qmake include, optional workflow, and offline tests. Rows below distinguish that work from future presentation/live integration.

| Files | Functions/symbols and changes | Risks / acceptance boundary |
| --- | --- | --- |
| Future P1: common-c four files above; parent gitlink/.gitmodules if maintained fork used | Constants/masks, performRtspHandshake(), getAttributesList(), validateDecodeUnitForPlayback() | Minimal patch on 62e0663; fixtures for missing SDP, absent/partial bits, 444-only offers, SDR and unchanged standard codecs. |
| Future P1: `app/settings/streamingpreferences.{h,cpp}`, `app/gui/SettingsView.qml` | enablePyroWave, read-only build availability, reload()/save(), experimental checkbox | Default false, unavailable builds cannot offer it. Preserve codec enum/settings; defer BPP UI. |
| Future P1: `app/streaming/session.h` | SupportedVideoFormatList::maskByServerCodecModes() maps both new bits | Separate SCM/format namespaces; filter base/444 independently. |
| Future P1: `app/streaming/session.cpp` | initialize(), validateLaunch(), getDecoderAvailability(), populateDecoderProperties(), chooseDecoder(), drSetup(), startConnectionAsync() | Preflight before offer, selected-format properties, dedicated decoder, safe retry. Preserve forced codec/HDR behavior. |
| P0: `app/streaming/video/pyrowave_runtime.{h,cpp}` | Restricted module/export table and offline decode wrapper; future full presentation preflight | Wrong/missing runtime is recoverable; retain module through all calls, release after worker shutdown. |
| P0: `app/streaming/video/pyrowave_frame.{h,cpp}` | Bounded container parser independent of Vulkan | Reject bad count/size/entry sum/truncation/trailing bytes. Fixtures and fuzz/sanitizer coverage; no unchecked allocations. |
| Future P1: new `app/streaming/video/pyrowave_decoder.h`, `pyrowave.cpp` | IVideoDecoder initialize, submit/decode, render, cleanup and SDR contract | Reference is not drop-in. Test resource lifetime, resize/reconnect/device loss, 420 then 444 patterns. Preserve pacing. |
| P0: `app/app.pro` and `app/streaming/video/pyrowave_experimental.pri` | Explicit off-by-default `CONFIG+=pyrowave_experimental`, target headers/runtime adapter | No mandatory DLL import, mixed target paths or app build migration. |
| P0: `scripts/build-pyrowave-deps.ps1`, `scripts/pyrowave/`; future normal packaging hooks | Fetch pins, build/archive per architecture, stage experimental runtime/notices/source, collect imports/hashes | Retain existing Qt/dependency and ZIP architecture guards; no reuse of x64 output for ARM64. |
| P0: `.github/workflows/pyrowave-offline.yml`; baseline workflows retained | Optional x64/native ARM64 dependency/prototype jobs alongside unchanged baseline matrix | Required baseline jobs remain. Feature-on cross-build alone is not hardware support. |
| P0: `tests/pyrowave/`; future P0.5 presentation fixtures | Load runtime, decode generated PYRW frame into I420 CPU buffer; SDL presentation remains next | No host advertisement needed for first proof. Record architecture, driver, exports, output and cleanup. |

## Milestone progression

**P0 complete:** exact dependency builds, GPU-free bounded parser, restricted runtime/API/export loading, and generated 1080p SDR 4:2:0 GPU decode into known I420 CPU buffers on x64 and native ARM64. Each hardware proof passed three decoder lifetimes and malformed-frame rejection/recovery. This is offline codec/runtime validation, not presentation or production streaming support.

**P0.5 next:** real SDL IYUV presentation, SDR BT.709/range/chroma patterns, resize/recreate and device-loss behavior on both targets, separate 4:4:4 qualification, and runtime deployment/import/shipping review. Keep HDR excluded. See [NEXT_STEP.md](NEXT_STEP.md).

**P1 planned, not started:** runtime-gated opt-in 4:2:0 against pinned Pyrollo, minimal capability/RTSP/SDP/common-c changes, safe decoder selection, and standard-codec fallback/reconnect without host-app side effects. Test absent SCM/SDP, partial/444-only offers, wrong DLL/export/API/driver, unavailable features, forced software/standard codecs, HDR exclusion, setup failure, host encoder failure, reconnect/device loss, and container/FEC/MTU/loss/allocation limits. Re-run H.264/HEVC/AV1 lifecycle/audio/input and measured comparisons on both targets. Hosted runners are not GPU/Apollo interoperability tests.

## Historical validation of the source-diff change

Local `scripts/test-baseline-preflight.ps1`, `scripts/test-package-architecture-tests.ps1` and `scripts/test-arm64-package-repair.ps1` passed on 2026-09-25, including mixed-architecture rejection. No application or workflow code changed. The focused PR records the exact candidate SHA and hosted upstream/candidate x64/ARM64 results; those baseline builds do not build or qualify PyroWave. P0 dependency builds and offline hardware decode were outstanding at that source-diff stage; the completed results below supersede that status. Presentation and live interoperability remain outstanding.

## P0 implementation and evidence (2026-09-28 local / 2026-09-29 UTC)

Merged [PR #16](https://github.com/Unitron07/Asteria-Windows/pull/16) added the pinned
dependency helper, GPU-free parser/tests, Windows dynamic runtime wrapper and
generated 1080p SDR 420 decode-to-CPU-buffer proof. Reproduction commands and
fixture provenance are in [tests/pyrowave/README.md](../tests/pyrowave/README.md).
`CONFIG+=pyrowave_experimental` is explicit and off by default. The new app
include does nothing in ordinary builds; there is no PyroWave import library,
DLL copy, startup probe, UI setting or Session/protocol/pacing change.

### Reproducible source and build boundary

`scripts/build-pyrowave-deps.ps1` builds x64 and ARM64 into independent
`build/pyrowave/<arch>/{source,build,install,evidence}` trees. It verifies every
exact commit and both Granite gitlinks, rejects mismatched or dirty pins,
records CMake/compiler executable hashes, MSVC/SDK versions, target, options,
source trees/archive hashes, patch diff/hash, imports/exports and PE machine.
It stages only the minimal shared target, its `pyrowave-shared.lib`, header and
notices; it does not replace Asteria's qmake build or fetch development tooling.
Use a fresh output root when changing ARM64 patch mode.

The original [joemossjr16/pyrowave source](https://github.com/joemossjr16/pyrowave)
(`https://github.com/joemossjr16/pyrowave.git`) returned GitHub 404; upstream
could not serve exact commit `f6fb84eb0d8538f43f6f54e58d2040d101c8676c`.
The helper prefers that original source if available, then uses the verified
[source-history recovery bundle](../scripts/pyrowave/README.md) only on fetch
failure. The bundle preserves the clean audited source content, available Git
history, MIT license, and provenance for reproducibility; it contains no built
codec runtime. Bundle SHA-256:
`e4387ce6b691724aa342d6df7677f51efffe30e98e3949415718e7b2b955c56e`.
The helper used by optional CI verifies checksum, bundle validity, exact HEAD
and clean tracked content; arbitrary source, moving HEAD, and silently changed
PyroWave revisions are rejected.

Canonical `git archive HEAD` SHA-256s from the actual dependency builds:

| Component | Source archive SHA-256 |
| --- | --- |
| PyroWave | `ed676276ef40c7116cd7dbf88941d2af339f16c2d5bb3a0d2d50ec2eea07a555` |
| Granite (original, before isolated patch) | `ff6fc6e7eb65b64c534fd8750db89d6097aafe3d2f88be31f3ca964d38b52690` |
| volk | `ab1e778960783a03aa21917db92279db88601dbf4be82161a18df6d553dc72ce` |
| Vulkan-Headers | `7329b22a6984fe21ac968840ff41fbe3ec834e82516faaa29731d70949268b89` |

The unchanged source commits are the four pins above. MIT/applicable header
licenses and generated shaders remain in the archives/notices. Experimental
artifacts include the four source archives and compatibility diff; all regular
release packaging and architecture-contamination tests remain intact.

### Actual Windows builds and ARM64 investigation

Both targets use MSVC **19.51.36257.0**, tools directory **14.51.36231**, Windows
SDK **10.0.26100.0**, CMake **4.4.3**, Release, and the six specified minimal
options. x64 uses `Hostx64/x64`; native ARM64 uses `Hostarm64/arm64` on GitHub's
`windows-11-arm` runner. CMake executable SHA-256 is
`ab8247ca4554871e5d75c0ee118ee8663727bd6fdf37ad76b17e9cc8d5f286a8` (x64) and
`a32ae68e4b8b030a9df26a27c4094fca8eb0f3083cd0c3a9db348c9c4cdab54b` (ARM64).
These are recorded toolchain inputs, not a promise of byte-identical DLLs on
future hosted images; each run retains its actual hashes.

Unpatched ARM64 compilation was actually attempted in
[run 36513013000](https://github.com/Unitron07/Asteria-Windows/actions/runs/36513013000).
It fails **at compile time** in Granite's SIMD-only frustum helper and invalid
scalar affine transpose. The first patched build also exposed scalar mat4
transpose recursion (C4717). The final isolated patch repairs only those three
portable fallback bodies; x86 and existing NEON branches are untouched. No
unrelated Granite changes or architecture macro impersonation are used.
See [COMPATIBILITY.md](../scripts/pyrowave/COMPATIBILITY.md).

The pinned generic `_MSC_VER/_WIN64` `__popcnt/__popcnt64` and bit-scan branches
compile and pass zero/all-bit/high-bit execution tests with this native ARM64
compiler. No speculative bitops change is necessary at this toolchain version.
MSVC ARM64 does not select the pin's GCC-style NEON macros; the repaired portable
path is intentional. Older MSVC versions are not established by this evidence.

In [run 36513981148](https://github.com/Unitron07/Asteria-Windows/actions/runs/36513981148),
both dependency DLL/import libraries and CMake experimental probes compile/link,
and all five parser/runtime-rejection/Granite tests pass on each native target.
The real codec DLL loads and unloads on both. x64 qmake also passes there; the
ARM64 job then fails in Qt archive selection, before qmake. The corrected Qt
host input is validated in passing
[run 36514467221](https://github.com/Unitron07/Asteria-Windows/actions/runs/36514467221)
at code commit `151d057e29e16904f6b828482000ba1df711fc50`: both dependency builds,
all five tests on each native target, real runtime load/unload, no codec/Vulkan
startup import, PE checks and **Qt 6.11.2/qmake compile/link** pass. Both hosted
roundtrips record exit **77** (`PYROWAVE_ERROR_NO_VULKAN`, no usable Vulkan
device); these are explicit unavailable results, not decoder successes. The
ARM64 build had no remaining observed compile/link blocker. The later
Surface/Adreno hardware proof below completes offline GPU decode qualification.
Local baseline preflight, package architecture, ARM64 package
repair and a synthetic wrong-revision helper rejection also pass.

Final optional [P0 run 36515183774](https://github.com/Unitron07/Asteria-Windows/actions/runs/36515183774)
passed x64 and native ARM64 at final head
`a02902fb58ac66ae4820373dd3978d3087de7d24`, including dependency builds,
parser/runtime/compatibility tests, PE/import checks, real DLL load/reload,
and qmake probes. Final baseline [push run 36515181039](https://github.com/Unitron07/Asteria-Windows/actions/runs/36515181039)
and [PR run 36515184040](https://github.com/Unitron07/Asteria-Windows/actions/runs/36515184040)
also passed. GPU evidence comes from the hardware runs below, separately from
hosted build/test results. Earlier run hashes retain their original scope.

Hash-bound DLL inventory from passing run 36514467221:

| Target | PE machine | `libpyrowave-shared-0.dll` SHA-256 |
| --- | --- | --- |
| x64 | `0x8664` | `490cf7923151b573ed3efbbea8fa0157f541fb9195d8bf39a14b5503e7bb470d` |
| ARM64 | `0xAA64` | `baca5855a5dd06b76c0786ebb34c6cb0124ceba47050879a75f618d8a59c0250` |

Final portable-math patch SHA-256:
`0ce30e7019e45677182995248af83819a2159512e9ac799a6ae33f72ace93ffe`.
The qmake probe is likewise verified as x64 `0x8664` / ARM64 `0xAA64`.

Both install `pyrowave-shared.lib` and API 0.6.0 headers. Normal DLL imports are
`KERNEL32.dll`, `USER32.dll`, `MSVCP140.dll`, `VCRUNTIME140.dll`, and UCRT API-set
stdio/math/string/convert/environment/heap/runtime/time DLLs. x64 also imports
`VCRUNTIME140_1.dll`; ARM64 does not. No separate Granite/volk DLL is produced.
Vulkan is dynamically loaded and needs a target-native system loader and ICD.
Full normal/delay import and export data is retained in `runtime-imports.txt`,
`runtime-dependents.txt` and `runtime-exports.txt`; the CMake probe import scan
confirms no PyroWave or Vulkan startup import.

### Parser and runtime policy

Full private PYRW containers are capped at **850,000 bytes including overhead**
and **1,024 packets**. Both are chosen P0 safety limits, not protocol universals.
A host payload at its maximum 850,000-byte encoder budget may be rejected once
container overhead is included. Revisit FEC/MTU/overhead policy before P1.
Parsing validates caller/reassembly length, header fields, count, nonempty
lengths, subtraction-based remaining-byte bounds, exact end position and length
sum before allocating a bounded offset table. Payloads are not copied; caller
input must stay immutable. Every error clears packet/payload state. The suite
passes 34 explicit cases plus 10,000 bounded deterministic mutations without
Vulkan; the wrapper separately fixes output extent to 1920x1080 and bounds I420
allocation to 3,110,400 bytes.

`LoadLibraryExW` loads only an explicit absolute codec file and searches
dependencies in that directory/System32. API 0.6.0 is checked before decoder
exports. Encoder exports are resolved only by the local proof. Missing, wrong
API or missing-export DLLs remain recoverable, with useful reasons. System32
Vulkan is preloaded before Granite's indirect loader lookup; no process-wide
search-path change or startup hard-link is introduced. Decoder packets clear
on success and every failure; encoder/decoder/device destruction precedes
module unload. The pinned Granite default-device path retains its own
process-lifetime Vulkan loader reference without an exported teardown hook;
the wrapper releases its owned references. Do not claim a driver/module leak
soak from these short tests; review repeated module cycling before live use.

### Local x64 decode proof and limits

The final x64 artifact from passing run 36514467221 was downloaded and SHA-256
verified (`ddbb65aa7eb1acba7a6c1267fedbe8b61c25fd08249277c23d44afe0cca73e4b`).
On Windows 11 Pro **10.0.26200**, a native x64 probe selected **NVIDIA GeForce
RTX 4070 Ti**, vendor 4318/device 10114, Vulkan API integer 4211039 and driver
integer 2585198592. Windows display-driver version is **32.0.16.1692**; system
Vulkan loader file version **1.4.341.0**. This is a named-device offline result,
not a general GPU support matrix.

The pinned encoder generated a 60,312-byte PYRW frame from the deterministic
1080p gray ramp/neutral-chroma 8-bit SDR 420 fixture. Parsing/decoding succeeded
through three decoder lifetimes; each returned I420 planes of **2,073,600 /
518,400 / 518,400 bytes**, sample mean absolute error **0.000694444**, and
recovered after a malformed reserved byte. The decoded data was copied into a
known tightly packed CPU buffer. Generated fixture SHA-256 for this run:
`4a8856af5b774a2189b7cdaaa3908125c4ad95ae3a38648a07ba1dac6c1e1802`;
decoded I420 SHA-256:
`fd885fd91466133f7594c5b25722db60da8d47bbe44f4ac9fec56b2734a0ed43`.
Generated bytes can vary with encoder initialization/GPU/driver; generate the
fixture during test setup rather than treating that hash as protocol identity.

### Native ARM64 real-hardware offline decode proof

The owner ran the native ARM64 CI artifact on a **Surface Pro 11th Edition**,
**Snapdragon X Plus**, native Windows ARM64, **Qualcomm Adreno X1-85 GPU**.
Exact commands from the extracted artifact root:

```powershell
.\probe-arm64\Release\pyrowave-offline-proof.exe --load ".\pyrowave-patched\arm64\install\bin"
.\probe-arm64\Release\pyrowave-offline-proof.exe --roundtrip ".\pyrowave-patched\arm64\install\bin" ".\roundtrip-output"
```

Restricted runtime loading, required exports, API **0.6.0**, and unload/reload
passed. Vulkan adapter/device creation succeeded:

```text
Qualcomm(R) Adreno(TM) X1-85 GPU vendorID=20803 deviceID=909329200 driverVersion=2151112704 apiVersion=4210983
```

These driver/API values are raw Vulkan integers. Cycles **0, 1, and 2** each
decoded the generated **60,312-byte** frame to **1920×1080 8-bit SDR 4:2:0**,
with I420 **Y = 2,073,600; U = 518,400; V = 518,400 bytes** and MAE
**0.00104167**. This ARM64 MAE is separate from the RTX 4070 Ti x64 result
**0.000694444**.

Each `PyroWave P0: nonzero reserved byte` line is an intentional malformed-frame
rejection. A valid decode immediately afterward succeeded, demonstrating
recovery through all three decoder lifetimes; these diagnostics are not
failures. Final result:

```text
PASS: known CPU pixel buffer copied; SDL IYUV-compatible (no SDL window/pacing test)
```

This validates native ARM64 PyroWave GPU decoding on actual Snapdragon/Adreno
hardware. It does not qualify real SDL presentation, display color/range/chroma
siting, pacing, or live streaming. The ARM64 Windows build, Windows display-driver
version, artifact hash, and generated frame/output hashes were not supplied;
do not copy those values from the x64 record.

Hosted GPU availability is recorded separately: native parser/loader execution
alone does not prove GPU decode, and roundtrip exit 77 means unavailable native
Vulkan. Other decode failures fail the optional job. The two real-hardware
results complete P0's known-CPU-buffer route; no production support or latency
comparison is claimed.

### Remaining gates before live host negotiation

- SDL presentation/color/range/chroma-siting tests on each target, then separate
  444 coverage; keep HDR excluded.
- Review upstream loader lifetime, driver/device loss, repeated decoder/device
  recreation and clean-machine CRT/Vulkan deployment; scan every experimental
  package runtime and decide final runtime shipping/package policy.
- Resolve Session's single-format selection and selected-decoder properties
  versus missing/changed RTSP/SDP; retain safe standard-codec fallback and
  reconnect without replaying host application actions.
- Review application resolution/allocation limits, container overhead, FEC/MTU,
  loss/malformed codec packets and lifecycle recovery before host input.
- Production latency/performance comparisons and Apollo/Vibepollo end-to-end
  PyroWave streaming remain unvalidated.
- A separate P1 PR may add the minimal common-c delta, runtime-gated opt-in 420
  and pinned-host interoperability/negative cases. P0 adds no advertisement,
  RTSP/SDP changes, codec preference, bitrate behavior or frame-pacing changes.
