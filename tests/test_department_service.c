#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "service/DepartmentService.h"

static void build_test_path(
    char *buffer,
    size_t buffer_size,
    const char *file_name
) {
    snprintf(
        buffer,
        buffer_size,
        "build/test_department_service_data/%ld_%lu_%s",
        (long)time(0),
        (unsigned long)clock(),
        file_name
    );
}

static void test_department_service_add_update_and_find(void) {
    char path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    DepartmentService service;
    Department created = {
        "DEP5001",
        "Cardiology",
        "Building A",
        "Heart disease clinic"
    };
    Department updated = {
        "DEP5001",
        "Cardiology Center",
        "Building B",
        "Expanded cardiology services"
    };
    Department loaded;
    Result result;

    build_test_path(path, sizeof(path), "departments.txt");

    result = DepartmentService_init(&service, path);
    assert(result.success == 1);

    result = DepartmentService_add(&service, &created);
    assert(result.success == 1);

    memset(&loaded, 0, sizeof(loaded));
    result = DepartmentService_get_by_id(&service, "DEP5001", &loaded);
    assert(result.success == 1);
    assert(strcmp(loaded.name, "Cardiology") == 0);
    assert(strcmp(loaded.location, "Building A") == 0);

    result = DepartmentService_update(&service, &updated);
    assert(result.success == 1);

    memset(&loaded, 0, sizeof(loaded));
    result = DepartmentService_get_by_id(&service, "DEP5001", &loaded);
    assert(result.success == 1);
    assert(strcmp(loaded.name, "Cardiology Center") == 0);
    assert(strcmp(loaded.location, "Building B") == 0);
    assert(strcmp(loaded.description, "Expanded cardiology services") == 0);
}

static void test_department_service_rejects_invalid_duplicate_and_missing_update(void) {
    char path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    DepartmentService service;
    Department missing_name = {
        "DEP5002",
        "   ",
        "Building C",
        "Missing name must fail"
    };
    Department valid = {
        "DEP5002",
        "Neurology",
        "Building C",
        "Brain and nerve clinic"
    };
    Department duplicate = {
        "DEP5002",
        "Neurology Annex",
        "Building D",
        "Duplicate id must fail"
    };
    Department missing = {
        "DEP5999",
        "Unknown",
        "Building X",
        "Update target missing"
    };
    Result result;

    build_test_path(path, sizeof(path), "departments_invalid.txt");

    result = DepartmentService_init(&service, path);
    assert(result.success == 1);

    result = DepartmentService_add(&service, &missing_name);
    assert(result.success == 0);

    result = DepartmentService_add(&service, &valid);
    assert(result.success == 1);

    result = DepartmentService_add(&service, &duplicate);
    assert(result.success == 0);

    result = DepartmentService_update(&service, &missing);
    assert(result.success == 0);
}

void test_department_service_run(void) {
    test_department_service_add_update_and_find();
    test_department_service_rejects_invalid_duplicate_and_missing_update();
}
