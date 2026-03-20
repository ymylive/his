#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "common/LinkedList.h"
#include "domain/Department.h"
#include "domain/Doctor.h"
#include "domain/Patient.h"
#include "domain/Registration.h"
#include "repository/DepartmentRepository.h"
#include "repository/DoctorRepository.h"
#include "repository/PatientRepository.h"
#include "repository/RegistrationRepository.h"
#include "service/RegistrationService.h"

typedef struct RegistrationServiceTestContext {
    char patient_path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    char doctor_path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    char department_path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    char registration_path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    PatientRepository patient_repository;
    DoctorRepository doctor_repository;
    DepartmentRepository department_repository;
    RegistrationRepository registration_repository;
    RegistrationService service;
} RegistrationServiceTestContext;

static void build_test_path(
    char *buffer,
    size_t buffer_size,
    const char *test_name,
    const char *file_name
) {
    assert(buffer != 0);
    assert(test_name != 0);
    assert(file_name != 0);

    snprintf(
        buffer,
        buffer_size,
        "build/test_registration_service_data/%s_%ld_%lu_%s",
        test_name,
        (long)time(0),
        (unsigned long)clock(),
        file_name
    );
}

static void copy_text(char *destination, size_t capacity, const char *source) {
    assert(destination != 0);
    assert(capacity > 0);

    if (source == 0) {
        destination[0] = '\0';
        return;
    }

    strncpy(destination, source, capacity - 1);
    destination[capacity - 1] = '\0';
}

static Registration make_registration(
    const char *registration_id,
    const char *patient_id,
    const char *doctor_id,
    const char *department_id,
    const char *registered_at,
    RegistrationStatus status,
    const char *diagnosed_at,
    const char *cancelled_at
) {
    Registration registration;

    memset(&registration, 0, sizeof(registration));
    copy_text(registration.registration_id, sizeof(registration.registration_id), registration_id);
    copy_text(registration.patient_id, sizeof(registration.patient_id), patient_id);
    copy_text(registration.doctor_id, sizeof(registration.doctor_id), doctor_id);
    copy_text(registration.department_id, sizeof(registration.department_id), department_id);
    copy_text(registration.registered_at, sizeof(registration.registered_at), registered_at);
    registration.status = status;
    copy_text(registration.diagnosed_at, sizeof(registration.diagnosed_at), diagnosed_at);
    copy_text(registration.cancelled_at, sizeof(registration.cancelled_at), cancelled_at);
    return registration;
}

static const Registration *registration_at(const LinkedList *list, size_t index) {
    const LinkedListNode *node = 0;
    size_t current_index = 0;

    assert(list != 0);
    node = list->head;
    while (node != 0) {
        if (current_index == index) {
            return (const Registration *)node->data;
        }

        node = node->next;
        current_index++;
    }

    return 0;
}

static void setup_context(RegistrationServiceTestContext *context, const char *test_name) {
    Department department_a = {"DEP0001", "Internal", "Floor1", "Internal medicine"};
    Department department_b = {"DEP0002", "Surgery", "Floor2", "General surgery"};
    Doctor doctor_a = {
        "DOC0001",
        "Dr.Alice",
        "Chief",
        "DEP0001",
        "Mon-Fri AM",
        DOCTOR_STATUS_ACTIVE
    };
    Doctor doctor_b = {
        "DOC0002",
        "Dr.Bob",
        "Attending",
        "DEP0002",
        "Tue-Thu PM",
        DOCTOR_STATUS_ACTIVE
    };
    Patient patient_a = {
        "PAT0001",
        "Alice",
        PATIENT_GENDER_FEMALE,
        28,
        "13800000000",
        "ID0001",
        "None",
        "Healthy",
        0,
        "First visit"
    };
    Patient patient_b = {
        "PAT0002",
        "Bob",
        PATIENT_GENDER_MALE,
        35,
        "13800000001",
        "ID0002",
        "Penicillin",
        "Asthma",
        0,
        "Review"
    };
    Result result;

    memset(context, 0, sizeof(*context));
    build_test_path(context->patient_path, sizeof(context->patient_path), test_name, "patients.txt");
    build_test_path(context->doctor_path, sizeof(context->doctor_path), test_name, "doctors.txt");
    build_test_path(
        context->department_path,
        sizeof(context->department_path),
        test_name,
        "departments.txt"
    );
    build_test_path(
        context->registration_path,
        sizeof(context->registration_path),
        test_name,
        "registrations.txt"
    );

    result = PatientRepository_init(&context->patient_repository, context->patient_path);
    assert(result.success == 1);
    result = DoctorRepository_init(&context->doctor_repository, context->doctor_path);
    assert(result.success == 1);
    result = DepartmentRepository_init(&context->department_repository, context->department_path);
    assert(result.success == 1);
    result = RegistrationRepository_init(
        &context->registration_repository,
        context->registration_path
    );
    assert(result.success == 1);

    assert(DepartmentRepository_save(&context->department_repository, &department_a).success == 1);
    assert(DepartmentRepository_save(&context->department_repository, &department_b).success == 1);
    assert(DoctorRepository_save(&context->doctor_repository, &doctor_a).success == 1);
    assert(DoctorRepository_save(&context->doctor_repository, &doctor_b).success == 1);
    assert(PatientRepository_append(&context->patient_repository, &patient_a).success == 1);
    assert(PatientRepository_append(&context->patient_repository, &patient_b).success == 1);

    result = RegistrationService_init(
        &context->service,
        &context->registration_repository,
        &context->patient_repository,
        &context->doctor_repository,
        &context->department_repository
    );
    assert(result.success == 1);
}

