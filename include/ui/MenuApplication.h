/**
 * @file MenuApplication.h
 * @brief 菜单应用程序模块 - HIS 系统的核心业务协调层
 *
 * 本模块是医院信息系统(HIS)的应用层核心，负责：
 * 1. 整合所有业务服务（认证、患者、医生、挂号、病历、住院、药房等）
 * 2. 管理用户登录状态和患者会话
 * 3. 提供统一的业务操作接口（增删改查患者、挂号、诊疗、住院、发药等）
 * 4. 将菜单操作分派到具体的业务逻辑处理函数
 */

#ifndef HIS_UI_MENU_APPLICATION_H
#define HIS_UI_MENU_APPLICATION_H

#include <stddef.h>
#include <stdio.h>
#include <time.h>

#include "common/Result.h"

/**
 * @brief 登录会话空闲超时阈值（秒）
 *
 * 超过此时长未产生用户输入则自动登出，满足 HIPAA §164.312(a)(2)(iii)
 * 中对未监控工作站的自动注销要求。默认 15 分钟。
 */
#ifndef HIS_SESSION_IDLE_TIMEOUT_SECONDS
#define HIS_SESSION_IDLE_TIMEOUT_SECONDS 900
#endif
#include "domain/Department.h"
#include "domain/Doctor.h"
#include "domain/Medicine.h"
#include "domain/Patient.h"
#include "domain/User.h"
#include "service/AuditService.h"
#include "service/AuthService.h"
#include "service/BedService.h"
#include "service/DoctorService.h"
#include "service/InpatientService.h"
#include "service/MedicalRecordService.h"
#include "service/PatientService.h"
#include "service/InpatientOrderService.h"
#include "service/NursingRecordService.h"
#include "service/PharmacyService.h"
#include "service/RoundRecordService.h"
#include "service/PrescriptionService.h"
#include "service/RegistrationService.h"
#include "service/SequenceService.h"
#include "ui/MenuController.h"

/**
 * @brief 应用程序数据文件路径集合
 *
 * 包含系统运行所需的所有文本数据文件的路径，
 * 用于初始化各个业务服务的数据仓库。
 */
typedef struct MenuApplicationPaths {
    const char *user_path;            /**< 用户数据文件路径 */
    const char *patient_path;         /**< 患者数据文件路径 */
    const char *department_path;      /**< 科室数据文件路径 */
    const char *doctor_path;          /**< 医生数据文件路径 */
    const char *registration_path;    /**< 挂号数据文件路径 */
    const char *visit_path;           /**< 就诊记录数据文件路径 */
    const char *examination_path;     /**< 检查记录数据文件路径 */
    const char *ward_path;            /**< 病房数据文件路径 */
    const char *bed_path;             /**< 床位数据文件路径 */
    const char *admission_path;       /**< 住院记录数据文件路径 */
    const char *medicine_path;        /**< 药品数据文件路径 */
    const char *dispense_record_path; /**< 发药记录数据文件路径 */
    const char *prescription_path;    /**< 处方数据文件路径 */
    const char *inpatient_order_path; /**< 住院医嘱数据文件路径 */
    const char *nursing_record_path;  /**< 护理记录数据文件路径 */
    const char *round_record_path;    /**< 查房记录数据文件路径 */
    const char *data_dir;             /**< 数据目录路径（审计日志等写入此目录，可 NULL） */
    const char *sequences_path;       /**< 序列号状态文件路径（可 NULL，自动从 registration_path 推导） */
} MenuApplicationPaths;

/**
 * @brief 菜单应用程序主结构体
 *
 * 持有所有业务服务实例、当前登录用户信息和患者会话状态。
 * 是整个应用的运行时上下文。
 */
