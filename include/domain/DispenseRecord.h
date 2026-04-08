/**
 * @file DispenseRecord.h
 * @brief 发药记录领域模型头文件
 *
 * 定义了发药记录(DispenseRecord)的数据结构和状态枚举。
 * 发药记录跟踪药房根据处方向患者发放药品的过程，
 * 记录发药的药剂师、数量和完成状态。
 */

#ifndef HIS_DOMAIN_DISPENSE_RECORD_H
#define HIS_DOMAIN_DISPENSE_RECORD_H

#include "domain/DomainTypes.h"

/**
 * @brief 发药状态枚举
 */
typedef enum DispenseStatus {
    DISPENSE_STATUS_PENDING = 0,    /* 待发药状态 */
    DISPENSE_STATUS_COMPLETED = 1   /* 已发药状态 */
} DispenseStatus;

/**
 * @brief 发药记录结构体
 *
 * 存储一次发药操作的完整信息，关联患者、处方、药品及药剂师。
 */
typedef struct DispenseRecord {
    char dispense_id[HIS_DOMAIN_ID_CAPACITY];       /* 发药记录唯一标识ID */
    char patient_id[HIS_DOMAIN_ID_CAPACITY];        /* 取药患者ID */
    char prescription_id[HIS_DOMAIN_ID_CAPACITY];   /* 关联的处方ID */
    char medicine_id[HIS_DOMAIN_ID_CAPACITY];       /* 发放的药品ID */
    int quantity;                                     /* 发药数量 */
    char pharmacist_id[HIS_DOMAIN_ID_CAPACITY];     /* 发药药剂师ID */
    char dispensed_at[HIS_DOMAIN_TIME_CAPACITY];     /* 发药时间 */
    DispenseStatus status;                            /* 发药状态（待发/已发） */
} DispenseRecord;

#endif
