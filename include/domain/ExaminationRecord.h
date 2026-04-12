/**
 * @file ExaminationRecord.h
 * @brief 检查记录领域模型头文件
 *
 * 定义了检查记录(ExaminationRecord)的数据结构和状态枚举。
 * 检查记录用于跟踪医生为患者开具的各类检查（如化验、影像等），
 * 记录检查项目、类型、结果及完成状态。
 */

#ifndef HIS_DOMAIN_EXAMINATION_RECORD_H
#define HIS_DOMAIN_EXAMINATION_RECORD_H

#include "domain/DomainTypes.h"

/**
 * @brief 检查状态枚举
 */
typedef enum ExaminationStatus {
    EXAM_STATUS_PENDING = 0,    /* 待检查状态（已开具，等待执行） */
    EXAM_STATUS_COMPLETED = 1   /* 已完成状态（检查结果已出） */
} ExaminationStatus;

/**
 * @brief 检查记录结构体
 *
 * 存储一次检查的完整信息，包括检查项目、类型、结果和时间节点。
 */
typedef struct ExaminationRecord {
    char examination_id[HIS_DOMAIN_ID_CAPACITY];   /* 检查记录唯一标识ID */
    char visit_id[HIS_DOMAIN_ID_CAPACITY];         /* 关联的就诊记录ID */
    char patient_id[HIS_DOMAIN_ID_CAPACITY];       /* 受检患者ID */
    char doctor_id[HIS_DOMAIN_ID_CAPACITY];        /* 开具检查的医生ID */
    char exam_item[HIS_DOMAIN_NAME_CAPACITY];      /* 检查项目名称（如血常规、CT等） */
    char exam_type[HIS_DOMAIN_NAME_CAPACITY];      /* 检查类型（如化验、影像、超声等） */
    ExaminationStatus status;                       /* 检查当前状态（待检/已完成） */
    char result[HIS_DOMAIN_TEXT_CAPACITY];          /* 检查结果 */
    double exam_fee;                                /* 检查费用(元) */
    char requested_at[HIS_DOMAIN_TIME_CAPACITY];   /* 检查申请时间 */
    char completed_at[HIS_DOMAIN_TIME_CAPACITY];   /* 检查完成时间 */
} ExaminationRecord;

#endif
