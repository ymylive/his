/**
 * @file PrescriptionRepository.c
 * @brief 处方数据仓储层实现
 *
 * 实现处方（Prescription）数据的持久化存储操作，包括：
 * - 处方的序列化与反序列化
 * - 数据校验（必填字段检查）
 * - 追加、按处方ID查找、按就诊ID筛选、全量加载、全量保存
 */

#include "repository/PrescriptionRepository.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "repository/RepositoryUtils.h"

/** 处方数据文件的表头行 */
static const char PRESCRIPTION_REPOSITORY_HEADER[] =
    "prescription_id|visit_id|medicine_id|quantity|usage";

/** 按处方ID查找时使用的上下文 */
typedef struct PrescriptionFindByIdContext {
    const char *prescription_id;
    Prescription *out_prescription;
    int found;
} PrescriptionFindByIdContext;

/** 按就诊ID收集时使用的上下文 */
typedef struct PrescriptionCollectByVisitContext {
    const char *visit_id;
    LinkedList *out_prescriptions;
} PrescriptionCollectByVisitContext;

/** 加载所有处方时使用的上下文 */
typedef struct PrescriptionLoadContext {
    LinkedList *out_prescriptions;
} PrescriptionLoadContext;

/** 判断文本是否为空 */
static int PrescriptionRepository_is_empty_text(const char *text) {
    return text == 0 || text[0] == '\0';
}

/** 安全复制字符串 */
static void PrescriptionRepository_copy_text(
    char *destination, size_t capacity, const char *source
) {
    if (destination == 0 || capacity == 0) return;
    if (source == 0) { destination[0] = '\0'; return; }
    strncpy(destination, source, capacity - 1);
    destination[capacity - 1] = '\0';
}

/** 校验单个文本字段 */
static Result PrescriptionRepository_validate_text_field(
    const char *text, const char *field_name, int allow_empty
) {
    char message[RESULT_MESSAGE_CAPACITY];
    if (text == 0 || (!allow_empty && PrescriptionRepository_is_empty_text(text))) {
        snprintf(message, sizeof(message), "%s missing", field_name);
        return Result_make_failure(message);
    }
    if (!RepositoryUtils_is_safe_field_text(text)) {
        snprintf(message, sizeof(message), "%s contains reserved characters", field_name);
        return Result_make_failure(message);
    }
    return Result_make_success("prescription text field ok");
}

/** 解析整数字段 */
static Result PrescriptionRepository_parse_int(const char *field, int *out_value) {
    char *end = 0;
    long value = 0;
    if (field == 0 || out_value == 0 || field[0] == '\0') return Result_make_failure("prescription integer missing");
    value = strtol(field, &end, 10);
    if (end == 0 || *end != '\0') return Result_make_failure("prescription integer invalid");
    *out_value = (int)value;
    return Result_make_success("prescription integer parsed");
}

/**
 * @brief 校验处方记录的合法性
 */
static Result PrescriptionRepository_validate(const Prescription *prescription) {
    Result result;
    if (prescription == 0) return Result_make_failure("prescription missing");

    result = PrescriptionRepository_validate_text_field(prescription->prescription_id, "prescription_id", 0);
    if (!result.success) return result;
    result = PrescriptionRepository_validate_text_field(prescription->visit_id, "visit_id", 0);
    if (!result.success) return result;
    result = PrescriptionRepository_validate_text_field(prescription->medicine_id, "medicine_id", 0);
    if (!result.success) return result;

    if (prescription->quantity <= 0) return Result_make_failure("prescription quantity invalid");

    result = PrescriptionRepository_validate_text_field(prescription->usage, "usage", 1);
    if (!result.success) return result;

    return Result_make_success("prescription valid");
}

/** 丢弃行剩余字符 */
static void PrescriptionRepository_discard_rest_of_line(FILE *file) {
    int character = 0;
    while ((character = fgetc(file)) != EOF) {
        if (character == '\n') return;
    }
}

