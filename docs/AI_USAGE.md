# AI 使用指南

## 仓库结构

- `include/`: 头文件
- `src/service/`: 业务服务
- `src/ui/MenuApplication.c`: 应用层聚合和权限约束
- `src/ui/DemoData.c`: 演示数据重置与种子覆盖
- `src/ui/DesktopAdapters.c`: 桌面业务桥接
- `src/ui/workbench/*.c`: 七角色专门界面
- `data/*.txt`: 当前运行数据
- `data/demo_seed/*.txt`: 演示种子数据
- `tests/*.c`: 单测、流程测试、演示数据测试

## 推荐执行路径

1. 看 `tests/`
2. 看 `include/ui/*.h`
3. 看 `src/ui/DesktopAdapters.c` 和 `src/ui/MenuApplication.c`
4. 最后看具体 `workbench`

## 必跑命令

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

## 桌面问题定位顺序

1. `DesktopTheme.*`
2. `WorkbenchCommon.*`
3. `DesktopPages.*`
4. `src/ui/workbench/*.c`

## 约束

- 不要绕过 `DesktopAdapters -> MenuApplication -> service/repository`
- 不要放开患者 ownership 限制
- 不要引入收费/结算
- UI 重叠优先复用 `Workbench_compute_*` helper 修根因
- 搜索选择交互优先复用 `WorkbenchSearchSelectState` 和 `MenuApplication` 里的选择 helper

## 关键测试

- `test_desktop_state`
- `test_desktop_layout_rules`
- `test_menu_application`
- `test_desktop_adapters`
- `test_desktop_workflows`
- `test_patient_workbench_flow`
- `test_clerk_workbench_flow`
- `test_doctor_workbench_flow`
- `test_inpatient_ward_workbench_flow`
- `test_pharmacy_admin_workbench_flow`
- `test_demo_data_integrity`
