# Windows dependency inputs

Both v15 archives were downloaded and SHA-256 verified during the M0A harness work. Pins live in `scripts/baseline-deps.json`. Setup inventories every extracted file's hash and available Windows version resources in `build/evidence/<architecture>/dependency-files.json`. A successful setup writes a completion marker; the build checks that marker and rehashes the inventory before invoking Qt/MSVC.

| Target | Archive | SHA-256 |
| --- | --- | --- |
| x64 | Windows-x64.zip | `60003d5cf5147100352dede9836c1ba3c537dfff938fe0e3ef947bd4f51ca622` |
| ARM64 | Windows-ARM64.zip | `db61462026107a3a60b1f00b7469e6ddc4f56ccfc424724e7884f0001fe7c55d` |

[Release assets](https://github.com/moonlight-stream/moonlight-qt-deps/releases/tag/v15) are pinned by content hash. [Build recipes and source submodule revisions](https://github.com/moonlight-stream/moonlight-qt-deps/tree/2ab26b8cd5c42899ffd97c573ff2c678738f41b1) are fixed at `2ab26b8cd5c42899ffd97c573ff2c678738f41b1`. Follow each submodule at that revision for its source and license texts, including transitive dependencies selected by the build recipes. The archive is consumed as published; this harness does not rebuild those libraries.

## ARM64 runtime inventory

Versions below come from the downloaded DLL resources. Missing version resources are identified by the source pin instead of guessing a release version.

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
