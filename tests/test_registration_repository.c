#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "repository/RegistrationRepository.h"

static const char *TEST_REGISTRATION_PATH =
    "build/test_registration_repository_data/registrations.txt";
static const char *TEST_REGISTRATION_HEADER =
    "registration_id|patient_id|doctor_id|department_id|registered_at|status|diagnosed_at|cancelled_at";

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

static Registration *clone_registration(const Registration *source) {
    Registration *copy = (Registration *)malloc(sizeof(*copy));

    assert(copy != 0);
    *copy = *source;
    return copy;
}

static const Registration *registration_at(const LinkedList *list, size_t index) {
    LinkedListNode *node = 0;
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

static void assert_header_written(void) {
    FILE *file = fopen(TEST_REGISTRATION_PATH, "r");
    char line[256];

    assert(file != 0);
    assert(fgets(line, sizeof(line), file) != 0);
    fclose(file);

    line[strcspn(line, "\r\n")] = '\0';
    assert(strcmp(line, TEST_REGISTRATION_HEADER) == 0);
}

static RegistrationRepository seed_repository(
    Registration *pending,
    Registration *diagnosed,
    Registration *cancelled
) {
    RegistrationRepository repository;
    Result result;

    remove(TEST_REGISTRATION_PATH);

    *pending = make_registration(
        "REG0001",
        "PAT0001",
        "DOC0001",
        "DEP0001",
        "2026-03-20T08:00",
        REG_STATUS_PENDING,
        "",
        ""
    );
    *diagnosed = make_registration(
        "REG0002",
        "PAT0002",
        "DOC0002",
        "DEP0001",
        "2026-03-20T09:00",
        REG_STATUS_DIAGNOSED,
        "2026-03-20T09:30",
        ""
    );
    *cancelled = make_registration(
        "REG0003",
        "PAT0001",
        "DOC0003",
        "DEP0002",
        "2026-03-21T10:00",
        REG_STATUS_CANCELLED,
        "",
        "2026-03-21T10:30"
    );

    result = RegistrationRepository_init(&repository, TEST_REGISTRATION_PATH);
    assert(result.success == 1);

    result = RegistrationRepository_append(&repository, pending);
    assert(result.success == 1);

    result = RegistrationRepository_append(&repository, diagnosed);
    assert(result.success == 1);

    result = RegistrationRepository_append(&repository, cancelled);
    assert(result.success == 1);

    assert_header_written();
    return repository;
}

static void test_append_and_find_by_registration_id(void) {
    RegistrationRepository repository;
    Registration pending;
    Registration diagnosed;
    Registration cancelled;
    Registration loaded;
    Result result;

    repository = seed_repository(&pending, &diagnosed, &cancelled);

    result = RegistrationRepository_find_by_registration_id(&repository, "REG0002", &loaded);
    assert(result.success == 1);
    assert(strcmp(loaded.registration_id, diagnosed.registration_id) == 0);
    assert(strcmp(loaded.patient_id, diagnosed.patient_id) == 0);
    assert(loaded.status == REG_STATUS_DIAGNOSED);
    assert(strcmp(loaded.diagnosed_at, "2026-03-20T09:30") == 0);
    assert(strcmp(loaded.cancelled_at, "") == 0);

    result = RegistrationRepository_find_by_registration_id(&repository, "REG9999", &loaded);
    assert(result.success == 0);

    result = RegistrationRepository_find_by_registration_id(&repository, "REG0003", &loaded);
    assert(result.success == 1);
    assert(loaded.status == REG_STATUS_CANCELLED);
    assert(strcmp(loaded.cancelled_at, "2026-03-21T10:30") == 0);
}

static void test_find_by_patient_id_filters(void) {
    RegistrationRepository repository;
    Registration pending;
    Registration diagnosed;
    Registration cancelled;
    LinkedList matches;
    RegistrationRepositoryFilter filter;
    Result result;
    const Registration *record = 0;

    repository = seed_repository(&pending, &diagnosed, &cancelled);

    LinkedList_init(&matches);
    result = RegistrationRepository_find_by_patient_id(&repository, "PAT0001", 0, &matches);
    assert(result.success == 1);
    assert(LinkedList_count(&matches) == 2);
    assert(strcmp(registration_at(&matches, 0)->registration_id, "REG0001") == 0);
    assert(strcmp(registration_at(&matches, 1)->registration_id, "REG0003") == 0);
    RegistrationRepository_clear_list(&matches);

    LinkedList_init(&matches);
    RegistrationRepositoryFilter_init(&filter);
    filter.use_status = 1;
    filter.status = REG_STATUS_CANCELLED;
    result = RegistrationRepository_find_by_patient_id(&repository, "PAT0001", &filter, &matches);
    assert(result.success == 1);
    assert(LinkedList_count(&matches) == 1);
    record = registration_at(&matches, 0);
    assert(record != 0);
    assert(record->status == REG_STATUS_CANCELLED);
    assert(strcmp(record->registration_id, "REG0003") == 0);
    RegistrationRepository_clear_list(&matches);

    LinkedList_init(&matches);
    RegistrationRepositoryFilter_init(&filter);
    filter.registered_at_from = "2026-03-20T08:00";
    filter.registered_at_to = "2026-03-20T08:00";
    result = RegistrationRepository_find_by_patient_id(&repository, "PAT0001", &filter, &matches);
    assert(result.success == 1);
    assert(LinkedList_count(&matches) == 1);
    record = registration_at(&matches, 0);
    assert(record != 0);
    assert(strcmp(record->registration_id, "REG0001") == 0);
    RegistrationRepository_clear_list(&matches);

    LinkedList_init(&matches);
    RegistrationRepositoryFilter_init(&filter);
    filter.use_status = 1;
    filter.status = REG_STATUS_CANCELLED;
    filter.registered_at_from = "2026-03-21T00:00";
    filter.registered_at_to = "2026-03-21T23:59";
    result = RegistrationRepository_find_by_patient_id(&repository, "PAT0001", &filter, &matches);
    assert(result.success == 1);
    assert(LinkedList_count(&matches) == 1);
    record = registration_at(&matches, 0);
    assert(record != 0);
    assert(strcmp(record->cancelled_at, "2026-03-21T10:30") == 0);
    RegistrationRepository_clear_list(&matches);
}

static void test_save_all_rewrites_table(void) {
    RegistrationRepository repository;
    Registration pending;
    Registration diagnosed;
    Registration cancelled;
    Registration replacement;
    Registration loaded;
    LinkedList rewrite_records;
    LinkedList patient_records;
    Result result;

    repository = seed_repository(&pending, &diagnosed, &cancelled);
    replacement = make_registration(
        "REG0009",
        "PAT0009",
        "DOC0009",
        "DEP0009",
        "2026-03-22T11:00",
        REG_STATUS_PENDING,
        "",
        ""
    );

    LinkedList_init(&rewrite_records);
    assert(LinkedList_append(&rewrite_records, clone_registration(&cancelled)) == 1);
    assert(LinkedList_append(&rewrite_records, clone_registration(&replacement)) == 1);

    result = RegistrationRepository_save_all(&repository, &rewrite_records);
    assert(result.success == 1);
    RegistrationRepository_clear_list(&rewrite_records);
    assert_header_written();

    result = RegistrationRepository_find_by_registration_id(&repository, "REG0002", &loaded);
    assert(result.success == 0);

    result = RegistrationRepository_find_by_registration_id(&repository, "REG0009", &loaded);
    assert(result.success == 1);
    assert(strcmp(loaded.patient_id, "PAT0009") == 0);
    assert(loaded.status == REG_STATUS_PENDING);

    LinkedList_init(&patient_records);
    result = RegistrationRepository_find_by_patient_id(&repository, "PAT0001", 0, &patient_records);
    assert(result.success == 1);
    assert(LinkedList_count(&patient_records) == 1);
    assert(strcmp(registration_at(&patient_records, 0)->registration_id, "REG0003") == 0);
    RegistrationRepository_clear_list(&patient_records);
}

int main(void) {
    test_append_and_find_by_registration_id();
    test_find_by_patient_id_filters();
    test_save_all_rewrites_table();
    return 0;
}
