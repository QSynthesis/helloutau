param(
    [string]$BuildDir,
    [string]$OutputDir,
    [string]$Iscc,
    [switch]$NoInstaller
)
# Packages a release build of HelloUtau for Windows into the release assets of
# docs/Distribution.md, in $OutputDir:
#
#     helloutau-<version>-<triplet>.zip         the portable package, a folder HelloUtau
#     helloutau-<version>-<triplet>-setup.exe   the installer, compiled from setup.iss
#     helloutau-<version>-<triplet>-symbols.zip the PDBs of the binaries, in the layout of the
#                                               package, if the build records them
#     SHA256SUMS                                the digests of the files above, as sha256sum
#                                               writes them
#
# A build of another channel than release (HELLO_BUILD_CHANNEL) follows the version with
# -<channel>.<commit>, such as helloutau-<version>-ci.1a2b3c4-<triplet>.zip, as other projects
# follow the version with the identifier of the build.
#
# The installation is laid out as
#
#     bin/                    helloutau.exe, the libraries of the application, of Qt and of the
#                             other dependencies, and the C++ runtime
#     lib/helloutau/plugins/  the plugins of the application, where cmake --install puts them
#     lib/Qt/plugins/         the plugins of Qt, which AppLoader::addQtPluginPaths() adds
#     share/helloutau/translations/
#                             the translations of the libraries, installed by
#                             helloutau/CMakeLists.txt
#     share/Qt/translations/  the translations of Qt, installed by helloutau/CMakeLists.txt
#     share/helloutau/update/ UpdateManifest.json, installed by helloutau/CMakeLists.txt
#     share/helloutau/LICENSE the license, installed by helloutau/CMakeLists.txt, which the
#                             installer shows
#
# The build must be complete: this script does not build, because building requires the
# environment of the compiler. Steps: cmake --install, which deploys Qt and the other libraries
# (HELLO_DEPLOY, see helloutau/CMakeLists.txt), remove the development files, copy the C++
# runtime of MSVC, archive, and compile the installer with Inno Setup 7.
#
#     powershell -File dist\installer\windows\package.ps1 [-BuildDir <dir>] [-OutputDir <dir>]
#         [-Iscc <ISCC.exe>] [-NoInstaller]
#
# BuildDir defaults to build\Release of the repository, OutputDir to package in BuildDir, and Iscc
# to ISCC.exe of the installation of Inno Setup 7 that its setup records, or else to ISCC.exe in
# PATH.
$ErrorActionPreference = "Stop"
$repository = (Resolve-Path (Join-Path $PSScriptRoot "..\..\..")).Path

if (-not $BuildDir) {
    $BuildDir = Join-Path $repository "build\Release"
}
$BuildDir = (Resolve-Path $BuildDir).Path
if (-not $OutputDir) {
    $OutputDir = Join-Path $BuildDir "package"
}
# The path is made absolute, because ISCC resolves a relative path against the directory of
# setup.iss. The directory need not exist yet.
$OutputDir = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($OutputDir)

function Invoke-Step([string]$name, [scriptblock]$step) {
    Write-Output "== $name"
    & $step
    if ($LASTEXITCODE -ne 0) {
        throw "$name failed with exit code $LASTEXITCODE"
    }
}

# Returns the value of the entry name in the file at path, whose lines are the name, separator
# and the value, or $null if absent.
function Get-Value([string]$path, [string]$name, [string]$separator) {
    foreach ($line in Get-Content -LiteralPath $path) {
        if ($line -match "^$([regex]::Escape($name))$separator(.*)$") {
            return $Matches[1]
        }
    }
    return $null
}

