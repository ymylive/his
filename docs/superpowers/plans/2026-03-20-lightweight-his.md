# Lightweight HIS Implementation Plan

> **For agentic workers:** REQUIRED: Use superpowers:subagent-driven-development (if subagents available) or superpowers:executing-plans to implement this plan. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a lightweight HIS for a small hospital with complete outpatient and basic inpatient flow, using C and txt files.

**Architecture:** Use a single-machine layered architecture with UI, application services, domain structs, and txt repositories. Implement outpatient and inpatient capabilities with `.h/.c` module pairs, `struct + enum + function` style boundaries, and linked-list-backed in-memory collections so the team can land the outpatient MVP first and then extend to inpatient, reporting, and conditional analytics.

**Tech Stack:** C11, CMake, standard C library, txt data files, CTest, Windows PowerShell

---

## 语言硬约束

- 所有业务源码必须使用 `C`，文件后缀统一为 `.c/.h`
- 所有测试源码也使用 `C`
- 不引入 `C++`、`Java`、`Python` 作为核心实现或辅助生成脚本
- 若需要数据初始化程序，也必须使用 `C`
- 用于代码检查的 C 源码必须包含必要代码注释
- 全程以链表作为核心数据组织方式实现主要实体管理

## 课程交付约束

- 团队规模按 `3-4` 人进行分工设计
- 最终除源码与数据外，还需提交总结报告
- 总结报告至少包含：测试方案、成员分工、完成情况、演示说明
- 最后 2 次实验课要面向代码检查和现场答辩准备材料
- 若教师要求增强版或团队中包含补修/重修学生，需加入数据分析与预测功能

## 题签框架摘要

### 管理员视角

- 患者信息管理：添加、修改、查询患者，覆盖患者编号、姓名、性别、年龄、联系方式、是否住院
- 医生与科室管理：添加医生、查询医生、维护科室信息，覆盖工号、姓名、职称、科室、出诊安排
- 医疗记录管理：覆盖挂号记录与住院记录，支持新增、修改、删除、按患者查询、按时间范围查询
- 病房与床位管理：查看病房信息、查看床位状态、分配床位、出院释放床位
- 药房与药品管理：添加药品、入库、出库/发药、查询库存、库存不足提醒

### 患者视角

- 基本信息：登记、修改、查询患者基本信息，补充证件信息、过敏史或备注字段
- 挂号：选择医生挂号、查看挂号信息、取消挂号
- 看诊：查看医生诊断结果、医生建议，并记录是否需要检查、住院或开药
- 检查：添加检查项目、查看检查结果
- 住院：办理住院登记、办理出院
- 取药：查询发药记录、查看药品使用方法
- 历史记录：查询个人看诊、检查、住院、发药记录

### 医生视角

- 查询患者信息与历史：查看患者基本信息、挂号记录、历史就诊记录和过敏史/备注
- 诊疗记录：添加、修改和查看诊断记录、医生建议
- 处方与发药：开具处方、查询药品库存、执行发药操作
- 检查记录：查看患者检查记录、记录新的检查项目与检查结果

### 合并原则

- 题签中的管理员、患者、医生三类框架已并入本计划的任务拆分和菜单设计
- 由于当前项目已明确去除缴费相关功能，题签中涉及“费用”的表述仅保留为记录字段参考，不扩展为收费、账单或结算模块

## 文件结构规划

### 计划创建目录

- `include/common/`
- `include/domain/`
- `include/service/`
- `include/repository/`
- `include/ui/`
- `src/common/`
- `src/domain/`
- `src/service/`
- `src/repository/`
- `src/ui/`
- `tests/`
- `data/`

### 计划创建核心文件

- `CMakeLists.txt`
- `src/main.c`
- `include/common/InputHelper.h`
- `include/common/IdGenerator.h`
- `include/common/Result.h`
- `include/common/LinkedList.h`
- `include/domain/*.h`
- `include/service/*.h`
- `include/repository/*.h`
- `include/ui/MenuController.h`
- `src/common/*.c`
- `src/domain/*.c`
- `src/service/*.c`
- `src/repository/*.c`
- `src/ui/MenuController.c`
- `tests/test_*.c`
- `data/*.txt`

