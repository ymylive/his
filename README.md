# 轻量级 HIS 课程设计

面向课程设计答辩的轻量级医院信息系统，采用 `C + raylib + raygui + txt repository`，覆盖门诊、检查、住院、药房与管理员总览场景。

## 版本 2.0 - 全面可视化增强 + 跨平台支持 ✨

当前版本已完成：

- **跨平台支持**：Windows、macOS (Apple Silicon)
- 七角色专门桌面工作台，**全新可视化界面**
- **7 个增强 UI 组件**：表单标签、状态徽章、床位网格、数据表格、图标按钮、进度条、搜索框
- **床位网格可视化**：绿色/红色直观显示床位状态
- **库存进度条**：实时显示药品库存水平
- **状态徽章**：彩色标识住院、挂号、发药等各类状态
- 中文字体兜底与 glyph seed 扩展
- 响应式布局 helper，修复按钮/输入框重叠根因
- 患者自助挂号、挂号员接待、医生接诊、住院/病区、药房、管理员跨角色闭环
- 内置答辩型模拟数据
- `28/28` 测试全绿

## 角色工作台（全新可视化界面）

- **患者**：自助挂号、病历/检查摘要、住院状态、发药记录、药品说明
  - ✨ 状态徽章显示挂号/住院/发药状态
  - ✨ 卡片式信息展示，时间线查看记录

- **挂号员**：患者建档与修改、患者查询、挂号办理、挂号查询与取消
  - ✨ 必填字段清晰标识
  - ✨ 统一搜索框体验
  - ✨ 挂号状态徽章（待诊/已完成/已取消）

- **医生**：待诊、患者历史、接诊录入、检查管理、visit handoff 交接
  - ✨ 待诊列表状态徽章
  - ✨ 接诊表单分组展示
  - ✨ 检查状态可视化

- **住院登记员**：入院、出院、住院查询
  - ✨ 入院登记表单优化，必填字段标识
  - ✨ 出院办理 4 步流程指引
  - ✨ 住院状态徽章（住院中/已出院）

- **病区管理员**：病房总览、床位状态、转床、出院前检查
  - ✨ **床位网格可视化**（绿色=可用，红色=占用）
  - ✨ 转床操作 3 步指引
  - ✨ 状态说明图例

- **药房**：药品建档、补货、发药、库存查询、低库存预警
  - ✨ **库存进度条**显示库存水平
  - ✨ 库存状态徽章（充足/低库存/缺货）
  - ✨ 低库存预警醒目展示

- **管理员**：患者总览、科室/医生维护、病房药品总览、系统信息
  - ✨ 患者搜索框优化
  - ✨ 科室医生表单分组
  - ✨ 住院状态徽章

## 演示链路

1. 患者自助挂号 -> 医生接诊 -> 药房发药 -> 患者查看发药与药品说明
2. 患者挂号 -> 医生开检查 -> 回写结果 -> 患者查看病历检查
3. 患者挂号 -> 医生建议住院 -> 住院登记员办理入院 -> 病区管理员转床/检查 -> 办理出院

## 快速开始

### Windows
```powershell
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
.\build\his_desktop.exe
```

### macOS
```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
./build/his_desktop
```

### Linux
```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
./build/his_desktop
```

## 文档

- [安装依赖](docs/INSTALL.md)
- [构建与运行](docs/BUILD_AND_RUN.md)
- [macOS 支持](docs/MACOS_SUPPORT.md) ✨ 新增
- [AI 使用](docs/AI_USAGE.md)
- [界面改进详情](UI_IMPROVEMENTS.md) ✨ 新增
- [更新日志](CHANGELOG.md)

## 演示账号

- `ADM0001 / admin123`
- `CLK0001 / clerk123`
- `DOC0001 / doctor123`
- `INP0001 / inpatient123`
- `WRD0001 / ward123`
- `PHA0001 / pharmacy123`
- `PAT0001 / patient123`

## 打包便携版

### Windows
```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\package-release.ps1
```

### macOS
```bash
./scripts/package-release-macos.sh
```

产物输出到 `dist/`。

## Docker

```bash
docker build -t lightweight-his:latest .
docker run --rm -it lightweight-his:latest
```

说明：Docker 主要用于构建、测试和控制台演示，不作为桌面 GUI 默认运行环境。
