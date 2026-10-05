. (Join-Path $PSScriptRoot 'pe-machine.ps1')
function Assert-PyroWaveRuntime([string]$DependencyRoot, [string]$Architecture) {
    $runtime = Join-Path $DependencyRoot 'install/bin/libpyrowave-shared-0.dll'
    Assert-PyroWavePe $runtime $Architecture | Out-Null
    $metadata = Get-Content (Join-Path $DependencyRoot 'install/bin/pyrowave-runtime.json') -Raw | ConvertFrom-Json
    if ($metadata.architecture -cne $Architecture -or
        $metadata.codecCommit -cne '186f0393b77f7755953b5ecde994bb1cec2e4155' -or
        $metadata.bitstreamId -cne '186f0393' -or $metadata.apiVersion -cne '0.6.0' -or
        $metadata.sha256 -cne (Get-FileHash -LiteralPath $runtime -Algorithm SHA256).Hash.ToLowerInvariant()) {
        throw 'PyroWave runtime provenance mismatch'
    }
    return $metadata
}
