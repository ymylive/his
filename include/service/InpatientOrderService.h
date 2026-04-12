/**
 * @file InpatientOrderService.h
 * @brief 住院医嘱服务模块头文件
 *
 * 提供住院医嘱的业务操作功能，包括创建医嘱、按入院记录查询、
 * 执行医嘱、取消医嘱以及加载全部医嘱。
 * 该服务依赖住院医嘱仓储层进行数据持久化。
 */

#ifndef HIS_SERVICE_INPATIENT_ORDER_SERVICE_H
#define HIS_SERVICE_INPATIENT_ORDER_SERVICE_H

#include "common/LinkedList.h"
#include "common/Result.h"
#include "domain/InpatientOrder.h"
#include "repository/InpatientOrderRepository.h"

/**
 * @brief 住院医嘱服务结构体
 *
 * 封装住院医嘱仓储，提供统一的医嘱管理接口。
 */
typedef struct InpatientOrderService {
    InpatientOrderRepository order_repository; /**< 住院医嘱数据仓储 */
} InpatientOrderService;

/**
 * @brief 初始化住院医嘱服务
 *
 * 初始化底层医嘱仓储，确保存储文件就绪。
 *
 * @param service 指向待初始化的服务结构体
 * @param path    医嘱数据文件路径
 * @return Result 操作结果，success=1 表示成功
 */
Result InpatientOrderService_init(InpatientOrderService *service, const char *path);

/**
 * @brief 创建住院医嘱
 *
 * 自动生成医嘱ID，校验必填字段后追加到存储中。
 * 新创建的医嘱状态为待执行（PENDING）。
 *
 * @param service 指向服务结构体
 * @param order   指向待创建的医嘱（order_id 将被自动生成覆盖）
 * @return Result 操作结果，success=1 表示创建成功
 */
Result InpatientOrderService_create_order(
    InpatientOrderService *service,
    InpatientOrder *order
);

/**
 * @brief 按入院记录ID查找所有关联的医嘱
 *
 * 加载所有医嘱后筛选出指定入院记录的医嘱列表。
 *
 * @param service      指向服务结构体
 * @param admission_id 入院记录ID
 * @param out_list     输出参数，存放匹配的医嘱链表
 * @return Result      操作结果，success=1 表示查询完成
 */
Result InpatientOrderService_find_by_admission_id(
    InpatientOrderService *service,
    const char *admission_id,
    LinkedList *out_list
);

/**
 * @brief 执行医嘱
 *
 * 将指定医嘱标记为已执行状态，记录执行时间。
 * 仅待执行状态的医嘱可被执行。
 *
 * @param service     指向服务结构体
 * @param order_id    医嘱ID
 * @param executed_at 执行时间字符串
 * @return Result     操作结果，success=1 表示执行成功
 */
Result InpatientOrderService_execute_order(
    InpatientOrderService *service,
    const char *order_id,
    const char *executed_at
);

/**
 * @brief 取消医嘱
 *
 * 将指定医嘱标记为已取消状态，记录取消时间。
 * 仅待执行状态的医嘱可被取消。
 *
 * @param service      指向服务结构体
 * @param order_id     医嘱ID
 * @param cancelled_at 取消时间字符串
 * @return Result      操作结果，success=1 表示取消成功
 */
Result InpatientOrderService_cancel_order(
    InpatientOrderService *service,
    const char *order_id,
    const char *cancelled_at
);

/**
 * @brief 加载所有住院医嘱
 *
 * @param service  指向服务结构体
 * @param out_list 输出参数，存放所有医嘱的链表
 * @return Result  操作结果，success=1 表示加载成功
 */
Result InpatientOrderService_load_all(
    InpatientOrderService *service,
    LinkedList *out_list
);

/**
 * @brief 清理医嘱查询结果链表
 *
 * 释放链表中所有动态分配的 InpatientOrder 节点。
 *
 * @param list 指向待清理的医嘱链表
 */
void InpatientOrderService_clear_results(LinkedList *list);

#endif
