/**
 * @file RepositoryUtils.h
 * @brief 仓储层通用工具函数头文件
 *
 * 提供仓储层中通用的文本处理工具函数，包括：
 * - 行尾符剥离
 * - 空行检测
 * - 字段安全性校验（防止换行符等保留字符注入）
 * - 字段分隔符转义与反转义（支持字段内包含管道符）
 * - 字段数量验证
 * - 基于管道符 '|' 的行拆分（支持转义的管道符）
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
 * @brief 判断文本是否为非空字符串
 *
 * 检查指针非空且首字符不为 '\0'，用于替代各仓储文件中
 * 重复实现的 _has_text() 内部函数。
 *
 * @param text 待检查的字符串指针
 * @return 1 表示有内容，0 表示为空指针或空串
 */
int RepositoryUtils_has_text(const char *text);

/**
 * @brief 安全复制字符串到目标缓冲区
 *
 * 等价于各仓储文件中重复实现的 _copy_text()/_copy_string() 内部函数。
 * 始终以 '\0' 结尾。源为空指针时，目标写入空字符串。
 *
 * @param destination 目标缓冲区
 * @param capacity    目标缓冲区容量
 * @param source      源字符串，可为 NULL
 */
void RepositoryUtils_copy_text(char *destination, size_t capacity, const char *source);

/**
 * @brief 将文本解析为整数
 *
 * 替代各仓储文件中重复实现的 _parse_int() 内部函数。
 * 要求整行正是一个整数（前后不允许额外字符），并检查 int 范围溢出。
 *
 * @param text       待解析的文本
 * @param out_value  输出参数，解析得到的整数
 * @param field_name 字段名，用于构造可读的错误消息
 * @return 成功返回 success；为空、格式不合法或溢出时返回 failure
 */
Result RepositoryUtils_parse_int(const char *text, int *out_value, const char *field_name);

/**
 * @brief 将文本解析为双精度浮点数
 *
 * 替代各仓储文件中重复实现的 _parse_double() 内部函数。
 * 要求整行正是一个浮点数（前后不允许额外字符）。
 *
 * @param text       待解析的文本
 * @param out_value  输出参数，解析得到的浮点数
 * @param field_name 字段名，用于构造可读的错误消息
 * @return 成功返回 success；为空或格式不合法时返回 failure
 */
Result RepositoryUtils_parse_double(const char *text, double *out_value, const char *field_name);

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
 * 安全的字段内容不能包含回车符 '\r' 或换行符 '\n'，
 * 因为这些字符在文件存储格式中用作行分隔符。
 * 管道符 '|' 允许出现，会在写入时被转义。
 *
 * @param text 待检查的文本
 * @return 1 表示安全，0 表示不安全或为空指针
 */
int RepositoryUtils_is_safe_field_text(const char *text);

/**
 * @brief 转义字段文本中的分隔符，使其可安全存储
 *
 * 转义规则：'\\' → '\\\\'，'|' → '\\|'，其余字符不变。
 * 始终以 '\\0' 结尾。
 *
 * @param dest          目标缓冲区
 * @param dest_capacity 目标缓冲区容量
 * @param src           源字符串
 * @return 写入的字节数（不含终止符）
 */
size_t RepositoryUtils_escape_field(char *dest, size_t dest_capacity, const char *src);

/**
 * @brief 反转义已存储的字段文本，还原为原始内容
 *
 * 反转义规则：'\\\\' → '\\'，'\\|' → '|'，'\\x'（其他）→ 'x'，其余不变。
 * 始终以 '\\0' 结尾。
 *
 * @param dest          目标缓冲区
 * @param dest_capacity 目标缓冲区容量
 * @param src           源字符串
 * @return 写入的字节数（不含终止符）
 */
size_t RepositoryUtils_unescape_field(char *dest, size_t dest_capacity, const char *src);

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
