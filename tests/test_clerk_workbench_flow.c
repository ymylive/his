#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "domain/Department.h"
#include "domain/Doctor.h"
#include "domain/Patient.h"
#include "repository/DepartmentRepository.h"
#include "repository/DoctorRepository.h"
#include "repository/PatientRepository.h"
#include "repository/RegistrationRepository.h"
#include "ui/MenuApplication.h"

typedef struct ClerkWorkbenchTestContext {
    char user_path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    char patient_path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    char department_path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    char doctor_path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    char registration_path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    char visit_path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    char examination_path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    char ward_path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    char bed_path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    char admission_path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    char medicine_path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    char dispense_record_path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    MenuApplicationPaths paths;
} ClerkWorkbenchTestContext;

static void make_path(char *buffer, size_t buffer_size, const char *test_name, const char *file_name) {
    static long sequence = 1;

    snprintf(
        buffer,
        buffer_size,
        "build/test_clerk_workbench/%s_%ld_%ld_%s",
        test_name,
        (long)time(0),
        sequence++,
        file_name
    );
    remove(buffer);
}

static void setup_context(ClerkWorkbenchTestContext *context, const char *test_name) {
    memset(context, 0, sizeof(*context));
    make_path(context->user_path, sizeof(context->user_path), test_name, "users.txt");
    make_path(context->patient_path, sizeof(context->patient_path), test_name, "patients.txt");
    make_path(context->department_path, sizeof(context->department_path), test_name, "departments.txt");
    make_path(context->doctor_path, sizeof(context->doctor_path), test_name, "doctors.txt");
    make_path(context->registration_path, sizeof(context->registration_path), test_name, "registrations.txt");
    make_path(context->visit_path, sizeof(context->visit_path), test_name, "visits.txt");
    make_path(context->examination_path, sizeof(context->examination_path), test_name, "examinations.txt");
    make_path(context->ward_path, sizeof(context->ward_path), test_name, "wards.txt");
    make_path(context->bed_path, sizeof(context->bed_path), test_name, "beds.txt");
    make_path(context->admission_path, sizeof(context->admission_path), test_name, "admissions.txt");
    make_path(context->medicine_path, sizeof(context->medicine_path), test_name, "medicines.txt");
    make_path(context->dispense_record_path, sizeof(context->dispense_record_path), test_name, "dispense_records.txt");

    context->paths.user_path = context->user_path;
    context->paths.patient_path = context->patient_path;
    context->paths.department_path = context->department_path;
    context->paths.doctor_path = context->doctor_path;
    context->paths.registration_path = context->registration_path;
    context->paths.visit_path = context->visit_path;
    context->paths.examination_path = context->examination_path;
    context->paths.ward_path = context->ward_path;
    context->paths.bed_path = context->bed_path;
    context->paths.admission_path = context->admission_path;
    context->paths.medicine_path = context->medicine_path;
    context->paths.dispense_record_path = context->dispense_record_path;
}

static void seed_department_doctor(ClerkWorkbenchTestContext *context) {
    DepartmentRepository department_repository;
    DoctorRepository doctor_repository;
    Department department = { "DEP0201", "外科", "门诊二层", "普通外科" };
    Doctor doctor = { "DOC0201", "王磊", "副主任医师", "DEP0201", "周一至周六", DOCTOR_STATUS_ACTIVE };
    Result result = DepartmentRepository_init(&department_repository, context->department_path);

    assert(result.success == 1);
    result = DoctorRepository_init(&doctor_repository, context->doctor_path);
    assert(result.success == 1);
    assert(DepartmentRepository_save(&department_repository, &department).success == 1);
    assert(DoctorRepository_save(&doctor_repository, &doctor).success == 1);
}

static void test_clerk_reception_flow(void) {
    ClerkWorkbenchTestContext context;
    MenuApplication application;
    PatientRepository patient_repository;
    RegistrationRepository registration_repository;
    Patient patient;
    Registration registration;
    char buffer[2048];
    Result result;

    setup_context(&context, "clerk_reception");
    seed_department_doctor(&context);

    result = MenuApplication_init(&application, &context.paths);
    assert(result.success == 1);
    result = PatientRepository_init(&patient_repository, context.patient_path);
    assert(result.success == 1);
    result = RegistrationRepository_init(&registration_repository, context.registration_path);
    assert(result.success == 1);

    memset(&patient, 0, sizeof(patient));
    strcpy(patient.patient_id, "PATC001");
    strcpy(patient.name, "周宁");
    patient.gender = PATIENT_GENDER_MALE;
    patient.age = 34;
    strcpy(patient.contact, "13800000002");
    strcpy(patient.id_card, "110101199001010022");
    strcpy(patient.allergy, "青霉素");
    strcpy(patient.medical_history, "胃炎");
    strcpy(patient.remarks, "前台建档");

    result = MenuApplication_add_patient(&application, &patient, buffer, sizeof(buffer));
    assert(result.success == 1);
    assert(strstr(buffer, "PATC001") != 0);
    assert(PatientRepository_find_by_id(&patient_repository, "PATC001", &patient).success == 1);

    strcpy(patient.contact, "13800009999");
    strcpy(patient.medical_history, "胃炎复诊");
    result = MenuApplication_update_patient(&application, &patient, buffer, sizeof(buffer));
    assert(result.success == 1);
    assert(strstr(buffer, "更新") != 0 || strstr(buffer, "成功") != 0);

    memset(buffer, 0, sizeof(buffer));
    result = MenuApplication_query_patient(&application, "PATC001", buffer, sizeof(buffer));
    assert(result.success == 1);
    assert(strstr(buffer, "周宁") != 0);
    assert(strstr(buffer, "13800009999") != 0);

    memset(buffer, 0, sizeof(buffer));
    result = MenuApplication_create_registration(
        &application,
        "PATC001",
        "DOC0201",
        "DEP0201",
        "2026-03-27 13:00",
        buffer,
        sizeof(buffer)
    );
    assert(result.success == 1);
    assert(strstr(buffer, "REG0001") != 0);
    assert(RegistrationRepository_find_by_registration_id(&registration_repository, "REG0001", &registration).success == 1);

    memset(buffer, 0, sizeof(buffer));
    result = MenuApplication_query_registration(&application, "REG0001", buffer, sizeof(buffer));
    assert(result.success == 1);
    assert(strstr(buffer, "PATC001") != 0);
    assert(strstr(buffer, "待诊") != 0);

    memset(buffer, 0, sizeof(buffer));
    result = MenuApplication_query_registrations_by_patient(&application, "PATC001", buffer, sizeof(buffer));
    assert(result.success == 1);
    assert(strstr(buffer, "REG0001") != 0);

    memset(buffer, 0, sizeof(buffer));
    result = MenuApplication_cancel_registration(&application, "REG0001", "2026-03-27 13:20", buffer, sizeof(buffer));
    assert(result.success == 1);
    assert(strstr(buffer, "已取消") != 0 || strstr(buffer, "取消") != 0);
}

int main(void) {
    test_clerk_reception_flow();
    return 0;
}
