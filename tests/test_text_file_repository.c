/**
 * @file test_text_file_repository.c
 * @brief TextFileRepository（文本文件仓储基础层）的单元测试文件
 *
 * 本文件测试文本文件仓储基础层的核心功能，包括：
 * - 初始化与路径校验
 * - 确保文件存在（自动创建目录和文件）
 * - append_line 追加数据行
 * - for_each_data_line 遍历数据行（跳过表头）
 * - for_each_non_empty_line 遍历所有非空行
 * - 超长行的拒绝处理
 * - 空文件遍历
 * - save_file 全量保存与原子替换
 * - 空指针和无效参数的错误处理
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "repository/TextFileRepository.h"

/**
 * @brief 辅助函数：构建测试用的临时文件路径并删除已存在的文件
 */
static void make_test_path(char *buffer, size_t buffer_size, const char *test_name) {
    assert(buffer != 0);
    assert(test_name != 0);

    snprintf(
        buffer,
        buffer_size,
        "build/test_text_file_repository_data/%s_%ld.txt",
        test_name,
        (long)time(0)
    );
    remove(buffer); /* 清理旧文件 */
}

/**
 * @brief 行遍历回调上下文：计数器和行内容收集
 */
typedef struct LineCollector {
    int count;                                          /* 已遍历的行数 */
    char lines[16][TEXT_FILE_REPOSITORY_LINE_CAPACITY]; /* 收集的行内容 */
} LineCollector;

/**
 * @brief 行遍历回调：收集行内容并计数
 */
static Result collect_line(const char *line, void *context) {
    LineCollector *collector = (LineCollector *)context;

    assert(collector != 0);
    if (collector->count < 16) {
        strncpy(
            collector->lines[collector->count],
            line,
            TEXT_FILE_REPOSITORY_LINE_CAPACITY - 1
        );
        collector->lines[collector->count][TEXT_FILE_REPOSITORY_LINE_CAPACITY - 1] = '\0';
    }

    collector->count++;
    return Result_make_success("ok");
}

/**
 * @brief 行遍历回调：在第二行时返回失败（用于测试回调终止遍历）
 */
static Result fail_on_second_line(const char *line, void *context) {
    int *count = (int *)context;

    (void)line;
    (*count)++;
    if (*count >= 2) {
        return Result_make_failure("intentional failure on second line");
    }

    return Result_make_success("ok");
}

/**
 * @brief 测试初始化：正常路径
 *
 * 验证 TextFileRepository_init 能正确保存路径并标记为未初始化。
 */
static void test_init_with_valid_path(void) {
    TextFileRepository repository;
    Result result;

    memset(&repository, 0, sizeof(repository));
    result = TextFileRepository_init(&repository, "build/test_text_file_repository_data/init_test.txt");
    assert(result.success == 1);
    assert(strcmp(repository.path, "build/test_text_file_repository_data/init_test.txt") == 0);
    assert(repository.initialized == 0); /* 初始化后尚未确认文件存在 */
}

/**
 * @brief 测试初始化：空路径被拒绝
 */
static void test_init_rejects_empty_path(void) {
    TextFileRepository repository;
    Result result;

    memset(&repository, 0, sizeof(repository));
    result = TextFileRepository_init(&repository, "");
    assert(result.success == 0);
}

/**
 * @brief 测试初始化：NULL路径被拒绝
 */
static void test_init_rejects_null_path(void) {
    TextFileRepository repository;
    Result result;

    memset(&repository, 0, sizeof(repository));
    result = TextFileRepository_init(&repository, 0);
    assert(result.success == 0);
}

/**
 * @brief 测试初始化：NULL仓储指针被拒绝
 */
static void test_init_rejects_null_repository(void) {
    Result result;

    result = TextFileRepository_init(0, "some_path.txt");
    assert(result.success == 0);
}

/**
 * @brief 测试初始化：超长路径被拒绝
 */
