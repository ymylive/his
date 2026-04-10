/**
 * @file InpatientOrder.h
 * @brief 住院医嘱领域模型头文件
 *
 * 定义了住院医嘱(InpatientOrder)的数据结构、状态枚举及操作函数。
 * 住院医嘱是医生为住院患者开具的治疗指令，
 * 支持执行和取消操作，跟踪医嘱的完整生命周期。
 */

#ifndef HIS_DOMAIN_INPATIENT_ORDER_H
#define HIS_DOMAIN_INPATIENT_ORDER_H

#include "domain/DomainTypes.h"

/**
 * @brief 住院医嘱状态枚举
 */
typedef enum InpatientOrderStatus {
    INPATIENT_ORDER_STATUS_PENDING = 0,    /* 待执行状态 */
    INPATIENT_ORDER_STATUS_EXECUTED = 1,   /* 已执行状态 */
    INPATIENT_ORDER_STATUS_CANCELLED = 2   /* 已取消状态 */
} InpatientOrderStatus;

/**
 * @brief 住院医嘱结构体
 *
 * 存储一条住院医嘱的完整信息，包括医嘱类型、内容及各时间节点。
 */
typedef struct InpatientOrder {
    char order_id[HIS_DOMAIN_ID_CAPACITY];              /* 医嘱唯一标识ID */
    char admission_id[HIS_DOMAIN_ID_CAPACITY];          /* 关联的入院记录ID */
    char order_type[HIS_DOMAIN_NAME_CAPACITY];          /* 医嘱类型（如用药、检查、护理等） */
    char content[HIS_DOMAIN_LARGE_TEXT_CAPACITY];       /* 医嘱具体内容 */
    char ordered_at[HIS_DOMAIN_TIME_CAPACITY];          /* 医嘱开具时间 */
    InpatientOrderStatus status;                         /* 医嘱当前状态 */
    char executed_at[HIS_DOMAIN_TIME_CAPACITY];          /* 医嘱执行时间 */
    char cancelled_at[HIS_DOMAIN_TIME_CAPACITY];        /* 医嘱取消时间 */
} InpatientOrder;

/**
 * @brief 将医嘱标记为已执行
 *
 * 仅当医嘱状态为"待执行"时才可标记为已执行。
 *
 * @param order       指向住院医嘱的指针
 * @param executed_at 执行时间字符串
 * @return int 成功返回1，失败返回0（参数无效或状态不允许）
 */
int InpatientOrder_mark_executed(InpatientOrder *order, const char *executed_at);

/**
 * @brief 取消医嘱
 *
 * 仅当医嘱状态为"待执行"时才可取消。
 *
 * @param order        指向住院医嘱的指针
 * @param cancelled_at 取消时间字符串
 * @return int 成功返回1，失败返回0（参数无效或状态不允许）
 */
int InpatientOrder_cancel(InpatientOrder *order, const char *cancelled_at);

#endif
