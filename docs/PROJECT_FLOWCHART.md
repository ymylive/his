# HIS 项目流程图

## 系统总流程

```mermaid
flowchart TD
    Start(["启动 his"])
    Init["控制台初始化<br/>data 目录权限加固"]
    Paths["配置数据文件路径<br/>users / patients / doctors / wards / medicines ..."]
    AppInit["MenuApplication_init<br/>装配业务服务与仓库"]
    Update["TUI 启动画面<br/>Logo / Banner / 检查更新"]
    MainMenu{"主菜单<br/>选择角色或系统操作"}

    Start --> Init --> Paths --> AppInit --> Update --> MainMenu

    MainMenu -->|退出| Exit(["退出系统"])
    MainMenu -->|重置演示数据| Reset["DemoData_reset<br/>从 data/demo_seed 恢复初始数据"]
    Reset --> Files
    Reset --> MainMenu

    MainMenu -->|患者| PatientGate{"患者入口"}
    PatientGate -->|注册新账号| PatientRegister["患者注册<br/>填写档案 / 设置账号密码"]
    PatientRegister --> PatientService
    PatientRegister --> AuthService
    PatientRegister --> MainMenu
    PatientGate -->|登录已有账号| Login

    MainMenu -->|管理员 / 医生 / 住院管理员 / 药房| Login["登录验证<br/>用户名 + 密码 + 角色匹配"]
    Login --> AuthService
    AuthService -->|成功| PasswordGate{"首次登录<br/>默认密码?"}
    AuthService -->|失败| Login
    PasswordGate -->|是| ChangePassword["强制修改密码<br/>记录审计日志"]
    ChangePassword --> RoleMenu
    PasswordGate -->|否| RoleMenu["角色菜单循环<br/>方向键/数字选择操作"]

    RoleMenu --> IdleGate{"空闲超时?"}
    IdleGate -->|是| Logout["自动登出<br/>返回主菜单"]
    IdleGate -->|否| Dispatch["MenuApplication_execute_action<br/>权限校验 + 操作分派"]
    Logout --> MainMenu
    Dispatch --> AdminOps
    Dispatch --> DoctorOps
    Dispatch --> PatientOps
    Dispatch --> InpatientOps
    Dispatch --> PharmacyOps
    Dispatch --> RoleMenu

    subgraph Roles["角色功能"]
        AdminOps["管理员<br/>患者管理<br/>医生与科室管理<br/>医疗记录查询<br/>病房床位总览<br/>药品库存总览<br/>收入/工作量/床位/药品/流量/绩效/周转统计"]
        DoctorOps["医生<br/>待诊列表<br/>患者历史<br/>叫号接诊<br/>已诊断列表<br/>处方与库存<br/>检查记录与结果<br/>查房记录"]
        PatientOps["患者<br/>基本信息<br/>自助挂号<br/>挂号/看诊/检查/住院/发药历史<br/>药品说明<br/>费用查询"]
        InpatientOps["住院管理员<br/>病区床位查询<br/>入院/出院<br/>住院状态<br/>转床<br/>出院检查<br/>医嘱创建/查询/执行/取消<br/>护理记录"]
        PharmacyOps["药房<br/>药品建档<br/>入库补货<br/>出库发药<br/>库存盘点<br/>低库存预警"]
    end

    subgraph Services["业务服务层"]
        AuthService["AuthService<br/>认证 / 注册 / 改密 / 锁定"]
        PatientService["PatientService<br/>患者档案 CRUD / 检索"]
        DoctorService["DoctorService + DepartmentService<br/>医生 / 科室"]
        RegistrationService["RegistrationService + SequenceService<br/>挂号 / 取消 / 序列号"]
        MedicalRecordService["MedicalRecordService<br/>就诊 / 检查 / 病史"]
        InpatientService["InpatientService + BedService<br/>住院 / 床位 / 出院"]
        PharmacyService["PharmacyService + PrescriptionService<br/>药品 / 处方 / 发药"]
        WardCareService["InpatientOrderService<br/>NursingRecordService<br/>RoundRecordService"]
        AuditService["AuditService<br/>登录 / 改密 / 敏感操作审计"]
    end

    AdminOps --> PatientService
    AdminOps --> DoctorService
    AdminOps --> MedicalRecordService
    AdminOps --> InpatientService
    AdminOps --> PharmacyService

    DoctorOps --> RegistrationService
    DoctorOps --> MedicalRecordService
    DoctorOps --> PharmacyService
    DoctorOps --> WardCareService

    PatientOps --> PatientService
    PatientOps --> RegistrationService
    PatientOps --> MedicalRecordService
    PatientOps --> InpatientService
    PatientOps --> PharmacyService

    InpatientOps --> InpatientService
    InpatientOps --> WardCareService

    PharmacyOps --> PharmacyService

    AuthService --> Files
    PatientService --> Files
    DoctorService --> Files
    RegistrationService --> Files
    MedicalRecordService --> Files
    InpatientService --> Files
    PharmacyService --> Files
    WardCareService --> Files
    AuditService --> Files

    Files[("文本文件数据层<br/>data/*.txt + audit.log")]

    classDef entry fill:#f7f7f5,stroke:#555,color:#222;
    classDef decision fill:#fff4d6,stroke:#b7791f,color:#222;
    classDef process fill:#e8f3ff,stroke:#2b6cb0,color:#102a43;
    classDef service fill:#eefaf1,stroke:#2f855a,color:#123524;
    classDef storage fill:#f3e8ff,stroke:#6b46c1,color:#2d174d;
    classDef role fill:#fff1f2,stroke:#be123c,color:#4c0519;

    class Start,Exit entry;
    class MainMenu,PatientGate,PasswordGate,IdleGate decision;
    class Init,Paths,AppInit,Update,Reset,PatientRegister,Login,ChangePassword,RoleMenu,Dispatch,Logout process;
    class AuthService,PatientService,DoctorService,RegistrationService,MedicalRecordService,InpatientService,PharmacyService,WardCareService,AuditService service;
    class Files storage;
    class AdminOps,DoctorOps,PatientOps,InpatientOps,PharmacyOps role;
```

