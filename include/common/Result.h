/**
 * @file Result.h
 * @brief 操作结果模块 - 封装操作的成功/失败状态及提示信息
 *
 * 本模块定义了通用的操作结果结构体 Result，用于在函数间
 * 传递操作是否成功以及相关的描述性消息。
 */

#ifndef HIS_COMMON_RESULT_H
#define HIS_COMMON_RESULT_H

/** 结果消息缓冲区的最大容量（字节数，含末尾 '\0'） */
#define RESULT_MESSAGE_CAPACITY 128

/**
 * @brief 操作结果结构体
 *
 * 包含操作的成功/失败标志和描述性消息。
 */
typedef struct Result {
    int success;                            /**< 成功标志：1 表示成功，0 表示失败 */
    char message[RESULT_MESSAGE_CAPACITY];  /**< 描述性消息（成功提示或错误原因） */
} Result;

/**
 * @brief 创建一个表示成功的 Result
 *
 * @param message 成功提示消息，可以为 NULL
 * @return 填充好的 Result 结构体（success = 1）
 */
Result Result_make_success(const char *message);

/**
 * @brief 创建一个表示失败的 Result
 *
 * @param message 错误原因描述，可以为 NULL
 * @return 填充好的 Result 结构体（success = 0）
 */
Result Result_make_failure(const char *message);

#endif
