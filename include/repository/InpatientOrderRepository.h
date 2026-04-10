/**
 * @file InpatientOrderRepository.h
 * @brief 住院医嘱数据仓储层头文件
 *
 * 提供住院医嘱（InpatientOrder）数据的持久化存储功能，基于文本文件存储。
 * 支持医嘱的追加、按ID查找、全量加载和全量保存操作。
 *
 * 数据文件格式：每行一条医嘱记录，字段以管道符 '|' 分隔。
 * 表头：order_id|admission_id|order_type|content|ordered_at|status|executed_at|cancelled_at
 */

#ifndef HIS_REPOSITORY_INPATIENT_ORDER_REPOSITORY_H
#define HIS_REPOSITORY_INPATIENT_ORDER_REPOSITORY_H

#include "common/LinkedList.h"
#include "common/Result.h"
#include "domain/InpatientOrder.h"
#include "repository/TextFileRepository.h"

/** 住院医嘱数据文件的表头行 */
#define INPATIENT_ORDER_REPOSITORY_HEADER \
    "order_id|admission_id|order_type|content|ordered_at|status|executed_at|cancelled_at"

/** 住院医嘱记录的字段数量 */
#define INPATIENT_ORDER_REPOSITORY_FIELD_COUNT 8

/**
 * @brief 住院医嘱仓储结构体
 */
typedef struct InpatientOrderRepository {
    TextFileRepository storage; /**< 底层文本文件仓储 */
} InpatientOrderRepository;

/**
 * @brief 初始化住院医嘱仓储
 * @param repository 仓储实例指针
 * @param path       数据文件路径
 * @return 成功返回 success；参数无效或文件操作失败时返回 failure
 */
Result InpatientOrderRepository_init(InpatientOrderRepository *repository, const char *path);

/**
 * @brief 追加一条住院医嘱记录
 *
 * 会先加载所有记录并检查ID唯一性后再追加保存。
 *
 * @param repository 仓储实例指针
 * @param order      要追加的医嘱记录
 * @return 成功返回 success；ID重复或写入失败时返回 failure
 */
Result InpatientOrderRepository_append(
    const InpatientOrderRepository *repository,
    const InpatientOrder *order
);

/**
 * @brief 按医嘱ID查找医嘱记录
 * @param repository 仓储实例指针
 * @param order_id   要查找的医嘱ID
 * @param out_order  输出参数，存放找到的医嘱记录
 * @return 找到返回 success；未找到时返回 failure
 */
Result InpatientOrderRepository_find_by_id(
    const InpatientOrderRepository *repository,
    const char *order_id,
    InpatientOrder *out_order
);

/**
 * @brief 加载所有住院医嘱记录到链表
 * @param repository 仓储实例指针
 * @param out_orders 输出参数，存放医嘱记录的链表
 * @return 成功返回 success；加载失败时返回 failure
 */
Result InpatientOrderRepository_load_all(
    const InpatientOrderRepository *repository,
    LinkedList *out_orders
);

/**
 * @brief 全量保存住院医嘱列表到文件（覆盖写入）
 * @param repository 仓储实例指针
 * @param orders     要保存的医嘱链表
 * @return 成功返回 success；数据无效或写入失败时返回 failure
 */
Result InpatientOrderRepository_save_all(
    const InpatientOrderRepository *repository,
    const LinkedList *orders
);

/**
 * @brief 清空并释放住院医嘱链表中的所有元素
 * @param orders 要清空的医嘱链表
 */
void InpatientOrderRepository_clear_loaded_list(LinkedList *orders);

#endif
