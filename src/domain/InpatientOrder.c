/**
 * @file InpatientOrder.c
 * @brief 住院医嘱领域模型实现文件
 *
 * 实现住院医嘱(InpatientOrder)的业务操作函数，
 * 包括将医嘱标记为已执行和取消医嘱。
 * 所有状态变更操作均包含前置条件校验，确保状态转换的合法性。
 */

#include "domain/InpatientOrder.h"

#include <string.h>

/**
 * @brief 安全拷贝时间字符串（内部辅助函数）
 *
 * 将源时间字符串安全地拷贝到目标缓冲区，确保不会发生缓冲区溢出。
 * 如果源字符串为空指针，则将目标缓冲区清空。
 *
 * @param destination 目标缓冲区指针
 * @param source      源时间字符串指针
 */
static void InpatientOrder_copy_time(char *destination, const char *source) {
    /* 目标缓冲区为空，无法写入，直接返回 */
    if (destination == 0) {
        return;
    }

    /* 源字符串为空，将目标缓冲区置为空字符串 */
    if (source == 0) {
        destination[0] = '\0';
        return;
    }

    /* 使用 strncpy 安全拷贝，预留最后一个字节用于终止符 */
    strncpy(destination, source, HIS_DOMAIN_TIME_CAPACITY - 1);
    /* 确保字符串以 '\0' 结尾，防止溢出 */
    destination[HIS_DOMAIN_TIME_CAPACITY - 1] = '\0';
}

/**
 * @brief 将医嘱标记为已执行
 *
 * 仅当医嘱处于"待执行"状态时才允许标记为已执行。
 * 执行后记录执行时间，并清空取消时间。
 *
 * @param order       指向住院医嘱的指针
 * @param executed_at 执行时间字符串
 * @return int 成功返回1，失败返回0
 */
int InpatientOrder_mark_executed(InpatientOrder *order, const char *executed_at) {
    /* 参数校验：医嘱指针、执行时间非空，且执行时间不为空字符串 */
    /* 状态校验：仅"待执行"状态的医嘱才可被标记为已执行 */
    if (order == 0 || executed_at == 0 || executed_at[0] == '\0' ||
        order->status != INPATIENT_ORDER_STATUS_PENDING) {
        return 0;
    }

    /* 更新状态为已执行 */
    order->status = INPATIENT_ORDER_STATUS_EXECUTED;
    /* 记录执行时间 */
    InpatientOrder_copy_time(order->executed_at, executed_at);
    /* 清空取消时间（已执行的医嘱不存在取消时间） */
    order->cancelled_at[0] = '\0';
    return 1;
}

/**
 * @brief 取消医嘱
 *
 * 仅当医嘱处于"待执行"状态时才允许取消。
 * 取消后记录取消时间，并清空执行时间。
 *
 * @param order        指向住院医嘱的指针
 * @param cancelled_at 取消时间字符串
 * @return int 成功返回1，失败返回0
 */
int InpatientOrder_cancel(InpatientOrder *order, const char *cancelled_at) {
    /* 参数校验：医嘱指针、取消时间非空，且取消时间不为空字符串 */
    /* 状态校验：仅"待执行"状态的医嘱才可被取消 */
    if (order == 0 || cancelled_at == 0 || cancelled_at[0] == '\0' ||
        order->status != INPATIENT_ORDER_STATUS_PENDING) {
        return 0;
    }

    /* 更新状态为已取消 */
    order->status = INPATIENT_ORDER_STATUS_CANCELLED;
    /* 清空执行时间（已取消的医嘱不存在执行时间） */
    order->executed_at[0] = '\0';
    /* 记录取消时间 */
    InpatientOrder_copy_time(order->cancelled_at, cancelled_at);
    return 1;
}
