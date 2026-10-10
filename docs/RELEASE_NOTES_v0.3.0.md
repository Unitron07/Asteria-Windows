# Asteria v0.3.0 — Native Vulkan Release

Asteria v0.3.0 introduces live native Vulkan presentation for **Experimental PyroWave** streaming. Supported GPUs can keep decoded video GPU-resident through BT.709 color conversion and presentation, avoiding the previous synchronous CPU I420 readback and D3D11 video upload path. Native overlays, bounded GPU frame management and initialization-time fallbacks accompany this change. Normal H.264, HEVC and AV1 streaming is retained.

## Highlights

- Native Vulkan presentation for live PyroWave on supported Windows hardware.
- Decode and presentation share one Asteria-owned Vulkan device.
- Three video slots with GPU-safe retirement of dropped, unpresented frames.
- Native debug/status overlays and retained-frame redraws, including overlay-only updates.
- Resize, minimize/restore, maximize and fullscreen/windowed lifecycle handling, reconnect support and clean session teardown.
- Qualified BT.709 FULL/LIMITED conversion with CENTER chroma presentation.
- Normal Windows x64 and native ARM64 portable packages with pinned runtime, shader provenance, symbols and corresponding source.

## Native Vulkan PyroWave

Select **PyroWave (Experimental)** explicitly when connecting to a compatible Vibepollo host. Automatic continues to choose standard codecs only. Native Vulkan is preferred when initialization proves the required device features, image usage, synchronization, borrowing and presentation capabilities.

On the previous Surface path, Vulkan decode was followed by synchronous CPU I420 readback, D3D11 texture upload and SDL/D3D11 presentation. The new preferred path is:

```text
PyroWave Vulkan decode on Asteria's device
 -> caller-owned GPU-resident Y/U/V
 -> qualified Vulkan BT.709 conversion
 -> native Vulkan swapchain
 -> display
```

The native video path uses no CPU video readback, external memory/semaphore handles or D3D11 video resources. Initialization-time legacy GPU interoperability and compute/CPU-I420 fallbacks remain available where native Vulkan cannot initialize. A fatal native error during streaming ends that stream through normal cleanup; it does not silently change backends.

Native Vulkan is specific to PyroWave in this release. It is not used for every codec or guaranteed on every GPU/driver.

## Streaming and compatibility

H.264, HEVC, AV1, Automatic selection, audio/input and the existing codec-aware bitrate settings retain their behavior. The PyroWave profile covered here is **SDR, 8-bit, 4:2:0, BT.709, CENTER chroma**. Stage 4 separately qualified FULL and LIMITED; the first Stage 5 live log verified FULL range, with no precise LIMITED-range live evidence recorded.

PyroWave remains pinned to commit `186f0393b77f7755953b5ecde994bb1cec2e4155`, bitstream `186f0393`, C API `0.6.0`; Granite remains `b6cffd5ce81f540f0855e6778428483e14763d9b`. Approved local patches, borrowed-device cleanup, strict DLL hashing/provenance and restricted runtime loading are retained.

## Windows x64 / ARM64

Use the x64 package on Intel/AMD Windows PCs, or the native ARM64 package on Windows-on-ARM hardware. Both use the established unsigned Release build configuration and include the required Qt/SDL/codec dependencies and notices. No Vulkan SDK, bundled Vulkan loader or runtime shader compiler is required; native Vulkan uses the installed Windows GPU driver.

Stage 5 live native Vulkan was owner-qualified on **Surface Pro 11 / Snapdragon X Plus / Qualcomm Adreno X1-85**, using normal ARM64 Asteria and normal Vibepollo. The first live log at qualified implementation `5c7ec593d9c5c27b097d194ea758b48b09a2bba4` verified fragment decode, all borrowed-device handle matches, zero CPU video readbacks/external handles/D3D11 resources, 93 safely retired presentation drops, bounded slot reuse and clean teardown.

Five reconnect cycles in the same process, window transitions, functional overlay-only updates, standard-codec streaming and audio/input across reconnects are separately **owner-confirmed**. The first log recorded `overlayRedraws=0`; additional machine counters or logs are not asserted. Hosted GPU tests and Vulkan validation were unavailable and recorded as **SKIP**, not hardware or validation PASS. The owner also confirmed final normal ARM64 release-package smoke on exact source `2f29a8963078dadbe4e5031791a6390098e4635d` / portable ZIP SHA-256 `4754bf1cf7c6809aa8877fd379a6c04a978f23788d8175cbf4d0b26d75f82791`: launch, native PyroWave video, audio/input, debug overlay, no fatal errors, clean disconnect and standard-codec streaming passed. This is owner functional sign-off; no additional machine counters or log verification are claimed. Broader GPU and x64 native-path hardware coverage remains separate.

