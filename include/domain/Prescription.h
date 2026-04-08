/**
 * @file Prescription.h
 * @brief 处方领域模型头文件
 *
 * 定义了处方(Prescription)的数据结构及数量校验函数。
 * 处方由医生在就诊时开具，关联就诊记录和药品，
 * 记录开药数量和用法用量。
 */

#ifndef HIS_DOMAIN_PRESCRIPTION_H
#define HIS_DOMAIN_PRESCRIPTION_H

#include "domain/DomainTypes.h"

/**
 * @brief 处方信息结构体
 *
 * 存储一条处方明细，包括关联的就诊记录、药品、数量和用法。
 */
typedef struct Prescription {
    char prescription_id[HIS_DOMAIN_ID_CAPACITY];  /* 处方唯一标识ID */
    char visit_id[HIS_DOMAIN_ID_CAPACITY];         /* 关联的就诊记录ID */
    char medicine_id[HIS_DOMAIN_ID_CAPACITY];      /* 处方药品ID */
    int quantity;                                    /* 开药数量 */
    char usage[HIS_DOMAIN_TEXT_CAPACITY];           /* 用法用量说明 */
} Prescription;

/**
 * @brief 校验处方数量是否合法
 *
 * 检查开药数量是否大于0，可用库存是否非负，
 * 以及开药数量是否不超过可用库存。
 *
 * @param quantity        开药数量
 * @param available_stock 当前可用库存数量
 * @return int 合法返回1，不合法返回0
 */
static inline int Prescription_has_valid_quantity(int quantity, int available_stock) {
    return quantity > 0 && available_stock >= 0 && quantity <= available_stock;
}

#endif
