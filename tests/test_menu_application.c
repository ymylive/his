#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "domain/Admission.h"
#include "domain/Bed.h"
#include "domain/Department.h"
#include "domain/DispenseRecord.h"
#include "domain/Doctor.h"
#include "domain/Medicine.h"
#include "domain/Patient.h"
#include "domain/User.h"
#include "domain/Ward.h"
#include "repository/AdmissionRepository.h"
#include "repository/BedRepository.h"
#include "repository/DepartmentRepository.h"
#include "repository/DispenseRecordRepository.h"
#include "repository/DoctorRepository.h"
#include "repository/ExaminationRecordRepository.h"
#include "repository/PatientRepository.h"
#include "repository/RegistrationRepository.h"
#include "repository/VisitRecordRepository.h"
#include "repository/WardRepository.h"
#include "service/AuthService.h"
#include "ui/MenuApplication.h"

typedef struct MenuApplicationTestContext {
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
} MenuApplicationTestContext;

static void build_test_path(
    char *buffer,
    size_t buffer_size,
    const char *test_name,
    const char *file_name
) {
    assert(buffer != 0);
    assert(test_name != 0);
    assert(file_name != 0);

    snprintf(
        buffer,
        buffer_size,
        "build/test_menu_application_data/%s_%ld_%lu_%s",
        test_name,
        (long)time(0),
        (unsigned long)clock(),
        file_name
    );
}

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