static void test_init_rejects_overlong_path(void) {
    TextFileRepository repository;
    char overlong_path[TEXT_FILE_REPOSITORY_PATH_CAPACITY + 32];
    Result result;

    memset(overlong_path, 'a', sizeof(overlong_path) - 1);
    overlong_path[sizeof(overlong_path) - 1] = '\0';

    memset(&repository, 0, sizeof(repository));
    result = TextFileRepository_init(&repository, overlong_path);
    assert(result.success == 0);
}

/**
 * @brief 测试 ensure_file_exists 自动创建目录和文件
 */
static void test_ensure_file_exists_creates_file(void) {
    char path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    TextFileRepository repository;
    FILE *file = 0;
    Result result;

    make_test_path(path, sizeof(path), "ensure_exists");
    result = TextFileRepository_init(&repository, path);
    assert(result.success == 1);
    assert(repository.initialized == 0);

    result = TextFileRepository_ensure_file_exists(&repository);
    assert(result.success == 1);
    assert(repository.initialized == 1);

    /* 验证文件确实被创建了 */
    file = fopen(path, "r");
    assert(file != 0);
    fclose(file);
}

/**
 * @brief 测试 ensure_file_exists 对未初始化仓储的拒绝
 */
static void test_ensure_file_exists_rejects_uninitialized(void) {
    TextFileRepository repository;
    Result result;

    memset(&repository, 0, sizeof(repository));
    /* path 为空字符串 */
    result = TextFileRepository_ensure_file_exists(&repository);
    assert(result.success == 0);
}

/**
 * @brief 测试 append_line 正常追加数据行
 *
 * 追加两行后读取文件验证内容正确。
 */
static void test_append_line_adds_data(void) {
    char path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    TextFileRepository repository;
    FILE *file = 0;
    char line[TEXT_FILE_REPOSITORY_LINE_CAPACITY];
    Result result;

    make_test_path(path, sizeof(path), "append_line");
    result = TextFileRepository_init(&repository, path);
    assert(result.success == 1);

    /* 追加表头 */
    result = TextFileRepository_append_line(&repository, "id|name|age");
    assert(result.success == 1);

    /* 追加数据行 */
    result = TextFileRepository_append_line(&repository, "001|Alice|28");
    assert(result.success == 1);

    /* 追加另一条数据行 */
    result = TextFileRepository_append_line(&repository, "002|Bob|35");
    assert(result.success == 1);

    /* 验证文件内容 */
    file = fopen(path, "r");
    assert(file != 0);
    assert(fgets(line, sizeof(line), file) != 0);
    assert(strcmp(line, "id|name|age\n") == 0);
    assert(fgets(line, sizeof(line), file) != 0);
    assert(strcmp(line, "001|Alice|28\n") == 0);
    assert(fgets(line, sizeof(line), file) != 0);
    assert(strcmp(line, "002|Bob|35\n") == 0);
    assert(fgets(line, sizeof(line), file) == 0); /* 没有更多行 */
    fclose(file);
}

/**
 * @brief 测试 append_line 自动补充换行符
 *
 * 当追加的行末尾已有换行符时，不应重复添加。
 */
static void test_append_line_with_trailing_newline(void) {
    char path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    TextFileRepository repository;
    FILE *file = 0;
    char line[TEXT_FILE_REPOSITORY_LINE_CAPACITY];
    Result result;

    make_test_path(path, sizeof(path), "append_newline");
    result = TextFileRepository_init(&repository, path);
    assert(result.success == 1);

    /* 追加带换行符的行 */
    result = TextFileRepository_append_line(&repository, "data_with_newline\n");
    assert(result.success == 1);

    /* 验证不会出现双换行 */
    file = fopen(path, "r");
    assert(file != 0);
    assert(fgets(line, sizeof(line), file) != 0);
    assert(strcmp(line, "data_with_newline\n") == 0);
    assert(fgets(line, sizeof(line), file) == 0); /* 只有一行 */
    fclose(file);
}

/**
 * @brief 测试 append_line 拒绝 NULL 行
 */
static void test_append_line_rejects_null(void) {
    char path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    TextFileRepository repository;
    Result result;

    make_test_path(path, sizeof(path), "append_null");
    result = TextFileRepository_init(&repository, path);
    assert(result.success == 1);

    result = TextFileRepository_append_line(&repository, 0);
    assert(result.success == 0);
}

