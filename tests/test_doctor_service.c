#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "service/DepartmentService.h"
#include "service/DoctorService.h"

void test_department_service_run(void);

static void build_test_path(
    char *buffer,
    size_t buffer_size,
    const char *file_name
) {
    snprintf(
        buffer,
        buffer_size,
        "build/test_doctor_service_data/%ld_%lu_%s",
        (long)time(0),
        (unsigned long)clock(),
        file_name
    );
}

static void seed_department(
    const char *department_path,
    const Department *department
) {
    DepartmentService department_service;
    Result result = DepartmentService_init(&department_service, department_path);

    assert(result.success == 1);
    result = DepartmentService_add(&department_service, department);
    assert(result.success == 1);
}

static void test_doctor_service_add_find_and_list_by_department(void) {
    char department_path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    char doctor_path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    DoctorService service;
    Department cardiology = {
        "DEP6001",
        "Cardiology",
        "Block A",
        "Cardiology clinic"
    };
    Department surgery = {
        "DEP6002",
        "Surgery",
        "Block B",
        "Surgical clinic"
    };
    Doctor doctor_a = {
        "DOC6001",
        "Dr.Alice",
        "Chief Physician",
        "DEP6001",
        "Mon AM;Wed PM",
        DOCTOR_STATUS_ACTIVE
    };
    Doctor doctor_b = {
        "DOC6002",
        "Dr.Bob",
        "Attending",
        "DEP6002",
        "Tue AM;Thu PM",
        DOCTOR_STATUS_ACTIVE
    };
    Doctor loaded;
    LinkedList doctors_in_cardiology;
    Result result;

    build_test_path(department_path, sizeof(department_path), "departments.txt");
    build_test_path(doctor_path, sizeof(doctor_path), "doctors.txt");

    seed_department(department_path, &cardiology);
    seed_department(department_path, &surgery);

    result = DoctorService_init(&service, doctor_path, department_path);
    assert(result.success == 1);

    result = DoctorService_add(&service, &doctor_a);
    assert(result.success == 1);
    result = DoctorService_add(&service, &doctor_b);
    assert(result.success == 1);

    memset(&loaded, 0, sizeof(loaded));
    result = DoctorService_get_by_id(&service, "DOC6001", &loaded);
    assert(result.success == 1);
    assert(strcmp(loaded.name, "Dr.Alice") == 0);
    assert(strcmp(loaded.title, "Chief Physician") == 0);
    assert(strcmp(loaded.schedule, "Mon AM;Wed PM") == 0);

    result = DoctorService_list_by_department(
        &service,
        "DEP6001",
        &doctors_in_cardiology
    );
    assert(result.success == 1);
    assert(LinkedList_count(&doctors_in_cardiology) == 1);
    assert(
        strcmp(
            ((Doctor *)doctors_in_cardiology.head->data)->doctor_id,
            "DOC6001"
        ) == 0
    );
    DoctorService_clear_list(&doctors_in_cardiology);
}

static void test_doctor_service_rejects_invalid_duplicate_and_unknown_department(void) {
    char department_path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    char doctor_path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    DoctorService service;
    Department department = {
        "DEP6101",
        "Pediatrics",
        "Block C",
        "Children clinic"
    };
    Doctor missing_id = {
        "",
        "Dr.Carol",
        "Attending",
        "DEP6101",
        "Fri AM",
        DOCTOR_STATUS_ACTIVE
    };
    Doctor missing_name = {
        "DOC6101",
        "   ",
        "Attending",
        "DEP6101",
        "Fri AM",
        DOCTOR_STATUS_ACTIVE
    };
    Doctor missing_title = {
        "DOC6102",
        "Dr.David",
        "",
        "DEP6101",
        "Fri PM",
        DOCTOR_STATUS_ACTIVE
    };
    Doctor missing_department = {
        "DOC6103",
        "Dr.Erin",
        "Resident",
        " ",
        "Sat AM",
        DOCTOR_STATUS_ACTIVE
    };
    Doctor missing_schedule = {
        "DOC6104",
        "Dr.Frank",
        "Resident",
        "DEP6101",
        "  ",
        DOCTOR_STATUS_ACTIVE
    };
    Doctor invalid_status = {
        "DOC6105",
        "Dr.Grace",
        "Resident",
        "DEP6101",
        "Sun AM",
        (DoctorStatus)99
    };
    Doctor valid = {
        "DOC6106",
        "Dr.Helen",
        "Chief Physician",
        "DEP6101",
        "Mon-Fri AM",
        DOCTOR_STATUS_ACTIVE
    };
    Doctor duplicate_id = {
        "DOC6106",
        "Dr.Ian",
        "Attending",
        "DEP6101",
        "Tue PM",
        DOCTOR_STATUS_ACTIVE
    };
    Doctor unknown_department = {
        "DOC6107",
        "Dr.Jane",
        "Attending",
        "DEP6199",
        "Wed AM",
        DOCTOR_STATUS_ACTIVE
    };
    Result result;

    build_test_path(department_path, sizeof(department_path), "departments_invalid.txt");
    build_test_path(doctor_path, sizeof(doctor_path), "doctors_invalid.txt");

    seed_department(department_path, &department);

    result = DoctorService_init(&service, doctor_path, department_path);
    assert(result.success == 1);

    result = DoctorService_add(&service, &missing_id);
    assert(result.success == 0);
    result = DoctorService_add(&service, &missing_name);
    assert(result.success == 0);
    result = DoctorService_add(&service, &missing_title);
    assert(result.success == 0);
    result = DoctorService_add(&service, &missing_department);
    assert(result.success == 0);
    result = DoctorService_add(&service, &missing_schedule);
    assert(result.success == 0);
    result = DoctorService_add(&service, &invalid_status);
    assert(result.success == 0);

    result = DoctorService_add(&service, &valid);
    assert(result.success == 1);

    result = DoctorService_add(&service, &duplicate_id);
    assert(result.success == 0);

    result = DoctorService_add(&service, &unknown_department);
    assert(result.success == 0);
}

void test_doctor_service_run(void) {
    test_doctor_service_add_find_and_list_by_department();
    test_doctor_service_rejects_invalid_duplicate_and_unknown_department();
}

int main(void) {
    test_department_service_run();
    test_doctor_service_run();
    return 0;
}