static void test_create_find_and_query_by_patient(void) {
    RegistrationServiceTestContext context;
    Registration first_created;
    Registration second_created;
    Registration loaded;
    LinkedList patient_records;
    Result result;

    setup_context(&context, "create_find_query");

    result = RegistrationService_create(
        &context.service,
        "PAT0001",
        "DOC0001",
        "DEP0001",
        "2026-03-20T08:15",
        &first_created
    );
    assert(result.success == 1);
    assert(strcmp(first_created.registration_id, "REG0001") == 0);
    assert(strcmp(first_created.registered_at, "2026-03-20T08:15") == 0);
    assert(first_created.status == REG_STATUS_PENDING);
    assert(strcmp(first_created.diagnosed_at, "") == 0);
    assert(strcmp(first_created.cancelled_at, "") == 0);

    result = RegistrationService_create(
        &context.service,
        "PAT0001",
        "DOC0001",
        "DEP0001",
        "2026-03-20T08:30",
        &second_created
    );
    assert(result.success == 1);
    assert(strcmp(second_created.registration_id, "REG0002") == 0);

    result = RegistrationService_find_by_registration_id(
        &context.service,
        "REG0002",
        &loaded
    );
    assert(result.success == 1);
    assert(strcmp(loaded.patient_id, "PAT0001") == 0);
    assert(strcmp(loaded.doctor_id, "DOC0001") == 0);
    assert(strcmp(loaded.department_id, "DEP0001") == 0);
    assert(strcmp(loaded.registered_at, "2026-03-20T08:30") == 0);

    LinkedList_init(&patient_records);
    result = RegistrationService_find_by_patient_id(
        &context.service,
        "PAT0001",
        0,
        &patient_records
    );
    assert(result.success == 1);
    assert(LinkedList_count(&patient_records) == 2);
    assert(strcmp(registration_at(&patient_records, 0)->registration_id, "REG0001") == 0);
    assert(strcmp(registration_at(&patient_records, 1)->registration_id, "REG0002") == 0);
    RegistrationRepository_clear_list(&patient_records);
}

static void test_create_rejects_invalid_related_entities(void) {
    RegistrationServiceTestContext context;
    Registration created;
    Result result;

    setup_context(&context, "invalid_related_entities");

    result = RegistrationService_create(
        &context.service,
        "PAT9999",
        "DOC0001",
        "DEP0001",
        "2026-03-20T09:00",
        &created
    );
    assert(result.success == 0);

    result = RegistrationService_create(
        &context.service,
        "PAT0001",
        "DOC9999",
        "DEP0001",
        "2026-03-20T09:05",
        &created
    );
    assert(result.success == 0);

    result = RegistrationService_create(
        &context.service,
        "PAT0001",
        "DOC0001",
        "DEP9999",
        "2026-03-20T09:10",
        &created
    );
    assert(result.success == 0);

    result = RegistrationService_create(
        &context.service,
        "PAT0001",
        "DOC0001",
        "DEP0002",
        "2026-03-20T09:15",
        &created
    );
    assert(result.success == 0);
}

