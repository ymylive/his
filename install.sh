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

REPO="ymylive/his"
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
    printf "  ${CYAN}║${RESET}  ${BOLD}HIS${RESET} ${DIM}轻量级医院信息系统${RESET}                  ${CYAN}║${RESET}\n"
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

    case "$OS" in
        Darwin) PLATFORM_KEY="macos" ;;
        Linux)  PLATFORM_KEY="linux" ;;
        *)      fail "不支持的系统: $OS" ;;
    esac

    case "$ARCH" in
        arm64|aarch64) ARCH_KEY="arm64" ;;
        x86_64)        ARCH_KEY="x86_64" ;;
        *)             ARCH_KEY="$ARCH" ;;
    esac

    ASSET_KEYWORD="${PLATFORM_KEY}-${ARCH_KEY}"
    info "系统: $OS ($ARCH)"

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

    # ── 查询最新 Release ──
    info "正在查询最新版本..."
    API_URL="https://api.github.com/repos/${REPO}/releases/latest"
    API_RESPONSE="$(curl -s -m 10 -H "Accept: application/vnd.github.v3+json" "$API_URL" 2>/dev/null)" || true

    VERSION=""
    DOWNLOAD_URL=""

    USE_TARGZ=0
    if [ -n "$API_RESPONSE" ]; then
        # 提取 tag_name
        VERSION="$(echo "$API_RESPONSE" | grep '"tag_name"' | head -1 | sed 's/.*"tag_name"[[:space:]]*:[[:space:]]*"\([^"]*\)".*/\1/')"

        # Linux 优先用 tar.gz，macOS 用 zip
        if [ "$OS" = "Linux" ]; then
            DOWNLOAD_URL="$(echo "$API_RESPONSE" | grep '"browser_download_url"' | grep "$ASSET_KEYWORD" | grep '\.tar\.gz"' | head -1 | sed 's/.*"browser_download_url"[[:space:]]*:[[:space:]]*"\([^"]*\)".*/\1/')"
            if [ -n "$DOWNLOAD_URL" ]; then
                USE_TARGZ=1
            fi
        fi

        # 回退到 zip
        if [ -z "$DOWNLOAD_URL" ]; then
            DOWNLOAD_URL="$(echo "$API_RESPONSE" | grep '"browser_download_url"' | grep "$ASSET_KEYWORD" | grep '\.zip"' | head -1 | sed 's/.*"browser_download_url"[[:space:]]*:[[:space:]]*"\([^"]*\)".*/\1/')"
        fi
    fi

    # 回退: 如果 API 失败，使用硬编码
    if [ -z "$VERSION" ]; then
        VERSION="v3.2.0"
        warn "无法连接 GitHub API，使用默认版本 $VERSION"
    fi

    if [ -z "$DOWNLOAD_URL" ]; then
        NORM_VER="${VERSION#v}"
        if [ "$OS" = "Linux" ]; then
            DOWNLOAD_URL="https://github.com/${REPO}/releases/download/${VERSION}/lightweight-his-portable-v${NORM_VER}-${ASSET_KEYWORD}.tar.gz"
            USE_TARGZ=1
        else
            DOWNLOAD_URL="https://github.com/${REPO}/releases/download/${VERSION}/lightweight-his-portable-v${NORM_VER}-${ASSET_KEYWORD}.zip"
        fi
    fi

    info "版本: $VERSION"

    # ── 下载 ──
    mkdir -p "$INSTALL_DIR"
    TMPDIR_DL="$(mktemp -d)"
    DL_FILE="${TMPDIR_DL}/his_release"

    info "正在下载..."
    curl -fSL --progress-bar -o "$DL_FILE" "$DOWNLOAD_URL" || fail "下载失败，请检查网络连接\n  URL: $DOWNLOAD_URL"
    success "下载完成"

    # ── 解压 ──
    info "正在解压..."
    EXTRACT_DIR="${TMPDIR_DL}/extracted"
    mkdir -p "$EXTRACT_DIR"
    if [ "$USE_TARGZ" = "1" ]; then
        tar xzf "$DL_FILE" -C "$EXTRACT_DIR" || fail "解压失败"
    else
        unzip -q -o "$DL_FILE" -d "$EXTRACT_DIR" || fail "解压失败"
    fi

    # 找到解压后的顶层目录（zip 内通常有一层文件夹）
    INNER_DIR="$(find "$EXTRACT_DIR" -mindepth 1 -maxdepth 1 -type d | head -1)"
    if [ -z "$INNER_DIR" ]; then
        INNER_DIR="$EXTRACT_DIR"
    fi

    # ── 安装文件 ──
    info "正在安装到 $INSTALL_DIR ..."
    cp -Rf "$INNER_DIR"/* "$INSTALL_DIR/" 2>/dev/null || true
    rm -rf "$TMPDIR_DL"

    # ── 授权 ──
    if [ "$OS" = "Darwin" ]; then
        info "正在授权..."
        xattr -cr "$INSTALL_DIR" 2>/dev/null || true
    fi
    chmod +x "$INSTALL_DIR/his" 2>/dev/null || true
    chmod +x "$INSTALL_DIR/his_desktop" 2>/dev/null || true
    success "权限已设置"

    # ── 创建全局命令 ──
    info "正在创建 his 命令..."

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
        ln -sf "$INSTALL_DIR/his-wrapper" "$INSTALL_DIR/his-cmd" 2>/dev/null || true
    fi

    # ── 完成 ──
    echo ""
    printf "  ${GREEN}══════════════════════════════════════${RESET}\n"
    success "安装完成！ ($VERSION)"
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