### 计划新增住院相关文件

- `include/domain/Ward.h`
- `include/domain/Bed.h`
- `include/domain/Admission.h`
- `include/domain/InpatientOrder.h`
- `include/repository/InpatientOrderRepository.h`
- `include/repository/BedRepository.h`
- `include/repository/AdmissionRepository.h`
- `include/service/InpatientService.h`
- `include/service/BedService.h`
- `src/domain/Ward.c`
- `src/domain/Bed.c`
- `src/domain/Admission.c`
- `src/domain/InpatientOrder.c`
- `src/repository/InpatientOrderRepository.c`
- `src/repository/BedRepository.c`
- `src/repository/AdmissionRepository.c`
- `src/service/InpatientService.c`
- `src/service/BedService.c`
- `tests/test_inpatient_service.c`
- `tests/test_bed_service.c`

## Chunk 1: 工程骨架与公共能力

### Task 1: 初始化工程与构建脚本

**Files:**
- Create: `CMakeLists.txt`
- Create: `src/main.c`
- Create: `include/common/Result.h`
- Create: `include/common/InputHelper.h`
- Create: `src/common/InputHelper.c`
- Create: `src/common/Result.c`
- Create: `include/common/LinkedList.h`
- Create: `src/common/LinkedList.c`
- Test: `tests/test_input_helper.c`

- [x] **Step 1: 写出最小失败测试**

```c
#include <assert.h>
#include <string.h>
#include "common/Result.h"

int main(void) {
    Result ok = Result_make_success("ok");
    assert(ok.success == 1);
    assert(strcmp(ok.message, "ok") == 0);
    return 0;
}
```

- [x] **Step 2: 运行测试确认当前失败**

Run: `cmake -S . -B build`
Expected: configure succeeds after `CMakeLists.txt` is added, but test target is not ready yet

- [x] **Step 3: 实现最小可运行骨架**

```c
typedef struct {
    int success;
    char message[64];
} Result;

Result Result_make_success(const char *msg);
Result Result_make_failure(const char *msg);
```

- [x] **Step 4: 接入 CTest**

Run: `cmake -S . -B build && cmake --build build`
Expected: project and test executable build successfully

- [x] **Step 5: 执行测试**

Run: `ctest --test-dir build --output-on-failure`
Expected: `100% tests passed`

- [ ] **Step 6: 提交阶段性成果**

```bash
git add CMakeLists.txt include/common src/common src/main.c tests
git commit -m "chore: bootstrap lightweight his project"
```

### Task 2: 统一输入校验、主键生成与链表基础设施

**Files:**
- Create: `include/common/IdGenerator.h`
- Create: `src/common/IdGenerator.c`
- Modify: `include/common/InputHelper.h`
- Modify: `src/common/InputHelper.c`
- Modify: `include/common/LinkedList.h`
- Modify: `src/common/LinkedList.c`
- Test: `tests/test_common.c`

- [x] **Step 1: 写输入与编号测试**
- [x] **Step 2: 支持整数、金额、非空字符串校验**
- [x] **Step 3: 支持前缀型主键生成，如 `PAT0001`**
- [x] **Step 4: 实现通用链表节点的新增、删除、查找、遍历能力**
- [x] **Step 5: 针对头结点、尾结点和空链表写链表边界测试**
- [x] **Step 6: 运行 `ctest --test-dir build --output-on-failure`**
- [ ] **Step 7: 提交**

## Chunk 2: 领域模型与 txt 仓储

### Task 3: 建立核心领域对象

**Files:**
- Create: `include/domain/User.h`
- Create: `include/domain/Department.h`
- Create: `include/domain/Doctor.h`
- Create: `include/domain/Patient.h`
- Create: `include/domain/Registration.h`
- Create: `include/domain/VisitRecord.h`
- Create: `include/domain/ExaminationRecord.h`
- Create: `include/domain/Prescription.h`
- Create: `include/domain/DispenseRecord.h`
- Create: `include/domain/Medicine.h`
- Create: `src/domain/Registration.c`
- Create: `src/domain/VisitRecord.c`
- Create: `src/domain/ExaminationRecord.c`
- Create: `src/domain/DispenseRecord.c`
- Test: `tests/test_domain.c`

