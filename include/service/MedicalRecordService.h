/**
 * @file MedicalRecordService.h
 * @brief 病历服务模块头文件
 *
 * 提供病历管理的核心业务功能，包括就诊记录（VisitRecord）和检查记录
 * （ExaminationRecord）的增删改，以及患者病历历史和按时间范围查询。
 * 该服务协调挂号仓库、就诊记录仓库、检查记录仓库和住院记录仓库。
 */

#ifndef HIS_SERVICE_MEDICAL_RECORD_SERVICE_H
#define HIS_SERVICE_MEDICAL_RECORD_SERVICE_H

#include "common/LinkedList.h"
#include "common/Result.h"
#include "domain/Admission.h"
#include "domain/ExaminationRecord.h"
#include "domain/VisitRecord.h"
#include "repository/AdmissionRepository.h"
#include "repository/ExaminationRecordRepository.h"
#include "repository/RegistrationRepository.h"
#include "repository/VisitRecordRepository.h"

/**
 * @brief 就诊记录参数结构体
 *
 * 用于创建和更新就诊记录时传递参数，避免函数参数过多。
 */
typedef struct VisitRecordParams {
    const char *registration_id;   /* 关联的挂号ID（创建时使用） */
    const char *chief_complaint;   /* 主诉（可为空） */
    const char *diagnosis;         /* 诊断（可为空） */
    const char *advice;            /* 医嘱建议（可为空） */
    int need_exam;                 /* 是否需要检查（0或1） */
    int need_admission;            /* 是否需要住院（0或1） */
    int need_medicine;             /* 是否需要开药（0或1） */
    const char *visit_time;        /* 就诊时间字符串 */
} VisitRecordParams;

/**
 * @brief 病历历史记录结构体
 *
 * 聚合了一个患者或某个时间范围内的所有医疗记录，
 * 包括挂号记录、就诊记录、检查记录和住院记录。
 */
typedef struct MedicalRecordHistory {
    LinkedList registrations;  /* 挂号记录链表 */
    LinkedList visits;         /* 就诊记录链表 */
    LinkedList examinations;   /* 检查记录链表 */
    LinkedList admissions;     /* 住院记录链表 */
} MedicalRecordHistory;

/**
 * @brief 病历服务结构体
 *
 * 封装挂号、就诊、检查、住院四个仓库，提供统一的病历管理接口。
 */
typedef struct MedicalRecordService {
    RegistrationRepository registration_repository;        /* 挂号数据仓库 */
    VisitRecordRepository visit_repository;                /* 就诊记录仓库 */
    ExaminationRecordRepository examination_repository;    /* 检查记录仓库 */
    AdmissionRepository admission_repository;              /* 住院记录仓库 */
} MedicalRecordService;

/**
 * @brief 初始化病历服务
 *
 * 分别初始化挂号、就诊、检查和住院记录仓库，并确保存储文件就绪。
 *
 * @param service            指向待初始化的病历服务结构体
 * @param registration_path  挂号数据文件路径
 * @param visit_path         就诊记录数据文件路径
 * @param examination_path   检查记录数据文件路径
 * @param admission_path     住院记录数据文件路径
 * @return Result            操作结果，success=1 表示成功
 */
Result MedicalRecordService_init(
    MedicalRecordService *service,
    const char *registration_path,
    const char *visit_path,
    const char *examination_path,
    const char *admission_path
);

/**
 * @brief 创建就诊记录
 *
 * 根据挂号ID创建就诊记录，自动生成就诊ID，同时将挂号状态标记为已诊。
 * 每个挂号只能创建一条就诊记录。
 *
 * @param service     指向病历服务结构体
 * @param params      就诊记录参数（registration_id 为关联的挂号ID）
 * @param out_record  输出参数，创建成功时存放就诊记录
 * @return Result     操作结果，success=1 表示创建成功
 */
Result MedicalRecordService_create_visit_record(
    MedicalRecordService *service,
    const VisitRecordParams *params,
    VisitRecord *out_record
);

