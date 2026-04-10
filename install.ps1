# ═══════════════════════════════════════════════════════════
#  HIS 轻量级医院信息系统 — Windows 安装 / 卸载 / 启动
# ═══════════════════════════════════════════════════════════
#
#  安装:  irm https://raw.githubusercontent.com/ymylive/his/main/install.ps1 | iex
#  卸载:  & ([scriptblock]::Create((irm https://raw.githubusercontent.com/ymylive/his/main/install.ps1))) uninstall
#  启动:  his  (安装后可直接使用)
#

param(
    [Parameter(Position = 0)]
    [string]$Action = "install"
)

$ErrorActionPreference = "Stop"

$Version   = "v3.1.0"
$Repo      = "ymylive/his"
$InstallDir = Join-Path $env:LOCALAPPDATA "his"
$BinDir     = Join-Path $env:LOCALAPPDATA "his\bin"

# ── 颜色输出 ──

function Write-Info    { param([string]$Msg) Write-Host "  *" -ForegroundColor Cyan -NoNewline; Write-Host " $Msg" }
function Write-Ok      { param([string]$Msg) Write-Host "  √" -ForegroundColor Green -NoNewline; Write-Host " $Msg" }
function Write-Warn    { param([string]$Msg) Write-Host "  !" -ForegroundColor Yellow -NoNewline; Write-Host " $Msg" }
function Write-Fail    { param([string]$Msg) Write-Host "  X" -ForegroundColor Red -NoNewline; Write-Host " $Msg"; exit 1 }

function Write-Banner {
    Write-Host ""
    Write-Host "  ╔══════════════════════════════════════════╗" -ForegroundColor Cyan
    Write-Host "  ║  " -ForegroundColor Cyan -NoNewline
    Write-Host "HIS" -ForegroundColor White -NoNewline
    Write-Host " 轻量级医院信息系统  $Version" -ForegroundColor DarkGray -NoNewline
    Write-Host "          ║" -ForegroundColor Cyan
    Write-Host "  ╚══════════════════════════════════════════╝" -ForegroundColor Cyan
    Write-Host ""
}

# ════════════════════════════════════════════════════════════
#  卸载
# ════════════════════════════════════════════════════════════

function Do-Uninstall {
    Write-Banner
    Write-Info "正在卸载 HIS..."

    if (Test-Path $InstallDir) {
        Remove-Item -Recurse -Force $InstallDir
        Write-Ok "已删除安装目录: $InstallDir"
    } else {
        Write-Info "未找到安装目录: $InstallDir"
    }

    # 从 PATH 中移除
    $userPath = [Environment]::GetEnvironmentVariable("Path", "User")
    if ($userPath -and $userPath.Contains($BinDir)) {
        $newPath = ($userPath -split ";" | Where-Object { $_ -ne $BinDir }) -join ";"
        [Environment]::SetEnvironmentVariable("Path", $newPath, "User")
        Write-Ok "已从 PATH 中移除: $BinDir"
    }

    Write-Host ""
    Write-Ok "HIS 已完全卸载"
    Write-Host ""
}

# ════════════════════════════════════════════════════════════
#  安装
# ════════════════════════════════════════════════════════════

