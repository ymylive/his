/**
 * @file RepositoryUtils.c
 * @brief 仓储层通用工具函数实现
 *
 * 实现仓储层中通用的文本处理工具函数，包括行尾符剥离、空行检测、
 * 字段安全性校验、字段数量验证以及管道符行拆分等功能。
 */

#include "repository/RepositoryUtils.h"

#include <ctype.h>
#include <stdio.h>

/**
 * @brief 判断文本是否为非空字符串
 * @param text 待检查的字符串指针
 * @return 1 表示有内容，0 表示为空指针或空串
 */
int RepositoryUtils_has_text(const char *text) {
    return text != 0 && text[0] != '\0';
}

/**
 * @brief 剥离字符串末尾的换行符（\n 和 \r）
 * @param line 待处理的字符串（原地修改）
 */
void RepositoryUtils_strip_line_endings(char *line) {
    size_t length = 0;

    /* 空指针检查 */
    if (line == 0) {
        return;
    }

    /* 手动计算字符串长度 */
    while (line[length] != '\0') {
        length++;
    }

    /* 从末尾逐个去除 \n 和 \r 字符 */
    while (length > 0 && (line[length - 1] == '\n' || line[length - 1] == '\r')) {
        line[length - 1] = '\0';
        length--;
    }
}

/**
 * @brief 判断一行是否为空行
 * @param line 待检测的字符串
 * @return 1 表示空行（仅空白字符或空指针），0 表示非空行
 */
int RepositoryUtils_is_blank_line(const char *line) {
    if (line == 0) {
        return 1;
    }

    /* 逐字符检查是否全为空白字符 */
    while (*line != '\0') {
        if (!isspace((unsigned char)*line)) {
            return 0; /* 发现非空白字符，不是空行 */
        }

        line++;
    }

    return 1; /* 全部为空白字符或空串 */
}

/**
 * @brief 检查文本是否为安全的字段内容
 *
 * 字段中不能包含管道符 '|'（字段分隔符）、'\r' 或 '\n'（行分隔符）。
 *
 * @param text 待检查的文本
 * @return 1 表示安全，0 表示包含保留字符或为空指针
 */
int RepositoryUtils_is_safe_field_text(const char *text) {
    if (text == 0) {
        return 0;
    }

    /* 逐字符扫描，检查是否包含保留字符 */
    while (*text != '\0') {
        if (*text == '|' || *text == '\r' || *text == '\n') {
            return 0; /* 包含保留字符，不安全 */
        }

        text++;
    }

    return 1; /* 未发现保留字符，安全 */
}

/**
 * @brief 校验实际字段数量是否等于期望值
 * @param actual_count   实际字段数量
 * @param expected_count 期望字段数量
 * @return 匹配时返回 success；不匹配时返回带有详细信息的 failure
 */
Result RepositoryUtils_validate_field_count(size_t actual_count, size_t expected_count) {
    char message[RESULT_MESSAGE_CAPACITY];

    if (actual_count == expected_count) {
        return Result_make_success("field count ok");
    }

    /* 格式化错误信息，包含期望值和实际值 */
    snprintf(
        message,
        sizeof(message),
        "expected %u fields, got %u",
        (unsigned int)expected_count,
        (unsigned int)actual_count
    );
    return Result_make_failure(message);
}

/**
 * @brief 将一行文本按管道符 '|' 拆分为多个字段
 *
 * 原地修改 line，将管道符替换为 '\0'，各字段起始地址存入 fields 数组。
 *
 * @param line            待拆分的行文本（原地修改）
 * @param fields          输出参数，各字段指针数组
 * @param max_fields      fields 数组的最大容量
 * @param out_field_count 输出参数，实际拆分的字段数
 * @return 成功返回 success；参数无效或字段过多时返回 failure
 */
Result RepositoryUtils_split_pipe_line(
    char *line,
    char **fields,
    size_t max_fields,
    size_t *out_field_count
) {
    size_t field_count = 1; /* 至少有一个字段（第一个字段在第一个 '|' 之前） */
    char *cursor = 0;

    /* 安全初始化输出参数 */
    if (out_field_count != 0) {
        *out_field_count = 0;
    }

    /* 参数合法性校验 */
    if (line == 0 || fields == 0 || out_field_count == 0 || max_fields == 0) {
        return Result_make_failure("invalid split arguments");
    }

    /* 先去除行尾换行符 */
    RepositoryUtils_strip_line_endings(line);

    /* 第一个字段从行首开始 */
    fields[0] = line;
    cursor = line;

    /* 遍历整行，遇到管道符时拆分 */
    while (*cursor != '\0') {
        if (*cursor == '|') {
            /* 字段数超过容量限制 */
            if (field_count >= max_fields) {
                return Result_make_failure("too many fields");
            }

            /* 将管道符替换为字符串终止符，下一个字段从此处之后开始 */
            *cursor = '\0';
            fields[field_count] = cursor + 1;
            field_count++;
        }

        cursor++;
    }

    *out_field_count = field_count;
    return Result_make_success("split ok");
}