typedef struct MenuApplication {
    AuditService audit_service;                    /**< 审计日志服务 */
    AuthService auth_service;                      /**< 认证服务 */
    PatientService patient_service;                /**< 患者服务 */
    DoctorService doctor_service;                  /**< 医生服务（含科室） */
    RegistrationRepository registration_repository;/**< 挂号数据仓库 */
    RegistrationService registration_service;      /**< 挂号服务 */
    MedicalRecordService medical_record_service;   /**< 病历服务 */
    InpatientService inpatient_service;            /**< 住院服务 */
    BedService bed_service;                        /**< 床位服务 */
    PharmacyService pharmacy_service;              /**< 药房服务 */
    PrescriptionService prescription_service;      /**< 处方服务 */
    InpatientOrderService inpatient_order_service; /**< 住院医嘱服务 */
    NursingRecordService nursing_record_service;  /**< 护理记录服务 */
    RoundRecordService round_record_service;    /**< 查房记录服务 */
    SequenceService sequence_service;              /**< 持久化序列号服务（CRUD 快路径依赖） */
    User authenticated_user;                       /**< 当前已认证用户 */
    int has_authenticated_user;                    /**< 是否有已认证用户（0=无，1=有） */
    char bound_patient_id[HIS_DOMAIN_ID_CAPACITY]; /**< 绑定的患者编号（患者角色登录时自动绑定） */
    int has_bound_patient_session;                 /**< 是否已绑定患者会话（0=未绑定，1=已绑定） */
    time_t last_activity;                          /**< 最近一次有效用户操作的时间戳（用于空闲超时） */
} MenuApplication;

/**
 * @brief 就诊记录交接数据结构
 *
 * 在创建就诊记录后，将关键信息打包传递给后续流程
 * （如检查、住院、发药等），用于跨步骤的数据传递。
 */
typedef struct MenuApplicationVisitHandoff {
    char visit_id[HIS_DOMAIN_ID_CAPACITY];         /**< 就诊记录编号 */
    char registration_id[HIS_DOMAIN_ID_CAPACITY];  /**< 挂号编号 */
    char patient_id[HIS_DOMAIN_ID_CAPACITY];       /**< 患者编号 */
    char doctor_id[HIS_DOMAIN_ID_CAPACITY];        /**< 医生编号 */
    char department_id[HIS_DOMAIN_ID_CAPACITY];    /**< 科室编号 */
    char diagnosis[HIS_DOMAIN_TEXT_CAPACITY];       /**< 诊断结果 */
    char advice[HIS_DOMAIN_TEXT_CAPACITY];          /**< 医嘱/建议 */
    int need_exam;                                  /**< 是否需要检查（0=否，1=是） */
    int need_admission;                             /**< 是否需要住院（0=否，1=是） */
    int need_medicine;                              /**< 是否需要用药（0=否，1=是） */
    char visit_time[HIS_DOMAIN_TIME_CAPACITY];     /**< 就诊时间 */
} MenuApplicationVisitHandoff;

/**
 * @brief 初始化菜单应用程序
 *
 * 验证所有数据文件路径的有效性，初始化所有业务服务。
 *
 * @param application 应用程序实例指针
 * @param paths       数据文件路径集合
 * @return Result     成功或失败的结果
 */
Result MenuApplication_init(MenuApplication *application, const MenuApplicationPaths *paths);

/**
 * @brief 用户登录
 *
 * 验证用户身份并设置认证状态。若用户为患者角色，自动绑定患者会话。
 *
 * @param application   应用程序实例指针
 * @param user_id       用户编号
 * @param password      密码
 * @param required_role 要求的用户角色
 * @return Result       成功或失败的结果
 */
Result MenuApplication_login(
    MenuApplication *application,
    const char *user_id,
    const char *password,
    UserRole required_role
);

/**
 * @brief 用户登出
 *
 * 清除认证状态和患者会话绑定。
 *
 * @param application 应用程序实例指针
 */
void MenuApplication_logout(MenuApplication *application);

/**
 * @brief 刷新最后活动时间戳
 *
 * 每当接收到有效用户操作（如菜单选择、输入确认）时调用，
 * 用于重置空闲超时计时器。
 *
 * @param application 应用程序实例指针
 */
void MenuApplication_touch_activity(MenuApplication *application);

/**
 * @brief 判断当前会话是否已空闲超时
 *
 * 基于 HIS_SESSION_IDLE_TIMEOUT_SECONDS 与 last_activity 字段判定。
 * 若无已登录用户或 last_activity 为 0，则视为未超时。
 *
 * @param application 应用程序实例指针
 * @param now_seconds 调用方提供的当前 Unix 时间戳（秒）
 * @return 已超时返回 1，否则返回 0
 */
int MenuApplication_is_session_idle(const MenuApplication *application, time_t now_seconds);

/**
 * @brief 绑定患者会话
 *
 * 将指定患者编号与当前会话绑定，用于患者角色的数据隔离。
 *
 * @param application 应用程序实例指针
 * @param patient_id  要绑定的患者编号
 * @return Result     成功或失败的结果
 */
