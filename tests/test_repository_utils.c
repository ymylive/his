/**
 * @file test_repository_utils.c
 * @brief 仓储工具函数（RepositoryUtils）和文本文件仓储（TextFileRepository）的单元测试文件
 *
 * 本文件测试底层仓储工具的核心功能，包括：
 * - 管道符分隔行的解析（split_pipe_line）
 * - 空列保留
 * - 字段数量校验
 * - 保留字符检测
 * - 遍历非空行（跳过空白行）
 * - 遍历数据行（跳过表头）
 * - 缺失文件的自动创建
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "repository/RepositoryUtils.h"
#include "repository/TextFileRepository.h"

/**
 * @brief 行收集器结构体，用于测试行遍历回调
 */
typedef struct LineCollector {
    size_t count;          /* 已收集的行数 */
    char lines[4][64];     /* 最多收集4行内容 */
} LineCollector;

/**
 * @brief 行收集回调函数
 *
 * 每遍历到一行就存入收集器中。
 */
static Result LineCollector_collect(const char *line, void *context) {
    LineCollector *collector = (LineCollector *)context;

    assert(collector != 0);
    assert(collector->count < 4); /* 防止溢出 */

    strcpy(collector->lines[collector->count], line);
    collector->count++;
    return Result_make_success("line captured");
}

/**
 * @brief 测试管道符分隔行的解析
 *
 * 输入 "PAT0001|Alice|28\r\n"，解析后应得到3个字段，
 * 且行尾的换行符被正确去除。
 */
static void test_split_pipe_line(void) {
    char line[] = "PAT0001|Alice|28\r\n";
    char *fields[3];
    size_t field_count = 0;
    Result result = RepositoryUtils_split_pipe_line(line, fields, 3, &field_count);

    assert(result.success == 1);
    assert(field_count == 3);                           /* 3个字段 */
    assert(strcmp(fields[0], "PAT0001") == 0);          /* 第1个字段 */
    assert(strcmp(fields[1], "Alice") == 0);             /* 第2个字段 */
    assert(strcmp(fields[2], "28") == 0);                /* 第3个字段 */
    assert(RepositoryUtils_validate_field_count(field_count, 3).success == 1); /* 字段数校验通过 */
}

/**
 * @brief 测试空列保留
 *
 * 输入 "PAT0001||\r\n"，解析后第2、3个字段应为空字符串。
 */
static void test_split_pipe_line_keeps_empty_columns(void) {
    char line[] = "PAT0001||\r\n";
    char *fields[3];
    size_t field_count = 0;
    Result result = RepositoryUtils_split_pipe_line(line, fields, 3, &field_count);

    assert(result.success == 1);
    assert(field_count == 3);
    assert(strcmp(fields[0], "PAT0001") == 0);
    assert(strcmp(fields[1], "") == 0);    /* 空列保留 */
    assert(strcmp(fields[2], "") == 0);    /* 空列保留 */
}

/**
 * @brief 测试字段数量不足时校验失败
 *
 * 输入只有2个字段，但期望3个字段，校验应失败。
 */
static void test_validate_field_count_failure(void) {
    char line[] = "PAT0001|Alice\r\n";
    char *fields[3];
    size_t field_count = 0;
    Result split_result = RepositoryUtils_split_pipe_line(line, fields, 3, &field_count);
    Result validate_result = RepositoryUtils_validate_field_count(field_count, 3);

    assert(split_result.success == 1);
    assert(field_count == 2);              /* 实际只有2个字段 */
    assert(validate_result.success == 0);  /* 期望3个字段，校验失败 */
}

/**
 * @brief 测试保留字符检测
 *
 * 换行符 '\n' 和 '\r' 是保留字符，不允许出现在字段文本中。
 * 管道符 '|' 现在允许出现（会在写入时转义）。
 */
