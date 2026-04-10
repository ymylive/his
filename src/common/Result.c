/**
 * @file Result.c
 * @brief 操作结果模块的实现 - 创建表示成功或失败的 Result 结构体
 */

#include "common/Result.h"

#include <string.h>

/**
 * @brief 将消息字符串安全地复制到 Result 的 message 缓冲区
 *
 * 内部辅助函数，确保拷贝不超出缓冲区且始终以 '\0' 结尾。
 *
 * @param destination 目标消息缓冲区（长度为 RESULT_MESSAGE_CAPACITY）
 * @param message     源消息字符串，可以为 NULL
 */
static void Result_copy_message(char *destination, const char *message) {
    /* 源消息为 NULL 时，将目标缓冲区设为空字符串 */
    if (message == 0) {
        destination[0] = '\0';
        return;
    }

    /* 安全拷贝，最多复制 RESULT_MESSAGE_CAPACITY-1 个字符 */
    strncpy(destination, message, RESULT_MESSAGE_CAPACITY - 1);
    /* 确保末尾有 '\0'，防止截断时遗漏 */
    destination[RESULT_MESSAGE_CAPACITY - 1] = '\0';
}

/**
 * @brief 创建一个表示成功的 Result
 *
 * @param message 成功提示消息，可以为 NULL
 * @return 填充好的 Result 结构体（success = 1）
 */
Result Result_make_success(const char *message) {
    Result result;

    result.success = 1;
    Result_copy_message(result.message, message);
    return result;
}

/**
 * @brief 创建一个表示失败的 Result
 *
 * @param message 错误原因描述，可以为 NULL
 * @return 填充好的 Result 结构体（success = 0）
 */
Result Result_make_failure(const char *message) {
    Result result;

    result.success = 0;
    Result_copy_message(result.message, message);
    return result;
}
