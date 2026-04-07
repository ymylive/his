# HIS 轻量级医院信息系统

面向课程设计答辩演示的轻量级医院信息系统，纯终端 TUI 界面，覆盖挂号、接诊、检查、住院、药房和管理员总览场景。

## 快速开始

### macOS — 一键安装

打开终端，粘贴：

```bash
curl -fsSL https://raw.githubusercontent.com/ymylive/his/main/install.sh | bash
```

安装完成后，随时输入 `his` 即可启动。

### macOS — 卸载

```bash
curl -fsSL https://raw.githubusercontent.com/ymylive/his/main/install.sh | bash -s uninstall
```

### Windows

1. 从 [最新 Release](https://github.com/ymylive/his/releases/latest) 下载 `his-v3.0.0-win64.zip`
2. 解压到任意目录
3. 打开 **Windows Terminal**，进入解压目录运行 `his.exe`

> ⚠️ Windows 请使用 Windows Terminal 或支持 ANSI/UTF-8 的终端，cmd.exe 可能无法正确显示颜色和 Unicode 字符。

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
| 🛡️ 管理员 | 患者/科室/医生/病房/药品总览与维护 |
| ⚕️ 医生 | 待诊列表、接诊录入、检查管理、处方与发药 |
| ♥️ 患者 | 自助挂号、看诊/检查/住院/发药记录查询、药品说明 |
| 🏥 住院管理 | 入院出院办理、病房床位调度、转床、出院检查 |
| 💊 药房 | 药品建档、入库补货、发药处理、库存预警 |

## TUI 界面特性

- 渐变色 ASCII 艺术 Logo
- 彩虹 Banner 装饰框
- 角色主题配色（每个角色专属颜色和图标）
- 框线彩色表格（病房/床位/低库存/待诊列表）
- 动画系统：打字机效果、菜单滑入、加载旋转器、进度条、过渡动画
- 70+ Unicode 符号图标

## 演示数据

系统内置完整模拟数据：30 名患者、10 名医生、6 个科室、5 个病区、12 种药品及全流程记录。

恢复初始数据：主菜单选择「重置演示数据」。

## 技术栈

- **语言**: C11
- **构建**: CMake 3.20+
- **界面**: 纯终端 TUI（ANSI + Unicode）
- **存储**: 文本文件仓库（管道分隔格式）
- **依赖**: 无外部依赖
- **测试**: 19 个测试用例

## 更多文档

- [安装依赖](docs/INSTALL.md)
- [构建与运行](docs/BUILD_AND_RUN.md)
- [更新日志](CHANGELOG.md)