# The version of the project and the triplet of cmake/DetectTriplet.cmake in the cache of the build
$cache = Join-Path $BuildDir "CMakeCache.txt"
$version = Get-Value $cache "CMAKE_PROJECT_VERSION" ":[A-Z]+="
$triplet = Get-Value $cache "HELLO_TRIPLET" ":[A-Z]+="
if (-not $version -or -not $triplet) {
    throw "The version or the triplet is missing from $cache. Configure the build again."
}
# The deployment is an install rule, which a build without HELLO_DEPLOY or a Debug build lacks
if ((Get-Value $cache "HELLO_DEPLOY" ":[A-Z]+=") -notin @("ON", "TRUE", "1", "YES")) {
    throw "HELLO_DEPLOY is off in $cache. Configure the build with -DHELLO_DEPLOY=ON."
}
if ((Get-Value $cache "CMAKE_BUILD_TYPE" ":[A-Z]+=") -eq "Debug") {
    throw "$BuildDir is a Debug build, which is not deployed. Package a Release build."
}
# The channel of the build. A build of another channel than release is named by its channel and
# its abbreviated commit, in the file names and in the name of the installer, so that it is not
# taken for a release. The commit is the one that hellokit/BuildInfo.h records and the About
# dialog shows.
$channel = Get-Value $cache "HELLO_BUILD_CHANNEL" ":[A-Z]+="
if ($channel -notin @("release", "ci", "dev")) {
    throw "HELLO_BUILD_CHANNEL is '$channel' in $cache, not release, ci or dev."
}
$base = "helloutau-$version-$triplet"
$appVerName = "HelloUtau $version"
if ($channel -ne "release") {
    $buildInfo = Join-Path $BuildDir "out\include\hellokit\BuildInfo.h"
    $commit = (Select-String -LiteralPath $buildInfo `
        -Pattern '^#define HELLOKIT_GIT_LAST_COMMIT_HASH "([0-9a-f]{7})').Matches.Groups[1].Value
    if (-not $commit) {
        throw "The commit is missing from $buildInfo."
    }
    $base = "helloutau-$version-$channel.$commit-$triplet"
    $appVerName = "HelloUtau $version-$channel.$commit"
}
Write-Output "Packaging $base from $BuildDir"

$staging = Join-Path $OutputDir "HelloUtau"
if (Test-Path -LiteralPath $staging) {
    Remove-Item -LiteralPath $staging -Recurse -Force
}
New-Item -ItemType Directory -Force $OutputDir | Out-Null

Invoke-Step "Install" { cmake --install $BuildDir --prefix $staging }

$bin = Join-Path $staging "bin"
$mingw = $triplet -like "*-mingw"

Write-Output "== Remove the development files"
foreach ($path in @("include", "lib\cmake")) {
    $full = Join-Path $staging $path
    if (Test-Path -LiteralPath $full) {
        Remove-Item -LiteralPath $full -Recurse -Force
    }
}
# Removed one by one: Windows PowerShell 5.1 fails on piped items found with -Include.
$files = Get-ChildItem -LiteralPath $staging -Recurse -File |
    Where-Object { $_.Extension -in ".lib", ".pdb", ".ilk" }
foreach ($file in $files) {
    Remove-Item -LiteralPath $file.FullName -Force
}

# The C++ runtime as DLLs beside the program, so that the package runs without installing
# anything. The newest runtime of the newest Visual Studio is taken. The Universal CRT is part of
# Windows 10 and later. Visual Studio provides dumpbin below for MinGW as well.
$vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
$visualStudio = & $vswhere -latest -products * -property installationPath
if (-not $mingw) {
    Write-Output "== Copy the C++ runtime"
    $crt = Get-ChildItem (Join-Path $visualStudio "VC\Redist\MSVC\*\x64\Microsoft.VC14*.CRT") `
        -Directory -ErrorAction SilentlyContinue |
        Where-Object { $_.Parent.Parent.Name -match '^[0-9.]+$' } |
        Sort-Object { [version]$_.Parent.Parent.Name } | Select-Object -Last 1
    if (-not $crt) {
        throw "The C++ runtime of Visual Studio was not found in $visualStudio."
    }
    foreach ($dll in Get-ChildItem -LiteralPath $crt.FullName -Filter *.dll) {
        Copy-Item -LiteralPath $dll.FullName -Destination $bin -Force
        Write-Output "  $($dll.Name)"
    }
}

# The debug symbols of the binaries of the package, in the layout of the package, for the analysis
# of crash dumps. dumpbin reports the PDB that each binary records, found at the recorded path,
# beside the binary or in the alternative symbol path, here the binaries of vcpkg, whose ports are
# built as Release with their PDBs, as this build is (cmake/ConfigureBuildTypes.cmake). A build
# that records no PDB gets no archive of symbols. Qt is published without PDBs.
Write-Output "== Collect the debug symbols"
$dumpbin = (Get-Command dumpbin.exe -ErrorAction SilentlyContinue).Source
if (-not $dumpbin) {
    $dumpbin = Get-ChildItem (Join-Path $visualStudio "VC\Tools\MSVC\*\bin\Hostx64\x64\dumpbin.exe") `
        -ErrorAction SilentlyContinue | Select-Object -Last 1 | ForEach-Object { $_.FullName }
}
if (-not $dumpbin) {
    throw "dumpbin.exe of Visual Studio was not found in $visualStudio."
}
$vcpkgInstalled = Get-Value $cache "_VCPKG_INSTALLED_DIR" ":[A-Z]+="
$vcpkgTriplet = Get-Value $cache "VCPKG_TARGET_TRIPLET" ":[A-Z]+="
if ($vcpkgInstalled -and $vcpkgTriplet) {
    $env:_NT_ALT_SYMBOL_PATH = Join-Path $vcpkgInstalled "$vcpkgTriplet\bin"
}
$symbols = Join-Path $OutputDir "symbols"
if (Test-Path -LiteralPath $symbols) {
    Remove-Item -LiteralPath $symbols -Recurse -Force
}
$symbolCount = 0
$packaged = Get-ChildItem -LiteralPath $staging -Recurse -File |
    Where-Object { $_.Extension -in ".exe", ".dll" }
foreach ($binary in $packaged) {
    $report = & $dumpbin /NOLOGO /PDBPATH $binary.FullName | Out-String
    if ($report -match "PDB file found at '([^']+)'") {
        $relative = $binary.DirectoryName.Substring($staging.Length).TrimStart('\')
        $target = Join-Path $symbols $relative
        New-Item -ItemType Directory -Force $target | Out-Null
        Copy-Item -LiteralPath $Matches[1] -Destination $target -Force
        $symbolCount++
    }
}
Write-Output "  $symbolCount of $($packaged.Count) binaries have symbols"

$archive = Join-Path $OutputDir "$base.zip"
if (Test-Path -LiteralPath $archive) {
    Remove-Item -LiteralPath $archive -Force
}
Write-Output "== Create $archive"
Compress-Archive -Path $staging -DestinationPath $archive -CompressionLevel Optimal
$assets = @($archive)

if (-not $NoInstaller) {
    if (-not $Iscc) {
        # The installation that the setup of Inno Setup 7 records, on any drive, and then PATH
        $uninstall = "Microsoft\Windows\CurrentVersion\Uninstall\Inno Setup 7_is1"
        $keys = @("HKCU:\SOFTWARE\$uninstall", "HKLM:\SOFTWARE\$uninstall",
            "HKLM:\SOFTWARE\WOW6432Node\$uninstall")
        $candidates = foreach ($key in $keys) {
            $location = (Get-ItemProperty -LiteralPath $key -ErrorAction SilentlyContinue).InstallLocation
            if ($location) {
                Join-Path $location "ISCC.exe"
            }
        }
        $Iscc = $candidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
        if (-not $Iscc) {
            $command = Get-Command ISCC.exe -ErrorAction SilentlyContinue
            if ($command) {
                $Iscc = $command.Source
            }
        }
        if (-not $Iscc) {
            throw "ISCC.exe of Inno Setup 7 was not found. Give it with -Iscc."
        }
    }
    Invoke-Step "Compile the installer" {
        & $Iscc /Q "/DAppVersion=$version" "/DAppVerName=$appVerName" "/DSourceDir=$staging" `
            "/DOutputDir=$OutputDir" "/DOutputBaseFilename=$base-setup" `
            (Join-Path $PSScriptRoot "setup.iss")
    }
    $assets += Join-Path $OutputDir "$base-setup.exe"
}

if ($symbolCount -gt 0) {
    $symbolArchive = Join-Path $OutputDir "$base-symbols.zip"
    if (Test-Path -LiteralPath $symbolArchive) {
        Remove-Item -LiteralPath $symbolArchive -Force
    }
    Write-Output "== Create $symbolArchive"
    Compress-Archive -Path (Join-Path $symbols "*") -DestinationPath $symbolArchive `
        -CompressionLevel Optimal
    $assets += $symbolArchive
}

# One line for each asset, the lowercase digest, two spaces and the file name, as sha256sum writes
# them, with line feeds
$sums = Join-Path $OutputDir "SHA256SUMS"
$lines = foreach ($asset in $assets) {
    "$((Get-FileHash -LiteralPath $asset -Algorithm SHA256).Hash.ToLowerInvariant())  $(Split-Path -Leaf $asset)"
}
[System.IO.File]::WriteAllText($sums, ($lines -join "`n") + "`n")
Write-Output "== Packaged into $OutputDir"
