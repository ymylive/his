#include "repository/PatientRepository.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "repository/RepositoryUtils.h"

typedef struct PatientRepositoryFindContext {
    const char *patient_id;
    Patient *out_patient;
    int found;
} PatientRepositoryFindContext;

typedef struct PatientRepositoryLoadContext {
    LinkedList *patients;
} PatientRepositoryLoadContext;

static Result PatientRepository_validate_patient(const Patient *patient) {
    if (patient == 0) {
        return Result_make_failure("patient missing");
    }

    if (patient->patient_id[0] == '\0') {
        return Result_make_failure("patient id missing");
    }

    if (!RepositoryUtils_is_safe_field_text(patient->patient_id) ||
        !RepositoryUtils_is_safe_field_text(patient->name) ||
        !RepositoryUtils_is_safe_field_text(patient->contact) ||
        !RepositoryUtils_is_safe_field_text(patient->id_card) ||
        !RepositoryUtils_is_safe_field_text(patient->allergy) ||
        !RepositoryUtils_is_safe_field_text(patient->medical_history) ||
        !RepositoryUtils_is_safe_field_text(patient->remarks)) {
        return Result_make_failure("patient field contains reserved characters");
    }

    if (patient->gender < PATIENT_GENDER_UNKNOWN || patient->gender > PATIENT_GENDER_FEMALE) {
        return Result_make_failure("patient gender invalid");
    }

    if (!Patient_is_valid_age(patient->age)) {
        return Result_make_failure("patient age invalid");
    }

    if (patient->is_inpatient != 0 && patient->is_inpatient != 1) {
        return Result_make_failure("patient inpatient flag invalid");
    }

    return Result_make_success("patient valid");
}

static Result PatientRepository_parse_int(const char *text, int *out_value) {
    char *end = 0;
    long value = 0;

    if (text == 0 || out_value == 0 || text[0] == '\0') {
        return Result_make_failure("integer field missing");
    }

    errno = 0;
    value = strtol(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0') {
        return Result_make_failure("integer field invalid");
    }

    *out_value = (int)value;
    return Result_make_success("integer field parsed");
}

static Result PatientRepository_parse_line(const char *line, Patient *out_patient) {
    char buffer[TEXT_FILE_REPOSITORY_LINE_CAPACITY];
    char *fields[PATIENT_REPOSITORY_FIELD_COUNT];
    size_t field_count = 0;
    int parsed_value = 0;
    Result result;

    if (line == 0 || out_patient == 0) {
        return Result_make_failure("patient line arguments missing");
    }

    if (strlen(line) >= sizeof(buffer)) {
        return Result_make_failure("patient line too long");
    }

    strcpy(buffer, line);
    result = RepositoryUtils_split_pipe_line(
        buffer,
        fields,
        PATIENT_REPOSITORY_FIELD_COUNT,
        &field_count
    );
    if (result.success == 0) {
        return result;
    }

    result = RepositoryUtils_validate_field_count(field_count, PATIENT_REPOSITORY_FIELD_COUNT);
    if (result.success == 0) {
        return result;
    }

    memset(out_patient, 0, sizeof(*out_patient));
    strcpy(out_patient->patient_id, fields[0]);
    strcpy(out_patient->name, fields[1]);

    result = PatientRepository_parse_int(fields[2], &parsed_value);
    if (result.success == 0) {
        return result;
    }
    out_patient->gender = (PatientGender)parsed_value;

    result = PatientRepository_parse_int(fields[3], &parsed_value);
    if (result.success == 0) {
        return result;
    }
    out_patient->age = parsed_value;

    strcpy(out_patient->contact, fields[4]);
    strcpy(out_patient->id_card, fields[5]);
    strcpy(out_patient->allergy, fields[6]);
    strcpy(out_patient->medical_history, fields[7]);

    result = PatientRepository_parse_int(fields[8], &parsed_value);
    if (result.success == 0) {
        return result;
    }
    out_patient->is_inpatient = parsed_value;

    strcpy(out_patient->remarks, fields[9]);

    return PatientRepository_validate_patient(out_patient);
}

