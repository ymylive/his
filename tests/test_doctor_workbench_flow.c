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
#include "ui/DesktopAdapters.h"

typedef struct DoctorWorkbenchTestContext {
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
} DoctorWorkbenchTestContext;

static void make_path(char *buffer, size_t buffer_size, const char *test_name, const char *file_name) {
    static long sequence = 1;

    snprintf(
        buffer,
        buffer_size,
        "build/test_doctor_workbench/%s_%ld_%ld_%s",
        test_name,
        (long)time(0),
        sequence++,
        file_name
    );
    remove(buffer);
}

static void setup_context(DoctorWorkbenchTestContext *context, const char *test_name) {
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

static void seed_patient_and_doctor(const DoctorWorkbenchTestContext *context) {
    PatientRepository patient_repository;
    DepartmentRepository department_repository;
    DoctorRepository doctor_repository;
    Patient patient;
    Department department = { "DEP0301", "儿科", "门诊三层", "儿童门诊" };
    Doctor doctor = { "DOC0301", "陈雪", "主任医师", "DEP0301", "周一至周五全天", DOCTOR_STATUS_ACTIVE };
    Result result = PatientRepository_init(&patient_repository, context->patient_path);

    assert(result.success == 1);
    memset(&patient, 0, sizeof(patient));
    strcpy(patient.patient_id, "PATD301");
    strcpy(patient.name, "赵安");
    patient.gender = PATIENT_GENDER_MALE;
    patient.age = 11;
    strcpy(patient.contact, "13800000003");
    strcpy(patient.id_card, "110101201501010033");
    strcpy(patient.allergy, "花粉");
    strcpy(patient.medical_history, "支气管炎");
    strcpy(patient.remarks, "doctor flow");
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

static void test_doctor_pending_visit_exam_and_handoff(void) {
    DoctorWorkbenchTestContext context;
    MenuApplication application;
    Registration registration;
    char visit_id[HIS_DOMAIN_ID_CAPACITY];
    char examination_id[HIS_DOMAIN_ID_CAPACITY];
    char buffer[2048];
    Result result;

    setup_context(&context, "doctor_pending_chain");
    seed_patient_and_doctor(&context);

    result = DesktopAdapters_init_application(&application, &context.paths);
    assert(result.success == 1);

    memset(&registration, 0, sizeof(registration));
    result = DesktopAdapters_submit_registration(
        &application,
        "PATD301",
        "DOC0301",
        "DEP0301",
        "2026-03-27 14:00",
        &registration
    );
    assert(result.success == 1);

    memset(buffer, 0, sizeof(buffer));
    result = DesktopAdapters_query_pending_registrations_by_doctor(&application, "DOC0301", buffer, sizeof(buffer));
    assert(result.success == 1);
    assert(strstr(buffer, registration.registration_id) != 0);

    memset(buffer, 0, sizeof(buffer));
    result = DesktopAdapters_create_visit_record(
        &application,
        registration.registration_id,
        "持续咳嗽",
        "急性支气管炎",
        "建议检查并观察，必要时住院",
        1,
        1,
        1,
        "2026-03-27 14:20",
        buffer,
        sizeof(buffer)
    );
    assert(result.success == 1);
    assert(strstr(buffer, "看诊记录已创建") != 0);
    extract_id_after_prefix(buffer, "看诊记录已创建:", visit_id, sizeof(visit_id));

    memset(buffer, 0, sizeof(buffer));
    result = DesktopAdapters_create_examination_record(
        &application,
        visit_id,
        "肺部影像",
        "影像",
        "2026-03-27 14:30",
        buffer,
        sizeof(buffer)
    );
    assert(result.success == 1);
    extract_id_after_prefix(buffer, "检查记录已创建:", examination_id, sizeof(examination_id));

    memset(buffer, 0, sizeof(buffer));
    result = DesktopAdapters_complete_examination_record(
        &application,
        examination_id,
        "支气管纹理增粗",
        "2026-03-27 15:00",
        buffer,
        sizeof(buffer)
    );
    assert(result.success == 1);
    assert(strstr(buffer, examination_id) != 0);

    memset(buffer, 0, sizeof(buffer));
    result = DesktopAdapters_query_patient_history(&application, "PATD301", buffer, sizeof(buffer));
    assert(result.success == 1);
    assert(strstr(buffer, "visits=1") != 0);
    assert(strstr(buffer, "examinations=1") != 0);

    memset(buffer, 0, sizeof(buffer));
    result = DesktopAdapters_query_records_by_time_range(
        &application,
        "2026-03-27 00:00",
        "2026-03-27 23:59",
        buffer,
        sizeof(buffer)
    );
    assert(result.success == 1);
    assert(strstr(buffer, "registrations=1") != 0);
    assert(strstr(buffer, "visits=1") != 0);
    assert(strstr(buffer, "examinations=1") != 0);

    memset(buffer, 0, sizeof(buffer));
    result = DesktopAdapters_query_pending_registrations_by_doctor(&application, "DOC0301", buffer, sizeof(buffer));
    assert(result.success == 1);
    assert(strstr(buffer, registration.registration_id) == 0);
}

int main(void) {
    test_doctor_pending_visit_exam_and_handoff();
    return 0;
}