static void test_cancel_pending_registration(void) {
    RegistrationServiceTestContext context;
    Registration created;
    Registration cancelled;
    Registration loaded;
    Result result;

    setup_context(&context, "cancel_pending");

    result = RegistrationService_create(
        &context.service,
        "PAT0002",
        "DOC0002",
        "DEP0002",
        "2026-03-20T10:00",
        &created
    );
    assert(result.success == 1);

    result = RegistrationService_cancel(
        &context.service,
        created.registration_id,
        "2026-03-20T10:15",
        &cancelled
    );
    assert(result.success == 1);
    assert(cancelled.status == REG_STATUS_CANCELLED);
    assert(strcmp(cancelled.cancelled_at, "2026-03-20T10:15") == 0);

    result = RegistrationService_find_by_registration_id(
        &context.service,
        created.registration_id,
        &loaded
    );
    assert(result.success == 1);
    assert(loaded.status == REG_STATUS_CANCELLED);
    assert(strcmp(loaded.cancelled_at, "2026-03-20T10:15") == 0);
}

static void test_cancel_rejects_diagnosed_registration(void) {
    RegistrationServiceTestContext context;
    Registration diagnosed;
    Registration loaded;
    Result result;

    setup_context(&context, "cancel_diagnosed");

    diagnosed = make_registration(
        "REG0007",
        "PAT0001",
        "DOC0001",
        "DEP0001",
        "2026-03-20T11:00",
        REG_STATUS_DIAGNOSED,
        "2026-03-20T11:20",
        ""
    );
    result = RegistrationRepository_append(&context.registration_repository, &diagnosed);
    assert(result.success == 1);

    result = RegistrationService_cancel(
        &context.service,
        "REG0007",
        "2026-03-20T11:30",
        &loaded
    );
    assert(result.success == 0);

    result = RegistrationService_find_by_registration_id(&context.service, "REG0007", &loaded);
    assert(result.success == 1);
    assert(loaded.status == REG_STATUS_DIAGNOSED);
    assert(strcmp(loaded.diagnosed_at, "2026-03-20T11:20") == 0);
    assert(strcmp(loaded.cancelled_at, "") == 0);
}

static void test_query_by_doctor_with_status_and_time_filter(void) {
    RegistrationServiceTestContext context;
    RegistrationRepositoryFilter filter;
    Registration first;
    Registration second;
    Registration third;
    LinkedList registrations;
    Result result;

    setup_context(&context, "query_by_doctor");

    first = make_registration(
        "REG0001",
        "PAT0001",
        "DOC0001",
        "DEP0001",
        "2026-03-20T08:00",
        REG_STATUS_PENDING,
        "",
        ""
    );
    second = make_registration(
        "REG0002",
        "PAT0002",
        "DOC0001",
        "DEP0001",
        "2026-03-20T08:30",
        REG_STATUS_PENDING,
        "",
        ""
    );
    third = make_registration(
        "REG0003",
        "PAT0001",
        "DOC0001",
        "DEP0001",
        "2026-03-20T09:00",
        REG_STATUS_DIAGNOSED,
        "2026-03-20T09:20",
        ""
    );

    assert(RegistrationRepository_append(&context.registration_repository, &first).success == 1);
    assert(RegistrationRepository_append(&context.registration_repository, &second).success == 1);
    assert(RegistrationRepository_append(&context.registration_repository, &third).success == 1);

    RegistrationRepositoryFilter_init(&filter);
    filter.use_status = 1;
    filter.status = REG_STATUS_PENDING;
    filter.registered_at_from = "2026-03-20T08:10";
    filter.registered_at_to = "2026-03-20T08:40";

    LinkedList_init(&registrations);
    result = RegistrationService_find_by_doctor_id(
        &context.service,
        "DOC0001",
        &filter,
        &registrations
    );
    assert(result.success == 1);
    assert(LinkedList_count(&registrations) == 1);
    assert(strcmp(registration_at(&registrations, 0)->registration_id, "REG0002") == 0);
    RegistrationRepository_clear_list(&registrations);
}

int main(void) {
    test_create_find_and_query_by_patient();
    test_create_rejects_invalid_related_entities();
    test_cancel_pending_registration();
    test_cancel_rejects_diagnosed_registration();
    test_query_by_doctor_with_status_and_time_filter();
    return 0;
}
