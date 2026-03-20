#include "service/RegistrationService.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "common/IdGenerator.h"
#include "repository/RepositoryUtils.h"

#define REGISTRATION_SERVICE_FIELD_COUNT 8
#define REGISTRATION_SERVICE_ID_PREFIX "REG"
#define REGISTRATION_SERVICE_ID_WIDTH 4

typedef struct RegistrationServiceLoadContext {
    LinkedList *registrations;
} RegistrationServiceLoadContext;

static int RegistrationService_has_text(const char *text) {
    return text != 0 && text[0] != '\0';
}

static void RegistrationService_copy_text(
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

static Result RegistrationService_parse_status(
    const char *field,
    RegistrationStatus *out_status
) {
    char *end = 0;
    long value = 0;

    if (!RegistrationService_has_text(field) || out_status == 0) {
        return Result_make_failure("registration status missing");
    }

    errno = 0;
    value = strtol(field, &end, 10);
    if (errno != 0 || end == field || end == 0 || *end != '\0') {
        return Result_make_failure("registration status invalid");
    }

    if (value < REG_STATUS_PENDING || value > REG_STATUS_CANCELLED) {
        return Result_make_failure("registration status invalid");
    }

    *out_status = (RegistrationStatus)value;
    return Result_make_success("registration status parsed");
}

static Result RegistrationService_parse_registration_line(
    const char *line,
    Registration *out_registration
) {
    char buffer[TEXT_FILE_REPOSITORY_LINE_CAPACITY];
    char *fields[REGISTRATION_SERVICE_FIELD_COUNT];
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
        REGISTRATION_SERVICE_FIELD_COUNT,
        &field_count
    );
    if (!result.success) {
        return result;
    }

    result = RepositoryUtils_validate_field_count(
        field_count,
        REGISTRATION_SERVICE_FIELD_COUNT
    );
    if (!result.success) {
        return result;
    }

    memset(out_registration, 0, sizeof(*out_registration));
    RegistrationService_copy_text(
        out_registration->registration_id,
        sizeof(out_registration->registration_id),
        fields[0]
    );
    RegistrationService_copy_text(
        out_registration->patient_id,
        sizeof(out_registration->patient_id),
        fields[1]
    );
    RegistrationService_copy_text(
        out_registration->doctor_id,
        sizeof(out_registration->doctor_id),
        fields[2]
    );
    RegistrationService_copy_text(
        out_registration->department_id,
        sizeof(out_registration->department_id),
        fields[3]
    );
    RegistrationService_copy_text(
        out_registration->registered_at,
        sizeof(out_registration->registered_at),
        fields[4]
    );

    result = RegistrationService_parse_status(fields[5], &out_registration->status);
    if (!result.success) {
        return result;
    }

    RegistrationService_copy_text(
        out_registration->diagnosed_at,
        sizeof(out_registration->diagnosed_at),
        fields[6]
    );
    RegistrationService_copy_text(
        out_registration->cancelled_at,
        sizeof(out_registration->cancelled_at),
        fields[7]
    );

    return Result_make_success("registration parsed");
}

static Result RegistrationService_collect_registration_line(
    const char *line,
    void *context
) {
    RegistrationServiceLoadContext *load_context =
        (RegistrationServiceLoadContext *)context;
    Registration *copy = 0;
    Result result;

    if (load_context == 0 || load_context->registrations == 0) {
        return Result_make_failure("registration load context missing");
    }

    copy = (Registration *)malloc(sizeof(*copy));
    if (copy == 0) {
        return Result_make_failure("failed to allocate registration");
    }

    result = RegistrationService_parse_registration_line(line, copy);
    if (!result.success) {
        free(copy);
        return result;
    }

    if (!LinkedList_append(load_context->registrations, copy)) {
        free(copy);
        return Result_make_failure("failed to append registration");
    }

    return Result_make_success("registration loaded");
}

