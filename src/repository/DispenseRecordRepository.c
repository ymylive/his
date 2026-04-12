/**
 * @file DispenseRecordRepository.c
 * @brief 发药记录数据仓储层实现
 *
 * 实现发药记录（DispenseRecord）数据的持久化存储操作，包括：
 * - 发药记录的序列化与反序列化
 * - 数据校验（必填字段、状态与药剂师/时间戳一致性检查）
 * - 追加、按发药ID查找、按处方ID筛选、按患者ID筛选、全量加载、全量保存
 */

#include "repository/DispenseRecordRepository.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "repository/RepositoryUtils.h"

/** 发药记录数据文件的表头行 */
static const char DISPENSE_RECORD_REPOSITORY_HEADER[] =
    "dispense_id|patient_id|prescription_id|medicine_id|quantity|pharmacist_id|dispensed_at|status";

/** 按发药ID查找时使用的上下文 */
typedef struct DispenseRecordFindByIdContext {
    const char *dispense_id;
    DispenseRecord *out_record;
    int found;
} DispenseRecordFindByIdContext;

/** 按处方ID收集时使用的上下文 */
typedef struct DispenseRecordCollectByPrescriptionContext {
    const char *prescription_id;
    LinkedList *out_records;
} DispenseRecordCollectByPrescriptionContext;

/** 按患者ID收集时使用的上下文 */
typedef struct DispenseRecordCollectByPatientContext {
    const char *patient_id;
    LinkedList *out_records;
} DispenseRecordCollectByPatientContext;

/** 加载所有发药记录时使用的上下文 */
typedef struct DispenseRecordLoadContext {
    LinkedList *out_records;
} DispenseRecordLoadContext;

/** 判断文本是否为空 */
static int DispenseRecordRepository_is_empty_text(const char *text) {
    return text == 0 || text[0] == '\0';
}

/** 安全复制字符串 */
static void DispenseRecordRepository_copy_text(
    char *destination, size_t capacity, const char *source
) {
    if (destination == 0 || capacity == 0) return;
    if (source == 0) { destination[0] = '\0'; return; }
    strncpy(destination, source, capacity - 1);
    destination[capacity - 1] = '\0';
}

/** 校验单个文本字段 */
static Result DispenseRecordRepository_validate_text_field(
    const char *text, const char *field_name, int allow_empty
) {
    char message[RESULT_MESSAGE_CAPACITY];
    if (text == 0 || (!allow_empty && DispenseRecordRepository_is_empty_text(text))) {
        snprintf(message, sizeof(message), "%s missing", field_name);
        return Result_make_failure(message);
    }
    if (!RepositoryUtils_is_safe_field_text(text)) {
        snprintf(message, sizeof(message), "%s contains reserved characters", field_name);
        return Result_make_failure(message);
    }
    return Result_make_success("dispense text field ok");
}

/** 解析整数字段 */
static Result DispenseRecordRepository_parse_int(const char *field, int *out_value) {
    char *end = 0;
    long value = 0;
    if (field == 0 || out_value == 0 || field[0] == '\0') return Result_make_failure("dispense integer missing");
    value = strtol(field, &end, 10);
    if (end == 0 || *end != '\0') return Result_make_failure("dispense integer invalid");
    *out_value = (int)value;
    return Result_make_success("dispense integer parsed");
}

/** 解析发药状态枚举值 */
static Result DispenseRecordRepository_parse_status(const char *field, DispenseStatus *out_status) {
    int value = 0;
    Result result = DispenseRecordRepository_parse_int(field, &value);
    if (!result.success) return result;

    if (value < DISPENSE_STATUS_PENDING || value > DISPENSE_STATUS_COMPLETED) {
        return Result_make_failure("dispense status out of range");
    }
    *out_status = (DispenseStatus)value;
    return Result_make_success("dispense status parsed");
}

/**
 * @brief 校验发药记录的合法性
 *
 * 状态一致性规则：
 * - 待发药：药剂师ID和发药时间可为空
 * - 已完成：药剂师ID和发药时间必须非空
 */
