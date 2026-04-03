#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "domain/Bed.h"
#include "domain/Department.h"
#include "domain/Doctor.h"
#include "domain/Medicine.h"
#include "domain/Patient.h"
#include "repository/BedRepository.h"
#include "repository/ExaminationRecordRepository.h"
#include "repository/VisitRecordRepository.h"
#include "service/AuthService.h"
#include "ui/DesktopAdapters.h"

typedef struct DesktopWorkflowTestContext {
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
} DesktopWorkflowTestContext;

static void make_path(char *buffer, size_t buffer_size, const char *test_name, const char *file_name) {
    static long sequence = 1;

    snprintf(
        buffer,
        buffer_size,
        "build/test_desktop_data/%s_%ld_%ld_%s",
        test_name,
        (long)time(0),
        sequence++,
        file_name
    );
    remove(buffer);
}

static void setup_context(DesktopWorkflowTestContext *context, const char *test_name) {
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

static void seed_patient(const DesktopWorkflowTestContext *context, const char *patient_id, const char *name) {
    PatientRepository repository;
    Patient patient;
    Result result = PatientRepository_init(&repository, context->patient_path);

    assert(result.success == 1);
    memset(&patient, 0, sizeof(patient));
    strcpy(patient.patient_id, patient_id);
    strcpy(patient.name, name);
    patient.gender = PATIENT_GENDER_MALE;
    patient.age = 28;
    strcpy(patient.contact, "13800000000");
    strcpy(patient.id_card, "110101199601010011");
    strcpy(patient.allergy, "None");
    strcpy(patient.medical_history, "desktop workflow");
    strcpy(patient.remarks, "workflow");
    assert(PatientRepository_append(&repository, &patient).success == 1);
}

static void seed_department_and_doctor(const DesktopWorkflowTestContext *context) {
    DepartmentRepository department_repository;
    DoctorRepository doctor_repository;
    Department department = { "DEP0001", "General", "Floor 1", "General clinic" };
    Doctor doctor = { "DOC0001", "DesktopDoctor", "Attending", "DEP0001", "Mon-Fri", DOCTOR_STATUS_ACTIVE };
    Result result = DepartmentRepository_init(&department_repository, context->department_path);

    assert(result.success == 1);
    result = DoctorRepository_init(&doctor_repository, context->doctor_path);
    assert(result.success == 1);
    assert(DepartmentRepository_save(&department_repository, &department).success == 1);
    assert(DoctorRepository_save(&doctor_repository, &doctor).success == 1);
}

static void seed_user_account(
    const DesktopWorkflowTestContext *context,
    const char *user_id,
    const char *password,
    UserRole role
) {
    AuthService auth_service;
    Result result = AuthService_init(&auth_service, context->user_path, context->patient_path);

    assert(result.success == 1);
    assert(AuthService_register_user(&auth_service, user_id, password, role).success == 1);
}

static void seed_ward_and_beds(const DesktopWorkflowTestContext *context) {
    WardRepository ward_repository;
    BedRepository bed_repository;
    Ward ward;
    Bed first_bed;
    Bed second_bed;
    Result result;

    memset(&ward, 0, sizeof(ward));
    strcpy(ward.ward_id, "WRD0001");
    strcpy(ward.name, "Ward A");
    strcpy(ward.department_id, "DEP0001");
    strcpy(ward.location, "Floor 5");
    ward.capacity = 4;
    ward.status = WARD_STATUS_ACTIVE;

    memset(&first_bed, 0, sizeof(first_bed));
    strcpy(first_bed.bed_id, "BED0001");
    strcpy(first_bed.ward_id, "WRD0001");
    strcpy(first_bed.room_no, "501");
    strcpy(first_bed.bed_no, "01");
    first_bed.status = BED_STATUS_AVAILABLE;

    memset(&second_bed, 0, sizeof(second_bed));
    strcpy(second_bed.bed_id, "BED0002");
    strcpy(second_bed.ward_id, "WRD0001");
    strcpy(second_bed.room_no, "501");
    strcpy(second_bed.bed_no, "02");
    second_bed.status = BED_STATUS_AVAILABLE;

    result = WardRepository_init(&ward_repository, context->ward_path);
    assert(result.success == 1);
    result = BedRepository_init(&bed_repository, context->bed_path);
    assert(result.success == 1);
    assert(WardRepository_append(&ward_repository, &ward).success == 1);
    assert(BedRepository_append(&bed_repository, &first_bed).success == 1);
    assert(BedRepository_append(&bed_repository, &second_bed).success == 1);
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

static void test_clerk_and_doctor_desktop_workflows(void) {
    DesktopWorkflowTestContext context;
    MenuApplication application;
    Registration registration;
    Registration cancellable_registration;
    char buffer[2048];
    char visit_id[HIS_DOMAIN_ID_CAPACITY];
    char examination_id[HIS_DOMAIN_ID_CAPACITY];
    Medicine medicine;
    Result result;

    setup_context(&context, "desktop_workflows_clerk_doctor");
    seed_patient(&context, "PATW001", "WorkflowPatient");
    seed_department_and_doctor(&context);

    result = DesktopAdapters_init_application(&application, &context.paths);
    assert(result.success == 1);

    memset(&registration, 0, sizeof(registration));
    result = DesktopAdapters_submit_registration(
        &application,
        "PATW001",
        "DOC0001",
        "DEP0001",
        "2026-03-22 09:00",
        &registration
    );
    assert(result.success == 1);

    memset(buffer, 0, sizeof(buffer));
    result = DesktopAdapters_query_registration(&application, registration.registration_id, buffer, sizeof(buffer));
    assert(result.success == 1);
    assert(strstr(buffer, registration.registration_id) != 0);

    memset(buffer, 0, sizeof(buffer));
    result = DesktopAdapters_query_registrations_by_patient(&application, "PATW001", buffer, sizeof(buffer));
    assert(result.success == 1);
    assert(strstr(buffer, "PATW001") != 0);

    memset(buffer, 0, sizeof(buffer));
    result = DesktopAdapters_query_pending_registrations_by_doctor(&application, "DOC0001", buffer, sizeof(buffer));
    assert(result.success == 1);
    assert(strstr(buffer, registration.registration_id) != 0);

    memset(buffer, 0, sizeof(buffer));
    result = DesktopAdapters_create_visit_record(
        &application,
        registration.registration_id,
        "头痛",
        "上呼吸道感染",
        "多喝水",
        1,
        0,
        1,
        "2026-03-22 09:30",
        buffer,
        sizeof(buffer)
    );
    assert(result.success == 1);
    extract_id_after_prefix(buffer, "看诊记录已创建:", visit_id, sizeof(visit_id));

    memset(&medicine, 0, sizeof(medicine));
    strcpy(medicine.medicine_id, "MED0001");
    strcpy(medicine.name, "Ibuprofen");
    strcpy(medicine.department_id, "DEP0001");
    medicine.price = 12.5f;
    medicine.stock = 20;
    medicine.low_stock_threshold = 5;
    result = DesktopAdapters_add_medicine(&application, &medicine, buffer, sizeof(buffer));
    assert(result.success == 1);

    memset(buffer, 0, sizeof(buffer));
    result = DesktopAdapters_create_examination_record(
        &application,
        visit_id,
        "血常规",
        "Lab",
        "2026-03-22 10:00",
        buffer,
        sizeof(buffer)
    );
    assert(result.success == 1);
    extract_id_after_prefix(buffer, "检查记录已创建:", examination_id, sizeof(examination_id));

    memset(buffer, 0, sizeof(buffer));
    result = DesktopAdapters_complete_examination_record(
        &application,
        examination_id,
        "正常",
        "2026-03-22 11:00",
        buffer,
        sizeof(buffer)
    );
    assert(result.success == 1);
    assert(strstr(buffer, examination_id) != 0);

    memset(buffer, 0, sizeof(buffer));
    result = DesktopAdapters_query_records_by_time_range(
        &application,
        "2026-03-22 00:00",
        "2026-03-22 23:59",
        buffer,
        sizeof(buffer)
    );
    assert(result.success == 1);
    assert(strstr(buffer, "registrations=1") != 0);
    assert(strstr(buffer, "visits=1") != 0);
    assert(strstr(buffer, "examinations=1") != 0);

    memset(buffer, 0, sizeof(buffer));
    result = DesktopAdapters_query_patient_history(&application, "PATW001", buffer, sizeof(buffer));
    assert(result.success == 1);
    assert(strstr(buffer, "visits=1") != 0);

    memset(buffer, 0, sizeof(buffer));
    result = DesktopAdapters_query_medicine_detail(&application, "MED0001", 0, buffer, sizeof(buffer));
    assert(result.success == 1);
    assert(strstr(buffer, "Ibuprofen") != 0);

    memset(&cancellable_registration, 0, sizeof(cancellable_registration));
    result = DesktopAdapters_submit_registration(
        &application,
        "PATW001",
        "DOC0001",
        "DEP0001",
        "2026-03-22 11:30",
        &cancellable_registration
    );
    assert(result.success == 1);

    memset(buffer, 0, sizeof(buffer));
    result = DesktopAdapters_cancel_registration(
        &application,
        cancellable_registration.registration_id,
        "2026-03-22 12:00",
        buffer,
        sizeof(buffer)
    );
    assert(result.success == 1);
    assert(strstr(buffer, "已取消") != 0 || strstr(buffer, "取消") != 0);
}

static void test_inpatient_and_pharmacy_desktop_workflows(void) {
    DesktopWorkflowTestContext context;
    MenuApplication application;
    char buffer[2048];
    char admission_id[HIS_DOMAIN_ID_CAPACITY];
    LinkedList dispense_history;
    DispenseRecord *record = 0;
    Medicine medicine;
    Result result;

    setup_context(&context, "desktop_workflows_inpatient_pharmacy");
    seed_patient(&context, "PATW002", "WardPatient");
    seed_department_and_doctor(&context);
    seed_ward_and_beds(&context);

    result = DesktopAdapters_init_application(&application, &context.paths);
    assert(result.success == 1);

    memset(buffer, 0, sizeof(buffer));
    result = DesktopAdapters_list_wards(&application, buffer, sizeof(buffer));
    assert(result.success == 1);
    assert(strstr(buffer, "WRD0001") != 0);

    memset(buffer, 0, sizeof(buffer));
    result = DesktopAdapters_list_beds_by_ward(&application, "WRD0001", buffer, sizeof(buffer));
    assert(result.success == 1);
    assert(strstr(buffer, "BED0001") != 0);
    assert(strstr(buffer, "BED0002") != 0);

    memset(buffer, 0, sizeof(buffer));
    result = DesktopAdapters_admit_patient(
        &application,
        "PATW002",
        "WRD0001",
        "BED0001",
        "2026-03-22 13:00",
        "观察治疗",
        buffer,
        sizeof(buffer)
    );
    assert(result.success == 1);
    extract_id_after_prefix(buffer, "住院办理成功:", admission_id, sizeof(admission_id));

    memset(buffer, 0, sizeof(buffer));
    result = DesktopAdapters_query_active_admission_by_patient(&application, "PATW002", buffer, sizeof(buffer));
    assert(result.success == 1);
    assert(strstr(buffer, "BED0001") != 0);

    memset(buffer, 0, sizeof(buffer));
    result = DesktopAdapters_query_current_patient_by_bed(&application, "BED0001", buffer, sizeof(buffer));
    assert(result.success == 1);
    assert(strstr(buffer, "PATW002") != 0);

    memset(buffer, 0, sizeof(buffer));
    result = DesktopAdapters_transfer_bed(
        &application,
        admission_id,
        "BED0002",
        "2026-03-22 14:00",
        buffer,
        sizeof(buffer)
    );
    assert(result.success == 1);
    assert(strstr(buffer, "BED0002") != 0);

    memset(buffer, 0, sizeof(buffer));
    result = DesktopAdapters_discharge_check(&application, admission_id, buffer, sizeof(buffer));
    assert(result.success == 1);
    assert(strstr(buffer, "可办理出院") != 0);

    memset(&medicine, 0, sizeof(medicine));
    strcpy(medicine.medicine_id, "MED0002");
    strcpy(medicine.name, "Amoxicillin");
    strcpy(medicine.department_id, "DEP0001");
    medicine.price = 8.0f;
    medicine.stock = 6;
    medicine.low_stock_threshold = 5;
    result = DesktopAdapters_add_medicine(&application, &medicine, buffer, sizeof(buffer));
    assert(result.success == 1);

    memset(buffer, 0, sizeof(buffer));
    result = DesktopAdapters_restock_medicine(&application, "MED0002", 4, buffer, sizeof(buffer));
    assert(result.success == 1);

    memset(buffer, 0, sizeof(buffer));
    result = DesktopAdapters_dispense_medicine(
        &application,
        "PATW002",
        "PRE0001",
        "MED0002",
        7,
        "PHA0001",
        "2026-03-22 15:00",
        buffer,
        sizeof(buffer)
    );
    assert(result.success == 1);

    memset(buffer, 0, sizeof(buffer));
    result = DesktopAdapters_query_medicine_stock(&application, "MED0002", buffer, sizeof(buffer));
    assert(result.success == 1);
    assert(strstr(buffer, "药品库存: MED0002 | 3") != 0);

    memset(buffer, 0, sizeof(buffer));
    result = DesktopAdapters_find_low_stock_medicines(&application, buffer, sizeof(buffer));
    assert(result.success == 1);
    assert(strstr(buffer, "MED0002") != 0);

    LinkedList_init(&dispense_history);
    result = DesktopAdapters_load_dispense_history(&application, "PATW002", &dispense_history);
    assert(result.success == 1);
    assert(LinkedList_count(&dispense_history) == 1);
    record = (DispenseRecord *)dispense_history.head->data;
    assert(record != 0);
    assert(strcmp(record->medicine_id, "MED0002") == 0);
    LinkedList_clear(&dispense_history, free);

    memset(buffer, 0, sizeof(buffer));
    result = DesktopAdapters_query_medicine_detail(&application, "MED0002", 1, buffer, sizeof(buffer));
    assert(result.success == 1);
    assert(strstr(buffer, "未维护用法说明") != 0);

    memset(buffer, 0, sizeof(buffer));
    result = DesktopAdapters_discharge_patient(
        &application,
        admission_id,
        "2026-03-22 16:00",
        "病情稳定",
        buffer,
        sizeof(buffer)
    );
    assert(result.success == 1);
    assert(strstr(buffer, "出院办理成功") != 0);
}

int main(void) {
    test_clerk_and_doctor_desktop_workflows();
    test_inpatient_and_pharmacy_desktop_workflows();
    return 0;
}
