/**
 * @file NursingRecordRepository.c
 * @brief 护理记录数据仓储层实现
 *
 * 实现护理记录（NursingRecord）数据的持久化存储操作，包括：
 * - 护理记录的序列化与反序列化
 * - 数据校验（必填字段检查）
 * - 追加、按住院记录ID筛选、全量加载、全量保存
 */

#include "repository/NursingRecordRepository.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "repository/RepositoryUtils.h"

/** 护理记录数据文件的表头行 */
static const char NURSING_RECORD_REPOSITORY_HEADER[] =
    "nursing_id|admission_id|nurse_name|record_type|content|recorded_at";

/** 按住院记录ID收集时使用的上下文 */
typedef struct NursingRecordCollectByAdmissionContext {
    const char *admission_id;
    LinkedList *out_records;
} NursingRecordCollectByAdmissionContext;

/** 加载所有护理记录时使用的上下文 */
typedef struct NursingRecordLoadContext {
    LinkedList *out_records;
} NursingRecordLoadContext;

/** 判断文本是否为空 */
static int NursingRecordRepository_is_empty_text(const char *text) {
    return text == 0 || text[0] == '\0';
}

/** 校验单个文本字段 */
static Result NursingRecordRepository_validate_text_field(
    const char *text, const char *field_name, int allow_empty
) {
    char message[RESULT_MESSAGE_CAPACITY];
    if (text == 0 || (!allow_empty && NursingRecordRepository_is_empty_text(text))) {
        snprintf(message, sizeof(message), "%s missing", field_name);
        return Result_make_failure(message);
    }
    if (!RepositoryUtils_is_safe_field_text(text)) {
        snprintf(message, sizeof(message), "%s contains reserved characters", field_name);
        return Result_make_failure(message);
    }
    return Result_make_success("nursing text field ok");
}

/**
 * @brief 校验护理记录的合法性
 *
 * 所有字段均为必填。
 */
static Result NursingRecordRepository_validate(const NursingRecord *record) {
    Result result;
    if (record == 0) return Result_make_failure("nursing record missing");

    result = NursingRecordRepository_validate_text_field(record->nursing_id, "nursing_id", 0);
    if (!result.success) return result;
    result = NursingRecordRepository_validate_text_field(record->admission_id, "admission_id", 0);
    if (!result.success) return result;
    result = NursingRecordRepository_validate_text_field(record->nurse_name, "nurse_name", 0);
    if (!result.success) return result;
    result = NursingRecordRepository_validate_text_field(record->record_type, "record_type", 0);
    if (!result.success) return result;
    result = NursingRecordRepository_validate_text_field(record->content, "content", 0);
    if (!result.success) return result;
    result = NursingRecordRepository_validate_text_field(record->recorded_at, "recorded_at", 0);
    if (!result.success) return result;

    return Result_make_success("nursing record valid");
}

/** 丢弃行剩余字符 */
static void NursingRecordRepository_discard_rest_of_line(FILE *file) {
    int character = 0;
    while ((character = fgetc(file)) != EOF) {
        if (character == '\n') return;
    }
}

/** 写入表头（覆盖） */
static Result NursingRecordRepository_write_header(const NursingRecordRepository *repository) {
    char content[TEXT_FILE_REPOSITORY_LINE_CAPACITY];
    int written = snprintf(content, sizeof(content), "%s\n", NURSING_RECORD_REPOSITORY_HEADER);
    if (written < 0 || (size_t)written >= sizeof(content)) {
        return Result_make_failure("nursing header too long");
    }
    return TextFileRepository_save_file(&repository->storage, content);
}

/** 将护理记录序列化为文本行 */
static Result NursingRecordRepository_serialize(
    const NursingRecord *record, char *line, size_t capacity
) {
    int written = 0;
    Result result = NursingRecordRepository_validate(record);
    if (!result.success) return result;

    written = snprintf(line, capacity, "%s|%s|%s|%s|%s|%s",
        record->nursing_id, record->admission_id, record->nurse_name,
        record->record_type, record->content, record->recorded_at);
    if (written < 0 || (size_t)written >= capacity) return Result_make_failure("nursing line too long");
    return Result_make_success("nursing serialized");
}

