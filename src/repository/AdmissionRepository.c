#include "repository/AdmissionRepository.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "repository/RepositoryUtils.h"

typedef struct AdmissionRepositoryLoadContext {
    LinkedList *admissions;
} AdmissionRepositoryLoadContext;

static int AdmissionRepository_has_text(const char *text) {
    return text != 0 && text[0] != '\0';
}

static void AdmissionRepository_copy_text(
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

static Result AdmissionRepository_parse_int(const char *field, int *out_value) {
    char *end_pointer = 0;
    long value = 0;

    if (field == 0 || out_value == 0 || field[0] == '\0') {
        return Result_make_failure("admission integer missing");
    }

    value = strtol(field, &end_pointer, 10);
    if (end_pointer == field || end_pointer == 0 || *end_pointer != '\0') {
        return Result_make_failure("admission integer invalid");
    }

    *out_value = (int)value;
    return Result_make_success("admission integer parsed");
}

static Result AdmissionRepository_validate(const Admission *admission) {
    if (admission == 0) {
        return Result_make_failure("admission missing");
    }

    if (!AdmissionRepository_has_text(admission->admission_id) ||
        !AdmissionRepository_has_text(admission->patient_id) ||
        !AdmissionRepository_has_text(admission->ward_id) ||
        !AdmissionRepository_has_text(admission->bed_id) ||
        !AdmissionRepository_has_text(admission->admitted_at)) {
        return Result_make_failure("admission required field missing");
    }

    if (!RepositoryUtils_is_safe_field_text(admission->admission_id) ||
        !RepositoryUtils_is_safe_field_text(admission->patient_id) ||
        !RepositoryUtils_is_safe_field_text(admission->ward_id) ||
        !RepositoryUtils_is_safe_field_text(admission->bed_id) ||
        !RepositoryUtils_is_safe_field_text(admission->admitted_at) ||
        !RepositoryUtils_is_safe_field_text(admission->discharged_at) ||
        !RepositoryUtils_is_safe_field_text(admission->summary)) {
        return Result_make_failure("admission field contains reserved character");
    }

    if (admission->status < ADMISSION_STATUS_ACTIVE ||
        admission->status > ADMISSION_STATUS_DISCHARGED) {
        return Result_make_failure("admission status invalid");
    }

    if (admission->status == ADMISSION_STATUS_ACTIVE &&
        AdmissionRepository_has_text(admission->discharged_at)) {
        return Result_make_failure("active admission has discharge time");
    }

    if (admission->status == ADMISSION_STATUS_DISCHARGED &&
        !AdmissionRepository_has_text(admission->discharged_at)) {
        return Result_make_failure("discharged admission missing discharge time");
    }

    return Result_make_success("admission valid");
}

static Result AdmissionRepository_format_line(
    const Admission *admission,
    char *line,
    size_t line_capacity
) {
    int written = 0;
    Result result = AdmissionRepository_validate(admission);

    if (result.success == 0) {
        return result;
    }

    if (line == 0 || line_capacity == 0) {
        return Result_make_failure("admission line buffer missing");
    }

    written = snprintf(
        line,
        line_capacity,
        "%s|%s|%s|%s|%s|%d|%s|%s",
        admission->admission_id,
        admission->patient_id,
        admission->ward_id,
        admission->bed_id,
        admission->admitted_at,
        (int)admission->status,
        admission->discharged_at,
        admission->summary
    );
    if (written < 0 || (size_t)written >= line_capacity) {
        return Result_make_failure("admission line too long");
    }

    return Result_make_success("admission formatted");
}

static Result AdmissionRepository_parse_line(const char *line, Admission *admission) {
    char mutable_line[TEXT_FILE_REPOSITORY_LINE_CAPACITY];
    char *fields[ADMISSION_REPOSITORY_FIELD_COUNT];
    size_t field_count = 0;
    int status_value = 0;
    Result result;

    if (line == 0 || admission == 0) {
        return Result_make_failure("admission line missing");
    }

    AdmissionRepository_copy_text(mutable_line, sizeof(mutable_line), line);
    result = RepositoryUtils_split_pipe_line(
        mutable_line,
        fields,
        ADMISSION_REPOSITORY_FIELD_COUNT,
        &field_count
    );
    if (result.success == 0) {
        return result;
    }

    result = RepositoryUtils_validate_field_count(field_count, ADMISSION_REPOSITORY_FIELD_COUNT);
    if (result.success == 0) {
        return result;
    }

    memset(admission, 0, sizeof(*admission));
    AdmissionRepository_copy_text(
        admission->admission_id,
        sizeof(admission->admission_id),
        fields[0]
    );
    AdmissionRepository_copy_text(
        admission->patient_id,
        sizeof(admission->patient_id),
        fields[1]
    );
    AdmissionRepository_copy_text(admission->ward_id, sizeof(admission->ward_id), fields[2]);
    AdmissionRepository_copy_text(admission->bed_id, sizeof(admission->bed_id), fields[3]);
    AdmissionRepository_copy_text(
        admission->admitted_at,
        sizeof(admission->admitted_at),
        fields[4]
    );

    result = AdmissionRepository_parse_int(fields[5], &status_value);
    if (result.success == 0) {
        return result;
    }
    admission->status = (AdmissionStatus)status_value;

    AdmissionRepository_copy_text(
        admission->discharged_at,
        sizeof(admission->discharged_at),
        fields[6]
    );
    AdmissionRepository_copy_text(admission->summary, sizeof(admission->summary), fields[7]);

    return AdmissionRepository_validate(admission);
}

static Result AdmissionRepository_ensure_header(const AdmissionRepository *repository) {
    char line[TEXT_FILE_REPOSITORY_LINE_CAPACITY];
    FILE *file = 0;
    Result result;

    if (repository == 0) {
        return Result_make_failure("admission repository missing");
    }

    result = TextFileRepository_ensure_file_exists(&repository->storage);
    if (result.success == 0) {
        return result;
    }

    file = fopen(repository->storage.path, "r");
    if (file == 0) {
        return Result_make_failure("failed to inspect admission repository");
    }

    while (fgets(line, sizeof(line), file) != 0) {
        RepositoryUtils_strip_line_endings(line);
        if (RepositoryUtils_is_blank_line(line)) {
            continue;
        }

        fclose(file);
        if (strcmp(line, ADMISSION_REPOSITORY_HEADER) != 0) {
            return Result_make_failure("admission repository header mismatch");
        }

        return Result_make_success("admission header ready");
    }

    fclose(file);
    return TextFileRepository_append_line(&repository->storage, ADMISSION_REPOSITORY_HEADER);
}

static Result AdmissionRepository_collect_line(const char *line, void *context) {
    AdmissionRepositoryLoadContext *load_context = (AdmissionRepositoryLoadContext *)context;
    Admission *admission = 0;
    Result result;

    if (load_context == 0 || load_context->admissions == 0) {
        return Result_make_failure("admission load context missing");
    }

    admission = (Admission *)malloc(sizeof(*admission));
    if (admission == 0) {
        return Result_make_failure("failed to allocate admission");
    }

    result = AdmissionRepository_parse_line(line, admission);
    if (result.success == 0) {
        free(admission);
        return result;
    }

    if (!LinkedList_append(load_context->admissions, admission)) {
        free(admission);
        return Result_make_failure("failed to append admission");
    }

    return Result_make_success("admission loaded");
}

static int AdmissionRepository_validate_list(const LinkedList *admissions) {
    const LinkedListNode *outer = 0;

    if (admissions == 0) {
        return 0;
    }

    outer = admissions->head;
    while (outer != 0) {
        const LinkedListNode *inner = outer->next;
        const Admission *admission = (const Admission *)outer->data;
        Result result = AdmissionRepository_validate(admission);

        if (result.success == 0) {
            return 0;
        }

        while (inner != 0) {
            const Admission *other = (const Admission *)inner->data;

            if (strcmp(admission->admission_id, other->admission_id) == 0) {
                return 0;
            }

            if (admission->status == ADMISSION_STATUS_ACTIVE &&
                other->status == ADMISSION_STATUS_ACTIVE) {
                if (strcmp(admission->patient_id, other->patient_id) == 0 ||
                    strcmp(admission->bed_id, other->bed_id) == 0) {
                    return 0;
                }
            }

            inner = inner->next;
        }

        outer = outer->next;
    }

    return 1;
}

static Admission *AdmissionRepository_find_in_list(
    LinkedList *admissions,
    const char *admission_id
) {
    LinkedListNode *current = 0;

    if (admissions == 0 || admission_id == 0) {
        return 0;
    }

    current = admissions->head;
    while (current != 0) {
        Admission *admission = (Admission *)current->data;
        if (strcmp(admission->admission_id, admission_id) == 0) {
            return admission;
        }

        current = current->next;
    }

    return 0;
}

static Admission *AdmissionRepository_find_active_by_field(
    LinkedList *admissions,
    const char *value,
    int match_patient
) {
    LinkedListNode *current = 0;

    if (admissions == 0 || value == 0) {
        return 0;
    }

    current = admissions->head;
    while (current != 0) {
        Admission *admission = (Admission *)current->data;
        const char *field_value = match_patient ? admission->patient_id : admission->bed_id;

        if (admission->status == ADMISSION_STATUS_ACTIVE && strcmp(field_value, value) == 0) {
            return admission;
        }

        current = current->next;
    }

    return 0;
}

Result AdmissionRepository_init(AdmissionRepository *repository, const char *path) {
    Result result;

    if (repository == 0) {
        return Result_make_failure("admission repository missing");
    }

    result = TextFileRepository_init(&repository->storage, path);
    if (result.success == 0) {
        return result;
    }

    return AdmissionRepository_ensure_header(repository);
}

Result AdmissionRepository_append(
    const AdmissionRepository *repository,
    const Admission *admission
) {
    LinkedList admissions;
    Admission *copy = 0;
    Result result = AdmissionRepository_validate(admission);

    if (result.success == 0) {
        return result;
    }

    result = AdmissionRepository_load_all(repository, &admissions);
    if (result.success == 0) {
        return result;
    }

    if (AdmissionRepository_find_in_list(&admissions, admission->admission_id) != 0) {
        AdmissionRepository_clear_loaded_list(&admissions);
        return Result_make_failure("admission id already exists");
    }

    if (admission->status == ADMISSION_STATUS_ACTIVE) {
        if (AdmissionRepository_find_active_by_field(&admissions, admission->patient_id, 1) != 0) {
            AdmissionRepository_clear_loaded_list(&admissions);
            return Result_make_failure("patient already admitted");
        }

        if (AdmissionRepository_find_active_by_field(&admissions, admission->bed_id, 0) != 0) {
            AdmissionRepository_clear_loaded_list(&admissions);
            return Result_make_failure("bed already occupied");
        }
    }

    copy = (Admission *)malloc(sizeof(*copy));
    if (copy == 0) {
        AdmissionRepository_clear_loaded_list(&admissions);
        return Result_make_failure("failed to allocate admission copy");
    }

    *copy = *admission;
    if (!LinkedList_append(&admissions, copy)) {
        free(copy);
        AdmissionRepository_clear_loaded_list(&admissions);
        return Result_make_failure("failed to append admission copy");
    }

    result = AdmissionRepository_save_all(repository, &admissions);
    AdmissionRepository_clear_loaded_list(&admissions);
    return result;
}

Result AdmissionRepository_find_by_id(
    const AdmissionRepository *repository,
    const char *admission_id,
    Admission *out_admission
) {
    LinkedList admissions;
    Admission *admission = 0;
    Result result;

    if (!AdmissionRepository_has_text(admission_id) || out_admission == 0) {
        return Result_make_failure("admission query arguments missing");
    }

    result = AdmissionRepository_load_all(repository, &admissions);
    if (result.success == 0) {
        return result;
    }

    admission = AdmissionRepository_find_in_list(&admissions, admission_id);
    if (admission == 0) {
        AdmissionRepository_clear_loaded_list(&admissions);
        return Result_make_failure("admission not found");
    }

    *out_admission = *admission;
    AdmissionRepository_clear_loaded_list(&admissions);
    return Result_make_success("admission found");
}

Result AdmissionRepository_find_active_by_patient_id(
    const AdmissionRepository *repository,
    const char *patient_id,
    Admission *out_admission
) {
    LinkedList admissions;
    Admission *admission = 0;
    Result result;

    if (!AdmissionRepository_has_text(patient_id) || out_admission == 0) {
        return Result_make_failure("active admission query arguments missing");
    }

    result = AdmissionRepository_load_all(repository, &admissions);
    if (result.success == 0) {
        return result;
    }

    admission = AdmissionRepository_find_active_by_field(&admissions, patient_id, 1);
    if (admission == 0) {
        AdmissionRepository_clear_loaded_list(&admissions);
        return Result_make_failure("active admission not found");
    }

    *out_admission = *admission;
    AdmissionRepository_clear_loaded_list(&admissions);
    return Result_make_success("active admission found");
}

Result AdmissionRepository_find_active_by_bed_id(
    const AdmissionRepository *repository,
    const char *bed_id,
    Admission *out_admission
) {
    LinkedList admissions;
    Admission *admission = 0;
    Result result;

    if (!AdmissionRepository_has_text(bed_id) || out_admission == 0) {
        return Result_make_failure("active admission query arguments missing");
    }

    result = AdmissionRepository_load_all(repository, &admissions);
    if (result.success == 0) {
        return result;
    }

    admission = AdmissionRepository_find_active_by_field(&admissions, bed_id, 0);
    if (admission == 0) {
        AdmissionRepository_clear_loaded_list(&admissions);
        return Result_make_failure("active admission not found");
    }

    *out_admission = *admission;
    AdmissionRepository_clear_loaded_list(&admissions);
    return Result_make_success("active admission found");
}

Result AdmissionRepository_load_all(
    const AdmissionRepository *repository,
    LinkedList *out_admissions
) {
    AdmissionRepositoryLoadContext context;
    Result result;

    if (out_admissions == 0) {
        return Result_make_failure("admission output list missing");
    }

    LinkedList_init(out_admissions);
    context.admissions = out_admissions;

    result = AdmissionRepository_ensure_header(repository);
    if (result.success == 0) {
        return result;
    }

    result = TextFileRepository_for_each_data_line(
        &repository->storage,
        AdmissionRepository_collect_line,
        &context
    );
    if (result.success == 0) {
        AdmissionRepository_clear_loaded_list(out_admissions);
        return result;
    }

    return Result_make_success("admissions loaded");
}

Result AdmissionRepository_save_all(
    const AdmissionRepository *repository,
    const LinkedList *admissions
) {
    const LinkedListNode *current = 0;
    FILE *file = 0;
    Result result;

    if (repository == 0 || admissions == 0) {
        return Result_make_failure("admission save arguments missing");
    }

    if (!AdmissionRepository_validate_list(admissions)) {
        return Result_make_failure("admission list invalid");
    }

    result = TextFileRepository_ensure_file_exists(&repository->storage);
    if (result.success == 0) {
        return result;
    }

    file = fopen(repository->storage.path, "w");
    if (file == 0) {
        return Result_make_failure("failed to rewrite admission repository");
    }

    if (fprintf(file, "%s\n", ADMISSION_REPOSITORY_HEADER) < 0) {
        fclose(file);
        return Result_make_failure("failed to write admission header");
    }

    current = admissions->head;
    while (current != 0) {
        char line[TEXT_FILE_REPOSITORY_LINE_CAPACITY];

        result = AdmissionRepository_format_line(
            (const Admission *)current->data,
            line,
            sizeof(line)
        );
        if (result.success == 0) {
            fclose(file);
            return result;
        }

        if (fprintf(file, "%s\n", line) < 0) {
            fclose(file);
            return Result_make_failure("failed to write admission line");
        }

        current = current->next;
    }

    fclose(file);
    return Result_make_success("admissions saved");
}

void AdmissionRepository_clear_loaded_list(LinkedList *admissions) {
    LinkedList_clear(admissions, free);
}
