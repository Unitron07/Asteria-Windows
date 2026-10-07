function Assert-Stage4Api($Api,$Shader,$Lifecycle,[string]$Revision,[string]$Architecture,[bool]$Visible) {
    if ($Api.result -cne 'API_PASS' -or $Api.apiPass -ne $true -or $Api.sourceRevision -cne $Revision -or $Api.architecture -cne $Architecture -or
        $Api.borrowedDeviceMatch -ne $true -or $Api.cpuYuvReadbackInPresentation -ne $false -or
        $Api.externalMemoryHandles -ne 0 -or $Api.externalSemaphoreHandles -ne 0 -or $Api.d3d11Resources -ne 0 -or
        $Api.presentationBackend -cne 'raw Vulkan' -or $Api.presentationPath -cne 'GPU_DECODE_CALLER_YUV_SHADER_SWAPCHAIN' -or
        $Api.shaderVerificationPass -ne $true -or $Api.chromaSitingPass -ne $true -or $Api.validationErrors -ne 0 -or
        $Api.validationStatus -cnotin @('SKIP','NO_REPORTED_ERRORS') -or $Api.maxRgbError -gt 1 -or $Api.maxRgbError -lt 0 -or
        $Lifecycle.cleanupPass -ne $true -or $Lifecycle.queueLockBalanced -ne $true) { throw 'Stage 4 API/ownership/color/lifetime evidence rejected' }
    if ($Shader.pass -ne $true -or $Shader.rgbTolerance -ne 1 -or $Shader.maxError -gt 1 -or $Shader.maxError -lt 0 -or
        $Shader.chromaNegativeControlPixels -le 0 -or @($Shader.cases).Count -ne 80) { throw 'Stage 4 shader evidence rejected' }
    $keys=@{}
    foreach ($case in $Shader.cases) {
        if ($case.maxError -lt 0 -or $case.maxError -gt 1 -or $case.failedComponents -ne 0 -or
            $case.range -cnotin @('FULL','LIMITED') -or $case.filter -cnotin @('NEAREST','LINEAR') -or
            $case.pattern -cnotin @('range','bt709-bars','centered-chroma','geometry','gradient') -or
            "$($case.width)x$($case.height)" -cnotin @('1920x1080','1001x751','1801x700','127x93')) { throw 'Shader case rejected' }
        $key="$($case.range)/$($case.filter)/$($case.pattern)/$($case.width)x$($case.height)"
        if ($keys.ContainsKey($key)) { throw 'Duplicate shader case' }; $keys[$key]=$true
    }
    if ($Visible -and ($Api.verifyOnly -ne $false -or @($Api.fixturePresents).Count -ne 10 -or
        @($Api.fixturePresents | Where-Object { $_ -le 0 }).Count -ne 0 -or $Api.decodedFrames -lt 30 -or
        $Api.swapchainRecreationCount -lt 20 -or $Lifecycle.hidden -ne $false -or $Lifecycle.automaticSteps -lt 30)) {
        throw 'Visible sequence/lifecycle evidence incomplete'
    }
}
