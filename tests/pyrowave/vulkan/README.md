# Isolated Stage 2 native Vulkan presenter probe

This executable presents synthetic RGB diagnostic clears. It does not load
PyroWave, decode video, accelerate live streaming, create output planes, use
external handles, or change Asteria's production renderer. The SDL/D3D11 test
is sequential fallback on the same window, not Vulkan/D3D11 interop.

Visible Surface Pro 11 ARM64 qualification is **PASS / OWNER_CONFIRMED**;
validation remained **SKIP**. See the [qualification record](QUALIFICATION.md)
for the tested revision, runner fix and remaining Stage 3 review points.

## Surface Pro 11 owner procedure (mandatory before Stage 3)

1. Download the **ARM64** `vulkan-stage2-arm64-<run>` Actions artifact. Extract
   its inner `vulkan-owner-arm64.zip` to a new folder. Keep the executable,
   `SDL2.dll`, and `SDL3.dll` together. Do not replace DLLs with system copies.
2. Close active stream sessions. Use the normal desktop with the existing
   Qualcomm Vulkan driver. No Vulkan SDK is required. Validation layers are
   optional; their absence is explicitly reported as SKIP.
3. Open native PowerShell in that folder. Run:

   ```powershell
   .\pyrowave-vulkan-policy-tests.exe
   powershell -NoProfile -ExecutionPolicy Bypass -File .\run-owner-tests.ps1 -Surface
   ```

   PowerShell 7 can run the script directly with `pwsh` instead. If policy
   prevents the script, use the direct commands below; do not weaken system
   execution policy globally. The MSVC CI package uses a static client CRT.
4. Each of the three visible runs lasts approximately 20 seconds, followed by
   three seconds of SDL output. In native Vulkan, verify six upper bars:
   **red, green, blue, cyan, magenta, yellow**. The lower row must be **black,
   white, gray**. No video/overlays are expected. The window resizes repeatedly,
   minimizes at about two seconds, and restores at about three seconds.
5. Confirm output returns correctly after each resize/restore. Press **R**
   several times to force rebuilds; drag/maximize/restore the window. **F**
   toggles desktop fullscreen. **Esc** ends native presentation early and runs
   the SDL test. Let the scripted run finish to exercise repeated rebuilds.
6. After native teardown the **same window** must display dark green with a
   centered white rectangle. Confirm that output is visible. This proves
   actual SDL/D3D11 rendering after Vulkan, not only renderer creation.
7. For each script prompt, type `YES` only after observing all listed behavior.
   Record flicker, blank frames, wrong colors, focus/capture issues, freezes,
   errors or mismatches. Do not infer success from exit code alone.
8. Return the entire `owner-evidence` folder, especially `owner-result.json`
   and all logs. Check `executable_arch=ARM64`, selected device containing
   `X1-85`, driver/device identity, `surface_window_identity=STABLE`, nonzero
   presents, multiple generations, same-window D3D11 API result and validation
   status. Stage 3 requires separate authorization and architecture review.

Direct commands if the runner script cannot be used:

```powershell
.\pyrowave-vulkan-probe.exe --exercise --seconds 20 --expect-device X1-85 --log surface-flagged.log
.\pyrowave-vulkan-probe.exe --exercise --without-vulkan-flag --seconds 20 --expect-device X1-85 --log surface-unflagged.log
.\pyrowave-vulkan-probe.exe --exercise --no-vsync --seconds 20 --expect-device X1-85 --log surface-no-vsync.log
```

Record the same visual observations manually. `--expect-device` is an owner
assertion after selection, not vendor-name GPU matching. x64 owners use the
x64 artifact and omit `-Surface` / `--expect-device X1-85`.

## Results and limits

- Exit **0 / API_PASS**: native API submission plus same-window SDL/D3D11
  rendering calls succeeded, or a requested fault was observed and cleaned.
  It does not certify visual correctness. Fault logs identify the injection.
- Exit **77 / SKIP**: required window/loader/hardware unavailable. Not PASS.
- Exit **1 / FAIL**: initialization/render/cleanup/fallback or validation error.
- Exit **2**: command-line or evidence-output error.
- `validation_status=SKIP`: layers/debug-utils unavailable or validation
  explicitly disabled. Zero reported errors with SKIP is not a validation pass.
- `--hidden` is automated API evidence only. Minimize/restore and visible output
  in hidden tests are not qualified.

No SDK, codec DLL, import library, shader compiler or additional GPU abstraction
is shipped. Vulkan is dynamically loaded from an absolute System32 path; SDL
uses that same path. The synthetic path needs Vulkan 1.0 core plus surface,
Win32 surface and swapchain extensions. It requests instance 1.1 where supported
for identity queries; no 1.3 minimum or codec capability claim is made.
`--api-1-0` explicitly exercises an instance 1.0 path with optional KHR
properties2 diagnostics. Automated CI includes this case; the codec's minimum
borrowed-device requirement remains deferred to real decode in Stage 3.

Two command resources are bounded by GPU fence readiness. Acquire timeout is
one millisecond, retried through a 16 ms SDL event wait. Present semaphores are
per swapchain image; the next image acquisition and submission dependency
protect reuse. Device idle is used only for resize/teardown. Ordinary surface
loss/device loss is terminal in this probe, with cleanup and a recorded error.

**Core WSI retirement limitation:** the probe uses the conventional rare
device-idle resize/teardown drain, as authorized for Stage 2. It does not use a
present-completion extension. Core idle/render fences alone do not prove full
presentation-engine completion in all WSI implementations. This is an explicit
review point before production integration: qualify the drivers, and evaluate
swapchain-maintenance present fences or libplacebo's maintained retirement
machinery before Stage 3. Do not mistake present-call success for scanout.
See [Khronos semaphore reuse guidance](https://docs.vulkan.org/guide/latest/swapchain_semaphore_reuse.html).

## Build and automated tests

GPU-free policy tests require CMake 3.27+ and C++17 only:

```powershell
cmake -S tests/pyrowave/vulkan -B build/vulkan-policy
cmake --build build/vulkan-policy --config Release
ctest --test-dir build/vulkan-policy -C Release --output-on-failure
```

Opt into the Windows probe using the exact Vulkan-Headers pin
`6802bb4733b63ed5efd3adb308a6c885ef180ea1` and the verified v19 SDL archive:

```powershell
cmake -S tests/pyrowave/vulkan -B build/vulkan-arm64 -A ARM64 -DPYROWAVE_VULKAN_PROBE=ON -DSDL_ARCH=arm64 -DSDL_ROOT=<absolute-libs/windows> -DVULKAN_HEADERS=<absolute-pinned-include>
cmake --build build/vulkan-arm64 --config Release
ctest --test-dir build/vulkan-arm64 -C Release --output-on-failure
```

CI also tests flagged/unflagged windows, no-V-sync policy, ten partial-init
failure checkpoints and two missing-function cases where hardware exists. It
checks PE architecture/imports for both executables and SDL DLLs, then packages
notices and SHA-256 manifests. GPU-free tests cover extent/image bounds, queue
requirements, mode order, resource reuse, timeout/suboptimal/out-of-date/terminal
state handling. CI cannot replace mandatory Surface visual owner execution.
The GPU-free packaged runner regression exercises Windows PowerShell 5.1 and
pwsh from an unrelated directory, with default, explicit and whitespace evidence
paths. It uses an unavailable CPU-only fixture and never records an owner pass.