/**
 * @brief 测试 for_each_data_line 遍历数据行（跳过表头）
 *
 * 写入1行表头和2行数据，验证只遍历到2行数据。
 */
static void test_for_each_data_line_skips_header(void) {
    char path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    TextFileRepository repository;
    LineCollector collector;
    Result result;

    make_test_path(path, sizeof(path), "data_line");
    result = TextFileRepository_init(&repository, path);
    assert(result.success == 1);

    /* 写入表头和数据 */
    assert(TextFileRepository_append_line(&repository, "header_field1|header_field2").success == 1);
    assert(TextFileRepository_append_line(&repository, "data1|value1").success == 1);
    assert(TextFileRepository_append_line(&repository, "data2|value2").success == 1);

    /* 遍历数据行 */
    memset(&collector, 0, sizeof(collector));
    result = TextFileRepository_for_each_data_line(&repository, collect_line, &collector);
    assert(result.success == 1);
    assert(collector.count == 2); /* 应只有2行数据，表头被跳过 */
    assert(strcmp(collector.lines[0], "data1|value1") == 0);
    assert(strcmp(collector.lines[1], "data2|value2") == 0);
}

/**
 * @brief 测试 for_each_non_empty_line 遍历所有非空行（不跳过表头）
 */
static void test_for_each_non_empty_line_includes_header(void) {
    char path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    TextFileRepository repository;
    LineCollector collector;
    Result result;

    make_test_path(path, sizeof(path), "non_empty_line");
    result = TextFileRepository_init(&repository, path);
    assert(result.success == 1);

    assert(TextFileRepository_append_line(&repository, "header_line").success == 1);
    assert(TextFileRepository_append_line(&repository, "data_line_1").success == 1);
    assert(TextFileRepository_append_line(&repository, "data_line_2").success == 1);

    memset(&collector, 0, sizeof(collector));
    result = TextFileRepository_for_each_non_empty_line(&repository, collect_line, &collector);
    assert(result.success == 1);
    assert(collector.count == 3); /* 表头也被遍历，共3行 */
    assert(strcmp(collector.lines[0], "header_line") == 0);
    assert(strcmp(collector.lines[1], "data_line_1") == 0);
    assert(strcmp(collector.lines[2], "data_line_2") == 0);
}

/**
 * @brief 测试空文件遍历
 *
 * 对空文件进行遍历，应成功完成且回调不被调用。
 */
static void test_iterate_empty_file(void) {
    char path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    TextFileRepository repository;
    LineCollector collector;
    Result result;

    make_test_path(path, sizeof(path), "empty_file");
    result = TextFileRepository_init(&repository, path);
    assert(result.success == 1);

    /* 确保文件存在但为空 */
    result = TextFileRepository_ensure_file_exists(&repository);
    assert(result.success == 1);

    memset(&collector, 0, sizeof(collector));
    result = TextFileRepository_for_each_data_line(&repository, collect_line, &collector);
    assert(result.success == 1);
    assert(collector.count == 0); /* 空文件无数据行 */
}

/**
 * @brief 测试仅有表头的文件遍历
 *
 * 文件只有表头，for_each_data_line 应返回0行数据。
 */
static void test_iterate_header_only_file(void) {
    char path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    TextFileRepository repository;
    LineCollector collector;
    Result result;

    make_test_path(path, sizeof(path), "header_only");
    result = TextFileRepository_init(&repository, path);
    assert(result.success == 1);

    assert(TextFileRepository_append_line(&repository, "header_only_line").success == 1);

    memset(&collector, 0, sizeof(collector));
    result = TextFileRepository_for_each_data_line(&repository, collect_line, &collector);
    assert(result.success == 1);
    assert(collector.count == 0); /* 只有表头，无数据行 */
}

/**
 * @brief 测试超长行被拒绝
 *
 * 手动写入超过 TEXT_FILE_REPOSITORY_LINE_CAPACITY 长度的行，
 * 遍历时应返回失败。
 */
