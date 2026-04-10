#!/bin/bash
# test-font-loading.sh - 测试字体加载

echo "🔍 测试 raylib 字体加载..."
echo ""

# 创建测试程序
cat > /tmp/test_font.c << 'EOF'
#include <stdio.h>
#include <raylib.h>

int main(void) {
    const char *font_path = "/System/Library/Fonts/STHeiti Light.ttc";

    printf("Testing font: %s\n", font_path);
    printf("File exists: %s\n", FileExists(font_path) ? "YES" : "NO");

    if (!FileExists(font_path)) {
        printf("ERROR: Font file not found!\n");
        return 1;
    }

    // Try to load font
    Font font = LoadFontEx(font_path, 40, NULL, 0);

    if (font.texture.id == 0) {
        printf("ERROR: Failed to load font texture!\n");
        return 1;
    }

    if (font.glyphCount == 0) {
        printf("ERROR: Font has no glyphs!\n");
        UnloadFont(font);
        return 1;
    }

    printf("SUCCESS: Font loaded!\n");
    printf("  Texture ID: %d\n", font.texture.id);
    printf("  Glyph count: %d\n", font.glyphCount);
    printf("  Base size: %d\n", font.baseSize);

    UnloadFont(font);
    return 0;
}
EOF

# 编译测试程序
echo "编译测试程序..."
cc /tmp/test_font.c -o /tmp/test_font \
    -I third_party/raylib_vendor/src \
    -L build-release/third_party/raylib_vendor/raylib \
    -lraylib \
    -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo \
    2>&1

if [ $? -eq 0 ]; then
    echo "✅ 编译成功"
    echo ""
    echo "运行测试..."
    /tmp/test_font
else
    echo "❌ 编译失败"
fi
