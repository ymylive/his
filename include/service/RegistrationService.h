/**
 * @file RegistrationService.h
 * @brief 挂号服务模块头文件
 *
 * 提供门诊挂号的创建、取消和查询功能。该服务依赖挂号仓库、患者仓库、
 * 医生仓库和科室仓库，在创建挂号时会验证关联实体（患者、医生、科室）
 * 是否存在且状态有效。
 */

#ifndef HIS_SERVICE_REGISTRATION_SERVICE_H
#define HIS_SERVICE_REGISTRATION_SERVICE_H

#include "common/LinkedList.h"
#include "common/Result.h"
#include "domain/Registration.h"
#include "repository/DepartmentRepository.h"
#include "repository/DoctorRepository.h"
#include "repository/PatientRepository.h"
#include "repository/RegistrationRepository.h"

/**
 * @brief 挂号服务结构体
 *
 * 持有各关联仓库的指针，提供统一的挂号管理接口。
 * 注意：此结构体中的仓库字段为指针类型，需由调用方管理其生命周期。
 */
typedef struct RegistrationService {
    RegistrationRepository *registration_repository;  /* 挂号数据仓库指针 */
    PatientRepository *patient_repository;            /* 患者数据仓库指针 */
    DoctorRepository *doctor_repository;              /* 医生数据仓库指针 */
    DepartmentRepository *department_repository;      /* 科室数据仓库指针 */
} RegistrationService;

/**
 * @brief 初始化挂号服务
 *
 * 将外部创建的各仓库指针注入到挂号服务中，并确保存储已就绪。
 *
 * @param service                   指向待初始化的挂号服务结构体
 * @param registration_repository   挂号仓库指针
 * @param patient_repository        患者仓库指针
 * @param doctor_repository         医生仓库指针
 * @param department_repository     科室仓库指针
 * @return Result                   操作结果，success=1 表示成功
 */
Result RegistrationService_init(
    RegistrationService *service,
    RegistrationRepository *registration_repository,
    PatientRepository *patient_repository,
    DoctorRepository *doctor_repository,
    DepartmentRepository *department_repository
);

/**
 * @brief 创建新挂号记录
 *
 * 验证患者、医生和科室是否存在且状态有效，自动生成挂号ID，
 * 初始状态为待诊（PENDING）。
 *
 * @param service            指向挂号服务结构体
 * @param patient_id         患者ID
 * @param doctor_id          医生ID
 * @param department_id      科室ID
 * @param registered_at      挂号时间字符串
 * @param registration_type  挂号类型（普通号/专家号/急诊号）
 * @param registration_fee   挂号费(元)
 * @param out_registration   输出参数，创建成功时存放挂号信息
 * @return Result            操作结果，success=1 表示创建成功
 */
Result RegistrationService_create(
    RegistrationService *service,
    const char *patient_id,
    const char *doctor_id,
    const char *department_id,
    const char *registered_at,
    RegistrationType registration_type,
    double registration_fee,
    Registration *out_registration
);

/**
 * @brief 根据挂号ID查找挂号记录
 *
 * @param service           指向挂号服务结构体
 * @param registration_id   挂号ID
 * @param out_registration  输出参数，查找成功时存放挂号信息
 * @return Result           操作结果，success=1 表示找到记录
 */
Result RegistrationService_find_by_registration_id(
    RegistrationService *service,
    const char *registration_id,
    Registration *out_registration
);

/**
 * @brief 取消挂号
 *
 * 将指定挂号记录标记为已取消状态。
 *
 * @param service           指向挂号服务结构体
 * @param registration_id   待取消的挂号ID
 * @param cancelled_at      取消时间字符串
 * @param out_registration  输出参数，取消成功时存放更新后的挂号信息
 * @return Result           操作结果，success=1 表示取消成功
 */
Result RegistrationService_cancel(
    RegistrationService *service,
    const char *registration_id,
    const char *cancelled_at,
    Registration *out_registration
);

/**
 * @brief 根据患者ID查找挂号记录
 *
 * 先验证患者是否存在，然后按过滤条件查找该患者的挂号记录。
 *
 * @param service            指向挂号服务结构体
 * @param patient_id         患者ID
 * @param filter             过滤条件（可按状态和时间范围过滤，可为 NULL）
 * @param out_registrations  输出参数，查找结果链表
 * @return Result            操作结果，success=1 表示查找完成
 */
Result RegistrationService_find_by_patient_id(
    RegistrationService *service,
    const char *patient_id,
    const RegistrationRepositoryFilter *filter,
    LinkedList *out_registrations
);

/**
 * @brief 根据医生ID查找挂号记录
 *
 * 先验证医生是否存在，然后按过滤条件查找该医生的挂号记录。
 *
 * @param service            指向挂号服务结构体
 * @param doctor_id          医生ID
 * @param filter             过滤条件（可按状态和时间范围过滤，可为 NULL）
 * @param out_registrations  输出参数，查找结果链表
 * @return Result            操作结果，success=1 表示查找完成
 */
Result RegistrationService_find_by_doctor_id(
    RegistrationService *service,
    const char *doctor_id,
    const RegistrationRepositoryFilter *filter,
    LinkedList *out_registrations
);

#endif
