[CmdletBinding()]
param([Parameter(Mandatory)][string]$PackageRoot,[Parameter(Mandatory)][string]$FixtureExe,[Parameter(Mandatory)][string]$EvidenceRoot)
$ErrorActionPreference='Stop'
$PackageRoot=(Resolve-Path $PackageRoot).Path; $FixtureExe=(Resolve-Path $FixtureExe).Path
New-Item -ItemType Directory -Force $EvidenceRoot | Out-Null; $EvidenceRoot=(Resolve-Path $EvidenceRoot).Path
$unrelated=Join-Path $EvidenceRoot 'unrelated working directory'; New-Item -ItemType Directory $unrelated | Out-Null
$build=Get-Content (Join-Path $PackageRoot 'build.json') -Raw | ConvertFrom-Json
$shells=@((Get-Command powershell.exe).Source); if(Get-Command pwsh.exe -ErrorAction SilentlyContinue) { $shells+=(Get-Command pwsh.exe).Source }
foreach($shell in $shells) { foreach($case in @('default','explicit','whitespace','hash-fail','source-fail','runtime-fail','shader-fail','architecture-fail')) {
    $package=Join-Path $EvidenceRoot "$(Split-Path $shell -Leaf) $case package with spaces"
    Copy-Item -LiteralPath $PackageRoot -Destination $package -Recurse
    foreach($file in @('pyrowave-vulkan-stage4.exe','pyrowave-vulkan-stage4-policy-tests.exe','pyrowave-vulkan-stage4-resource-tests.exe','pyrowave-factory-cleanup-tests.exe')) { Copy-Item $FixtureExe (Join-Path $package $file) -Force }
    $runtimePath=Join-Path $package 'install/bin/pyrowave-runtime.json'; $runtime=Get-Content $runtimePath -Raw | ConvertFrom-Json
    $factoryPath=Join-Path $package 'install/bin/factory-ownership.json'; $factory=Get-Content $factoryPath -Raw | ConvertFrom-Json
    $factory.testBinarySha256=(Get-FileHash $FixtureExe).Hash.ToLowerInvariant(); $factory | ConvertTo-Json -Depth 8 | Set-Content $factoryPath -Encoding utf8
    $runtime.factoryOwnershipSha256=(Get-FileHash $factoryPath).Hash.ToLowerInvariant()
    if($case -eq 'runtime-fail') { $runtime.codecCommit='BAD_PIN' }
    $runtime | ConvertTo-Json -Depth 8 | Set-Content $runtimePath -Encoding utf8
    if($case -eq 'source-fail') { $metadata=Get-Content (Join-Path $package 'build.json') -Raw | ConvertFrom-Json; $metadata.sourceRevision='0000000000000000000000000000000000000000'; $metadata | ConvertTo-Json -Depth 10 | Set-Content (Join-Path $package 'build.json') }
    if($case -eq 'architecture-fail') { $metadata=Get-Content (Join-Path $package 'build.json') -Raw | ConvertFrom-Json; $metadata.architecture=if($build.architecture -eq 'arm64') { 'x64' } else { 'arm64' }; $metadata | ConvertTo-Json -Depth 10 | Set-Content (Join-Path $package 'build.json') }
    if($case -eq 'shader-fail') { $metadata=Get-Content (Join-Path $package 'shaders/shader-provenance.json') -Raw | ConvertFrom-Json; $metadata.embeddedHeaderSha256='BAD'; $metadata | ConvertTo-Json -Depth 8 | Set-Content (Join-Path $package 'shaders/shader-provenance.json') }
    Get-ChildItem $package -Recurse -File | Where-Object Name -ne 'sha256.json' | ForEach-Object { @{file=$_.FullName.Substring($package.Length+1);sha256=(Get-FileHash $_.FullName).Hash.ToLowerInvariant()} } | ConvertTo-Json | Set-Content (Join-Path $package 'sha256.json') -Encoding utf8
    if($case -eq 'hash-fail') { Add-Content (Join-Path $package 'build.json') 'TAMPER' }
    $root=Join-Path $package 'stage4-evidence'; $arguments=@('-NoProfile','-ExecutionPolicy','Bypass','-File',(Join-Path $package 'run-stage4-owner-tests.ps1'),'-NonInteractive')
    if($case -eq 'explicit') { $root=Join-Path $EvidenceRoot "$(Split-Path $shell -Leaf) explicit evidence with spaces"; $arguments+=@('-EvidenceRoot',$root) }
    if($case -eq 'whitespace') { $arguments+=@('-EvidenceRoot','   ') }
    Push-Location $unrelated
    try { $output=& $shell @arguments 2>&1 | Out-String; $code=$LASTEXITCODE } finally { Pop-Location }
    $output | Set-Content (Join-Path $EvidenceRoot "$(Split-Path $shell -Leaf)-$case.log")
    $record=Get-Content (Join-Path $root 'owner-result.json') -Raw | ConvertFrom-Json
    if($case -in @('default','explicit','whitespace')) { if($code -ne 77 -or $record.stage4Qualification -cne 'SKIP' -or $record.ownerVisualConfirmed -ne $false) { throw "Runner SKIP regression: $case / $shell" } }
    elseif($code -ne 1 -or $record.stage4Qualification -cne 'FAIL') { throw "Runner rejection regression: $case / $shell" }
} }
# Independently validate all hard numerical/ownership/visible gates without a GPU.
. (Join-Path $PackageRoot 'stage4-evidence-policy.ps1')
$cases=@(); foreach($range in @('FULL','LIMITED')) { foreach($filter in @('NEAREST','LINEAR')) { foreach($pattern in @('range','bt709-bars','centered-chroma','geometry','gradient')) { foreach($extent in @(@(1920,1080),@(1001,751),@(1801,700),@(127,93))) { $cases+=@{range=$range;filter=$filter;pattern=$pattern;width=$extent[0];height=$extent[1];maxError=1;failedComponents=0} } } } }
$shader=@{pass=$true;rgbTolerance=1;maxError=1;chromaNegativeControlPixels=99;cases=$cases}
$api=@{result='API_PASS';apiPass=$true;sourceRevision=$build.sourceRevision;architecture=$build.architecture;borrowedDeviceMatch=$true;cpuYuvReadbackInPresentation=$false;externalMemoryHandles=0;externalSemaphoreHandles=0;d3d11Resources=0;presentationBackend='raw Vulkan';presentationPath='GPU_DECODE_CALLER_YUV_SHADER_SWAPCHAIN';shaderVerificationPass=$true;chromaSitingPass=$true;validationErrors=0;validationStatus='SKIP';maxRgbError=1;verifyOnly=$false;fixturePresents=@(1..10);decodedFrames=300;swapchainRecreationCount=25}
$life=@{cleanupPass=$true;queueLockBalanced=$true;hidden=$false;automaticSteps=40;recreations=25}
Assert-Stage4Api $api $shader $life $build.sourceRevision $build.architecture $true
foreach($field in @('borrowedDeviceMatch','cpuYuvReadbackInPresentation','externalMemoryHandles','externalSemaphoreHandles','d3d11Resources','shaderVerificationPass','chromaSitingPass','validationErrors','maxRgbError','swapchainRecreationCount','fixturePresents')) {
    $bad=$api.Clone(); $bad[$field]=switch($field) { 'borrowedDeviceMatch' {$false}; 'shaderVerificationPass' {$false}; 'chromaSitingPass' {$false}; 'cpuYuvReadbackInPresentation' {$true}; 'maxRgbError' {2}; 'swapchainRecreationCount' {19}; 'fixturePresents' {@(0,1,2,3,4,5,6,7,8,9)}; default {1} }
    $rejected=$false; try { Assert-Stage4Api $bad $shader $life $build.sourceRevision $build.architecture $true } catch { $rejected=$true }; if(!$rejected) { throw "Evidence gate accepted $field violation" }
}
$badShader=$shader.Clone(); $badShader.cases=@($cases[0]) * 80; $rejected=$false
try { Assert-Stage4Api $api $badShader $life $build.sourceRevision $build.architecture $true } catch { $rejected=$true }; if(!$rejected) { throw 'Duplicate shader cases accepted' }
@{result='PASS';shells=$shells;packageCases=8;ownershipNumericalLifecycleCases=12;visibleOwnerPassGenerated=$false} | ConvertTo-Json -Depth 5 | Set-Content (Join-Path $EvidenceRoot 'runner-tests.json') -Encoding utf8
