#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "domain/Department.h"
#include "domain/Doctor.h"
#include "domain/Medicine.h"
#include "domain/Patient.h"
#include "repository/DepartmentRepository.h"
#include "repository/DoctorRepository.h"
#include "repository/PatientRepository.h"
#include "service/AuthService.h"
#include "ui/DesktopAdapters.h"

typedef struct PatientWorkbenchTestContext {
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
} PatientWorkbenchTestContext;

static void make_path(char *buffer, size_t buffer_size, const char *test_name, const char *file_name) {
    static long sequence = 1;

    snprintf(
        buffer,
        buffer_size,
        "build/test_patient_workbench/%s_%ld_%ld_%s",
        test_name,
        (long)time(0),
        sequence++,
        file_name
    );
    remove(buffer);
}

static void setup_context(PatientWorkbenchTestContext *context, const char *test_name) {
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

static void seed_patient(const PatientWorkbenchTestContext *context, const char *patient_id, const char *name) {
    PatientRepository repository;
    Patient patient;
    Result result = PatientRepository_init(&repository, context->patient_path);

    assert(result.success == 1);
    memset(&patient, 0, sizeof(patient));
    strcpy(patient.patient_id, patient_id);
    strcpy(patient.name, name);
    patient.gender = PATIENT_GENDER_FEMALE;
    patient.age = 25;
    strcpy(patient.contact, "13800000001");
    strcpy(patient.id_card, "110101199901010011");
    strcpy(patient.allergy, "None");
    strcpy(patient.medical_history, "Seasonal allergy");
    strcpy(patient.remarks, "patient flow");
    assert(PatientRepository_append(&repository, &patient).success == 1);
}

static void seed_department_doctor_and_user(const PatientWorkbenchTestContext *context) {
    DepartmentRepository department_repository;
    DoctorRepository doctor_repository;
    AuthService auth_service;
    Department department = { "DEP0101", "内科", "门诊一层", "综合内科" };
    Doctor doctor = { "DOC0101", "张明", "主治医师", "DEP0101", "周一至周五", DOCTOR_STATUS_ACTIVE };
    Result result = DepartmentRepository_init(&department_repository, context->department_path);

    assert(result.success == 1);
    result = DoctorRepository_init(&doctor_repository, context->doctor_path);
    assert(result.success == 1);
    assert(DepartmentRepository_save(&department_repository, &department).success == 1);
    assert(DoctorRepository_save(&doctor_repository, &doctor).success == 1);

    result = AuthService_init(&auth_service, context->user_path, context->patient_path);
    assert(result.success == 1);
    assert(AuthService_register_user(&auth_service, "PATP001", "patient-pass", USER_ROLE_PATIENT).success == 1);
}

static void extract_id_after_prefix(
    const char *text,
    const char *prefix,
    char *out_id,
    size_t out_id_size
) {
    const char *start = strstr(text, prefix);
    size_t length = 0;

    assert(start != 0);
    start += strlen(prefix);
    while (*start == ' ') {
        start++;
    }

    while (start[length] != '\0' &&
           start[length] != ' ' &&
           start[length] != '|' &&
           start[length] != '\n') {
        length++;
    }

    assert(length > 0);
    assert(length < out_id_size);
    memcpy(out_id, start, length);
    out_id[length] = '\0';
}

static void test_patient_self_service_chain(void) {
    PatientWorkbenchTestContext context;
    MenuApplication application;
    User user;
    Registration registration;
    LinkedList dispense_history;
    DispenseRecord *record = 0;
    Medicine medicine;
    char visit_buffer[2048];
    char examination_id[HIS_DOMAIN_ID_CAPACITY];
    char visit_id[HIS_DOMAIN_ID_CAPACITY];
    char buffer[2048];
    Result result;

    setup_context(&context, "patient_self_service");
    seed_patient(&context, "PATP001", "林悦");
    seed_department_doctor_and_user(&context);

    result = DesktopAdapters_init_application(&application, &context.paths);
    assert(result.success == 1);

    result = DesktopAdapters_login(&application, "PATP001", "patient-pass", USER_ROLE_PATIENT, &user);
    assert(result.success == 1);
    assert(user.role == USER_ROLE_PATIENT);

    memset(&registration, 0, sizeof(registration));
    result = DesktopAdapters_submit_registration(
        &application,
        "PATP001",
        "DOC0101",
        "DEP0101",
        "2026-03-27 09:00",
        &registration
    );
    assert(result.success == 1);
    assert(strcmp(registration.patient_id, "PATP001") == 0);

    memset(visit_buffer, 0, sizeof(visit_buffer));
    result = DesktopAdapters_create_visit_record(
        &application,
        registration.registration_id,
        "发热两天",
        "上呼吸道感染",
        "口服退热药，多休息",
        1,
        0,
        1,
        "2026-03-27 09:30",
        visit_buffer,
        sizeof(visit_buffer)
    );
    assert(result.success == 1);
    extract_id_after_prefix(visit_buffer, "看诊记录已创建:", visit_id, sizeof(visit_id));

    memset(&medicine, 0, sizeof(medicine));
    strcpy(medicine.medicine_id, "MED0101");
    strcpy(medicine.name, "布洛芬");
    strcpy(medicine.department_id, "DEP0101");
    medicine.price = 18.50;
    medicine.stock = 16;
    medicine.low_stock_threshold = 5;
    result = DesktopAdapters_add_medicine(&application, &medicine, buffer, sizeof(buffer));
    assert(result.success == 1);

    memset(buffer, 0, sizeof(buffer));
    result = DesktopAdapters_create_examination_record(
        &application,
        visit_id,
        "血常规",
        "检验",
        "2026-03-27 09:40",
        buffer,
        sizeof(buffer)
    );
    assert(result.success == 1);
    extract_id_after_prefix(buffer, "检查记录已创建:", examination_id, sizeof(examination_id));

    memset(buffer, 0, sizeof(buffer));
    result = DesktopAdapters_complete_examination_record(
        &application,
        examination_id,
        "白细胞略高",
        "2026-03-27 10:10",
        buffer,
        sizeof(buffer)
    );
    assert(result.success == 1);
    assert(strstr(buffer, "检查结果已回写") != 0);

    memset(buffer, 0, sizeof(buffer));
    result = DesktopAdapters_dispense_medicine(
        &application,
        "PATP001",
        visit_id,
        "MED0101",
        2,
        "PHA0101",
        "2026-03-27 10:20",
        buffer,
        sizeof(buffer)
    );
    assert(result.success == 1);
    assert(strstr(buffer, "发药") != 0);

    memset(buffer, 0, sizeof(buffer));
    result = DesktopAdapters_query_registrations_by_patient(&application, "PATP001", buffer, sizeof(buffer));
    assert(result.success == 1);
    assert(strstr(buffer, registration.registration_id) != 0);

    memset(buffer, 0, sizeof(buffer));
    result = DesktopAdapters_query_patient_history(&application, "PATP001", buffer, sizeof(buffer));
    assert(result.success == 1);
    assert(strstr(buffer, "visits=1") != 0);
    assert(strstr(buffer, "examinations=1") != 0);

    memset(buffer, 0, sizeof(buffer));
    result = DesktopAdapters_query_medicine_detail(&application, "MED0101", 1, buffer, sizeof(buffer));
    assert(result.success == 1);
    assert(strstr(buffer, "当前系统未维护用法说明") != 0);

    LinkedList_init(&dispense_history);
    result = DesktopAdapters_load_dispense_history(&application, "PATP001", &dispense_history);
    assert(result.success == 1);
    assert(LinkedList_count(&dispense_history) == 1);
    record = (DispenseRecord *)dispense_history.head->data;
    assert(record != 0);
    assert(strcmp(record->prescription_id, visit_id) == 0);
    LinkedList_clear(&dispense_history, free);
}

int main(void) {
    test_patient_self_service_chain();
    return 0;
}
