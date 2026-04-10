/**
 * @file VisitRecord.h
 * @brief 就诊记录领域模型头文件
 *
 * 定义了就诊记录(VisitRecord)的数据结构。
 * 就诊记录是门诊流程的核心，记录医生对患者的诊断结果、
 * 主诉、医嘱建议，以及是否需要检查、住院或开药等信息。
 */

#ifndef HIS_DOMAIN_VISIT_RECORD_H
#define HIS_DOMAIN_VISIT_RECORD_H

#include "domain/DomainTypes.h"

/**
 * @brief 就诊记录结构体
 *
 * 存储一次门诊就诊的完整信息，包括关联的挂号、诊断和后续处置标记。
 */
typedef struct VisitRecord {
    char visit_id[HIS_DOMAIN_ID_CAPACITY];             /* 就诊记录唯一标识ID */
    char registration_id[HIS_DOMAIN_ID_CAPACITY];      /* 关联的挂号记录ID */
    char patient_id[HIS_DOMAIN_ID_CAPACITY];           /* 就诊患者ID */
    char doctor_id[HIS_DOMAIN_ID_CAPACITY];            /* 接诊医生ID */
    char department_id[HIS_DOMAIN_ID_CAPACITY];        /* 就诊科室ID */
    char chief_complaint[HIS_DOMAIN_TEXT_CAPACITY];    /* 主诉（患者自述症状） */
    char diagnosis[HIS_DOMAIN_TEXT_CAPACITY];           /* 诊断结果 */
    char advice[HIS_DOMAIN_TEXT_CAPACITY];              /* 医嘱/建议 */
    int need_exam;                                       /* 是否需要检查（1=需要，0=不需要） */
    int need_admission;                                  /* 是否需要住院（1=需要，0=不需要） */
    int need_medicine;                                   /* 是否需要开药（1=需要，0=不需要） */
    char visit_time[HIS_DOMAIN_TIME_CAPACITY];          /* 就诊时间 */
} VisitRecord;

#endif
