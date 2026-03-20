#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "repository/RepositoryUtils.h"
#include "repository/TextFileRepository.h"

typedef struct LineCollector {
    size_t count;
    char lines[4][64];
} LineCollector;

static Result LineCollector_collect(const char *line, void *context) {
    LineCollector *collector = (LineCollector *)context;

    assert(collector != 0);
    assert(collector->count < 4);

    strcpy(collector->lines[collector->count], line);
    collector->count++;
    return Result_make_success("line captured");
}

static void test_split_pipe_line(void) {
    char line[] = "PAT0001|Alice|28\r\n";
    char *fields[3];
    size_t field_count = 0;
    Result result = RepositoryUtils_split_pipe_line(line, fields, 3, &field_count);

    assert(result.success == 1);
    assert(field_count == 3);
    assert(strcmp(fields[0], "PAT0001") == 0);
    assert(strcmp(fields[1], "Alice") == 0);
    assert(strcmp(fields[2], "28") == 0);
    assert(RepositoryUtils_validate_field_count(field_count, 3).success == 1);
}

static void test_split_pipe_line_keeps_empty_columns(void) {
    char line[] = "PAT0001||\r\n";
    char *fields[3];
    size_t field_count = 0;
    Result result = RepositoryUtils_split_pipe_line(line, fields, 3, &field_count);

    assert(result.success == 1);
    assert(field_count == 3);
    assert(strcmp(fields[0], "PAT0001") == 0);
    assert(strcmp(fields[1], "") == 0);
    assert(strcmp(fields[2], "") == 0);
}

static void test_validate_field_count_failure(void) {
    char line[] = "PAT0001|Alice\r\n";
    char *fields[3];
    size_t field_count = 0;
    Result split_result = RepositoryUtils_split_pipe_line(line, fields, 3, &field_count);
    Result validate_result = RepositoryUtils_validate_field_count(field_count, 3);

    assert(split_result.success == 1);
    assert(field_count == 2);
    assert(validate_result.success == 0);
}

static void test_reserved_characters_are_rejected(void) {
    assert(RepositoryUtils_is_safe_field_text("safe text") == 1);
    assert(RepositoryUtils_is_safe_field_text("unsafe|text") == 0);
    assert(RepositoryUtils_is_safe_field_text("unsafe\ntext") == 0);
}

static void test_for_each_non_empty_line_ignores_blank_lines(void) {
    const char *path = "build/test_repository_utils_data/records.txt";
    TextFileRepository repository;
    FILE *file = 0;
    LineCollector collector;
    Result result = TextFileRepository_init(&repository, path);

    assert(result.success == 1);
    result = TextFileRepository_ensure_file_exists(&repository);
    assert(result.success == 1);

    file = fopen(path, "w");
    assert(file != 0);
    fputs("\n", file);
    fputs("PAT0001|Alice|28\n", file);
    fputs("   \r\n", file);
    fputs("PAT0002|Bob|35\r\n", file);
    fclose(file);

    memset(&collector, 0, sizeof(collector));
    result = TextFileRepository_for_each_non_empty_line(
        &repository,
        LineCollector_collect,
        &collector
    );

    assert(result.success == 1);
    assert(collector.count == 2);
    assert(strcmp(collector.lines[0], "PAT0001|Alice|28") == 0);
    assert(strcmp(collector.lines[1], "PAT0002|Bob|35") == 0);
}

static void test_for_each_data_line_skips_header(void) {
    const char *path = "build/test_repository_utils_data/header_records.txt";
    TextFileRepository repository;
    FILE *file = 0;
    LineCollector collector;
    Result result = TextFileRepository_init(&repository, path);

    assert(result.success == 1);
    result = TextFileRepository_ensure_file_exists(&repository);
    assert(result.success == 1);

    file = fopen(path, "w");
    assert(file != 0);
    fputs("patient_id|name|age\n", file);
    fputs("PAT0001|Alice|28\n", file);
    fputs("PAT0002|Bob|35\n", file);
    fclose(file);

    memset(&collector, 0, sizeof(collector));
    result = TextFileRepository_for_each_data_line(
        &repository,
        LineCollector_collect,
        &collector
    );

    assert(result.success == 1);
    assert(collector.count == 2);
    assert(strcmp(collector.lines[0], "PAT0001|Alice|28") == 0);
    assert(strcmp(collector.lines[1], "PAT0002|Bob|35") == 0);
}

static void test_missing_file_is_created_safely(void) {
    char path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    TextFileRepository repository;
    FILE *file = 0;
    Result result;

    snprintf(
        path,
        sizeof(path),
        "build/test_repository_utils_data/run_%ld/missing.txt",
        (long)time(0)
    );

    result = TextFileRepository_init(&repository, path);
    assert(result.success == 1);

    result = TextFileRepository_ensure_file_exists(&repository);
    assert(result.success == 1);

    file = fopen(path, "r");
    assert(file != 0);
    fclose(file);
}

int main(void) {
    test_split_pipe_line();
    test_split_pipe_line_keeps_empty_columns();
    test_validate_field_count_failure();
    test_reserved_characters_are_rejected();
    test_for_each_non_empty_line_ignores_blank_lines();
    test_for_each_data_line_skips_header();
    test_missing_file_is_created_safely();
    return 0;
}
