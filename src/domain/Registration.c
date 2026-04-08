/**
 * @file Registration.c
 * @brief 挂号记录领域模型实现文件
 *
 * 实现挂号记录(Registration)的业务操作函数，
 * 包括标记已就诊和取消挂号。
 * 所有状态变更操作均要求当前状态为"待诊"，确保状态转换的合法性。
 */

#include "domain/Registration.h"

#include <string.h>

/**
 * @brief 安全拷贝时间字符串（内部辅助函数）
 *
 * 将源时间字符串安全地拷贝到目标缓冲区，确保不会发生缓冲区溢出。
 * 如果源字符串为空指针，则将目标缓冲区清空。
 *
 * @param destination 目标缓冲区指针
 * @param value       源时间字符串指针
 */
static void Registration_copy_time(char *destination, const char *value) {
    /* 目标缓冲区为空，无法写入，直接返回 */
    if (destination == 0) {
        return;
    }

    /* 源字符串为空，将目标缓冲区置为空字符串 */
    if (value == 0) {
        destination[0] = '\0';
        return;
    }

    /* 使用 strncpy 安全拷贝，预留最后一个字节用于终止符 */
    strncpy(destination, value, HIS_DOMAIN_TIME_CAPACITY - 1);
    /* 确保字符串以 '\0' 结尾，防止溢出 */
    destination[HIS_DOMAIN_TIME_CAPACITY - 1] = '\0';
}

/**
 * @brief 将挂号记录标记为已就诊
 *
 * 仅当挂号状态为"待诊"时才允许标记为已就诊，并记录就诊完成时间。
 *
 * @param registration 指向挂号记录的指针
 * @param diagnosed_at 就诊完成时间字符串
 * @return int 成功返回1，失败返回0
 */
int Registration_mark_diagnosed(Registration *registration, const char *diagnosed_at) {
    /* 参数校验：挂号记录指针和就诊时间均不能为空 */
    /* 状态校验：仅"待诊"状态的挂号才可标记为已就诊 */
    if (registration == 0 || diagnosed_at == 0 || registration->status != REG_STATUS_PENDING) {
        return 0;
    }

    /* 更新状态为已就诊 */
    registration->status = REG_STATUS_DIAGNOSED;
    /* 记录就诊完成时间 */
    Registration_copy_time(registration->diagnosed_at, diagnosed_at);
    return 1;
}

/**
 * @brief 取消挂号记录
 *
 * 仅当挂号状态为"待诊"时才允许取消，并记录取消时间。
 *
 * @param registration 指向挂号记录的指针
 * @param cancelled_at 取消时间字符串
 * @return int 成功返回1，失败返回0
 */
int Registration_cancel(Registration *registration, const char *cancelled_at) {
    /* 参数校验：挂号记录指针和取消时间均不能为空 */
    /* 状态校验：仅"待诊"状态的挂号才可取消 */
    if (registration == 0 || cancelled_at == 0 || registration->status != REG_STATUS_PENDING) {
        return 0;
    }

    /* 更新状态为已取消 */
    registration->status = REG_STATUS_CANCELLED;
    /* 记录取消时间 */
    Registration_copy_time(registration->cancelled_at, cancelled_at);
    return 1;
}
