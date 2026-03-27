# 轻量级 HIS 课程设计

面向课程设计答辩的轻量级医院信息系统，采用 `C + raylib + raygui + txt repository`，覆盖门诊、检查、住院、药房与管理员总览场景。

当前版本已完成：

- 七角色专门桌面工作台
- 中文字体兜底与 glyph seed 扩展
- 响应式布局 helper，修复按钮/输入框重叠根因
- 患者自助挂号、挂号员接待、医生接诊、住院/病区、药房、管理员跨角色闭环
- 内置答辩型模拟数据
- `28/28` 测试全绿

## 角色工作台

- 患者：自助挂号、病历/检查摘要、住院状态、发药记录、药品说明
- 挂号员：患者建档与修改、患者查询、挂号办理、挂号查询与取消
- 医生：待诊、患者历史、接诊录入、检查管理、visit handoff 交接
- 住院登记员：入院、出院、住院查询
- 病区管理员：病房总览、床位状态、转床、出院前检查
- 药房：药品建档、补货、发药、库存查询、低库存预警
- 管理员：患者总览、科室/医生维护、病房药品总览、系统信息

## 演示链路

1. 患者自助挂号 -> 医生接诊 -> 药房发药 -> 患者查看发药与药品说明
2. 患者挂号 -> 医生开检查 -> 回写结果 -> 患者查看病历检查
3. 患者挂号 -> 医生建议住院 -> 住院登记员办理入院 -> 病区管理员转床/检查 -> 办理出院

## 快速开始

```powershell
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
.\build\his_desktop.exe
```

## 文档

- [安装依赖](docs/INSTALL.md)
- [构建与运行](docs/BUILD_AND_RUN.md)
- [AI 使用](docs/AI_USAGE.md)

## 演示账号

- `ADM0001 / admin123`
- `CLK0001 / clerk123`
- `DOC0001 / doctor123`
- `INP0001 / inpatient123`
- `WRD0001 / ward123`
- `PHA0001 / pharmacy123`
- `PAT0001 / patient123`

## 打包便携版

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\package-release.ps1
```

产物输出到 `dist/`。

## Docker

```bash
docker build -t lightweight-his:latest .
docker run --rm -it lightweight-his:latest
```

说明：Docker 主要用于构建、测试和控制台演示，不作为桌面 GUI 默认运行环境。
