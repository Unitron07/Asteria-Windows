[CmdletBinding()]
param([Parameter(Mandatory)][string]$PackageRoot,[Parameter(Mandatory)][string]$EvidenceRoot)
$ErrorActionPreference = 'Stop'
$PackageRoot = (Resolve-Path -LiteralPath $PackageRoot).Path
$runner = Join-Path $PackageRoot 'run-stage3-owner-tests.ps1'
if ((Get-Content -LiteralPath $runner -Raw) -match '(?i)[a-z]:[\\/]|/users/|/home/') { throw 'Runner contains developer-local paths' }
New-Item -ItemType Directory -Force -Path $EvidenceRoot | Out-Null
$EvidenceRoot = (Resolve-Path -LiteralPath $EvidenceRoot).Path
$unrelated = Join-Path $EvidenceRoot 'unrelated working directory'
New-Item -ItemType Directory -Force -Path $unrelated | Out-Null
$fixture = Join-Path $EvidenceRoot 'Unavailable.cs'
@'
using System; using System.IO;
public static class Fixture {
 public static int Main(string[] args) {
  if (Environment.GetCommandLineArgs()[0].Contains("policy-tests")) return 0;
  string log=null;
  for(int i=0;i<args.Length;i++) {
   if((args[i]=="--runtime" || args[i]=="--log") && i+1<args.Length) {
    if(args[i]=="--log") log=args[i+1];
    i++;
   } else return 2;
  }
  if(log==null) return 2;
  File.WriteAllText(log,"validation_status=SKIP\noverall=SKIP\n");
  return 77;
 }
}
'@ | Set-Content -LiteralPath $fixture
$csc = Join-Path ([Environment]::GetFolderPath('Windows')) 'Microsoft.NET/Framework64/v4.0.30319/csc.exe'
$fixtureExe = Join-Path $EvidenceRoot 'fixture.exe'
& $csc /nologo /platform:x64 /target:exe "/out:$fixtureExe" $fixture
if ($LASTEXITCODE) { throw 'Runner regression fixture compile failed' }
$shells = @((Get-Command powershell.exe -ErrorAction Stop).Source)
$pwsh = Get-Command pwsh.exe -ErrorAction SilentlyContinue
if ($pwsh) { $shells += $pwsh.Source }
foreach ($shell in $shells) {
    foreach ($case in @('default','explicit','whitespace')) {
        $package = Join-Path $EvidenceRoot ((Split-Path $shell -Leaf) + " $case package with spaces")
        New-Item -ItemType Directory -Force -Path $package,(Join-Path $package 'install/bin') | Out-Null
        foreach ($name in @('run-stage3-owner-tests.ps1','runtime-provenance.ps1','pe-machine.ps1')) {
            Copy-Item -LiteralPath (Join-Path $PackageRoot $name) -Destination $package
        }
        foreach ($name in @('pyrowave-vulkan-shared-device.exe','pyrowave-vulkan-shared-device-policy-tests.exe')) {
            Copy-Item -LiteralPath $fixtureExe -Destination (Join-Path $package $name)
        }
        $runtime = Join-Path $package 'install/bin/libpyrowave-shared-0.dll'
        Copy-Item -LiteralPath $fixtureExe -Destination $runtime
        @{architecture='x64';codecCommit='186f0393b77f7755953b5ecde994bb1cec2e4155';bitstreamId='186f0393';apiVersion='0.6.0';
            sha256=(Get-FileHash -LiteralPath $runtime).Hash.ToLowerInvariant()} |
            ConvertTo-Json | Set-Content -LiteralPath (Join-Path $package 'install/bin/pyrowave-runtime.json')
        @{architecture='x64';sourceRevision='GPU_FREE_FIXTURE'} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $package 'build.json')
        Get-ChildItem -LiteralPath $package -Recurse -File | ForEach-Object {
            @{file=$_.FullName.Substring($package.Length+1);sha256=(Get-FileHash -LiteralPath $_.FullName).Hash.ToLowerInvariant()}
        } | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $package 'sha256.json')
        $expected = Join-Path $package 'stage3-evidence'
        $arguments = @('-NoProfile','-ExecutionPolicy','Bypass','-File',(Join-Path $package 'run-stage3-owner-tests.ps1'))
        if ($case -eq 'explicit') { $expected = Join-Path $EvidenceRoot ((Split-Path $shell -Leaf) + ' explicit evidence'); $arguments += @('-EvidenceRoot',$expected) }
        if ($case -eq 'whitespace') { $arguments += @('-EvidenceRoot','   ') }
        $start = New-Object Diagnostics.ProcessStartInfo
        $start.FileName = $shell; $start.Arguments = ($arguments | ForEach-Object { '"' + $_ + '"' }) -join ' '
        $start.WorkingDirectory = $unrelated; $start.UseShellExecute = $false; $start.CreateNoWindow = $true
        if ((Split-Path $shell -Leaf) -ieq 'powershell.exe') {
            # Match native PowerShell 7 -> Windows PowerShell launching, which
            # strips the host kit's incompatible module directories.
            $start.EnvironmentVariables['PSModulePath'] = Join-Path ([Environment]::GetFolderPath('Windows')) 'System32/WindowsPowerShell/v1.0/Modules'
        }
        $start.RedirectStandardOutput = $true; $start.RedirectStandardError = $true
        $process = [Diagnostics.Process]::Start($start)
        $stdout = $process.StandardOutput.ReadToEndAsync(); $stderr = $process.StandardError.ReadToEndAsync()
        if (!$process.WaitForExit(30000)) { $process.Kill(); throw 'Runner regression timeout' }
        $code = $process.ExitCode; $process.Dispose()
        if ($code -ne 77) { throw "Runner SKIP fixture returned ${code}: $($stdout.Result) $($stderr.Result)" }
        $record = Get-Content -LiteralPath (Join-Path $expected 'owner-result.json') -Raw | ConvertFrom-Json
        if ($record.overall -cne 'SKIP' -or $record.sourceRevision -cne 'GPU_FREE_FIXTURE') { throw 'Runner evidence mismatch' }
        if (Test-Path -LiteralPath (Join-Path $unrelated 'stage3-evidence')) { throw 'Runner wrote relative to CWD' }
    }
}
Write-Output 'PASS: actual packaged Stage 3 runner, Windows PowerShell 5.1/pwsh, unrelated CWD, spaces/default/explicit roots, explicit SKIP'
