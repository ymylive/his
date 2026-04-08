/**
 * @file VisitRecordRepository.c
 * @brief 就诊记录数据仓储层实现
 *
 * 实现就诊记录（VisitRecord）数据的持久化存储操作，包括：
 * - 就诊记录的序列化与反序列化（12个字段）
 * - 数据校验（必填字段、布尔标志值域、保留字符检测）
 * - 追加、按就诊ID查找、按挂号ID筛选、全量加载、全量保存
 */

#include "repository/VisitRecordRepository.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "repository/RepositoryUtils.h"

/** 就诊记录数据文件的表头行 */
static const char VISIT_RECORD_REPOSITORY_HEADER[] =
    "visit_id|registration_id|patient_id|doctor_id|department_id|chief_complaint|diagnosis|advice|need_exam|need_admission|need_medicine|visit_time";

/** 按就诊ID查找时使用的上下文结构体 */
typedef struct VisitRecordFindByIdContext {
    const char *visit_id;   /**< 要查找的就诊ID */
    VisitRecord *out_record; /**< 输出：找到的就诊记录 */
    int found;               /**< 是否已找到标志 */
} VisitRecordFindByIdContext;

/** 按挂号ID收集就诊记录时使用的上下文结构体 */
typedef struct VisitRecordCollectByRegistrationContext {
    const char *registration_id; /**< 要匹配的挂号ID */
    LinkedList *out_records;     /**< 输出：匹配的就诊记录链表 */
} VisitRecordCollectByRegistrationContext;

/** 加载所有就诊记录时使用的上下文结构体 */
typedef struct VisitRecordLoadContext {
    LinkedList *out_records; /**< 用于存放加载结果的链表 */
} VisitRecordLoadContext;

/** 判断文本是否为空 */
static int VisitRecordRepository_is_empty_text(const char *text) {
    return text == 0 || text[0] == '\0';
}

/** 安全复制字符串 */
static void VisitRecordRepository_copy_text(char *destination, size_t capacity, const char *source) {
    if (destination == 0 || capacity == 0) return;
    if (source == 0) { destination[0] = '\0'; return; }
    strncpy(destination, source, capacity - 1);
    destination[capacity - 1] = '\0';
}

/** 校验单个文本字段 */
static Result VisitRecordRepository_validate_text_field(
    const char *text, const char *field_name, int allow_empty
) {
    char message[RESULT_MESSAGE_CAPACITY];

    if (text == 0 || (!allow_empty && VisitRecordRepository_is_empty_text(text))) {
        snprintf(message, sizeof(message), "%s missing", field_name);
        return Result_make_failure(message);
    }
    if (!RepositoryUtils_is_safe_field_text(text)) {
        snprintf(message, sizeof(message), "%s contains reserved characters", field_name);
        return Result_make_failure(message);
    }
    return Result_make_success("visit text field ok");
}

/**
 * @brief 解析布尔标志字段（只允许 0 或 1）
 * @param field     文本字段
 * @param out_value 输出参数，解析得到的标志值
 * @return 成功返回 success；值不为 0 或 1 时返回 failure
 */
static Result VisitRecordRepository_parse_flag(const char *field, int *out_value) {
    char *end = 0;
    long value = 0;

    if (field == 0 || out_value == 0 || field[0] == '\0') {
        return Result_make_failure("visit flag missing");
    }
    value = strtol(field, &end, 10);
    if (end == 0 || *end != '\0' || (value != 0 && value != 1)) {
        return Result_make_failure("visit flag invalid");
    }
    *out_value = (int)value;
    return Result_make_success("visit flag parsed");
}

/**
 * @brief 校验就诊记录的合法性
 *
 * 检查必填/可选字段、保留字符，以及三个布尔标志（need_exam、need_admission、need_medicine）。
 */
