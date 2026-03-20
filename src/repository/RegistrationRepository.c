#include "repository/RegistrationRepository.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "repository/RepositoryUtils.h"

static const char REGISTRATION_REPOSITORY_HEADER[] =
    "registration_id|patient_id|doctor_id|department_id|registered_at|status|diagnosed_at|cancelled_at";

typedef struct RegistrationFindByIdContext {
    const char *registration_id;
    Registration *out_registration;
    int found;
} RegistrationFindByIdContext;

typedef struct RegistrationCollectByPatientContext {
    const char *patient_id;
    const RegistrationRepositoryFilter *filter;
    LinkedList *out_registrations;
} RegistrationCollectByPatientContext;

static int RegistrationRepository_is_empty_text(const char *text) {
    return text == 0 || text[0] == '\0';
}

static void RegistrationRepository_copy_text(char *destination, size_t capacity, const char *source) {
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

static Result RegistrationRepository_make_field_failure(const char *field_name, const char *suffix) {
    char message[RESULT_MESSAGE_CAPACITY];

    snprintf(message, sizeof(message), "%s %s", field_name, suffix);
    return Result_make_failure(message);
}

static Result RegistrationRepository_validate_text_field(
    const char *text,
    const char *field_name,
    int allow_empty
) {
    if (text == 0) {
        return RegistrationRepository_make_field_failure(field_name, "missing");
    }

    if (!allow_empty && RegistrationRepository_is_empty_text(text)) {
        return RegistrationRepository_make_field_failure(field_name, "missing");
    }

    if (!RepositoryUtils_is_safe_field_text(text)) {
        return RegistrationRepository_make_field_failure(field_name, "contains reserved characters");
    }

    return Result_make_success("text field ok");
}

static Result RegistrationRepository_parse_status(
    const char *field,
    RegistrationStatus *out_status
) {
    char *end = 0;
    long value = 0;

    if (field == 0 || out_status == 0 || field[0] == '\0') {
        return Result_make_failure("registration status missing");
    }

    value = strtol(field, &end, 10);
    if (end == 0 || *end != '\0') {
        return Result_make_failure("registration status invalid");
    }

    if (value < REG_STATUS_PENDING || value > REG_STATUS_CANCELLED) {
        return Result_make_failure("registration status out of range");
    }

    *out_status = (RegistrationStatus)value;
    return Result_make_success("registration status parsed");
}

static Result RegistrationRepository_validate(const Registration *registration) {
    Result result;

    if (registration == 0) {
        return Result_make_failure("registration missing");
    }

    result = RegistrationRepository_validate_text_field(
        registration->registration_id,
        "registration_id",
        0
    );
    if (!result.success) {
        return result;
    }

    result = RegistrationRepository_validate_text_field(
        registration->patient_id,
        "patient_id",
        0
    );
    if (!result.success) {
        return result;
    }

    result = RegistrationRepository_validate_text_field(
        registration->doctor_id,
        "doctor_id",
        0
    );
    if (!result.success) {
        return result;
    }

    result = RegistrationRepository_validate_text_field(
        registration->department_id,
        "department_id",
        0
    );
    if (!result.success) {
        return result;
    }

    result = RegistrationRepository_validate_text_field(
        registration->registered_at,
        "registered_at",
        0
    );
    if (!result.success) {
        return result;
    }

    result = RegistrationRepository_validate_text_field(
        registration->diagnosed_at,
        "diagnosed_at",
        1
    );
    if (!result.success) {
        return result;
    }

    result = RegistrationRepository_validate_text_field(
        registration->cancelled_at,
        "cancelled_at",
        1
    );
    if (!result.success) {
        return result;
    }

    if (registration->status == REG_STATUS_PENDING) {
        if (!RegistrationRepository_is_empty_text(registration->diagnosed_at) ||
            !RegistrationRepository_is_empty_text(registration->cancelled_at)) {
            return Result_make_failure("pending registration cannot have closed timestamps");
        }
    } else if (registration->status == REG_STATUS_DIAGNOSED) {
        if (RegistrationRepository_is_empty_text(registration->diagnosed_at) ||
            !RegistrationRepository_is_empty_text(registration->cancelled_at)) {
            return Result_make_failure("diagnosed registration timestamps invalid");
        }
    } else if (registration->status == REG_STATUS_CANCELLED) {
        if (RegistrationRepository_is_empty_text(registration->cancelled_at) ||
            !RegistrationRepository_is_empty_text(registration->diagnosed_at)) {
            return Result_make_failure("cancelled registration timestamps invalid");
        }
    } else {
        return Result_make_failure("registration status invalid");
    }

    return Result_make_success("registration valid");
}

static void RegistrationRepository_discard_rest_of_line(FILE *file) {
    int character = 0;

    while ((character = fgetc(file)) != EOF) {
        if (character == '\n') {
            return;
        }
    }
}

static Result RegistrationRepository_write_header(const RegistrationRepository *repository) {
    FILE *file = 0;

    if (repository == 0) {
        return Result_make_failure("registration repository missing");
    }

    file = fopen(repository->storage.path, "w");
    if (file == 0) {
        return Result_make_failure("failed to rewrite registration storage");
    }

    if (fputs(REGISTRATION_REPOSITORY_HEADER, file) == EOF || fputc('\n', file) == EOF) {
        fclose(file);
        return Result_make_failure("failed to write registration header");
    }

    fclose(file);
    return Result_make_success("registration header ready");
}

static Result RegistrationRepository_serialize(
    const Registration *registration,
    char *line,
    size_t capacity
) {
    int written = 0;
    Result result = RegistrationRepository_validate(registration);

    if (!result.success) {
        return result;
    }

    if (line == 0 || capacity == 0) {
        return Result_make_failure("registration line buffer missing");
    }

    written = snprintf(
        line,
        capacity,
        "%s|%s|%s|%s|%s|%d|%s|%s",
        registration->registration_id,
        registration->patient_id,
        registration->doctor_id,
        registration->department_id,
        registration->registered_at,
        (int)registration->status,
        registration->diagnosed_at,
        registration->cancelled_at
    );
    if (written < 0 || (size_t)written >= capacity) {
        return Result_make_failure("registration line too long");
    }

    return Result_make_success("registration serialized");
}

static Result RegistrationRepository_parse_line(const char *line, Registration *out_registration) {
    char buffer[TEXT_FILE_REPOSITORY_LINE_CAPACITY];
    char *fields[REGISTRATION_REPOSITORY_FIELD_COUNT];
    size_t field_count = 0;
    Result result;

    if (line == 0 || out_registration == 0) {
        return Result_make_failure("registration parse arguments invalid");
    }

    if (strlen(line) >= sizeof(buffer)) {
        return Result_make_failure("registration line too long");
    }

    strcpy(buffer, line);
    result = RepositoryUtils_split_pipe_line(
        buffer,
        fields,
        REGISTRATION_REPOSITORY_FIELD_COUNT,
        &field_count
    );
    if (!result.success) {
        return result;
    }

    result = RepositoryUtils_validate_field_count(
        field_count,
        REGISTRATION_REPOSITORY_FIELD_COUNT
    );
    if (!result.success) {
        return result;
    }

    memset(out_registration, 0, sizeof(*out_registration));
    RegistrationRepository_copy_text(
        out_registration->registration_id,
        sizeof(out_registration->registration_id),
        fields[0]
    );
    RegistrationRepository_copy_text(
        out_registration->patient_id,
        sizeof(out_registration->patient_id),
        fields[1]
    );
    RegistrationRepository_copy_text(
        out_registration->doctor_id,
        sizeof(out_registration->doctor_id),
        fields[2]
    );
    RegistrationRepository_copy_text(
        out_registration->department_id,
        sizeof(out_registration->department_id),
        fields[3]
    );
    RegistrationRepository_copy_text(
        out_registration->registered_at,
        sizeof(out_registration->registered_at),
        fields[4]
    );

    result = RegistrationRepository_parse_status(fields[5], &out_registration->status);
    if (!result.success) {
        return result;
    }

    RegistrationRepository_copy_text(
        out_registration->diagnosed_at,
        sizeof(out_registration->diagnosed_at),
        fields[6]
    );
    RegistrationRepository_copy_text(
        out_registration->cancelled_at,
        sizeof(out_registration->cancelled_at),
        fields[7]
    );

    return RegistrationRepository_validate(out_registration);
}

static int RegistrationRepository_is_in_time_range(
    const char *value,
    const char *from,
    const char *to
) {
    if (value == 0) {
        return 0;
    }

    if (from != 0 && from[0] != '\0' && strcmp(value, from) < 0) {
        return 0;
    }

    if (to != 0 && to[0] != '\0' && strcmp(value, to) > 0) {
        return 0;
    }

    return 1;
}

static int RegistrationRepository_matches_patient_filter(
    const Registration *registration,
    const char *patient_id,
    const RegistrationRepositoryFilter *filter
) {
    if (registration == 0 || patient_id == 0) {
        return 0;
    }

    if (strcmp(registration->patient_id, patient_id) != 0) {
        return 0;
    }

    if (filter == 0) {
        return 1;
    }

    if (filter->use_status && registration->status != filter->status) {
        return 0;
    }

    return RegistrationRepository_is_in_time_range(
        registration->registered_at,
        filter->registered_at_from,
        filter->registered_at_to
    );
}

static void RegistrationRepository_free_record(void *data) {
    free(data);
}

static Result RegistrationRepository_find_by_id_line_handler(const char *line, void *context) {
    RegistrationFindByIdContext *find_context = (RegistrationFindByIdContext *)context;
    Registration registration;
    Result result = RegistrationRepository_parse_line(line, &registration);

    if (!result.success) {
        return result;
    }

    if (strcmp(registration.registration_id, find_context->registration_id) != 0) {
        return Result_make_success("registration skipped");
    }

    if (find_context->found) {
        return Result_make_failure("duplicate registration id");
    }

    *find_context->out_registration = registration;
    find_context->found = 1;
    return Result_make_success("registration matched");
}

static Result RegistrationRepository_collect_by_patient_line_handler(
    const char *line,
    void *context
) {
    RegistrationCollectByPatientContext *collect_context =
        (RegistrationCollectByPatientContext *)context;
    Registration registration;
    Registration *copy = 0;
    Result result = RegistrationRepository_parse_line(line, &registration);

    if (!result.success) {
        return result;
    }

    if (!RegistrationRepository_matches_patient_filter(
            &registration,
            collect_context->patient_id,
            collect_context->filter)) {
        return Result_make_success("registration skipped");
    }

    copy = (Registration *)malloc(sizeof(*copy));
    if (copy == 0) {
        return Result_make_failure("failed to allocate registration result");
    }

    *copy = registration;
    if (!LinkedList_append(collect_context->out_registrations, copy)) {
        free(copy);
        return Result_make_failure("failed to append registration result");
    }

    return Result_make_success("registration collected");
}

Result RegistrationRepository_init(RegistrationRepository *repository, const char *path) {
    if (repository == 0) {
        return Result_make_failure("registration repository missing");
    }

    return TextFileRepository_init(&repository->storage, path);
}

Result RegistrationRepository_ensure_storage(const RegistrationRepository *repository) {
    char line[TEXT_FILE_REPOSITORY_LINE_CAPACITY];
    FILE *file = 0;
    Result result;

    if (repository == 0) {
        return Result_make_failure("registration repository missing");
    }

    result = TextFileRepository_ensure_file_exists(&repository->storage);
    if (!result.success) {
        return result;
    }

    file = fopen(repository->storage.path, "r");
    if (file == 0) {
        return Result_make_failure("failed to inspect registration storage");
    }

    while (fgets(line, sizeof(line), file) != 0) {
        if (strchr(line, '\n') == 0 && !feof(file)) {
            RegistrationRepository_discard_rest_of_line(file);
            fclose(file);
            return Result_make_failure("registration header line too long");
        }

        RepositoryUtils_strip_line_endings(line);
        if (RepositoryUtils_is_blank_line(line)) {
            continue;
        }

        fclose(file);
        if (strcmp(line, REGISTRATION_REPOSITORY_HEADER) != 0) {
            return Result_make_failure("unexpected registration header");
        }

        return Result_make_success("registration storage ready");
    }

    fclose(file);
    return RegistrationRepository_write_header(repository);
}

void RegistrationRepositoryFilter_init(RegistrationRepositoryFilter *filter) {
    if (filter == 0) {
        return;
    }

    filter->use_status = 0;
    filter->status = REG_STATUS_PENDING;
    filter->registered_at_from = 0;
    filter->registered_at_to = 0;
}

Result RegistrationRepository_append(
    const RegistrationRepository *repository,
    const Registration *registration
) {
    char line[TEXT_FILE_REPOSITORY_LINE_CAPACITY];
    Result result = RegistrationRepository_ensure_storage(repository);

    if (!result.success) {
        return result;
    }

    result = RegistrationRepository_serialize(registration, line, sizeof(line));
    if (!result.success) {
        return result;
    }

    return TextFileRepository_append_line(&repository->storage, line);
}

Result RegistrationRepository_find_by_registration_id(
    const RegistrationRepository *repository,
    const char *registration_id,
    Registration *out_registration
) {
    RegistrationFindByIdContext context;
    Result result;

    if (registration_id == 0 || registration_id[0] == '\0' || out_registration == 0) {
        return Result_make_failure("registration query arguments invalid");
    }

    result = RegistrationRepository_ensure_storage(repository);
    if (!result.success) {
        return result;
    }

    context.registration_id = registration_id;
    context.out_registration = out_registration;
    context.found = 0;
    result = TextFileRepository_for_each_data_line(
        &repository->storage,
        RegistrationRepository_find_by_id_line_handler,
        &context
    );
    if (!result.success) {
        return result;
    }

    if (!context.found) {
        return Result_make_failure("registration not found");
    }

    return Result_make_success("registration found");
}

Result RegistrationRepository_find_by_patient_id(
    const RegistrationRepository *repository,
    const char *patient_id,
    const RegistrationRepositoryFilter *filter,
    LinkedList *out_registrations
) {
    RegistrationCollectByPatientContext context;
    LinkedList matches;
    Result result;

    if (patient_id == 0 || patient_id[0] == '\0' || out_registrations == 0) {
        return Result_make_failure("patient registration query arguments invalid");
    }

    if (LinkedList_count(out_registrations) != 0) {
        return Result_make_failure("output registration list must be empty");
    }

    result = RegistrationRepository_ensure_storage(repository);
    if (!result.success) {
        return result;
    }

    LinkedList_init(&matches);
    context.patient_id = patient_id;
    context.filter = filter;
    context.out_registrations = &matches;
    result = TextFileRepository_for_each_data_line(
        &repository->storage,
        RegistrationRepository_collect_by_patient_line_handler,
        &context
    );
    if (!result.success) {
        RegistrationRepository_clear_list(&matches);
        return result;
    }

    *out_registrations = matches;
    return Result_make_success("patient registrations loaded");
}

Result RegistrationRepository_save_all(
    const RegistrationRepository *repository,
    const LinkedList *registrations
) {
    LinkedListNode *current = 0;
    FILE *file = 0;
    Result result;

    if (repository == 0 || registrations == 0) {
        return Result_make_failure("registration save arguments invalid");
    }

    result = TextFileRepository_ensure_file_exists(&repository->storage);
    if (!result.success) {
        return result;
    }

    file = fopen(repository->storage.path, "w");
    if (file == 0) {
        return Result_make_failure("failed to rewrite registration storage");
    }

    if (fputs(REGISTRATION_REPOSITORY_HEADER, file) == EOF || fputc('\n', file) == EOF) {
        fclose(file);
        return Result_make_failure("failed to write registration header");
    }

    current = registrations->head;
    while (current != 0) {
        char line[TEXT_FILE_REPOSITORY_LINE_CAPACITY];

        result = RegistrationRepository_serialize((const Registration *)current->data, line, sizeof(line));
        if (!result.success) {
            fclose(file);
            return result;
        }

        if (fputs(line, file) == EOF || fputc('\n', file) == EOF) {
            fclose(file);
            return Result_make_failure("failed to write registration row");
        }

        current = current->next;
    }

    fclose(file);
    return Result_make_success("registrations saved");
}

void RegistrationRepository_clear_list(LinkedList *registrations) {
    LinkedList_clear(registrations, RegistrationRepository_free_record);
}
