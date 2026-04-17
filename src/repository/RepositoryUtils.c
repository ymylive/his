/**
 * @file RepositoryUtils.c
 * @brief 仓储层通用工具函数实现
 *
 * 实现仓储层中通用的文本处理工具函数，包括行尾符剥离、空行检测、
 * 字段安全性校验、分隔符转义与反转义、字段数量验证以及管道符行拆分等功能。
 */

#include "repository/RepositoryUtils.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief 判断文本是否为非空字符串
 * @param text 待检查的字符串指针
 * @return 1 表示有内容，0 表示为空指针或空串
 */
int RepositoryUtils_has_text(const char *text) {
    return text != 0 && text[0] != '\0';
}

/**
 * @brief 安全复制字符串到目标缓冲区
 *
 * 始终保证目标以 '\0' 结尾。source 为空指针时目标写入空字符串。
 */
void RepositoryUtils_copy_text(char *destination, size_t capacity, const char *source) {
    if (destination == 0 || capacity == 0) {
        return;
    }
    if (source == 0) {
        destination[0] = '\0';
        return;
    }
    strncpy(destination, source, capacity - 1);
    destination[capacity - 1] = '\0';
}

/**
 * @brief 将文本解析为整数（严格模式）
 *
 * 要求文本整体就是一个可转换的整数，不允许前后有额外字符。
 * 同时检查 INT 溢出。错误消息以 field_name 作前缀，便于定位。
 */
Result RepositoryUtils_parse_int(const char *text, int *out_value, const char *field_name) {
    char message[RESULT_MESSAGE_CAPACITY];
    const char *name = (field_name != 0 && field_name[0] != '\0') ? field_name : "integer field";
    char *end_pointer = 0;
    long value = 0;

    if (text == 0 || out_value == 0 || text[0] == '\0') {
        snprintf(message, sizeof(message), "%s must be integer", name);
        return Result_make_failure(message);
    }

    errno = 0;
    value = strtol(text, &end_pointer, 10);
    if (end_pointer == text || end_pointer == 0 || *end_pointer != '\0') {
        snprintf(message, sizeof(message), "%s must be integer", name);
        return Result_make_failure(message);
    }
    if (errno == ERANGE || value < INT_MIN || value > INT_MAX) {
        snprintf(message, sizeof(message), "%s is out of range", name);
        return Result_make_failure(message);
    }

    *out_value = (int)value;
    return Result_make_success("integer parsed");
}

/**
 * @brief 将文本解析为双精度浮点数（严格模式）
 *
 * 要求文本整体就是一个可转换的浮点数，不允许前后有额外字符。
 * 错误消息以 field_name 作前缀，便于定位。
 */
Result RepositoryUtils_parse_double(const char *text, double *out_value, const char *field_name) {
    char message[RESULT_MESSAGE_CAPACITY];
    const char *name = (field_name != 0 && field_name[0] != '\0') ? field_name : "number field";
    char *end_pointer = 0;
    double value = 0.0;

    if (text == 0 || out_value == 0 || text[0] == '\0') {
        snprintf(message, sizeof(message), "%s must be number", name);
        return Result_make_failure(message);
    }

    errno = 0;
    value = strtod(text, &end_pointer);
    if (end_pointer == text || end_pointer == 0 || *end_pointer != '\0') {
        snprintf(message, sizeof(message), "%s must be number", name);
        return Result_make_failure(message);
    }
    if (errno == ERANGE) {
        snprintf(message, sizeof(message), "%s is out of range", name);
        return Result_make_failure(message);
    }

    *out_value = value;
    return Result_make_success("number parsed");
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
 * 字段中不能包含 '\r' 或 '\n'（行分隔符）。
 * 管道符 '|' 允许出现，会在写入时通过转义处理。
 *
 * @param text 待检查的文本
 * @return 1 表示安全，0 表示包含保留字符或为空指针
 */
int RepositoryUtils_is_safe_field_text(const char *text) {
    if (text == 0) {
        return 0;
    }

    /* 逐字符扫描，检查是否包含行分隔符 */
    while (*text != '\0') {
        if (*text == '\r' || *text == '\n') {
            return 0; /* 包含行分隔符，不安全 */
        }

        text++;
    }

    return 1; /* 未发现保留字符，安全 */
}

/**
 * @brief 转义字段文本中的分隔符
 *
 * '\\' → '\\\\'，'|' → '\\|'，其余不变。始终以 '\\0' 结尾。
 *
 * @param dest          目标缓冲区
 * @param dest_capacity 目标缓冲区容量
 * @param src           源字符串
 * @return 写入的字节数（不含终止符）
 */
size_t RepositoryUtils_escape_field(char *dest, size_t dest_capacity, const char *src) {
    size_t written = 0;

    if (dest == 0 || dest_capacity == 0) {
        return 0;
    }

    if (src == 0) {
        dest[0] = '\0';
        return 0;
    }

    while (*src != '\0' && written + 1 < dest_capacity) {
        if (*src == '\\' || *src == '|') {
            /* 需要2个字节存放转义序列，再加1个字节留给终止符 */
            if (written + 2 >= dest_capacity) {
                break;
            }
            dest[written++] = '\\';
            dest[written++] = *src;
        } else {
            dest[written++] = *src;
        }
        src++;
    }

    dest[written] = '\0';
    return written;
}

/**
 * @brief 反转义已存储的字段文本
 *
 * '\\\\' → '\\'，'\\|' → '|'，'\\x'（其他）→ 'x'，其余不变。
 *
 * @param dest          目标缓冲区
 * @param dest_capacity 目标缓冲区容量
 * @param src           源字符串
 * @return 写入的字节数（不含终止符）
 */
size_t RepositoryUtils_unescape_field(char *dest, size_t dest_capacity, const char *src) {
    size_t written = 0;

    if (dest == 0 || dest_capacity == 0) {
        return 0;
    }

    if (src == 0) {
        dest[0] = '\0';
        return 0;
    }

    while (*src != '\0' && written + 1 < dest_capacity) {
        if (*src == '\\' && *(src + 1) != '\0') {
            src++; /* skip backslash, take next char literally */
            dest[written++] = *src;
        } else {
            dest[written++] = *src;
        }
        src++;
    }

    dest[written] = '\0';
    return written;
}

/**
 * @brief 原地反转义字段内容（内部辅助函数）
 *
 * 由于反转义后长度总是 <= 原始长度，可以安全地原地操作。
 *
 * @param field 待反转义的字段字符串（原地修改）
 */
static void RepositoryUtils_unescape_field_inplace(char *field) {
    char *read = field;
    char *write = field;

    if (field == 0) {
        return;
    }

    while (*read != '\0') {
        if (*read == '\\' && *(read + 1) != '\0') {
            read++; /* skip backslash, take next char literally */
            *write = *read;
        } else {
            *write = *read;
        }
        read++;
        write++;
    }

    *write = '\0';
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
 * 原地修改 line，将未转义的管道符替换为 '\0'，各字段起始地址存入 fields 数组。
 * 转义的管道符 '\\|' 不作为分隔符。拆分完成后自动对每个字段进行原地反转义。
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
    size_t i = 0;
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

    /* 遍历整行，遇到未转义的管道符时拆分 */
    while (*cursor != '\0') {
        if (*cursor == '\\' && *(cursor + 1) != '\0') {
            cursor += 2; /* 跳过转义字符 */
            continue;
        }

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

    /* 拆分完成后，对每个字段进行原地反转义 */
    for (i = 0; i < field_count; i++) {
        RepositoryUtils_unescape_field_inplace(fields[i]);
    }

    *out_field_count = field_count;
    return Result_make_success("split ok");
}