Result MenuApplication_bind_patient_session(
    MenuApplication *application,
    const char *patient_id
);

/**
 * @brief 重置患者会话
 *
 * 清除当前绑定的患者会话。
 *
 * @param application 应用程序实例指针
 */
void MenuApplication_reset_patient_session(MenuApplication *application);

/**
 * @brief 添加患者
 * @param application 应用程序实例指针
 * @param patient     患者数据（patient_id 为空时自动生成）
 * @param buffer      输出缓冲区，存放操作结果消息
 * @param capacity    缓冲区容量
 * @return Result     成功或失败的结果
 */
Result MenuApplication_add_patient(
    MenuApplication *application,
    const Patient *patient,
    char *buffer,
    size_t capacity
);

/**
 * @brief 更新患者信息
 * @param application 应用程序实例指针
 * @param patient     包含更新数据的患者结构体
 * @param buffer      输出缓冲区
 * @param capacity    缓冲区容量
 * @return Result     成功或失败的结果
 */
Result MenuApplication_update_patient(
    MenuApplication *application,
    const Patient *patient,
    char *buffer,
    size_t capacity
);

/**
 * @brief 查询患者信息
 * @param application 应用程序实例指针
 * @param patient_id  患者编号
 * @param buffer      输出缓冲区，存放患者信息摘要
 * @param capacity    缓冲区容量
 * @return Result     成功或失败的结果
 */
Result MenuApplication_query_patient(
    MenuApplication *application,
    const char *patient_id,
    char *buffer,
    size_t capacity
);

/**
 * @brief 删除患者
 * @param application 应用程序实例指针
 * @param patient_id  要删除的患者编号
 * @param buffer      输出缓冲区
 * @param capacity    缓冲区容量
 * @return Result     成功或失败的结果
 */
Result MenuApplication_delete_patient(
    MenuApplication *application,
    const char *patient_id,
    char *buffer,
    size_t capacity
);

/**
 * @brief 列出所有科室
 * @param application 应用程序实例指针
 * @param buffer      输出缓冲区，存放科室列表
 * @param capacity    缓冲区容量
 * @return Result     成功或失败的结果
 */
Result MenuApplication_list_departments(
    MenuApplication *application,
    char *buffer,
    size_t capacity
);

/**
 * @brief 添加科室
 * @param application 应用程序实例指针
 * @param department  科室数据（department_id 为空时自动生成）
 * @param buffer      输出缓冲区
 * @param capacity    缓冲区容量
 * @return Result     成功或失败的结果
 */
Result MenuApplication_add_department(
    MenuApplication *application,
    const Department *department,
    char *buffer,
    size_t capacity
);

/**
 * @brief 更新科室信息
 * @param application 应用程序实例指针
 * @param department  包含更新数据的科室结构体
 * @param buffer      输出缓冲区
 * @param capacity    缓冲区容量
 * @return Result     成功或失败的结果
 */
Result MenuApplication_update_department(
    MenuApplication *application,
    const Department *department,
    char *buffer,
    size_t capacity
);

/**
 * @brief 添加医生
 * @param application 应用程序实例指针
 * @param doctor      医生数据（doctor_id 为空时自动生成）
 * @param buffer      输出缓冲区
 * @param capacity    缓冲区容量
 * @return Result     成功或失败的结果
 */
Result MenuApplication_add_doctor(
    MenuApplication *application,
    const Doctor *doctor,
    char *buffer,
    size_t capacity
);

/**
 * @brief 查询医生信息
 * @param application 应用程序实例指针
 * @param doctor_id   医生工号
 * @param buffer      输出缓冲区
 * @param capacity    缓冲区容量
 * @return Result     成功或失败的结果
 */
Result MenuApplication_query_doctor(
    MenuApplication *application,
    const char *doctor_id,
    char *buffer,
    size_t capacity
);

/**
 * @brief 按科室列出医生
 * @param application   应用程序实例指针
 * @param department_id 科室编号
 * @param buffer        输出缓冲区
 * @param capacity      缓冲区容量
 * @return Result       成功或失败的结果
 */
Result MenuApplication_list_doctors_by_department(
    MenuApplication *application,
    const char *department_id,
    char *buffer,
    size_t capacity
);

