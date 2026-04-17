#!/usr/bin/env bash
# Lightweight HIS one-shot self-updater (macOS / Linux)
#
# Usage:
#   curl -sSL https://raw.githubusercontent.com/ymylive/his/main/scripts/update.sh | bash
#   curl -sSL https://raw.githubusercontent.com/ymylive/his/main/scripts/update.sh | bash -s -- --target ./his
#
# Options:
#   --target <path>   Local his binary to replace (default: ./his, or the one on PATH)
#   --tag <vX.Y.Z>    Install a specific tag (default: latest)
#   --repo <owner/r>  Override repo (default: ymylive/his)

set -euo pipefail

REPO="${HIS_UPDATE_REPO:-ymylive/his}"
TAG=""
TARGET=""

while [ $# -gt 0 ]; do
    case "$1" in
        --target) TARGET="$2"; shift 2 ;;
        --tag)    TAG="$2";    shift 2 ;;
        --repo)   REPO="$2";   shift 2 ;;
        -h|--help)
            sed -n '1,15p' "$0" 2>/dev/null || true
            exit 0 ;;
        *) echo "unknown arg: $1" >&2; exit 2 ;;
    esac
done

log()  { printf '\033[1;36m[his-update]\033[0m %s\n' "$*"; }
warn() { printf '\033[1;33m[his-update]\033[0m %s\n' "$*" >&2; }
err()  { printf '\033[1;31m[his-update]\033[0m %s\n' "$*" >&2; }

command -v curl >/dev/null || { err "curl not found"; exit 1; }
command -v unzip >/dev/null || { err "unzip not found"; exit 1; }

# ── 平台检测 ──────────────────────────────────────────────
uname_s="$(uname -s)"
uname_m="$(uname -m)"
case "$uname_s" in
    Darwin)
        if [ "$uname_m" = "arm64" ] || [ "$uname_m" = "aarch64" ]; then
            PLATFORM_KEY="macos-arm64"
        else
            PLATFORM_KEY="macos-x86_64"
        fi ;;
    Linux)
        PLATFORM_KEY="linux-x86_64" ;;
    *) err "unsupported platform: $uname_s"; exit 1 ;;
esac
log "platform: $PLATFORM_KEY"

# ── 定位目标 ──────────────────────────────────────────────
if [ -z "$TARGET" ]; then
    if [ -x "./his" ]; then
        TARGET="./his"
    elif command -v his >/dev/null; then
        TARGET="$(command -v his)"
    else
        TARGET="./his"
        warn "no existing his binary found; will install to $TARGET"
    fi
fi
log "target:   $TARGET"

# ── 选 tag ────────────────────────────────────────────────
if [ -z "$TAG" ]; then
    log "fetching latest release tag from GitHub API..."
    TAG="$(curl -sSL \
        -H 'Accept: application/vnd.github+json' \
        -H 'User-Agent: his-updater-sh' \
        "https://api.github.com/repos/$REPO/releases/latest" \
        | sed -n 's/.*"tag_name":[[:space:]]*"\([^"]*\)".*/\1/p' \
        | head -n1)"
fi
if [ -z "$TAG" ]; then err "could not resolve latest tag"; exit 1; fi
log "tag:      $TAG"

ASSET_BASE="https://github.com/$REPO/releases/download/$TAG"
ZIP_NAME="lightweight-his-portable-${TAG}-${PLATFORM_KEY}.zip"
ZIP_URL="$ASSET_BASE/$ZIP_NAME"
SUMS_URL="$ASSET_BASE/SHA256SUMS"

# ── 下载 ──────────────────────────────────────────────────
TMPDIR="$(mktemp -d -t his-update.XXXXXX)"
trap 'rm -rf "$TMPDIR"' EXIT
log "downloading $ZIP_NAME..."
curl -fL --proto '=https' --tlsv1.2 -H 'User-Agent: his-updater-sh' \
     -o "$TMPDIR/pkg.zip" "$ZIP_URL"

log "downloading SHA256SUMS..."
curl -fL --proto '=https' --tlsv1.2 -H 'User-Agent: his-updater-sh' \
     -o "$TMPDIR/SHA256SUMS" "$SUMS_URL"

# ── 校验 ──────────────────────────────────────────────────
expected="$(awk -v f="$ZIP_NAME" '$2 == f {print $1}' "$TMPDIR/SHA256SUMS" | head -n1)"
if [ -z "$expected" ]; then
    err "$ZIP_NAME not found in SHA256SUMS"
    exit 1
fi
if command -v shasum >/dev/null; then
    actual="$(shasum -a 256 "$TMPDIR/pkg.zip" | awk '{print $1}')"
else
    actual="$(sha256sum "$TMPDIR/pkg.zip" | awk '{print $1}')"
fi
if [ "$expected" != "$actual" ]; then
    err "SHA-256 mismatch: expected $expected, got $actual"
    exit 1
fi
log "SHA-256 OK"

# ── 解包 + 替换 ───────────────────────────────────────────
log "extracting..."
unzip -q "$TMPDIR/pkg.zip" -d "$TMPDIR/extracted"
new_bin="$(find "$TMPDIR/extracted" -type f -name 'his' -perm -u+x 2>/dev/null | head -n1)"
if [ -z "$new_bin" ]; then
    new_bin="$(find "$TMPDIR/extracted" -type f -name 'his' | head -n1)"
fi
if [ -z "$new_bin" ]; then
    err "his binary not found inside $ZIP_NAME"
    exit 1
fi

chmod +x "$new_bin"

if [ -e "$TARGET" ]; then
    backup="${TARGET}.bak.$(date +%s)"
    log "backup old binary to $backup"
    cp -p "$TARGET" "$backup"
fi

target_dir="$(dirname "$TARGET")"
mkdir -p "$target_dir"
mv "$new_bin" "$TARGET"
chmod +x "$TARGET"

log "updated $TARGET → $TAG"
"$TARGET" --version 2>/dev/null || true
