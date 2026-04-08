/**
 * @file StringUtils.h
 * @brief 字符串工具模块 - 提供字符串判空、安全复制和字段验证等常用操作
 *
 * 本模块封装了常用的字符串处理函数，包括空白检测、安全拷贝、
 * 时间值拷贝以及必填字段的验证逻辑。
 */

#ifndef HIS_COMMON_STRING_UTILS_H
#define HIS_COMMON_STRING_UTILS_H

#include <stddef.h>
#include <time.h>

#include "common/Result.h"

/**
 * @brief 判断字符串是否为空白（NULL 或全部为空白字符）
 *
 * @param text 待检测的字符串指针，可以为 NULL
 * @return 如果为 NULL 或全为空白字符返回 1，否则返回 0
 */
int StringUtils_is_blank(const char *text);

/**
 * @brief 判断字符串是否包含非空白内容
 *
 * 与 StringUtils_is_blank 互为反义操作。
 *
 * @param text 待检测的字符串指针，可以为 NULL
 * @return 如果非 NULL 且包含非空白字符返回 1，否则返回 0
 */
int StringUtils_has_text(const char *text);

/**
 * @brief 安全地将源字符串复制到固定大小的目标缓冲区
 *
 * 当 src 为 NULL 时，将 dest 清空（设为空字符串）。
 * 始终确保目标缓冲区以 '\0' 结尾，防止缓冲区溢出。
 *
 * @param dest     目标缓冲区
 * @param capacity 目标缓冲区的容量（字节数，含末尾 '\0'）
 * @param src      源字符串，可以为 NULL
 */
void StringUtils_copy(char *dest, size_t capacity, const char *src);

/**
 * @brief 安全地复制 time_t 时间值
 *
 * 当 src 为 NULL 时，将 dest 置零。
 *
 * @param dest 目标时间指针
 * @param src  源时间指针，可以为 NULL
 */
void StringUtils_copy_time(time_t *dest, const time_t *src);

/**
 * @brief 验证必填文本字段
 *
 * 依次检查：是否为空白、是否超出最大长度、是否包含保留字符。
 *
 * @param text         待验证的文本内容
 * @param field_name   字段名称（用于生成错误提示信息）
 * @param max_capacity 字段允许的最大长度（含 '\0'），为 0 表示不限制长度
 * @return Result 结构体，success 为 1 表示验证通过，为 0 表示验证失败（含错误信息）
 */
Result StringUtils_validate_required(
    const char *text,
    const char *field_name,
    size_t max_capacity
);

#endif
