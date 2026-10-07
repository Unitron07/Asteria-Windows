[CmdletBinding()]
param([switch]$Surface,[switch]$NonInteractive,[string]$EvidenceRoot,[string]$ExpectedSourceRevision)
$ErrorActionPreference='Stop'
$package=$PSScriptRoot
if (!$package) { $package=Split-Path $MyInvocation.MyCommand.Path -Parent }
if ([string]::IsNullOrWhiteSpace($EvidenceRoot)) { $EvidenceRoot=Join-Path $package 'stage4-evidence' }
$EvidenceRoot=[IO.Path]::GetFullPath($EvidenceRoot)
if (Test-Path -LiteralPath $EvidenceRoot) { throw 'Use a fresh evidence directory' }
New-Item -ItemType Directory -Path $EvidenceRoot | Out-Null
$result=[ordered]@{sourceRevision=$null;architecture=$null;packageVerification='PENDING';ownerVisualConfirmed=$false;stage4Qualification='FAIL';validationStatus='SKIP'}
$exitCode=1
try {
    . (Join-Path $package 'runtime-provenance.ps1')
    . (Join-Path $package 'stage4-evidence-policy.ps1')
    $manifest=Get-Content (Join-Path $package 'sha256.json') -Raw | ConvertFrom-Json
    $inventory=@{}
    foreach ($entry in $manifest) {
        $file=[IO.Path]::GetFullPath((Join-Path $package $entry.file))
        if (!$file.StartsWith($package+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase) -or $inventory.ContainsKey($file)) { throw 'Invalid package manifest path' }
        $inventory[$file]=$true
        if (!(Test-Path -LiteralPath $file -PathType Leaf) -or (Get-FileHash -LiteralPath $file).Hash.ToLowerInvariant() -cne $entry.sha256) { throw "Package hash mismatch: $($entry.file)" }
    }
    # Evidence may be inside the package, but no extra executable/DLL is permitted.
    foreach ($binary in Get-ChildItem -LiteralPath $package -Recurse -File | Where-Object { $_.Extension -in @('.exe','.dll') }) {
        if (!$inventory.ContainsKey($binary.FullName)) { throw "Unmanifested binary: $($binary.Name)" }
    }
    if (Get-ChildItem -LiteralPath $package -Recurse -Filter vulkan-1.dll) { throw 'Bundled Vulkan loader forbidden' }
    $build=Get-Content (Join-Path $package 'build.json') -Raw | ConvertFrom-Json
    $result.sourceRevision=$build.sourceRevision; $result.architecture=$build.architecture
    if ($build.architecture -cnotin @('x64','arm64') -or $build.sourceRevision -cnotmatch '^[0-9a-f]{40}$' -or ($Surface -and $build.architecture -cne 'arm64')) { throw 'Architecture/source revision mismatch' }
    if ($ExpectedSourceRevision -and $ExpectedSourceRevision -cne $build.sourceRevision) { throw 'Unexpected source revision' }
    $exe=Join-Path $package 'pyrowave-vulkan-stage4.exe'
    foreach ($binary in Get-ChildItem -LiteralPath $package -Recurse -File | Where-Object { $_.Extension -in @('.exe','.dll') }) { Assert-PyroWavePe $binary.FullName $build.architecture | Out-Null }
    $reported=(& $exe --source-revision) -join ''
    if ($LASTEXITCODE -or $reported.Trim() -cne $build.sourceRevision) { throw 'Executable source revision mismatch' }
    $runtime=Assert-PyroWaveRuntime $package $build.architecture
    if ($runtime.graniteCommit -cne 'b6cffd5ce81f540f0855e6778428483e14763d9b' -or
        $runtime.factoryCleanupPatchSha256 -cne '8fe5906706bb27814ed7344f8932cd5ccf00c368ed24d253839d13cfeeb78c86' -or
        $runtime.factoryCleanupSourceTest -cne 'PASSED') { throw 'Runtime patch/Granite provenance mismatch' }
    $factoryPath=Join-Path $package 'install/bin/factory-ownership.json'
    $factory=Get-Content $factoryPath -Raw | ConvertFrom-Json
    if ((Get-FileHash $factoryPath).Hash.ToLowerInvariant() -cne $runtime.factoryOwnershipSha256 -or $factory.result -cne 'PASSED' -or
        $factory.liveWrappers -ne 0 -or $factory.allocations -ne $factory.destructions -or
        (Get-FileHash (Join-Path $package 'pyrowave-factory-cleanup-tests.exe')).Hash.ToLowerInvariant() -cne $factory.testBinarySha256 -or
        (Get-FileHash (Join-Path $package 'source-notices/runtime/patches/0004-borrowed-factory-cleanup.patch')).Hash.ToLowerInvariant() -cne $runtime.factoryCleanupPatchSha256) { throw 'Factory cleanup source/test provenance mismatch' }
    $runtime | ConvertTo-Json -Depth 6 | Set-Content (Join-Path $EvidenceRoot 'runtime-provenance.json') -Encoding utf8
    $shader=Get-Content (Join-Path $package 'shaders/shader-provenance.json') -Raw | ConvertFrom-Json
    foreach ($entry in $shader.files) {
        foreach ($field in @(@('source','sourceSha256'),@('spirv','spirvSha256'))) {
            if ((Get-FileHash -LiteralPath (Join-Path $package "shaders/$($entry.($field[0]))")).Hash.ToLowerInvariant() -cne $entry.($field[1])) { throw 'Shader source/SPIR-V hash mismatch' }
        }
    }
    if ((Get-FileHash (Join-Path $package 'shaders/video_spirv.h')).Hash.ToLowerInvariant() -cne $shader.embeddedHeaderSha256) { throw 'Embedded shader hash mismatch' }
    $shader | ConvertTo-Json -Depth 6 | Set-Content (Join-Path $EvidenceRoot 'shader-provenance.json') -Encoding utf8
    & (Join-Path $package 'pyrowave-factory-cleanup-tests.exe')
    if ($LASTEXITCODE) { throw 'Factory ownership regression' }
    & (Join-Path $package 'pyrowave-vulkan-stage4-policy-tests.exe')
    if ($LASTEXITCODE) { throw 'Stage 4 policy failure' }
    & (Join-Path $package 'pyrowave-vulkan-stage4-resource-tests.exe')
    if ($LASTEXITCODE) { throw 'Stage 4 resource unwind failure' }
    $result.packageVerification='PASS'
    @{status='PASS';sourceRevision=$build.sourceRevision;architecture=$build.architecture} | ConvertTo-Json | Set-Content (Join-Path $EvidenceRoot 'package-verification.json') -Encoding utf8
    $args=@('--runtime',(Join-Path $package 'install/bin'))
    if ($Surface) { $args+=@('--expect-device','X1-85') }
    & $exe @args --verify-only --evidence (Join-Path $EvidenceRoot 'nonvisual') > (Join-Path $EvidenceRoot 'nonvisual-console.log') 2>&1
    $apiExit=$LASTEXITCODE
    $api=Get-Content (Join-Path $EvidenceRoot 'nonvisual/api-result.json') -Raw | ConvertFrom-Json
    if ($apiExit -eq 77 -and $api.result -ceq 'SKIP') { $result.stage4Qualification='SKIP'; $exitCode=77 }
    elseif ($apiExit) { throw "Nonvisual GPU verifier failed: $apiExit" }
    else {
        $proof=Get-Content (Join-Path $EvidenceRoot 'nonvisual/shader-verification.json') -Raw | ConvertFrom-Json
        $life=Get-Content (Join-Path $EvidenceRoot 'nonvisual/lifecycle.json') -Raw | ConvertFrom-Json
        Assert-Stage4Api $api $proof $life $build.sourceRevision $build.architecture $false
        $result.stage4Qualification='API_PASS_OWNER_QUALIFICATION_PENDING'; $exitCode=0
        if (!$NonInteractive) {
            & $exe @args --exercise --seconds 40 --evidence (Join-Path $EvidenceRoot 'visible') > (Join-Path $EvidenceRoot 'visible-console.log') 2>&1
            if ($LASTEXITCODE) { throw 'Visible presentation API/lifecycle failed' }
            $api=Get-Content (Join-Path $EvidenceRoot 'visible/api-result.json') -Raw | ConvertFrom-Json
            $proof=Get-Content (Join-Path $EvidenceRoot 'visible/shader-verification.json') -Raw | ConvertFrom-Json
            $life=Get-Content (Join-Path $EvidenceRoot 'visible/lifecycle.json') -Raw | ConvertFrom-Json
            Assert-Stage4Api $api $proof $life $build.sourceRevision $build.architecture $true
            Write-Host 'Confirm all ten FULL/LIMITED patterns, bars/order/endpoints, CENTER chroma fiducials, orientation, aspect fit/black bars, resize/minimize/maximize/restore, no stale frames/flicker/corruption, clean shutdown (STAGE4.md checklist).'
            $answer=Read-Host 'Type YES only if every visible check passed'
            $result.ownerVisualConfirmed=$answer -ceq 'YES'
            if ($result.ownerVisualConfirmed) { $result.stage4Qualification='PASS_OWNER_CONFIRMED' }
            else { $result.stage4Qualification='OWNER_VISUAL_NOT_CONFIRMED'; $exitCode=1 }
        }
        foreach ($name in @('selectedDevice','borrowedDeviceMatch','preferredDecoderPath','actualDecoderPath','presentationBackend','presentationPath','cpuYuvReadbackInPresentation','externalMemoryHandles','externalSemaphoreHandles','d3d11Resources','chromaSitingPass','shaderVerificationPass','swapchainRecreationCount','validationStatus','validationErrors','validationWarnings')) { $result[$name]=$api.$name }
        if ($Surface -and ($api.selectedDevice -notmatch 'X1-85' -or $api.architecture -cne 'arm64' -or $api.preferredDecoderPath -cne 'fragment' -or $api.actualDecoderPath -cne 'fragment')) { throw 'Surface normal-selection/path assertion failed' }
        $result.fullRangePass=$true; $result.limitedRangePass=$true; $result.resizeRestorePass=(!$NonInteractive -and $life.recreations -ge 20)
        $child=if ($NonInteractive) { 'nonvisual' } else { 'visible' }
        foreach ($file in @('presentation.log','color-reference.json','shader-verification.json','swapchain.json','lifecycle.json','validation.log','validation.json','timings.json')) { Copy-Item -LiteralPath (Join-Path $EvidenceRoot "$child/$file") -Destination $EvidenceRoot }
    }
} catch { $result.stage4Qualification='FAIL'; $result.error=$_.Exception.Message; $exitCode=1; Write-Warning $result.error }
$result | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $EvidenceRoot 'owner-result.json') -Encoding utf8
exit $exitCode
