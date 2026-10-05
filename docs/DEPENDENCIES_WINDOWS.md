# Windows dependency inputs

Both current v19 archives were downloaded and their SHA-256 hashes verified against GitHub release metadata for the v6.2.0 sync. The original M0A v15 evidence remains historical in BASELINE.md. Pins live in `scripts/baseline-deps.json`. Setup inventories every extracted file's hash and available Windows version resources in `build/evidence/<architecture>/dependency-files.json`. A successful setup writes a completion marker; the build checks that marker and rehashes the inventory before invoking Qt/MSVC.

| Target | Archive | SHA-256 |
| --- | --- | --- |
| x64 | Windows-x64.zip | `65ff1bd439222e1e2750054f960dcd6fac46190027da16db7403e345c295f10f` |
| ARM64 | Windows-ARM64.zip | `ac606be421c8bf14e94128ea2116d3257a425c3d6fa102cd92bbd017f56647e2` |

[Release assets](https://github.com/moonlight-stream/moonlight-qt-deps/releases/tag/v19) are pinned by content hash. [Build recipes and source submodule revisions](https://github.com/moonlight-stream/moonlight-qt-deps/tree/79621271459ebb945343f58d7acd6ad24d402c38) are fixed at `79621271459ebb945343f58d7acd6ad24d402c38`. Follow each submodule at that revision for its source and license texts, including transitive dependencies selected by the build recipes. The archive is consumed as published; this harness does not rebuild those libraries.

## Historical v15 ARM64 runtime inventory

Versions below describe the earlier v15 download, not the current v19 package. Current setup records actual v19 per-file versions/hashes; consult the build evidence instead of applying this historical table to v19. Missing version resources are identified by the source pin instead of guessing a release version.

| Runtime | Observed version | Source at the pinned dependency revision; license location |
| --- | --- | --- |
| avcodec / avformat / avutil / swscale | 63.1.100 / 63.1.100 / 61.1.100 / 10.1.100; product d32b387 | FFmpeg; COPYING* and LICENSE.md |
| dav1d | product 1.5.4 (DLL ABI 7.0.0) | dav1d; COPYING |
| discord-rpc | no version resource | discord-rpc; LICENSE |
| OpenSSL libcrypto / libssl | 3.6.4 | openssl; LICENSE.txt |
| libplacebo | v7.371.0 | libplacebo; LICENSE |
| opus | no version resource | opus; COPYING |
| SDL2_ttf | 2.25.0.0 | SDL_ttf; LICENSE.txt |
| SDL2 | 2.32.70.0 | sdl2-compat; LICENSE.txt |
| SDL3 | 3.4.16.0 | SDL; LICENSE.txt |

The archive includes SDL3 and the SDL2 compatibility runtime. It is not a plain SDL2-only dependency set. Static inputs and headers (including Detours and Vulkan headers) are included in the generated full-file inventory and pinned source tree. The package's source-notices folder includes this provenance map; complete release redistribution notices remain a release-qualification task.

Qt is installed separately: 6.11.2, `msvc2022_64` for x64 and `msvc2022_arm64` with matching x64 host tools for cross-compilation. [Qt source archives](https://download.qt.io/archive/qt/6.11/6.11.2/submodules/) include module license texts. Record the actual installed Qt, compiler and SDK in each build's evidence. The active workflow installs both kits for ARM64 and has passed upstream/candidate builds on both targets. The new final-ZIP architecture gate includes deployed Qt plugins and runtime DLLs; hosted validation passes in [run 34797782854](https://github.com/Unitron07/Asteria-Windows/actions/runs/34797782854); M0A real-device native ARM64 validation is complete; see [BASELINE.md](BASELINE.md) for its scope.
