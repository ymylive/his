#!/bin/bash
# test-fonts.sh - 测试字体加载

echo "🔍 检测 macOS 中文字体..."
echo ""

# 定义字体候选列表（与 DesktopTheme.c 保持一致）
FONTS=(
    "/System/Library/Fonts/STHeiti Light.ttc"
    "/System/Library/Fonts/STHeiti Medium.ttc"
    "/System/Library/Fonts/Supplemental/Arial Unicode.ttf"
    "/System/Library/Fonts/ヒラギノ角ゴシック W3.ttc"
    "/System/Library/Fonts/ヒラギノ角ゴシック W4.ttc"
    "/System/Library/Fonts/ヒラギノ明朝 ProN.ttc"
    "/Library/Fonts/NotoSansSC-Regular.ttf"
    "/Library/Fonts/NotoSansCJK-Regular.ttc"
    "/Library/Fonts/SourceHanSansSC-Regular.otf"
    "/Library/Fonts/Arial Unicode.ttf"
    "/usr/local/share/fonts/NotoSansSC-Regular.ttf"
    "/opt/homebrew/share/fonts/NotoSansSC-Regular.ttf"
    "/System/Library/Fonts/PingFang.ttc"
)

FOUND=0
FIRST_FOUND=""

for font in "${FONTS[@]}"; do
    if [ -f "$font" ]; then
        echo "✅ 找到: $font"
        if [ -z "$FIRST_FOUND" ]; then
            FIRST_FOUND="$font"
        fi
        FOUND=$((FOUND + 1))
    else
        echo "❌ 缺失: $font"
    fi
done

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
if [ $FOUND -gt 0 ]; then
    echo "✅ 找到 $FOUND 个可用字体"
    echo "📝 将使用: $FIRST_FOUND"
    echo ""
    echo "字体信息:"
    ls -lh "$FIRST_FOUND"
else
    echo "❌ 未找到任何中文字体！"
    echo ""
    echo "建议安装 Noto Sans SC："
    echo "  brew tap homebrew/cask-fonts"
    echo "  brew install --cask font-noto-sans-cjk-sc"
fi
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