static Result RegistrationService_load_all_registrations(
    RegistrationService *service,
    LinkedList *out_registrations
) {
    RegistrationServiceLoadContext context;
    Result result;

    if (service == 0 || service->registration_repository == 0 || out_registrations == 0) {
        return Result_make_failure("registration service missing");
    }

    LinkedList_init(out_registrations);
    result = RegistrationRepository_ensure_storage(service->registration_repository);
    if (!result.success) {
        return result;
    }

    context.registrations = out_registrations;
    result = TextFileRepository_for_each_data_line(
        &service->registration_repository->storage,
        RegistrationService_collect_registration_line,
        &context
    );
    if (!result.success) {
        RegistrationRepository_clear_list(out_registrations);
        return result;
    }

    return Result_make_success("registrations loaded");
}

static int RegistrationService_matches_filter(
    const Registration *registration,
    const RegistrationRepositoryFilter *filter
) {
    if (registration == 0) {
        return 0;
    }

    if (filter == 0) {
        return 1;
    }

    if (filter->use_status && registration->status != filter->status) {
        return 0;
    }

    if (RegistrationService_has_text(filter->registered_at_from) &&
        strcmp(registration->registered_at, filter->registered_at_from) < 0) {
        return 0;
    }

    if (RegistrationService_has_text(filter->registered_at_to) &&
        strcmp(registration->registered_at, filter->registered_at_to) > 0) {
        return 0;
    }

    return 1;
}

static Result RegistrationService_append_registration_copy(
    LinkedList *out_registrations,
    const Registration *registration
) {
    Registration *copy = 0;

    if (out_registrations == 0 || registration == 0) {
        return Result_make_failure("registration copy arguments invalid");
    }

    copy = (Registration *)malloc(sizeof(*copy));
    if (copy == 0) {
        return Result_make_failure("failed to allocate registration copy");
    }

    *copy = *registration;
    if (!LinkedList_append(out_registrations, copy)) {
        free(copy);
        return Result_make_failure("failed to append registration copy");
    }

    return Result_make_success("registration copy appended");
}

static Result RegistrationService_validate_related_entities(
    RegistrationService *service,
    const char *patient_id,
    const char *doctor_id,
    const char *department_id
) {
    Patient patient;
    Doctor doctor;
    Department department;
    Result result;

    result = PatientRepository_find_by_id(
        service->patient_repository,
        patient_id,
        &patient
    );
    if (!result.success) {
        return Result_make_failure("patient not found");
    }

    result = DoctorRepository_find_by_doctor_id(
        service->doctor_repository,
        doctor_id,
        &doctor
    );
    if (!result.success) {
        return Result_make_failure("doctor not found");
    }

    result = DepartmentRepository_find_by_department_id(
        service->department_repository,
        department_id,
        &department
    );
    if (!result.success) {
        return Result_make_failure("department not found");
    }

    if (doctor.status != DOCTOR_STATUS_ACTIVE) {
        return Result_make_failure("doctor inactive");
    }

    if (strcmp(doctor.department_id, department_id) != 0) {
        return Result_make_failure("doctor department mismatch");
    }

    return Result_make_success("related entities valid");
}

static Result RegistrationService_next_registration_sequence(
    RegistrationService *service,
    int *out_sequence
) {
    LinkedList registrations;
    LinkedListNode *current = 0;
    int max_sequence = 0;
    Result result;

    if (out_sequence == 0) {
        return Result_make_failure("registration sequence output missing");
    }

    result = RegistrationService_load_all_registrations(service, &registrations);
    if (!result.success) {
        return result;
    }

    current = registrations.head;
    while (current != 0) {
        const Registration *registration = (const Registration *)current->data;
        const char *suffix = registration->registration_id;
        char *end = 0;
        long value = 0;

        if (strncmp(
                registration->registration_id,
                REGISTRATION_SERVICE_ID_PREFIX,
                strlen(REGISTRATION_SERVICE_ID_PREFIX)) != 0) {
            RegistrationRepository_clear_list(&registrations);
            return Result_make_failure("registration id format invalid");
        }

        suffix += strlen(REGISTRATION_SERVICE_ID_PREFIX);
        if (!RegistrationService_has_text(suffix)) {
            RegistrationRepository_clear_list(&registrations);
            return Result_make_failure("registration id format invalid");
        }

        errno = 0;
        value = strtol(suffix, &end, 10);
        if (errno != 0 || end == suffix || end == 0 || *end != '\0' || value < 0) {
            RegistrationRepository_clear_list(&registrations);
            return Result_make_failure("registration id format invalid");
        }

        if ((int)value > max_sequence) {
            max_sequence = (int)value;
        }

        current = current->next;
    }

    RegistrationRepository_clear_list(&registrations);
    *out_sequence = max_sequence + 1;
    return Result_make_success("registration sequence ready");
}