static void setup_context(MenuApplicationTestContext *context, const char *test_name) {
    assert(context != 0);

    memset(context, 0, sizeof(*context));
    build_test_path(context->user_path, sizeof(context->user_path), test_name, "users.txt");
    build_test_path(context->patient_path, sizeof(context->patient_path), test_name, "patients.txt");
    build_test_path(
        context->department_path,
        sizeof(context->department_path),
        test_name,
        "departments.txt"
    );
    build_test_path(context->doctor_path, sizeof(context->doctor_path), test_name, "doctors.txt");
    build_test_path(
        context->registration_path,
        sizeof(context->registration_path),
        test_name,
        "registrations.txt"
    );
    build_test_path(context->visit_path, sizeof(context->visit_path), test_name, "visits.txt");
    build_test_path(
        context->examination_path,
        sizeof(context->examination_path),
        test_name,
        "examinations.txt"
    );
    build_test_path(context->ward_path, sizeof(context->ward_path), test_name, "wards.txt");
    build_test_path(context->bed_path, sizeof(context->bed_path), test_name, "beds.txt");
    build_test_path(
        context->admission_path,
        sizeof(context->admission_path),
        test_name,
        "admissions.txt"
    );
    build_test_path(
        context->medicine_path,
        sizeof(context->medicine_path),
        test_name,
        "medicines.txt"
    );
    build_test_path(
        context->dispense_record_path,
        sizeof(context->dispense_record_path),
        test_name,
        "dispense_records.txt"
    );

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

static void seed_user_account(
    const MenuApplicationTestContext *context,
    const char *user_id,
    const char *password,
    UserRole role
) {
    AuthService auth_service;
    Result result = AuthService_init(&auth_service, context->user_path, context->patient_path);

    assert(result.success == 1);
    result = AuthService_register_user(&auth_service, user_id, password, role);
    assert(result.success == 1);
}

static void seed_department_and_doctor(const MenuApplicationTestContext *context) {
    DepartmentRepository department_repository;
    DoctorRepository doctor_repository;
    Department department = {
        "DEP0001",
        "Internal",
        "Floor 1",
        "Internal medicine"
    };
    Doctor doctor = {
        "DOC0001",
        "Dr.Alice",
        "Chief",
        "DEP0001",
        "Mon-Fri AM",
        DOCTOR_STATUS_ACTIVE
    };
    Result result;

    result = DepartmentRepository_init(&department_repository, context->department_path);
    assert(result.success == 1);
    result = DoctorRepository_init(&doctor_repository, context->doctor_path);
    assert(result.success == 1);
    assert(DepartmentRepository_save(&department_repository, &department).success == 1);
    assert(DoctorRepository_save(&doctor_repository, &doctor).success == 1);
}

static void seed_patient(
    const MenuApplicationTestContext *context,
    const char *patient_id,
    const char *name,
    int is_inpatient
) {
    PatientRepository repository;
    Patient patient;
    Result result;

    memset(&patient, 0, sizeof(patient));
    copy_text(patient.patient_id, sizeof(patient.patient_id), patient_id);
    copy_text(patient.name, sizeof(patient.name), name);
    patient.gender = PATIENT_GENDER_FEMALE;
    patient.age = 30;
    copy_text(patient.contact, sizeof(patient.contact), "13800000000");
    copy_text(patient.id_card, sizeof(patient.id_card), "110101199001011234");
    copy_text(patient.allergy, sizeof(patient.allergy), "None");
    copy_text(patient.medical_history, sizeof(patient.medical_history), "None");
    patient.is_inpatient = is_inpatient;
    copy_text(patient.remarks, sizeof(patient.remarks), "seed");

    result = PatientRepository_init(&repository, context->patient_path);
    assert(result.success == 1);
    assert(PatientRepository_append(&repository, &patient).success == 1);
}

static void seed_ward_and_bed(const MenuApplicationTestContext *context) {
    WardRepository ward_repository;
    BedRepository bed_repository;
    Ward ward;
    Bed bed;
    Result result;

    memset(&ward, 0, sizeof(ward));
    copy_text(ward.ward_id, sizeof(ward.ward_id), "WRD0001");
    copy_text(ward.name, sizeof(ward.name), "Ward A");
    copy_text(ward.department_id, sizeof(ward.department_id), "DEP0001");
    copy_text(ward.location, sizeof(ward.location), "Floor 5");
    ward.capacity = 3;
    ward.occupied_beds = 0;
    ward.status = WARD_STATUS_ACTIVE;

    memset(&bed, 0, sizeof(bed));
    copy_text(bed.bed_id, sizeof(bed.bed_id), "BED0001");
    copy_text(bed.ward_id, sizeof(bed.ward_id), "WRD0001");
    copy_text(bed.room_no, sizeof(bed.room_no), "501");
    copy_text(bed.bed_no, sizeof(bed.bed_no), "01");
    bed.status = BED_STATUS_AVAILABLE;

    result = WardRepository_init(&ward_repository, context->ward_path);
    assert(result.success == 1);
    result = BedRepository_init(&bed_repository, context->bed_path);
    assert(result.success == 1);
    assert(WardRepository_append(&ward_repository, &ward).success == 1);
    assert(BedRepository_append(&bed_repository, &bed).success == 1);
}

static Result execute_action_with_text_io(
    MenuApplication *application,
    MenuAction action,
    const char *input_text,
    char *output_buffer,
    size_t output_capacity
) {
    FILE *input_file = 0;
    FILE *output_file = 0;
    size_t bytes_read = 0;
    Result result;

    assert(application != 0);
    assert(output_buffer != 0);
    assert(output_capacity > 0);

    input_file = tmpfile();
    output_file = tmpfile();
    assert(input_file != 0);
    assert(output_file != 0);

    if (input_text != 0) {
        fputs(input_text, input_file);
    }

    rewind(input_file);
    result = MenuApplication_execute_action(application, action, input_file, output_file);
    fflush(output_file);
    rewind(output_file);

    memset(output_buffer, 0, output_capacity);
    bytes_read = fread(output_buffer, 1, output_capacity - 1, output_file);
    output_buffer[bytes_read] = '\0';

    fclose(input_file);
    fclose(output_file);
    return result;
}

static void test_clerk_flow_add_query_patient_and_registration(void) {
    MenuApplicationTestContext context;
    MenuApplication application;
    PatientRepository patient_repository;
    RegistrationRepository registration_repository;
    Patient loaded_patient;
    Registration loaded_registration;
    char output[1024];
    Result result;

    setup_context(&context, "clerk_flow");
    seed_department_and_doctor(&context);

    result = MenuApplication_init(&application, &context.paths);
    assert(result.success == 1);

    result = PatientRepository_init(&patient_repository, context.patient_path);
    assert(result.success == 1);
    result = RegistrationRepository_init(&registration_repository, context.registration_path);
    assert(result.success == 1);

    memset(output, 0, sizeof(output));
    result = MenuApplication_add_patient(
        &application,
        &(Patient){
            "PAT1001",
            "Alice",
            PATIENT_GENDER_FEMALE,
            28,
            "13800000001",
            "110101199001010011",
            "Penicillin",
            "Healthy",
            0,
            "First visit"
        },
        output,
        sizeof(output)
    );
    assert(result.success == 1);
    assert(strstr(output, "PAT1001") != 0);
    assert(PatientRepository_find_by_id(&patient_repository, "PAT1001", &loaded_patient).success == 1);
    assert(strcmp(loaded_patient.name, "Alice") == 0);

    memset(output, 0, sizeof(output));
    result = MenuApplication_query_patient(
        &application,
        "PAT1001",
        output,
        sizeof(output)
    );
    assert(result.success == 1);
    assert(strstr(output, "Alice") != 0);
    assert(strstr(output, "13800000001") != 0);

    memset(output, 0, sizeof(output));
    result = MenuApplication_create_registration(
        &application,
        "PAT1001",
        "DOC0001",
        "DEP0001",
        "2026-03-20T08:15",
        output,
        sizeof(output)
    );
    assert(result.success == 1);
    assert(strstr(output, "REG0001") != 0);
    assert(
        RegistrationRepository_find_by_registration_id(
            &registration_repository,
            "REG0001",
            &loaded_registration
        ).success == 1
    );
    assert(strcmp(loaded_registration.patient_id, "PAT1001") == 0);

    memset(output, 0, sizeof(output));
    result = MenuApplication_query_registration(
        &application,
        "REG0001",
        output,
        sizeof(output)
    );
    assert(result.success == 1);
    assert(strstr(output, "PAT1001") != 0);
    assert(strstr(output, "DOC0001") != 0);
}

static void test_patient_self_registration_requires_session_and_tracks_duplicates(void) {
    MenuApplicationTestContext context;
    MenuApplication application;
    Registration first_registration;
    Registration second_registration;
    char output[1024];
    Result result;

    setup_context(&context, "self_registration");
    seed_department_and_doctor(&context);
    seed_patient(&context, "PAT5101", "SelfRegistered", 0);
    seed_patient(&context, "PAT5102", "OtherPatient", 0);
    seed_user_account(&context, "PAT5101", "self-pass", USER_ROLE_PATIENT);

    result = MenuApplication_init(&application, &context.paths);
    assert(result.success == 1);

    memset(&first_registration, 0, sizeof(first_registration));
    result = MenuApplication_create_self_registration(
        &application,
        "DOC0001",
        "DEP0001",
        "2026-03-21T08:00",
        &first_registration
    );
    assert(result.success == 0);
    assert(strstr(result.message, "session") != 0);

    result = MenuApplication_login(&application, "PAT5101", "self-pass", USER_ROLE_PATIENT);
    assert(result.success == 1);

    result = MenuApplication_create_self_registration(
        &application,
        "DOC0001",
        "DEP0001",
        "2026-03-21T08:05",
        &first_registration
    );
    assert(result.success == 1);
    assert(strcmp(first_registration.registration_id, "REG0001") == 0);
    assert(strcmp(first_registration.patient_id, "PAT5101") == 0);

    memset(&second_registration, 0, sizeof(second_registration));
    result = MenuApplication_create_self_registration(
        &application,
        "DOC0001",
        "DEP0001",
        "2026-03-21T08:10",
        &second_registration
    );
    assert(result.success == 1);
    assert(strcmp(second_registration.registration_id, "REG0002") == 0);
    assert(strcmp(second_registration.patient_id, "PAT5101") == 0);

    memset(output, 0, sizeof(output));
    result = MenuApplication_query_registrations_by_patient(
        &application,
        "PAT5101",
        output,
        sizeof(output)
    );
    assert(result.success == 1);
    assert(strstr(output, "count=2") != 0);
    assert(strstr(output, "REG0001") != 0);
    assert(strstr(output, "REG0002") != 0);
}

static void test_menu_application_lists_departments_and_creates_visit_handoff(void) {
    MenuApplicationTestContext context;
    MenuApplication application;
    MenuApplicationVisitHandoff handoff;
    char output[1024];
    Result result;

    setup_context(&context, "department_and_handoff");
    seed_department_and_doctor(&context);
    seed_patient(&context, "PAT5201", "VisitPatient", 0);

    result = MenuApplication_init(&application, &context.paths);
    assert(result.success == 1);

    memset(output, 0, sizeof(output));
    result = MenuApplication_list_departments(&application, output, sizeof(output));
    assert(result.success == 1);
    assert(strstr(output, "DEP0001") != 0);
    assert(strstr(output, "Internal") != 0);

    memset(output, 0, sizeof(output));
    result = MenuApplication_list_doctors_by_department(
        &application,
        "DEP0001",
        output,
        sizeof(output)
    );
    assert(result.success == 1);
    assert(strstr(output, "DOC0001") != 0);
    assert(strstr(output, "Dr.Alice") != 0);

    result = MenuApplication_create_registration(
        &application,
        "PAT5201",
        "DOC0001",
        "DEP0001",
        "2026-03-21T09:00",
        output,
        sizeof(output)
    );
    assert(result.success == 1);

    memset(&handoff, 0, sizeof(handoff));
    result = MenuApplication_create_visit_record_handoff(
        &application,
        "REG0001",
        "Fever",
        "Influenza",
        "Admit and dispense",
        1,
        1,
        1,
        "2026-03-21T09:20",
        &handoff
    );
    assert(result.success == 1);
    assert(strcmp(handoff.visit_id, "VIS0001") == 0);
    assert(strcmp(handoff.registration_id, "REG0001") == 0);
    assert(strcmp(handoff.patient_id, "PAT5201") == 0);
    assert(strcmp(handoff.doctor_id, "DOC0001") == 0);
    assert(strcmp(handoff.department_id, "DEP0001") == 0);
    assert(strcmp(handoff.diagnosis, "Influenza") == 0);
    assert(strcmp(handoff.advice, "Admit and dispense") == 0);
    assert(handoff.need_exam == 1);
    assert(handoff.need_admission == 1);
    assert(handoff.need_medicine == 1);
}

static void test_doctor_flow_create_visit_and_query_history(void) {
    MenuApplicationTestContext context;
    MenuApplication application;
    VisitRecordRepository visit_repository;
    VisitRecord loaded_visit;
    char output[1024];
    Result result;

    setup_context(&context, "doctor_flow");
    seed_department_and_doctor(&context);

    result = MenuApplication_init(&application, &context.paths);
    assert(result.success == 1);

    memset(output, 0, sizeof(output));
    result = MenuApplication_add_patient(
        &application,
        &(Patient){
            "PAT2001",
            "Bob",
            PATIENT_GENDER_MALE,
            33,
            "13800000002",
            "110101199202020022",
            "None",
            "Cough",
            0,
            "follow-up"
        },
        output,
        sizeof(output)
    );
    assert(result.success == 1);

    result = MenuApplication_create_registration(
        &application,
        "PAT2001",
        "DOC0001",
        "DEP0001",
        "2026-03-20T09:00",
        output,
        sizeof(output)
    );
    assert(result.success == 1);

    result = VisitRecordRepository_init(&visit_repository, context.visit_path);
    assert(result.success == 1);

    memset(output, 0, sizeof(output));
    result = MenuApplication_create_visit_record(
        &application,
        "REG0001",
        "Cough",
        "Upper respiratory infection",
        "Drink water",
        1,
        0,
        1,
        "2026-03-20T09:30",
        output,
        sizeof(output)
    );
    assert(result.success == 1);
    assert(strstr(output, "VIS0001") != 0);
    assert(
        VisitRecordRepository_find_by_visit_id(&visit_repository, "VIS0001", &loaded_visit).success == 1
    );
    assert(strcmp(loaded_visit.patient_id, "PAT2001") == 0);

    memset(output, 0, sizeof(output));
    result = MenuApplication_query_patient_history(
        &application,
        "PAT2001",
        output,
        sizeof(output)
    );
    assert(result.success == 1);
    assert(strstr(output, "PAT2001") != 0);
    assert(strstr(output, "registrations=1") != 0);
    assert(strstr(output, "visits=1") != 0);
}

static void test_inpatient_flow_list_admit_and_discharge(void) {
    MenuApplicationTestContext context;
    MenuApplication application;
    AdmissionRepository admission_repository;
    PatientRepository patient_repository;
    BedRepository bed_repository;
    Admission admission;
    Patient patient;
    Bed bed;
    char output[1024];
    Result result;

    setup_context(&context, "inpatient_flow");
    seed_patient(&context, "PAT3001", "Carol", 0);
    seed_ward_and_bed(&context);

    result = MenuApplication_init(&application, &context.paths);
    assert(result.success == 1);

    result = AdmissionRepository_init(&admission_repository, context.admission_path);
    assert(result.success == 1);
    result = PatientRepository_init(&patient_repository, context.patient_path);
    assert(result.success == 1);
    result = BedRepository_init(&bed_repository, context.bed_path);
    assert(result.success == 1);

    memset(output, 0, sizeof(output));
    result = MenuApplication_list_wards(&application, output, sizeof(output));
    assert(result.success == 1);
    assert(strstr(output, "WRD0001") != 0);

    memset(output, 0, sizeof(output));
    result = MenuApplication_list_beds_by_ward(
        &application,
        "WRD0001",
        output,
        sizeof(output)
    );
    assert(result.success == 1);
    assert(strstr(output, "BED0001") != 0);

    memset(output, 0, sizeof(output));
    result = MenuApplication_admit_patient(
        &application,
        "PAT3001",
        "WRD0001",
        "BED0001",
        "2026-03-20T10:00",
        "observation",
        output,
        sizeof(output)
    );
    assert(result.success == 1);
    assert(strstr(output, "ADM0001") != 0);
    assert(AdmissionRepository_find_by_id(&admission_repository, "ADM0001", &admission).success == 1);
    assert(strcmp(admission.patient_id, "PAT3001") == 0);
    assert(PatientRepository_find_by_id(&patient_repository, "PAT3001", &patient).success == 1);
    assert(patient.is_inpatient == 1);
    assert(BedRepository_find_by_id(&bed_repository, "BED0001", &bed).success == 1);
    assert(bed.status == BED_STATUS_OCCUPIED);

    memset(output, 0, sizeof(output));
    result = MenuApplication_discharge_patient(
        &application,
        "ADM0001",
        "2026-03-22T11:00",
        "stable",
        output,
        sizeof(output)
    );
    assert(result.success == 1);
    assert(strstr(output, "ADM0001") != 0);
    assert(AdmissionRepository_find_by_id(&admission_repository, "ADM0001", &admission).success == 1);
    assert(admission.status == ADMISSION_STATUS_DISCHARGED);
    assert(PatientRepository_find_by_id(&patient_repository, "PAT3001", &patient).success == 1);
    assert(patient.is_inpatient == 0);
    assert(BedRepository_find_by_id(&bed_repository, "BED0001", &bed).success == 1);
    assert(bed.status == BED_STATUS_AVAILABLE);
}

static void test_pharmacy_flow_add_restock_dispense_and_low_stock(void) {
    MenuApplicationTestContext context;
    MenuApplication application;
    DispenseRecordRepository record_repository;
    LinkedList records;
    char output[1024];
    int stock = 0;
    Result result;

    setup_context(&context, "pharmacy_flow");
    result = MenuApplication_init(&application, &context.paths);
    assert(result.success == 1);

    result = DispenseRecordRepository_init(&record_repository, context.dispense_record_path);
    assert(result.success == 1);

    memset(output, 0, sizeof(output));
    result = MenuApplication_add_medicine(
        &application,
        &(Medicine){
            "MED4001",
            "Ibuprofen",
            18.00,
            6,
            "DEP0001",
            5
        },
        output,
        sizeof(output)
    );
    assert(result.success == 1);
    assert(strstr(output, "MED4001") != 0);

    memset(output, 0, sizeof(output));
    result = MenuApplication_restock_medicine(
        &application,
        "MED4001",
        4,
        output,
        sizeof(output)
    );
    assert(result.success == 1);
    assert(strstr(output, "10") != 0);

    memset(output, 0, sizeof(output));
    result = MenuApplication_query_medicine_stock(
        &application,
        "MED4001",
        output,
        sizeof(output)
    );
    assert(result.success == 1);
    assert(strstr(output, "10") != 0);

    memset(output, 0, sizeof(output));
    result = MenuApplication_dispense_medicine(
        &application,
        "PAT4001",
        "RX4001",
        "MED4001",
        6,
        "PHA0001",
        "2026-03-20T12:00",
        output,
        sizeof(output)
    );
    assert(result.success == 1);
    assert(strstr(output, "RX4001") != 0);

    LinkedList_init(&records);
    result = DispenseRecordRepository_find_by_prescription_id(
        &record_repository,
        "RX4001",
        &records
    );
    assert(result.success == 1);
    assert(LinkedList_count(&records) == 1);
    DispenseRecordRepository_clear_list(&records);

    memset(output, 0, sizeof(output));
    result = MenuApplication_query_medicine_stock(
        &application,
        "MED4001",
        output,
        sizeof(output)
    );
    assert(result.success == 1);
    assert(strstr(output, "4") != 0);

    memset(output, 0, sizeof(output));
    result = MenuApplication_find_low_stock_medicines(
        &application,
        output,
        sizeof(output)
    );
    assert(result.success == 1);
    assert(strstr(output, "MED4001") != 0);
}

static void test_execute_action_inpatient_bed_query_lists_beds(void) {
    MenuApplicationTestContext context;
    MenuApplication application;
    char output[8192];
    Result result;

    setup_context(&context, "execute_inpatient_bed_query");
    seed_ward_and_bed(&context);

    result = MenuApplication_init(&application, &context.paths);
    assert(result.success == 1);

    result = execute_action_with_text_io(
        &application,
        MENU_ACTION_INPATIENT_QUERY_BED,
        "WRD0001\n",
        output,
        sizeof(output)
    );
    assert(result.success == 1);
    assert(strstr(output, "BED0001") != 0);
    assert(strstr(output, "床位列表") != 0);
}

static void test_execute_action_ward_transfer_bed_moves_inpatient_to_target_bed(void) {
    MenuApplicationTestContext context;
    MenuApplication application;
    AdmissionRepository admission_repository;
    BedRepository bed_repository;
    Bed second_bed;
    Admission admission;
    char output[8192];
    Result result;

    setup_context(&context, "execute_ward_transfer_bed");
    seed_patient(&context, "PAT3002", "Dana", 0);
    seed_ward_and_bed(&context);

    result = MenuApplication_init(&application, &context.paths);
    assert(result.success == 1);

    result = BedRepository_init(&bed_repository, context.bed_path);
    assert(result.success == 1);
    memset(&second_bed, 0, sizeof(second_bed));
    copy_text(second_bed.bed_id, sizeof(second_bed.bed_id), "BED0002");
    copy_text(second_bed.ward_id, sizeof(second_bed.ward_id), "WRD0001");
    copy_text(second_bed.room_no, sizeof(second_bed.room_no), "501");
    copy_text(second_bed.bed_no, sizeof(second_bed.bed_no), "02");
    second_bed.status = BED_STATUS_AVAILABLE;
    assert(BedRepository_append(&bed_repository, &second_bed).success == 1);

    memset(output, 0, sizeof(output));
    result = MenuApplication_admit_patient(
        &application,
        "PAT3002",
        "WRD0001",
        "BED0001",
        "2026-03-20T10:30",
        "transfer seed",
        output,
        sizeof(output)
    );
    assert(result.success == 1);
    assert(strstr(output, "ADM0001") != 0);

    memset(output, 0, sizeof(output));
    result = execute_action_with_text_io(
        &application,
        MENU_ACTION_INPATIENT_TRANSFER_BED,
        "ADM0001\n"
        "BED0002\n"
        "2026-03-21T09:00\n",
        output,
        sizeof(output)
    );
    assert(result.success == 1);
    assert(strstr(output, "转床") != 0);
    assert(strstr(output, "ADM0001") != 0);
    assert(strstr(output, "BED0002") != 0);

    result = AdmissionRepository_init(&admission_repository, context.admission_path);
    assert(result.success == 1);
    assert(AdmissionRepository_find_by_id(&admission_repository, "ADM0001", &admission).success == 1);
    assert(strcmp(admission.bed_id, "BED0002") == 0);
}

static void test_execute_action_doctor_visit_preserves_long_complaint(void) {
    MenuApplicationTestContext context;
    MenuApplication application;
    VisitRecordRepository visit_repository;
    VisitRecord visit;
    char output[8192];
    const char *chief_complaint =
        "Long chief complaint text for execute action route coverage";
    Result result;

    setup_context(&context, "execute_doctor_visit");
    seed_department_and_doctor(&context);

    result = MenuApplication_init(&application, &context.paths);
    assert(result.success == 1);
    result = MenuApplication_add_patient(
        &application,
        &(Patient){
            "PAT5001",
            "Evan",
            PATIENT_GENDER_MALE,
            40,
            "13800000055",
            "110101199505050055",
            "None",
            "Healthy",
            0,
            "execute"
        },
        output,
        sizeof(output)
    );
    assert(result.success == 1);
    result = MenuApplication_create_registration(
        &application,
        "PAT5001",
        "DOC0001",
        "DEP0001",
        "2026-03-20T13:00",
        output,
        sizeof(output)
    );
    assert(result.success == 1);

    result = execute_action_with_text_io(
        &application,
        MENU_ACTION_DOCTOR_VISIT_RECORD,
        "REG0001\n"
        "Long chief complaint text for execute action route coverage\n"
        "Diagnosis\n"
        "Advice text\n"
        "1\n"
        "0\n"
        "1\n"
        "2026-03-20T13:30\n",
        output,
        sizeof(output)
    );
    assert(result.success == 1);

    result = VisitRecordRepository_init(&visit_repository, context.visit_path);
    assert(result.success == 1);
    assert(
        VisitRecordRepository_find_by_visit_id(&visit_repository, "VIS0001", &visit).success == 1
    );
    assert(strcmp(visit.chief_complaint, chief_complaint) == 0);
}

static void test_execute_action_rejects_invalid_medicine_price(void) {
    MenuApplicationTestContext context;
    MenuApplication application;
    char output[8192];
    Result result;

    setup_context(&context, "execute_invalid_price");
    result = MenuApplication_init(&application, &context.paths);
    assert(result.success == 1);

    result = execute_action_with_text_io(
        &application,
        MENU_ACTION_PHARMACY_ADD_MEDICINE,
        "InvalidPriceMedicine\n"
        "abc\n"
        "5\n"
        "DEP0001\n"
        "2\n",
        output,
        sizeof(output)
    );
    assert(result.success == 0);
    assert(strstr(result.message, "invalid") != 0);
}

static void test_execute_action_rejects_overlong_prompt_input(void) {
    MenuApplicationTestContext context;
    MenuApplication application;
    char output[8192];
    Result result;

    setup_context(&context, "execute_overlong_prompt");
    seed_patient(&context, "PAT9901", "OverflowPatient", 0);
    result = MenuApplication_init(&application, &context.paths);
    assert(result.success == 1);

    result = execute_action_with_text_io(
        &application,
        MENU_ACTION_INPATIENT_ADMIT,
        "PAT12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890\n",
        output,
        sizeof(output)
    );
    assert(result.success == 0);
    assert(strstr(result.message, "too long") != 0);
}

static void test_execute_action_patient_query_registration_lists_records(void) {
    MenuApplicationTestContext context;
    MenuApplication application;
    char output[8192];
    Result result;

    setup_context(&context, "execute_patient_registration_query");
    seed_department_and_doctor(&context);

    result = MenuApplication_init(&application, &context.paths);
    assert(result.success == 1);
    result = MenuApplication_add_patient(
        &application,
        &(Patient){
            "PAT6002",
            "Grace",
            PATIENT_GENDER_FEMALE,
            29,
            "13800000066",
            "110101199606060066",
            "None",
            "Healthy",
            0,
            "patient route"
        },
        output,
        sizeof(output)
    );
    assert(result.success == 1);
    result = MenuApplication_create_registration(
        &application,
        "PAT6002",
        "DOC0001",
        "DEP0001",
        "2026-03-20T14:00",
        output,
        sizeof(output)
    );
    assert(result.success == 1);
    seed_user_account(&context, "PAT6002", "patient-pass", USER_ROLE_PATIENT);
    result = MenuApplication_login(&application, "PAT6002", "patient-pass", USER_ROLE_PATIENT);
    assert(result.success == 1);

    result = execute_action_with_text_io(
        &application,
        MENU_ACTION_PATIENT_QUERY_REGISTRATION,
        "PAT6002\n",
        output,
        sizeof(output)
    );
    assert(result.success == 1);
    assert(strstr(output, "REG0001") != 0);
    assert(strstr(output, "PAT6002") != 0);
    MenuApplication_logout(&application);
}

static void test_patient_session_authorizes_only_bound_patient_routes(void) {
    MenuApplicationTestContext context;
    MenuApplication application;
    char output[8192];
    Result result;

    setup_context(&context, "patient_session_patient_routes");
    seed_department_and_doctor(&context);

    result = MenuApplication_init(&application, &context.paths);
    assert(result.success == 1);
    result = MenuApplication_add_patient(
        &application,
        &(Patient){
            "PAT7001",
            "Nina",
            PATIENT_GENDER_FEMALE,
            29,
            "13800000101",
            "110101199701010101",
            "None",
            "Healthy",
            0,
            "session A"
        },
        output,
        sizeof(output)
    );
    assert(result.success == 1);
    result = MenuApplication_add_patient(
        &application,
        &(Patient){
            "PAT7002",
            "Owen",
            PATIENT_GENDER_MALE,
            32,
            "13800000102",
            "110101199402020202",
            "None",
            "Healthy",
            0,
            "session B"
        },
        output,
        sizeof(output)
    );
    assert(result.success == 1);
    result = MenuApplication_create_registration(
        &application,
        "PAT7001",
        "DOC0001",
        "DEP0001",
        "2026-03-20T14:20",
        output,
        sizeof(output)
    );
    assert(result.success == 1);
    result = MenuApplication_create_registration(
        &application,
        "PAT7002",
        "DOC0001",
        "DEP0001",
        "2026-03-20T14:25",
        output,
        sizeof(output)
    );
    assert(result.success == 1);

    result = MenuApplication_create_visit_record(
        &application,
        "REG0001",
        "Fever",
        "Common cold",
        "Hydrate",
        0,
        0,
        1,
        "2026-03-20T14:30",
        output,
        sizeof(output)
    );
    assert(result.success == 1);
    result = MenuApplication_create_visit_record(
        &application,
        "REG0002",
        "Cough",
        "Flu",
        "Rest",
        1,
        0,
        1,
        "2026-03-20T14:40",
        output,
        sizeof(output)
    );
    assert(result.success == 1);

    result = MenuApplication_add_medicine(
        &application,
        &(Medicine){
            "MED7001",
            "SessionMed",
            12.00,
            20,
            "DEP0001",
            3
        },
        output,
        sizeof(output)
    );
    assert(result.success == 1);
    result = MenuApplication_dispense_medicine(
        &application,
        "PAT7001",
        "RX7001",
        "MED7001",
        2,
        "PHA7001",
        "2026-03-20T15:00",
        output,
        sizeof(output)
    );
    assert(result.success == 1);
    result = MenuApplication_dispense_medicine(
        &application,
        "PAT7002",
        "RX7002",
        "MED7001",
        1,
        "PHA7001",
        "2026-03-20T15:10",
        output,
        sizeof(output)
    );
    assert(result.success == 1);

    seed_user_account(&context, "PAT7001", "session-pass", USER_ROLE_PATIENT);
    result = MenuApplication_login(&application, "PAT7001", "session-pass", USER_ROLE_PATIENT);
    assert(result.success == 1);

    result = execute_action_with_text_io(
        &application,
        MENU_ACTION_PATIENT_BASIC_INFO,
        "PAT7001\n",
        output,
        sizeof(output)
    );
    assert(result.success == 1);
    assert(strstr(output, "PAT7001") != 0);

    result = execute_action_with_text_io(
        &application,
        MENU_ACTION_PATIENT_BASIC_INFO,
        "",
        output,
        sizeof(output)
    );
    assert(result.success == 1);
    assert(strstr(output, "PAT7001") != 0);
    assert(strstr(output, "PAT7002") == 0);

    result = execute_action_with_text_io(
        &application,
        MENU_ACTION_PATIENT_QUERY_REGISTRATION,
        "",
        output,
        sizeof(output)
    );
    assert(result.success == 1);
    assert(strstr(output, "PAT7001") != 0);
    assert(strstr(output, "PAT7002") == 0);

    result = execute_action_with_text_io(
        &application,
        MENU_ACTION_PATIENT_QUERY_VISITS,
        "",
        output,
        sizeof(output)
    );
    assert(result.success == 1);
    assert(strstr(output, "PAT7001") != 0);
    assert(strstr(output, "PAT7002") == 0);

    result = execute_action_with_text_io(
        &application,
        MENU_ACTION_PATIENT_QUERY_EXAMS,
        "",
        output,
        sizeof(output)
    );
    assert(result.success == 1);
    assert(strstr(output, "PAT7001") != 0);
    assert(strstr(output, "PAT7002") == 0);

    result = execute_action_with_text_io(
        &application,
        MENU_ACTION_PATIENT_QUERY_ADMISSIONS,
        "",
        output,
        sizeof(output)
    );
    assert(result.success == 1);
    assert(strstr(output, "PAT7001") != 0);
    assert(strstr(output, "PAT7002") == 0);

    result = execute_action_with_text_io(
        &application,
        MENU_ACTION_PATIENT_QUERY_DISPENSE,
        "",
        output,
        sizeof(output)
    );
    assert(result.success == 1);
    assert(strstr(output, "PAT7001") != 0);
    assert(strstr(output, "RX7001") != 0);
    assert(strstr(output, "RX7002") == 0);

    result = MenuApplication_login(&application, "PAT7999", "wrong-pass", USER_ROLE_PATIENT);
    assert(result.success == 0);

    result = execute_action_with_text_io(
        &application,
        MENU_ACTION_PATIENT_BASIC_INFO,
        "PAT7001\n",
        output,
        sizeof(output)
    );
    assert(result.success == 0);
    assert(strstr(result.message, "session") != 0);

    MenuApplication_logout(&application);
}

static void test_execute_action_patient_query_dispense_lists_records(void) {
    MenuApplicationTestContext context;
    MenuApplication application;
    char output[8192];
    Result result;

    setup_context(&context, "execute_patient_dispense_query");
    result = MenuApplication_init(&application, &context.paths);
    assert(result.success == 1);

    result = MenuApplication_add_medicine(
        &application,
        &(Medicine){
            "MED8001",
            "PatientDispenseMed",
            8.00,
            10,
            "DEP0001",
            1
        },
        output,
        sizeof(output)
    );
    assert(result.success == 1);

    result = MenuApplication_dispense_medicine(
        &application,
        "PAT8001",
        "RX8001",
        "MED8001",
        3,
        "PHA8001",
        "2026-03-20T20:00",
        output,
        sizeof(output)
    );
    assert(result.success == 1);
    result = MenuApplication_add_patient(
        &application,
        &(Patient){
            "PAT8001",
            "DispenseViewer",
            PATIENT_GENDER_FEMALE,
            30,
            "13800008001",
            "110101199101010801",
            "None",
            "Healthy",
            0,
            "dispense route"
        },
        output,
        sizeof(output)
    );
    assert(result.success == 1);
    seed_user_account(&context, "PAT8001", "dispense-pass", USER_ROLE_PATIENT);
    result = MenuApplication_login(&application, "PAT8001", "dispense-pass", USER_ROLE_PATIENT);
    assert(result.success == 1);

    result = execute_action_with_text_io(
        &application,
        MENU_ACTION_PATIENT_QUERY_DISPENSE,
        "",
        output,
        sizeof(output)
    );
    assert(result.success == 1);
    assert(strstr(output, "RX8001") != 0);
    assert(strstr(output, "MED8001") != 0);
    MenuApplication_logout(&application);
}

static void test_execute_action_doctor_exam_record_create_and_complete(void) {
    MenuApplicationTestContext context;
    MenuApplication application;
    ExaminationRecordRepository examination_repository;
    ExaminationRecord record;
    char output[8192];
    Result result;

    setup_context(&context, "execute_doctor_exam");
    seed_department_and_doctor(&context);

    result = MenuApplication_init(&application, &context.paths);
    assert(result.success == 1);
    result = MenuApplication_add_patient(
        &application,
        &(Patient){
            "PAT6003",
            "Helen",
            PATIENT_GENDER_FEMALE,
            37,
            "13800000077",
            "110101198808080088",
            "None",
            "Fever",
            0,
            "exam route"
        },
        output,
        sizeof(output)
    );
    assert(result.success == 1);
    result = MenuApplication_create_registration(
        &application,
        "PAT6003",
        "DOC0001",
        "DEP0001",
        "2026-03-20T15:00",
        output,
        sizeof(output)
    );
    assert(result.success == 1);
    result = MenuApplication_create_visit_record(
        &application,
        "REG0001",
        "Fever",
        "Influenza",
        "Need exam",
        1,
        0,
        1,
        "2026-03-20T15:30",
        output,
        sizeof(output)
    );
    assert(result.success == 1);
    result = ExaminationRecordRepository_init(&examination_repository, context.examination_path);
    assert(result.success == 1);

    result = execute_action_with_text_io(
        &application,
        MENU_ACTION_DOCTOR_EXAM_RECORD,
        "1\n"
        "VIS0001\n"
        "BloodTest\n"
        "Lab\n"
        "2026-03-20T16:00\n",
        output,
        sizeof(output)
    );
    assert(result.success == 1);
    assert(strstr(output, "EXM0001") != 0);
    assert(
        ExaminationRecordRepository_find_by_examination_id(
            &examination_repository,
            "EXM0001",
            &record
        ).success == 1
    );
    assert(strcmp(record.exam_item, "BloodTest") == 0);
    assert(record.status == EXAM_STATUS_PENDING);

    result = execute_action_with_text_io(
        &application,
        MENU_ACTION_DOCTOR_EXAM_RECORD,
        "2\n"
        "EXM0001\n"
        "All clear\n"
        "2026-03-20T17:00\n",
        output,
        sizeof(output)
    );
    assert(result.success == 1);
    assert(
        ExaminationRecordRepository_find_by_examination_id(
            &examination_repository,
            "EXM0001",
            &record
        ).success == 1
    );
    assert(record.status == EXAM_STATUS_COMPLETED);
    assert(strcmp(record.result, "All clear") == 0);
}

static void test_execute_action_doctor_pending_list_filters_diagnosed(void) {
    MenuApplicationTestContext context;
    MenuApplication application;
    char output[8192];
    Result result;

    setup_context(&context, "execute_doctor_pending");
    seed_department_and_doctor(&context);

    result = MenuApplication_init(&application, &context.paths);
    assert(result.success == 1);
    result = MenuApplication_add_patient(
        &application,
        &(Patient){
            "PAT6004",
            "Ivy",
            PATIENT_GENDER_FEMALE,
            31,
            "13800000088",
            "110101199101010091",
            "None",
            "Healthy",
            0,
            "pending one"
        },
        output,
        sizeof(output)
    );
    assert(result.success == 1);
    result = MenuApplication_add_patient(
        &application,
        &(Patient){
            "PAT6005",
            "Jack",
            PATIENT_GENDER_MALE,
            42,
            "13800000089",
            "110101198202020092",
            "None",
            "Healthy",
            0,
            "pending two"
        },
        output,
        sizeof(output)
    );
    assert(result.success == 1);
    result = MenuApplication_create_registration(
        &application,
        "PAT6004",
        "DOC0001",
        "DEP0001",
        "2026-03-20T18:00",
        output,
        sizeof(output)
    );
    assert(result.success == 1);
    result = MenuApplication_create_registration(
        &application,
        "PAT6005",
        "DOC0001",
        "DEP0001",
        "2026-03-20T18:10",
        output,
        sizeof(output)
    );
    assert(result.success == 1);
    result = MenuApplication_create_visit_record(
        &application,
        "REG0001",
        "Cough",
        "Cold",
        "Rest",
        0,
        0,
        1,
        "2026-03-20T18:20",
        output,
        sizeof(output)
    );
    assert(result.success == 1);

    result = execute_action_with_text_io(
        &application,
        MENU_ACTION_DOCTOR_PENDING_LIST,
        "DOC0001\n",
        output,
        sizeof(output)
    );
    assert(result.success == 1);
    assert(strstr(output, "REG0002") != 0);
    assert(strstr(output, "PAT6005") != 0);
    assert(strstr(output, "REG0001") == 0);
}

static void test_execute_action_admin_patient_management_deletes_patient(void) {
    MenuApplicationTestContext context;
    MenuApplication application;
    PatientRepository patient_repository;
    Patient patient;
    char output[8192];
    Result result;

    setup_context(&context, "execute_admin_patient");
    seed_patient(&context, "PAT9101", "AdminDelete", 0);
    result = MenuApplication_init(&application, &context.paths);
    assert(result.success == 1);
    result = PatientRepository_init(&patient_repository, context.patient_path);
    assert(result.success == 1);

    result = execute_action_with_text_io(
        &application,
        MENU_ACTION_ADMIN_PATIENT_MANAGEMENT,
        "3\n"
        "PAT9101\n",
        output,
        sizeof(output)
    );
    assert(result.success == 1);
    assert(strstr(output, "PAT9101") != 0);
    assert(PatientRepository_find_by_id(&patient_repository, "PAT9101", &patient).success == 0);
}

static void test_execute_action_admin_doctor_department_adds_and_lists_doctor(void) {
    MenuApplicationTestContext context;
    MenuApplication application;
    char output[8192];
    Result result;

    setup_context(&context, "execute_admin_doctor_department");
    result = MenuApplication_init(&application, &context.paths);
    assert(result.success == 1);

    result = execute_action_with_text_io(
        &application,
        MENU_ACTION_ADMIN_DOCTOR_DEPARTMENT,
        "1\n"
        "Surgery\n"
        "Floor 9\n"
        "Surgery Dept\n",
        output,
        sizeof(output)
    );
    assert(result.success == 1);
    assert(strstr(output, "DEP0001") != 0);

    result = execute_action_with_text_io(
        &application,
        MENU_ACTION_ADMIN_DOCTOR_DEPARTMENT,
        "3\n"
        "Dr.Bob\n"
        "Attending\n"
        "DEP0001\n"
        "Tue PM\n"
        "1\n",
        output,
        sizeof(output)
    );
    assert(result.success == 1);
    assert(strstr(output, "DOC0001") != 0);

    result = execute_action_with_text_io(
        &application,
        MENU_ACTION_ADMIN_DOCTOR_DEPARTMENT,
        "5\n"
        "DEP0001\n",
        output,
        sizeof(output)
    );
    assert(result.success == 1);
    assert(strstr(output, "DOC0001") != 0);
    assert(strstr(output, "DEP0001") != 0);
}

static void test_execute_action_admin_medical_records_queries_time_range(void) {
    MenuApplicationTestContext context;
    MenuApplication application;
    char output[8192];
    Result result;

    setup_context(&context, "execute_admin_medical_records");
    seed_department_and_doctor(&context);
    result = MenuApplication_init(&application, &context.paths);
    assert(result.success == 1);
    result = MenuApplication_add_patient(
        &application,
        &(Patient){
            "PAT9102",
            "AdminRecord",
            PATIENT_GENDER_FEMALE,
            28,
            "13800001111",
            "110101199901019999",
            "None",
            "Healthy",
            0,
            "admin record"
        },
        output,
        sizeof(output)
    );
    assert(result.success == 1);
    result = MenuApplication_create_registration(
        &application,
        "PAT9102",
        "DOC0001",
        "DEP0001",
        "2026-03-20T09:00",
        output,
        sizeof(output)
    );
    assert(result.success == 1);
    result = MenuApplication_create_visit_record(
        &application,
        "REG0001",
        "Headache",
        "Migraine",
        "Rest",
        0,
        0,
        1,
        "2026-03-20T09:30",
        output,
        sizeof(output)
    );
    assert(result.success == 1);

    result = execute_action_with_text_io(
        &application,
        MENU_ACTION_ADMIN_MEDICAL_RECORDS,
        "2026-03-20T00:00\n"
        "2026-03-20T23:59\n",
        output,
        sizeof(output)
    );
    assert(result.success == 1);
    assert(strstr(output, "registrations=1") != 0);
    assert(strstr(output, "visits=1") != 0);
}

static void test_execute_action_admin_ward_bed_overview_lists_beds(void) {
    MenuApplicationTestContext context;
    MenuApplication application;
    char output[8192];
    Result result;

    setup_context(&context, "execute_admin_ward_overview");
    seed_ward_and_bed(&context);
    result = MenuApplication_init(&application, &context.paths);
    assert(result.success == 1);

    result = execute_action_with_text_io(
        &application,
        MENU_ACTION_ADMIN_WARD_BED_OVERVIEW,
        "WRD0001\n",
        output,
        sizeof(output)
    );
    assert(result.success == 1);
    assert(strstr(output, "WRD0001") != 0);
    assert(strstr(output, "BED0001") != 0);
}

static void test_execute_action_admin_medicine_overview_lists_low_stock(void) {
    MenuApplicationTestContext context;
    MenuApplication application;
    char output[8192];
    Result result;

    setup_context(&context, "execute_admin_medicine");
    result = MenuApplication_init(&application, &context.paths);
    assert(result.success == 1);
    result = MenuApplication_add_medicine(
        &application,
        &(Medicine){
            "MED9101",
            "AdminLowStock",
            6.50,
            1,
            "DEP0001",
            2
        },
        output,
        sizeof(output)
    );
    assert(result.success == 1);

    result = execute_action_with_text_io(
        &application,
        MENU_ACTION_ADMIN_MEDICINE_OVERVIEW,
        "2\n",
        output,
        sizeof(output)
    );
    assert(result.success == 1);
    assert(strstr(output, "MED9101") != 0);
}

static void test_execute_action_patient_query_medicine_usage_reports_missing_instruction(void) {
    MenuApplicationTestContext context;
    MenuApplication application;
    char output[8192];
    Result result;

    setup_context(&context, "execute_patient_medicine_usage");
    result = MenuApplication_init(&application, &context.paths);
    assert(result.success == 1);
    result = MenuApplication_add_medicine(
        &application,
        &(Medicine){
            "MED9102",
            "UsageInfoMed",
            9.90,
            8,
            "DEP0001",
            1
        },
        output,
        sizeof(output)
    );
    assert(result.success == 1);
    result = MenuApplication_add_patient(
        &application,
        &(Patient){
            "PAT9104",
            "MedicineUsageViewer",
            PATIENT_GENDER_FEMALE,
            26,
            "13800009104",
            "110101199801019104",
            "None",
            "Healthy",
            0,
            "medicine usage route"
        },
        output,
        sizeof(output)
    );
    assert(result.success == 1);
    seed_user_account(&context, "PAT9104", "usage-pass", USER_ROLE_PATIENT);
    result = MenuApplication_login(&application, "PAT9104", "usage-pass", USER_ROLE_PATIENT);
    assert(result.success == 1);

    result = execute_action_with_text_io(
        &application,
        MENU_ACTION_PATIENT_QUERY_MEDICINE_USAGE,
        "MED9102\n",
        output,
        sizeof(output)
    );
    assert(result.success == 1);
    assert(strstr(output, "MED9102") != 0);
    assert(strstr(output, "未维护用法说明") != 0);
    MenuApplication_logout(&application);
}

static void test_execute_action_ward_discharge_check_reports_ready_status(void) {
    MenuApplicationTestContext context;
    MenuApplication application;
    char output[8192];
    Result result;

    setup_context(&context, "execute_ward_discharge_check");
    seed_patient(&context, "PAT9103", "DischargeReady", 0);
    seed_ward_and_bed(&context);
    result = MenuApplication_init(&application, &context.paths);
    assert(result.success == 1);

    result = MenuApplication_admit_patient(
        &application,
        "PAT9103",
        "WRD0001",
        "BED0001",
        "2026-03-20T11:00",
        "observation",
        output,
        sizeof(output)
    );
    assert(result.success == 1);

    result = execute_action_with_text_io(
        &application,
        MENU_ACTION_INPATIENT_DISCHARGE_CHECK,
        "ADM0001\n",
        output,
        sizeof(output)
    );
    assert(result.success == 1);
    assert(strstr(output, "ADM0001") != 0);
    assert(strstr(output, "可办理出院") != 0);
}

int main(void) {
    test_clerk_flow_add_query_patient_and_registration();
    test_patient_self_registration_requires_session_and_tracks_duplicates();
    test_menu_application_lists_departments_and_creates_visit_handoff();
    test_doctor_flow_create_visit_and_query_history();
    test_inpatient_flow_list_admit_and_discharge();
    test_pharmacy_flow_add_restock_dispense_and_low_stock();
    test_execute_action_inpatient_bed_query_lists_beds();
    test_execute_action_ward_transfer_bed_moves_inpatient_to_target_bed();
    test_execute_action_doctor_visit_preserves_long_complaint();
    test_execute_action_rejects_invalid_medicine_price();
    test_execute_action_rejects_overlong_prompt_input();
    test_execute_action_patient_query_registration_lists_records();
    test_patient_session_authorizes_only_bound_patient_routes();
    test_execute_action_patient_query_dispense_lists_records();
    test_execute_action_doctor_exam_record_create_and_complete();
    test_execute_action_doctor_pending_list_filters_diagnosed();
    test_execute_action_admin_patient_management_deletes_patient();
    test_execute_action_admin_doctor_department_adds_and_lists_doctor();
    test_execute_action_admin_medical_records_queries_time_range();
    test_execute_action_admin_ward_bed_overview_lists_beds();
    test_execute_action_admin_medicine_overview_lists_low_stock();
    test_execute_action_patient_query_medicine_usage_reports_missing_instruction();
    test_execute_action_ward_discharge_check_reports_ready_status();
    return 0;
}
