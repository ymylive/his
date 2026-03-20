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

- 落地医疗记录、药房、住院相关服务层
- 落地住院、病房、床位领域与服务层
- 补齐测试数据、集成验证与答辩材料