static Result RegistrationService_fill_new_registration(
    Registration *registration,
    const char *registration_id,
    const char *patient_id,
    const char *doctor_id,
    const char *department_id,
    const char *registered_at
) {
    if (registration == 0) {
        return Result_make_failure("registration output missing");
    }

    memset(registration, 0, sizeof(*registration));
    RegistrationService_copy_text(
        registration->registration_id,
        sizeof(registration->registration_id),
        registration_id
    );
    RegistrationService_copy_text(
        registration->patient_id,
        sizeof(registration->patient_id),
        patient_id
    );
    RegistrationService_copy_text(
        registration->doctor_id,
        sizeof(registration->doctor_id),
        doctor_id
    );
    RegistrationService_copy_text(
        registration->department_id,
        sizeof(registration->department_id),
        department_id
    );
    RegistrationService_copy_text(
        registration->registered_at,
        sizeof(registration->registered_at),
        registered_at
    );
    registration->status = REG_STATUS_PENDING;
    registration->diagnosed_at[0] = '\0';
    registration->cancelled_at[0] = '\0';
    return Result_make_success("registration filled");
}

static Result RegistrationService_find_record_in_list(
    LinkedList *registrations,
    const char *registration_id,
    Registration **out_registration
) {
    LinkedListNode *current = 0;

    if (registrations == 0 || !RegistrationService_has_text(registration_id) ||
        out_registration == 0) {
        return Result_make_failure("registration list arguments invalid");
    }

    current = registrations->head;
    while (current != 0) {
        Registration *registration = (Registration *)current->data;
        if (strcmp(registration->registration_id, registration_id) == 0) {
            *out_registration = registration;
            return Result_make_success("registration found in list");
        }

        current = current->next;
    }

    return Result_make_failure("registration not found");
}

Result RegistrationService_init(
    RegistrationService *service,
    RegistrationRepository *registration_repository,
    PatientRepository *patient_repository,
    DoctorRepository *doctor_repository,
    DepartmentRepository *department_repository
) {
    Result result;

    if (service == 0 || registration_repository == 0 || patient_repository == 0 ||
        doctor_repository == 0 || department_repository == 0) {
        return Result_make_failure("registration service dependencies missing");
    }

    result = RegistrationRepository_ensure_storage(registration_repository);
    if (!result.success) {
        return result;
    }

    service->registration_repository = registration_repository;
    service->patient_repository = patient_repository;
    service->doctor_repository = doctor_repository;
    service->department_repository = department_repository;
    return Result_make_success("registration service ready");
}

Result RegistrationService_create(
    RegistrationService *service,
    const char *patient_id,
    const char *doctor_id,
    const char *department_id,
    const char *registered_at,
    Registration *out_registration
) {
    char registration_id[HIS_DOMAIN_ID_CAPACITY];
    int next_sequence = 0;
    Result result;

    if (service == 0 || !RegistrationService_has_text(patient_id) ||
        !RegistrationService_has_text(doctor_id) ||
        !RegistrationService_has_text(department_id) ||
        !RegistrationService_has_text(registered_at) ||
        out_registration == 0) {
        return Result_make_failure("registration create arguments invalid");
    }

    result = RegistrationService_validate_related_entities(
        service,
        patient_id,
        doctor_id,
        department_id
    );
    if (!result.success) {
        return result;
    }

    result = RegistrationService_next_registration_sequence(service, &next_sequence);
    if (!result.success) {
        return result;
    }

    if (!IdGenerator_format(
            registration_id,
            sizeof(registration_id),
            REGISTRATION_SERVICE_ID_PREFIX,
            next_sequence,
            REGISTRATION_SERVICE_ID_WIDTH)) {
        return Result_make_failure("failed to generate registration id");
    }

    result = RegistrationService_fill_new_registration(
        out_registration,
        registration_id,
        patient_id,
        doctor_id,
        department_id,
        registered_at
    );
    if (!result.success) {
        return result;
    }

    return RegistrationRepository_append(
        service->registration_repository,
        out_registration
    );
}

