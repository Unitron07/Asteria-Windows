[CmdletBinding()]
param(
    [ValidateSet('x64','arm64')][string[]]$Architecture = @('x64','arm64'),
    [string]$OutputRoot = (Join-Path (Split-Path $PSScriptRoot -Parent) 'build/pyrowave'),
    [string]$CMake = 'cmake',
    [string]$Generator,
    [switch]$UnpatchedArm64
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'pyrowave/pe-machine.ps1')
$lock = Get-Content (Join-Path $PSScriptRoot 'pyrowave/dependencies.json') -Raw | ConvertFrom-Json
$OutputRoot = [IO.Path]::GetFullPath($OutputRoot)
New-Item -ItemType Directory -Force $OutputRoot | Out-Null

function Invoke-Native([string]$Tool, [string[]]$Arguments) {
    & $Tool @Arguments | Out-Host
    if ($LASTEXITCODE -ne 0) { throw "$Tool failed ($LASTEXITCODE): $($Arguments -join ' ')" }
}
function Get-Git([string]$Path, [string[]]$Arguments) {
    $result = & git -C $Path @Arguments
    if ($LASTEXITCODE -ne 0) { throw "git failed in $Path" }
    return $result
}
function Assert-Revision([string]$Path, [string]$Commit) {
    $actual = Get-Git $Path @('rev-parse','HEAD')
    if ($actual.Trim() -cne $Commit) { throw "Revision mismatch in ${Path}: $actual != $Commit" }
}
function Fetch-Pinned([string]$Path, $Pin, [switch]$Codec) {
    if (Test-Path (Join-Path $Path '.git')) {
        Assert-Revision $Path $Pin.commit
        if (Get-Git $Path @('status','--porcelain','--untracked-files=no')) {
            throw "Dirty pinned dependency: $Path; use a fresh OutputRoot for a patched rebuild"
        }
        return 'existing verified checkout'
    }
    New-Item -ItemType Directory -Force $Path | Out-Null
    Invoke-Native git @('init',$Path)
    Invoke-Native git @('-C',$Path,'remote','add','origin',$Pin.url)
    & git -C $Path fetch --depth 1 origin $Pin.commit | Out-Host
    $origin = $Pin.url
    if ($LASTEXITCODE -ne 0) {
        if (!$Codec) { throw "Cannot fetch pinned dependency: $($Pin.url)" }
        $bundle = Join-Path $PSScriptRoot "pyrowave/$($lock.sourceBundle.file)"
        if (!(Test-Path -LiteralPath $bundle) -or
            (Get-FileHash -LiteralPath $bundle -Algorithm SHA256).Hash.ToLowerInvariant() -cne $lock.sourceBundle.sha256) {
            throw 'Pinned codec unavailable and recovery bundle missing/hash mismatch'
        }
        Invoke-Native git @('-C',$Path,'bundle','verify',$bundle)
        Invoke-Native git @('-C',$Path,'fetch',$bundle,'HEAD')
        $origin = "verified bundle SHA256=$($lock.sourceBundle.sha256)"
    }
    Invoke-Native git @('-C',$Path,'checkout','--detach',$Pin.commit)
    Assert-Revision $Path $Pin.commit
    if (Get-Git $Path @('status','--porcelain','--untracked-files=no')) { throw "Dirty new checkout: $Path" }
    return $origin
}

