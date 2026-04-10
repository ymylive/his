/**
 * @file IdGenerator.c
 * @brief ID 生成器模块的实现 - 将前缀和序列号组合为格式化 ID 字符串
 */

#include "common/IdGenerator.h"

#include <stdio.h>

/**
 * @brief 将前缀和序列号格式化为 ID 字符串
 *
 * 使用 snprintf 将前缀与零填充的序列号拼接成完整的 ID 字符串。
 * 例如：prefix="P", sequence=5, width=3 -> "P005"
 *
 * @param buffer   输出缓冲区，用于存放生成的 ID 字符串
 * @param capacity 输出缓冲区的容量（字节数，含末尾 '\0'）
 * @param prefix   ID 的前缀字符串（如 "P"、"D" 等）
 * @param sequence 序列号（非负整数）
 * @param width    序列号部分的最小位宽（不足则左侧补零）
 * @return 成功返回 1，参数无效或缓冲区不足返回 0
 */
int IdGenerator_format(
    char *buffer,
    size_t capacity,
    const char *prefix,
    int sequence,
    int width
) {
    int written = 0;

    /* 参数合法性检查：缓冲区、前缀不能为空，序列号和位宽不能为负 */
    if (buffer == 0 || capacity == 0 || prefix == 0 || sequence < 0 || width < 0) {
        return 0;
    }

    /* 使用 snprintf 格式化输出，%0*d 表示零填充、动态宽度的整数 */
    written = snprintf(buffer, capacity, "%s%0*d", prefix, width, sequence);

    /* snprintf 返回负值表示编码错误 */
    if (written < 0) {
        buffer[0] = '\0';
        return 0;
    }

    /* 实际需要的字符数 >= 缓冲区容量，说明输出被截断 */
    if ((size_t)written >= capacity) {
        buffer[0] = '\0';
        return 0;
    }

    return 1;
}