/**
 * @brief 创建挂号记录（管理员操作）
 * @param application       应用程序实例指针
 * @param patient_id        患者编号
 * @param doctor_id         医生工号
 * @param department_id     科室编号
 * @param registered_at     挂号时间
 * @param registration_type 挂号类型（普通号/专家号/急诊号）
 * @param registration_fee  挂号费(元)
 * @param buffer            输出缓冲区
 * @param capacity          缓冲区容量
 * @return Result           成功或失败的结果
 */
Result MenuApplication_create_registration(
    MenuApplication *application,
    const char *patient_id,
    const char *doctor_id,
    const char *department_id,
    const char *registered_at,
    RegistrationType registration_type,
    double registration_fee,
    char *buffer,
    size_t capacity
);

/**
 * @brief 患者自助挂号
 *
 * 使用当前绑定的患者会话自动填充患者编号。
 *
 * @param application       应用程序实例指针
 * @param doctor_id         医生工号
 * @param department_id     科室编号
 * @param registered_at     挂号时间
 * @param registration_type 挂号类型（普通号/专家号/急诊号）
 * @param registration_fee  挂号费(元)
 * @param out_registration  输出参数，创建成功后的挂号记录
 * @return Result           成功或失败的结果
 */
Result MenuApplication_create_self_registration(
    MenuApplication *application,
    const char *doctor_id,
    const char *department_id,
    const char *registered_at,
    RegistrationType registration_type,
    double registration_fee,
    Registration *out_registration
);

/**
 * @brief 查询挂号记录
 * @param application     应用程序实例指针
 * @param registration_id 挂号编号
 * @param buffer          输出缓冲区
 * @param capacity        缓冲区容量
 * @return Result         成功或失败的结果
 */
Result MenuApplication_query_registration(
    MenuApplication *application,
    const char *registration_id,
    char *buffer,
    size_t capacity
);

/**
 * @brief 取消挂号
 * @param application     应用程序实例指针
 * @param registration_id 挂号编号
 * @param cancelled_at    取消时间
 * @param buffer          输出缓冲区
 * @param capacity        缓冲区容量
 * @return Result         成功或失败的结果
 */
Result MenuApplication_cancel_registration(
    MenuApplication *application,
    const char *registration_id,
    const char *cancelled_at,
    char *buffer,
    size_t capacity
);

/**
 * @brief 查询指定患者的所有挂号记录
 * @param application 应用程序实例指针
 * @param patient_id  患者编号
 * @param buffer      输出缓冲区
 * @param capacity    缓冲区容量
 * @return Result     成功或失败的结果
 */
Result MenuApplication_query_registrations_by_patient(
    MenuApplication *application,
    const char *patient_id,
    char *buffer,
    size_t capacity
);

/**
 * @brief 按时间范围查询医疗记录
 * @param application 应用程序实例指针
 * @param time_from   起始时间
 * @param time_to     结束时间
 * @param buffer      输出缓冲区
 * @param capacity    缓冲区容量
 * @return Result     成功或失败的结果
 */
Result MenuApplication_query_records_by_time_range(
    MenuApplication *application,
    const char *time_from,
    const char *time_to,
    char *buffer,
    size_t capacity
);

/**
 * @brief 查询指定医生的待诊挂号列表
 * @param application 应用程序实例指针
 * @param doctor_id   医生工号
 * @param buffer      输出缓冲区
 * @param capacity    缓冲区容量
 * @return Result     成功或失败的结果
 */
Result MenuApplication_query_pending_registrations_by_doctor(
    MenuApplication *application,
    const char *doctor_id,
    char *buffer,
    size_t capacity
);

/**
 * @brief 创建就诊记录（结果写入缓冲区）
 * @param application     应用程序实例指针
 * @param registration_id 挂号编号
 * @param chief_complaint 主诉
 * @param diagnosis       诊断结果
 * @param advice          医嘱/建议
 * @param need_exam       是否需要检查
 * @param need_admission  是否需要住院
 * @param need_medicine   是否需要用药
 * @param visit_time      就诊时间
 * @param buffer          输出缓冲区
 * @param capacity        缓冲区容量
 * @return Result         成功或失败的结果
 */
Result MenuApplication_create_visit_record(
    MenuApplication *application,
    const char *registration_id,
    const char *chief_complaint,
    const char *diagnosis,
    const char *advice,
    int need_exam,
    int need_admission,
    int need_medicine,
    const char *visit_time,
    char *buffer,
    size_t capacity
);

