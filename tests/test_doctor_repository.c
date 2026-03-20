#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "common/LinkedList.h"
#include "domain/Department.h"
#include "domain/Doctor.h"
#include "domain/Medicine.h"
#include "repository/DepartmentRepository.h"
#include "repository/DoctorRepository.h"
#include "repository/MedicineRepository.h"

static void build_test_path(
    char *buffer,
    size_t buffer_size,
    const char *file_name
) {
    snprintf(
        buffer,
        buffer_size,
        "build/test_doctor_repository_data/%ld_%lu_%s",
        (long)time(0),
        (unsigned long)clock(),
        file_name
    );
}

static void assert_first_line_equals(const char *path, const char *expected) {
    char line[256];
    FILE *file = fopen(path, "r");

    assert(file != 0);
    assert(fgets(line, sizeof(line), file) != 0);
    fclose(file);

    line[strcspn(line, "\r\n")] = '\0';
    assert(strcmp(line, expected) == 0);
}

static Doctor *allocate_doctor(const Doctor *source) {
    Doctor *doctor = (Doctor *)malloc(sizeof(Doctor));

    assert(doctor != 0);
    *doctor = *source;
    return doctor;
}

static Medicine *allocate_medicine(const Medicine *source) {
    Medicine *medicine = (Medicine *)malloc(sizeof(Medicine));

    assert(medicine != 0);
    *medicine = *source;
    return medicine;
}

static Department *allocate_department(const Department *source) {
    Department *department = (Department *)malloc(sizeof(Department));

    assert(department != 0);
    *department = *source;
    return department;
}

static void test_doctor_repository_supports_append_find_filter_and_rewrite(void) {
    char path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    DoctorRepository repository;
    Doctor doctor_a = {
        "DOC1001",
        "Dr.Alice",
        "Chief",
        "DEP1001",
        "Mon-Fri AM",
        DOCTOR_STATUS_ACTIVE
    };
    Doctor doctor_b = {
        "DOC1002",
        "Dr.Bob",
        "Attending",
        "DEP2001",
        "Tue-Thu PM",
        DOCTOR_STATUS_INACTIVE
    };
    Doctor rewritten = {
        "DOC1003",
        "Dr.Chen",
        "Resident",
        "DEP1001",
        "Weekend",
        DOCTOR_STATUS_ACTIVE
    };
    Doctor found;
    LinkedList filtered;
    LinkedList rewritten_list;
    LinkedList loaded;
    Result result;

    build_test_path(path, sizeof(path), "doctors.txt");
    result = DoctorRepository_init(&repository, path);
    assert(result.success == 1);

    result = DoctorRepository_save(&repository, &doctor_a);
    assert(result.success == 1);
    result = DoctorRepository_save(&repository, &doctor_b);
    assert(result.success == 1);

    assert_first_line_equals(
        path,
        "doctor_id|name|title|department_id|schedule|status"
    );

    memset(&found, 0, sizeof(found));
    result = DoctorRepository_find_by_doctor_id(&repository, "DOC1002", &found);
    assert(result.success == 1);
    assert(strcmp(found.name, "Dr.Bob") == 0);
    assert(found.status == DOCTOR_STATUS_INACTIVE);

    result = DoctorRepository_find_by_department_id(&repository, "DEP1001", &filtered);
    assert(result.success == 1);
    assert(LinkedList_count(&filtered) == 1);
    assert(strcmp(((Doctor *)filtered.head->data)->doctor_id, "DOC1001") == 0);
    DoctorRepository_clear_list(&filtered);

    LinkedList_init(&rewritten_list);
    assert(LinkedList_append(&rewritten_list, allocate_doctor(&rewritten)) == 1);
    result = DoctorRepository_save_all(&repository, &rewritten_list);
    assert(result.success == 1);
    DoctorRepository_clear_list(&rewritten_list);

    result = DoctorRepository_load_all(&repository, &loaded);
    assert(result.success == 1);
    assert(LinkedList_count(&loaded) == 1);
    assert(strcmp(((Doctor *)loaded.head->data)->doctor_id, "DOC1003") == 0);
    DoctorRepository_clear_list(&loaded);

    result = DoctorRepository_find_by_doctor_id(&repository, "DOC1001", &found);
    assert(result.success == 0);
}

static void test_department_and_medicine_repository_smoke(void) {
    char department_path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    char medicine_path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    DepartmentRepository department_repository;
    MedicineRepository medicine_repository;
    Department department = {
        "DEP3001",
        "Pharmacy",
        "Floor3",
        "Storage and dispensing"
    };
    Medicine medicine = {
        "MED2001",
        "Amoxicillin",
        18.50,
        24,
        "DEP3001",
        6
    };
    Department loaded_department;
    Medicine loaded_medicine;
    LinkedList medicines;
    LinkedList departments;
    Result result;

    build_test_path(department_path, sizeof(department_path), "departments.txt");
    build_test_path(medicine_path, sizeof(medicine_path), "medicines.txt");

    result = DepartmentRepository_init(&department_repository, department_path);
    assert(result.success == 1);
    result = DepartmentRepository_save(&department_repository, &department);
    assert(result.success == 1);
    result = DepartmentRepository_find_by_department_id(
        &department_repository,
        "DEP3001",
        &loaded_department
    );
    assert(result.success == 1);
    assert(strcmp(loaded_department.name, "Pharmacy") == 0);

    LinkedList_init(&departments);
    assert(LinkedList_append(&departments, allocate_department(&department)) == 1);
    result = DepartmentRepository_save_all(&department_repository, &departments);
    assert(result.success == 1);
    DepartmentRepository_clear_list(&departments);

    result = MedicineRepository_init(&medicine_repository, medicine_path);
    assert(result.success == 1);
    result = MedicineRepository_save(&medicine_repository, &medicine);
    assert(result.success == 1);
    result = MedicineRepository_find_by_medicine_id(
        &medicine_repository,
        "MED2001",
        &loaded_medicine
    );
    assert(result.success == 1);
    assert(strcmp(loaded_medicine.name, "Amoxicillin") == 0);
    assert(loaded_medicine.stock == 24);

    LinkedList_init(&medicines);
    assert(LinkedList_append(&medicines, allocate_medicine(&medicine)) == 1);
    result = MedicineRepository_save_all(&medicine_repository, &medicines);
    assert(result.success == 1);
    MedicineRepository_clear_list(&medicines);

    result = MedicineRepository_load_all(&medicine_repository, &medicines);
    assert(result.success == 1);
    assert(LinkedList_count(&medicines) == 1);
    assert(strcmp(((Medicine *)medicines.head->data)->medicine_id, "MED2001") == 0);
    MedicineRepository_clear_list(&medicines);
}

int main(void) {
    test_doctor_repository_supports_append_find_filter_and_rewrite();
    test_department_and_medicine_repository_smoke();
    return 0;
}