static void test_reserved_characters_are_rejected(void) {
    assert(RepositoryUtils_is_safe_field_text("safe text") == 1);       /* 安全文本 */
    assert(RepositoryUtils_is_safe_field_text("text|with|pipes") == 1); /* 管道符现在允许 */
    assert(RepositoryUtils_is_safe_field_text("unsafe\ntext") == 0);    /* 包含换行符 */
    assert(RepositoryUtils_is_safe_field_text("unsafe\rtext") == 0);    /* 包含回车符 */
}

/**
 * @brief 测试字段转义功能
 *
 * 管道符和反斜杠应被正确转义。
 */
static void test_escape_field(void) {
    char dest[128];
    size_t len;

    /* 普通文本不变 */
    len = RepositoryUtils_escape_field(dest, sizeof(dest), "hello");
    assert(len == 5);
    assert(strcmp(dest, "hello") == 0);

    /* 管道符被转义 */
    len = RepositoryUtils_escape_field(dest, sizeof(dest), "a|b|c");
    assert(strcmp(dest, "a\\|b\\|c") == 0);
    assert(len == 7);

    /* 反斜杠被转义 */
    len = RepositoryUtils_escape_field(dest, sizeof(dest), "a\\b");
    assert(strcmp(dest, "a\\\\b") == 0);
    assert(len == 4);

    /* 混合转义 */
    len = RepositoryUtils_escape_field(dest, sizeof(dest), "a|b\\c");
    assert(strcmp(dest, "a\\|b\\\\c") == 0);
    assert(len == 7);

    /* 空字符串 */
    len = RepositoryUtils_escape_field(dest, sizeof(dest), "");
    assert(len == 0);
    assert(dest[0] == '\0');
}

/**
 * @brief 测试字段反转义功能
 */
static void test_unescape_field(void) {
    char dest[128];
    size_t len;

    /* 普通文本不变 */
    len = RepositoryUtils_unescape_field(dest, sizeof(dest), "hello");
    assert(len == 5);
    assert(strcmp(dest, "hello") == 0);

    /* 转义管道符还原 */
    len = RepositoryUtils_unescape_field(dest, sizeof(dest), "a\\|b\\|c");
    assert(strcmp(dest, "a|b|c") == 0);
    assert(len == 5);

    /* 转义反斜杠还原 */
    len = RepositoryUtils_unescape_field(dest, sizeof(dest), "a\\\\b");
    assert(strcmp(dest, "a\\b") == 0);
    assert(len == 3);

    /* 混合还原 */
    len = RepositoryUtils_unescape_field(dest, sizeof(dest), "a\\|b\\\\c");
    assert(strcmp(dest, "a|b\\c") == 0);
    assert(len == 5);
}

/**
 * @brief 测试含转义管道符的行拆分
 *
 * 转义的管道符不应被当作分隔符。
 */
static void test_split_pipe_line_with_escaped_pipes(void) {
    char line[] = "field1|value\\|with\\|pipes|field3\r\n";
    char *fields[3];
    size_t field_count = 0;
    Result result = RepositoryUtils_split_pipe_line(line, fields, 3, &field_count);

    assert(result.success == 1);
    assert(field_count == 3);
    assert(strcmp(fields[0], "field1") == 0);
    assert(strcmp(fields[1], "value|with|pipes") == 0);  /* 反转义后还原 */
    assert(strcmp(fields[2], "field3") == 0);
}

/**
 * @brief 测试含转义反斜杠的行拆分
 */
static void test_split_pipe_line_with_escaped_backslash(void) {
    char line[] = "a\\\\b|c\\\\d\n";
    char *fields[2];
    size_t field_count = 0;
    Result result = RepositoryUtils_split_pipe_line(line, fields, 2, &field_count);

    assert(result.success == 1);
    assert(field_count == 2);
    assert(strcmp(fields[0], "a\\b") == 0);  /* 反转义后 \\\\ → \\ */
    assert(strcmp(fields[1], "c\\d") == 0);
}

/**
 * @brief 测试遍历非空行（跳过空白行）
 *
 * 文件包含空行和空白行，遍历时只返回有实际内容的行。
 */
