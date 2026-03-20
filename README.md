# 轻量级 HIS 课程设计

本仓库用于实现一个面向课程设计答辩的轻量级医院信息管理系统（HIS）。系统采用纯 `C` 语言开发，使用链表组织主要业务数据，使用 `txt` 文件完成持久化，覆盖门诊与基础住院场景。

## 课题范围

系统当前规划覆盖以下业务模块：

- 患者信息管理
- 医生与科室管理
- 医疗记录管理
- 病房与床位管理
- 药房与药品管理
- 基础统计与日志
- 基础住院流程

系统明确不包含收费、缴费、结算等费用相关功能。

## 硬性约束

- 所有源码和测试统一使用 `C`
- 核心实体管理基于链表实现
- 数据持久化统一采用 `txt` 文本文件
- 源码保留必要注释，满足代码检查要求
- 需准备现场演示与答辩所需原始数据

## 当前落地进度

当前已经完成第一阶段公共基础设施：

- `CMake + CTest` 工程骨架
- 程序入口 `src/main.c`
- 统一结果结构 `Result`
- 输入校验工具 `InputHelper`
- 编号生成工具 `IdGenerator`
- 通用链表基础设施 `LinkedList`
- 单元测试 `test_result`、`test_common`

当前已经完成第二阶段基础建模增量：

- 核心领域模型 `Department / Doctor / Patient / Registration`
- 门诊记录模型 `VisitRecord / ExaminationRecord / Prescription / DispenseRecord / Medicine`
- `Registration` 状态流转与时间戳约束
- 通用 `txt` 仓储基座 `RepositoryUtils / TextFileRepository`
- 单元测试 `test_domain`、`test_repository_utils`

当前已经完成第三阶段仓储落地增量：

- 患者仓储 `PatientRepository`
- 医生/科室/药品仓储 `DoctorRepository / DepartmentRepository / MedicineRepository`
- 挂号与记录仓储 `RegistrationRepository / VisitRecordRepository / ExaminationRecordRepository / DispenseRecordRepository`
- 单元测试 `test_patient_repository`、`test_doctor_repository`、`test_registration_repository`

当前已经完成第四阶段门诊服务增量：

- 患者服务 `PatientService`
- 科室与医生服务 `DepartmentService / DoctorService`
- 挂号服务 `RegistrationService`
- 单元测试 `test_patient_service`、`test_doctor_service`、`test_registration_service`

当前已经完成第五阶段药房服务增量：

- 药房服务 `PharmacyService`
- 支持药品添加、入库、发药、库存查询、低库存提醒
- 发药过程联动 `DispenseRecordRepository` 写入发药记录
- 单元测试 `test_pharmacy_service`

当前已经完成第六阶段住院基础增量：

- 住院领域模型 `Ward / Bed / Admission / InpatientOrder`
- 住院仓储 `WardRepository / BedRepository / AdmissionRepository / InpatientOrderRepository`
- 支持床位状态流转、活动住院冲突校验、住院医嘱持久化
- 单元测试 `test_inpatient_domain`

当前已经完成第七阶段住院服务增量：

- 病房与床位查询服务 `BedService`
- 入院与出院事务服务 `InpatientService`
- 支持病房查询、按病房查看床位、按床位查询当前入住患者
- 支持办理住院、重复入院拦截、出院释放床位与清除住院标记
- 单元测试 `test_bed_service`、`test_inpatient_service`

当前已经完成第八阶段医疗记录服务增量：

- 医疗记录服务 `MedicalRecordService`
- 支持看诊记录新增、修改、删除，并在首次接诊时联动挂号状态更新
- 支持检查记录新增、结果回写、修改、删除
- 支持按患者查询挂号/看诊/检查/住院四类历史记录
- 支持按时间范围查询挂号/看诊/检查/住院四类记录
- 保持挂号生命周期由 `RegistrationService` 管理，住院生命周期由 `InpatientService` 管理
- 单元测试 `test_medical_record_service`

当前已经完成第九阶段控制台菜单增量：

- 菜单控制器 `MenuController`
- 支持系统管理员、挂号员、医生、患者、住院登记员、病区管理员、药房人员七类角色菜单
- 支持主菜单路由、角色菜单路由、非法输入回退与统一操作反馈
- 患者菜单已包含挂号查询、个人看诊/检查/住院/发药历史、药品使用方法入口
- 住院相关菜单已包含病区床位查询、住院登记、出院办理入口
- `src/main.c` 已切换为控制台循环入口
- 单元测试 `test_menu_controller`

当前已经完成第十阶段菜单执行接线增量：

- 菜单应用层 `MenuApplication`
- `src/main.c` 已从“统一反馈”切换为“输入字段采集 + 调用 service + 输出业务结果”
- 已接通的真实动作包括：
- 挂号员：添加患者、查询患者、挂号办理、挂号查询/取消
- 医生：待诊列表、查询患者历史、写入看诊记录、检查记录新增与结果回写、查询药品库存
- 住院登记员/病区管理员：查看病房、查看床位、办理入院、办理出院、按患者查当前住院、按床位查当前入住患者
- 药房：添加药品、药品入库、药品发药、查询库存、库存不足提醒
- 患者：按患者编号查询挂号记录
- 挂号员：修改患者信息
- 应用层集成测试 `test_menu_application`
- 当前刻意未接通的菜单项包括：患者侧发药记录、药品使用方法、独立药品出库、管理员综合维护入口、转床与出院前检查

当前本地统一验证结果：

- `cmake --build build`
- `ctest --test-dir build --output-on-failure`
- 最新结果 `17/17` 测试通过

## 构建与测试

在项目根目录执行：

```powershell
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

## 文档入口

- 规格说明与可行性分析：
  `docs/superpowers/specs/2026-03-20-lightweight-his-design.md`
- 开发计划：
  `docs/superpowers/plans/2026-03-20-lightweight-his.md`

## 设计规模

课程设计目标数据规模至少包括：

- `100` 名门诊患者
- `30` 名住院患者
- `20` 名医生
- `5` 个科室
- `3` 类病房
- `20` 类药品

## 当前目录结构

```text
.
├─ CMakeLists.txt
├─ README.md
├─ data/
├─ docs/
│  └─ superpowers/
│     ├─ plans/
│     └─ specs/
├─ include/
│  └─ common/
├─ src/
│  ├─ common/
│  └─ main.c
└─ tests/
```

## 下一步

下一阶段继续按计划推进：

- 继续补齐管理员菜单、医生检查记录、患者侧个人记录查询的真实执行
- 准备 `data/` 演示数据集，让挂号、住院、药房链路可直接现场演示
- 继续增强住院/病历跨文件保存的一致性补偿逻辑，并补集成验证与答辩材料