static Result VisitRecordRepository_validate(const VisitRecord *record) {
    Result result;
    if (record == 0) return Result_make_failure("visit record missing");

    result = VisitRecordRepository_validate_text_field(record->visit_id, "visit_id", 0);
    if (!result.success) return result;

    result = VisitRecordRepository_validate_text_field(record->registration_id, "registration_id", 0);
    if (!result.success) return result;

    result = VisitRecordRepository_validate_text_field(record->patient_id, "patient_id", 0);
    if (!result.success) return result;

    result = VisitRecordRepository_validate_text_field(record->doctor_id, "doctor_id", 0);
    if (!result.success) return result;

    result = VisitRecordRepository_validate_text_field(record->department_id, "department_id", 0);
    if (!result.success) return result;

    /* 主诉、诊断、建议为可选字段 */
    result = VisitRecordRepository_validate_text_field(record->chief_complaint, "chief_complaint", 1);
    if (!result.success) return result;

    result = VisitRecordRepository_validate_text_field(record->diagnosis, "diagnosis", 1);
    if (!result.success) return result;

    result = VisitRecordRepository_validate_text_field(record->advice, "advice", 1);
    if (!result.success) return result;

    result = VisitRecordRepository_validate_text_field(record->visit_time, "visit_time", 0);
    if (!result.success) return result;

    /* 校验三个布尔标志 */
    if ((record->need_exam != 0 && record->need_exam != 1) ||
        (record->need_admission != 0 && record->need_admission != 1) ||
        (record->need_medicine != 0 && record->need_medicine != 1)) {
        return Result_make_failure("visit flags invalid");
    }

    return Result_make_success("visit record valid");
}

/** 丢弃当前行剩余字符 */
static void VisitRecordRepository_discard_rest_of_line(FILE *file) {
    int character = 0;
    while ((character = fgetc(file)) != EOF) {
        if (character == '\n') return;
    }
}

/** 向文件写入就诊记录表头（覆盖写入） */
static Result VisitRecordRepository_write_header(const VisitRecordRepository *repository) {
    FILE *file = fopen(repository->storage.path, "w");
    if (file == 0) return Result_make_failure("failed to rewrite visit storage");

    if (fputs(VISIT_RECORD_REPOSITORY_HEADER, file) == EOF || fputc('\n', file) == EOF) {
        fclose(file);
        return Result_make_failure("failed to write visit header");
    }
    fclose(file);
    return Result_make_success("visit header ready");
}

/** 将就诊记录序列化为管道符分隔的文本行 */
static Result VisitRecordRepository_serialize(
    const VisitRecord *record, char *line, size_t capacity
) {
    int written = 0;
    Result result = VisitRecordRepository_validate(record);
    if (!result.success) return result;

    written = snprintf(line, capacity, "%s|%s|%s|%s|%s|%s|%s|%s|%d|%d|%d|%s",
        record->visit_id, record->registration_id, record->patient_id,
        record->doctor_id, record->department_id, record->chief_complaint,
        record->diagnosis, record->advice, record->need_exam,
        record->need_admission, record->need_medicine, record->visit_time);
    if (written < 0 || (size_t)written >= capacity) {
        return Result_make_failure("visit line too long");
    }
    return Result_make_success("visit serialized");
}

/** 将一行文本解析为就诊记录结构体 */
static Result VisitRecordRepository_parse_line(const char *line, VisitRecord *out_record) {
    char buffer[TEXT_FILE_REPOSITORY_LINE_CAPACITY];
    char *fields[VISIT_RECORD_REPOSITORY_FIELD_COUNT];
    size_t field_count = 0;
    Result result;

    if (line == 0 || out_record == 0) return Result_make_failure("visit parse arguments invalid");
    if (strlen(line) >= sizeof(buffer)) return Result_make_failure("visit line too long");

    strcpy(buffer, line);
    result = RepositoryUtils_split_pipe_line(buffer, fields, VISIT_RECORD_REPOSITORY_FIELD_COUNT, &field_count);
    if (!result.success) return result;

    result = RepositoryUtils_validate_field_count(field_count, VISIT_RECORD_REPOSITORY_FIELD_COUNT);
    if (!result.success) return result;

    /* 逐字段解析 */
    memset(out_record, 0, sizeof(*out_record));
    VisitRecordRepository_copy_text(out_record->visit_id, sizeof(out_record->visit_id), fields[0]);
    VisitRecordRepository_copy_text(out_record->registration_id, sizeof(out_record->registration_id), fields[1]);
    VisitRecordRepository_copy_text(out_record->patient_id, sizeof(out_record->patient_id), fields[2]);
    VisitRecordRepository_copy_text(out_record->doctor_id, sizeof(out_record->doctor_id), fields[3]);
    VisitRecordRepository_copy_text(out_record->department_id, sizeof(out_record->department_id), fields[4]);
    VisitRecordRepository_copy_text(out_record->chief_complaint, sizeof(out_record->chief_complaint), fields[5]);
    VisitRecordRepository_copy_text(out_record->diagnosis, sizeof(out_record->diagnosis), fields[6]);
    VisitRecordRepository_copy_text(out_record->advice, sizeof(out_record->advice), fields[7]);

    /* 解析三个布尔标志 */
    result = VisitRecordRepository_parse_flag(fields[8], &out_record->need_exam);
    if (!result.success) return result;
    result = VisitRecordRepository_parse_flag(fields[9], &out_record->need_admission);
    if (!result.success) return result;
    result = VisitRecordRepository_parse_flag(fields[10], &out_record->need_medicine);
    if (!result.success) return result;

    VisitRecordRepository_copy_text(out_record->visit_time, sizeof(out_record->visit_time), fields[11]);
    return VisitRecordRepository_validate(out_record);
}