## 核心业务闭环

```mermaid
flowchart LR
    Patient["患者"]
    Doctor["医生"]
    Inpatient["住院管理员"]
    Pharmacy["药房"]
    Admin["管理员"]

    Reg["自助挂号<br/>生成 registration"]
    Pending["待诊列表<br/>按医生查看挂号"]
    Visit["叫号接诊<br/>生成 visit"]
    Rx["开具处方<br/>生成 prescription"]
    Dispense["发药<br/>生成 dispense_record<br/>扣减库存"]
    DispenseQuery["患者查看发药历史"]

    ExamCreate["开立检查<br/>生成 examination"]
    ExamComplete["回写检查结果"]
    ExamQuery["患者/管理员查看检查摘要"]

    AdmissionAdvice["接诊建议住院"]
    Admit["办理入院<br/>生成 admission<br/>占用 bed"]
    WardWork["住院期间<br/>转床 / 查房 / 医嘱 / 护理"]
    DischargeCheck["出院前检查"]
    Settlement["费用结算"]
    Discharge["办理出院<br/>释放 bed"]
    AdmissionQuery["患者/管理员查看住院记录"]

    AdminMaintain["基础资料维护<br/>患者 / 医生 / 科室 / 病房 / 药品"]
    AdminStats["运营统计<br/>收入 / 工作量 / 床位利用率<br/>药品消耗 / 患者流量 / 绩效 / 周转率"]

    Patient --> Reg --> Pending --> Doctor --> Visit
    Visit --> Rx --> Pharmacy --> Dispense --> DispenseQuery --> Patient

    Visit --> ExamCreate --> ExamComplete --> ExamQuery --> Patient
    ExamQuery --> Admin

    Visit --> AdmissionAdvice --> Inpatient --> Admit --> WardWork --> DischargeCheck --> Settlement --> Discharge --> AdmissionQuery --> Patient
    AdmissionQuery --> Admin

    Admin --> AdminMaintain
    AdminMaintain --> Reg
    AdminMaintain --> Visit
    AdminMaintain --> Admit
    AdminMaintain --> Dispense
    Admin --> AdminStats
    Reg --> AdminStats
    Visit --> AdminStats
    Admit --> AdminStats
    Dispense --> AdminStats

    classDef actor fill:#f7f7f5,stroke:#444,color:#111;
    classDef outpatient fill:#e8f3ff,stroke:#2b6cb0,color:#102a43;
    classDef exam fill:#eefaf1,stroke:#2f855a,color:#123524;
    classDef inpatient fill:#fff4d6,stroke:#b7791f,color:#3d2500;
    classDef admin fill:#f3e8ff,stroke:#6b46c1,color:#2d174d;

    class Patient,Doctor,Inpatient,Pharmacy,Admin actor;
    class Reg,Pending,Visit,Rx,Dispense,DispenseQuery outpatient;
    class ExamCreate,ExamComplete,ExamQuery exam;
    class AdmissionAdvice,Admit,WardWork,DischargeCheck,Settlement,Discharge,AdmissionQuery inpatient;
    class AdminMaintain,AdminStats admin;
```