/**
 * @brief 创建就诊记录并返回交接数据
 *
 * 与 create_visit_record 类似，但返回结构化的交接数据以供后续流程使用。
 *
 * @param application     应用程序实例指针
 * @param registration_id 挂号编号
 * @param chief_complaint 主诉
 * @param diagnosis       诊断结果
 * @param advice          医嘱/建议
 * @param need_exam       是否需要检查
 * @param need_admission  是否需要住院
 * @param need_medicine   是否需要用药
 * @param visit_time      就诊时间
 * @param out_handoff     输出参数，就诊交接数据
 * @return Result         成功或失败的结果
 */
Result MenuApplication_create_visit_record_handoff(
    MenuApplication *application,
    const char *registration_id,
    const char *chief_complaint,
    const char *diagnosis,
    const char *advice,
    int need_exam,
    int need_admission,
    int need_medicine,
    const char *visit_time,
    MenuApplicationVisitHandoff *out_handoff
);

/**
 * @brief 查询患者的完整就医历史
 * @param application 应用程序实例指针
 * @param patient_id  患者编号
 * @param buffer      输出缓冲区
 * @param capacity    缓冲区容量
 * @return Result     成功或失败的结果
 */
Result MenuApplication_query_patient_history(
    MenuApplication *application,
    const char *patient_id,
    char *buffer,
    size_t capacity
);

/**
 * @brief 创建检查记录
 * @param application  应用程序实例指针
 * @param visit_id     就诊记录编号
 * @param exam_item    检查项目名称
 * @param exam_type    检查类型
 * @param exam_fee     检查费用(元)
 * @param requested_at 申请时间
 * @param buffer       输出缓冲区
 * @param capacity     缓冲区容量
 * @return Result      成功或失败的结果
 */
Result MenuApplication_create_examination_record(
    MenuApplication *application,
    const char *visit_id,
    const char *exam_item,
    const char *exam_type,
    double exam_fee,
    const char *requested_at,
    char *buffer,
    size_t capacity
);

/**
 * @brief 完成检查记录（回写检查结果）
 * @param application    应用程序实例指针
 * @param examination_id 检查记录编号
 * @param result_text    检查结果文本
 * @param completed_at   完成时间
 * @param buffer         输出缓冲区
 * @param capacity       缓冲区容量
 * @return Result        成功或失败的结果
 */
Result MenuApplication_complete_examination_record(
    MenuApplication *application,
    const char *examination_id,
    const char *result_text,
    const char *completed_at,
    char *buffer,
    size_t capacity
);

/**
 * @brief 列出所有病房
 * @param application 应用程序实例指针
 * @param buffer      输出缓冲区
 * @param capacity    缓冲区容量
 * @return Result     成功或失败的结果
 */
Result MenuApplication_list_wards(
    MenuApplication *application,
    char *buffer,
    size_t capacity
);

/**
 * @brief 列出指定病房的所有床位
 * @param application 应用程序实例指针
 * @param ward_id     病房编号
 * @param buffer      输出缓冲区
 * @param capacity    缓冲区容量
 * @return Result     成功或失败的结果
 */
Result MenuApplication_list_beds_by_ward(
    MenuApplication *application,
    const char *ward_id,
    char *buffer,
    size_t capacity
);

/**
 * @brief 办理患者入院
 * @param application 应用程序实例指针
 * @param patient_id  患者编号
 * @param ward_id     病房编号
 * @param bed_id      床位编号
 * @param admitted_at 入院时间
 * @param summary     住院摘要
 * @param buffer      输出缓冲区
 * @param capacity    缓冲区容量
 * @return Result     成功或失败的结果
 */
Result MenuApplication_admit_patient(
    MenuApplication *application,
    const char *patient_id,
    const char *ward_id,
    const char *bed_id,
    const char *admitted_at,
    const char *summary,
    char *buffer,
    size_t capacity
);

/**
 * @brief 办理患者出院
 * @param application   应用程序实例指针
 * @param admission_id  住院记录编号
 * @param discharged_at 出院时间
 * @param summary       出院小结
 * @param buffer        输出缓冲区
 * @param capacity      缓冲区容量
 * @return Result       成功或失败的结果
 */
Result MenuApplication_discharge_patient(
    MenuApplication *application,
    const char *admission_id,
    const char *discharged_at,
    const char *summary,
    char *buffer,
    size_t capacity
);

/**
 * @brief 查询患者当前的住院记录
 * @param application 应用程序实例指针
 * @param patient_id  患者编号
 * @param buffer      输出缓冲区
 * @param capacity    缓冲区容量
 * @return Result     成功或失败的结果
 */