/** 释放就诊记录 */
static void VisitRecordRepository_free_record(void *data) {
    free(data);
}

/** 按就诊ID查找的行处理回调 */
static Result VisitRecordRepository_find_by_id_line_handler(const char *line, void *context) {
    VisitRecordFindByIdContext *find_context = (VisitRecordFindByIdContext *)context;
    VisitRecord record;
    Result result = VisitRecordRepository_parse_line(line, &record);
    if (!result.success) return result;

    if (strcmp(record.visit_id, find_context->visit_id) != 0) {
        return Result_make_success("visit skipped");
    }
    if (find_context->found) return Result_make_failure("duplicate visit id");

    *find_context->out_record = record;
    find_context->found = 1;
    return Result_make_success("visit matched");
}

/** 按挂号ID收集就诊记录的行处理回调 */
static Result VisitRecordRepository_collect_by_registration_line_handler(
    const char *line, void *context
) {
    VisitRecordCollectByRegistrationContext *collect_context =
        (VisitRecordCollectByRegistrationContext *)context;
    VisitRecord record;
    VisitRecord *copy = 0;
    Result result = VisitRecordRepository_parse_line(line, &record);
    if (!result.success) return result;

    if (strcmp(record.registration_id, collect_context->registration_id) != 0) {
        return Result_make_success("visit skipped");
    }

    copy = (VisitRecord *)malloc(sizeof(*copy));
    if (copy == 0) return Result_make_failure("failed to allocate visit result");
    *copy = record;
    if (!LinkedList_append(collect_context->out_records, copy)) {
        free(copy);
        return Result_make_failure("failed to append visit result");
    }
    return Result_make_success("visit collected");
}

/** 加载所有就诊记录的行处理回调 */
static Result VisitRecordRepository_load_line_handler(const char *line, void *context) {
    VisitRecordLoadContext *load_context = (VisitRecordLoadContext *)context;
    VisitRecord record;
    VisitRecord *copy = 0;
    Result result = VisitRecordRepository_parse_line(line, &record);
    if (!result.success) return result;

    if (load_context == 0 || load_context->out_records == 0) {
        return Result_make_failure("visit load context missing");
    }

    copy = (VisitRecord *)malloc(sizeof(*copy));
    if (copy == 0) return Result_make_failure("failed to allocate visit result");
    *copy = record;
    if (!LinkedList_append(load_context->out_records, copy)) {
        free(copy);
        return Result_make_failure("failed to append visit result");
    }
    return Result_make_success("visit loaded");
}

/** 初始化就诊记录仓储 */
Result VisitRecordRepository_init(VisitRecordRepository *repository, const char *path) {
    if (repository == 0) return Result_make_failure("visit repository missing");
    return TextFileRepository_init(&repository->storage, path);
}

/** 确保存储文件已就绪（含表头校验/写入） */
Result VisitRecordRepository_ensure_storage(const VisitRecordRepository *repository) {
    char line[TEXT_FILE_REPOSITORY_LINE_CAPACITY];
    FILE *file = 0;
    Result result;

    if (repository == 0) return Result_make_failure("visit repository missing");

    result = TextFileRepository_ensure_file_exists(&repository->storage);
    if (!result.success) return result;

    file = fopen(repository->storage.path, "r");
    if (file == 0) return Result_make_failure("failed to inspect visit storage");

    while (fgets(line, sizeof(line), file) != 0) {
        if (strchr(line, '\n') == 0 && !feof(file)) {
            VisitRecordRepository_discard_rest_of_line(file);
            fclose(file);
            return Result_make_failure("visit header line too long");
        }
        RepositoryUtils_strip_line_endings(line);
        if (RepositoryUtils_is_blank_line(line)) continue;

        fclose(file);
        if (strcmp(line, VISIT_RECORD_REPOSITORY_HEADER) != 0) {
            return Result_make_failure("unexpected visit header");
        }
        return Result_make_success("visit storage ready");
    }

    fclose(file);
    return VisitRecordRepository_write_header(repository);
}

