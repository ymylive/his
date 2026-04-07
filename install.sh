#!/bin/bash
# ═══════════════════════════════════════════════════════════
#  HIS 轻量级医院信息系统 — macOS 一键安装启动脚本
# ═══════════════════════════════════════════════════════════
#
#  使用方法（终端粘贴一行即可）:
#
#    curl -fsSL https://raw.githubusercontent.com/ymylive/his/main/install.sh | bash
#
#  此脚本会：
#    1. 下载最新 macOS 版本
#    2. 解除 Gatekeeper 隔离
#    3. 赋予执行权限
#    4. 自动启动程序
#

set -e

VERSION="v3.0.0"
REPO="ymylive/his"
DMG_NAME="his-${VERSION}-macos-arm64.dmg"
INSTALL_DIR="$HOME/HIS"

# ── 颜色 ──
RED='\033[1;31m'
GREEN='\033[1;32m'
YELLOW='\033[1;33m'
CYAN='\033[1;36m'
BOLD='\033[1m'
RESET='\033[0m'

info()    { printf "  ${CYAN}•${RESET} %s\n" "$1"; }
success() { printf "  ${GREEN}✓${RESET} %s\n" "$1"; }
warn()    { printf "  ${YELLOW}▲${RESET} %s\n" "$1"; }
fail()    { printf "  ${RED}✗${RESET} %s\n" "$1"; exit 1; }

echo ""
printf "  ${BOLD}╔══════════════════════════════════════╗${RESET}\n"
printf "  ${BOLD}║   HIS 医院信息系统 — 安装启动脚本   ║${RESET}\n"
printf "  ${BOLD}╚══════════════════════════════════════╝${RESET}\n"
echo ""

# ── 检测架构 ──
ARCH="$(uname -m)"
if [ "$ARCH" != "arm64" ] && [ "$ARCH" != "x86_64" ]; then
    fail "不支持的架构: $ARCH"
fi
info "检测到架构: $ARCH"

# ── 下载 ──
DOWNLOAD_URL="https://github.com/${REPO}/releases/download/${VERSION}/${DMG_NAME}"

if [ -f "$INSTALL_DIR/his" ]; then
    info "检测到已安装版本: $INSTALL_DIR/his"
    printf "  ${YELLOW}▶${RESET} 是否重新安装? [y/N] "
    read -r REPLY
    if [ "$REPLY" != "y" ] && [ "$REPLY" != "Y" ]; then
        info "跳过下载，直接启动..."
        cd "$INSTALL_DIR"
        exec ./his
    fi
fi

mkdir -p "$INSTALL_DIR"
TMPDIR_DL="$(mktemp -d)"
DMG_PATH="${TMPDIR_DL}/${DMG_NAME}"

info "正在下载 ${VERSION}..."
if command -v curl >/dev/null 2>&1; then
    curl -fSL --progress-bar -o "$DMG_PATH" "$DOWNLOAD_URL" || fail "下载失败，请检查网络"
elif command -v wget >/dev/null 2>&1; then
    wget -q --show-progress -O "$DMG_PATH" "$DOWNLOAD_URL" || fail "下载失败，请检查网络"
else
    fail "需要 curl 或 wget"
fi
success "下载完成"

# ── 挂载 DMG 并复制文件 ──
info "正在解包..."
MOUNT_POINT="$(hdiutil attach "$DMG_PATH" -nobrowse -quiet 2>/dev/null | grep '/Volumes/' | awk '{print $NF}')"

if [ -z "$MOUNT_POINT" ]; then
    # 如果是 tar.gz 回退方案
    fail "DMG 挂载失败"
fi

cp "$MOUNT_POINT/his" "$INSTALL_DIR/his"
cp -r "$MOUNT_POINT/data" "$INSTALL_DIR/" 2>/dev/null || true
cp "$MOUNT_POINT/README.txt" "$INSTALL_DIR/" 2>/dev/null || true

hdiutil detach "$MOUNT_POINT" -quiet 2>/dev/null
rm -rf "$TMPDIR_DL"
success "解包完成"

# ── 解除隔离 + 赋权 ──
info "正在解除安全限制..."
xattr -cr "$INSTALL_DIR" 2>/dev/null || true
xattr -d com.apple.quarantine "$INSTALL_DIR/his" 2>/dev/null || true
chmod +x "$INSTALL_DIR/his"
success "权限已授予"

# ── 验证签名 ──
if codesign -v "$INSTALL_DIR/his" 2>/dev/null; then
    success "代码签名验证通过"
else
    warn "Ad-hoc 签名（不影响运行）"
fi

# ── 完成 ──
echo ""
success "安装完成！"
info "安装位置: $INSTALL_DIR"
info "后续可直接运行: $INSTALL_DIR/his"
echo ""

# ── 启动 ──
printf "  ${YELLOW}▶${RESET} 现在启动 HIS? [Y/n] "
read -r REPLY
if [ "$REPLY" = "n" ] || [ "$REPLY" = "N" ]; then
    info "稍后可运行: cd $INSTALL_DIR && ./his"
    exit 0
fi

cd "$INSTALL_DIR"
exec ./his
