/**
 * @file IdGenerator.h
 * @brief ID 生成器模块 - 用于生成带前缀的格式化编号字符串
 *
 * 本模块提供将前缀和序列号组合成固定格式 ID 字符串的功能，
 * 例如将前缀 "P" 和序列号 5 组合成 "P005"。
 */

#ifndef HIS_COMMON_ID_GENERATOR_H
#define HIS_COMMON_ID_GENERATOR_H

#include <stddef.h>

/**
 * @brief 将前缀和序列号格式化为 ID 字符串
 *
 * 生成格式为 "<前缀><零填充序列号>" 的字符串，例如 "P005"。
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
);

#endif
