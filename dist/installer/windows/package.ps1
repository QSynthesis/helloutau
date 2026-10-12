param(
    [string]$BuildDir,
    [string]$OutputDir,
    [string]$Qt,
    [string]$Iscc,
    [switch]$NoInstaller
)
# Packages a release build of HelloUtau for Windows into the release assets of
# docs/Distribution.md, in $OutputDir:
#
#     helloutau-<version>-<triplet>.zip         the portable package, a folder HelloUtau
#     helloutau-<version>-<triplet>-setup.exe   the installer, compiled from setup.iss
#     SHA256SUMS                                the digests of both, as sha256sum writes them
#
# The installation is laid out as
#
#     bin/                    helloutau.exe, the libraries of the application, of Qt and of the
#                             other dependencies, and the C++ runtime
#     lib/helloutau/plugins/  the plugins of the application, where cmake --install puts them
#     lib/Qt/plugins/         the plugins of Qt, which AppLoader::addQtPluginPaths() adds
#     share/Qt/translations/  the translations of Qt, installed by helloutau/CMakeLists.txt
#     share/helloutau/update/ UpdateManifest.json, installed by helloutau/CMakeLists.txt
#
# The build must be complete: this script does not build, because building requires the
# environment of the compiler. Steps: cmake --install, copy the DLLs that the build copied beside
# the program but cmake --install does not install (stdcorelib, stdutau, substate, QActionKit,
# wolf-midi and the like), deploy Qt with windeployqt, remove what only developers need, copy the
# C++ runtime, archive, and compile the installer with Inno Setup 7.
#
#     powershell -File dist\installer\windows\package.ps1 [-BuildDir <dir>] [-OutputDir <dir>]
#         [-Qt <dir>] [-Iscc <ISCC.exe>] [-NoInstaller]
#
# BuildDir defaults to build\Release of the repository, OutputDir to package in BuildDir, Qt to the
# Qt of the build, and Iscc to ISCC.exe of the installation of Inno Setup 7 that its setup
# records, or else to ISCC.exe in PATH.
$ErrorActionPreference = "Stop"
$repository = (Resolve-Path (Join-Path $PSScriptRoot "..\..\..")).Path

if (-not $BuildDir) {
    $BuildDir = Join-Path $repository "build\Release"
}
$BuildDir = (Resolve-Path $BuildDir).Path
if (-not $OutputDir) {
    $OutputDir = Join-Path $BuildDir "package"
}

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
if (-not $Qt) {
    # Qt6_DIR is <Qt>\lib\cmake\Qt6.
    $qtDir = Get-Value $cache "Qt6_DIR" ":[A-Z]+="
    if (-not $qtDir) {
        throw "Qt6_DIR was not found in the CMake cache. Give the directory of Qt with -Qt."
    }
    $Qt = (Resolve-Path (Join-Path $qtDir "..\..\..")).Path
}
$base = "helloutau-$version-$triplet"
Write-Output "Packaging $base from $BuildDir"

$staging = Join-Path $OutputDir "HelloUtau"
if (Test-Path -LiteralPath $staging) {
    Remove-Item -LiteralPath $staging -Recurse -Force
}
New-Item -ItemType Directory -Force $OutputDir | Out-Null

Invoke-Step "Install" { cmake --install $BuildDir --prefix $staging }

$bin = Join-Path $staging "bin"
$qtPlugins = Join-Path $staging "lib\Qt\plugins"

Write-Output "== Copy the dependencies"
foreach ($dll in Get-ChildItem -LiteralPath (Join-Path $BuildDir "out\bin") -Filter *.dll) {
    $target = Join-Path $bin $dll.Name
    if (-not (Test-Path -LiteralPath $target)) {
        Copy-Item -LiteralPath $dll.FullName -Destination $target
        Write-Output "  $($dll.Name)"
    }
}

# Qt for the program, every library and every plugin of the application, with the plugins of Qt
# in lib/Qt/plugins. The C++ runtime is copied below instead of the installer of windeployqt.
$binaries = @(Join-Path $bin "helloutau.exe")
$binaries += Get-ChildItem -LiteralPath $bin -Filter *.dll | ForEach-Object { $_.FullName }
$binaries += Get-ChildItem -LiteralPath (Join-Path $staging "lib\helloutau\plugins") -Recurse `
    -Filter *.dll | ForEach-Object { $_.FullName }
Invoke-Step "windeployqt" {
    & (Join-Path $Qt "bin\windeployqt.exe") --release --dir $bin --plugindir $qtPlugins `
        --no-translations --no-compiler-runtime @binaries
}

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
# Windows 10 and later.
Write-Output "== Copy the C++ runtime"
$vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
$visualStudio = & $vswhere -latest -products * -property installationPath
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
        & $Iscc /Q "/DAppVersion=$version" "/DSourceDir=$staging" "/DOutputDir=$OutputDir" `
            "/DOutputBaseFilename=$base-setup" (Join-Path $PSScriptRoot "setup.iss")
    }
    $assets += Join-Path $OutputDir "$base-setup.exe"
}

# One line for each asset, the lowercase digest, two spaces and the file name, as sha256sum writes
# them, with line feeds
$sums = Join-Path $OutputDir "SHA256SUMS"
$lines = foreach ($asset in $assets) {
    "$((Get-FileHash -LiteralPath $asset -Algorithm SHA256).Hash.ToLowerInvariant())  $(Split-Path -Leaf $asset)"
}
[System.IO.File]::WriteAllText($sums, ($lines -join "`n") + "`n")
Write-Output "== Packaged into $OutputDir"
