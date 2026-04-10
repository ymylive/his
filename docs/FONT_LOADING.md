# 字体加载说明

## 概述

Lightweight HIS 使用平台特定的字体路径来加载中文字体，确保在不同操作系统上都能正确显示中文界面。

## 字体加载逻辑

### Windows

自动检测以下字体（按优先级）：
1. `C:/Windows/Fonts/NotoSansSC-VF.ttf` - Noto Sans 简体中文（可变字体）
2. `C:/Windows/Fonts/msyhl.ttc` - 微软雅黑 Light
3. `C:/Windows/Fonts/msyhbd.ttc` - 微软雅黑 Bold
4. `C:/Windows/Fonts/Deng.ttf` - 等线
5. `C:/Windows/Fonts/Dengb.ttf` - 等线 Bold
6. `C:/Windows/Fonts/simhei.ttf` - 黑体
7. `C:/Windows/Fonts/simfang.ttf` - 仿宋
8. `C:/Windows/Fonts/simkai.ttf` - 楷体
9. `C:/Windows/Fonts/msyh.ttc` - 微软雅黑
10. `C:/Windows/Fonts/simsun.ttc` - 宋体
11. `C:/Windows/Fonts/simsunb.ttf` - 宋体 Bold

### macOS

自动检测以下字体（按优先级）：
1. `/System/Library/Fonts/STHeiti Light.ttc` - 华文黑体 Light ✅
2. `/System/Library/Fonts/STHeiti Medium.ttc` - 华文黑体 Medium ✅
3. `/System/Library/Fonts/Supplemental/Arial Unicode.ttf` - Arial Unicode ✅
4. `/System/Library/Fonts/ヒラギノ角ゴシック W3.ttc` - Hiragino Sans ✅
5. `/System/Library/Fonts/ヒラギノ角ゴシック W4.ttc` - Hiragino Sans ✅
6. `/System/Library/Fonts/ヒラギノ明朝 ProN.ttc` - Hiragino Mincho ✅
7. `/Library/Fonts/NotoSansSC-Regular.ttf` - Noto Sans 简体中文（用户安装）
8. `/Library/Fonts/NotoSansCJK-Regular.ttc` - Noto Sans CJK（用户安装）
9. `/Library/Fonts/SourceHanSansSC-Regular.otf` - 思源黑体（用户安装）
10. `/Library/Fonts/Arial Unicode.ttf` - Arial Unicode（用户安装）✅
11. `/usr/local/share/fonts/NotoSansSC-Regular.ttf` - Homebrew 安装
12. `/opt/homebrew/share/fonts/NotoSansSC-Regular.ttf` - Homebrew 安装（Apple Silicon）
13. `/System/Library/Fonts/PingFang.ttc` - 苹方（备用）

✅ = 系统默认包含

### Linux

自动检测以下字体（按优先级）：
1. `/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc`
2. `/usr/share/fonts/truetype/noto/NotoSansCJK-Regular.ttc`
3. `/usr/share/fonts/truetype/droid/DroidSansFallbackFull.ttf`
4. `/usr/share/fonts/truetype/wqy/wqy-microhei.ttc` - 文泉驿微米黑
5. `/usr/share/fonts/truetype/wqy/wqy-zenhei.ttc` - 文泉驿正黑
6. `/usr/share/fonts/opentype/noto/NotoSansSC-Regular.otf`
7. `/usr/share/fonts/truetype/arphic/uming.ttc` - AR PL UMing

## 字体加载流程

1. **检测阶段**：按优先级顺序检查每个字体文件是否存在
2. **加载阶段**：使用第一个找到的字体文件
3. **字符集生成**：根据 UI 文本生成所需的字符集
4. **渲染优化**：应用双线性过滤提升渲染质量

## 测试字体

使用提供的测试脚本检查系统中可用的字体：

```bash
./scripts/test-fonts.sh
```

输出示例：
```
🔍 检测 macOS 中文字体...

✅ 找到: /System/Library/Fonts/STHeiti Light.ttc
✅ 找到: /System/Library/Fonts/STHeiti Medium.ttc
...

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
✅ 找到 7 个可用字体
📝 将使用: /System/Library/Fonts/STHeiti Light.ttc
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
```

## 故障排查

### 问题：中文显示为方块或乱码

**原因**：未找到可用的中文字体

**解决方案**：

#### macOS
系统默认字体应该足够，如果仍有问题，安装 Noto Sans CJK：
```bash
brew tap homebrew/cask-fonts
brew install --cask font-noto-sans-cjk-sc
```

#### Windows
Windows 10/11 默认包含微软雅黑，如果缺失：
1. 下载 [Noto Sans SC](https://fonts.google.com/noto/specimen/Noto+Sans+SC)
2. 安装到 `C:/Windows/Fonts/`

#### Linux
```bash
# Ubuntu/Debian
sudo apt install fonts-noto-cjk

# Fedora/RHEL
sudo dnf install google-noto-sans-cjk-fonts

# Arch Linux
sudo pacman -S noto-fonts-cjk
```

### 问题：字体加载失败

**检查步骤**：
1. 运行 `./scripts/test-fonts.sh` 检查可用字体
2. 确认至少有一个字体文件存在
3. 检查文件权限（应该可读）
4. 查看应用启动日志

### 问题：字体渲染模糊

**原因**：字体大小或渲染设置不当

**解决方案**：
- 字体大小默认为 40px，可以在代码中调整
- 已应用双线性过滤优化渲染质量
- 高 DPI 显示器可能需要调整缩放

## 技术细节

### 字符集生成

应用会根据 UI 中使用的所有中文文本生成字符集：
- 包含所有界面文本中的汉字
- 包含常用标点符号
- 动态生成，避免加载整个字体

### 内存优化

- 只加载需要的字符，不加载整个字体
- 使用纹理图集存储字形
- 应用关闭时自动卸载字体

### 渲染优化

- 双线性过滤（TEXTURE_FILTER_BILINEAR）
- 抗锯齿渲染
- GPU 加速

## 添加新字体

如果需要添加新的字体路径：

1. 编辑 `src/ui/DesktopTheme.c`
2. 在对应平台的 `DESKTOP_CJK_FONT_CANDIDATES` 数组中添加路径
3. 按优先级排序（推荐的字体放在前面）
4. 重新编译

示例：
```c
#ifdef __APPLE__
static const char *DESKTOP_CJK_FONT_CANDIDATES[] = {
    "/System/Library/Fonts/STHeiti Light.ttc",
    "/path/to/your/custom/font.ttf",  // 添加自定义字体
    // ... 其他字体
};
#endif
```

## 相关文件

- `src/ui/DesktopTheme.c` - 字体加载实现
- `include/ui/DesktopTheme.h` - 字体加载接口
- `scripts/test-fonts.sh` - 字体检测工具

## 参考资料

- [raylib 字体加载文档](https://www.raylib.com/cheatsheet/cheatsheet.html#Font)
- [Noto Fonts](https://fonts.google.com/noto)
- [macOS 字体位置](https://support.apple.com/en-us/HT201722)
- [Windows 字体位置](https://support.microsoft.com/en-us/windows/how-to-install-or-remove-a-font-in-windows-f12d0657-2fc8-7613-c76f-88d043b334b8)