- [x] **Step 1: 为状态流转最复杂的 `Registration` 写测试**

```c
assert(reg.status == REG_STATUS_PENDING);
Registration_mark_diagnosed(&reg);
assert(reg.status == REG_STATUS_DIAGNOSED);
```

- [x] **Step 2: 实现患者、医生、科室、挂号、看诊记录、检查记录、处方、发药记录、药品等结构体与枚举**
- [x] **Step 3: 加入金额、数量、库存等边界校验**
- [x] **Step 4: 为每类核心实体设计链表节点挂接方式**
- [x] **Step 5: 构建并运行 `ctest --test-dir build --output-on-failure`**
- [ ] **Step 6: 提交**

### Task 4: 建立通用 txt 仓储基类

**Files:**
- Create: `include/repository/TextFileRepository.h`
- Create: `src/repository/TextFileRepository.c`
- Create: `include/repository/RepositoryUtils.h`
- Create: `src/repository/RepositoryUtils.c`
- Test: `tests/test_repository_utils.c`

- [x] **Step 1: 写字段切分、字段数校验、空行过滤测试**
- [x] **Step 2: 实现按 `|` 分隔的解析逻辑**
- [x] **Step 3: 实现文件不存在时的安全创建策略**
- [x] **Step 4: 运行 `ctest --test-dir build --output-on-failure`**
- [ ] **Step 5: 提交**

### Task 5: 为患者、医生、科室、挂号、记录和药品建立专用仓储

**Files:**
- Create: `include/repository/DepartmentRepository.h`
- Create: `include/repository/DoctorRepository.h`
- Create: `include/repository/PatientRepository.h`
- Create: `include/repository/RegistrationRepository.h`
- Create: `include/repository/VisitRecordRepository.h`
- Create: `include/repository/ExaminationRecordRepository.h`
- Create: `include/repository/DispenseRecordRepository.h`
- Create: `include/repository/MedicineRepository.h`
- Create: `src/repository/DepartmentRepository.c`
- Create: `src/repository/DoctorRepository.c`
- Create: `src/repository/PatientRepository.c`
- Create: `src/repository/RegistrationRepository.c`
- Create: `src/repository/VisitRecordRepository.c`
- Create: `src/repository/ExaminationRecordRepository.c`
- Create: `src/repository/DispenseRecordRepository.c`
- Create: `src/repository/MedicineRepository.c`
- Test: `tests/test_doctor_repository.c`
- Test: `tests/test_patient_repository.c`
- Test: `tests/test_registration_repository.c`

- [ ] **Step 1: 用临时 txt 文件写读写回归测试**
- [ ] **Step 2: 实现查单条、查列表、追加保存、整表回写**
- [ ] **Step 3: 补齐医生、科室、看诊记录、检查记录对应仓储接口**
- [ ] **Step 4: 加入主键重复检测**
- [ ] **Step 5: 运行 `ctest --test-dir build --output-on-failure`**
- [ ] **Step 6: 提交**

## Chunk 3: 门诊核心业务闭环

### Task 6: 患者信息管理服务

**Files:**
- Create: `include/service/PatientService.h`
- Create: `src/service/PatientService.c`
- Modify: `include/repository/PatientRepository.h`
- Test: `tests/test_patient_service.c`

- [ ] **Step 1: 写“患者新增、修改、删除、查询”的失败测试**
- [ ] **Step 2: 实现患者编号、姓名、性别、年龄、联系方式、证件信息、过敏史/备注、是否住院等字段校验**
- [ ] **Step 3: 实现按编号、姓名、手机号、证件号查询患者**
- [ ] **Step 4: 处理重复手机号或重复身份证号规则（如题目未要求身份证，可先仅校验手机号）**
- [ ] **Step 5: 运行 `ctest --test-dir build --output-on-failure`**
- [ ] **Step 6: 提交**