static void test_overlong_line_is_rejected(void) {
    char path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    TextFileRepository repository;
    LineCollector collector;
    FILE *file = 0;
    char overlong_line[TEXT_FILE_REPOSITORY_LINE_CAPACITY + 256];
    Result result;

    make_test_path(path, sizeof(path), "overlong_line");
    result = TextFileRepository_init(&repository, path);
    assert(result.success == 1);

    /* 手动写入超长行 */
    memset(overlong_line, 'X', sizeof(overlong_line) - 2);
    overlong_line[sizeof(overlong_line) - 2] = '\n';
    overlong_line[sizeof(overlong_line) - 1] = '\0';

    file = fopen(path, "w");
    assert(file != 0);
    fputs("header\n", file);
    fputs(overlong_line, file);
    fclose(file);
    repository.initialized = 1; /* 标记文件已存在 */

    /* 遍历时应失败 */
    memset(&collector, 0, sizeof(collector));
    result = TextFileRepository_for_each_data_line(&repository, collect_line, &collector);
    assert(result.success == 0);
}

/**
 * @brief 测试回调返回失败时终止遍历
 */
static void test_callback_failure_stops_iteration(void) {
    char path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    TextFileRepository repository;
    int count = 0;
    Result result;

    make_test_path(path, sizeof(path), "callback_fail");
    result = TextFileRepository_init(&repository, path);
    assert(result.success == 1);

    assert(TextFileRepository_append_line(&repository, "header").success == 1);
    assert(TextFileRepository_append_line(&repository, "line1").success == 1);
    assert(TextFileRepository_append_line(&repository, "line2").success == 1);
    assert(TextFileRepository_append_line(&repository, "line3").success == 1);

    result = TextFileRepository_for_each_data_line(&repository, fail_on_second_line, &count);
    assert(result.success == 0); /* 回调在第二行失败 */
    assert(count == 2);          /* 只处理了2行就终止 */
}

/**
 * @brief 测试 NULL 回调被拒绝
 */
static void test_null_handler_is_rejected(void) {
    char path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    TextFileRepository repository;
    Result result;

    make_test_path(path, sizeof(path), "null_handler");
    result = TextFileRepository_init(&repository, path);
    assert(result.success == 1);

    result = TextFileRepository_for_each_data_line(&repository, 0, 0);
    assert(result.success == 0);
}

/**
 * @brief 测试 save_file 全量保存（原子替换）
 *
 * 先追加旧数据，再用 save_file 覆盖为新内容，验证旧数据被替换。
 */
static void test_save_file_replaces_content(void) {
    char path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    TextFileRepository repository;
    FILE *file = 0;
    char line[TEXT_FILE_REPOSITORY_LINE_CAPACITY];
    Result result;

    make_test_path(path, sizeof(path), "save_file");
    result = TextFileRepository_init(&repository, path);
    assert(result.success == 1);

    /* 先追加旧数据 */
    assert(TextFileRepository_append_line(&repository, "old_header").success == 1);
    assert(TextFileRepository_append_line(&repository, "old_data_1").success == 1);
    assert(TextFileRepository_append_line(&repository, "old_data_2").success == 1);

    /* 用 save_file 全量覆盖 */
    result = TextFileRepository_save_file(&repository, "new_header\nnew_data_only\n");
    assert(result.success == 1);

    /* 验证文件内容已被完全替换 */
    file = fopen(path, "r");
    assert(file != 0);
    assert(fgets(line, sizeof(line), file) != 0);
    assert(strcmp(line, "new_header\n") == 0);
    assert(fgets(line, sizeof(line), file) != 0);
    assert(strcmp(line, "new_data_only\n") == 0);
    assert(fgets(line, sizeof(line), file) == 0); /* 旧数据已不存在 */
    fclose(file);
}

/**
 * @brief 测试 save_file 拒绝 NULL 内容
 */
static void test_save_file_rejects_null_content(void) {
    char path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    TextFileRepository repository;
    Result result;

    make_test_path(path, sizeof(path), "save_null");
    result = TextFileRepository_init(&repository, path);
    assert(result.success == 1);

    result = TextFileRepository_save_file(&repository, 0);
    assert(result.success == 0);
}

/**
 * @brief 测试 save_file 后仍可正常追加和遍历
 */