Result MenuApplication_query_active_admission_by_patient(
    MenuApplication *application,
    const char *patient_id,
    char *buffer,
    size_t capacity
);

/**
 * @brief 查询指定床位当前入住的患者
 * @param application 应用程序实例指针
 * @param bed_id      床位编号
 * @param buffer      输出缓冲区
 * @param capacity    缓冲区容量
 * @return Result     成功或失败的结果
 */
Result MenuApplication_query_current_patient_by_bed(
    MenuApplication *application,
    const char *bed_id,
    char *buffer,
    size_t capacity
);

/**
 * @brief 转床操作
 * @param application    应用程序实例指针
 * @param admission_id   住院记录编号
 * @param target_bed_id  目标床位编号
 * @param transferred_at 转床时间
 * @param buffer         输出缓冲区
 * @param capacity       缓冲区容量
 * @return Result        成功或失败的结果
 */
Result MenuApplication_transfer_bed(
    MenuApplication *application,
    const char *admission_id,
    const char *target_bed_id,
    const char *transferred_at,
    char *buffer,
    size_t capacity
);

/**
 * @brief 出院前检查
 *
 * 验证住院记录状态是否允许出院。
 *
 * @param application  应用程序实例指针
 * @param admission_id 住院记录编号
 * @param buffer       输出缓冲区
 * @param capacity     缓冲区容量
 * @return Result      成功或失败的结果
 */
Result MenuApplication_discharge_check(
    MenuApplication *application,
    const char *admission_id,
    char *buffer,
    size_t capacity
);

/**
 * @brief 添加药品
 * @param application 应用程序实例指针
 * @param medicine    药品数据（medicine_id 为空时自动生成）
 * @param buffer      输出缓冲区
 * @param capacity    缓冲区容量
 * @return Result     成功或失败的结果
 */
Result MenuApplication_add_medicine(
    MenuApplication *application,
    const Medicine *medicine,
    char *buffer,
    size_t capacity
);

/**
 * @brief 药品入库（补充库存）
 * @param application 应用程序实例指针
 * @param medicine_id 药品编号
 * @param quantity    入库数量
 * @param buffer      输出缓冲区
 * @param capacity    缓冲区容量
 * @return Result     成功或失败的结果
 */
Result MenuApplication_restock_medicine(
    MenuApplication *application,
    const char *medicine_id,
    int quantity,
    char *buffer,
    size_t capacity
);

/**
 * @brief 发药操作
 * @param application   应用程序实例指针
 * @param patient_id    患者编号
 * @param prescription_id 处方编号（就诊记录编号）
 * @param medicine_id   药品编号
 * @param quantity      发药数量
 * @param pharmacist_id 药师工号
 * @param dispensed_at  发药时间
 * @param buffer        输出缓冲区
 * @param capacity      缓冲区容量
 * @return Result       成功或失败的结果
 */
Result MenuApplication_dispense_medicine(
    MenuApplication *application,
    const char *patient_id,
    const char *prescription_id,
    const char *medicine_id,
    int quantity,
    const char *pharmacist_id,
    const char *dispensed_at,
    char *buffer,
    size_t capacity
);

/**
 * @brief 查询药品库存
 * @param application 应用程序实例指针
 * @param medicine_id 药品编号
 * @param buffer      输出缓冲区
 * @param capacity    缓冲区容量
 * @return Result     成功或失败的结果
 */
Result MenuApplication_query_medicine_stock(
    MenuApplication *application,
    const char *medicine_id,
    char *buffer,
    size_t capacity
);

/**
 * @brief 查询药品详细信息
 * @param application            应用程序实例指针
 * @param medicine_id            药品编号
 * @param buffer                 输出缓冲区
 * @param capacity               缓冲区容量
 * @param include_instruction_note 是否包含用药说明（1=包含，0=不包含）
 * @return Result                成功或失败的结果
 */
Result MenuApplication_query_medicine_detail(
    MenuApplication *application,
    const char *medicine_id,
    char *buffer,
    size_t capacity,
    int include_instruction_note
);

/**
 * @brief 查找库存不足的药品列表
 * @param application 应用程序实例指针
 * @param buffer      输出缓冲区
 * @param capacity    缓冲区容量
 * @return Result     成功或失败的结果
 */
