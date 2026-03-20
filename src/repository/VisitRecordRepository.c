#include "repository/VisitRecordRepository.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "repository/RepositoryUtils.h"

static const char VISIT_RECORD_REPOSITORY_HEADER[] =
    "visit_id|registration_id|patient_id|doctor_id|department_id|chief_complaint|diagnosis|advice|need_exam|need_admission|need_medicine|visit_time";

typedef struct VisitRecordFindByIdContext {
    const char *visit_id;
    VisitRecord *out_record;
    int found;
} VisitRecordFindByIdContext;

typedef struct VisitRecordCollectByRegistrationContext {
    const char *registration_id;
    LinkedList *out_records;
} VisitRecordCollectByRegistrationContext;

static int VisitRecordRepository_is_empty_text(const char *text) {
    return text == 0 || text[0] == '\0';
}

static void VisitRecordRepository_copy_text(char *destination, size_t capacity, const char *source) {
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

static Result VisitRecordRepository_validate_text_field(
    const char *text,
    const char *field_name,
    int allow_empty
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

static Result VisitRecordRepository_validate(const VisitRecord *record) {
    Result result;

    if (record == 0) {
        return Result_make_failure("visit record missing");
    }

    result = VisitRecordRepository_validate_text_field(record->visit_id, "visit_id", 0);
    if (!result.success) {
        return result;
    }

    result = VisitRecordRepository_validate_text_field(
        record->registration_id,
        "registration_id",
        0
    );
    if (!result.success) {
        return result;
    }

    result = VisitRecordRepository_validate_text_field(record->patient_id, "patient_id", 0);
    if (!result.success) {
        return result;
    }

    result = VisitRecordRepository_validate_text_field(record->doctor_id, "doctor_id", 0);
    if (!result.success) {
        return result;
    }

    result = VisitRecordRepository_validate_text_field(
        record->department_id,
        "department_id",
        0
    );
    if (!result.success) {
        return result;
    }

    result = VisitRecordRepository_validate_text_field(
        record->chief_complaint,
        "chief_complaint",
        1
    );
    if (!result.success) {
        return result;
    }

    result = VisitRecordRepository_validate_text_field(record->diagnosis, "diagnosis", 1);
    if (!result.success) {
        return result;
    }

    result = VisitRecordRepository_validate_text_field(record->advice, "advice", 1);
    if (!result.success) {
        return result;
    }

    result = VisitRecordRepository_validate_text_field(record->visit_time, "visit_time", 0);
    if (!result.success) {
        return result;
    }

    if ((record->need_exam != 0 && record->need_exam != 1) ||
        (record->need_admission != 0 && record->need_admission != 1) ||
        (record->need_medicine != 0 && record->need_medicine != 1)) {
        return Result_make_failure("visit flags invalid");
    }

    return Result_make_success("visit record valid");
}

static void VisitRecordRepository_discard_rest_of_line(FILE *file) {
    int character = 0;

    while ((character = fgetc(file)) != EOF) {
        if (character == '\n') {
            return;
        }
    }
}

static Result VisitRecordRepository_write_header(const VisitRecordRepository *repository) {
    FILE *file = fopen(repository->storage.path, "w");

    if (file == 0) {
        return Result_make_failure("failed to rewrite visit storage");
    }

    if (fputs(VISIT_RECORD_REPOSITORY_HEADER, file) == EOF || fputc('\n', file) == EOF) {
        fclose(file);
        return Result_make_failure("failed to write visit header");
    }

    fclose(file);
    return Result_make_success("visit header ready");
}

static Result VisitRecordRepository_serialize(
    const VisitRecord *record,
    char *line,
    size_t capacity
) {
    int written = 0;
    Result result = VisitRecordRepository_validate(record);

    if (!result.success) {
        return result;
    }

    written = snprintf(
        line,
        capacity,
        "%s|%s|%s|%s|%s|%s|%s|%s|%d|%d|%d|%s",
        record->visit_id,
        record->registration_id,
        record->patient_id,
        record->doctor_id,
        record->department_id,
        record->chief_complaint,
        record->diagnosis,
        record->advice,
        record->need_exam,
        record->need_admission,
        record->need_medicine,
        record->visit_time
    );
    if (written < 0 || (size_t)written >= capacity) {
        return Result_make_failure("visit line too long");
    }

    return Result_make_success("visit serialized");
}

static Result VisitRecordRepository_parse_line(const char *line, VisitRecord *out_record) {
    char buffer[TEXT_FILE_REPOSITORY_LINE_CAPACITY];
    char *fields[VISIT_RECORD_REPOSITORY_FIELD_COUNT];
    size_t field_count = 0;
    Result result;

    if (line == 0 || out_record == 0) {
        return Result_make_failure("visit parse arguments invalid");
    }

    if (strlen(line) >= sizeof(buffer)) {
        return Result_make_failure("visit line too long");
    }

    strcpy(buffer, line);
    result = RepositoryUtils_split_pipe_line(
        buffer,
        fields,
        VISIT_RECORD_REPOSITORY_FIELD_COUNT,
        &field_count
    );
    if (!result.success) {
        return result;
    }

    result = RepositoryUtils_validate_field_count(field_count, VISIT_RECORD_REPOSITORY_FIELD_COUNT);
    if (!result.success) {
        return result;
    }

    memset(out_record, 0, sizeof(*out_record));
    VisitRecordRepository_copy_text(out_record->visit_id, sizeof(out_record->visit_id), fields[0]);
    VisitRecordRepository_copy_text(
        out_record->registration_id,
        sizeof(out_record->registration_id),
        fields[1]
    );
    VisitRecordRepository_copy_text(
        out_record->patient_id,
        sizeof(out_record->patient_id),
        fields[2]
    );
    VisitRecordRepository_copy_text(
        out_record->doctor_id,
        sizeof(out_record->doctor_id),
        fields[3]
    );
    VisitRecordRepository_copy_text(
        out_record->department_id,
        sizeof(out_record->department_id),
        fields[4]
    );
    VisitRecordRepository_copy_text(
        out_record->chief_complaint,
        sizeof(out_record->chief_complaint),
        fields[5]
    );
    VisitRecordRepository_copy_text(
        out_record->diagnosis,
        sizeof(out_record->diagnosis),
        fields[6]
    );
    VisitRecordRepository_copy_text(out_record->advice, sizeof(out_record->advice), fields[7]);

    result = VisitRecordRepository_parse_flag(fields[8], &out_record->need_exam);
    if (!result.success) {
        return result;
    }

    result = VisitRecordRepository_parse_flag(fields[9], &out_record->need_admission);
    if (!result.success) {
        return result;
    }

    result = VisitRecordRepository_parse_flag(fields[10], &out_record->need_medicine);
    if (!result.success) {
        return result;
    }

    VisitRecordRepository_copy_text(
        out_record->visit_time,
        sizeof(out_record->visit_time),
        fields[11]
    );
    return VisitRecordRepository_validate(out_record);
}

static void VisitRecordRepository_free_record(void *data) {
    free(data);
}

static Result VisitRecordRepository_find_by_id_line_handler(const char *line, void *context) {
    VisitRecordFindByIdContext *find_context = (VisitRecordFindByIdContext *)context;
    VisitRecord record;
    Result result = VisitRecordRepository_parse_line(line, &record);

    if (!result.success) {
        return result;
    }

    if (strcmp(record.visit_id, find_context->visit_id) != 0) {
        return Result_make_success("visit skipped");
    }

    if (find_context->found) {
        return Result_make_failure("duplicate visit id");
    }

    *find_context->out_record = record;
    find_context->found = 1;
    return Result_make_success("visit matched");
}

static Result VisitRecordRepository_collect_by_registration_line_handler(
    const char *line,
    void *context
) {
    VisitRecordCollectByRegistrationContext *collect_context =
        (VisitRecordCollectByRegistrationContext *)context;
    VisitRecord record;
    VisitRecord *copy = 0;
    Result result = VisitRecordRepository_parse_line(line, &record);

    if (!result.success) {
        return result;
    }

    if (strcmp(record.registration_id, collect_context->registration_id) != 0) {
        return Result_make_success("visit skipped");
    }

    copy = (VisitRecord *)malloc(sizeof(*copy));
    if (copy == 0) {
        return Result_make_failure("failed to allocate visit result");
    }

    *copy = record;
    if (!LinkedList_append(collect_context->out_records, copy)) {
        free(copy);
        return Result_make_failure("failed to append visit result");
    }

    return Result_make_success("visit collected");
}

Result VisitRecordRepository_init(VisitRecordRepository *repository, const char *path) {
    if (repository == 0) {
        return Result_make_failure("visit repository missing");
    }

    return TextFileRepository_init(&repository->storage, path);
}

Result VisitRecordRepository_ensure_storage(const VisitRecordRepository *repository) {
    char line[TEXT_FILE_REPOSITORY_LINE_CAPACITY];
    FILE *file = 0;
    Result result;

    if (repository == 0) {
        return Result_make_failure("visit repository missing");
    }

    result = TextFileRepository_ensure_file_exists(&repository->storage);
    if (!result.success) {
        return result;
    }

    file = fopen(repository->storage.path, "r");
    if (file == 0) {
        return Result_make_failure("failed to inspect visit storage");
    }

    while (fgets(line, sizeof(line), file) != 0) {
        if (strchr(line, '\n') == 0 && !feof(file)) {
            VisitRecordRepository_discard_rest_of_line(file);
            fclose(file);
            return Result_make_failure("visit header line too long");
        }

        RepositoryUtils_strip_line_endings(line);
        if (RepositoryUtils_is_blank_line(line)) {
            continue;
        }

        fclose(file);
        if (strcmp(line, VISIT_RECORD_REPOSITORY_HEADER) != 0) {
            return Result_make_failure("unexpected visit header");
        }

        return Result_make_success("visit storage ready");
    }

    fclose(file);
    return VisitRecordRepository_write_header(repository);
}

Result VisitRecordRepository_append(
    const VisitRecordRepository *repository,
    const VisitRecord *record
) {
    char line[TEXT_FILE_REPOSITORY_LINE_CAPACITY];
    Result result = VisitRecordRepository_ensure_storage(repository);

    if (!result.success) {
        return result;
    }

    result = VisitRecordRepository_serialize(record, line, sizeof(line));
    if (!result.success) {
        return result;
    }

    return TextFileRepository_append_line(&repository->storage, line);
}

Result VisitRecordRepository_find_by_visit_id(
    const VisitRecordRepository *repository,
    const char *visit_id,
    VisitRecord *out_record
) {
    VisitRecordFindByIdContext context;
    Result result;

    if (visit_id == 0 || visit_id[0] == '\0' || out_record == 0) {
        return Result_make_failure("visit query arguments invalid");
    }

    result = VisitRecordRepository_ensure_storage(repository);
    if (!result.success) {
        return result;
    }

    context.visit_id = visit_id;
    context.out_record = out_record;
    context.found = 0;
    result = TextFileRepository_for_each_data_line(
        &repository->storage,
        VisitRecordRepository_find_by_id_line_handler,
        &context
    );
    if (!result.success) {
        return result;
    }

    if (!context.found) {
        return Result_make_failure("visit not found");
    }

    return Result_make_success("visit found");
}

Result VisitRecordRepository_find_by_registration_id(
    const VisitRecordRepository *repository,
    const char *registration_id,
    LinkedList *out_records
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
    if (!result.success) {
        return result;
    }

    LinkedList_init(&matches);
    context.registration_id = registration_id;
    context.out_records = &matches;
    result = TextFileRepository_for_each_data_line(
        &repository->storage,
        VisitRecordRepository_collect_by_registration_line_handler,
        &context
    );
    if (!result.success) {
        VisitRecordRepository_clear_list(&matches);
        return result;
    }

    *out_records = matches;
    return Result_make_success("visit records loaded");
}

Result VisitRecordRepository_save_all(
    const VisitRecordRepository *repository,
    const LinkedList *records
) {
    LinkedListNode *current = 0;
    FILE *file = 0;
    Result result;

    if (repository == 0 || records == 0) {
        return Result_make_failure("visit save arguments invalid");
    }

    result = TextFileRepository_ensure_file_exists(&repository->storage);
    if (!result.success) {
        return result;
    }

    file = fopen(repository->storage.path, "w");
    if (file == 0) {
        return Result_make_failure("failed to rewrite visit storage");
    }

    if (fputs(VISIT_RECORD_REPOSITORY_HEADER, file) == EOF || fputc('\n', file) == EOF) {
        fclose(file);
        return Result_make_failure("failed to write visit header");
    }

    current = records->head;
    while (current != 0) {
        char line[TEXT_FILE_REPOSITORY_LINE_CAPACITY];

        result = VisitRecordRepository_serialize((const VisitRecord *)current->data, line, sizeof(line));
        if (!result.success) {
            fclose(file);
            return result;
        }

        if (fputs(line, file) == EOF || fputc('\n', file) == EOF) {
            fclose(file);
            return Result_make_failure("failed to write visit row");
        }

        current = current->next;
    }

    fclose(file);
    return Result_make_success("visit records saved");
}

void VisitRecordRepository_clear_list(LinkedList *records) {
    LinkedList_clear(records, VisitRecordRepository_free_record);
}
