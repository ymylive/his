#include "repository/DispenseRecordRepository.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "repository/RepositoryUtils.h"

static const char DISPENSE_RECORD_REPOSITORY_HEADER[] =
    "dispense_id|prescription_id|medicine_id|quantity|pharmacist_id|dispensed_at|status";

typedef struct DispenseRecordFindByIdContext {
    const char *dispense_id;
    DispenseRecord *out_record;
    int found;
} DispenseRecordFindByIdContext;

typedef struct DispenseRecordCollectByPrescriptionContext {
    const char *prescription_id;
    LinkedList *out_records;
} DispenseRecordCollectByPrescriptionContext;

static int DispenseRecordRepository_is_empty_text(const char *text) {
    return text == 0 || text[0] == '\0';
}

static void DispenseRecordRepository_copy_text(
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

static Result DispenseRecordRepository_validate_text_field(
    const char *text,
    const char *field_name,
    int allow_empty
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

static Result DispenseRecordRepository_parse_int(const char *field, int *out_value) {
    char *end = 0;
    long value = 0;

    if (field == 0 || out_value == 0 || field[0] == '\0') {
        return Result_make_failure("dispense integer missing");
    }

    value = strtol(field, &end, 10);
    if (end == 0 || *end != '\0') {
        return Result_make_failure("dispense integer invalid");
    }

    *out_value = (int)value;
    return Result_make_success("dispense integer parsed");
}

static Result DispenseRecordRepository_parse_status(
    const char *field,
    DispenseStatus *out_status
) {
    int value = 0;
    Result result = DispenseRecordRepository_parse_int(field, &value);

    if (!result.success) {
        return result;
    }

    if (value < DISPENSE_STATUS_PENDING || value > DISPENSE_STATUS_COMPLETED) {
        return Result_make_failure("dispense status out of range");
    }

    *out_status = (DispenseStatus)value;
    return Result_make_success("dispense status parsed");
}

static Result DispenseRecordRepository_validate(const DispenseRecord *record) {
    Result result;

    if (record == 0) {
        return Result_make_failure("dispense record missing");
    }

    result = DispenseRecordRepository_validate_text_field(record->dispense_id, "dispense_id", 0);
    if (!result.success) {
        return result;
    }

    result = DispenseRecordRepository_validate_text_field(
        record->prescription_id,
        "prescription_id",
        0
    );
    if (!result.success) {
        return result;
    }

    result = DispenseRecordRepository_validate_text_field(record->medicine_id, "medicine_id", 0);
    if (!result.success) {
        return result;
    }

    if (record->quantity <= 0) {
        return Result_make_failure("dispense quantity invalid");
    }

    result = DispenseRecordRepository_validate_text_field(
        record->pharmacist_id,
        "pharmacist_id",
        record->status == DISPENSE_STATUS_PENDING
    );
    if (!result.success) {
        return result;
    }

    result = DispenseRecordRepository_validate_text_field(
        record->dispensed_at,
        "dispensed_at",
        record->status == DISPENSE_STATUS_PENDING
    );
    if (!result.success) {
        return result;
    }

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

static void DispenseRecordRepository_discard_rest_of_line(FILE *file) {
    int character = 0;

    while ((character = fgetc(file)) != EOF) {
        if (character == '\n') {
            return;
        }
    }
}

static Result DispenseRecordRepository_write_header(
    const DispenseRecordRepository *repository
) {
    FILE *file = fopen(repository->storage.path, "w");

    if (file == 0) {
        return Result_make_failure("failed to rewrite dispense storage");
    }

    if (fputs(DISPENSE_RECORD_REPOSITORY_HEADER, file) == EOF || fputc('\n', file) == EOF) {
        fclose(file);
        return Result_make_failure("failed to write dispense header");
    }

    fclose(file);
    return Result_make_success("dispense header ready");
}

static Result DispenseRecordRepository_serialize(
    const DispenseRecord *record,
    char *line,
    size_t capacity
) {
    int written = 0;
    Result result = DispenseRecordRepository_validate(record);

    if (!result.success) {
        return result;
    }

    written = snprintf(
        line,
        capacity,
        "%s|%s|%s|%d|%s|%s|%d",
        record->dispense_id,
        record->prescription_id,
        record->medicine_id,
        record->quantity,
        record->pharmacist_id,
        record->dispensed_at,
        (int)record->status
    );
    if (written < 0 || (size_t)written >= capacity) {
        return Result_make_failure("dispense line too long");
    }

    return Result_make_success("dispense serialized");
}

static Result DispenseRecordRepository_parse_line(
    const char *line,
    DispenseRecord *out_record
) {
    char buffer[TEXT_FILE_REPOSITORY_LINE_CAPACITY];
    char *fields[DISPENSE_RECORD_REPOSITORY_FIELD_COUNT];
    size_t field_count = 0;
    Result result;

    if (line == 0 || out_record == 0) {
        return Result_make_failure("dispense parse arguments invalid");
    }

    if (strlen(line) >= sizeof(buffer)) {
        return Result_make_failure("dispense line too long");
    }

    strcpy(buffer, line);
    result = RepositoryUtils_split_pipe_line(
        buffer,
        fields,
        DISPENSE_RECORD_REPOSITORY_FIELD_COUNT,
        &field_count
    );
    if (!result.success) {
        return result;
    }

    result = RepositoryUtils_validate_field_count(field_count, DISPENSE_RECORD_REPOSITORY_FIELD_COUNT);
    if (!result.success) {
        return result;
    }

    memset(out_record, 0, sizeof(*out_record));
    DispenseRecordRepository_copy_text(
        out_record->dispense_id,
        sizeof(out_record->dispense_id),
        fields[0]
    );
    DispenseRecordRepository_copy_text(
        out_record->prescription_id,
        sizeof(out_record->prescription_id),
        fields[1]
    );
    DispenseRecordRepository_copy_text(
        out_record->medicine_id,
        sizeof(out_record->medicine_id),
        fields[2]
    );

    result = DispenseRecordRepository_parse_int(fields[3], &out_record->quantity);
    if (!result.success) {
        return result;
    }

    DispenseRecordRepository_copy_text(
        out_record->pharmacist_id,
        sizeof(out_record->pharmacist_id),
        fields[4]
    );
    DispenseRecordRepository_copy_text(
        out_record->dispensed_at,
        sizeof(out_record->dispensed_at),
        fields[5]
    );

    result = DispenseRecordRepository_parse_status(fields[6], &out_record->status);
    if (!result.success) {
        return result;
    }

    return DispenseRecordRepository_validate(out_record);
}

static void DispenseRecordRepository_free_record(void *data) {
    free(data);
}

static Result DispenseRecordRepository_find_by_id_line_handler(
    const char *line,
    void *context
) {
    DispenseRecordFindByIdContext *find_context = (DispenseRecordFindByIdContext *)context;
    DispenseRecord record;
    Result result = DispenseRecordRepository_parse_line(line, &record);

    if (!result.success) {
        return result;
    }

    if (strcmp(record.dispense_id, find_context->dispense_id) != 0) {
        return Result_make_success("dispense skipped");
    }

    if (find_context->found) {
        return Result_make_failure("duplicate dispense id");
    }

    *find_context->out_record = record;
    find_context->found = 1;
    return Result_make_success("dispense matched");
}

static Result DispenseRecordRepository_collect_by_prescription_line_handler(
    const char *line,
    void *context
) {
    DispenseRecordCollectByPrescriptionContext *collect_context =
        (DispenseRecordCollectByPrescriptionContext *)context;
    DispenseRecord record;
    DispenseRecord *copy = 0;
    Result result = DispenseRecordRepository_parse_line(line, &record);

    if (!result.success) {
        return result;
    }

    if (strcmp(record.prescription_id, collect_context->prescription_id) != 0) {
        return Result_make_success("dispense skipped");
    }

    copy = (DispenseRecord *)malloc(sizeof(*copy));
    if (copy == 0) {
        return Result_make_failure("failed to allocate dispense result");
    }

    *copy = record;
    if (!LinkedList_append(collect_context->out_records, copy)) {
        free(copy);
        return Result_make_failure("failed to append dispense result");
    }

    return Result_make_success("dispense collected");
}

Result DispenseRecordRepository_init(DispenseRecordRepository *repository, const char *path) {
    if (repository == 0) {
        return Result_make_failure("dispense repository missing");
    }

    return TextFileRepository_init(&repository->storage, path);
}

Result DispenseRecordRepository_ensure_storage(
    const DispenseRecordRepository *repository
) {
    char line[TEXT_FILE_REPOSITORY_LINE_CAPACITY];
    FILE *file = 0;
    Result result;

    if (repository == 0) {
        return Result_make_failure("dispense repository missing");
    }

    result = TextFileRepository_ensure_file_exists(&repository->storage);
    if (!result.success) {
        return result;
    }

    file = fopen(repository->storage.path, "r");
    if (file == 0) {
        return Result_make_failure("failed to inspect dispense storage");
    }

    while (fgets(line, sizeof(line), file) != 0) {
        if (strchr(line, '\n') == 0 && !feof(file)) {
            DispenseRecordRepository_discard_rest_of_line(file);
            fclose(file);
            return Result_make_failure("dispense header line too long");
        }

        RepositoryUtils_strip_line_endings(line);
        if (RepositoryUtils_is_blank_line(line)) {
            continue;
        }

        fclose(file);
        if (strcmp(line, DISPENSE_RECORD_REPOSITORY_HEADER) != 0) {
            return Result_make_failure("unexpected dispense header");
        }

        return Result_make_success("dispense storage ready");
    }

    fclose(file);
    return DispenseRecordRepository_write_header(repository);
}

Result DispenseRecordRepository_append(
    const DispenseRecordRepository *repository,
    const DispenseRecord *record
) {
    char line[TEXT_FILE_REPOSITORY_LINE_CAPACITY];
    Result result = DispenseRecordRepository_ensure_storage(repository);

    if (!result.success) {
        return result;
    }

    result = DispenseRecordRepository_serialize(record, line, sizeof(line));
    if (!result.success) {
        return result;
    }

    return TextFileRepository_append_line(&repository->storage, line);
}

Result DispenseRecordRepository_find_by_dispense_id(
    const DispenseRecordRepository *repository,
    const char *dispense_id,
    DispenseRecord *out_record
) {
    DispenseRecordFindByIdContext context;
    Result result;

    if (dispense_id == 0 || dispense_id[0] == '\0' || out_record == 0) {
        return Result_make_failure("dispense query arguments invalid");
    }

    result = DispenseRecordRepository_ensure_storage(repository);
    if (!result.success) {
        return result;
    }

    context.dispense_id = dispense_id;
    context.out_record = out_record;
    context.found = 0;
    result = TextFileRepository_for_each_data_line(
        &repository->storage,
        DispenseRecordRepository_find_by_id_line_handler,
        &context
    );
    if (!result.success) {
        return result;
    }

    if (!context.found) {
        return Result_make_failure("dispense not found");
    }

    return Result_make_success("dispense found");
}

Result DispenseRecordRepository_find_by_prescription_id(
    const DispenseRecordRepository *repository,
    const char *prescription_id,
    LinkedList *out_records
) {
    DispenseRecordCollectByPrescriptionContext context;
    LinkedList matches;
    Result result;

    if (prescription_id == 0 || prescription_id[0] == '\0' || out_records == 0) {
        return Result_make_failure("dispense prescription query arguments invalid");
    }

    if (LinkedList_count(out_records) != 0) {
        return Result_make_failure("output dispense list must be empty");
    }

    result = DispenseRecordRepository_ensure_storage(repository);
    if (!result.success) {
        return result;
    }

    LinkedList_init(&matches);
    context.prescription_id = prescription_id;
    context.out_records = &matches;
    result = TextFileRepository_for_each_data_line(
        &repository->storage,
        DispenseRecordRepository_collect_by_prescription_line_handler,
        &context
    );
    if (!result.success) {
        DispenseRecordRepository_clear_list(&matches);
        return result;
    }

    *out_records = matches;
    return Result_make_success("dispense records loaded");
}

Result DispenseRecordRepository_save_all(
    const DispenseRecordRepository *repository,
    const LinkedList *records
) {
    LinkedListNode *current = 0;
    FILE *file = 0;
    Result result;

    if (repository == 0 || records == 0) {
        return Result_make_failure("dispense save arguments invalid");
    }

    result = TextFileRepository_ensure_file_exists(&repository->storage);
    if (!result.success) {
        return result;
    }

    file = fopen(repository->storage.path, "w");
    if (file == 0) {
        return Result_make_failure("failed to rewrite dispense storage");
    }

    if (fputs(DISPENSE_RECORD_REPOSITORY_HEADER, file) == EOF || fputc('\n', file) == EOF) {
        fclose(file);
        return Result_make_failure("failed to write dispense header");
    }

    current = records->head;
    while (current != 0) {
        char line[TEXT_FILE_REPOSITORY_LINE_CAPACITY];

        result = DispenseRecordRepository_serialize(
            (const DispenseRecord *)current->data,
            line,
            sizeof(line)
        );
        if (!result.success) {
            fclose(file);
            return result;
        }

        if (fputs(line, file) == EOF || fputc('\n', file) == EOF) {
            fclose(file);
            return Result_make_failure("failed to write dispense row");
        }

        current = current->next;
    }

    fclose(file);
    return Result_make_success("dispense records saved");
}

void DispenseRecordRepository_clear_list(LinkedList *records) {
    LinkedList_clear(records, DispenseRecordRepository_free_record);
}
