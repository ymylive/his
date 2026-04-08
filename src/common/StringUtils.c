/**
 * @file StringUtils.c
 * @brief 字符串工具模块的实现 - 提供字符串判空、安全复制和字段验证等操作
 */

#include "common/StringUtils.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "repository/RepositoryUtils.h"

/**
 * @brief 判断字符串是否为空白（NULL 或全部为空白字符）
 *
 * @param text 待检测的字符串指针，可以为 NULL
 * @return 如果为 NULL 或全为空白字符返回 1，否则返回 0
 */
int StringUtils_is_blank(const char *text) {
    /* NULL 指针视为空白 */
    if (text == 0) {
        return 1;
    }

    /* 逐字符扫描，遇到非空白字符则不是空白 */
    while (*text != '\0') {
        if (!isspace((unsigned char)*text)) {
            return 0;
        }

        text++;
    }

    /* 全部字符都是空白（或空字符串） */
    return 1;
}

/**
 * @brief 判断字符串是否包含非空白内容
 *
 * @param text 待检测的字符串指针，可以为 NULL
 * @return 如果非 NULL 且包含非空白字符返回 1，否则返回 0
 */
int StringUtils_has_text(const char *text) {
    return !StringUtils_is_blank(text);
}

/**
 * @brief 安全地将源字符串复制到固定大小的目标缓冲区
 *
 * @param dest     目标缓冲区
 * @param capacity 目标缓冲区的容量（字节数，含末尾 '\0'）
 * @param src      源字符串，可以为 NULL（此时将 dest 清空）
 */
void StringUtils_copy(char *dest, size_t capacity, const char *src) {
    /* 目标缓冲区无效则直接返回 */
    if (dest == 0 || capacity == 0) {
        return;
    }

    /* 源字符串为 NULL 时，将目标缓冲区清空 */
    if (src == 0) {
        dest[0] = '\0';
        return;
    }

    /* 最多复制 capacity-1 个字符，预留末尾 '\0' 的位置 */
    strncpy(dest, src, capacity - 1);
    /* 确保末尾始终有 '\0'，防止 strncpy 未自动添加 */
    dest[capacity - 1] = '\0';
}

/**
 * @brief 安全地复制 time_t 时间值
 *
 * @param dest 目标时间指针
 * @param src  源时间指针，可以为 NULL（此时将 dest 置零）
 */
void StringUtils_copy_time(time_t *dest, const time_t *src) {
    /* 目标指针无效则直接返回 */
    if (dest == 0) {
        return;
    }

    /* 源指针为 NULL 时，将目标值置零 */
    if (src == 0) {
        *dest = 0;
        return;
    }

    *dest = *src;
}

/**
 * @brief 验证必填文本字段
 *
 * 依次进行三项检查：
 * 1. 是否为空白
 * 2. 是否超出最大长度
 * 3. 是否包含保留字符（如文件分隔符等）
 *
 * @param text         待验证的文本内容
 * @param field_name   字段名称（用于生成错误提示信息）
 * @param max_capacity 字段允许的最大长度（含 '\0'），为 0 表示不限制长度
 * @return Result 结构体，success 为 1 表示验证通过，为 0 表示验证失败
 */
Result StringUtils_validate_required(
    const char *text,
    const char *field_name,
    size_t max_capacity
) {
    char message[RESULT_MESSAGE_CAPACITY];

    /* 检查是否为空白 */
    if (StringUtils_is_blank(text)) {
        snprintf(message, sizeof(message), "%s missing", field_name);
        return Result_make_failure(message);
    }

    /* 检查是否超出允许的最大长度 */
    if (max_capacity > 0 && strlen(text) >= max_capacity) {
        snprintf(message, sizeof(message), "%s too long", field_name);
        return Result_make_failure(message);
    }

    /* 检查是否包含保留字符（如文件存储中的字段分隔符） */
    if (!RepositoryUtils_is_safe_field_text(text)) {
        snprintf(message, sizeof(message), "%s contains reserved characters", field_name);
        return Result_make_failure(message);
    }

    return Result_make_success("text valid");
}