## Performance observations

An initial Surface screenshot at 2560x1440 sampled about 119.91 incoming, decoding and rendering FPS. Its individual timing scopes were:

| Measurement | Sample |
| --- | --- |
| Decode CPU API / asynchronous submission | 0.38 ms |
| Frame queue delay | 0.34 ms |
| Rendering CPU time | 0.27 ms |
| Shutdown GPU iDWT fragment timing | 1.886 ms/frame |
| Shutdown GPU Dequant timing | 0.782 ms/frame |

CPU API/submission time is not total GPU decoding time. These samples are neither a matched comparison with the old fallback nor an end-to-end/scanout latency measurement. There is no universal 120 FPS or latency guarantee, and PyroWave is not claimed to outperform HEVC or AV1 generally. Controlled performance qualification and v1.0 polish remain the next development phase.

## Experimental feature limitations

- Native Vulkan availability depends on hardware and driver capabilities.
- PyroWave stays Experimental and explicitly selected; the Asteria release channel does not graduate the feature.
- PyroWave HDR, 10-bit, 4:4:4 and BT.2020 are outside this implementation.
- No calibrated ICC/display-color-management or general end-to-end latency guarantee.
- Broader compatibility, long-run resource/performance behavior and forced physical GPU-loss recovery remain separate qualification work.
- Vulkan validation was unavailable on the owner configuration and remains SKIP.

## Known issues

The tested Qualcomm driver rejects legacy Vulkan/D3D11 shared-fence import. Native Vulkan avoids that interoperability requirement on the qualified configuration; existing compute/CPU-I420 fallback remains available when native initialization is unsupported. The native success run did not exercise those fallback paths.

Packages are unsigned portable ZIPs. This release does not ship an installer, MultiSeat or VR, and does not add automatic diagnostic uploads. Report problems with the selected backend, source/package identity and sanitized logs; omit pairing data, credentials and private host details.

## Installation

Download `Asteria-v0.3.0-windows-x64-portable.zip` or `Asteria-v0.3.0-windows-arm64-portable.zip`. Extract into a writable folder and keep `portable.dat` beside `Asteria.exe`. Keep all bundled dependencies together; launch Asteria normally and pair/authenticate manually.

Asteria's release identity is **v0.3.0 — Native Vulkan Release**. The inherited Moonlight application/version display remains **6.2.0**; it is not the Asteria release tag.

## Verification

`SHA256SUMS.txt` covers the exact final portable, symbols, source and evidence asset bytes for both architectures. For example:

```powershell
Get-FileHash .\Asteria-v0.3.0-windows-arm64-portable.zip -Algorithm SHA256
```

Compare the result with the checksum file. The accompanying evidence records the release source SHA, run IDs, executable source revision, runtime/shader provenance and PE inventory. The bundled `verify-stage5-owner-package.ps1` additionally verifies the extracted package against that exact source/architecture and original ZIP hash. Package manifests bind identity; hardware approval is recorded separately in the release evidence.

Both architectures are built from release source [`2f29a8963078dadbe4e5031791a6390098e4635d`](https://github.com/Unitron07/Asteria-Windows/commit/2f29a8963078dadbe4e5031791a6390098e4635d), using [Windows baseline run 38091218299](https://github.com/Unitron07/Asteria-Windows/actions/runs/38091218299). Matching symbols and source snapshots include recursive submodules and the existing dependency/build provenance. Historical v0.2.0 release assets and notes remain intact.

## Credits / upstream acknowledgments

Asteria derives from [Moonlight PC](https://github.com/moonlight-stream/moonlight-qt) and retains its v6.2.0 baseline, history and licensing. Thanks to the Moonlight/common-c, PyroWave, Granite, Qt, SDL, FFmpeg and other dependency contributors, and to the Surface owner for functional qualification. Preserve all bundled licenses/notices and corresponding-source obligations. This release does not constitute an independent security audit or a new upstream dependency update.
