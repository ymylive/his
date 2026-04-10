# macOS 支持文档

## 概述

Lightweight HIS 现已支持 macOS 平台，提供与 Windows 版本相同的功能体验。

## 系统要求

- **操作系统**: macOS 12.0 (Monterey) 或更高版本
- **架构**: Apple Silicon (M1/M2/M3) 或 Intel x86_64
- **内存**: 至少 4GB RAM
- **存储**: 至少 100MB 可用空间

## 安装方式

### 方式一：下载预编译版本（推荐）

1. 访问 [Releases 页面](https://github.com/ymylive/his/releases)
2. 下载最新的 `lightweight-his-portable-v*-macos-arm64.zip`
3. 解压到任意目录
4. **重要**：运行配置脚本（首次必须）
   ```bash
   cd lightweight-his-portable-v*-macos-arm64
   ./setup-macos.sh
   ```
5. 启动应用
   ```bash
   ./run_desktop.sh
   ```

**注意**：由于应用未经 Apple 签名，首次运行前必须配置安全设置。详见 [macOS 安全配置指南](MACOS_SECURITY.md)。

### 方式二：从源码编译

```bash
# 安装依赖
brew install cmake ninja

# 克隆仓库
git clone https://github.com/ymylive/his.git
cd his

# 构建
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(sysctl -n hw.ncpu)

# 运行
./build/his_desktop
```

## macOS 特定配置

### 安全与隐私

⚠️ **重要**：由于应用未经 Apple 开发者签名和公证，macOS 会阻止运行。

**推荐解决方案**：使用配置脚本
```bash
./setup-macos.sh
```

**手动配置**：
```bash
# 移除隔离属性
xattr -cr .

# 添加执行权限
chmod +x run_desktop.sh his_desktop
```

**详细说明**：请参阅 [macOS 安全配置指南](MACOS_SECURITY.md)，包含：
- 为什么会被阻止
- 多种解决方案
- 安全性说明
- 常见问题解答

### 文件权限

确保启动脚本有执行权限：

```bash
chmod +x run_desktop.sh run_console.sh
chmod +x his his_desktop
```

## 平台差异

### 已适配功能

- ✅ 图形界面 (raylib)
- ✅ 控制台界面
- ✅ 所有业务功能
- ✅ 数据持久化
- ✅ 中文显示

### 已知限制

- 窗口标题栏使用系统默认样式（与 Windows 略有不同）
- 某些快捷键可能与系统快捷键冲突

## 目录结构

```
lightweight-his-portable-v2.0.0-macos-arm64/
├── his                    # 控制台版本可执行文件
├── his_desktop            # 桌面版可执行文件
├── run_desktop.sh         # 桌面版启动脚本
├── run_console.sh         # 控制台版启动脚本
├── data/                  # 数据目录
│   ├── patients.csv
│   ├── doctors.csv
│   └── ...
├── README.md
├── CHANGELOG.md
└── START_HERE.txt

## 启动方式

### 桌面版（推荐）

```bash
./run_desktop.sh
```

或在 Finder 中双击 `run_desktop.sh`

### 控制台版

```bash
./run_console.sh
```

## 故障排查

### 问题：应用无法启动

**症状**：双击后没有反应

**解决方案**：
1. 检查文件权限：`ls -l his_desktop`
2. 添加执行权限：`chmod +x his_desktop`
3. 从终端运行查看错误：`./his_desktop`

### 问题：提示"已损坏"

**症状**：macOS 提示应用已损坏无法打开

**解决方案**：
```bash
xattr -cr .
```

### 问题：中文显示乱码

**症状**：界面中文显示为方块或乱码

**解决方案**：
- 确保 `data/` 目录与可执行文件在同一目录
- 检查字体文件是否存在

### 问题：数据文件无法保存

**症状**：修改后的数据没有保存

**解决方案**：
1. 检查 `data/` 目录权限：`ls -ld data`
2. 添加写权限：`chmod -R u+w data`

## 性能优化

### Apple Silicon (M1/M2/M3)

- 原生 ARM64 编译，性能最优
- 无需 Rosetta 2 转译

### Intel Mac

- 如需 Intel 版本，请从源码编译：
  ```bash
  cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_OSX_ARCHITECTURES=x86_64
  cmake --build build
  ```

## 开发者信息

### 构建 Universal Binary

如需同时支持 Intel 和 Apple Silicon：

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
cmake --build build
```

### 打包脚本

使用项目提供的打包脚本：

```bash
./scripts/package-release-macos.sh [version]
```

生成的 zip 文件位于 `dist/` 目录。

## 反馈与支持

如遇到 macOS 特定问题，请在 [GitHub Issues](https://github.com/ymylive/his/issues) 提交，并标注 `macOS` 标签。

提交问题时请包含：
- macOS 版本（如 macOS 14.2）
- 芯片类型（Apple Silicon 或 Intel）
- 错误信息或截图
- 复现步骤
