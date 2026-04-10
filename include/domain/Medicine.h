/**
 * @file Medicine.h
 * @brief 药品领域模型头文件
 *
 * 定义了药品(Medicine)的数据结构及库存校验函数。
 * 药品是药房管理和处方开具的核心实体，
 * 记录药品的基本信息、价格、库存及库存预警阈值。
 */

#ifndef HIS_DOMAIN_MEDICINE_H
#define HIS_DOMAIN_MEDICINE_H

#include "domain/DomainTypes.h"

/**
 * @brief 药品信息结构体
 *
 * 存储药品的基本信息、定价、库存数量及低库存预警阈值。
 */
typedef struct Medicine {
    char medicine_id[HIS_DOMAIN_ID_CAPACITY];      /* 药品唯一标识ID */
    char name[HIS_DOMAIN_NAME_CAPACITY];            /* 药品名称 */
    char alias[HIS_DOMAIN_NAME_CAPACITY];           /**< 药品别名/通用名 */
    double price;                                    /* 药品单价（元） */
    int stock;                                       /* 当前库存数量 */
    char department_id[HIS_DOMAIN_ID_CAPACITY];    /* 所属科室/药房ID */
    int low_stock_threshold;                         /* 低库存预警阈值 */
} Medicine;

/**
 * @brief 校验药品库存数据是否合法
 *
 * 检查药品指针非空、价格非负、库存非负、预警阈值非负。
 *
 * @param medicine 指向药品的常量指针
 * @return int 数据合法返回1，不合法返回0
 */
static inline int Medicine_has_valid_inventory(const Medicine *medicine) {
    return medicine != 0 &&
        medicine->price >= 0.0 &&
        medicine->stock >= 0 &&
        medicine->low_stock_threshold >= 0;
}

#endif
