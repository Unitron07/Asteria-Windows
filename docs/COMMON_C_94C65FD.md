# common-c 94c65fd compatibility review

Dependabot PR #66 advanced only the gitlink. Its candidate builds and both PyroWave
offline jobs failed, consistent with the exact-SHA guards in
`moonlight-common-c/pyrowave_patch.pri` and `scripts/apply-common-c-p1a.ps1`. The
same guard stopped the weekly Moonlight sync in issue #67, whose upstream range
(`1ced9dd` → `d0ce158`) also moves common-c to `94c65fd` in Moonlight `8016954c`.
Neither failure is evidence of a runtime fault.

This change bumps only common-c and the build plumbing it needs. The remaining
Moonlight app commits from #67 are left for the next upstream sync.

## Reviewed upstream range

`5d3c0ecca00c1bb3f587acb8060476edcf8e24aa` →
`94c65fd364d17b223f00a8763c9a5d94a66cb8dd` contains 48 commits. Grouped by effect:

| Area | Commits | Effect on this integration |
| --- | --- | --- |
| C11 atomics | `b0deb0a`, `7fd0f25`, `7d884fa`, `c3360b7`, `edb4e9e`, `e44e429`, `eac4c46`, `08ebedb`, `a0262e7`, `e64327c` | `<stdatomic.h>` is now included from internal headers. MSVC needs `/std:c11 /experimental:c11atomics`; Moonlight's `moonlight-common-c.pro` change from `8016954c` is taken unchanged, and `tests/pyrowave/CMakeLists.txt` sets the same flags. No app C++ file includes the internal headers. |
| Replay detection | `c7dff22` | RTSP and control decryption now reject replayed sequence numbers. `PltCreateCryptoContext()` takes a replay window count and `PltDecryptMessage()` takes a window index and sequence number. Requires a live Vibepollo check (below). |
| Jumbo control / ENet | `4853e70`, `7d61e2d`, `e80a786` | Larger-than-16-bit control payloads; no ENet backpressure for large sends; short-header disconnects handled. |
| STUN | `134eee4`, `7bf16de`, `2e18599`, `ee160c9` | Stricter STUN response validation. |
| Video reassembly | `bf71226`, `3387805`, `ae6633e` | Frames are capped at 32 MiB and 65,536 fragments; continuation packets need a prior SOF. PyroWave at its 3000 Mbps UI ceiling and 30 FPS is about 12.5 MB and about 9,000 packets per frame, well inside both caps. |
| Intra refresh | `0d92c94` | Sunshine-family ANNOUNCE now always carries `x-ss-video[0].intraRefresh`. Asteria never sets `CAPABILITY_INTRA_REFRESH`, so it sends `0`. |
| Public API | `3b6ee9d`, `541e396`, `a1aa0a1` | `LiPeekNextVideoFrame` removed; `LiGetRTP{Audio,Video}Stats` now fill a caller struct; `LiSendMultiControllerEvent` takes `uint8_t`/`uint16_t`. Asteria does not call the first two; its `gamepad.cpp` calls convert implicitly. |
| Fixes | remaining commits | Audio/LBQ/queue shutdown leaks and races, PKCS7 padding, Win32 timer overflow, 3DS/Darwin platform fixes, docs. |

## P1a patch

The five P1a files are `Limelight.h`, `PyroWave.h` (new), `RtspConnection.c`,
`SdpGenerator.c` and `VideoDepacketizer.c`. Upstream changed four of them, but none
of the PyroWave hunks. The patch was regenerated on `94c65fd`; only hunk offsets and
index lines moved. Forward and reverse `git apply --check` both pass against a clean
`94c65fd` checkout.

- `RtspConnection.c`: upstream changed decrypt/context arguments elsewhere in the
  file; the PyroWave DESCRIBE validation block is unchanged.
- `SdpGenerator.c`: the intra-refresh attribute is in the Sunshine block, separate
  from the PyroWave `bitStreamFormat:3` branch.
- `VideoDepacketizer.c`: PyroWave still takes the opaque full-frame assertion path
  shared with AV1, and is subject to the new reassembly caps above.

## Pins updated

- `moonlight-common-c/moonlight-common-c` gitlink
- `moonlight-common-c/pyrowave_patch.pri` (qmake SHA guard)
- `scripts/apply-common-c-p1a.ps1` and `scripts/test-common-c-p1a.ps1`
- `scripts/pyrowave/common-c-p1a.patch` header
- `docs/PYROWAVE_VIBEPOLLO.md` and `tests/pyrowave/README.md`

## Regression coverage and limits

The production-source C suites in `tests/pyrowave` had their stubs updated to the new
internal signatures (`PltCreateCryptoContext`, `PltDecryptMessage`, `PltSleepMs`,
`PltBroadcastConditionVariable`, `sendInputPacketOnControlStream`, atomic
`ConnectionInterrupted`). Their assertions are unchanged, except that the SDP
generator suite now also requires `intraRefresh:0`.

This was prepared without a local C/C++ toolchain. Patch application was checked
with Git only; MSVC, ARM64, Qt and packaging rely on CI for this exact head.

The fixtures do not exercise real encryption, so replay detection is unqualified
against Vibepollo. Owner smoke: connect to Vibepollo, stream AV1 briefly, start a
PyroWave stream, paste mixed ASCII/non-ASCII text, use a controller and mouse,
then disconnect and reconnect normally. No benchmark is required.
