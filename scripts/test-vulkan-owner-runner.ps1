[CmdletBinding()]
param([string]$RunnerPath, [string]$EvidenceRoot)
$ErrorActionPreference = 'Stop'
if ([string]::IsNullOrWhiteSpace($RunnerPath)) {
    $RunnerPath = Join-Path (Split-Path $PSScriptRoot -Parent) 'tests/pyrowave/vulkan/run-owner-tests.ps1'
}
$RunnerPath = (Resolve-Path -LiteralPath $RunnerPath).Path
if ((Get-Content -LiteralPath $RunnerPath -Raw) -match '(?i)[a-z]:[\\/]|/users/|/home/') {
    throw 'Packaged runner contains an absolute developer-local path'
}
if ([string]::IsNullOrWhiteSpace($EvidenceRoot)) {
    $EvidenceRoot = Join-Path ([IO.Path]::GetTempPath()) ('asteria-vulkan-runner-' + [guid]::NewGuid().ToString('N'))
}
New-Item -ItemType Directory -Force -Path $EvidenceRoot | Out-Null
$EvidenceRoot = (Resolve-Path -LiteralPath $EvidenceRoot).Path
$unrelated = Join-Path $EvidenceRoot 'unrelated working directory'
New-Item -ItemType Directory -Force -Path $unrelated | Out-Null

function Invoke-Shell([string]$Shell, [string[]]$Arguments) {
    $start = New-Object Diagnostics.ProcessStartInfo
    $start.FileName = $Shell
    $start.Arguments = ($Arguments | ForEach-Object { '"' + $_ + '"' }) -join ' '
    $start.WorkingDirectory = $unrelated
    $start.UseShellExecute = $false
    $start.CreateNoWindow = $true
    $start.RedirectStandardOutput = $true
    $start.RedirectStandardError = $true
    $process = [Diagnostics.Process]::Start($start)
    $stdout = $process.StandardOutput.ReadToEndAsync()
    $stderr = $process.StandardError.ReadToEndAsync()
    if (!$process.WaitForExit(30000)) { $process.Kill(); throw 'Runner regression child timed out' }
    $result = @{exitCode=$process.ExitCode;output=$stdout.Result + $stderr.Result}
    $process.Dispose()
    return $result
}

# A CPU-only executable intentionally reports unavailable. This exercises the
# real -File runner without a GPU, prompts or fabricated owner confirmation.
$windowsPowerShell = (Get-Command powershell.exe -ErrorAction Stop).Source
$fixture = Join-Path $EvidenceRoot 'pyrowave-vulkan-probe.exe'
$compileScript = Join-Path $EvidenceRoot 'compile-fixture.ps1'
@'
param([string]$Output)
$ErrorActionPreference = 'Stop'
Add-Type -OutputAssembly $Output -OutputType ConsoleApplication -TypeDefinition @"
using System.IO;
public static class UnavailableProbe {
    public static int Main(string[] args) {
        for (int i = 0; i + 1 < args.Length; ++i)
            if (args[i] == "--log") File.WriteAllText(args[i + 1], "validation_status=SKIP\n");
        return 77;
    }
}
"@
'@ | Set-Content -LiteralPath $compileScript
$compiled = Invoke-Shell $windowsPowerShell @('-NoProfile','-ExecutionPolicy','Bypass','-File',$compileScript,'-Output',$fixture)
if ($compiled.exitCode -ne 0 -or !(Test-Path -LiteralPath $fixture)) { throw "Fixture compile failed: $($compiled.output)" }
$shells = @(@{name='windows-powershell';path=$windowsPowerShell})
$pwsh = Get-Command pwsh.exe -ErrorAction SilentlyContinue
if ($pwsh) { $shells += @{name='pwsh';path=$pwsh.Source} }
else { Write-Output 'SKIP pwsh runner regression: shell unavailable' }
$results = @()
foreach ($shell in $shells) {
    foreach ($case in @('default','explicit','whitespace')) {
        $package = Join-Path $EvidenceRoot "$($shell.name) $case package"
        New-Item -ItemType Directory -Path $package | Out-Null
        $runner = Join-Path $package 'run-owner-tests.ps1'
        Copy-Item -LiteralPath $RunnerPath -Destination $runner
        Copy-Item -LiteralPath $fixture -Destination (Join-Path $package 'pyrowave-vulkan-probe.exe')
        $expected = Join-Path $package 'owner-evidence'
        $arguments = @('-NoProfile','-ExecutionPolicy','Bypass','-File',$runner)
        if ($case -eq 'explicit') {
            $expected = Join-Path $EvidenceRoot "$($shell.name) explicit evidence"
            $arguments += @('-EvidenceRoot',$expected)
        } elseif ($case -eq 'whitespace') { $arguments += @('-EvidenceRoot','   ') }
        $child = Invoke-Shell $shell.path $arguments
        $child.output | Set-Content -LiteralPath (Join-Path $package 'child-output.txt')
        $json = Join-Path $expected 'owner-result.json'
        if (!(Test-Path -LiteralPath $json)) { throw "Evidence resolution failed ($($shell.name)/$case): $($child.output)" }
        $record = Get-Content -LiteralPath $json -Raw | ConvertFrom-Json
        if ($child.exitCode -eq 0 -or $record.status -ne 'SKIP' -or $record.exitCode -ne 77 -or $record.stage3Authorized) {
            throw "Fixture unavailable result was misreported ($($shell.name)/$case)"
        }
        if (!(Test-Path -LiteralPath (Join-Path $expected 'flagged.log')) -or
            (Test-Path -LiteralPath (Join-Path $unrelated 'owner-evidence'))) {
            throw 'Runner used the working directory instead of its package location'
        }
        if ($case -eq 'explicit' -and (Test-Path -LiteralPath (Join-Path $package 'owner-evidence'))) {
            throw 'Explicit evidence directory was ignored'
        }
        $results += @{shell=$shell.name;case=$case;status='PASS';probeStatus='SKIP fixture';gpuRequired=$false}
        Write-Output "PASS owner runner: $($shell.name)/$case"
    }
}
$results | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $EvidenceRoot 'runner-results.json')
Write-Output 'PASS GPU-free packaged owner runner directory regression'
