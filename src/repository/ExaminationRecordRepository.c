/**
 * @file ExaminationRecordRepository.c
 * @brief 检查记录数据仓储层实现
 *
 * 实现检查记录（ExaminationRecord）数据的持久化存储操作，包括：
 * - 检查记录的序列化与反序列化（11个字段）
 * - 数据校验（必填字段、状态与完成时间一致性、保留字符检测）
 * - 追加、按检查ID查找、按就诊ID筛选、全量加载、全量保存
 */

#include "repository/ExaminationRecordRepository.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "repository/RepositoryUtils.h"

/** 检查记录数据文件的表头行 */
static const char EXAMINATION_RECORD_REPOSITORY_HEADER[] =
    "examination_id|visit_id|patient_id|doctor_id|exam_item|exam_type|status|result|exam_fee|requested_at|completed_at";

/** 按检查ID查找时使用的上下文 */
typedef struct ExaminationRecordFindByIdContext {
    const char *examination_id;
    ExaminationRecord *out_record;
    int found;
} ExaminationRecordFindByIdContext;

/** 按就诊ID收集时使用的上下文 */
typedef struct ExaminationRecordCollectByVisitContext {
    const char *visit_id;
    LinkedList *out_records;
} ExaminationRecordCollectByVisitContext;

/** 加载所有检查记录时使用的上下文 */
typedef struct ExaminationRecordLoadContext {
    LinkedList *out_records;
} ExaminationRecordLoadContext;

/** 判断文本是否为空 */
static int ExaminationRecordRepository_is_empty_text(const char *text) {
    return text == 0 || text[0] == '\0';
}

/** 安全复制字符串 */
static void ExaminationRecordRepository_copy_text(
    char *destination, size_t capacity, const char *source
) {
    if (destination == 0 || capacity == 0) return;
    if (source == 0) { destination[0] = '\0'; return; }
    strncpy(destination, source, capacity - 1);
    destination[capacity - 1] = '\0';
}

/** 校验单个文本字段 */
static Result ExaminationRecordRepository_validate_text_field(
    const char *text, const char *field_name, int allow_empty
) {
    char message[RESULT_MESSAGE_CAPACITY];
    if (text == 0 || (!allow_empty && ExaminationRecordRepository_is_empty_text(text))) {
        snprintf(message, sizeof(message), "%s missing", field_name);
        return Result_make_failure(message);
    }
    if (!RepositoryUtils_is_safe_field_text(text)) {
        snprintf(message, sizeof(message), "%s contains reserved characters", field_name);
        return Result_make_failure(message);
    }
    return Result_make_success("exam text field ok");
}

/** 解析检查状态枚举值 */
static Result ExaminationRecordRepository_parse_status(
    const char *field, ExaminationStatus *out_status
) {
    char *end = 0;
    long value = 0;
    if (field == 0 || out_status == 0 || field[0] == '\0') return Result_make_failure("exam status missing");
    value = strtol(field, &end, 10);
    if (end == 0 || *end != '\0') return Result_make_failure("exam status invalid");
    if (value < EXAM_STATUS_PENDING || value > EXAM_STATUS_COMPLETED) {
        return Result_make_failure("exam status out of range");
    }
    *out_status = (ExaminationStatus)value;
    return Result_make_success("exam status parsed");
}

/**
 * @brief 校验检查记录的合法性
 *
 * 状态一致性规则：
 * - 待检查：不能有完成时间
 * - 已完成：必须有完成时间
 */
static Result ExaminationRecordRepository_validate(const ExaminationRecord *record) {
    Result result;
    if (record == 0) return Result_make_failure("exam record missing");

    result = ExaminationRecordRepository_validate_text_field(record->examination_id, "examination_id", 0);
    if (!result.success) return result;
    result = ExaminationRecordRepository_validate_text_field(record->visit_id, "visit_id", 0);
    if (!result.success) return result;
    result = ExaminationRecordRepository_validate_text_field(record->patient_id, "patient_id", 0);
    if (!result.success) return result;
    result = ExaminationRecordRepository_validate_text_field(record->doctor_id, "doctor_id", 0);
    if (!result.success) return result;
    result = ExaminationRecordRepository_validate_text_field(record->exam_item, "exam_item", 0);
    if (!result.success) return result;
    result = ExaminationRecordRepository_validate_text_field(record->exam_type, "exam_type", 0);
    if (!result.success) return result;
    result = ExaminationRecordRepository_validate_text_field(record->result, "result", 1); /* 可为空 */
    if (!result.success) return result;
    if (record->exam_fee < 0) return Result_make_failure("exam_fee must be non-negative");
    result = ExaminationRecordRepository_validate_text_field(record->requested_at, "requested_at", 0);
    if (!result.success) return result;
    result = ExaminationRecordRepository_validate_text_field(record->completed_at, "completed_at", 1); /* 可为空 */
    if (!result.success) return result;

    /* 待检查状态不能有完成时间 */
    if (record->status == EXAM_STATUS_PENDING) {
        if (!ExaminationRecordRepository_is_empty_text(record->completed_at)) {
            return Result_make_failure("pending exam cannot have completed_at");
        }
    } else if (record->status == EXAM_STATUS_COMPLETED) {
        /* 已完成状态必须有完成时间 */
        if (ExaminationRecordRepository_is_empty_text(record->completed_at)) {
            return Result_make_failure("completed exam missing completed_at");
        }
    } else {
        return Result_make_failure("exam status invalid");
    }

    return Result_make_success("exam record valid");
}

