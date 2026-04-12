/**
 * @file NursingRecord.h
 * @brief 护理记录领域模型头文件
 *
 * 定义护理记录（NursingRecord）结构体，用于跟踪住院患者的护理数据，
 * 包括体温、血压、用药、护理观察、输液等记录类型。
 */

#ifndef HIS_DOMAIN_NURSING_RECORD_H
#define HIS_DOMAIN_NURSING_RECORD_H

#include "domain/DomainTypes.h"

/**
 * @brief 护理记录结构体
 *
 * 存储一条护理记录的完整信息，关联住院记录。
 */
typedef struct NursingRecord {
    char nursing_id[HIS_DOMAIN_ID_CAPACITY];       /**< 护理记录ID */
    char admission_id[HIS_DOMAIN_ID_CAPACITY];     /**< 关联住院记录ID */
    char nurse_name[HIS_DOMAIN_NAME_CAPACITY];     /**< 护士姓名 */
    char record_type[HIS_DOMAIN_NAME_CAPACITY];    /**< 记录类型(体温/血压/用药/护理观察/输液) */
    char content[HIS_DOMAIN_LARGE_TEXT_CAPACITY];   /**< 记录内容 */
    char recorded_at[HIS_DOMAIN_TIME_CAPACITY];    /**< 记录时间 */
} NursingRecord;

#endif
