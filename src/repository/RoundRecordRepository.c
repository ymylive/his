/**
 * @file RoundRecordRepository.c
 * @brief 查房记录数据仓储层实现
 *
 * 实现查房记录（RoundRecord）数据的持久化存储操作，包括：
 * - 查房记录的序列化与反序列化
 * - 数据校验（必填字段检查）
 * - 追加、按住院记录ID筛选、全量加载、全量保存
 */

#include "repository/RoundRecordRepository.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "repository/RepositoryUtils.h"

/** 查房记录数据文件的表头行 */
static const char ROUND_RECORD_REPOSITORY_HEADER[] =
    "round_id|admission_id|doctor_id|findings|plan|rounded_at";

/** 按住院记录ID收集时使用的上下文 */
typedef struct RoundRecordCollectByAdmissionContext {
    const char *admission_id;
    LinkedList *out_records;
} RoundRecordCollectByAdmissionContext;

/** 加载所有查房记录时使用的上下文 */
typedef struct RoundRecordLoadContext {
    LinkedList *out_records;
} RoundRecordLoadContext;

/** 判断文本是否为空 */
static int RoundRecordRepository_is_empty_text(const char *text) {
    return text == 0 || text[0] == '\0';
}

/** 安全复制字符串 */
static void RoundRecordRepository_copy_text(
    char *destination, size_t capacity, const char *source
) {
    if (destination == 0 || capacity == 0) return;
    if (source == 0) { destination[0] = '\0'; return; }
    strncpy(destination, source, capacity - 1);
    destination[capacity - 1] = '\0';
}

/** 校验单个文本字段 */
static Result RoundRecordRepository_validate_text_field(
    const char *text, const char *field_name, int allow_empty
) {
    char message[RESULT_MESSAGE_CAPACITY];
    if (text == 0 || (!allow_empty && RoundRecordRepository_is_empty_text(text))) {
        snprintf(message, sizeof(message), "%s missing", field_name);
        return Result_make_failure(message);
    }
    if (!RepositoryUtils_is_safe_field_text(text)) {
        snprintf(message, sizeof(message), "%s contains reserved characters", field_name);
        return Result_make_failure(message);
    }
    return Result_make_success("round text field ok");
}

/**
 * @brief 校验查房记录的合法性
 */
static Result RoundRecordRepository_validate(const RoundRecord *record) {
    Result result;
    if (record == 0) return Result_make_failure("round record missing");

    result = RoundRecordRepository_validate_text_field(record->round_id, "round_id", 0);
    if (!result.success) return result;
    result = RoundRecordRepository_validate_text_field(record->admission_id, "admission_id", 0);
    if (!result.success) return result;
    result = RoundRecordRepository_validate_text_field(record->doctor_id, "doctor_id", 0);
    if (!result.success) return result;
    result = RoundRecordRepository_validate_text_field(record->findings, "findings", 0);
    if (!result.success) return result;
    result = RoundRecordRepository_validate_text_field(record->plan, "plan", 0);
    if (!result.success) return result;
    result = RoundRecordRepository_validate_text_field(record->rounded_at, "rounded_at", 0);
    if (!result.success) return result;

    return Result_make_success("round record valid");
}

/** 丢弃行剩余字符 */
static void RoundRecordRepository_discard_rest_of_line(FILE *file) {
    int character = 0;
    while ((character = fgetc(file)) != EOF) {
        if (character == '\n') return;
    }
}

/** 写入表头（覆盖） */
static Result RoundRecordRepository_write_header(const RoundRecordRepository *repository) {
    FILE *file = fopen(repository->storage.path, "w");
    if (file == 0) return Result_make_failure("failed to rewrite round storage");

    if (fputs(ROUND_RECORD_REPOSITORY_HEADER, file) == EOF || fputc('\n', file) == EOF) {
        fclose(file);
        return Result_make_failure("failed to write round header");
    }
    fclose(file);
    return Result_make_success("round header ready");
}

/** 将查房记录序列化为文本行 */
static Result RoundRecordRepository_serialize(
    const RoundRecord *record, char *line, size_t capacity
) {
    int written = 0;
    Result result = RoundRecordRepository_validate(record);
    if (!result.success) return result;

    written = snprintf(line, capacity, "%s|%s|%s|%s|%s|%s",
        record->round_id, record->admission_id, record->doctor_id,
        record->findings, record->plan, record->rounded_at);
    if (written < 0 || (size_t)written >= capacity) return Result_make_failure("round line too long");
    return Result_make_success("round serialized");
}