foreach ($arch in $Architecture) {
    # Each architecture owns source, CMake cache, binaries, install and evidence.
    $root = Join-Path $OutputRoot $arch
    $source = Join-Path $root 'source'
    $build = Join-Path $root 'build'
    $install = Join-Path $root 'install'
    $evidence = Join-Path $root 'evidence'
    New-Item -ItemType Directory -Force $evidence | Out-Null
    Start-Transcript -Path (Join-Path $evidence 'dependency.log') -Force | Out-Null
    $phase = 'fetch'
    try {
        $codecOrigin = Fetch-Pinned $source $lock.pyrowave -Codec
        $granite = Join-Path $source 'Granite'
        $null = Fetch-Pinned $granite $lock.granite
        foreach ($entry in @(@('third_party/volk',$lock.volk),
                            @('third_party/khronos/vulkan-headers',$lock.vulkanHeaders))) {
            $path = $entry[0]; $pin = $entry[1]
            $gitlink = Get-Git $granite @('ls-tree','HEAD',$path)
            if ($gitlink -notmatch "^160000 commit $($pin.commit)\s") { throw "Granite gitlink mismatch: $path" }
            Invoke-Native git @('-C',$granite,'submodule','update','--init','--depth','1','--',$path)
            Assert-Revision (Join-Path $granite $path) $pin.commit
            if (Get-Git (Join-Path $granite $path) @('status','--porcelain')) { throw "Dirty submodule: $path" }
        }
        $hashes = @()
        foreach ($entry in @(@('pyrowave',$source),@('granite',$granite),
                            @('volk',(Join-Path $granite 'third_party/volk')),
                            @('vulkanHeaders',(Join-Path $granite 'third_party/khronos/vulkan-headers')))) {
            $archive = Join-Path $evidence "$($entry[0])-source.tar"
            Invoke-Native git @('-C',$entry[1],'archive','--format=tar',"--output=$archive",'HEAD')
            $hashes += [pscustomobject]@{component=$entry[0]; commit=(Get-Git $entry[1] @('rev-parse','HEAD'));
                tree=(Get-Git $entry[1] @('rev-parse','HEAD^{tree}'));
                sourceArchiveSha256=(Get-FileHash -LiteralPath $archive).Hash.ToLowerInvariant()}
        }
        $hashes | ConvertTo-Json | Set-Content (Join-Path $evidence 'sources.json') -Encoding utf8
        $patchHash = $null
        if ($arch -eq 'arm64' -and !$UnpatchedArm64) {
            $patch = Join-Path $PSScriptRoot 'pyrowave/granite-msvc-arm64-portable-math.patch'
            Invoke-Native git @('-C',$granite,'apply','--check',$patch)
            Invoke-Native git @('-C',$granite,'apply',$patch)
            $patchHash = (Get-FileHash -LiteralPath $patch).Hash.ToLowerInvariant()
            Get-Git $granite @('diff') | Set-Content (Join-Path $evidence 'granite-patch.diff')
        }
        $phase = 'configure'
        if (!$Generator) {
            $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
            if (!(Test-Path $vswhere)) { throw 'MSVC/Visual Studio Installer is required' }
            $vsVersion = & $vswhere -latest -property installationVersion
            if ($LASTEXITCODE -ne 0 -or !$vsVersion) { throw 'No Visual Studio installation found' }
            $Generator = switch (([version]$vsVersion).Major) {
                17 { 'Visual Studio 17 2022' }
                18 { 'Visual Studio 18 2026' }
                default { throw "Select -Generator explicitly for Visual Studio $vsVersion" }
            }
        }
        $cmakePath = (Get-Command $CMake -ErrorAction Stop).Source
        $cmakeVersion = & $cmakePath --version
        if ($LASTEXITCODE -ne 0 -or $cmakeVersion[0] -notmatch 'cmake version (\d+\.\d+\.\d+)') { throw 'Unable to read CMake version' }
        if ([version]$Matches[1] -lt [version]'3.27') { throw 'CMake >= 3.27 required' }
        $target = if ($arch -eq 'arm64') { 'ARM64' } else { 'x64' }
        $options = @('-DPYROWAVE_DEVEL=OFF','-DPYROWAVE_UTILS=OFF','-DGRANITE_SHARED=OFF',
                     '-DGRANITE_TARGET_NATIVE=OFF','-DPYROWAVE_FP32_STORAGE=OFF','-DPYROWAVE_FP32_MATH=ON')
        Invoke-Native $cmakePath (@('-S',$source,'-B',$build,'-G',$Generator,'-A',$target,
            "-DCMAKE_INSTALL_PREFIX=$install") + $options)
        Copy-Item (Join-Path $build 'CMakeCache.txt') (Join-Path $evidence 'CMakeCache.txt')
        $compilerFile = @(Get-ChildItem (Join-Path $build 'CMakeFiles') -Filter CMakeCXXCompiler.cmake -Recurse)
        if ($compilerFile.Count -ne 1) { throw 'Expected one CMake compiler record' }
        $compilerText = Get-Content $compilerFile[0].FullName -Raw
        if ($compilerText -notmatch 'set\(CMAKE_CXX_COMPILER "([^"]+)"\)') { throw 'CMake compiler path missing' }
        $compiler = $Matches[1]
        Copy-Item $compilerFile[0].FullName (Join-Path $evidence 'compiler.cmake')
        if ($compilerText -notmatch 'set\(CMAKE_CXX_COMPILER_ID "MSVC"\)') { throw 'MSVC is required' }
        $dumpbin = Join-Path (Split-Path $compiler -Parent) 'dumpbin.exe'
        & $compiler /Bv 2>&1 | Set-Content (Join-Path $evidence 'compiler.txt')
        [pscustomobject]@{architecture=$arch; configuration='MSVC Release'; codecOrigin=$codecOrigin;
            cmake=$cmakeVersion; cmakePath=$cmakePath; cmakeSha256=(Get-FileHash $cmakePath).Hash;
            compiler=$compiler; compilerSha256=(Get-FileHash $compiler).Hash;
            windowsSdk=(@(([xml](Get-Content (Join-Path $build 'pyrowave-shared.vcxproj') -Raw)).Project.PropertyGroup | ForEach-Object { $_.WindowsTargetPlatformVersion } | Where-Object { $_ }) | Select-Object -First 1);
            options=$options; patchSha256=$patchHash; os=[Environment]::OSVersion.VersionString;
            workflowRun=$env:GITHUB_RUN_ID} | ConvertTo-Json -Depth 4 |
            Set-Content (Join-Path $evidence 'build.json') -Encoding utf8
        $phase = 'compile/link'
        Invoke-Native $cmakePath @('--build',$build,'--config','Release','--target','pyrowave-shared','--parallel','2')
        $phase = 'stage/inventory'
        $dll = @(Get-ChildItem $build -Recurse -Filter libpyrowave-shared-0.dll)
        $lib = @(Get-ChildItem $build -Recurse -Filter '*pyrowave-shared*.lib')
        if ($dll.Count -ne 1 -or $lib.Count -ne 1) { throw 'Expected one runtime DLL and one import library' }
        # Stage only the minimal built target. Upstream install also requires unrelated tools.
        foreach ($dir in @('bin','lib','include/pyrowave','source-notices')) {
            New-Item -ItemType Directory -Force (Join-Path $install $dir) | Out-Null
        }
        Copy-Item $dll[0].FullName (Join-Path $install 'bin/libpyrowave-shared-0.dll')
        Copy-Item $lib[0].FullName (Join-Path $install "lib/$($lib[0].Name)")
        Copy-Item (Join-Path $source 'pyrowave.h') (Join-Path $install 'include/pyrowave/pyrowave.h')
        foreach ($entry in @(@('pyrowave',$source),@('granite',$granite),
                            @('volk',(Join-Path $granite 'third_party/volk')),
                            @('vulkanHeaders',(Join-Path $granite 'third_party/khronos/vulkan-headers')))) {
            $dest = Join-Path $install "source-notices/$($entry[0])"
            New-Item -ItemType Directory -Force $dest | Out-Null
            Get-ChildItem $entry[1] -Filter 'LICENSE*' | Copy-Item -Destination $dest
        }
        Copy-Item (Join-Path $PSScriptRoot 'pyrowave/dependencies.json') (Join-Path $install 'source-notices')
        $runtime = Join-Path $install 'bin/libpyrowave-shared-0.dll'
        Assert-PyroWavePe $runtime $arch | ConvertTo-Json | Set-Content (Join-Path $evidence 'runtime-pe.json')
        foreach ($kind in @('headers','imports','dependents','exports')) {
            & $dumpbin "/$kind" $runtime | Set-Content (Join-Path $evidence "runtime-$kind.txt")
            if ($LASTEXITCODE -ne 0) { throw "dumpbin /$kind failed" }
        }
        Get-ChildItem $install -File -Recurse | Get-FileHash -Algorithm SHA256 |
            Select-Object Path,Hash | ConvertTo-Json | Set-Content (Join-Path $evidence 'install-sha256.json')
        [pscustomobject]@{passed=$true; architecture=$arch; phase='built; not runtime-qualified'} |
            ConvertTo-Json | Set-Content (Join-Path $evidence 'result.json')
    } catch {
        [pscustomobject]@{passed=$false; architecture=$arch; phase=$phase; reason=$_.Exception.Message} |
            ConvertTo-Json | Set-Content (Join-Path $evidence 'result.json')
        throw
    } finally { Stop-Transcript | Out-Null }
}
