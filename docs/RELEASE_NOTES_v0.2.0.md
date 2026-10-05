# Asteria v0.2.0

Asteria v0.2.0 adds live Experimental PyroWave, improves streaming settings and branding, and advances the Moonlight upstream baseline while retaining native Windows x64 and ARM64 packages.

## Highlights

- Live Vibepollo PyroWave SDR 8-bit 4:2:0 streaming, bundled in the normal x64 and ARM64 packages.
- Codec-aware bitrate limits, PyroWave automatic bitrate calculation and persistent manual overrides.
- Codec selection beside Resolution/FPS in Basic Settings.
- Final Moonlight PC v6.2.0 baseline, updated Windows dependencies and input fixes.
- Finalized Asteria logo and application icons, plus expanded automated qualification and review-only upstream tracking.

## PyroWave

Select **PyroWave (Experimental)** explicitly. Automatic continues to select standard codecs only. No separate Asteria-P1a-Experimental download is needed.

The validated live profile is SDR, 8-bit, 4:2:0 with BT.709 full or limited range. The bundled codec/runtime is pinned to `186f0393b77f7755953b5ecde994bb1cec2e4155`, bitstream ID `186f0393`, API `0.6.0`, with strict runtime/provenance validation and Vibepollo compatibility framing. Parser/runtime/live-frame validation, malformed-frame recovery, safe runtime failure handling, performance-overlay integration and native GPU timing diagnostics have been added.

Asteria attempts GPU presentation where supported. On the tested Surface Pro 11 / Snapdragon X Plus / Qualcomm Adreno X1-85, Vulkan/D3D11 shared-fence import returns `PYROWAVE_ERROR_UNSUPPORTED_EXTERNAL_HANDLE`. Asteria safely recreates the decoder for compute-path decode plus CPU I420 readback/presentation. This fallback works; the limitation is external-resource synchronization/interoperability, rather than an inability to decode PyroWave.

Owner validation on native ARM64 Asteria with a Vibepollo host at 2560×1440, up to a 120 Hz target, covered negotiation, decoding, rendering, audio and input. Workload FPS varied. This is one named workload/device, not a universal benchmark, end-to-end latency measurement or performance guarantee.

## Streaming / Settings

- Automatic, H.264, HEVC and AV1: up to **500 Mbps**; PyroWave: up to **3000 Mbps**.
- Removed the old “Unlock bitrate limit (Experimental)” checkbox.
- PyroWave automatic bitrate uses approximately `width × height × fps × 1.6` bits/s (1.6 bits/pixel/frame).
- Manual bitrate overrides survive resolution/FPS changes. Codec changes preserve valid manual values and clamp only values exceeding the new codec ceiling.
- Codec selection is now in Basic Settings beside Resolution/FPS. New installs default to 1080p; existing saved dimensions remain in effect.
- Enter and Numpad Enter are distinguished with compatible host software; keyboard-release state regression coverage has been added.

## Moonlight v6.2.0 sync

The upstream baseline is the final Moonlight PC v6.2.0 release, `de2467e433821664cdd2224aad8c89a625be1ad9`, integrating the remaining upstream changes while preserving Asteria behavior. The original import already contained most pre-release development; the late upstream delta is 17 commits, not the entire Moonlight v6.2.0 changelog.

Relevant Windows changes include dependency refreshes and FFmpeg/SDL/OpenSSL fixes, input cleanup, Enter/Numpad Enter handling, the 1080p new-install default and build fixes. Asteria includes the corresponding security fixes from its full Moonlight v6.2.0 baseline for CVE-2026-33546, CVE-2026-33547 and CVE-2026-41210. Some were already inherited before the late delta; this is not an independent Asteria security audit. See [Moonlight’s release notes](https://github.com/moonlight-stream/moonlight-qt/releases/tag/v6.2.0).

## Windows / ARM64 and branding

Both Windows x64 and native ARM64 are shipped as unsigned portable ZIPs, with recursive PE architecture checks. Native ARM64 was already established before v0.1.0; this release preserves it and adds bundled PyroWave. Hardware qualification remains limited, primarily to the named Surface Pro 11 for live ARM64 PyroWave.

The finalized logo appears in the application, Windows executable, Qt/window/taskbar and portable package resources. WiX/installer artwork and README branding assets are updated; this release distributes portable ZIPs, not an installer or an unrelated UI redesign.

## Build / CI / Maintenance

Windows dependencies now use hash-verified moonlight-qt-deps v19 x64/ARM64 archives. Expanded coverage includes PyroWave parser, runtime/provenance, live-frame, presentation, queue/stats and GPU-presentation tests; settings persistence/UI and keyboard/input regressions; branding/icons; package architecture; bundled codec validation; and startup/import checks. Hosted tests do not establish broad GPU compatibility or physical-device-loss behavior.

Weekly Moonlight upstream tracking watches `master`, proposes history-preserving sync PRs and reports conflicts/manual-review cases. It never auto-merges or resolves conflicts unsafely. This is maintenance infrastructure, not runtime behavior.

Roadmap priorities are measured codec/performance work, native Vulkan PyroWave presentation, isolated sessions / MultiSeat, then Asteria VR. Old M2–M4 Apollo convenience milestones are deprioritized; useful plumbing may return later. These are future plans, not shipped features.

## Known limitations

- PyroWave remains Experimental, not production-stable. The validated live path is SDR 8-bit 4:2:0; PyroWave HDR and 4:4:4 are not implemented.
- Native Vulkan PyroWave presentation is planned post-v0.2.0 and is not included.
- The tested Qualcomm driver uses compute + CPU I420 fallback because Vulkan/D3D11 shared-fence import is unsupported. Broad GPU/hardware compatibility and shared GPU-path performance remain unqualified.
- Isolated sessions / MultiSeat and VR are not implemented. Generic Apollo clipboard, server commands and virtual-display control UI are not shipped.

## Downloads / verification

Use `Asteria-v0.2.0-windows-x64-portable.zip` for Intel/AMD Windows PCs, or `Asteria-v0.2.0-windows-arm64-portable.zip` for native Windows-on-ARM devices. Extract into a writable folder and keep `portable.dat` beside `Asteria.exe`.

Matching symbols, corresponding source snapshots including recursive submodules, and build/architecture evidence are provided for each architecture. Verify downloads against `SHA256SUMS.txt`, for example with PowerShell:

```powershell
Get-FileHash .\Asteria-v0.2.0-windows-arm64-portable.zip -Algorithm SHA256
```

The project release is v0.2.0; the inherited application version remains 6.2.0. Exact release commit, qualification run IDs and portable hashes are recorded in the accompanying release evidence and the repository’s validation record.

## Release qualification

Release commit and tag target: `42f756e465288157608fe894e3a3dfe800b05344`. Windows x64/native ARM64 candidate and upstream comparison jobs passed in [run 37262907765](https://github.com/Unitron07/Asteria-Windows/actions/runs/37262907765); both PyroWave jobs passed on the same commit in [run 37263855400](https://github.com/Unitron07/Asteria-Windows/actions/runs/37263855400).

Portable SHA-256:

- x64: `50977afd156f0fca6c216ce120e6b2e84a0442e0377828ba09c71f8f8a2ad5b0`
- ARM64: `65c1538c327e2358b7f74dc5353c73848ad95c007a93cbc49ef688fd92e7b53c`

Assets are the exact qualified outputs, renamed without rebuilding. Release-status documentation follows publication in a documentation-only commit; binaries and source remain tied to the tag above. [Full v0.1.0…v0.2.0 comparison](https://github.com/Unitron07/Asteria-Windows/compare/v0.1.0...v0.2.0).
