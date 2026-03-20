#include "repository/ExaminationRecordRepository.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "repository/RepositoryUtils.h"

static const char EXAMINATION_RECORD_REPOSITORY_HEADER[] =
    "examination_id|visit_id|patient_id|doctor_id|exam_item|exam_type|status|result|requested_at|completed_at";

typedef struct ExaminationRecordFindByIdContext {
    const char *examination_id;
    ExaminationRecord *out_record;
    int found;
} ExaminationRecordFindByIdContext;

typedef struct ExaminationRecordCollectByVisitContext {
    const char *visit_id;
    LinkedList *out_records;
} ExaminationRecordCollectByVisitContext;

typedef struct ExaminationRecordLoadContext {
    LinkedList *out_records;
} ExaminationRecordLoadContext;

static int ExaminationRecordRepository_is_empty_text(const char *text) {
    return text == 0 || text[0] == '\0';
}

static void ExaminationRecordRepository_copy_text(
    char *destination,
    size_t capacity,
    const char *source
) {
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

static Result ExaminationRecordRepository_validate_text_field(
    const char *text,
    const char *field_name,
    int allow_empty
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

static Result ExaminationRecordRepository_parse_status(
    const char *field,
    ExaminationStatus *out_status
) {
    char *end = 0;
    long value = 0;

    if (field == 0 || out_status == 0 || field[0] == '\0') {
        return Result_make_failure("exam status missing");
    }

    value = strtol(field, &end, 10);
    if (end == 0 || *end != '\0') {
        return Result_make_failure("exam status invalid");
    }

    if (value < EXAM_STATUS_PENDING || value > EXAM_STATUS_COMPLETED) {
        return Result_make_failure("exam status out of range");
    }

    *out_status = (ExaminationStatus)value;
    return Result_make_success("exam status parsed");
}

static Result ExaminationRecordRepository_validate(const ExaminationRecord *record) {
    Result result;

    if (record == 0) {
        return Result_make_failure("exam record missing");
    }

    result = ExaminationRecordRepository_validate_text_field(
        record->examination_id,
        "examination_id",
        0
    );
    if (!result.success) {
        return result;
    }

    result = ExaminationRecordRepository_validate_text_field(record->visit_id, "visit_id", 0);
    if (!result.success) {
        return result;
    }

    result = ExaminationRecordRepository_validate_text_field(
        record->patient_id,
        "patient_id",
        0
    );
    if (!result.success) {
        return result;
    }

    result = ExaminationRecordRepository_validate_text_field(
        record->doctor_id,
        "doctor_id",
        0
    );
    if (!result.success) {
        return result;
    }

    result = ExaminationRecordRepository_validate_text_field(record->exam_item, "exam_item", 0);
    if (!result.success) {
        return result;
    }

    result = ExaminationRecordRepository_validate_text_field(record->exam_type, "exam_type", 0);
    if (!result.success) {
        return result;
    }

    result = ExaminationRecordRepository_validate_text_field(record->result, "result", 1);
    if (!result.success) {
        return result;
    }

    result = ExaminationRecordRepository_validate_text_field(
        record->requested_at,
        "requested_at",
        0
    );
    if (!result.success) {
        return result;
    }

    result = ExaminationRecordRepository_validate_text_field(
        record->completed_at,
        "completed_at",
        1
    );
    if (!result.success) {
        return result;
    }

    if (record->status == EXAM_STATUS_PENDING) {
        if (!ExaminationRecordRepository_is_empty_text(record->completed_at)) {
            return Result_make_failure("pending exam cannot have completed_at");
        }
    } else if (record->status == EXAM_STATUS_COMPLETED) {
        if (ExaminationRecordRepository_is_empty_text(record->completed_at)) {
            return Result_make_failure("completed exam missing completed_at");
        }
    } else {
        return Result_make_failure("exam status invalid");
    }

    return Result_make_success("exam record valid");
}

static void ExaminationRecordRepository_discard_rest_of_line(FILE *file) {
    int character = 0;

    while ((character = fgetc(file)) != EOF) {
        if (character == '\n') {
            return;
        }
    }
}

static Result ExaminationRecordRepository_write_header(
    const ExaminationRecordRepository *repository
) {
    FILE *file = fopen(repository->storage.path, "w");

    if (file == 0) {
        return Result_make_failure("failed to rewrite exam storage");
    }

    if (fputs(EXAMINATION_RECORD_REPOSITORY_HEADER, file) == EOF || fputc('\n', file) == EOF) {
        fclose(file);
        return Result_make_failure("failed to write exam header");
    }

    fclose(file);
    return Result_make_success("exam header ready");
}

static Result ExaminationRecordRepository_serialize(
    const ExaminationRecord *record,
    char *line,
    size_t capacity
) {
    int written = 0;
    Result result = ExaminationRecordRepository_validate(record);

    if (!result.success) {
        return result;
    }

    written = snprintf(
        line,
        capacity,
        "%s|%s|%s|%s|%s|%s|%d|%s|%s|%s",
        record->examination_id,
        record->visit_id,
        record->patient_id,
        record->doctor_id,
        record->exam_item,
        record->exam_type,
        (int)record->status,
        record->result,
        record->requested_at,
        record->completed_at
    );
    if (written < 0 || (size_t)written >= capacity) {
        return Result_make_failure("exam line too long");
    }

    return Result_make_success("exam serialized");
}

static Result ExaminationRecordRepository_parse_line(
    const char *line,
    ExaminationRecord *out_record
) {
    char buffer[TEXT_FILE_REPOSITORY_LINE_CAPACITY];
    char *fields[EXAMINATION_RECORD_REPOSITORY_FIELD_COUNT];
    size_t field_count = 0;
    Result result;

    if (line == 0 || out_record == 0) {
        return Result_make_failure("exam parse arguments invalid");
    }

    if (strlen(line) >= sizeof(buffer)) {
        return Result_make_failure("exam line too long");
    }

    strcpy(buffer, line);
    result = RepositoryUtils_split_pipe_line(
        buffer,
        fields,
        EXAMINATION_RECORD_REPOSITORY_FIELD_COUNT,
        &field_count
    );
    if (!result.success) {
        return result;
    }

    result = RepositoryUtils_validate_field_count(
        field_count,
        EXAMINATION_RECORD_REPOSITORY_FIELD_COUNT
    );
    if (!result.success) {
        return result;
    }

    memset(out_record, 0, sizeof(*out_record));
    ExaminationRecordRepository_copy_text(
        out_record->examination_id,
        sizeof(out_record->examination_id),
        fields[0]
    );
    ExaminationRecordRepository_copy_text(
        out_record->visit_id,
        sizeof(out_record->visit_id),
        fields[1]
    );
    ExaminationRecordRepository_copy_text(
        out_record->patient_id,
        sizeof(out_record->patient_id),
        fields[2]
    );
    ExaminationRecordRepository_copy_text(
        out_record->doctor_id,
        sizeof(out_record->doctor_id),
        fields[3]
    );
    ExaminationRecordRepository_copy_text(
        out_record->exam_item,
        sizeof(out_record->exam_item),
        fields[4]
    );
    ExaminationRecordRepository_copy_text(
        out_record->exam_type,
        sizeof(out_record->exam_type),
        fields[5]
    );

    result = ExaminationRecordRepository_parse_status(fields[6], &out_record->status);
    if (!result.success) {
        return result;
    }

    ExaminationRecordRepository_copy_text(
        out_record->result,
        sizeof(out_record->result),
        fields[7]
    );
    ExaminationRecordRepository_copy_text(
        out_record->requested_at,
        sizeof(out_record->requested_at),
        fields[8]
    );
    ExaminationRecordRepository_copy_text(
        out_record->completed_at,
        sizeof(out_record->completed_at),
        fields[9]
    );
    return ExaminationRecordRepository_validate(out_record);
}

static void ExaminationRecordRepository_free_record(void *data) {
    free(data);
}

static Result ExaminationRecordRepository_find_by_id_line_handler(
    const char *line,
    void *context
) {
    ExaminationRecordFindByIdContext *find_context =
        (ExaminationRecordFindByIdContext *)context;
    ExaminationRecord record;
    Result result = ExaminationRecordRepository_parse_line(line, &record);

    if (!result.success) {
        return result;
    }

    if (strcmp(record.examination_id, find_context->examination_id) != 0) {
        return Result_make_success("exam skipped");
    }

    if (find_context->found) {
        return Result_make_failure("duplicate examination id");
    }

    *find_context->out_record = record;
    find_context->found = 1;
    return Result_make_success("exam matched");
}

static Result ExaminationRecordRepository_collect_by_visit_line_handler(
    const char *line,
    void *context
) {
    ExaminationRecordCollectByVisitContext *collect_context =
        (ExaminationRecordCollectByVisitContext *)context;
    ExaminationRecord record;
    ExaminationRecord *copy = 0;
    Result result = ExaminationRecordRepository_parse_line(line, &record);

    if (!result.success) {
        return result;
    }

    if (strcmp(record.visit_id, collect_context->visit_id) != 0) {
        return Result_make_success("exam skipped");
    }

    copy = (ExaminationRecord *)malloc(sizeof(*copy));
    if (copy == 0) {
        return Result_make_failure("failed to allocate exam result");
    }

    *copy = record;
    if (!LinkedList_append(collect_context->out_records, copy)) {
        free(copy);
        return Result_make_failure("failed to append exam result");
    }

    return Result_make_success("exam collected");
}

static Result ExaminationRecordRepository_load_line_handler(
    const char *line,
    void *context
) {
    ExaminationRecordLoadContext *load_context = (ExaminationRecordLoadContext *)context;
    ExaminationRecord record;
    ExaminationRecord *copy = 0;
    Result result = ExaminationRecordRepository_parse_line(line, &record);

    if (!result.success) {
        return result;
    }

    if (load_context == 0 || load_context->out_records == 0) {
        return Result_make_failure("exam load context missing");
    }

    copy = (ExaminationRecord *)malloc(sizeof(*copy));
    if (copy == 0) {
        return Result_make_failure("failed to allocate exam result");
    }

    *copy = record;
    if (!LinkedList_append(load_context->out_records, copy)) {
        free(copy);
        return Result_make_failure("failed to append exam result");
    }

    return Result_make_success("exam loaded");
}

Result ExaminationRecordRepository_init(
    ExaminationRecordRepository *repository,
    const char *path
) {
    if (repository == 0) {
        return Result_make_failure("exam repository missing");
    }

    return TextFileRepository_init(&repository->storage, path);
}

Result ExaminationRecordRepository_ensure_storage(
    const ExaminationRecordRepository *repository
) {
    char line[TEXT_FILE_REPOSITORY_LINE_CAPACITY];
    FILE *file = 0;
    Result result;

    if (repository == 0) {
        return Result_make_failure("exam repository missing");
    }

    result = TextFileRepository_ensure_file_exists(&repository->storage);
    if (!result.success) {
        return result;
    }

    file = fopen(repository->storage.path, "r");
    if (file == 0) {
        return Result_make_failure("failed to inspect exam storage");
    }

    while (fgets(line, sizeof(line), file) != 0) {
        if (strchr(line, '\n') == 0 && !feof(file)) {
            ExaminationRecordRepository_discard_rest_of_line(file);
            fclose(file);
            return Result_make_failure("exam header line too long");
        }

        RepositoryUtils_strip_line_endings(line);
        if (RepositoryUtils_is_blank_line(line)) {
            continue;
        }

        fclose(file);
        if (strcmp(line, EXAMINATION_RECORD_REPOSITORY_HEADER) != 0) {
            return Result_make_failure("unexpected exam header");
        }

        return Result_make_success("exam storage ready");
    }

    fclose(file);
    return ExaminationRecordRepository_write_header(repository);
}

Result ExaminationRecordRepository_append(
    const ExaminationRecordRepository *repository,
    const ExaminationRecord *record
) {
    char line[TEXT_FILE_REPOSITORY_LINE_CAPACITY];
    Result result = ExaminationRecordRepository_ensure_storage(repository);

    if (!result.success) {
        return result;
    }

    result = ExaminationRecordRepository_serialize(record, line, sizeof(line));
    if (!result.success) {
        return result;
    }

    return TextFileRepository_append_line(&repository->storage, line);
}

Result ExaminationRecordRepository_find_by_examination_id(
    const ExaminationRecordRepository *repository,
    const char *examination_id,
    ExaminationRecord *out_record
) {
    ExaminationRecordFindByIdContext context;
    Result result;

    if (examination_id == 0 || examination_id[0] == '\0' || out_record == 0) {
        return Result_make_failure("exam query arguments invalid");
    }

    result = ExaminationRecordRepository_ensure_storage(repository);
    if (!result.success) {
        return result;
    }

    context.examination_id = examination_id;
    context.out_record = out_record;
    context.found = 0;
    result = TextFileRepository_for_each_data_line(
        &repository->storage,
        ExaminationRecordRepository_find_by_id_line_handler,
        &context
    );
    if (!result.success) {
        return result;
    }

    if (!context.found) {
        return Result_make_failure("exam not found");
    }

    return Result_make_success("exam found");
}

Result ExaminationRecordRepository_find_by_visit_id(
    const ExaminationRecordRepository *repository,
    const char *visit_id,
    LinkedList *out_records
) {
    ExaminationRecordCollectByVisitContext context;
    LinkedList matches;
    Result result;

    if (visit_id == 0 || visit_id[0] == '\0' || out_records == 0) {
        return Result_make_failure("exam visit query arguments invalid");
    }

    if (LinkedList_count(out_records) != 0) {
        return Result_make_failure("output exam list must be empty");
    }

    result = ExaminationRecordRepository_ensure_storage(repository);
    if (!result.success) {
        return result;
    }

    LinkedList_init(&matches);
    context.visit_id = visit_id;
    context.out_records = &matches;
    result = TextFileRepository_for_each_data_line(
        &repository->storage,
        ExaminationRecordRepository_collect_by_visit_line_handler,
        &context
    );
    if (!result.success) {
        ExaminationRecordRepository_clear_list(&matches);
        return result;
    }

    *out_records = matches;
    return Result_make_success("exam records loaded");
}

Result ExaminationRecordRepository_load_all(
    const ExaminationRecordRepository *repository,
    LinkedList *out_records
) {
    ExaminationRecordLoadContext context;
    LinkedList loaded;
    Result result;

    if (out_records == 0) {
        return Result_make_failure("exam output list missing");
    }

    if (LinkedList_count(out_records) != 0) {
        return Result_make_failure("output exam list must be empty");
    }

    LinkedList_init(&loaded);
    result = ExaminationRecordRepository_ensure_storage(repository);
    if (!result.success) {
        return result;
    }

    context.out_records = &loaded;
    result = TextFileRepository_for_each_data_line(
        &repository->storage,
        ExaminationRecordRepository_load_line_handler,
        &context
    );
    if (!result.success) {
        ExaminationRecordRepository_clear_list(&loaded);
        return result;
    }

    *out_records = loaded;
    return Result_make_success("exam records loaded");
}

Result ExaminationRecordRepository_save_all(
    const ExaminationRecordRepository *repository,
    const LinkedList *records
) {
    LinkedListNode *current = 0;
    FILE *file = 0;
    Result result;

    if (repository == 0 || records == 0) {
        return Result_make_failure("exam save arguments invalid");
    }

    result = TextFileRepository_ensure_file_exists(&repository->storage);
    if (!result.success) {
        return result;
    }

    file = fopen(repository->storage.path, "w");
    if (file == 0) {
        return Result_make_failure("failed to rewrite exam storage");
    }

    if (fputs(EXAMINATION_RECORD_REPOSITORY_HEADER, file) == EOF || fputc('\n', file) == EOF) {
        fclose(file);
        return Result_make_failure("failed to write exam header");
    }

    current = records->head;
    while (current != 0) {
        char line[TEXT_FILE_REPOSITORY_LINE_CAPACITY];

        result = ExaminationRecordRepository_serialize(
            (const ExaminationRecord *)current->data,
            line,
            sizeof(line)
        );
        if (!result.success) {
            fclose(file);
            return result;
        }

        if (fputs(line, file) == EOF || fputc('\n', file) == EOF) {
            fclose(file);
            return Result_make_failure("failed to write exam row");
        }

        current = current->next;
    }

    fclose(file);
    return Result_make_success("exam records saved");
}

void ExaminationRecordRepository_clear_list(LinkedList *records) {
    LinkedList_clear(records, ExaminationRecordRepository_free_record);
}