/**
 * @brief 更新就诊记录
 *
 * 根据就诊ID查找并更新就诊记录的各项内容。
 * 注意：params->registration_id 在更新时用作 visit_id（待更新的就诊ID）。
 *
 * @param service     指向病历服务结构体
 * @param visit_id    待更新的就诊ID
 * @param params      就诊记录参数（registration_id 字段在更新时不使用）
 * @param out_record  输出参数，更新成功时存放就诊记录
 * @return Result     操作结果，success=1 表示更新成功
 */
Result MedicalRecordService_update_visit_record(
    MedicalRecordService *service,
    const char *visit_id,
    const VisitRecordParams *params,
    VisitRecord *out_record
);

/**
 * @brief 删除就诊记录
 *
 * 删除指定就诊记录，同时将关联的挂号状态回退为待诊。
 * 如果该就诊记录下还有检查记录则不允许删除。
 *
 * @param service   指向病历服务结构体
 * @param visit_id  待删除的就诊ID
 * @return Result   操作结果，success=1 表示删除成功
 */
Result MedicalRecordService_delete_visit_record(
    MedicalRecordService *service,
    const char *visit_id
);

/**
 * @brief 创建检查记录
 *
 * 根据就诊ID创建检查记录，自动生成检查ID，初始状态为待检查。
 *
 * @param service       指向病历服务结构体
 * @param visit_id      关联的就诊ID
 * @param exam_item     检查项目名称
 * @param exam_type     检查类型
 * @param exam_fee      检查费用(元)，必须 >= 0
 * @param requested_at  申请检查的时间字符串
 * @param out_record    输出参数，创建成功时存放检查记录
 * @return Result       操作结果，success=1 表示创建成功
 */
Result MedicalRecordService_create_examination_record(
    MedicalRecordService *service,
    const char *visit_id,
    const char *exam_item,
    const char *exam_type,
    double exam_fee,
    const char *requested_at,
    ExaminationRecord *out_record
);

/**
 * @brief 更新检查记录
 *
 * 更新检查记录的状态、检查结果和完成时间。
 * 当状态为已完成时，必须提供检查结果和完成时间。
 *
 * @param service         指向病历服务结构体
 * @param examination_id  检查记录ID
 * @param status          新的检查状态
 * @param result          检查结果（已完成时必填）
 * @param completed_at    完成时间（已完成时必填）
 * @param out_record      输出参数，更新成功时存放检查记录
 * @return Result         操作结果，success=1 表示更新成功
 */
Result MedicalRecordService_update_examination_record(
    MedicalRecordService *service,
    const char *examination_id,
    ExaminationStatus status,
    const char *result,
    const char *completed_at,
    ExaminationRecord *out_record
);

/**
 * @brief 删除检查记录
 *
 * @param service         指向病历服务结构体
 * @param examination_id  待删除的检查记录ID
 * @return Result         操作结果，success=1 表示删除成功
 */
Result MedicalRecordService_delete_examination_record(
    MedicalRecordService *service,
    const char *examination_id
);

/**
 * @brief 查询患者的完整病历历史
 *
 * 聚合该患者的所有挂号记录、就诊记录、检查记录和住院记录。
 *
 * @param service      指向病历服务结构体
 * @param patient_id   患者ID
 * @param out_history  输出参数，查找成功时存放病历历史
 * @return Result      操作结果，success=1 表示查询完成
 */
Result MedicalRecordService_find_patient_history(
    MedicalRecordService *service,
    const char *patient_id,
    MedicalRecordHistory *out_history
);

/**
 * @brief 按时间范围查询病历记录
 *
 * 在指定时间范围内查找所有挂号、就诊、检查和住院记录。
 *
 * @param service      指向病历服务结构体
 * @param time_from    起始时间字符串（可为空表示不限制起始）
 * @param time_to      结束时间字符串（可为空表示不限制结束）
 * @param out_history  输出参数，查找成功时存放病历历史
 * @return Result      操作结果，success=1 表示查询完成
 */
Result MedicalRecordService_find_records_by_time_range(
    MedicalRecordService *service,
    const char *time_from,
    const char *time_to,
    MedicalRecordHistory *out_history
);

/**
 * @brief 清理病历历史记录
 *
 * 释放 MedicalRecordHistory 中所有链表的动态分配内存。
 *
 * @param history  指向待清理的病历历史结构体
 */
void MedicalRecordHistory_clear(MedicalRecordHistory *history);

#endif