static Result PatientRepository_format_line(const Patient *patient, char *buffer, size_t buffer_size) {
    int written = 0;
    Result result = PatientRepository_validate_patient(patient);

    if (result.success == 0) {
        return result;
    }

    if (buffer == 0 || buffer_size == 0) {
        return Result_make_failure("patient buffer missing");
    }

    written = snprintf(
        buffer,
        buffer_size,
        "%s|%s|%d|%d|%s|%s|%s|%s|%d|%s",
        patient->patient_id,
        patient->name,
        (int)patient->gender,
        patient->age,
        patient->contact,
        patient->id_card,
        patient->allergy,
        patient->medical_history,
        patient->is_inpatient,
        patient->remarks
    );
    if (written < 0 || (size_t)written >= buffer_size) {
        return Result_make_failure("patient line formatting failed");
    }

    return Result_make_success("patient line ready");
}

static Result PatientRepository_ensure_header(const PatientRepository *repository) {
    FILE *file = 0;
    char line[TEXT_FILE_REPOSITORY_LINE_CAPACITY];
    int has_content = 0;
    Result result;

    if (repository == 0) {
        return Result_make_failure("patient repository missing");
    }

    result = TextFileRepository_ensure_file_exists(&repository->file_repository);
    if (result.success == 0) {
        return result;
    }

    file = fopen(repository->file_repository.path, "r");
    if (file == 0) {
        return Result_make_failure("failed to read patient repository");
    }

    while (fgets(line, sizeof(line), file) != 0) {
        RepositoryUtils_strip_line_endings(line);
        if (RepositoryUtils_is_blank_line(line)) {
            continue;
        }

        has_content = 1;
        break;
    }

    fclose(file);

    if (!has_content) {
        return TextFileRepository_append_line(
            &repository->file_repository,
            PATIENT_REPOSITORY_HEADER
        );
    }

    if (strcmp(line, PATIENT_REPOSITORY_HEADER) != 0) {
        return Result_make_failure("patient repository header invalid");
    }

    return Result_make_success("patient repository header ready");
}

static Result PatientRepository_find_line_handler(const char *line, void *context) {
    PatientRepositoryFindContext *find_context = (PatientRepositoryFindContext *)context;
    Patient patient;
    Result result = PatientRepository_parse_line(line, &patient);

    if (result.success == 0) {
        return result;
    }

    if (strcmp(patient.patient_id, find_context->patient_id) != 0) {
        return Result_make_success("patient id mismatch");
    }

    *(find_context->out_patient) = patient;
    find_context->found = 1;
    return Result_make_failure("patient found");
}

static Result PatientRepository_find_internal(
    const PatientRepository *repository,
    const char *patient_id,
    Patient *out_patient,
    int *out_found
) {
    PatientRepositoryFindContext context;
    Result result;

    if (out_found != 0) {
        *out_found = 0;
    }

    if (repository == 0 || patient_id == 0 || patient_id[0] == '\0' || out_patient == 0) {
        return Result_make_failure("patient find arguments missing");
    }

    result = PatientRepository_ensure_header(repository);
    if (result.success == 0) {
        return result;
    }

    memset(&context, 0, sizeof(context));
    context.patient_id = patient_id;
    context.out_patient = out_patient;

    result = TextFileRepository_for_each_data_line(
        &repository->file_repository,
        PatientRepository_find_line_handler,
        &context
    );

    if (context.found != 0) {
        if (out_found != 0) {
            *out_found = 1;
        }
        return Result_make_success("patient found");
    }

    if (result.success == 0 && strcmp(result.message, "patient found") != 0) {
        return result;
    }

    return Result_make_success("patient not found");
}

static Result PatientRepository_load_line_handler(const char *line, void *context) {
    PatientRepositoryLoadContext *load_context = (PatientRepositoryLoadContext *)context;
    Patient parsed_patient;
    Patient *stored_patient = 0;
    Result result = PatientRepository_parse_line(line, &parsed_patient);

    if (result.success == 0) {
        return result;
    }

    stored_patient = (Patient *)malloc(sizeof(Patient));
    if (stored_patient == 0) {
        return Result_make_failure("failed to allocate patient");
    }

    *stored_patient = parsed_patient;
    if (!LinkedList_append(load_context->patients, stored_patient)) {
        free(stored_patient);
        return Result_make_failure("failed to append patient to list");
    }

    return Result_make_success("patient loaded");
}