/** 将一行文本解析为查房记录结构体 */
static Result RoundRecordRepository_parse_line(const char *line, RoundRecord *out_record) {
    char buffer[TEXT_FILE_REPOSITORY_LINE_CAPACITY];
    char *fields[ROUND_RECORD_REPOSITORY_FIELD_COUNT];
    size_t field_count = 0;
    Result result;

    if (line == 0 || out_record == 0) return Result_make_failure("round parse arguments invalid");
    if (strlen(line) >= sizeof(buffer)) return Result_make_failure("round line too long");

    strncpy(buffer, line, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';
    result = RepositoryUtils_split_pipe_line(buffer, fields, ROUND_RECORD_REPOSITORY_FIELD_COUNT, &field_count);
    if (!result.success) return result;
    result = RepositoryUtils_validate_field_count(field_count, ROUND_RECORD_REPOSITORY_FIELD_COUNT);
    if (!result.success) return result;

    memset(out_record, 0, sizeof(*out_record));
    RoundRecordRepository_copy_text(out_record->round_id, sizeof(out_record->round_id), fields[0]);
    RoundRecordRepository_copy_text(out_record->admission_id, sizeof(out_record->admission_id), fields[1]);
    RoundRecordRepository_copy_text(out_record->doctor_id, sizeof(out_record->doctor_id), fields[2]);
    RoundRecordRepository_copy_text(out_record->findings, sizeof(out_record->findings), fields[3]);
    RoundRecordRepository_copy_text(out_record->plan, sizeof(out_record->plan), fields[4]);
    RoundRecordRepository_copy_text(out_record->rounded_at, sizeof(out_record->rounded_at), fields[5]);

    return RoundRecordRepository_validate(out_record);
}

/** 释放查房记录 */
static void RoundRecordRepository_free_record(void *data) { free(data); }

/** 按住院记录ID收集的行处理回调 */
static Result RoundRecordRepository_collect_by_admission_line_handler(const char *line, void *context) {
    RoundRecordCollectByAdmissionContext *collect_context =
        (RoundRecordCollectByAdmissionContext *)context;
    RoundRecord record;
    RoundRecord *copy = 0;
    Result result = RoundRecordRepository_parse_line(line, &record);
    if (!result.success) return result;

    if (strcmp(record.admission_id, collect_context->admission_id) != 0) {
        return Result_make_success("round skipped");
    }

    copy = (RoundRecord *)malloc(sizeof(*copy));
    if (copy == 0) return Result_make_failure("failed to allocate round result");
    *copy = record;
    if (!LinkedList_append(collect_context->out_records, copy)) {
        free(copy);
        return Result_make_failure("failed to append round result");
    }
    return Result_make_success("round collected");
}

/** 加载所有查房记录的行处理回调 */
static Result RoundRecordRepository_load_line_handler(const char *line, void *context) {
    RoundRecordLoadContext *load_context = (RoundRecordLoadContext *)context;
    RoundRecord record;
    RoundRecord *copy = 0;
    Result result = RoundRecordRepository_parse_line(line, &record);
    if (!result.success) return result;

    copy = (RoundRecord *)malloc(sizeof(*copy));
    if (copy == 0) return Result_make_failure("failed to allocate round result");
    *copy = record;
    if (!LinkedList_append(load_context->out_records, copy)) {
        free(copy);
        return Result_make_failure("failed to append round result");
    }
    return Result_make_success("round loaded");
}

/** 初始化查房记录仓储 */
Result RoundRecordRepository_init(RoundRecordRepository *repository, const char *path) {
    if (repository == 0) return Result_make_failure("round repository missing");
    return TextFileRepository_init(&repository->storage, path);
}

/** 确保存储文件已就绪 */
Result RoundRecordRepository_ensure_storage(const RoundRecordRepository *repository) {
    char line[TEXT_FILE_REPOSITORY_LINE_CAPACITY];
    FILE *file = 0;
    Result result;

    if (repository == 0) return Result_make_failure("round repository missing");

    result = TextFileRepository_ensure_file_exists(&repository->storage);
    if (!result.success) return result;

    file = fopen(repository->storage.path, "r");
    if (file == 0) return Result_make_failure("failed to inspect round storage");

    while (fgets(line, sizeof(line), file) != 0) {
        if (strchr(line, '\n') == 0 && !feof(file)) {
            RoundRecordRepository_discard_rest_of_line(file);
            fclose(file);
            return Result_make_failure("round header line too long");
        }
        RepositoryUtils_strip_line_endings(line);
        if (RepositoryUtils_is_blank_line(line)) continue;
        fclose(file);
        if (strcmp(line, ROUND_RECORD_REPOSITORY_HEADER) != 0) {
            return Result_make_failure("unexpected round header");
        }
        return Result_make_success("round storage ready");
    }

    fclose(file);
    return RoundRecordRepository_write_header(repository);
}

/** 追加一条查房记录 */
Result RoundRecordRepository_append(
    const RoundRecordRepository *repository, const RoundRecord *record
) {
    char line[TEXT_FILE_REPOSITORY_LINE_CAPACITY];
    Result result = RoundRecordRepository_ensure_storage(repository);
    if (!result.success) return result;
    result = RoundRecordRepository_serialize(record, line, sizeof(line));
    if (!result.success) return result;
    return TextFileRepository_append_line(&repository->storage, line);
}

/** 按住院记录ID查找查房记录列表 */
Result RoundRecordRepository_find_by_admission_id(
    const RoundRecordRepository *repository, const char *admission_id, LinkedList *out_records
) {
    RoundRecordCollectByAdmissionContext context;
    LinkedList matches;
    Result result;

    if (admission_id == 0 || admission_id[0] == '\0' || out_records == 0) {
        return Result_make_failure("round admission query arguments invalid");
    }
    if (LinkedList_count(out_records) != 0) return Result_make_failure("output round list must be empty");

    result = RoundRecordRepository_ensure_storage(repository);
    if (!result.success) return result;

    LinkedList_init(&matches);
    context.admission_id = admission_id;
    context.out_records = &matches;
    result = TextFileRepository_for_each_data_line(
        &repository->storage, RoundRecordRepository_collect_by_admission_line_handler, &context
    );
    if (!result.success) { RoundRecordRepository_clear_list(&matches); return result; }

    *out_records = matches;
    return Result_make_success("round records loaded");
}

/** 加载所有查房记录到链表 */
Result RoundRecordRepository_load_all(
    const RoundRecordRepository *repository, LinkedList *out_records
) {
    RoundRecordLoadContext context;
    Result result;

    if (repository == 0 || out_records == 0) return Result_make_failure("round load arguments invalid");
    if (LinkedList_count(out_records) != 0) return Result_make_failure("output round list must be empty");

    result = RoundRecordRepository_ensure_storage(repository);
    if (!result.success) return result;

    context.out_records = out_records;
    result = TextFileRepository_for_each_data_line(
        &repository->storage, RoundRecordRepository_load_line_handler, &context
    );
    if (!result.success) { RoundRecordRepository_clear_list(out_records); return result; }
    return Result_make_success("round records loaded");
}

/** 全量保存查房记录列表到文件（覆盖写入） */
Result RoundRecordRepository_save_all(
    const RoundRecordRepository *repository, const LinkedList *records
) {
    LinkedListNode *current = 0;
    FILE *file = 0;
    Result result;

    if (repository == 0 || records == 0) return Result_make_failure("round save arguments invalid");
    result = TextFileRepository_ensure_file_exists(&repository->storage);
    if (!result.success) return result;

    file = fopen(repository->storage.path, "w");
    if (file == 0) return Result_make_failure("failed to rewrite round storage");

    if (fputs(ROUND_RECORD_REPOSITORY_HEADER, file) == EOF || fputc('\n', file) == EOF) {
        fclose(file);
        return Result_make_failure("failed to write round header");
    }

    current = records->head;
    while (current != 0) {
        char line[TEXT_FILE_REPOSITORY_LINE_CAPACITY];
        result = RoundRecordRepository_serialize(
            (const RoundRecord *)current->data, line, sizeof(line)
        );
        if (!result.success) { fclose(file); return result; }

        if (fputs(line, file) == EOF || fputc('\n', file) == EOF) {
            fclose(file);
            return Result_make_failure("failed to write round row");
        }
        current = current->next;
    }

    fclose(file);
    return Result_make_success("round records saved");
}

/** 清空并释放查房记录链表中的所有元素 */
void RoundRecordRepository_clear_list(LinkedList *records) {
    LinkedList_clear(records, RoundRecordRepository_free_record);
}