static Result DispenseRecordRepository_validate(const DispenseRecord *record) {
    Result result;
    if (record == 0) return Result_make_failure("dispense record missing");

    result = DispenseRecordRepository_validate_text_field(record->dispense_id, "dispense_id", 0);
    if (!result.success) return result;
    result = DispenseRecordRepository_validate_text_field(record->patient_id, "patient_id", 1);
    if (!result.success) return result;
    result = DispenseRecordRepository_validate_text_field(record->prescription_id, "prescription_id", 0);
    if (!result.success) return result;
    result = DispenseRecordRepository_validate_text_field(record->medicine_id, "medicine_id", 0);
    if (!result.success) return result;

    if (record->quantity <= 0) return Result_make_failure("dispense quantity invalid");

    /* 药剂师ID：待发药时可空，已完成时必填 */
    result = DispenseRecordRepository_validate_text_field(
        record->pharmacist_id, "pharmacist_id", record->status == DISPENSE_STATUS_PENDING
    );
    if (!result.success) return result;

    /* 发药时间：待发药时可空，已完成时必填 */
    result = DispenseRecordRepository_validate_text_field(
        record->dispensed_at, "dispensed_at", record->status == DISPENSE_STATUS_PENDING
    );
    if (!result.success) return result;

    if (record->status == DISPENSE_STATUS_PENDING) {
        return Result_make_success("dispense record valid");
    }

    if (record->status == DISPENSE_STATUS_COMPLETED) {
        if (DispenseRecordRepository_is_empty_text(record->pharmacist_id) ||
            DispenseRecordRepository_is_empty_text(record->dispensed_at)) {
            return Result_make_failure("completed dispense missing close data");
        }
        return Result_make_success("dispense record valid");
    }

    return Result_make_failure("dispense status invalid");
}

/** 丢弃行剩余字符 */
static void DispenseRecordRepository_discard_rest_of_line(FILE *file) {
    int character = 0;
    while ((character = fgetc(file)) != EOF) {
        if (character == '\n') return;
    }
}

/** 写入表头（覆盖） */
static Result DispenseRecordRepository_write_header(const DispenseRecordRepository *repository) {
    FILE *file = fopen(repository->storage.path, "w");
    if (file == 0) return Result_make_failure("failed to rewrite dispense storage");

    if (fputs(DISPENSE_RECORD_REPOSITORY_HEADER, file) == EOF || fputc('\n', file) == EOF) {
        fclose(file);
        return Result_make_failure("failed to write dispense header");
    }
    fclose(file);
    return Result_make_success("dispense header ready");
}

/** 将发药记录序列化为文本行 */
static Result DispenseRecordRepository_serialize(
    const DispenseRecord *record, char *line, size_t capacity
) {
    int written = 0;
    Result result = DispenseRecordRepository_validate(record);
    if (!result.success) return result;

    written = snprintf(line, capacity, "%s|%s|%s|%s|%d|%s|%s|%d",
        record->dispense_id, record->patient_id, record->prescription_id,
        record->medicine_id, record->quantity, record->pharmacist_id,
        record->dispensed_at, (int)record->status);
    if (written < 0 || (size_t)written >= capacity) return Result_make_failure("dispense line too long");
    return Result_make_success("dispense serialized");
}