### Task 7: 医生与科室管理服务

**Files:**
- Create: `include/service/DepartmentService.h`
- Create: `include/service/DoctorService.h`
- Create: `src/service/DepartmentService.c`
- Create: `src/service/DoctorService.c`
- Modify: `include/repository/DepartmentRepository.h`
- Modify: `include/repository/DoctorRepository.h`
- Test: `tests/test_doctor_service.c`
- Test: `tests/test_department_service.c`

- [ ] **Step 1: 写“添加医生、查询医生、按科室查看医生、维护科室信息”的失败测试**
- [ ] **Step 2: 实现医生工号、姓名、职称、科室、出诊安排等字段校验**
- [ ] **Step 3: 实现科室新增、修改、查询和医生按科室过滤**
- [ ] **Step 4: 运行 `ctest --test-dir build --output-on-failure`**
- [ ] **Step 5: 提交**

### Task 8: 挂号服务

**Files:**
- Create: `include/service/RegistrationService.h`
- Create: `src/service/RegistrationService.c`
- Modify: `include/domain/Registration.h`
- Modify: `include/repository/RegistrationRepository.h`
- Test: `tests/test_registration_service.c`

- [ ] **Step 1: 写挂号、退号、接诊前状态校验测试**
- [ ] **Step 2: 实现选择医生挂号、挂号时间记录与挂号单创建**
- [ ] **Step 3: 实现查看挂号信息与取消挂号，并限制已接诊记录不可退号**
- [ ] **Step 4: 运行 `ctest --test-dir build --output-on-failure`**
- [ ] **Step 5: 提交**

### Task 9: 医疗记录管理服务

**Files:**
- Create: `include/repository/VisitRecordRepository.h`
- Create: `include/repository/ExaminationRecordRepository.h`
- Create: `src/repository/VisitRecordRepository.c`
- Create: `src/repository/ExaminationRecordRepository.c`
- Create: `include/service/MedicalRecordService.h`
- Create: `src/service/MedicalRecordService.c`
- Test: `tests/test_medical_record_service.c`

- [ ] **Step 1: 写挂号记录、看诊记录、检查记录、住院记录四类记录的增删改查测试**
- [ ] **Step 2: 实现诊断结果、医生建议，以及是否需要检查、住院或开药等标记字段**
- [ ] **Step 3: 实现按患者查询历史记录和按时间范围查询记录，并将接诊保存映射到看诊记录、住院登记映射到住院记录**
- [ ] **Step 4: 补充检查项目新增、检查结果回写、检查记录修改与删除能力**
- [ ] **Step 5: 运行 `ctest --test-dir build --output-on-failure`**
- [ ] **Step 6: 提交**

### Task 10: 药房与药品管理服务

**Files:**
- Create: `include/service/PharmacyService.h`
- Create: `src/service/PharmacyService.c`
- Modify: `include/repository/MedicineRepository.h`
- Test: `tests/test_pharmacy_service.c`

- [ ] **Step 1: 写“添加药品、药品入库、药品出库/发药、查询库存、库存不足提醒”的失败测试**
- [ ] **Step 2: 实现药品编号、药品名、单价、库存、所属科室等字段校验**
- [ ] **Step 3: 实现药品入库、出库、发药扣减与发药记录写入逻辑**
- [ ] **Step 4: 实现库存查询、低库存提醒、发药记录查询与药品使用方法查看**
- [ ] **Step 5: 运行 `ctest --test-dir build --output-on-failure`**
- [ ] **Step 6: 提交**

## Chunk 4: 基础住院业务闭环

### Task 11: 住院领域模型与仓储