Result RegistrationService_find_by_registration_id(
    RegistrationService *service,
    const char *registration_id,
    Registration *out_registration
) {
    if (service == 0 || !RegistrationService_has_text(registration_id) ||
        out_registration == 0) {
        return Result_make_failure("registration query arguments invalid");
    }

    return RegistrationRepository_find_by_registration_id(
        service->registration_repository,
        registration_id,
        out_registration
    );
}

Result RegistrationService_cancel(
    RegistrationService *service,
    const char *registration_id,
    const char *cancelled_at,
    Registration *out_registration
) {
    LinkedList registrations;
    Registration *target = 0;
    Result result;

    if (service == 0 || !RegistrationService_has_text(registration_id) ||
        !RegistrationService_has_text(cancelled_at) ||
        out_registration == 0) {
        return Result_make_failure("registration cancel arguments invalid");
    }

    result = RegistrationService_load_all_registrations(service, &registrations);
    if (!result.success) {
        return result;
    }

    result = RegistrationService_find_record_in_list(
        &registrations,
        registration_id,
        &target
    );
    if (!result.success) {
        RegistrationRepository_clear_list(&registrations);
        return result;
    }

    if (!Registration_cancel(target, cancelled_at)) {
        RegistrationRepository_clear_list(&registrations);
        return Result_make_failure("registration cannot be cancelled");
    }

    result = RegistrationRepository_save_all(
        service->registration_repository,
        &registrations
    );
    if (!result.success) {
        RegistrationRepository_clear_list(&registrations);
        return result;
    }

    *out_registration = *target;
    RegistrationRepository_clear_list(&registrations);
    return Result_make_success("registration cancelled");
}

Result RegistrationService_find_by_patient_id(
    RegistrationService *service,
    const char *patient_id,
    const RegistrationRepositoryFilter *filter,
    LinkedList *out_registrations
) {
    Patient patient;
    Result result;

    if (service == 0 || !RegistrationService_has_text(patient_id) ||
        out_registrations == 0) {
        return Result_make_failure("patient registration query arguments invalid");
    }

    result = PatientRepository_find_by_id(
        service->patient_repository,
        patient_id,
        &patient
    );
    if (!result.success) {
        return Result_make_failure("patient not found");
    }

    return RegistrationRepository_find_by_patient_id(
        service->registration_repository,
        patient_id,
        filter,
        out_registrations
    );
}

Result RegistrationService_find_by_doctor_id(
    RegistrationService *service,
    const char *doctor_id,
    const RegistrationRepositoryFilter *filter,
    LinkedList *out_registrations
) {
    Doctor doctor;
    LinkedList registrations;
    const LinkedListNode *current = 0;
    Result result;

    if (service == 0 || !RegistrationService_has_text(doctor_id) || out_registrations == 0) {
        return Result_make_failure("doctor registration query arguments invalid");
    }

    result = DoctorRepository_find_by_doctor_id(
        service->doctor_repository,
        doctor_id,
        &doctor
    );
    if (!result.success) {
        return Result_make_failure("doctor not found");
    }

    LinkedList_init(out_registrations);
    result = RegistrationService_load_all_registrations(service, &registrations);
    if (!result.success) {
        return result;
    }

    current = registrations.head;
    while (current != 0) {
        const Registration *registration = (const Registration *)current->data;

        if (strcmp(registration->doctor_id, doctor_id) == 0 &&
            RegistrationService_matches_filter(registration, filter)) {
            result = RegistrationService_append_registration_copy(
                out_registrations,
                registration
            );
            if (!result.success) {
                RegistrationRepository_clear_list(&registrations);
                RegistrationRepository_clear_list(out_registrations);
                return result;
            }
        }

        current = current->next;
    }

    RegistrationRepository_clear_list(&registrations);
    return Result_make_success("doctor registrations loaded");
}