/** 将一行文本解析为护理记录结构体 */
static Result NursingRecordRepository_parse_line(const char *line, NursingRecord *out_record) {
    char buffer[TEXT_FILE_REPOSITORY_LINE_CAPACITY];
    char *fields[NURSING_RECORD_REPOSITORY_FIELD_COUNT];
    size_t field_count = 0;
    Result result;

    if (line == 0 || out_record == 0) return Result_make_failure("nursing parse arguments invalid");
    if (strlen(line) >= sizeof(buffer)) return Result_make_failure("nursing line too long");

    strncpy(buffer, line, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';
    result = RepositoryUtils_split_pipe_line(buffer, fields, NURSING_RECORD_REPOSITORY_FIELD_COUNT, &field_count);
    if (!result.success) return result;
    result = RepositoryUtils_validate_field_count(field_count, NURSING_RECORD_REPOSITORY_FIELD_COUNT);
    if (!result.success) return result;

    memset(out_record, 0, sizeof(*out_record));
    RepositoryUtils_copy_text(out_record->nursing_id, sizeof(out_record->nursing_id), fields[0]);
    RepositoryUtils_copy_text(out_record->admission_id, sizeof(out_record->admission_id), fields[1]);
    RepositoryUtils_copy_text(out_record->nurse_name, sizeof(out_record->nurse_name), fields[2]);
    RepositoryUtils_copy_text(out_record->record_type, sizeof(out_record->record_type), fields[3]);
    RepositoryUtils_copy_text(out_record->content, sizeof(out_record->content), fields[4]);
    RepositoryUtils_copy_text(out_record->recorded_at, sizeof(out_record->recorded_at), fields[5]);

    return NursingRecordRepository_validate(out_record);
}

/** 释放护理记录 */
static void NursingRecordRepository_free_record(void *data) { free(data); }

/** 按住院记录ID收集的行处理回调 */
static Result NursingRecordRepository_collect_by_admission_line_handler(const char *line, void *context) {
    NursingRecordCollectByAdmissionContext *collect_context =
        (NursingRecordCollectByAdmissionContext *)context;
    NursingRecord record;
    NursingRecord *copy = 0;
    Result result = NursingRecordRepository_parse_line(line, &record);
    if (!result.success) return result;

    if (strcmp(record.admission_id, collect_context->admission_id) != 0) {
        return Result_make_success("nursing skipped");
    }

    copy = (NursingRecord *)malloc(sizeof(*copy));
    if (copy == 0) return Result_make_failure("failed to allocate nursing result");
    *copy = record;
    if (!LinkedList_append(collect_context->out_records, copy)) {
        free(copy);
        return Result_make_failure("failed to append nursing result");
    }
    return Result_make_success("nursing collected");
}

/** 加载所有护理记录的行处理回调 */
static Result NursingRecordRepository_load_line_handler(const char *line, void *context) {
    NursingRecordLoadContext *load_context = (NursingRecordLoadContext *)context;
    NursingRecord record;
    NursingRecord *copy = 0;
    Result result = NursingRecordRepository_parse_line(line, &record);
    if (!result.success) return result;

    copy = (NursingRecord *)malloc(sizeof(*copy));
    if (copy == 0) return Result_make_failure("failed to allocate nursing result");
    *copy = record;
    if (!LinkedList_append(load_context->out_records, copy)) {
        free(copy);
        return Result_make_failure("failed to append nursing result");
    }
    return Result_make_success("nursing loaded");
}

/** 初始化护理记录仓储 */
Result NursingRecordRepository_init(NursingRecordRepository *repository, const char *path) {
    if (repository == 0) return Result_make_failure("nursing repository missing");
    return TextFileRepository_init(&repository->storage, path);
}

/** 确保存储文件已就绪 */
Result NursingRecordRepository_ensure_storage(const NursingRecordRepository *repository) {
    char line[TEXT_FILE_REPOSITORY_LINE_CAPACITY];
    FILE *file = 0;
    Result result;

    if (repository == 0) return Result_make_failure("nursing repository missing");

    result = TextFileRepository_ensure_file_exists(&repository->storage);
    if (!result.success) return result;

    file = fopen(repository->storage.path, "r");
    if (file == 0) return Result_make_failure("failed to inspect nursing storage");

    while (fgets(line, sizeof(line), file) != 0) {
        if (strchr(line, '\n') == 0 && !feof(file)) {
            NursingRecordRepository_discard_rest_of_line(file);
            fclose(file);
            return Result_make_failure("nursing header line too long");
        }
        RepositoryUtils_strip_line_endings(line);
        if (RepositoryUtils_is_blank_line(line)) continue;
        fclose(file);
        if (strcmp(line, NURSING_RECORD_REPOSITORY_HEADER) != 0) {
            return Result_make_failure("unexpected nursing header");
        }
        return Result_make_success("nursing storage ready");
    }

    fclose(file);
    return NursingRecordRepository_write_header(repository);
}

/** 追加一条护理记录 */
Result NursingRecordRepository_append(
    const NursingRecordRepository *repository, const NursingRecord *record
) {
    char line[TEXT_FILE_REPOSITORY_LINE_CAPACITY];
    Result result = NursingRecordRepository_ensure_storage(repository);
    if (!result.success) return result;
    result = NursingRecordRepository_serialize(record, line, sizeof(line));
    if (!result.success) return result;
    return TextFileRepository_append_line(&repository->storage, line);
}

/** 按住院记录ID查找护理记录列表 */
Result NursingRecordRepository_find_by_admission_id(
    const NursingRecordRepository *repository, const char *admission_id, LinkedList *out_records
) {
    NursingRecordCollectByAdmissionContext context;
    LinkedList matches;
    Result result;

    if (admission_id == 0 || admission_id[0] == '\0' || out_records == 0) {
        return Result_make_failure("nursing admission query arguments invalid");
    }
    if (LinkedList_count(out_records) != 0) return Result_make_failure("output nursing list must be empty");

    result = NursingRecordRepository_ensure_storage(repository);
    if (!result.success) return result;

    LinkedList_init(&matches);
    context.admission_id = admission_id;
    context.out_records = &matches;
    result = TextFileRepository_for_each_data_line(
        &repository->storage, NursingRecordRepository_collect_by_admission_line_handler, &context
    );
    if (!result.success) { NursingRecordRepository_clear_list(&matches); return result; }

    *out_records = matches;
    return Result_make_success("nursing records loaded");
}

/** 加载所有护理记录到链表 */
Result NursingRecordRepository_load_all(
    const NursingRecordRepository *repository, LinkedList *out_records
) {
    NursingRecordLoadContext context;
    Result result;

    if (repository == 0 || out_records == 0) return Result_make_failure("nursing load arguments invalid");
    if (LinkedList_count(out_records) != 0) return Result_make_failure("output nursing list must be empty");

    result = NursingRecordRepository_ensure_storage(repository);
    if (!result.success) return result;

    context.out_records = out_records;
    result = TextFileRepository_for_each_data_line(
        &repository->storage, NursingRecordRepository_load_line_handler, &context
    );
    if (!result.success) { NursingRecordRepository_clear_list(out_records); return result; }
    return Result_make_success("nursing records loaded");
}

/** 全量保存护理记录列表到文件（覆盖写入） */
Result NursingRecordRepository_save_all(
    const NursingRecordRepository *repository, const LinkedList *records
) {
    LinkedListNode *current = 0;
    Result result;

    if (repository == 0 || records == 0) return Result_make_failure("nursing save arguments invalid");

    {
        char line[TEXT_FILE_REPOSITORY_LINE_CAPACITY];
        char *content = 0;
        size_t capacity = (records->count + 2) * TEXT_FILE_REPOSITORY_LINE_CAPACITY;
        size_t used = 0;
        size_t len = 0;

        content = (char *)malloc(capacity);
        if (content == 0) {
            return Result_make_failure("failed to allocate nursing content buffer");
        }

        len = strlen(NURSING_RECORD_REPOSITORY_HEADER);
        memcpy(content + used, NURSING_RECORD_REPOSITORY_HEADER, len);
        used += len;
        content[used++] = '\n';

        current = records->head;
        while (current != 0) {
            result = NursingRecordRepository_serialize(
                (const NursingRecord *)current->data, line, sizeof(line)
            );
            if (!result.success) { free(content); return result; }

            len = strlen(line);
            if (used + len + 2 > capacity) {
                char *tmp;
                capacity *= 2;
                tmp = (char *)realloc(content, capacity);
                if (tmp == 0) {
                    free(content);
                    content = 0;
                    return Result_make_failure("failed to grow nursing content buffer");
                }
                content = tmp;
            }
            memcpy(content + used, line, len);
            used += len;
            content[used++] = '\n';

            current = current->next;
        }

        content[used] = '\0';
        result = TextFileRepository_save_file(&repository->storage, content);
        free(content);
        return result;
    }
}

/** 清空并释放护理记录链表中的所有元素 */
void NursingRecordRepository_clear_list(LinkedList *records) {
    LinkedList_clear(records, NursingRecordRepository_free_record);
}
