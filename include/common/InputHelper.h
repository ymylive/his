/**
 * @file InputHelper.h
 * @brief 输入辅助模块 - 提供输入验证和支持 ESC 取消的行读取功能
 *
 * 本模块封装了常用的输入验证函数（非空检测、正整数检测、非负数检测）
 * 以及支持 ESC 键取消的终端行读取功能。在交互式终端下，首个按键
 * 会以 raw 模式预读，以便捕获 ESC 键。
 */

#ifndef HIS_COMMON_INPUT_HELPER_H
#define HIS_COMMON_INPUT_HELPER_H

#include <stdio.h>
#include <stddef.h>

/**
 * @brief 判断文本是否非空（去除首尾空白后仍有内容）
 *
 * @param text 待检测的字符串，可以为 NULL
 * @return 非空返回 1，为空或 NULL 返回 0
 */
int InputHelper_is_non_empty(const char *text);

/**
 * @brief 判断文本是否为正整数
 *
 * 允许首尾空白，但数字部分必须为正整数（不能全为零）。
 *
 * @param text 待检测的字符串，可以为 NULL
 * @return 是正整数返回 1，否则返回 0
 */
int InputHelper_is_positive_integer(const char *text);

/**
 * @brief 判断文本是否为非负数（整数或小数）
 *
 * 允许首尾空白，支持小数点（最多一个），不允许负号。
 *
 * @param text 待检测的字符串，可以为 NULL
 * @return 是非负数返回 1，否则返回 0
 */
int InputHelper_is_non_negative_amount(const char *text);

/**
 * @brief 支持 ESC 取消的行读取函数
 *
 * 从指定输入流中读取一行文本，并去除末尾换行符。
 * 在交互式终端下，首个按键会以 raw 模式预读，
 * 以便捕获单独的 ESC 键按下事件。如果检测到 ESC，
 * 会排空后续的转义序列字节（如方向键），避免污染下次读取。
 *
 * @param input    输入流（通常为 stdin）
 * @param buffer   输出缓冲区
 * @param capacity 缓冲区容量（字节数，含末尾 '\0'）
 * @return  1 = 成功读取一行
 *          0 = 遇到 EOF 或错误
 *         -1 = 输入过长（该行被丢弃）
 *         -2 = 用户按下了 ESC 键（缓冲区被置空）
 */
int InputHelper_read_line(FILE *input, char *buffer, size_t capacity);

/** ESC 取消时返回的标识消息 */
#define INPUT_HELPER_ESC_MESSAGE "input cancelled by user"

/**
 * @brief 检查 Result 消息是否为 ESC 取消标识
 *
 * @param message 待检查的消息字符串
 * @return 是 ESC 取消消息返回 1，否则返回 0
 */
int InputHelper_is_esc_cancel(const char *message);

#endif