/** 将一行文本解析为发药记录结构体 */
static Result DispenseRecordRepository_parse_line(const char *line, DispenseRecord *out_record) {
    char buffer[TEXT_FILE_REPOSITORY_LINE_CAPACITY];
    char *fields[DISPENSE_RECORD_REPOSITORY_FIELD_COUNT];
    size_t field_count = 0;
    Result result;

    if (line == 0 || out_record == 0) return Result_make_failure("dispense parse arguments invalid");
    if (strlen(line) >= sizeof(buffer)) return Result_make_failure("dispense line too long");

    strncpy(buffer, line, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';
    result = RepositoryUtils_split_pipe_line(buffer, fields, DISPENSE_RECORD_REPOSITORY_FIELD_COUNT, &field_count);
    if (!result.success) return result;
    result = RepositoryUtils_validate_field_count(field_count, DISPENSE_RECORD_REPOSITORY_FIELD_COUNT);
    if (!result.success) return result;

    memset(out_record, 0, sizeof(*out_record));
    DispenseRecordRepository_copy_text(out_record->dispense_id, sizeof(out_record->dispense_id), fields[0]);
    DispenseRecordRepository_copy_text(out_record->patient_id, sizeof(out_record->patient_id), fields[1]);
    DispenseRecordRepository_copy_text(out_record->prescription_id, sizeof(out_record->prescription_id), fields[2]);
    DispenseRecordRepository_copy_text(out_record->medicine_id, sizeof(out_record->medicine_id), fields[3]);

    result = DispenseRecordRepository_parse_int(fields[4], &out_record->quantity);
    if (!result.success) return result;

    DispenseRecordRepository_copy_text(out_record->pharmacist_id, sizeof(out_record->pharmacist_id), fields[5]);
    DispenseRecordRepository_copy_text(out_record->dispensed_at, sizeof(out_record->dispensed_at), fields[6]);

    result = DispenseRecordRepository_parse_status(fields[7], &out_record->status);
    if (!result.success) return result;

    return DispenseRecordRepository_validate(out_record);
}

/** 释放发药记录 */
static void DispenseRecordRepository_free_record(void *data) { free(data); }

/** 按发药ID查找的行处理回调 */
static Result DispenseRecordRepository_find_by_id_line_handler(const char *line, void *context) {
    DispenseRecordFindByIdContext *find_context = (DispenseRecordFindByIdContext *)context;
    DispenseRecord record;
    Result result = DispenseRecordRepository_parse_line(line, &record);
    if (!result.success) return result;

    if (strcmp(record.dispense_id, find_context->dispense_id) != 0) return Result_make_success("dispense skipped");
    if (find_context->found) return Result_make_failure("duplicate dispense id");

    *find_context->out_record = record;
    find_context->found = 1;
    return Result_make_success("dispense matched");
}

/** 按处方ID收集的行处理回调 */
static Result DispenseRecordRepository_collect_by_prescription_line_handler(const char *line, void *context) {
    DispenseRecordCollectByPrescriptionContext *collect_context =
        (DispenseRecordCollectByPrescriptionContext *)context;
    DispenseRecord record;
    DispenseRecord *copy = 0;
    Result result = DispenseRecordRepository_parse_line(line, &record);
    if (!result.success) return result;

    if (strcmp(record.prescription_id, collect_context->prescription_id) != 0) {
        return Result_make_success("dispense skipped");
    }

    copy = (DispenseRecord *)malloc(sizeof(*copy));
    if (copy == 0) return Result_make_failure("failed to allocate dispense result");
    *copy = record;
    if (!LinkedList_append(collect_context->out_records, copy)) {
        free(copy);
        return Result_make_failure("failed to append dispense result");
    }
    return Result_make_success("dispense collected");
}

/** 按患者ID收集的行处理回调 */
static Result DispenseRecordRepository_collect_by_patient_line_handler(const char *line, void *context) {
    DispenseRecordCollectByPatientContext *collect_context =
        (DispenseRecordCollectByPatientContext *)context;
    DispenseRecord record;
    DispenseRecord *copy = 0;
    Result result = DispenseRecordRepository_parse_line(line, &record);
    if (!result.success) return result;

    if (strcmp(record.patient_id, collect_context->patient_id) != 0) {
        return Result_make_success("dispense skipped");
    }

    copy = (DispenseRecord *)malloc(sizeof(*copy));
    if (copy == 0) return Result_make_failure("failed to allocate dispense result");
    *copy = record;
    if (!LinkedList_append(collect_context->out_records, copy)) {
        free(copy);
        return Result_make_failure("failed to append dispense result");
    }
    return Result_make_success("dispense collected");
}

/** 加载所有发药记录的行处理回调 */
static Result DispenseRecordRepository_load_line_handler(const char *line, void *context) {
    DispenseRecordLoadContext *load_context = (DispenseRecordLoadContext *)context;
    DispenseRecord record;
    DispenseRecord *copy = 0;
    Result result = DispenseRecordRepository_parse_line(line, &record);
    if (!result.success) return result;

    copy = (DispenseRecord *)malloc(sizeof(*copy));
    if (copy == 0) return Result_make_failure("failed to allocate dispense result");
    *copy = record;
    if (!LinkedList_append(load_context->out_records, copy)) {
        free(copy);
        return Result_make_failure("failed to append dispense result");
    }
    return Result_make_success("dispense loaded");
}

/** 初始化发药记录仓储 */
Result DispenseRecordRepository_init(DispenseRecordRepository *repository, const char *path) {
    if (repository == 0) return Result_make_failure("dispense repository missing");
    return TextFileRepository_init(&repository->storage, path);
}

/** 确保存储文件已就绪 */
Result DispenseRecordRepository_ensure_storage(const DispenseRecordRepository *repository) {
    char line[TEXT_FILE_REPOSITORY_LINE_CAPACITY];
    FILE *file = 0;
    Result result;

    if (repository == 0) return Result_make_failure("dispense repository missing");

    result = TextFileRepository_ensure_file_exists(&repository->storage);
    if (!result.success) return result;

    file = fopen(repository->storage.path, "r");
    if (file == 0) return Result_make_failure("failed to inspect dispense storage");

    while (fgets(line, sizeof(line), file) != 0) {
        if (strchr(line, '\n') == 0 && !feof(file)) {
            DispenseRecordRepository_discard_rest_of_line(file);
            fclose(file);
            return Result_make_failure("dispense header line too long");
        }
        RepositoryUtils_strip_line_endings(line);
        if (RepositoryUtils_is_blank_line(line)) continue;
        fclose(file);
        if (strcmp(line, DISPENSE_RECORD_REPOSITORY_HEADER) != 0) {
            return Result_make_failure("unexpected dispense header");
        }
        return Result_make_success("dispense storage ready");
    }

    fclose(file);
    return DispenseRecordRepository_write_header(repository);
}

/** 追加一条发药记录 */
Result DispenseRecordRepository_append(
    const DispenseRecordRepository *repository, const DispenseRecord *record
) {
    char line[TEXT_FILE_REPOSITORY_LINE_CAPACITY];
    Result result = DispenseRecordRepository_ensure_storage(repository);
    if (!result.success) return result;
    result = DispenseRecordRepository_serialize(record, line, sizeof(line));
    if (!result.success) return result;
    return TextFileRepository_append_line(&repository->storage, line);
}

/** 按发药ID查找 */
Result DispenseRecordRepository_find_by_dispense_id(
    const DispenseRecordRepository *repository, const char *dispense_id, DispenseRecord *out_record
) {
    DispenseRecordFindByIdContext context;
    Result result;

    if (dispense_id == 0 || dispense_id[0] == '\0' || out_record == 0) {
        return Result_make_failure("dispense query arguments invalid");
    }
    result = DispenseRecordRepository_ensure_storage(repository);
    if (!result.success) return result;

    context.dispense_id = dispense_id;
    context.out_record = out_record;
    context.found = 0;
    result = TextFileRepository_for_each_data_line(
        &repository->storage, DispenseRecordRepository_find_by_id_line_handler, &context
    );
    if (!result.success) return result;
    if (!context.found) return Result_make_failure("dispense not found");
    return Result_make_success("dispense found");
}

/** 按处方ID查找发药记录列表 */
Result DispenseRecordRepository_find_by_prescription_id(
    const DispenseRecordRepository *repository, const char *prescription_id, LinkedList *out_records
) {
    DispenseRecordCollectByPrescriptionContext context;
    LinkedList matches;
    Result result;

    if (prescription_id == 0 || prescription_id[0] == '\0' || out_records == 0) {
        return Result_make_failure("dispense prescription query arguments invalid");
    }
    if (LinkedList_count(out_records) != 0) return Result_make_failure("output dispense list must be empty");

    result = DispenseRecordRepository_ensure_storage(repository);
    if (!result.success) return result;

    LinkedList_init(&matches);
    context.prescription_id = prescription_id;
    context.out_records = &matches;
    result = TextFileRepository_for_each_data_line(
        &repository->storage, DispenseRecordRepository_collect_by_prescription_line_handler, &context
    );
    if (!result.success) { DispenseRecordRepository_clear_list(&matches); return result; }

    *out_records = matches;
    return Result_make_success("dispense records loaded");
}

/** 按患者ID查找发药记录列表 */
Result DispenseRecordRepository_find_by_patient_id(
    const DispenseRecordRepository *repository, const char *patient_id, LinkedList *out_records
) {
    DispenseRecordCollectByPatientContext context;
    LinkedList matches;
    Result result;

    if (patient_id == 0 || patient_id[0] == '\0' || out_records == 0) {
        return Result_make_failure("dispense patient query arguments invalid");
    }
    if (LinkedList_count(out_records) != 0) return Result_make_failure("output dispense list must be empty");

    result = DispenseRecordRepository_ensure_storage(repository);
    if (!result.success) return result;

    LinkedList_init(&matches);
    context.patient_id = patient_id;
    context.out_records = &matches;
    result = TextFileRepository_for_each_data_line(
        &repository->storage, DispenseRecordRepository_collect_by_patient_line_handler, &context
    );
    if (!result.success) { DispenseRecordRepository_clear_list(&matches); return result; }

    *out_records = matches;
    return Result_make_success("dispense records loaded");
}

/** 加载所有发药记录到链表 */
Result DispenseRecordRepository_load_all(
    const DispenseRecordRepository *repository, LinkedList *out_records
) {
    DispenseRecordLoadContext context;
    Result result;

    if (repository == 0 || out_records == 0) return Result_make_failure("dispense load arguments invalid");
    if (LinkedList_count(out_records) != 0) return Result_make_failure("output dispense list must be empty");

    result = DispenseRecordRepository_ensure_storage(repository);
    if (!result.success) return result;

    context.out_records = out_records;
    result = TextFileRepository_for_each_data_line(
        &repository->storage, DispenseRecordRepository_load_line_handler, &context
    );
    if (!result.success) { DispenseRecordRepository_clear_list(out_records); return result; }
    return Result_make_success("dispense records loaded");
}

/** 全量保存发药记录列表到文件（覆盖写入） */
Result DispenseRecordRepository_save_all(
    const DispenseRecordRepository *repository, const LinkedList *records
) {
    LinkedListNode *current = 0;
    FILE *file = 0;
    Result result;

    if (repository == 0 || records == 0) return Result_make_failure("dispense save arguments invalid");
    result = TextFileRepository_ensure_file_exists(&repository->storage);
    if (!result.success) return result;

    file = fopen(repository->storage.path, "w");
    if (file == 0) return Result_make_failure("failed to rewrite dispense storage");

    if (fputs(DISPENSE_RECORD_REPOSITORY_HEADER, file) == EOF || fputc('\n', file) == EOF) {
        fclose(file);
        return Result_make_failure("failed to write dispense header");
    }

    current = records->head;
    while (current != 0) {
        char line[TEXT_FILE_REPOSITORY_LINE_CAPACITY];
        result = DispenseRecordRepository_serialize(
            (const DispenseRecord *)current->data, line, sizeof(line)
        );
        if (!result.success) { fclose(file); return result; }

        if (fputs(line, file) == EOF || fputc('\n', file) == EOF) {
            fclose(file);
            return Result_make_failure("failed to write dispense row");
        }
        current = current->next;
    }

    fclose(file);
    return Result_make_success("dispense records saved");
}

/** 清空并释放发药记录链表中的所有元素 */
void DispenseRecordRepository_clear_list(LinkedList *records) {
    LinkedList_clear(records, DispenseRecordRepository_free_record);
}