**Files:**
- Create: `include/domain/Ward.h`
- Create: `include/domain/Bed.h`
- Create: `include/domain/Admission.h`
- Create: `include/domain/InpatientOrder.h`
- Create: `src/domain/Ward.c`
- Create: `src/domain/Bed.c`
- Create: `src/domain/Admission.c`
- Create: `include/repository/WardRepository.h`
- Create: `include/repository/InpatientOrderRepository.h`
- Create: `include/repository/BedRepository.h`
- Create: `include/repository/AdmissionRepository.h`
- Create: `src/domain/InpatientOrder.c`
- Create: `src/repository/WardRepository.c`
- Create: `src/repository/BedRepository.c`
- Create: `src/repository/AdmissionRepository.c`
- Create: `src/repository/InpatientOrderRepository.c`
- Test: `tests/test_inpatient_domain.c`

- [ ] **Step 1: 为床位状态与住院状态流转写失败测试**
- [ ] **Step 2: 实现病区、床位、入院、住院记录结构体与状态函数**
- [ ] **Step 3: 实现病房、床位、入院、住院记录的 txt 读写仓储**
- [ ] **Step 4: 增加“占用床位不可再次分配、未出院不可重复入院”校验**
- [ ] **Step 5: 运行 `ctest --test-dir build --output-on-failure`**
- [ ] **Step 6: 提交**

### Task 12: 入院与床位服务

**Files:**
- Create: `include/service/InpatientService.h`
- Create: `src/service/InpatientService.c`
- Create: `include/service/BedService.h`
- Create: `src/service/BedService.c`
- Modify: `include/repository/BedRepository.h`
- Modify: `include/repository/AdmissionRepository.h`
- Test: `tests/test_inpatient_service.c`
- Test: `tests/test_bed_service.c`

- [ ] **Step 1: 写“空闲床位才能分配、同一患者不可重复入院”的失败测试**
- [ ] **Step 2: 实现查看病房信息、查看床位状态、分配床位、住院状态查询**
- [ ] **Step 3: 实现出院释放床位与转床逻辑**
- [ ] **Step 4: 在床位信息中维护当前入住患者**
- [ ] **Step 5: 运行 `ctest --test-dir build --output-on-failure`**
- [ ] **Step 6: 提交**

## Chunk 5: UI、报表与日志

### Task 13: 控制台菜单与角色视图

**Files:**
- Create: `include/ui/MenuController.h`
- Create: `src/ui/MenuController.c`
- Modify: `src/main.c`
- Test: `tests/test_menu_controller.c`

- [ ] **Step 1: 写菜单路由与非法输入回退测试**
- [ ] **Step 2: 为管理员、挂号员、医生、患者、住院登记员、病区管理员、药房人员提供独立菜单**
- [ ] **Step 3: 每次操作后统一输出编号、状态和下一步提示**
- [ ] **Step 4: 为患者菜单增加挂号查询、个人看诊/检查/住院/发药历史与药品使用方法入口，并为住院菜单增加病区床位查询、入院登记、出院办理入口**
- [ ] **Step 5: 运行 `ctest --test-dir build --output-on-failure`**
- [ ] **Step 6: 提交**

### Task 14: 报表与日志服务

**Files:**
- Create: `include/repository/LogRepository.h`
- Create: `src/repository/LogRepository.c`
- Create: `include/service/ReportService.h`
- Create: `src/service/ReportService.c`
- Test: `tests/test_report_service.c`

- [ ] **Step 1: 写日报、接诊统计、库存预警、床位占用率测试**
- [ ] **Step 2: 写关键操作日志记录测试**
- [ ] **Step 3: 实现按日期/科室/医生/病区的统计查询**
- [ ] **Step 4: 补齐医护视角、管理视角、患者视角三类报表入口，并支持个人看诊、检查、住院、发药历史查询**
- [ ] **Step 5: 运行 `ctest --test-dir build --output-on-failure`**
- [ ] **Step 6: 提交**

## Chunk 6: 原始数据、回归验证与答辩准备

### Task 15: 条件性数据分析与预测功能

**Files:**
- Create: `include/service/AnalyticsService.h`
- Create: `src/service/AnalyticsService.c`
- Test: `tests/test_analytics_service.c`