static Result PatientRepository_validate_patient_list(const LinkedList *patients) {
    const LinkedListNode *outer = 0;

    if (patients == 0) {
        return Result_make_failure("patient list missing");
    }

    outer = patients->head;
    while (outer != 0) {
        const LinkedListNode *inner = outer->next;
        const Patient *patient = (const Patient *)outer->data;
        Result result = PatientRepository_validate_patient(patient);

        if (result.success == 0) {
            return result;
        }

        while (inner != 0) {
            const Patient *other_patient = (const Patient *)inner->data;

            if (strcmp(patient->patient_id, other_patient->patient_id) == 0) {
                return Result_make_failure("duplicate patient id in list");
            }

            inner = inner->next;
        }

        outer = outer->next;
    }

    return Result_make_success("patient list valid");
}

static Result PatientRepository_write_all_lines(
    const PatientRepository *repository,
    const LinkedList *patients
) {
    const LinkedListNode *current = 0;
    FILE *file = 0;
    char line[TEXT_FILE_REPOSITORY_LINE_CAPACITY];
    Result result;

    result = TextFileRepository_ensure_file_exists(&repository->file_repository);
    if (result.success == 0) {
        return result;
    }

    file = fopen(repository->file_repository.path, "w");
    if (file == 0) {
        return Result_make_failure("failed to rewrite patient repository");
    }

    if (fputs(PATIENT_REPOSITORY_HEADER "\n", file) == EOF) {
        fclose(file);
        return Result_make_failure("failed to write patient repository header");
    }

    current = patients->head;
    while (current != 0) {
        result = PatientRepository_format_line((const Patient *)current->data, line, sizeof(line));
        if (result.success == 0) {
            fclose(file);
            return result;
        }

        if (fputs(line, file) == EOF || fputc('\n', file) == EOF) {
            fclose(file);
            return Result_make_failure("failed to write patient repository line");
        }

        current = current->next;
    }

    fclose(file);
    return Result_make_success("patient repository saved");
}

Result PatientRepository_init(PatientRepository *repository, const char *path) {
    Result result;

    if (repository == 0) {
        return Result_make_failure("patient repository missing");
    }

    result = TextFileRepository_init(&repository->file_repository, path);
    if (result.success == 0) {
        return result;
    }

    return PatientRepository_ensure_header(repository);
}

Result PatientRepository_append(const PatientRepository *repository, const Patient *patient) {
    Patient existing_patient;
    int found = 0;
    char line[TEXT_FILE_REPOSITORY_LINE_CAPACITY];
    Result result = PatientRepository_validate_patient(patient);

    if (result.success == 0) {
        return result;
    }

    result = PatientRepository_find_internal(
        repository,
        patient->patient_id,
        &existing_patient,
        &found
    );
    if (result.success == 0) {
        return result;
    }

    if (found != 0) {
        return Result_make_failure("patient id already exists");
    }

    result = PatientRepository_format_line(patient, line, sizeof(line));
    if (result.success == 0) {
        return result;
    }

    return TextFileRepository_append_line(&repository->file_repository, line);
}

Result PatientRepository_find_by_id(
    const PatientRepository *repository,
    const char *patient_id,
    Patient *out_patient
) {
    int found = 0;
    Result result = PatientRepository_find_internal(repository, patient_id, out_patient, &found);

    if (result.success == 0) {
        return result;
    }

    if (found == 0) {
        return Result_make_failure("patient not found");
    }

    return Result_make_success("patient found");
}

Result PatientRepository_load_all(
    const PatientRepository *repository,
    LinkedList *out_patients
) {
    PatientRepositoryLoadContext context;
    Result result;

    if (repository == 0 || out_patients == 0) {
        return Result_make_failure("patient load arguments missing");
    }

    LinkedList_init(out_patients);
    result = PatientRepository_ensure_header(repository);
    if (result.success == 0) {
        return result;
    }

    context.patients = out_patients;
    result = TextFileRepository_for_each_data_line(
        &repository->file_repository,
        PatientRepository_load_line_handler,
        &context
    );

    if (result.success == 0) {
        PatientRepository_clear_loaded_list(out_patients);
        return result;
    }

    return Result_make_success("patients loaded");
}

Result PatientRepository_save_all(
    const PatientRepository *repository,
    const LinkedList *patients
) {
    Result result;

    if (repository == 0) {
        return Result_make_failure("patient repository missing");
    }

    result = PatientRepository_validate_patient_list(patients);
    if (result.success == 0) {
        return result;
    }

    return PatientRepository_write_all_lines(repository, patients);
}

void PatientRepository_clear_loaded_list(LinkedList *patients) {
    LinkedList_clear(patients, free);
}
