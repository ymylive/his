/**
 * @file RepositoryUtils.h
 * @brief 仓储层通用工具函数头文件
 *
 * 提供仓储层中通用的文本处理工具函数，包括：
 * - 行尾符剥离
 * - 空行检测
 * - 字段安全性校验（防止管道符等保留字符注入）
 * - 字段数量验证
 * - 基于管道符 '|' 的行拆分
 *
 * 这些工具函数被各个具体的 Repository 模块共同使用，
 * 用于文本文件存储格式的解析与校验。
 */

#ifndef HIS_REPOSITORY_REPOSITORY_UTILS_H
#define HIS_REPOSITORY_REPOSITORY_UTILS_H

#include <stddef.h>

#include "common/Result.h"

/** 管道符分割时支持的最大字段数 */
#define REPOSITORY_UTILS_MAX_FIELDS 32

/**
 * @brief 剥离字符串末尾的换行符（\n 和 \r）
 * @param line 待处理的字符串（原地修改）
 */
void RepositoryUtils_strip_line_endings(char *line);

/**
 * @brief 判断一行是否为空行（仅包含空白字符或为空指针）
 * @param line 待检测的字符串
 * @return 1 表示空行，0 表示非空行
 */
int RepositoryUtils_is_blank_line(const char *line);

/**
 * @brief 检查文本是否为安全的字段内容
 *
 * 安全的字段内容不能包含管道符 '|'、回车符 '\r' 或换行符 '\n'，
 * 因为这些字符在文件存储格式中有特殊含义。
 *
 * @param text 待检查的文本
 * @return 1 表示安全，0 表示不安全或为空指针
 */
int RepositoryUtils_is_safe_field_text(const char *text);

/**
 * @brief 校验实际字段数量是否等于期望值
 * @param actual_count   实际字段数量
 * @param expected_count 期望字段数量
 * @return 成功时返回 success 结果；字段数不匹配时返回 failure 结果
 */
Result RepositoryUtils_validate_field_count(size_t actual_count, size_t expected_count);

/**
 * @brief 将一行文本按管道符 '|' 拆分为多个字段
 *
 * 该函数会原地修改 line，将管道符替换为 '\0'，
 * 然后将各字段的起始指针存入 fields 数组中。
 *
 * @param line            待拆分的行文本（会被原地修改）
 * @param fields          输出参数，存放各字段指针的数组
 * @param max_fields      fields 数组的最大容量
 * @param out_field_count 输出参数，实际拆分得到的字段数量
 * @return 成功时返回 success 结果；参数无效或字段过多时返回 failure 结果
 */
Result RepositoryUtils_split_pipe_line(
    char *line,
    char **fields,
    size_t max_fields,
    size_t *out_field_count
);

#endif