- [ ] **Step 1: 判断当前团队是否需要实现增强版数据分析功能**
- [ ] **Step 2: 若需要，实现历史住院数据分析与床位利用率统计**
- [ ] **Step 3: 若需要，实现科室病房分配优化建议或简单趋势预测**
- [ ] **Step 4: 若不需要，在文档中明确该任务为条件性高级要求**
- [ ] **Step 5: 运行 `ctest --test-dir build --output-on-failure` 或记录跳过原因**
- [ ] **Step 6: 提交**

### Task 16: 初始化现场评测数据

**Files:**
- Create: `data/users.txt`
- Create: `data/departments.txt`
- Create: `data/doctors.txt`
- Create: `data/patients.txt`
- Create: `data/registrations.txt`
- Create: `data/visit_records.txt`
- Create: `data/exam_records.txt`
- Create: `data/prescriptions.txt`
- Create: `data/dispense_records.txt`
- Create: `data/medicines.txt`
- Create: `data/wards.txt`
- Create: `data/beds.txt`
- Create: `data/admissions.txt`
- Create: `data/inpatient_orders.txt`
- Create: `data/logs.txt`

- [ ] **Step 1: 为每个文件写表头**
- [ ] **Step 2: 准备至少 100 名门诊患者、30 名住院患者、20 名医生、5 个科室、3 类病房、20 类药品**
- [ ] **Step 3: 保证每个 txt 文件至少 30 条记录，并考虑重名、长字段、别名等情况**
- [ ] **Step 4: 准备空白数据集、正常数据集、异常污染数据集三套样例**
- [ ] **Step 5: 增加住院床位冲突、重复出院等异常样例**
- [ ] **Step 6: 手动运行系统验证所有数据集都能被识别**
- [ ] **Step 7: 提交**

### Task 17: 最小集成验证

**Files:**
- Modify: `tests/`
- Modify: `docs/`

- [ ] **Step 1: 编写集成测试脚本或操作清单**

Run:
```powershell
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Expected:
```text
Build succeeds
All tests pass
```

- [ ] **Step 2: 走一遍演示链路**

演示顺序：
1. 登录管理员查看基础数据
2. 管理员维护科室并新增医生，按科室查看医生
3. 挂号员新增患者并挂号
4. 医生接诊、写看诊记录并补充检查记录
5. 药房发药并展示库存变化与库存预警
6. 患者查看诊断建议、检查结果和发药记录
7. 住院登记员办理入院并分配床位
8. 维护住院记录并办理出院释放床位
9. 查询患者历史医疗记录和时间范围记录
10. 管理员查看日报、床位统计与日志

- [ ] **Step 3: 准备代码检查版源码，补齐必要代码注释并确认链表实现可展示**
- [ ] **Step 4: 固化总结报告，至少包含测试方案、成员分工、完成情况**
- [ ] **Step 5: 固化答辩材料**
- [ ] **Step 6: 提交**

## 里程碑建议

| 周次 | 目标 | 输出 |
| --- | --- | --- |
| 第 1 周 | 明确需求、搭骨架、建数据模型 | 工程初始化、设计文档 |
| 第 2 周 | 完成患者、挂号、接诊 | 门诊前半流程 |
| 第 3 周 | 完成发药、库存与处方联动 | 门诊闭环 MVP |
| 第 4 周 | 完成基础住院：入院、床位、出院 | 门诊 + 住院闭环 |
| 第 5 周 | 完成统计、日志、数据样例、回归测试 | 答辩版系统 |
| 第 6 周 | 整理总结报告、代码检查材料、条件性分析功能 | 最终提交版 |

## 完成判定

满足以下条件即可视为课程设计版本完成：

- 可以从登录开始走通一次完整门诊流程；
- 可以走通一次完整基础住院流程；
- 关键异常输入不会导致程序崩溃；
- txt 原始数据文件完整且可用于现场演示；
- 容量数据满足课程最小规模要求；
- 全部源码与测试代码保持为 `C`；
- 源码具备必要注释，并能够展示链表实现；
- 已准备总结报告，包含测试方案和成员分工；
- 至少具备单元测试 + 集成演示清单两类验证证据；
- 文档、源码、数据文件三者目录清晰，可独立提交和展示。
