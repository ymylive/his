#!/bin/bash
# ═══════════════════════════════════════════════════════════
#  HIS 轻量级医院信息系统 — 安装 / 卸载 / 启动
# ═══════════════════════════════════════════════════════════
#
#  安装:  curl -fsSL https://raw.githubusercontent.com/ymylive/his/main/install.sh | bash
#  卸载:  curl -fsSL https://raw.githubusercontent.com/ymylive/his/main/install.sh | bash -s uninstall
#  启动:  his  (安装后可直接使用)
#

set -e

VERSION="v3.1.0"
REPO="ymylive/his"
DMG_NAME="his-${VERSION}-macos-arm64.dmg"
INSTALL_DIR="$HOME/.his"
BIN_LINK="/usr/local/bin/his"

# ── 颜色 ──
RED='\033[1;31m'
GREEN='\033[1;32m'
YELLOW='\033[1;33m'
CYAN='\033[1;36m'
DIM='\033[2m'
BOLD='\033[1m'
RESET='\033[0m'

info()    { printf "  ${CYAN}•${RESET} %s\n" "$1"; }
success() { printf "  ${GREEN}✓${RESET} %s\n" "$1"; }
warn()    { printf "  ${YELLOW}▲${RESET} %s\n" "$1"; }
fail()    { printf "  ${RED}✗${RESET} %s\n" "$1"; exit 1; }

print_banner() {
    echo ""
    printf "  ${CYAN}╔══════════════════════════════════════════╗${RESET}\n"
    printf "  ${CYAN}║${RESET}  ${BOLD}HIS${RESET} ${DIM}轻量级医院信息系统${RESET}  ${DIM}${VERSION}${RESET}          ${CYAN}║${RESET}\n"
    printf "  ${CYAN}╚══════════════════════════════════════════╝${RESET}\n"
    echo ""
}

# ════════════════════════════════════════════════════════════
#  卸载
# ════════════════════════════════════════════════════════════
do_uninstall() {
    print_banner
    info "正在卸载 HIS..."

    if [ -L "$BIN_LINK" ]; then
        rm -f "$BIN_LINK" 2>/dev/null || sudo rm -f "$BIN_LINK"
        success "已移除命令: $BIN_LINK"
    else
        info "未找到命令链接: $BIN_LINK"
    fi

    if [ -d "$INSTALL_DIR" ]; then
        rm -rf "$INSTALL_DIR"
        success "已删除安装目录: $INSTALL_DIR"
    else
        info "未找到安装目录: $INSTALL_DIR"
    fi

    echo ""
    success "HIS 已完全卸载"
    echo ""
}

