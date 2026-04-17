# Lightweight HIS one-shot self-updater (Windows PowerShell)
#
# Usage:
#   iwr -useb https://raw.githubusercontent.com/ymylive/his/main/scripts/update.ps1 | iex
#   $env:HIS_TARGET = '.\his.exe'; iwr ... | iex

[CmdletBinding()]
param(
    [string]$Target = $env:HIS_TARGET,
    [string]$Tag    = $env:HIS_TAG,
    [string]$Repo   = $(if ($env:HIS_UPDATE_REPO) { $env:HIS_UPDATE_REPO } else { 'ymylive/his' })
)

$ErrorActionPreference = 'Stop'

function Log  ($m) { Write-Host "[his-update] $m" -ForegroundColor Cyan }
function Warn ($m) { Write-Host "[his-update] $m" -ForegroundColor Yellow }
function Err  ($m) { Write-Host "[his-update] $m" -ForegroundColor Red }

# ── 平台 key ───────────────────────────────────────────
$PlatformKey = 'win64'
Log "platform: $PlatformKey"

# ── 目标路径 ───────────────────────────────────────────
if (-not $Target) {
    if (Test-Path '.\his.exe')  { $Target = '.\his.exe' }
    elseif (Get-Command his.exe -ErrorAction SilentlyContinue) {
        $Target = (Get-Command his.exe).Source
    } else {
        $Target = '.\his.exe'
        Warn "no existing his.exe found; will install to $Target"
    }
}
Log "target:   $Target"

# ── 选 tag ─────────────────────────────────────────────
if (-not $Tag) {
    Log 'fetching latest release tag...'
    $rel = Invoke-RestMethod -Uri "https://api.github.com/repos/$Repo/releases/latest" `
                             -Headers @{ 'User-Agent' = 'his-updater-ps' }
    $Tag = $rel.tag_name
}
if (-not $Tag) { Err 'could not resolve latest tag'; exit 1 }
Log "tag:      $Tag"

$AssetBase = "https://github.com/$Repo/releases/download/$Tag"
$ZipName   = "lightweight-his-portable-$Tag-$PlatformKey.zip"
$ZipUrl    = "$AssetBase/$ZipName"
$SumsUrl   = "$AssetBase/SHA256SUMS"

# ── 下载 ───────────────────────────────────────────────
$Work = Join-Path $env:TEMP "his-update-$([Guid]::NewGuid().ToString('N'))"
New-Item -ItemType Directory -Path $Work | Out-Null
try {
    $ZipPath = Join-Path $Work 'pkg.zip'
    $SumsPath = Join-Path $Work 'SHA256SUMS'
    Log "downloading $ZipName..."
    Invoke-WebRequest -Uri $ZipUrl  -OutFile $ZipPath  -UseBasicParsing -Headers @{ 'User-Agent' = 'his-updater-ps' }
    Log 'downloading SHA256SUMS...'
    Invoke-WebRequest -Uri $SumsUrl -OutFile $SumsPath -UseBasicParsing -Headers @{ 'User-Agent' = 'his-updater-ps' }

    # ── 校验 ──────────────────────────────────────────
    $expected = $null
    foreach ($line in Get-Content $SumsPath) {
        $parts = $line -split '\s+', 2
        if ($parts.Length -ge 2 -and $parts[1].Trim() -eq $ZipName) {
            $expected = $parts[0].Trim().ToLowerInvariant()
            break
        }
    }
    if (-not $expected) { Err "$ZipName not found in SHA256SUMS"; exit 1 }
    $actual = (Get-FileHash -Algorithm SHA256 -Path $ZipPath).Hash.ToLowerInvariant()
    if ($expected -ne $actual) { Err "SHA-256 mismatch: expected $expected, got $actual"; exit 1 }
    Log 'SHA-256 OK'

    # ── 解包 + 替换 ───────────────────────────────────
    Log 'extracting...'
    $ExtractDir = Join-Path $Work 'extracted'
    Expand-Archive -Path $ZipPath -DestinationPath $ExtractDir -Force
    $NewBin = Get-ChildItem -Path $ExtractDir -Recurse -Filter 'his.exe' |
              Select-Object -First 1 -ExpandProperty FullName
    if (-not $NewBin) { Err "his.exe not found inside $ZipName"; exit 1 }

    if (Test-Path $Target) {
        $backup = "$Target.bak.$([DateTimeOffset]::Now.ToUnixTimeSeconds())"
        Log "backup old binary to $backup"
        Copy-Item -Path $Target -Destination $backup -Force
    }
    $TargetDir = Split-Path -Parent $Target
    if ($TargetDir -and -not (Test-Path $TargetDir)) { New-Item -ItemType Directory -Path $TargetDir | Out-Null }
    Copy-Item -Path $NewBin -Destination $Target -Force
    Log "updated $Target → $Tag"
} finally {
    Remove-Item -Recurse -Force -Path $Work -ErrorAction SilentlyContinue
}
