/**
 * @file InpatientOrderService.c
 * @brief 住院医嘱服务模块实现
 *
 * 实现住院医嘱的业务操作功能，包括创建医嘱（含自动ID生成）、
 * 按入院记录查询、执行医嘱、取消医嘱以及加载全部医嘱。
 */

#include "service/InpatientOrderService.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/IdGenerator.h"
#include "common/StringUtils.h"
#include "repository/RepositoryUtils.h"

/**
 * @brief 医嘱计数上下文结构体
 *
 * 用于在逐行遍历回调中计数医嘱行数，以生成下一个医嘱ID。
 */
typedef struct InpatientOrderServiceCountContext {
    int count; /**< 当前已有的医嘱记录数量 */
} InpatientOrderServiceCountContext;

/**
 * @brief 校验必填文本字段
 *
 * @param text       待校验的文本
 * @param field_name 字段名称（用于错误消息）
 * @return Result    校验结果
 */
static Result InpatientOrderService_validate_text(const char *text, const char *field_name) {
    char message[RESULT_MESSAGE_CAPACITY];

    if (StringUtils_is_blank(text)) {
        snprintf(message, sizeof(message), "%s missing", field_name);
        return Result_make_failure(message);
    }

    if (!RepositoryUtils_is_safe_field_text(text)) {
        snprintf(message, sizeof(message), "%s contains reserved characters", field_name);
        return Result_make_failure(message);
    }

    return Result_make_success("order text valid");
}

/**
 * @brief 逐行计数回调：统计医嘱记录行数
 *
 * @param line    数据行文本（未使用）
 * @param context 计数上下文指针
 * @return Result 操作结果
 */
static Result InpatientOrderService_count_line(const char *line, void *context) {
    InpatientOrderServiceCountContext *count_context =
        (InpatientOrderServiceCountContext *)context;

    (void)line;
    if (count_context == 0) {
        return Result_make_failure("order count context missing");
    }

    count_context->count++;
    return Result_make_success("order counted");
}

/**
 * @brief 生成下一个医嘱ID
 *
 * 通过统计已有记录行数确定序列号，生成格式为 "ORD000001" 的医嘱ID。
 *
 * @param service     指向服务结构体
 * @param buffer      输出缓冲区
 * @param buffer_size 缓冲区容量
 * @return Result     操作结果
 */
static Result InpatientOrderService_generate_id(
    InpatientOrderService *service,
    char *buffer,
    size_t buffer_size
) {
    InpatientOrderServiceCountContext context;
    Result result;

    if (service == 0 || buffer == 0 || buffer_size == 0) {
        return Result_make_failure("order id arguments missing");
    }

    context.count = 0;
    result = TextFileRepository_for_each_data_line(
        &service->order_repository.storage,
        InpatientOrderService_count_line,
        &context
    );
    if (result.success == 0) {
        return result;
    }

    if (!IdGenerator_format(buffer, buffer_size, "ORD", context.count + 1, 6)) {
        return Result_make_failure("failed to generate order id");
    }

    return Result_make_success("order id generated");
}

/**
 * @brief 初始化住院医嘱服务
 */
Result InpatientOrderService_init(InpatientOrderService *service, const char *path) {
    if (service == 0) {
        return Result_make_failure("inpatient order service missing");
    }

    return InpatientOrderRepository_init(&service->order_repository, path);
}

/**
 * @brief 创建住院医嘱
 *
 * 流程：校验必填字段 -> 生成医嘱ID -> 设置初始状态 -> 追加到存储。
 */
Result InpatientOrderService_create_order(
    InpatientOrderService *service,
    InpatientOrder *order
) {
    Result result;

    if (service == 0 || order == 0) {
        return Result_make_failure("create order arguments missing");
    }

    /* 校验必填字段 */
    result = InpatientOrderService_validate_text(order->admission_id, "admission id");
    if (result.success == 0) {
        return result;
    }

    result = InpatientOrderService_validate_text(order->order_type, "order type");
    if (result.success == 0) {
        return result;
    }

    result = InpatientOrderService_validate_text(order->content, "order content");
    if (result.success == 0) {
        return result;
    }

    result = InpatientOrderService_validate_text(order->ordered_at, "ordered at");
    if (result.success == 0) {
        return result;
    }

    /* 生成医嘱ID */
    result = InpatientOrderService_generate_id(
        service,
        order->order_id,
        sizeof(order->order_id)
    );
    if (result.success == 0) {
        return result;
    }

    /* 设置初始状态为待执行 */
    order->status = INPATIENT_ORDER_STATUS_PENDING;
    order->executed_at[0] = '\0';
    order->cancelled_at[0] = '\0';

    /* 追加到存储 */
    return InpatientOrderRepository_append(&service->order_repository, order);
}

/**
 * @brief 按入院记录ID查找所有关联的医嘱
 */