# ════════════════════════════════════════════════════════════
#  安装
# ════════════════════════════════════════════════════════════
do_install() {
    print_banner

    # ── 检测系统 ──
    OS="$(uname -s)"
    ARCH="$(uname -m)"

    if [ "$OS" != "Darwin" ]; then
        fail "此脚本仅支持 macOS，当前系统: $OS"
    fi
    info "系统: macOS ($ARCH)"

    # ── 已安装? ──
    if [ -f "$INSTALL_DIR/his" ]; then
        info "检测到已安装版本"
        printf "  ${YELLOW}▶${RESET} 是否重新安装? [y/N] "
        read -r REPLY </dev/tty
        if [ "$REPLY" != "y" ] && [ "$REPLY" != "Y" ]; then
            echo ""
            info "保留当前版本，可运行: his"
            exit 0
        fi
    fi

    # ── 下载 ──
    DOWNLOAD_URL="https://github.com/${REPO}/releases/download/${VERSION}/${DMG_NAME}"
    mkdir -p "$INSTALL_DIR"
    TMPDIR_DL="$(mktemp -d)"
    DMG_PATH="${TMPDIR_DL}/${DMG_NAME}"

    info "正在下载 ${VERSION}..."
    curl -fSL --progress-bar -o "$DMG_PATH" "$DOWNLOAD_URL" || fail "下载失败，请检查网络连接"
    success "下载完成"

    # ── 解包 ──
    info "正在安装..."
    MOUNT_OUTPUT="$(hdiutil attach "$DMG_PATH" -nobrowse 2>/dev/null)"
    MOUNT_POINT="$(echo "$MOUNT_OUTPUT" | awk -F'\t' '/\/Volumes\//{print $NF}' | head -1)"

    if [ -z "$MOUNT_POINT" ] || [ ! -d "$MOUNT_POINT" ]; then
        fail "DMG 挂载失败"
    fi

    cp -f "$MOUNT_POINT/his" "$INSTALL_DIR/his"
    rm -rf "$INSTALL_DIR/data"
    cp -r "$MOUNT_POINT/data" "$INSTALL_DIR/" 2>/dev/null || true

    hdiutil detach "$MOUNT_POINT" -quiet 2>/dev/null || true
    rm -rf "$TMPDIR_DL"

    # ── 授权 ──
    info "正在授权..."
    xattr -cr "$INSTALL_DIR" 2>/dev/null || true
    chmod +x "$INSTALL_DIR/his"
    success "安全限制已解除"

    # ── 创建全局命令 ──
    info "正在创建 his 命令..."

    # 写一个 wrapper 脚本确保 data/ 路径正确
    cat > "$INSTALL_DIR/his-wrapper" <<'WRAPPER'
#!/bin/bash
cd "$HOME/.his" && exec ./his "$@"
WRAPPER
    chmod +x "$INSTALL_DIR/his-wrapper"

    LINK_OK=0
    if [ -d "/usr/local/bin" ] || sudo mkdir -p /usr/local/bin 2>/dev/null; then
        if ln -sf "$INSTALL_DIR/his-wrapper" "$BIN_LINK" 2>/dev/null; then
            LINK_OK=1
        elif sudo ln -sf "$INSTALL_DIR/his-wrapper" "$BIN_LINK" 2>/dev/null; then
            LINK_OK=1
        fi
    fi

    if [ "$LINK_OK" = "1" ] && command -v his >/dev/null 2>&1; then
        success "已创建全局命令: his"
    else
        # 备用方案: 写入 shell 配置
        SHELL_RC="$HOME/.zshrc"
        [ -n "$BASH_VERSION" ] && SHELL_RC="$HOME/.bashrc"
        EXPORT_LINE='export PATH="$HOME/.his:$PATH"'
        if ! grep -qF '.his' "$SHELL_RC" 2>/dev/null; then
            echo "$EXPORT_LINE" >> "$SHELL_RC"
            success "已添加 PATH 到 $SHELL_RC"
            info "请运行: source $SHELL_RC 或重新打开终端"
        else
            success "PATH 已配置"
        fi
        # 同时把 wrapper 软链到 .his 目录本身以便 PATH 方案也能用 his 命令
        ln -sf "$INSTALL_DIR/his-wrapper" "$INSTALL_DIR/his-cmd" 2>/dev/null || true
    fi

    # ── 完成 ──
    echo ""
    printf "  ${GREEN}══════════════════════════════════════${RESET}\n"
    success "安装完成！"
    printf "  ${GREEN}══════════════════════════════════════${RESET}\n"
    echo ""
    info "安装位置:  ~/.his/"
    printf "  ${CYAN}•${RESET} 启动命令:  ${BOLD}his${RESET}\n"
    info "卸载命令:  curl -fsSL https://raw.githubusercontent.com/${REPO}/main/install.sh | bash -s uninstall"
    echo ""

    # ── 启动 ──
    printf "  ${YELLOW}▶${RESET} 现在启动 HIS? [Y/n] "
    read -r REPLY </dev/tty
    if [ "$REPLY" = "n" ] || [ "$REPLY" = "N" ]; then
        printf "  ${CYAN}•${RESET} 稍后在终端输入 ${BOLD}his${RESET} 即可启动\n"
        exit 0
    fi

    cd "$INSTALL_DIR"
    exec ./his
}

# ════════════════════════════════════════════════════════════
#  入口
# ════════════════════════════════════════════════════════════
case "${1:-install}" in
    uninstall|remove|delete)
        do_uninstall
        ;;
    install|"")
        do_install
        ;;
    *)
        echo "用法: $0 [install|uninstall]"
        exit 1
        ;;
esac
