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
#include "ui/DesktopAdapters.h"

typedef struct PharmacyAdminTestContext {
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
} PharmacyAdminTestContext;

static void make_path(char *buffer, size_t buffer_size, const char *test_name, const char *file_name) {
    static long sequence = 1;

    snprintf(
        buffer,
        buffer_size,
        "build/test_pharmacy_admin/%s_%ld_%ld_%s",
        test_name,
        (long)time(0),
        sequence++,
        file_name
    );
    remove(buffer);
}

static void setup_context(PharmacyAdminTestContext *context, const char *test_name) {
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

static void seed_patient_doctor_and_registration_context(const PharmacyAdminTestContext *context) {
    PatientRepository patient_repository;
    DepartmentRepository department_repository;
    DoctorRepository doctor_repository;
    Patient patient;
    Department department = { "DEP0501", "感染科", "门诊四层", "感染门诊" };
    Doctor doctor = { "DOC0501", "韩青", "主治医师", "DEP0501", "周一至周五", DOCTOR_STATUS_ACTIVE };
    Result result = PatientRepository_init(&patient_repository, context->patient_path);

    assert(result.success == 1);
    memset(&patient, 0, sizeof(patient));
    strcpy(patient.patient_id, "PATP501");
    strcpy(patient.name, "谢阳");
    patient.gender = PATIENT_GENDER_MALE;
    patient.age = 39;
    strcpy(patient.contact, "13800000005");
    strcpy(patient.id_card, "110101198701010055");
    strcpy(patient.allergy, "无");
    strcpy(patient.medical_history, "既往胃病");
    strcpy(patient.remarks, "pharmacy admin flow");
    assert(PatientRepository_append(&patient_repository, &patient).success == 1);

    result = DepartmentRepository_init(&department_repository, context->department_path);
    assert(result.success == 1);
    result = DoctorRepository_init(&doctor_repository, context->doctor_path);
    assert(result.success == 1);
    assert(DepartmentRepository_save(&department_repository, &department).success == 1);
    assert(DoctorRepository_save(&doctor_repository, &doctor).success == 1);
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

static void test_pharmacy_and_admin_visibility(void) {
    PharmacyAdminTestContext context;
    MenuApplication application;
    DesktopDashboardState dashboard;
    Registration registration;
    Medicine medicine;
    LinkedList history;
    DispenseRecord *record = 0;
    char visit_id[HIS_DOMAIN_ID_CAPACITY];
    char buffer[2048];
    Result result;

    setup_context(&context, "pharmacy_admin_chain");
    seed_patient_doctor_and_registration_context(&context);

    result = DesktopAdapters_init_application(&application, &context.paths);
    assert(result.success == 1);

    memset(&medicine, 0, sizeof(medicine));
    strcpy(medicine.medicine_id, "MED0501");
    strcpy(medicine.name, "阿奇霉素");
    strcpy(medicine.department_id, "DEP0501");
    medicine.price = 26.80;
    medicine.stock = 3;
    medicine.low_stock_threshold = 4;
    result = DesktopAdapters_add_medicine(&application, &medicine, buffer, sizeof(buffer));
    assert(result.success == 1);

    memset(&registration, 0, sizeof(registration));
    result = DesktopAdapters_submit_registration(
        &application,
        "PATP501",
        "DOC0501",
        "DEP0501",
        "2026-03-27 16:20",
        &registration
    );
    assert(result.success == 1);

    memset(buffer, 0, sizeof(buffer));
    result = DesktopAdapters_create_visit_record(
        &application,
        registration.registration_id,
        "咽痛",
        "上呼吸道感染",
        "建议抗感染治疗",
        0,
        0,
        1,
        "2026-03-27 16:40",
        buffer,
        sizeof(buffer)
    );
    assert(result.success == 1);
    extract_id_after_prefix(buffer, "看诊记录已创建:", visit_id, sizeof(visit_id));

    memset(buffer, 0, sizeof(buffer));
    result = DesktopAdapters_find_low_stock_medicines(&application, buffer, sizeof(buffer));
    assert(result.success == 1);
    assert(strstr(buffer, "MED0501") != 0);

    memset(buffer, 0, sizeof(buffer));
    result = DesktopAdapters_restock_medicine(&application, "MED0501", 10, buffer, sizeof(buffer));
    assert(result.success == 1);
    assert(strstr(buffer, "库存") != 0 || strstr(buffer, "入库") != 0);

    memset(buffer, 0, sizeof(buffer));
    result = DesktopAdapters_dispense_medicine(
        &application,
        "PATP501",
        visit_id,
        "MED0501",
        2,
        "PHA0501",
        "2026-03-27 17:00",
        buffer,
        sizeof(buffer)
    );
    assert(result.success == 1);
    assert(strstr(buffer, "发药") != 0);

    memset(buffer, 0, sizeof(buffer));
    result = DesktopAdapters_query_medicine_stock(&application, "MED0501", buffer, sizeof(buffer));
    assert(result.success == 1);
    assert(strstr(buffer, "库存") != 0);

    LinkedList_init(&history);
    result = DesktopAdapters_load_dispense_history(&application, "PATP501", &history);
    assert(result.success == 1);
    assert(LinkedList_count(&history) == 1);
    record = (DispenseRecord *)history.head->data;
    assert(record != 0);
    assert(strcmp(record->pharmacist_id, "PHA0501") == 0);
    LinkedList_clear(&history, free);

    memset(&dashboard, 0, sizeof(dashboard));
    LinkedList_init(&dashboard.recent_registrations);
    LinkedList_init(&dashboard.recent_dispensations);
    result = DesktopAdapters_load_dashboard(&application, &dashboard);
    assert(result.success == 1);
    assert(dashboard.registration_count >= 1);
    assert(dashboard.low_stock_count == 0);
    assert(LinkedList_count(&dashboard.recent_registrations) == 1);
    assert(LinkedList_count(&dashboard.recent_dispensations) == 1);
    LinkedList_clear(&dashboard.recent_registrations, free);
    LinkedList_clear(&dashboard.recent_dispensations, free);
}

int main(void) {
    test_pharmacy_and_admin_visibility();
    return 0;
}
