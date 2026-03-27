#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "domain/Bed.h"
#include "domain/Department.h"
#include "domain/Doctor.h"
#include "domain/Patient.h"
#include "domain/Ward.h"
#include "repository/BedRepository.h"
#include "repository/DepartmentRepository.h"
#include "repository/DoctorRepository.h"
#include "repository/PatientRepository.h"
#include "repository/WardRepository.h"
#include "ui/DesktopAdapters.h"

typedef struct InpatientWardTestContext {
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
} InpatientWardTestContext;

static void make_path(char *buffer, size_t buffer_size, const char *test_name, const char *file_name) {
    static long sequence = 1;

    snprintf(
        buffer,
        buffer_size,
        "build/test_inpatient_ward/%s_%ld_%ld_%s",
        test_name,
        (long)time(0),
        sequence++,
        file_name
    );
    remove(buffer);
}

static void setup_context(InpatientWardTestContext *context, const char *test_name) {
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

static void seed_inpatient_graph(const InpatientWardTestContext *context) {
    PatientRepository patient_repository;
    DepartmentRepository department_repository;
    DoctorRepository doctor_repository;
    WardRepository ward_repository;
    BedRepository bed_repository;
    Patient patient;
    Department department = { "DEP0401", "呼吸科", "住院楼二层", "呼吸系统" };
    Doctor doctor = { "DOC0401", "刘洋", "主治医师", "DEP0401", "周一至周日", DOCTOR_STATUS_ACTIVE };
    Ward ward = { "WRD0401", "呼吸病区A", "DEP0401", "住院楼2F", 2, 0, WARD_STATUS_ACTIVE };
    Bed first_bed = { "BED0401", "WRD0401", "201", "01", BED_STATUS_AVAILABLE, "", "", "" };
    Bed second_bed = { "BED0402", "WRD0401", "201", "02", BED_STATUS_AVAILABLE, "", "", "" };
    Result result = PatientRepository_init(&patient_repository, context->patient_path);

    assert(result.success == 1);
    memset(&patient, 0, sizeof(patient));
    strcpy(patient.patient_id, "PATI401");
    strcpy(patient.name, "沈乔");
    patient.gender = PATIENT_GENDER_FEMALE;
    patient.age = 47;
    strcpy(patient.contact, "13800000004");
    strcpy(patient.id_card, "110101198001010044");
    strcpy(patient.allergy, "无");
    strcpy(patient.medical_history, "慢性支气管炎");
    strcpy(patient.remarks, "inpatient flow");
    assert(PatientRepository_append(&patient_repository, &patient).success == 1);

    result = DepartmentRepository_init(&department_repository, context->department_path);
    assert(result.success == 1);
    result = DoctorRepository_init(&doctor_repository, context->doctor_path);
    assert(result.success == 1);
    result = WardRepository_init(&ward_repository, context->ward_path);
    assert(result.success == 1);
    result = BedRepository_init(&bed_repository, context->bed_path);
    assert(result.success == 1);

    assert(DepartmentRepository_save(&department_repository, &department).success == 1);
    assert(DoctorRepository_save(&doctor_repository, &doctor).success == 1);
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

static void test_inpatient_and_ward_chain(void) {
    InpatientWardTestContext context;
    MenuApplication application;
    Registration registration;
    char admission_id[HIS_DOMAIN_ID_CAPACITY];
    char buffer[2048];
    Result result;

    setup_context(&context, "inpatient_ward_chain");
    seed_inpatient_graph(&context);

    result = DesktopAdapters_init_application(&application, &context.paths);
    assert(result.success == 1);

    memset(&registration, 0, sizeof(registration));
    result = DesktopAdapters_submit_registration(
        &application,
        "PATI401",
        "DOC0401",
        "DEP0401",
        "2026-03-27 15:20",
        &registration
    );
    assert(result.success == 1);

    memset(buffer, 0, sizeof(buffer));
    result = DesktopAdapters_create_visit_record(
        &application,
        registration.registration_id,
        "咳嗽气促",
        "肺部感染",
        "建议入院观察",
        0,
        1,
        0,
        "2026-03-27 15:40",
        buffer,
        sizeof(buffer)
    );
    assert(result.success == 1);

    memset(buffer, 0, sizeof(buffer));
    result = DesktopAdapters_list_wards(&application, buffer, sizeof(buffer));
    assert(result.success == 1);
    assert(strstr(buffer, "WRD0401") != 0);

    memset(buffer, 0, sizeof(buffer));
    result = DesktopAdapters_list_beds_by_ward(&application, "WRD0401", buffer, sizeof(buffer));
    assert(result.success == 1);
    assert(strstr(buffer, "BED0401") != 0);
    assert(strstr(buffer, "BED0402") != 0);

    memset(buffer, 0, sizeof(buffer));
    result = DesktopAdapters_admit_patient(
        &application,
        "PATI401",
        "WRD0401",
        "BED0401",
        "2026-03-27 16:00",
        "由门诊转入住院",
        buffer,
        sizeof(buffer)
    );
    assert(result.success == 1);
    extract_id_after_prefix(buffer, "住院办理成功:", admission_id, sizeof(admission_id));

    memset(buffer, 0, sizeof(buffer));
    result = DesktopAdapters_query_active_admission_by_patient(&application, "PATI401", buffer, sizeof(buffer));
    assert(result.success == 1);
    assert(strstr(buffer, admission_id) != 0);

    memset(buffer, 0, sizeof(buffer));
    result = DesktopAdapters_query_current_patient_by_bed(&application, "BED0401", buffer, sizeof(buffer));
    assert(result.success == 1);
    assert(strstr(buffer, "PATI401") != 0);

    memset(buffer, 0, sizeof(buffer));
    result = DesktopAdapters_transfer_bed(
        &application,
        admission_id,
        "BED0402",
        "2026-03-27 18:00",
        buffer,
        sizeof(buffer)
    );
    assert(result.success == 1);
    assert(strstr(buffer, "BED0402") != 0);

    memset(buffer, 0, sizeof(buffer));
    result = DesktopAdapters_discharge_check(&application, admission_id, buffer, sizeof(buffer));
    assert(result.success == 1);
    assert(strstr(buffer, "可办理出院") != 0);

    memset(buffer, 0, sizeof(buffer));
    result = DesktopAdapters_discharge_patient(
        &application,
        admission_id,
        "2026-03-28 09:00",
        "症状缓解，准予出院",
        buffer,
        sizeof(buffer)
    );
    assert(result.success == 1);
    assert(strstr(buffer, "出院") != 0);
}

int main(void) {
    test_inpatient_and_ward_chain();
    return 0;
}
