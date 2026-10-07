# Stage 4: offline native PyroWave presentation

Status: **Stage 4 COMPLETE / PASS_OWNER_CONFIRMED.**
The exact implementation head `7c6bf8e085ffe26fd36d7d72d7405b0ed5aa2e85`
was merged through [PR #39](https://github.com/Unitron07/Asteria-Windows/pull/39)
after final Surface Pro 11 / Snapdragon X Plus / Qualcomm Adreno X1-85 native
ARM64 owner qualification. No Session integration, production renderer selection,
codec pin update, overlays or Stage 5 implementation is included.

The final visible owner run recorded `stage4Qualification=PASS_OWNER_CONFIRMED`
and `ownerVisualConfirmed=true`: AUTO selected fragment, borrowed handles matched,
caller-owned R8 Y/U/V reached the explicit BT.709 shader and Vulkan swapchain,
FULL/LIMITED and CENTER chroma passed, all ten visible patterns were confirmed,
resize/minimize/restore/maximize/rebuild passed, three decode slots reused cleanly,
and teardown passed. Presentation used no CPU YUV readback, external handles or
D3D11 resources. Validation was `SKIP` with zero reported errors and warnings.

The independent verifier covered 80 cases and 81,957,240 pixels, max RGB error 1,
zero failed components, with 3,699,262 CENTER-vs-LEFT negative-control differences.
The visible run accumulated approximately 1,861 decoded/presented frames and 75
swapchain recreations; these are lifecycle evidence, not performance benchmarks.

## Architecture checkpoint (before presenter implementation)

Audited main: `7a038828c28cfcd28e0026c1e5018c7a852b3d7d`.
Reviewed the Stage 2 owner/dispatch/policy, Stage 3 requirements/output resources,
shared probe/policy, runtime borrowing and native decode, existing fixture suite,
and qualification/roadmap documents. Stage 2 already owns surface selection,
swapchain, per-image present semaphores, two fenced command resources and rare
resize/teardown drains. Stage 3 already owns stable feature chains, borrowed
create-info lifetime, identity checks, a nonrecursive queue lock and three native
R8 image/timeline slots. Stage 3 verification readback must not enter this path.

**Select raw Vulkan for Stage 4.** Remaining work is nine sampled plane views,
explicit nearest/linear samplers, three bounded descriptor sets, one small
fullscreen-triangle graphics pipeline with traditional render passes, and a
separate exact-plane upload/offscreen RGB verifier. WSI and codec ownership can
be reused. No Vulkan 1.3 requirement is needed.

Compared actual libplacebo revision
`92b5ac6db79f4d680eb656692f7bf51e9606f42a`, especially
`src/include/libplacebo/vulkan.h:392-621` and
`src/vulkan/swapchain.c:472-505`. Its alternative requires `pl_vulkan_import`,
audit/enabling of its required features, enabled extension/queue descriptions,
and queue callbacks. Each caller image would need `pl_vulkan_wrap` plus explicit
`release_ex`/`hold_ex` transitions and timeline handoffs into a second state
tracker. It can preserve the caller device, but these contracts need new tests.
It would replace the qualified WSI owner and add a package/build dependency and
its shader generation machinery. Its retirement code also uses queue-idle
mitigation when maintenance present fences are absent. Thus it does not remove
the known X1-85 limitation and is not substantially smaller/safer for this proof.
Only the raw alternative is implemented.

## Color and chroma contract (before shader implementation)

The pinned PyroWave `bitstream/bitstream.md:215-265` assigns CENTER=0, LEFT=1,
FULL=0, LIMITED=1, BT.709 primaries/matrix/transfer=0. Its
`pyrowave_encoder.cpp:1105-1113` zero-initializes the header and sets dimensions,
sequence and chroma resolution. CPU fixture encoding therefore emits CENTER,
FULL, SDR BT.709. Existing `presentation_patterns.h:52` explicitly authors a
chroma sample at `(2*i+0.5, 2*j+0.5)` in luma-index coordinates. Stage 4 validates
the actual frame header, rejects LEFT in this centered-only proof and selects
range from parsed frame metadata. LIMITED fixtures alter the range metadata
after encoding, as Stage 3 does; plane values are authored for that range.

For a luma-index coordinate x, centered chroma index c=(x-0.5)/2. A normalized
texture sample is (c+0.5)/(W/2)=(x+0.5)/W. Thus equal normalized Y/U/V coordinates
implement CENTER intentionally. LEFT would require a horizontal +0.5/W offset;
it is a negative control in the verifier, not an accepted presentation mode.
Nearest at native resolution alone cannot distinguish these phases reliably;
linear filtering and scaled nearest phase-sensitive cases must supplement it.
Samplers explicitly clamp to edge, use nearest or linear, one mip, no anisotropy.
The shader uses integer rational texel selection for nearest and explicit four-tap
bilinear reconstruction for linear. Native sampler/interpolated-UV scaled bars
initially failed at ties (a whole column); hardware linear filtering also produced
an error of 2 at a native sharp chroma edge. No tolerance was widened. Texel
coordinates now derive from gl_FragCoord and the integer fit rectangle, avoiding
interpolator and sampler fractional precision. Sampler objects remain explicit;
texelFetch does not use their filtering state. LINEAR means this shader's bilinear
policy, not a claim that hardware filtered texture() meets the ±1 contract.

The matrix returns nonlinear R'G'B' from nonlinear video Y'CbCr. Stage 4 retains
Stage 2's preference for B8G8R8A8_UNORM, then R8G8B8A8_UNORM, with
VK_COLOR_SPACE_SRGB_NONLINEAR_KHR. Shader output is encoded RGB, attachment
conversion is UNORM quantization only, **no sRGB attachment encode**. No extra
EOTF/OETF or ICC work is introduced. This is the simple SDR video-domain output
contract; it does not claim calibrated BT.1886/ICC display management.

FULL: Y=Y8/255, Cb=U8/255-0.5, Cr=V8/255-0.5.
LIMITED: Y=(Y8-16)/219, Cb=(U8-128)/224, Cr=(V8-128)/224.
Both use R=Y+1.5748*Cr, G=Y-0.187324*Cb-0.468124*Cr,
B=Y+1.8556*Cb, then clamp RGB to [0,1], alpha=1.
FULL chroma midpoint is intentionally 0.5 (127.5 code values), per the authorized
formula; LIMITED midpoint is 128. These are not interchangeable.

## Synchronization and lifetime

Three codec slots are independent of two WSI command resources and the acquired
swapchain image index. Each source slot has a native nonexportable timeline:
initial caller transition signals 1; decode waits consumed, signals consumed+1;
graphics waits that positive decode payload at FRAGMENT_SHADER, samples GENERAL
images, and GPU-signals consumed+2. Next decode waits that consumer value.
Source reuse waits sampling completion, never scanout completion. Static plane
descriptors are never rewritten while in flight. Queue submits/present/drains
share Stage 3's nonrecursive lock; SDL/WSI stay on the main thread.

Acquire uses Stage 2 finite waits and binary synchronization. Present semaphores
remain per swapchain image, protected by reacquisition and its submit wait.
Rare recreation/final drains remain allowed. On X1-85 no relevant EXT/KHR
maintenance present fence exists; core idle is the previously qualified driver
mitigation, not a universal presentation-engine completion guarantee. See
[Khronos lifetime guidance](https://docs.vulkan.org/guide/latest/swapchain_semaphore_reuse.html).

## Qualification boundary

Presentation: encoded fixture -> borrowed GPU decoder -> caller GPU Y/U/V ->
BT.709 shader -> swapchain -> present. No CPU YUV readback, no external handles,
no D3D11. Test verification: exact authored planes -> test-only GPU upload ->
same shader -> separate UNORM target -> RGB staging -> independent CPU reference.
The verifier isolates color/sampling from Stage 3's codec ±1 reconstruction bound.
Its RGB bound is ±1 output code value: Vulkan 8-bit UNORM conversion
permits either adjacent integer and the reference rounds to nearest. A larger
error fails; hardware empirical evidence must confirm this bound.
Test-input encoding uses the established Stage 3 separate codec-owned fixture
device, only to produce encoded input. The Stage 3 decoder-only caller feature
chain deliberately omits encoder-required shaderInt16. The candidate never uses
that device for decoding/presentation. No pin or patch was changed.

Local development ARM64 Clang/X1-85 run (uncommitted build, **not exact-head owner
qualification**): 80 shader cases, both ranges/five patterns/two filters/four
extents, max RGB error 1, zero failed components, CENTER/LEFT negative controls
distinguished. Hidden direct GPU-decode presentation exercised 1,737 frames and
all ten fixtures with bounded three-slot reuse. Validation was SKIP; no visible
confirmation is claimed. Exact-head MSVC CI packages and Surface owner evidence
remain mandatory. The real close() also passed every one of 61 partial allocation
prefixes under GPU-free destruction mocks, including a second close call.

Unavailable real GPU or validation is SKIP, never PASS. Visible qualification
requires explicit owner confirmation on Surface Pro 11 / X1-85 native ARM64.

## Build, package and owner procedure

The isolated CMake option is `PYROWAVE_VULKAN_STAGE4=ON`, with the same pinned
`PYROWAVE_ROOT`, `VULKAN_HEADERS`, `SDL_ROOT` and `SDL_ARCH` as Stage 3. Traditional
render passes preserve Vulkan 1.2 + reviewed extensions. There is no Vulkan import
library, shipped loader, runtime shader compiler or libplacebo dependency.
Checked shader source, SPIR-V, embedded header and `shader-provenance.json` are in
`shaders/`. Reproduce with `scripts/generate-stage4-shaders.ps1 -Compiler
<glslangValidator.exe> -OutputRoot <fresh-directory> -Verify`. The versioned
8.13.3559 archive and executable hashes are pinned in the provenance. Generation
targets Vulkan 1.0 / SPIR-V 1.0; the compiler is used by CI only and is not shipped.

Extract the architecture's `stage4-owner-*.zip` from the Stage 4 Actions artifact
to a fresh folder. Surface requires ARM64. Keep all DLLs, notices, shaders and
manifests together. No SDK is needed. Close active streams, then run:

```powershell
.\pyrowave-vulkan-stage4-policy-tests.exe
powershell -NoProfile -ExecutionPolicy Bypass -File .\run-stage4-owner-tests.ps1 -Surface
```

For x64 omit `-Surface`. To assert the reviewed PR head, add
`-ExpectedSourceRevision <40-character-head>`. PowerShell 7 is also supported.
`-NonInteractive` runs package checks and hidden shader proof only; it can never
qualify visible output. Unavailable GPU is exit 77 / SKIP. Evidence roots must be
fresh; relative roots resolve from the invoking CWD, executables from the script.

The visible run cycles FULL range/bars/chroma/geometry/gradient, then LIMITED,
with the pattern/range/filter in the title. Forty seconds exercise resize,
minimize/restore, maximize/restore and at least 20 actual rebuilds. R forces a
rebuild; Esc ends early (an incomplete sequence fails). It does not draw overlays.
All ten decoded fixture counters must be positive. After successful API checks,
type YES only after confirming the checklist. The runner records
`PASS_OWNER_CONFIRMED` only for an explicit visual confirmation. Return the entire
fresh `stage4-evidence/` directory. Do not merge until evidence is reviewed.

## Visible owner checklist

- Window appears; fitted video preserves 16:9, centered black bars stay black.
- No stretching, flipped orientation, plane swap, red/blue swap or resolution error.
- Bars order is white/yellow/cyan/green/magenta/red/blue/black; endpoints/neutral
  gray and FULL/LIMITED range are correct without accidental crush or lift.
- Centered chroma broad transitions and 2x2/4x4 quads align with luma fiducials.
- Geometry grid, corners and central square are correctly oriented and fitted.
- All ten patterns appear, resize/minimize/restore/maximize and R rebuild work.
- No stale frames, flicker, corruption, hangs or shutdown problems.

API correctness and visual confirmation are separate records. When available,
validation ERROR fails and warnings require review. No GPU timestamps or latency
claims: timing counters are CPU API intervals for decode, queue submit, present
and the presenter loop (including finite retry/recreation work).

## Evidence and limitations

Package checks cover architecture/PE imports, exact executable revision, runtime
pin/patch/Granite provenance, shader hashes, manifest inventory, factory source
test and policy/unwind tests. Evidence includes owner/API results, presentation
log, observed/expected RGB samples, all shader cases, swapchain generations,
lifecycle, runtime/shader provenance and validation. Faults after sampled views,
samplers, descriptor resources, modules, layout, pipeline and offscreen target
exercise cleanup; Stage 2 swapchain/framebuffer/present-semaphore faults remain.

The proof terminates on device loss or fatal submit/present error. No CPU/D3D11
fallback or Session recovery is added. Stage 4 is qualified for the isolated
offline presenter. Remaining limitations are live Session integration, production
renderer selection, overlays, HDR, 10-bit, 4:4:4, BT.2020, calibrated display/
ICC management, and Stage 4 performance/latency qualification. The rare X1-85
WSI recreation/final drains remain because the relevant maintenance present-fence
extension is unavailable. Vulkan validation was unavailable during the owner run.