static void test_save_file_then_append_and_iterate(void) {
    char path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    TextFileRepository repository;
    LineCollector collector;
    Result result;

    make_test_path(path, sizeof(path), "save_then_append");
    result = TextFileRepository_init(&repository, path);
    assert(result.success == 1);

    /* 全量写入 */
    result = TextFileRepository_save_file(&repository, "header_line\nrow1\n");
    assert(result.success == 1);

    /* 追加新行 */
    result = TextFileRepository_append_line(&repository, "row2");
    assert(result.success == 1);

    /* 遍历数据行 */
    memset(&collector, 0, sizeof(collector));
    result = TextFileRepository_for_each_data_line(&repository, collect_line, &collector);
    assert(result.success == 1);
    assert(collector.count == 2); /* row1 和 row2 */
    assert(strcmp(collector.lines[0], "row1") == 0);
    assert(strcmp(collector.lines[1], "row2") == 0);
}

/**
 * @brief 测试管道符在行中的正常存储
 *
 * 管道符是字段分隔符，写入和读取时应原样保留。
 */
static void test_pipe_delimiter_in_line(void) {
    char path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    TextFileRepository repository;
    LineCollector collector;
    Result result;

    make_test_path(path, sizeof(path), "pipe_delim");
    result = TextFileRepository_init(&repository, path);
    assert(result.success == 1);

    assert(TextFileRepository_append_line(&repository, "id|name|age").success == 1);
    assert(TextFileRepository_append_line(&repository, "P001|Zhang|25").success == 1);
    assert(TextFileRepository_append_line(&repository, "P002|Li|30").success == 1);

    memset(&collector, 0, sizeof(collector));
    result = TextFileRepository_for_each_data_line(&repository, collect_line, &collector);
    assert(result.success == 1);
    assert(collector.count == 2);
    assert(strcmp(collector.lines[0], "P001|Zhang|25") == 0);
    assert(strcmp(collector.lines[1], "P002|Li|30") == 0);
}

/**
 * @brief 测试空行在遍历时被自动跳过
 */
static void test_blank_lines_are_skipped(void) {
    char path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    TextFileRepository repository;
    LineCollector collector;
    FILE *file = 0;
    Result result;

    make_test_path(path, sizeof(path), "blank_lines");
    result = TextFileRepository_init(&repository, path);
    assert(result.success == 1);

    /* 手动写入包含空行的文件 */
    file = fopen(path, "w");
    assert(file != 0);
    fputs("header\n", file);
    fputs("\n", file);       /* 空行 */
    fputs("data1\n", file);
    fputs("  \n", file);     /* 空白行 */
    fputs("data2\n", file);
    fclose(file);
    repository.initialized = 1;

    memset(&collector, 0, sizeof(collector));
    result = TextFileRepository_for_each_data_line(&repository, collect_line, &collector);
    assert(result.success == 1);
    assert(collector.count == 2); /* 空行被跳过，只有 data1 和 data2 */
    assert(strcmp(collector.lines[0], "data1") == 0);
    assert(strcmp(collector.lines[1], "data2") == 0);
}

/**
 * @brief 测试主函数，依次运行所有 TextFileRepository 测试用例
 */
int main(void) {
    test_init_with_valid_path();
    test_init_rejects_empty_path();
    test_init_rejects_null_path();
    test_init_rejects_null_repository();
    test_init_rejects_overlong_path();
    test_ensure_file_exists_creates_file();
    test_ensure_file_exists_rejects_uninitialized();
    test_append_line_adds_data();
    test_append_line_with_trailing_newline();
    test_append_line_rejects_null();
    test_for_each_data_line_skips_header();
    test_for_each_non_empty_line_includes_header();
    test_iterate_empty_file();
    test_iterate_header_only_file();
    test_overlong_line_is_rejected();
    test_callback_failure_stops_iteration();
    test_null_handler_is_rejected();
    test_save_file_replaces_content();
    test_save_file_rejects_null_content();
    test_save_file_then_append_and_iterate();
    test_pipe_delimiter_in_line();
    test_blank_lines_are_skipped();
    return 0;
}