Result MenuApplication_find_low_stock_medicines(
    MenuApplication *application,
    char *buffer,
    size_t capacity
);

/**
 * @brief 创建处方
 *
 * 校验就诊记录和药品是否存在，检查药品库存后创建处方。
 *
 * @param application   应用程序实例指针
 * @param prescription  处方数据
 * @param buffer        输出缓冲区
 * @param capacity      缓冲区容量
 * @return Result       成功或失败的结果
 */
Result MenuApplication_create_prescription(
    MenuApplication *application,
    const Prescription *prescription,
    char *buffer,
    size_t capacity
);

/**
 * @brief 按就诊ID查询处方列表
 * @param application 应用程序实例指针
 * @param visit_id    就诊记录编号
 * @param buffer      输出缓冲区
 * @param capacity    缓冲区容量
 * @return Result     成功或失败的结果
 */
Result MenuApplication_query_prescriptions_by_visit(
    MenuApplication *application,
    const char *visit_id,
    char *buffer,
    size_t capacity
);

/**
 * @brief 科室收入统计
 * @param application 应用程序实例指针
 * @param buffer      输出缓冲区
 * @param capacity    缓冲区容量
 * @return Result     成功或失败的结果
 */
Result MenuApplication_stats_department_revenue(
    MenuApplication *application,
    char *buffer,
    size_t capacity
);

/**
 * @brief 医生工作量统计
 * @param application 应用程序实例指针
 * @param buffer      输出缓冲区
 * @param capacity    缓冲区容量
 * @return Result     成功或失败的结果
 */
Result MenuApplication_stats_doctor_workload(
    MenuApplication *application,
    char *buffer,
    size_t capacity
);

/**
 * @brief 床位利用率统计
 * @param application 应用程序实例指针
 * @param buffer      输出缓冲区
 * @param capacity    缓冲区容量
 * @return Result     成功或失败的结果
 */
Result MenuApplication_stats_bed_utilization(
    MenuApplication *application,
    char *buffer,
    size_t capacity
);

/**
 * @brief 药品消耗排行
 * @param application 应用程序实例指针
 * @param buffer      输出缓冲区
 * @param capacity    缓冲区容量
 * @return Result     成功或失败的结果
 */
Result MenuApplication_stats_medicine_consumption(
    MenuApplication *application,
    char *buffer,
    size_t capacity
);

/**
 * @brief 患者流量统计
 * @param application 应用程序实例指针
 * @param buffer      输出缓冲区
 * @param capacity    缓冲区容量
 * @return Result     成功或失败的结果
 */
Result MenuApplication_stats_patient_flow(
    MenuApplication *application,
    char *buffer,
    size_t capacity
);

/**
 * @brief 科室综合绩效
 * @param application 应用程序实例指针
 * @param buffer      输出缓冲区
 * @param capacity    缓冲区容量
 * @return Result     成功或失败的结果
 */
Result MenuApplication_stats_dept_performance(
    MenuApplication *application,
    char *buffer,
    size_t capacity
);

/**
 * @brief 住院周转率
 * @param application 应用程序实例指针
 * @param buffer      输出缓冲区
 * @param capacity    缓冲区容量
 * @return Result     成功或失败的结果
 */
Result MenuApplication_stats_admission_turnover(
    MenuApplication *application,
    char *buffer,
    size_t capacity
);

/**
 * @brief 患者费用查询
 * @param application 应用程序实例指针
 * @param patient_id  患者编号
 * @param buffer      输出缓冲区
 * @param capacity    缓冲区容量
 * @return Result     成功或失败的结果
 */
Result MenuApplication_query_patient_fees(
    MenuApplication *application,
    const char *patient_id,
    char *buffer,
    size_t capacity
);

/**
 * @brief 出院结算单
 *
 * 根据住院记录编号生成出院结算单，汇总住院期间的全部费用。
 *
 * @param application  应用程序实例指针
 * @param admission_id 住院记录编号
 * @param buffer       输出缓冲区
 * @param capacity     缓冲区容量
 * @return Result      成功或失败的结果
 */
Result MenuApplication_discharge_settlement(
    MenuApplication *application,
    const char *admission_id,
    char *buffer,
    size_t capacity
);

/**
 * @brief 执行菜单操作
 *
 * 根据传入的菜单操作枚举，分派到具体的业务处理流程。
 * 包括交互式输入采集、业务逻辑执行和结果输出。
 *
 * @param application 应用程序实例指针
 * @param action      要执行的菜单操作
 * @param input       输入流（通常为 stdin）
 * @param output      输出流（通常为 stdout）
 * @return Result     成功或失败的结果
 */
