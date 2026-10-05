[CmdletBinding()]
param([Parameter(Mandatory)][string]$Executable, [Parameter(Mandatory)][string]$EvidenceRoot)
$ErrorActionPreference = 'Stop'
$Executable = (Resolve-Path -LiteralPath $Executable).Path
New-Item -ItemType Directory -Force -Path $EvidenceRoot | Out-Null
$EvidenceRoot = (Resolve-Path -LiteralPath $EvidenceRoot).Path
$results = @()
$cases = @(
    @{name='flagged-smoke'; args=@('--hidden','--seconds','2')},
    @{name='unflagged-recreate'; args=@('--hidden','--without-vulkan-flag','--exercise','--seconds','6')},
    @{name='no-vsync'; args=@('--hidden','--no-vsync','--seconds','2')},
    @{name='api-1-0'; args=@('--hidden','--api-1-0','--seconds','2')}
)
foreach ($checkpoint in @('instance','surface','device','command-pool','command-resources','swapchain','render-pass','image-view','framebuffer','present-semaphore')) {
    $cases += @{name="failure-$checkpoint"; args=@('--hidden','--seconds','1','--fail-at',$checkpoint)}
}
foreach ($exportName in @('vkCreateDevice','vkCreateSwapchainKHR')) {
    $cases += @{name="missing-$exportName"; args=@('--hidden','--seconds','1','--missing-export',$exportName)}
}
foreach ($case in $cases) {
    $log = Join-Path $EvidenceRoot "$($case.name).log"
    & $Executable @($case.args) --log $log | Out-Host
    $code = $LASTEXITCODE
    $results += [pscustomobject]@{name=$case.name; exitCode=$code; status=$(if ($code -eq 77) {'SKIP'} elseif (!$code) {'API_PASS'} else {'FAIL'}); visualQualification='NOT_AUTOMATED'}
    $results | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $EvidenceRoot 'gpu-results.json')
    if ($code -ne 0 -and $code -ne 77) { throw "Vulkan probe case failed: $($case.name) ($code)" }
}
