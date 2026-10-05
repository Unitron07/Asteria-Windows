# Stage 2 qualification record

Visible owner qualification: **PASS / OWNER_CONFIRMED** on Surface Pro 11,
Snapdragon X Plus, Qualcomm Adreno X1-85, Windows ARM64. The owner reported these
results for the native ARM64 package from probe revision
`0008248ef75d2404225d9d2eed35c5a030c27fe9`. This record is based on the owner's
reported logs and visual observations; no screenshot verification is claimed.

The GPU-free policy executable passed. All three required visible runs passed:

| Run | API exit | Visual confirmed | Owner notes | Presents / generations (approximately) |
|---|---:|---|---|---|
| Flagged Vulkan window | 0 | true | none | 1035 / 38 |
| Initially unflagged window | 0 | true | none | 1032 / 38 |
| No V-sync | 0 | true | none | 1042 / 38 |

The counts are diagnostics, not performance benchmarks. The owner confirmed
red/green/blue/cyan/magenta/yellow upper bars, black/white/gray below, automatic
resize, minimize/restore, repeated rebuilds and correct output throughout.
After native teardown, SDL/D3D11 rendered visibly on the same window. No hangs,
freezes, wrong colors, flicker or other visual failures were reported.

Reported identity: `executable_arch=ARM64`, absolute System32 Vulkan loader,
SDL 2.32.74, selected Qualcomm(R) Adreno(TM) X1-85 GPU, valid Windows LUID,
existing-window Vulkan surface and stable HWND/window identity, VK_KHR_swapchain
and one combined queue family/index. Logs recorded `external_handles=NONE` and
`pyrowave_runtime=NOT_LOADED`.

The owner's result recorded `surface: true`, `status: OWNER_CONFIRMED`, and
`stage3Authorized: false`. **Validation remained SKIP unavailable in one or
more runs.** This is not a validation-layer pass.

## Owner runner correction

Before the probe started, the default documented Windows PowerShell 5.1 command
failed during parameter binding: Join-Path received an empty $PSScriptRoot in
the default EvidenceRoot expression. An explicit EvidenceRoot workaround let
the owner complete all three visible runs successfully. The subsequent fix
moves default resolution after parameter binding, uses the script location
with MyInvocation fallback, and preserves explicit paths. It also resolves the
executable relative to that same script directory.

The GPU-free regression runs the actual packaged script with Windows PowerShell
5.1 and pwsh if available, from an unrelated directory and package paths with
spaces. It checks default, explicit and whitespace roots, output placement and
absence of embedded absolute developer paths. A CPU-only unavailable fixture
exercises runner bookkeeping without a GPU or synthetic owner confirmation.
The focused workflow runs this regression for both package architectures.
Final focused and broad CI results are linked in [PR #35](https://github.com/Unitron07/Asteria-Windows/pull/35).

## Scope and remaining review points

This remains a synthetic Vulkan presenter: no live PyroWave acceleration,
production renderer selection change, codec/runtime pin change, shared codec
device, Y/U/V images, video shaders, overlays or external handles. The C++ probe
is unchanged by the runner correction. Stage 3 is not implemented or authorized.

Raw Vulkan is provisionally accepted, not permanently selected. Its five
low-level files contain 682 lines (525 resource-owner/device/WSI, 157 dispatch
and policy), plus 190 lines of CLI glue. Raw ownership includes loader/device
negotiation, framebuffers, bounded command contexts, legal binary synchronization,
recreation and partial-init cleanup. The libplacebo alternative can reuse its
device/WSI/rendering but requires shared-device borrowing and held/unwrapped
texture handoff adapted to the existing rendering contracts. No hybrid is built.

The Qualcomm driver advertises neither relevant EXT nor KHR swapchain-maintenance
present-fence extension. This is a capability limitation, not a failed Vulkan
probe. Stage 2 uses the authorized rare device drain for resize/teardown; core
render fences/device-idle do not by themselves universally prove every
presentation-engine lifetime property. Exact v19 libplacebo also documents the
gap and uses queue-idle mitigation without maintenance fences, so switching
libraries does not automatically remove this driver limitation. See the
[libplacebo implementation](https://github.com/haasn/libplacebo/blob/92b5ac6db79f4d680eb656692f7bf51e9606f42a/src/vulkan/swapchain.c)
and [Khronos guidance](https://docs.vulkan.org/guide/latest/swapchain_semaphore_reuse.html).

Before Stage 3, compare raw WSI/resource retirement complexity with the libplacebo
shared-device/texture-handoff alternative, qualify validation where available,
determine pinned codec borrowing requirements through real decode, and review
factory cleanup and real device-loss handling. Native x64 GPU qualification
remains desirable. Do not implement both presenter alternatives or treat this owner
result as separate Stage 3 authorization.