## 工程组成与交付

```mermaid
flowchart LR
    Headers["include/<br/>公共接口 / 领域模型 / 服务接口 / UI 接口"]
    Sources["src/<br/>common / domain / repository / service / ui / main"]
    CMake["CMakeLists.txt<br/>C11 / his_common / his"]
    App["his 可执行程序"]
    Tests["CTest 测试目标<br/>common / domain / repository / service / menu / demo data"]
    RuntimeData["data/*.txt<br/>运行期文本数据"]
    Seed["data/demo_seed/*.txt<br/>演示种子数据"]
    Docs["README + docs/<br/>安装 / 构建 / macOS / 安全 / AI 使用"]
    Scripts["install.sh / install.ps1<br/>scripts/update.sh / update.ps1"]
    Release["release / dist<br/>跨平台便携包"]
    Deck["deck / opening<br/>答辩演示材料"]

    Headers --> CMake
    Sources --> CMake
    CMake --> App
    CMake --> Tests
    Seed --> RuntimeData
    RuntimeData --> App
    Docs --> Scripts
    Scripts --> Release
    App --> Release
    Docs --> Deck
    App --> Deck

    classDef code fill:#e8f3ff,stroke:#2b6cb0,color:#102a43;
    classDef build fill:#eefaf1,stroke:#2f855a,color:#123524;
    classDef data fill:#f3e8ff,stroke:#6b46c1,color:#2d174d;
    classDef deliver fill:#fff4d6,stroke:#b7791f,color:#3d2500;

    class Headers,Sources code;
    class CMake,App,Tests build;
    class RuntimeData,Seed data;
    class Docs,Scripts,Release,Deck deliver;
```

## 数据文件覆盖

| 领域 | 文件 |
| --- | --- |
| 用户与患者 | `users.txt`, `patients.txt` |
| 医生与科室 | `doctors.txt`, `departments.txt` |
| 门诊 | `registrations.txt`, `visits.txt`, `examinations.txt`, `prescriptions.txt`, `dispense_records.txt` |
| 住院 | `wards.txt`, `beds.txt`, `admissions.txt`, `inpatient_orders.txt`, `nursing_records.txt`, `round_records.txt` |
| 药房 | `medicines.txt`, `prescriptions.txt`, `dispense_records.txt` |
| 系统 | `sequences.txt`, `audit.log`, `demo_seed/*.txt` |