/** 追加一条就诊记录到文件 */
Result VisitRecordRepository_append(
    const VisitRecordRepository *repository, const VisitRecord *record
) {
    char line[TEXT_FILE_REPOSITORY_LINE_CAPACITY];
    Result result = VisitRecordRepository_ensure_storage(repository);
    if (!result.success) return result;

    result = VisitRecordRepository_serialize(record, line, sizeof(line));
    if (!result.success) return result;

    return TextFileRepository_append_line(&repository->storage, line);
}

/** 按就诊ID查找就诊记录 */
Result VisitRecordRepository_find_by_visit_id(
    const VisitRecordRepository *repository, const char *visit_id, VisitRecord *out_record
) {
    VisitRecordFindByIdContext context;
    Result result;

    if (visit_id == 0 || visit_id[0] == '\0' || out_record == 0) {
        return Result_make_failure("visit query arguments invalid");
    }

    result = VisitRecordRepository_ensure_storage(repository);
    if (!result.success) return result;

    context.visit_id = visit_id;
    context.out_record = out_record;
    context.found = 0;
    result = TextFileRepository_for_each_data_line(
        &repository->storage, VisitRecordRepository_find_by_id_line_handler, &context
    );
    if (!result.success) return result;
    if (!context.found) return Result_make_failure("visit not found");

    return Result_make_success("visit found");
}

/** 按挂号ID查找关联的就诊记录列表 */
Result VisitRecordRepository_find_by_registration_id(
    const VisitRecordRepository *repository, const char *registration_id, LinkedList *out_records
) {
    VisitRecordCollectByRegistrationContext context;
    LinkedList matches;
    Result result;

    if (registration_id == 0 || registration_id[0] == '\0' || out_records == 0) {
        return Result_make_failure("visit registration query arguments invalid");
    }
    if (LinkedList_count(out_records) != 0) {
        return Result_make_failure("output visit list must be empty");
    }

    result = VisitRecordRepository_ensure_storage(repository);
    if (!result.success) return result;

    LinkedList_init(&matches);
    context.registration_id = registration_id;
    context.out_records = &matches;
    result = TextFileRepository_for_each_data_line(
        &repository->storage, VisitRecordRepository_collect_by_registration_line_handler, &context
    );
    if (!result.success) {
        VisitRecordRepository_clear_list(&matches);
        return result;
    }

    *out_records = matches;
    return Result_make_success("visit records loaded");
}

/** 加载所有就诊记录到链表 */
Result VisitRecordRepository_load_all(
    const VisitRecordRepository *repository, LinkedList *out_records
) {
    VisitRecordLoadContext context;
    LinkedList loaded;
    Result result;

    if (out_records == 0) return Result_make_failure("visit output list missing");
    if (LinkedList_count(out_records) != 0) {
        return Result_make_failure("output visit list must be empty");
    }

    LinkedList_init(&loaded);
    result = VisitRecordRepository_ensure_storage(repository);
    if (!result.success) return result;

    context.out_records = &loaded;
    result = TextFileRepository_for_each_data_line(
        &repository->storage, VisitRecordRepository_load_line_handler, &context
    );
    if (!result.success) {
        VisitRecordRepository_clear_list(&loaded);
        return result;
    }

    *out_records = loaded;
    return Result_make_success("visits loaded");
}

/** 全量保存就诊记录列表到文件（覆盖写入） */
Result VisitRecordRepository_save_all(
    const VisitRecordRepository *repository, const LinkedList *records
) {
    LinkedListNode *current = 0;
    FILE *file = 0;
    Result result;

    if (repository == 0 || records == 0) {
        return Result_make_failure("visit save arguments invalid");
    }

    result = TextFileRepository_ensure_file_exists(&repository->storage);
    if (!result.success) return result;

    file = fopen(repository->storage.path, "w");
    if (file == 0) return Result_make_failure("failed to rewrite visit storage");

    if (fputs(VISIT_RECORD_REPOSITORY_HEADER, file) == EOF || fputc('\n', file) == EOF) {
        fclose(file);
        return Result_make_failure("failed to write visit header");
    }

    current = records->head;
    while (current != 0) {
        char line[TEXT_FILE_REPOSITORY_LINE_CAPACITY];
        result = VisitRecordRepository_serialize((const VisitRecord *)current->data, line, sizeof(line));
        if (!result.success) { fclose(file); return result; }

        if (fputs(line, file) == EOF || fputc('\n', file) == EOF) {
            fclose(file);
            return Result_make_failure("failed to write visit row");
        }
        current = current->next;
    }

    fclose(file);
    return Result_make_success("visit records saved");
}

/** 清空并释放就诊记录链表中的所有元素 */
void VisitRecordRepository_clear_list(LinkedList *records) {
    LinkedList_clear(records, VisitRecordRepository_free_record);
}