/** 丢弃行剩余字符 */
static void ExaminationRecordRepository_discard_rest_of_line(FILE *file) {
    int character = 0;
    while ((character = fgetc(file)) != EOF) {
        if (character == '\n') return;
    }
}

/** 写入表头（覆盖） */
static Result ExaminationRecordRepository_write_header(const ExaminationRecordRepository *repository) {
    FILE *file = fopen(repository->storage.path, "w");
    if (file == 0) return Result_make_failure("failed to rewrite exam storage");

    if (fputs(EXAMINATION_RECORD_REPOSITORY_HEADER, file) == EOF || fputc('\n', file) == EOF) {
        fclose(file);
        return Result_make_failure("failed to write exam header");
    }
    fclose(file);
    return Result_make_success("exam header ready");
}

/** 将检查记录序列化为文本行 */
static Result ExaminationRecordRepository_serialize(
    const ExaminationRecord *record, char *line, size_t capacity
) {
    int written = 0;
    Result result = ExaminationRecordRepository_validate(record);
    if (!result.success) return result;

    written = snprintf(line, capacity, "%s|%s|%s|%s|%s|%s|%d|%s|%.2f|%s|%s",
        record->examination_id, record->visit_id, record->patient_id,
        record->doctor_id, record->exam_item, record->exam_type,
        (int)record->status, record->result, record->exam_fee,
        record->requested_at, record->completed_at);
    if (written < 0 || (size_t)written >= capacity) return Result_make_failure("exam line too long");
    return Result_make_success("exam serialized");
}