Result MenuApplication_execute_action(
    MenuApplication *application,
    MenuAction action,
    FILE *input,
    FILE *output
);

/**
 * @brief 创建住院医嘱
 * @param application  应用程序实例指针
 * @param admission_id 入院记录编号
 * @param order_type   医嘱类型（用药/检查/护理/饮食/输液）
 * @param content      医嘱内容
 * @param ordered_at   开具时间
 * @param buffer       输出缓冲区
 * @param capacity     缓冲区容量
 * @return Result      成功或失败的结果
 */
Result MenuApplication_create_inpatient_order(
    MenuApplication *application,
    const char *admission_id,
    const char *order_type,
    const char *content,
    const char *ordered_at,
    char *buffer,
    size_t capacity
);

/**
 * @brief 查询指定入院记录的所有医嘱
 * @param application  应用程序实例指针
 * @param admission_id 入院记录编号
 * @param buffer       输出缓冲区
 * @param capacity     缓冲区容量
 * @return Result      成功或失败的结果
 */
Result MenuApplication_query_orders_by_admission(
    MenuApplication *application,
    const char *admission_id,
    char *buffer,
    size_t capacity
);

/**
 * @brief 执行住院医嘱
 * @param application 应用程序实例指针
 * @param order_id    医嘱编号
 * @param executed_at 执行时间
 * @param buffer      输出缓冲区
 * @param capacity    缓冲区容量
 * @return Result     成功或失败的结果
 */
Result MenuApplication_execute_inpatient_order(
    MenuApplication *application,
    const char *order_id,
    const char *executed_at,
    char *buffer,
    size_t capacity
);

/**
 * @brief 取消住院医嘱
 * @param application  应用程序实例指针
 * @param order_id     医嘱编号
 * @param cancelled_at 取消时间
 * @param buffer       输出缓冲区
 * @param capacity     缓冲区容量
 * @return Result      成功或失败的结果
 */
Result MenuApplication_cancel_inpatient_order(
    MenuApplication *application,
    const char *order_id,
    const char *cancelled_at,
    char *buffer,
    size_t capacity
);

/**
 * @brief 创建护理记录
 * @param application  应用程序实例指针
 * @param admission_id 住院记录编号
 * @param record_type  记录类型（体温/血压/用药/护理观察/输液）
 * @param nurse_name   护士姓名
 * @param content      记录内容
 * @param recorded_at  记录时间
 * @param buffer       输出缓冲区
 * @param capacity     缓冲区容量
 * @return Result      成功或失败的结果
 */
Result MenuApplication_create_nursing_record(
    MenuApplication *application,
    const char *admission_id,
    const char *record_type,
    const char *nurse_name,
    const char *content,
    const char *recorded_at,
    char *buffer,
    size_t capacity
);

/**
 * @brief 查询指定住院记录的所有护理记录
 * @param application  应用程序实例指针
 * @param admission_id 住院记录编号
 * @param buffer       输出缓冲区
 * @param capacity     缓冲区容量
 * @return Result      成功或失败的结果
 */
Result MenuApplication_query_nursing_records(
    MenuApplication *application,
    const char *admission_id,
    char *buffer,
    size_t capacity
);

/**
 * @brief 创建查房记录
 * @param application  应用程序实例指针
 * @param admission_id 住院记录编号
 * @param doctor_id    查房医生编号
 * @param findings     查房发现/病情观察
 * @param plan         治疗计划/调整方案
 * @param rounded_at   查房时间
 * @param buffer       输出缓冲区
 * @param capacity     缓冲区容量
 * @return Result      成功或失败的结果
 */
Result MenuApplication_create_round_record(
    MenuApplication *application,
    const char *admission_id,
    const char *doctor_id,
    const char *findings,
    const char *plan,
    const char *rounded_at,
    char *buffer,
    size_t capacity
);

/**
 * @brief 查询指定住院记录的所有查房记录
 * @param application  应用程序实例指针
 * @param admission_id 住院记录编号
 * @param buffer       输出缓冲区
 * @param capacity     缓冲区容量
 * @return Result      成功或失败的结果
 */
Result MenuApplication_query_round_records(
    MenuApplication *application,
    const char *admission_id,
    char *buffer,
    size_t capacity
);

#endif
