/**
 * @file RoundRecord.h
 * @brief 查房记录领域模型头文件
 *
 * 定义每日查房记录（Ward Round Record）的数据结构，
 * 记录医生对住院患者的查房观察和治疗计划调整。
 */

#ifndef HIS_DOMAIN_ROUND_RECORD_H
#define HIS_DOMAIN_ROUND_RECORD_H

#include "domain/DomainTypes.h"

/**
 * @brief 查房记录结构体
 *
 * 记录医生对住院患者的每日查房信息，包括病情观察和治疗计划调整。
 */
typedef struct RoundRecord {
    char round_id[HIS_DOMAIN_ID_CAPACITY];          /**< 查房记录ID */
    char admission_id[HIS_DOMAIN_ID_CAPACITY];      /**< 关联住院记录ID */
    char doctor_id[HIS_DOMAIN_ID_CAPACITY];         /**< 查房医生ID */
    char findings[HIS_DOMAIN_LARGE_TEXT_CAPACITY];   /**< 查房发现/病情观察 */
    char plan[HIS_DOMAIN_LARGE_TEXT_CAPACITY];       /**< 治疗计划/调整方案 */
    char rounded_at[HIS_DOMAIN_TIME_CAPACITY];       /**< 查房时间 */
} RoundRecord;

#endif