/** 写入表头（覆盖） */
static Result PrescriptionRepository_write_header(const PrescriptionRepository *repository) {
    FILE *file = fopen(repository->storage.path, "w");
    if (file == 0) return Result_make_failure("failed to rewrite prescription storage");

    if (fputs(PRESCRIPTION_REPOSITORY_HEADER, file) == EOF || fputc('\n', file) == EOF) {
        fclose(file);
        return Result_make_failure("failed to write prescription header");
    }
    fclose(file);
    return Result_make_success("prescription header ready");
}

/** 将处方记录序列化为文本行 */
static Result PrescriptionRepository_serialize(
    const Prescription *prescription, char *line, size_t capacity
) {
    int written = 0;
    Result result = PrescriptionRepository_validate(prescription);
    if (!result.success) return result;

    written = snprintf(line, capacity, "%s|%s|%s|%d|%s",
        prescription->prescription_id, prescription->visit_id,
        prescription->medicine_id, prescription->quantity,
        prescription->usage);
    if (written < 0 || (size_t)written >= capacity) return Result_make_failure("prescription line too long");
    return Result_make_success("prescription serialized");
}

/** 将一行文本解析为处方结构体 */
static Result PrescriptionRepository_parse_line(const char *line, Prescription *out_prescription) {
    char buffer[TEXT_FILE_REPOSITORY_LINE_CAPACITY];
    char *fields[PRESCRIPTION_REPOSITORY_FIELD_COUNT];
    size_t field_count = 0;
    Result result;

    if (line == 0 || out_prescription == 0) return Result_make_failure("prescription parse arguments invalid");
    if (strlen(line) >= sizeof(buffer)) return Result_make_failure("prescription line too long");

    strncpy(buffer, line, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';
    result = RepositoryUtils_split_pipe_line(buffer, fields, PRESCRIPTION_REPOSITORY_FIELD_COUNT, &field_count);
    if (!result.success) return result;
    result = RepositoryUtils_validate_field_count(field_count, PRESCRIPTION_REPOSITORY_FIELD_COUNT);
    if (!result.success) return result;

    memset(out_prescription, 0, sizeof(*out_prescription));
    PrescriptionRepository_copy_text(out_prescription->prescription_id, sizeof(out_prescription->prescription_id), fields[0]);
    PrescriptionRepository_copy_text(out_prescription->visit_id, sizeof(out_prescription->visit_id), fields[1]);
    PrescriptionRepository_copy_text(out_prescription->medicine_id, sizeof(out_prescription->medicine_id), fields[2]);

    result = PrescriptionRepository_parse_int(fields[3], &out_prescription->quantity);
    if (!result.success) return result;

    PrescriptionRepository_copy_text(out_prescription->usage, sizeof(out_prescription->usage), fields[4]);

    return PrescriptionRepository_validate(out_prescription);
}

/** 释放处方记录 */
static void PrescriptionRepository_free_record(void *data) { free(data); }

/** 按处方ID查找的行处理回调 */
static Result PrescriptionRepository_find_by_id_line_handler(const char *line, void *context) {
    PrescriptionFindByIdContext *find_context = (PrescriptionFindByIdContext *)context;
    Prescription prescription;
    Result result = PrescriptionRepository_parse_line(line, &prescription);
    if (!result.success) return result;

    if (strcmp(prescription.prescription_id, find_context->prescription_id) != 0) return Result_make_success("prescription skipped");
    if (find_context->found) return Result_make_failure("duplicate prescription id");

    *find_context->out_prescription = prescription;
    find_context->found = 1;
    return Result_make_success("prescription matched");
}

/** 按就诊ID收集的行处理回调 */
static Result PrescriptionRepository_collect_by_visit_line_handler(const char *line, void *context) {
    PrescriptionCollectByVisitContext *collect_context =
        (PrescriptionCollectByVisitContext *)context;
    Prescription prescription;
    Prescription *copy = 0;
    Result result = PrescriptionRepository_parse_line(line, &prescription);
    if (!result.success) return result;

    if (strcmp(prescription.visit_id, collect_context->visit_id) != 0) {
        return Result_make_success("prescription skipped");
    }

    copy = (Prescription *)malloc(sizeof(*copy));
    if (copy == 0) return Result_make_failure("failed to allocate prescription result");
    *copy = prescription;
    if (!LinkedList_append(collect_context->out_prescriptions, copy)) {
        free(copy);
        return Result_make_failure("failed to append prescription result");
    }
    return Result_make_success("prescription collected");
}

/** 加载所有处方的行处理回调 */
static Result PrescriptionRepository_load_line_handler(const char *line, void *context) {
    PrescriptionLoadContext *load_context = (PrescriptionLoadContext *)context;
    Prescription prescription;
    Prescription *copy = 0;
    Result result = PrescriptionRepository_parse_line(line, &prescription);
    if (!result.success) return result;

    copy = (Prescription *)malloc(sizeof(*copy));
    if (copy == 0) return Result_make_failure("failed to allocate prescription result");
    *copy = prescription;
    if (!LinkedList_append(load_context->out_prescriptions, copy)) {
        free(copy);
        return Result_make_failure("failed to append prescription result");
    }
    return Result_make_success("prescription loaded");
}

/** 初始化处方仓储 */
Result PrescriptionRepository_init(PrescriptionRepository *repository, const char *path) {
    if (repository == 0) return Result_make_failure("prescription repository missing");
    return TextFileRepository_init(&repository->storage, path);
}

/** 确保存储文件已就绪 */
Result PrescriptionRepository_ensure_storage(const PrescriptionRepository *repository) {
    char line[TEXT_FILE_REPOSITORY_LINE_CAPACITY];
    FILE *file = 0;
    Result result;

    if (repository == 0) return Result_make_failure("prescription repository missing");

    result = TextFileRepository_ensure_file_exists(&repository->storage);
    if (!result.success) return result;

    file = fopen(repository->storage.path, "r");
    if (file == 0) return Result_make_failure("failed to inspect prescription storage");

    while (fgets(line, sizeof(line), file) != 0) {
        if (strchr(line, '\n') == 0 && !feof(file)) {
            PrescriptionRepository_discard_rest_of_line(file);
            fclose(file);
            return Result_make_failure("prescription header line too long");
        }
        RepositoryUtils_strip_line_endings(line);
        if (RepositoryUtils_is_blank_line(line)) continue;
        fclose(file);
        if (strcmp(line, PRESCRIPTION_REPOSITORY_HEADER) != 0) {
            return Result_make_failure("unexpected prescription header");
        }
        return Result_make_success("prescription storage ready");
    }

    fclose(file);
    return PrescriptionRepository_write_header(repository);
}

/** 追加一条处方记录 */
Result PrescriptionRepository_append(
    const PrescriptionRepository *repository, const Prescription *prescription
) {
    char line[TEXT_FILE_REPOSITORY_LINE_CAPACITY];
    Result result = PrescriptionRepository_ensure_storage(repository);
    if (!result.success) return result;
    result = PrescriptionRepository_serialize(prescription, line, sizeof(line));
    if (!result.success) return result;
    return TextFileRepository_append_line(&repository->storage, line);
}

/** 按处方ID查找 */
Result PrescriptionRepository_find_by_id(
    const PrescriptionRepository *repository, const char *prescription_id, Prescription *out_prescription
) {
    PrescriptionFindByIdContext context;
    Result result;

    if (prescription_id == 0 || prescription_id[0] == '\0' || out_prescription == 0) {
        return Result_make_failure("prescription query arguments invalid");
    }
    result = PrescriptionRepository_ensure_storage(repository);
    if (!result.success) return result;

    context.prescription_id = prescription_id;
    context.out_prescription = out_prescription;
    context.found = 0;
    result = TextFileRepository_for_each_data_line(
        &repository->storage, PrescriptionRepository_find_by_id_line_handler, &context
    );
    if (!result.success) return result;
    if (!context.found) return Result_make_failure("prescription not found");
    return Result_make_success("prescription found");
}

/** 按就诊ID查找处方列表 */
Result PrescriptionRepository_find_by_visit_id(
    const PrescriptionRepository *repository, const char *visit_id, LinkedList *out_prescriptions
) {
    PrescriptionCollectByVisitContext context;
    LinkedList matches;
    Result result;

    if (visit_id == 0 || visit_id[0] == '\0' || out_prescriptions == 0) {
        return Result_make_failure("prescription visit query arguments invalid");
    }
    if (LinkedList_count(out_prescriptions) != 0) return Result_make_failure("output prescription list must be empty");

    result = PrescriptionRepository_ensure_storage(repository);
    if (!result.success) return result;

    LinkedList_init(&matches);
    context.visit_id = visit_id;
    context.out_prescriptions = &matches;
    result = TextFileRepository_for_each_data_line(
        &repository->storage, PrescriptionRepository_collect_by_visit_line_handler, &context
    );
    if (!result.success) { PrescriptionRepository_clear_list(&matches); return result; }

    *out_prescriptions = matches;
    return Result_make_success("prescriptions loaded");
}

/** 加载所有处方到链表 */
Result PrescriptionRepository_load_all(
    const PrescriptionRepository *repository, LinkedList *out_prescriptions
) {
    PrescriptionLoadContext context;
    Result result;

    if (repository == 0 || out_prescriptions == 0) return Result_make_failure("prescription load arguments invalid");
    if (LinkedList_count(out_prescriptions) != 0) return Result_make_failure("output prescription list must be empty");

    result = PrescriptionRepository_ensure_storage(repository);
    if (!result.success) return result;

    context.out_prescriptions = out_prescriptions;
    result = TextFileRepository_for_each_data_line(
        &repository->storage, PrescriptionRepository_load_line_handler, &context
    );
    if (!result.success) { PrescriptionRepository_clear_list(out_prescriptions); return result; }
    return Result_make_success("prescriptions loaded");
}

/** 全量保存处方列表到文件（覆盖写入） */
Result PrescriptionRepository_save_all(
    const PrescriptionRepository *repository, const LinkedList *prescriptions
) {
    LinkedListNode *current = 0;
    FILE *file = 0;
    Result result;

    if (repository == 0 || prescriptions == 0) return Result_make_failure("prescription save arguments invalid");
    result = TextFileRepository_ensure_file_exists(&repository->storage);
    if (!result.success) return result;

    file = fopen(repository->storage.path, "w");
    if (file == 0) return Result_make_failure("failed to rewrite prescription storage");

    if (fputs(PRESCRIPTION_REPOSITORY_HEADER, file) == EOF || fputc('\n', file) == EOF) {
        fclose(file);
        return Result_make_failure("failed to write prescription header");
    }

    current = prescriptions->head;
    while (current != 0) {
        char line[TEXT_FILE_REPOSITORY_LINE_CAPACITY];
        result = PrescriptionRepository_serialize(
            (const Prescription *)current->data, line, sizeof(line)
        );
        if (!result.success) { fclose(file); return result; }

        if (fputs(line, file) == EOF || fputc('\n', file) == EOF) {
            fclose(file);
            return Result_make_failure("failed to write prescription row");
        }
        current = current->next;
    }

    fclose(file);
    return Result_make_success("prescriptions saved");
}

/** 清空并释放处方链表中的所有元素 */
void PrescriptionRepository_clear_list(LinkedList *prescriptions) {
    LinkedList_clear(prescriptions, PrescriptionRepository_free_record);
}