function Do-Install {
    Write-Banner

    # ── 架构检测 ──
    $arch = if ([Environment]::Is64BitOperatingSystem) { "win64" } else { "win32" }
    Write-Info "系统: Windows ($arch)"

    # ── 已安装? ──
    if (Test-Path (Join-Path $InstallDir "his.exe")) {
        Write-Info "检测到已安装版本"
        $reply = Read-Host "  > 是否重新安装? [y/N]"
        if ($reply -ne "y" -and $reply -ne "Y") {
            Write-Host ""
            Write-Info "保留当前版本，可运行: his"
            return
        }
    }

    # ── 查找 release 资产 ──
    Write-Info "正在查询最新版本..."
    $apiUrl = "https://api.github.com/repos/$Repo/releases/latest"
    try {
        $release = Invoke-RestMethod -Uri $apiUrl -Headers @{ "Accept" = "application/vnd.github.v3+json" } -TimeoutSec 10
    } catch {
        Write-Warn "无法连接 GitHub API，使用默认版本 $Version"
        $release = $null
    }

    $downloadUrl = $null
    if ($release) {
        foreach ($asset in $release.assets) {
            if ($asset.name -match $arch -and $asset.name -match "\.zip$") {
                $downloadUrl = $asset.browser_download_url
                $Version = $release.tag_name
                break
            }
        }
    }

    if (-not $downloadUrl) {
        # 回退: 构造 URL
        $normalizedVersion = $Version.TrimStart("v")
        $zipName = "lightweight-his-portable-v${normalizedVersion}-${arch}.zip"
        $downloadUrl = "https://github.com/$Repo/releases/download/$Version/$zipName"
    }

    Write-Info "版本: $Version ($arch)"

    # ── 下载 ──
    $tmpZip = Join-Path $env:TEMP "his_install.zip"
    $tmpDir = Join-Path $env:TEMP "his_install"

    Write-Info "正在下载..."
    try {
        $ProgressPreference = "SilentlyContinue"
        Invoke-WebRequest -Uri $downloadUrl -OutFile $tmpZip -UseBasicParsing
        $ProgressPreference = "Continue"
    } catch {
        Write-Fail "下载失败: $($_.Exception.Message)"
    }
    Write-Ok "下载完成"

    # ── 解压 ──
    Write-Info "正在解压..."
    if (Test-Path $tmpDir) { Remove-Item -Recurse -Force $tmpDir }
    Expand-Archive -Path $tmpZip -DestinationPath $tmpDir -Force
    Write-Ok "解压完成"

    # ── 安装 ──
    Write-Info "正在安装到 $InstallDir ..."
    New-Item -ItemType Directory -Force -Path $InstallDir | Out-Null
    New-Item -ItemType Directory -Force -Path $BinDir | Out-Null

    # 找到解压后的顶层文件夹
    $innerDir = Get-ChildItem -Path $tmpDir -Directory | Select-Object -First 1
    if ($innerDir) {
        Copy-Item -Path (Join-Path $innerDir.FullName "*") -Destination $InstallDir -Recurse -Force
    } else {
        Copy-Item -Path (Join-Path $tmpDir "*") -Destination $InstallDir -Recurse -Force
    }

    # ── 创建 launcher bat ──
    $launcherPath = Join-Path $BinDir "his.bat"
    @(
        "@echo off"
        "cd /d `"$InstallDir`""
        "`"$InstallDir\his.exe`" %*"
    ) | Set-Content -Path $launcherPath -Encoding ASCII
    Write-Ok "已创建启动器: $launcherPath"

    # ── 添加到 PATH ──
    $userPath = [Environment]::GetEnvironmentVariable("Path", "User")
    if (-not $userPath -or -not $userPath.Contains($BinDir)) {
        [Environment]::SetEnvironmentVariable("Path", "$BinDir;$userPath", "User")
        $env:Path = "$BinDir;$env:Path"
        Write-Ok "已添加到 PATH: $BinDir"
    } else {
        Write-Ok "PATH 已配置"
    }

    # ── 清理 ──
    Remove-Item -Force $tmpZip -ErrorAction SilentlyContinue
    Remove-Item -Recurse -Force $tmpDir -ErrorAction SilentlyContinue

    # ── 完成 ──
    Write-Host ""
    Write-Host "  ══════════════════════════════════════" -ForegroundColor Green
    Write-Ok "安装完成！"
    Write-Host "  ══════════════════════════════════════" -ForegroundColor Green
    Write-Host ""
    Write-Info "安装位置:  $InstallDir"
    Write-Host "  *" -ForegroundColor Cyan -NoNewline
    Write-Host " 启动命令:  " -NoNewline
    Write-Host "his" -ForegroundColor White
    Write-Info "卸载命令:  irm https://raw.githubusercontent.com/$Repo/main/install.ps1 | iex"
    Write-Info "           然后输入: uninstall"
    Write-Host ""

    # ── 启动 ──
    $reply = Read-Host "  > 现在启动 HIS? [Y/n]"
    if ($reply -eq "n" -or $reply -eq "N") {
        Write-Host "  *" -ForegroundColor Cyan -NoNewline
        Write-Host " 稍后在终端输入 " -NoNewline
        Write-Host "his" -ForegroundColor White -NoNewline
        Write-Host " 即可启动"
        return
    }

    Set-Location $InstallDir
    & "$InstallDir\his.exe"
}

# ════════════════════════════════════════════════════════════
#  入口
# ════════════════════════════════════════════════════════════

switch ($Action.ToLower()) {
    "uninstall" { Do-Uninstall }
    "remove"    { Do-Uninstall }
    "install"   { Do-Install }
    default     { Do-Install }
}
