/**
 * @file InpatientService.h
 * @brief 住院服务模块头文件
 *
 * 提供住院管理的核心业务功能，包括患者入院、出院、转床以及住院记录查询。
 * 该服务需要协调患者仓库、病房仓库、床位仓库和住院记录仓库，
 * 确保在状态变更时同步更新所有关联数据。
 */

#ifndef HIS_SERVICE_INPATIENT_SERVICE_H
#define HIS_SERVICE_INPATIENT_SERVICE_H

#include "common/Result.h"
#include "domain/Admission.h"
#include "repository/AdmissionRepository.h"
#include "repository/BedRepository.h"
#include "repository/PatientRepository.h"
#include "repository/WardRepository.h"

/**
 * @brief 住院服务结构体
 *
 * 封装患者、病房、床位和住院记录四个仓库，
 * 提供统一的住院管理接口。
 */
typedef struct InpatientService {
    PatientRepository patient_repository;      /* 患者数据仓库 */
    WardRepository ward_repository;            /* 病房数据仓库 */
    BedRepository bed_repository;              /* 床位数据仓库 */
    AdmissionRepository admission_repository;  /* 住院记录仓库 */
} InpatientService;

/**
 * @brief 初始化住院服务
 *
 * 分别初始化患者、病房、床位和住院记录仓库。
 *
 * @param service         指向待初始化的住院服务结构体
 * @param patient_path    患者数据文件路径
 * @param ward_path       病房数据文件路径
 * @param bed_path        床位数据文件路径
 * @param admission_path  住院记录数据文件路径
 * @return Result         操作结果，success=1 表示成功
 */
Result InpatientService_init(
    InpatientService *service,
    const char *patient_path,
    const char *ward_path,
    const char *bed_path,
    const char *admission_path
);

/**
 * @brief 患者入院
 *
 * 验证患者、病房、床位的状态和可用性，生成住院记录ID，
 * 创建住院记录并同步更新患者住院状态、病房占用数和床位状态。
 *
 * @param service        指向住院服务结构体
 * @param patient_id     患者ID
 * @param ward_id        病房ID
 * @param bed_id         床位ID
 * @param admitted_at    入院时间字符串
 * @param summary        入院摘要
 * @param out_admission  输出参数，入院成功时存放住院记录信息
 * @return Result        操作结果，success=1 表示入院成功
 */
Result InpatientService_admit_patient(
    InpatientService *service,
    const char *patient_id,
    const char *ward_id,
    const char *bed_id,
    const char *admitted_at,
    const char *summary,
    Admission *out_admission
);

/**
 * @brief 患者出院
 *
 * 查找活跃的住院记录并标记为已出院，同时释放床位和病房资源，
 * 更新患者住院状态。
 *
 * @param service        指向住院服务结构体
 * @param admission_id   住院记录ID
 * @param discharged_at  出院时间字符串
 * @param summary        出院摘要
 * @param out_admission  输出参数，出院成功时存放更新后的住院记录
 * @return Result        操作结果，success=1 表示出院成功
 */
Result InpatientService_discharge_patient(
    InpatientService *service,
    const char *admission_id,
    const char *discharged_at,
    const char *summary,
    Admission *out_admission
);

/**
 * @brief 转床
 *
 * 将住院患者从当前床位转移到目标床位，必要时同步更新病房占用数。
 * 当前床位和目标床位可以在不同病房。
 *
 * @param service          指向住院服务结构体
 * @param admission_id     住院记录ID
 * @param target_bed_id    目标床位ID
 * @param transferred_at   转床时间字符串
 * @param out_admission    输出参数，转床成功时存放更新后的住院记录
 * @return Result          操作结果，success=1 表示转床成功
 */
Result InpatientService_transfer_bed(
    InpatientService *service,
    const char *admission_id,
    const char *target_bed_id,
    const char *transferred_at,
    Admission *out_admission
);

/**
 * @brief 根据住院记录ID查找住院记录
 *
 * @param service        指向住院服务结构体
 * @param admission_id   住院记录ID
 * @param out_admission  输出参数，查找成功时存放住院记录信息
 * @return Result        操作结果，success=1 表示找到记录
 */
Result InpatientService_find_by_id(
    InpatientService *service,
    const char *admission_id,
    Admission *out_admission
);

/**
 * @brief 根据患者ID查找当前活跃的住院记录
 *
 * @param service        指向住院服务结构体
 * @param patient_id     患者ID
 * @param out_admission  输出参数，查找成功时存放住院记录信息
 * @return Result        操作结果，success=1 表示找到活跃住院记录
 */
Result InpatientService_find_active_by_patient_id(
    InpatientService *service,
    const char *patient_id,
    Admission *out_admission
);

#endif