static void test_for_each_non_empty_line_ignores_blank_lines(void) {
    const char *path = "build/test_repository_utils_data/records.txt";
    TextFileRepository repository;
    FILE *file = 0;
    LineCollector collector;
    Result result = TextFileRepository_init(&repository, path);

    assert(result.success == 1);
    result = TextFileRepository_ensure_file_exists(&repository);
    assert(result.success == 1);

    /* 写入测试数据：空行、数据行、空白行、数据行 */
    file = fopen(path, "w");
    assert(file != 0);
    fputs("\n", file);                      /* 空行 */
    fputs("PAT0001|Alice|28\n", file);      /* 数据行1 */
    fputs("   \r\n", file);                 /* 空白行 */
    fputs("PAT0002|Bob|35\r\n", file);      /* 数据行2 */
    fclose(file);

    /* 遍历非空行 */
    memset(&collector, 0, sizeof(collector));
    result = TextFileRepository_for_each_non_empty_line(
        &repository,
        LineCollector_collect,
        &collector
    );

    assert(result.success == 1);
    assert(collector.count == 2);                                       /* 只有2行有内容 */
    assert(strcmp(collector.lines[0], "PAT0001|Alice|28") == 0);       /* 行尾换行已去除 */
    assert(strcmp(collector.lines[1], "PAT0002|Bob|35") == 0);
}

/**
 * @brief 测试遍历数据行（跳过表头）
 *
 * 文件第一行为表头，遍历数据行时应自动跳过表头。
 */
static void test_for_each_data_line_skips_header(void) {
    const char *path = "build/test_repository_utils_data/header_records.txt";
    TextFileRepository repository;
    FILE *file = 0;
    LineCollector collector;
    Result result = TextFileRepository_init(&repository, path);

    assert(result.success == 1);
    result = TextFileRepository_ensure_file_exists(&repository);
    assert(result.success == 1);

    /* 写入测试数据：表头 + 2行数据 */
    file = fopen(path, "w");
    assert(file != 0);
    fputs("patient_id|name|age\n", file);    /* 表头 */
    fputs("PAT0001|Alice|28\n", file);       /* 数据行1 */
    fputs("PAT0002|Bob|35\n", file);         /* 数据行2 */
    fclose(file);

    /* 遍历数据行（跳过表头） */
    memset(&collector, 0, sizeof(collector));
    result = TextFileRepository_for_each_data_line(
        &repository,
        LineCollector_collect,
        &collector
    );

    assert(result.success == 1);
    assert(collector.count == 2);                                       /* 表头不计入 */
    assert(strcmp(collector.lines[0], "PAT0001|Alice|28") == 0);
    assert(strcmp(collector.lines[1], "PAT0002|Bob|35") == 0);
}

/**
 * @brief 测试缺失文件的自动创建
 *
 * 当文件路径不存在时，ensure_file_exists 应自动创建目录和文件。
 */
static void test_missing_file_is_created_safely(void) {
    char path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    TextFileRepository repository;
    FILE *file = 0;
    Result result;

    /* 使用时间戳生成唯一路径，确保文件不存在 */
    snprintf(
        path,
        sizeof(path),
        "build/test_repository_utils_data/run_%ld/missing.txt",
        (long)time(0)
    );

    result = TextFileRepository_init(&repository, path);
    assert(result.success == 1);

    /* 自动创建文件 */
    result = TextFileRepository_ensure_file_exists(&repository);
    assert(result.success == 1);

    /* 验证文件已存在 */
    file = fopen(path, "r");
    assert(file != 0);
    fclose(file);
}

/**
 * @brief 测试主函数，依次运行所有仓储工具测试用例
 */
int main(void) {
    test_split_pipe_line();
    test_split_pipe_line_keeps_empty_columns();
    test_validate_field_count_failure();
    test_reserved_characters_are_rejected();
    test_escape_field();
    test_unescape_field();
    test_split_pipe_line_with_escaped_pipes();
    test_split_pipe_line_with_escaped_backslash();
    test_for_each_non_empty_line_ignores_blank_lines();
    test_for_each_data_line_skips_header();
    test_missing_file_is_created_safely();
    return 0;
}