/** 将一行文本解析为检查记录结构体 */
static Result ExaminationRecordRepository_parse_line(const char *line, ExaminationRecord *out_record) {
    char buffer[TEXT_FILE_REPOSITORY_LINE_CAPACITY];
    char *fields[EXAMINATION_RECORD_REPOSITORY_FIELD_COUNT];
    size_t field_count = 0;
    Result result;

    if (line == 0 || out_record == 0) return Result_make_failure("exam parse arguments invalid");
    if (strlen(line) >= sizeof(buffer)) return Result_make_failure("exam line too long");

    strncpy(buffer, line, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';
    result = RepositoryUtils_split_pipe_line(
        buffer, fields, EXAMINATION_RECORD_REPOSITORY_FIELD_COUNT, &field_count
    );
    if (!result.success) return result;
    result = RepositoryUtils_validate_field_count(field_count, EXAMINATION_RECORD_REPOSITORY_FIELD_COUNT);
    if (!result.success) return result;

    memset(out_record, 0, sizeof(*out_record));
    ExaminationRecordRepository_copy_text(out_record->examination_id, sizeof(out_record->examination_id), fields[0]);
    ExaminationRecordRepository_copy_text(out_record->visit_id, sizeof(out_record->visit_id), fields[1]);
    ExaminationRecordRepository_copy_text(out_record->patient_id, sizeof(out_record->patient_id), fields[2]);
    ExaminationRecordRepository_copy_text(out_record->doctor_id, sizeof(out_record->doctor_id), fields[3]);
    ExaminationRecordRepository_copy_text(out_record->exam_item, sizeof(out_record->exam_item), fields[4]);
    ExaminationRecordRepository_copy_text(out_record->exam_type, sizeof(out_record->exam_type), fields[5]);

    result = ExaminationRecordRepository_parse_status(fields[6], &out_record->status);
    if (!result.success) return result;

    ExaminationRecordRepository_copy_text(out_record->result, sizeof(out_record->result), fields[7]);

    /* 解析检查费用 */
    {
        char *end_ptr = 0;
        errno = 0;
        out_record->exam_fee = strtod(fields[8], &end_ptr);
        if (end_ptr == fields[8] || errno == ERANGE) {
            return Result_make_failure("exam_fee parse failed");
        }
        if (out_record->exam_fee < 0) {
            return Result_make_failure("exam_fee must be non-negative");
        }
    }

    ExaminationRecordRepository_copy_text(out_record->requested_at, sizeof(out_record->requested_at), fields[9]);
    ExaminationRecordRepository_copy_text(out_record->completed_at, sizeof(out_record->completed_at), fields[10]);
    return ExaminationRecordRepository_validate(out_record);
}

/** 释放检查记录 */
static void ExaminationRecordRepository_free_record(void *data) { free(data); }

/** 按检查ID查找的行处理回调 */
static Result ExaminationRecordRepository_find_by_id_line_handler(const char *line, void *context) {
    ExaminationRecordFindByIdContext *find_context = (ExaminationRecordFindByIdContext *)context;
    ExaminationRecord record;
    Result result = ExaminationRecordRepository_parse_line(line, &record);
    if (!result.success) return result;

    if (strcmp(record.examination_id, find_context->examination_id) != 0) {
        return Result_make_success("exam skipped");
    }
    if (find_context->found) return Result_make_failure("duplicate examination id");

    *find_context->out_record = record;
    find_context->found = 1;
    return Result_make_success("exam matched");
}

/** 按就诊ID收集检查记录的行处理回调 */
static Result ExaminationRecordRepository_collect_by_visit_line_handler(const char *line, void *context) {
    ExaminationRecordCollectByVisitContext *collect_context =
        (ExaminationRecordCollectByVisitContext *)context;
    ExaminationRecord record;
    ExaminationRecord *copy = 0;
    Result result = ExaminationRecordRepository_parse_line(line, &record);
    if (!result.success) return result;

    if (strcmp(record.visit_id, collect_context->visit_id) != 0) {
        return Result_make_success("exam skipped");
    }

    copy = (ExaminationRecord *)malloc(sizeof(*copy));
    if (copy == 0) return Result_make_failure("failed to allocate exam result");
    *copy = record;
    if (!LinkedList_append(collect_context->out_records, copy)) {
        free(copy);
        return Result_make_failure("failed to append exam result");
    }
    return Result_make_success("exam collected");
}

/** 加载所有检查记录的行处理回调 */
static Result ExaminationRecordRepository_load_line_handler(const char *line, void *context) {
    ExaminationRecordLoadContext *load_context = (ExaminationRecordLoadContext *)context;
    ExaminationRecord record;
    ExaminationRecord *copy = 0;
    Result result = ExaminationRecordRepository_parse_line(line, &record);
    if (!result.success) return result;

    if (load_context == 0 || load_context->out_records == 0) {
        return Result_make_failure("exam load context missing");
    }

    copy = (ExaminationRecord *)malloc(sizeof(*copy));
    if (copy == 0) return Result_make_failure("failed to allocate exam result");
    *copy = record;
    if (!LinkedList_append(load_context->out_records, copy)) {
        free(copy);
        return Result_make_failure("failed to append exam result");
    }
    return Result_make_success("exam loaded");
}

/** 初始化检查记录仓储 */
Result ExaminationRecordRepository_init(ExaminationRecordRepository *repository, const char *path) {
    if (repository == 0) return Result_make_failure("exam repository missing");
    return TextFileRepository_init(&repository->storage, path);
}

/** 确保存储文件已就绪 */
Result ExaminationRecordRepository_ensure_storage(const ExaminationRecordRepository *repository) {
    char line[TEXT_FILE_REPOSITORY_LINE_CAPACITY];
    FILE *file = 0;
    Result result;

    if (repository == 0) return Result_make_failure("exam repository missing");
    result = TextFileRepository_ensure_file_exists(&repository->storage);
    if (!result.success) return result;

    file = fopen(repository->storage.path, "r");
    if (file == 0) return Result_make_failure("failed to inspect exam storage");

    while (fgets(line, sizeof(line), file) != 0) {
        if (strchr(line, '\n') == 0 && !feof(file)) {
            ExaminationRecordRepository_discard_rest_of_line(file);
            fclose(file);
            return Result_make_failure("exam header line too long");
        }
        RepositoryUtils_strip_line_endings(line);
        if (RepositoryUtils_is_blank_line(line)) continue;
        fclose(file);
        if (strcmp(line, EXAMINATION_RECORD_REPOSITORY_HEADER) != 0) {
            return Result_make_failure("unexpected exam header");
        }
        return Result_make_success("exam storage ready");
    }

    fclose(file);
    return ExaminationRecordRepository_write_header(repository);
}

/** 追加一条检查记录 */
Result ExaminationRecordRepository_append(
    const ExaminationRecordRepository *repository, const ExaminationRecord *record
) {
    char line[TEXT_FILE_REPOSITORY_LINE_CAPACITY];
    Result result = ExaminationRecordRepository_ensure_storage(repository);
    if (!result.success) return result;
    result = ExaminationRecordRepository_serialize(record, line, sizeof(line));
    if (!result.success) return result;
    return TextFileRepository_append_line(&repository->storage, line);
}

/** 按检查ID查找 */
Result ExaminationRecordRepository_find_by_examination_id(
    const ExaminationRecordRepository *repository, const char *examination_id, ExaminationRecord *out_record
) {
    ExaminationRecordFindByIdContext context;
    Result result;

    if (examination_id == 0 || examination_id[0] == '\0' || out_record == 0) {
        return Result_make_failure("exam query arguments invalid");
    }
    result = ExaminationRecordRepository_ensure_storage(repository);
    if (!result.success) return result;

    context.examination_id = examination_id;
    context.out_record = out_record;
    context.found = 0;
    result = TextFileRepository_for_each_data_line(
        &repository->storage, ExaminationRecordRepository_find_by_id_line_handler, &context
    );
    if (!result.success) return result;
    if (!context.found) return Result_make_failure("exam not found");
    return Result_make_success("exam found");
}

/** 按就诊ID查找关联的检查记录列表 */
Result ExaminationRecordRepository_find_by_visit_id(
    const ExaminationRecordRepository *repository, const char *visit_id, LinkedList *out_records
) {
    ExaminationRecordCollectByVisitContext context;
    LinkedList matches;
    Result result;

    if (visit_id == 0 || visit_id[0] == '\0' || out_records == 0) {
        return Result_make_failure("exam visit query arguments invalid");
    }
    if (LinkedList_count(out_records) != 0) return Result_make_failure("output exam list must be empty");

    result = ExaminationRecordRepository_ensure_storage(repository);
    if (!result.success) return result;

    LinkedList_init(&matches);
    context.visit_id = visit_id;
    context.out_records = &matches;
    result = TextFileRepository_for_each_data_line(
        &repository->storage, ExaminationRecordRepository_collect_by_visit_line_handler, &context
    );
    if (!result.success) { ExaminationRecordRepository_clear_list(&matches); return result; }

    *out_records = matches;
    return Result_make_success("exam records loaded");
}

/** 加载所有检查记录到链表 */
Result ExaminationRecordRepository_load_all(
    const ExaminationRecordRepository *repository, LinkedList *out_records
) {
    ExaminationRecordLoadContext context;
    LinkedList loaded;
    Result result;

    if (out_records == 0) return Result_make_failure("exam output list missing");
    if (LinkedList_count(out_records) != 0) return Result_make_failure("output exam list must be empty");

    LinkedList_init(&loaded);
    result = ExaminationRecordRepository_ensure_storage(repository);
    if (!result.success) return result;

    context.out_records = &loaded;
    result = TextFileRepository_for_each_data_line(
        &repository->storage, ExaminationRecordRepository_load_line_handler, &context
    );
    if (!result.success) { ExaminationRecordRepository_clear_list(&loaded); return result; }

    *out_records = loaded;
    return Result_make_success("exam records loaded");
}

/** 全量保存检查记录列表到文件（覆盖写入） */
Result ExaminationRecordRepository_save_all(
    const ExaminationRecordRepository *repository, const LinkedList *records
) {
    LinkedListNode *current = 0;
    FILE *file = 0;
    Result result;

    if (repository == 0 || records == 0) return Result_make_failure("exam save arguments invalid");
    result = TextFileRepository_ensure_file_exists(&repository->storage);
    if (!result.success) return result;

    file = fopen(repository->storage.path, "w");
    if (file == 0) return Result_make_failure("failed to rewrite exam storage");

    if (fputs(EXAMINATION_RECORD_REPOSITORY_HEADER, file) == EOF || fputc('\n', file) == EOF) {
        fclose(file);
        return Result_make_failure("failed to write exam header");
    }

    current = records->head;
    while (current != 0) {
        char line[TEXT_FILE_REPOSITORY_LINE_CAPACITY];
        result = ExaminationRecordRepository_serialize(
            (const ExaminationRecord *)current->data, line, sizeof(line)
        );
        if (!result.success) { fclose(file); return result; }

        if (fputs(line, file) == EOF || fputc('\n', file) == EOF) {
            fclose(file);
            return Result_make_failure("failed to write exam row");
        }
        current = current->next;
    }

    fclose(file);
    return Result_make_success("exam records saved");
}

/** 清空并释放检查记录链表中的所有元素 */
void ExaminationRecordRepository_clear_list(LinkedList *records) {
    LinkedList_clear(records, ExaminationRecordRepository_free_record);
}