Result InpatientOrderService_find_by_admission_id(
    InpatientOrderService *service,
    const char *admission_id,
    LinkedList *out_list
) {
    LinkedList all_orders;
    LinkedListNode *current = 0;
    Result result;

    if (service == 0 || out_list == 0) {
        return Result_make_failure("find orders arguments missing");
    }

    result = InpatientOrderService_validate_text(admission_id, "admission id");
    if (result.success == 0) {
        return result;
    }

    LinkedList_init(out_list);

    /* 加载全部医嘱 */
    result = InpatientOrderRepository_load_all(&service->order_repository, &all_orders);
    if (result.success == 0) {
        return result;
    }

    /* 筛选匹配的入院记录 */
    current = all_orders.head;
    while (current != 0) {
        const InpatientOrder *order = (const InpatientOrder *)current->data;

        if (strcmp(order->admission_id, admission_id) == 0) {
            InpatientOrder *copy = (InpatientOrder *)malloc(sizeof(*copy));
            if (copy == 0) {
                InpatientOrderRepository_clear_loaded_list(&all_orders);
                InpatientOrderService_clear_results(out_list);
                return Result_make_failure("failed to allocate order copy");
            }
            *copy = *order;
            if (!LinkedList_append(out_list, copy)) {
                free(copy);
                InpatientOrderRepository_clear_loaded_list(&all_orders);
                InpatientOrderService_clear_results(out_list);
                return Result_make_failure("failed to append order copy");
            }
        }

        current = current->next;
    }

    InpatientOrderRepository_clear_loaded_list(&all_orders);
    return Result_make_success("orders found by admission");
}

/**
 * @brief 执行医嘱
 *
 * 流程：加载全部医嘱 -> 查找目标 -> 标记已执行 -> 全量保存。
 */
Result InpatientOrderService_execute_order(
    InpatientOrderService *service,
    const char *order_id,
    const char *executed_at
) {
    LinkedList orders;
    LinkedListNode *current = 0;
    InpatientOrder *target = 0;
    Result result;

    if (service == 0) {
        return Result_make_failure("execute order service missing");
    }

    result = InpatientOrderService_validate_text(order_id, "order id");
    if (result.success == 0) {
        return result;
    }

    result = InpatientOrderService_validate_text(executed_at, "executed at");
    if (result.success == 0) {
        return result;
    }

    /* 加载全部医嘱 */
    result = InpatientOrderRepository_load_all(&service->order_repository, &orders);
    if (result.success == 0) {
        return result;
    }

    /* 查找目标医嘱 */
    current = orders.head;
    while (current != 0) {
        InpatientOrder *order = (InpatientOrder *)current->data;
        if (strcmp(order->order_id, order_id) == 0) {
            target = order;
            break;
        }
        current = current->next;
    }

    if (target == 0) {
        InpatientOrderRepository_clear_loaded_list(&orders);
        return Result_make_failure("order not found");
    }

    /* 标记已执行 */
    if (!InpatientOrder_mark_executed(target, executed_at)) {
        InpatientOrderRepository_clear_loaded_list(&orders);
        return Result_make_failure("order cannot be executed (not pending)");
    }

    /* 全量保存 */
    result = InpatientOrderRepository_save_all(&service->order_repository, &orders);
    InpatientOrderRepository_clear_loaded_list(&orders);
    return result;
}

/**
 * @brief 取消医嘱
 *
 * 流程：加载全部医嘱 -> 查找目标 -> 标记已取消 -> 全量保存。
 */
Result InpatientOrderService_cancel_order(
    InpatientOrderService *service,
    const char *order_id,
    const char *cancelled_at
) {
    LinkedList orders;
    LinkedListNode *current = 0;
    InpatientOrder *target = 0;
    Result result;

    if (service == 0) {
        return Result_make_failure("cancel order service missing");
    }

    result = InpatientOrderService_validate_text(order_id, "order id");
    if (result.success == 0) {
        return result;
    }

    result = InpatientOrderService_validate_text(cancelled_at, "cancelled at");
    if (result.success == 0) {
        return result;
    }

    /* 加载全部医嘱 */
    result = InpatientOrderRepository_load_all(&service->order_repository, &orders);
    if (result.success == 0) {
        return result;
    }

    /* 查找目标医嘱 */
    current = orders.head;
    while (current != 0) {
        InpatientOrder *order = (InpatientOrder *)current->data;
        if (strcmp(order->order_id, order_id) == 0) {
            target = order;
            break;
        }
        current = current->next;
    }

    if (target == 0) {
        InpatientOrderRepository_clear_loaded_list(&orders);
        return Result_make_failure("order not found");
    }

    /* 标记已取消 */
    if (!InpatientOrder_cancel(target, cancelled_at)) {
        InpatientOrderRepository_clear_loaded_list(&orders);
        return Result_make_failure("order cannot be cancelled (not pending)");
    }

    /* 全量保存 */
    result = InpatientOrderRepository_save_all(&service->order_repository, &orders);
    InpatientOrderRepository_clear_loaded_list(&orders);
    return result;
}

/**
 * @brief 加载所有住院医嘱
 */
Result InpatientOrderService_load_all(
    InpatientOrderService *service,
    LinkedList *out_list
) {
    if (service == 0 || out_list == 0) {
        return Result_make_failure("load all orders arguments missing");
    }

    return InpatientOrderRepository_load_all(&service->order_repository, out_list);
}

/**
 * @brief 清理医嘱查询结果链表
 */
void InpatientOrderService_clear_results(LinkedList *list) {
    LinkedList_clear(list, free);
}
