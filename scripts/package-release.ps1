param(
    [string]$BuildDir = "build-release",
    [string]$DistDir = "dist",
    [string]$Version = "0.3.0",
    [switch]$SkipTests,
    [switch]$SkipSmoke
)

$ErrorActionPreference = "Stop"

function Invoke-External {
    param(
        [Parameter(Mandatory = $true)]
        [string]$FilePath,
        [string[]]$Arguments = @()
    )

    & $FilePath @Arguments

    if ($LASTEXITCODE -ne 0) {
        $rendered = @($FilePath) + $Arguments
        throw "Command failed with exit code ${LASTEXITCODE}: $($rendered -join ' ')"
    }
}

function Get-ArchitectureTag {
    param(
        [Parameter(Mandatory = $true)]
        [string]$CacheFile,
        [Parameter(Mandatory = $true)]
        [string]$ExecutablePath
    )

    if (-not (Test-Path -LiteralPath $CacheFile)) {
        throw "Missing CMake cache: $CacheFile"
    }

    $compilerEntry = Select-String -Path $CacheFile -Pattern '^CMAKE_C_COMPILER:FILEPATH=(.+)$' |
        Select-Object -First 1

    if ($null -eq $compilerEntry) {
        throw "Unable to detect C compiler from $CacheFile"
    }

    $compilerPath = $compilerEntry.Matches[0].Groups[1].Value
    $objdumpPath = Join-Path (Split-Path -Path $compilerPath -Parent) "objdump.exe"

    if (-not (Test-Path -LiteralPath $objdumpPath)) {
        throw "Unable to locate objdump next to compiler: $objdumpPath"
    }

    $header = & $objdumpPath -f $ExecutablePath
    if ($LASTEXITCODE -ne 0) {
        throw "objdump failed for $ExecutablePath"
    }

    if ($header -match 'file format pei-i386') {
        return "win32"
    }

    if ($header -match 'file format pei-x86-64') {
        return "win64"
    }

    throw "Unsupported executable architecture: $ExecutablePath"
}

function Write-Launcher {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path,
        [Parameter(Mandatory = $true)]
        [string[]]$Lines
    )

    Set-Content -LiteralPath $Path -Value ($Lines -join "`r`n") -Encoding ascii
}

$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$buildPath = Join-Path $projectRoot $BuildDir
$distPath = Join-Path $projectRoot $DistDir
$normalizedVersion = if ($Version.StartsWith("v")) { $Version.Substring(1) } else { $Version }

New-Item -ItemType Directory -Force -Path $distPath | Out-Null

Invoke-External -FilePath "cmake" -Arguments @("-S", $projectRoot, "-B", $buildPath, "-G", "Ninja", "-DCMAKE_BUILD_TYPE=Release")
Invoke-External -FilePath "cmake" -Arguments @("--build", $buildPath)

if (-not $SkipTests) {
    Invoke-External -FilePath "ctest" -Arguments @("--test-dir", $buildPath, "--output-on-failure")
}

$desktopExecutable = Join-Path $buildPath "his_desktop.exe"
if (-not $SkipSmoke) {
    Invoke-External -FilePath $desktopExecutable -Arguments @("--smoke")
}

$archTag = Get-ArchitectureTag -CacheFile (Join-Path $buildPath "CMakeCache.txt") -ExecutablePath $desktopExecutable
$packageName = "lightweight-his-portable-v$normalizedVersion-$archTag"
$stagePath = Join-Path $distPath $packageName
$zipPath = Join-Path $distPath "$packageName.zip"

if (Test-Path -LiteralPath $stagePath) {
    Remove-Item -LiteralPath $stagePath -Recurse -Force
}

if (Test-Path -LiteralPath $zipPath) {
    Remove-Item -LiteralPath $zipPath -Force
}

New-Item -ItemType Directory -Force -Path $stagePath | Out-Null

Copy-Item -LiteralPath (Join-Path $buildPath "his.exe") -Destination (Join-Path $stagePath "his.exe")
Copy-Item -LiteralPath (Join-Path $buildPath "his_desktop.exe") -Destination (Join-Path $stagePath "his_desktop.exe")
Copy-Item -LiteralPath (Join-Path $projectRoot "README.md") -Destination (Join-Path $stagePath "README.md")
Copy-Item -LiteralPath (Join-Path $projectRoot "data") -Destination (Join-Path $stagePath "data") -Recurse

Write-Launcher -Path (Join-Path $stagePath "run_desktop.bat") -Lines @(
    "@echo off",
    "cd /d ""%~dp0""",
    "start """" ""%~dp0his_desktop.exe"""
)

Write-Launcher -Path (Join-Path $stagePath "run_console.bat") -Lines @(
    "@echo off",
    "cd /d ""%~dp0""",
    """%~dp0his.exe"""
)

$startHere = @"
Lightweight HIS Portable v$normalizedVersion

Start desktop:
  run_desktop.bat

Start console:
  run_console.bat

Demo accounts:
  ADM0001 / admin123
  CLK0001 / clerk123
  DOC0001 / doctor123
  INP0001 / inpatient123
  WRD0001 / ward123
  PHA0001 / pharmacy123
  PAT0001 / patient123

Notes:
  - Keep the data folder next to the executables.
  - If Windows blocks files from the zip, extract first and then run the bat launcher.
"@

Set-Content -LiteralPath (Join-Path $stagePath "START_HERE.txt") -Value $startHere -Encoding ascii

Compress-Archive -Path $stagePath -DestinationPath $zipPath -CompressionLevel Optimal

Write-Host "Portable release ready:"
Write-Host "  Folder: $stagePath"
Write-Host "  Zip:    $zipPath"
