# HIS 轻量级医院信息系统

面向课程设计答辩演示的轻量级医院信息系统，纯终端 TUI 界面，覆盖挂号、接诊、检查、住院、药房和管理员总览场景。

支持 **Windows**、**macOS**、**Linux** 三平台。

## 快速开始

### Windows 用户

> **Windows 请勿使用 `curl ... | bash` 命令，那是 macOS/Linux 专用的。**

打开 **PowerShell**（推荐使用 Windows Terminal），粘贴：

```powershell
irm https://raw.githubusercontent.com/ymylive/his/main/install.ps1 | iex
```

卸载：

```powershell
irm https://raw.githubusercontent.com/ymylive/his/main/install.ps1 | iex -Args uninstall
```

> 请使用 Windows Terminal 或支持 ANSI/UTF-8 的终端，cmd.exe 可能无法正确显示颜色和 Unicode 字符。

### macOS / Linux 用户

打开终端，粘贴：

```bash
curl -fsSL https://raw.githubusercontent.com/ymylive/his/main/install.sh | bash
```

卸载：

```bash
curl -fsSL https://raw.githubusercontent.com/ymylive/his/main/install.sh | bash -s uninstall
```

> install.sh 自动识别 macOS / Linux，无需区分。

### 一键升级

如果程序内置的"检查更新"失败（例如 v7.2.0 存在 SHA256SUMS 302 重定向问题），可以直接运行独立更新脚本：

**macOS / Linux**
```bash
curl -fsSL https://raw.githubusercontent.com/ymylive/his/main/scripts/update.sh | bash
```

**Windows PowerShell**
```powershell
irm https://raw.githubusercontent.com/ymylive/his/main/scripts/update.ps1 | iex
```

脚本会自动识别平台、下载最新 release、校验 SHA256、备份旧二进制（`his.bak.<timestamp>`）后替换。可以用 `--tag vX.Y.Z` / `--target /path/to/his` 指定版本和路径。

### 手动安装

从 [最新 Release](https://github.com/ymylive/his/releases/latest) 下载对应平台的 zip 包：

| 平台 | 文件名 |
|------|--------|
| Windows x64 | `lightweight-his-portable-vX.Y.Z-win64.zip` |
| macOS Apple Silicon | `lightweight-his-portable-vX.Y.Z-macos-arm64.zip` |
| Linux x86_64 | `lightweight-his-portable-vX.Y.Z-linux-x86_64.zip` |

解压后运行：
- **Windows**: 双击 `run_console.bat` 或运行 `his.exe`
- **macOS**: 先执行 `./setup-macos.sh`，然后 `./run_console.sh`
- **Linux**: `chmod +x his && ./his`

### 源码构建

```bash
git clone https://github.com/ymylive/his.git
cd his
mkdir build && cd build
cmake ..
cmake --build .
./his
```

## 演示账号

| 角色 | 账号 | 密码 |
|------|------|------|
| 管理员 | `ADM0001` | `admin123` |
| 医生 | `DOC0001` | `doctor123` |
| 患者 | `PAT0001` | `patient123` |
| 住院管理员 | `INP0001` | `inpatient123` |
| 药房 | `PHA0001` | `pharmacy123` |

## 推荐演示路线

三条路线覆盖系统全部业务闭环：

1. **门诊闭环**：患者自助挂号 → 医生接诊 → 药房发药 → 患者查看发药记录
2. **检查闭环**：患者挂号 → 医生开立检查 → 回写结果 → 患者查看检查摘要
3. **住院闭环**：患者挂号 → 医生建议住院 → 住院管理员办理入院 → 转床/出院检查 → 办理出院

## 功能总览

| 角色 | 功能 |
|------|------|
| 管理员 | 患者/科室/医生/病房/药品总览与维护 |
| 医生 | 待诊列表、接诊录入、检查管理、处方与发药 |
| 患者 | 自助挂号、看诊/检查/住院/发药记录查询、药品说明 |
| 住院管理 | 入院出院办理、病房床位调度、转床、出院检查 |
| 药房 | 药品建档、入库补货、发药处理、库存预警 |

## TUI 界面特性

- 渐变色 ASCII 艺术 Logo
- 彩虹 Banner 装饰框
- 角色主题配色（每个角色专属颜色和图标）
- 框线彩色表格（病房/床位/低库存/待诊列表）
- 动画系统：打字机效果、菜单滑入、加载旋转器、进度条、过渡动画
- 70+ Unicode 符号图标
- ESC 键快速返回上一级菜单
- 启动时自动检测新版本，支持终端内一键更新

## 演示数据

系统内置完整模拟数据：30 名患者、10 名医生、6 个科室、5 个病区、12 种药品及全流程记录。

恢复初始数据：主菜单选择「重置演示数据」。

## 技术栈

- **语言**: C11
- **构建**: CMake 3.20+
- **界面**: 纯终端 TUI（ANSI + Unicode）
- **存储**: 文本文件仓库（管道分隔格式）
- **平台**: Windows / macOS / Linux
- **依赖**: 无外部依赖
- **测试**: 19 个测试用例

## 更多文档

- [安装依赖](docs/INSTALL.md)
- [构建与运行](docs/BUILD_AND_RUN.md)
- [更新日志](CHANGELOG.md)
