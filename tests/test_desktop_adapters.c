#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "domain/Department.h"
#include "domain/Doctor.h"
#include "domain/Medicine.h"
#include "domain/Patient.h"
#include "service/AuthService.h"
#include "repository/BedRepository.h"
#include "repository/DepartmentRepository.h"
#include "repository/DoctorRepository.h"
#include "repository/PatientRepository.h"
#include "repository/WardRepository.h"
#include "ui/DesktopAdapters.h"

typedef struct DesktopAdapterTestContext {
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
} DesktopAdapterTestContext;

static void make_path(
    char *buffer,
    size_t buffer_size,
    const char *test_name,
    const char *file_name
) {
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

static void setup_context(DesktopAdapterTestContext *context, const char *test_name) {
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
    make_path(context->dispense_record_path, sizeof(context->dispense_record_path), test_name, "dispense.txt");

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

static void seed_patient(const DesktopAdapterTestContext *context, const char *patient_id, const char *name) {
    PatientRepository repository;
    Patient patient;
    Result result = PatientRepository_init(&repository, context->patient_path);

    assert(result.success == 1);
    memset(&patient, 0, sizeof(patient));
    strcpy(patient.patient_id, patient_id);
    strcpy(patient.name, name);
    patient.gender = PATIENT_GENDER_FEMALE;
    patient.age = 30;
    strcpy(patient.contact, "13800000000");
    strcpy(patient.id_card, "110101199001010011");
    strcpy(patient.allergy, "None");
    strcpy(patient.medical_history, "Healthy");
    patient.is_inpatient = 0;
    strcpy(patient.remarks, "desktop seed");
    assert(PatientRepository_append(&repository, &patient).success == 1);
}

static void seed_department_and_doctor(const DesktopAdapterTestContext *context) {
    DepartmentRepository department_repository;
    DoctorRepository doctor_repository;
    Department department = {
        "DEP0001",
        "General",
        "Floor 1",
        "General clinic"
    };
    Doctor doctor = {
        "DOC0001",
        "DesktopDoctor",
        "Attending",
        "DEP0001",
        "Mon-Fri",
        DOCTOR_STATUS_ACTIVE
    };
    Result result = DepartmentRepository_init(&department_repository, context->department_path);

    assert(result.success == 1);
    result = DoctorRepository_init(&doctor_repository, context->doctor_path);
    assert(result.success == 1);
    assert(DepartmentRepository_save(&department_repository, &department).success == 1);
    assert(DoctorRepository_save(&doctor_repository, &doctor).success == 1);
}

static void seed_user_account(
    const DesktopAdapterTestContext *context,
    const char *user_id,
    const char *password,
    UserRole role
) {
    AuthService auth_service;
    Result result = AuthService_init(&auth_service, context->user_path, context->patient_path);

    assert(result.success == 1);
    assert(AuthService_register_user(&auth_service, user_id, password, role).success == 1);
}

static void seed_ward_and_bed(const DesktopAdapterTestContext *context) {
    WardRepository ward_repository;
    BedRepository bed_repository;
    Ward ward;
    Bed bed;
    Result result;

    memset(&ward, 0, sizeof(ward));
    strcpy(ward.ward_id, "WRD0001");
    strcpy(ward.name, "Ward A");
    strcpy(ward.department_id, "DEP0001");
    strcpy(ward.location, "Floor 5");
    ward.capacity = 3;
    ward.occupied_beds = 0;
    ward.status = WARD_STATUS_ACTIVE;

    memset(&bed, 0, sizeof(bed));
    strcpy(bed.bed_id, "BED0001");
    strcpy(bed.ward_id, "WRD0001");
    strcpy(bed.room_no, "501");
    strcpy(bed.bed_no, "01");
    bed.status = BED_STATUS_AVAILABLE;

    result = WardRepository_init(&ward_repository, context->ward_path);
    assert(result.success == 1);
    result = BedRepository_init(&bed_repository, context->bed_path);
    assert(result.success == 1);
    assert(WardRepository_append(&ward_repository, &ward).success == 1);
    assert(BedRepository_append(&bed_repository, &bed).success == 1);
}

static void test_login_and_patient_search(void) {
    DesktopAdapterTestContext context;
    MenuApplication application;
    User user;
    LinkedList results;
    LinkedList invalid_mode_results;
    Patient *patient = 0;
    Result result;

    setup_context(&context, "desktop_login");
    seed_patient(&context, "PATD001", "DesktopPatient");
    seed_patient(&context, "PATD003", "DesktopPatient");
    seed_user_account(&context, "PATD001", "desktop-pass", USER_ROLE_PATIENT);

    result = DesktopAdapters_init_application(&application, &context.paths);
    assert(result.success == 1);

    result = DesktopAdapters_login(&application, "PATD001", "desktop-pass", USER_ROLE_PATIENT, &user);
    assert(result.success == 1);
    assert(strcmp(user.user_id, "PATD001") == 0);

    LinkedList_init(&results);
    result = DesktopAdapters_search_patients(
        &application,
        DESKTOP_PATIENT_SEARCH_BY_ID,
        "PATD001",
        &results
    );
    assert(result.success == 1);
    assert(LinkedList_count(&results) == 1);
    patient = (Patient *)results.head->data;
    assert(patient != 0);
    assert(strcmp(patient->name, "DesktopPatient") == 0);
    LinkedList_clear(&results, free);

    LinkedList_init(&results);
    result = DesktopAdapters_search_patients(
        &application,
        DESKTOP_PATIENT_SEARCH_BY_NAME,
        "DesktopPatient",
        &results
    );
    assert(result.success == 1);
    assert(LinkedList_count(&results) == 2);
    LinkedList_clear(&results, free);

    LinkedList_init(&invalid_mode_results);
    result = DesktopAdapters_search_patients(
        &application,
        (DesktopPatientSearchMode)99,
        "DesktopPatient",
        &invalid_mode_results
    );
    assert(result.success == 0);
    assert(LinkedList_count(&invalid_mode_results) == 0);
}

static void test_dashboard_loads_recent_data(void) {
    DesktopAdapterTestContext context;
    MenuApplication application;
    DesktopDashboardState dashboard;
    Registration registration;
    Registration *recent_registration = 0;
    Registration *previous_registration = 0;
    const LinkedListNode *registration_node = 0;
    LinkedList dispenses;
    DispenseRecord *record = 0;
    DispenseRecord *recent_dispense = 0;
    DispenseRecord *previous_dispense = 0;
    const LinkedListNode *dispense_node = 0;
    char buffer[256];
    char registered_at[HIS_DOMAIN_TIME_CAPACITY];
    char dispensed_at[HIS_DOMAIN_TIME_CAPACITY];
    char prescription_id[HIS_DOMAIN_ID_CAPACITY];
    int index = 0;
    Result result;

    setup_context(&context, "desktop_dashboard");
    seed_patient(&context, "PATD002", "DashboardPatient");
    seed_department_and_doctor(&context);
    seed_user_account(&context, "ADM0001", "admin-pass", USER_ROLE_ADMIN);

    result = DesktopAdapters_init_application(&application, &context.paths);
    assert(result.success == 1);

    result = MenuApplication_add_medicine(
        &application,
        &(Medicine){
            "MEDD001",
            "DesktopMedicine",
            12.00,
            10,
            "DEP0001",
            10
        },
        buffer,
        sizeof(buffer)
    );
    assert(result.success == 1);

    for (index = 0; index < 6; index++) {
        snprintf(
            registered_at,
            sizeof(registered_at),
            "2026-03-21T09:0%d",
            index
        );
        memset(&registration, 0, sizeof(registration));
        result = DesktopAdapters_submit_registration(
            &application,
            "PATD002",
            "DOC0001",
            "DEP0001",
            registered_at,
            &registration
        );
        assert(result.success == 1);

        snprintf(prescription_id, sizeof(prescription_id), "RXD%03d", index + 1);
        snprintf(
            dispensed_at,
            sizeof(dispensed_at),
            "2026-03-21T10:0%d",
            index
        );
        result = MenuApplication_dispense_medicine(
            &application,
            "PATD002",
            prescription_id,
            "MEDD001",
            1,
            "PHA0001",
            dispensed_at,
            buffer,
            sizeof(buffer)
        );
        assert(result.success == 1);
    }

    memset(&dashboard, 0, sizeof(dashboard));
    LinkedList_init(&dashboard.recent_registrations);
    LinkedList_init(&dashboard.recent_dispensations);
    result = DesktopAdapters_load_dashboard(&application, &dashboard);
    assert(result.success == 1);
    assert(dashboard.patient_count >= 1);
    assert(dashboard.registration_count >= 1);
    assert(dashboard.low_stock_count >= 1);
    assert(LinkedList_count(&dashboard.recent_registrations) == 5);
    assert(LinkedList_count(&dashboard.recent_dispensations) == 5);
    registration_node = dashboard.recent_registrations.head;
    while (registration_node != 0) {
        recent_registration = (Registration *)registration_node->data;
        assert(recent_registration != 0);
        if (previous_registration != 0) {
            assert(strcmp(recent_registration->registered_at, previous_registration->registered_at) < 0);
        }
        previous_registration = recent_registration;
        registration_node = registration_node->next;
    }
    dispense_node = dashboard.recent_dispensations.head;
    while (dispense_node != 0) {
        recent_dispense = (DispenseRecord *)dispense_node->data;
        assert(recent_dispense != 0);
        if (previous_dispense != 0) {
            assert(strcmp(recent_dispense->dispensed_at, previous_dispense->dispensed_at) < 0);
        }
        previous_dispense = recent_dispense;
        dispense_node = dispense_node->next;
    }
    LinkedList_clear(&dashboard.recent_registrations, free);
    LinkedList_clear(&dashboard.recent_dispensations, free);

    LinkedList_init(&dispenses);
    result = DesktopAdapters_load_dispense_history(&application, "PATD002", &dispenses);
    assert(result.success == 1);
    assert(LinkedList_count(&dispenses) == 6);
    record = (DispenseRecord *)dispenses.head->data;
    assert(record != 0);
    assert(strcmp(record->patient_id, "PATD002") == 0);
    assert(strcmp(record->prescription_id, "RXD006") == 0);
    LinkedList_clear(&dispenses, free);
}

static void test_business_helpers_wrap_menu_application_apis(void) {
    DesktopAdapterTestContext context;
    MenuApplication application;
    Registration registration;
    Medicine medicine = {
        "MEDD010",
        "BusinessMedicine",
        18.00,
        6,
        "DEP0001",
        5
    };
    char output[1024];
    Result result;

    setup_context(&context, "desktop_business_helpers");
    seed_patient(&context, "PATD010", "BusinessPatient");
    seed_department_and_doctor(&context);
    seed_ward_and_bed(&context);

    result = DesktopAdapters_init_application(&application, &context.paths);
    assert(result.success == 1);

    memset(&registration, 0, sizeof(registration));
    result = DesktopAdapters_submit_registration(
        &application,
        "PATD010",
        "DOC0001",
        "DEP0001",
        "2026-03-21T09:00",
        &registration
    );
    assert(result.success == 1);

    memset(output, 0, sizeof(output));
    result = DesktopAdapters_query_pending_registrations_by_doctor(
        &application,
        "DOC0001",
        output,
        sizeof(output)
    );
    assert(result.success == 1);
    assert(strstr(output, "REG0001") != 0);

    memset(output, 0, sizeof(output));
    result = DesktopAdapters_create_visit_record(
        &application,
        "REG0001",
        "Cough",
        "Upper respiratory infection",
        "Drink water",
        1,
        0,
        1,
        "2026-03-21T09:30",
        output,
        sizeof(output)
    );
    assert(result.success == 1);
    assert(strstr(output, "VIS0001") != 0);

    memset(output, 0, sizeof(output));
    result = DesktopAdapters_query_patient_history(
        &application,
        "PATD010",
        output,
        sizeof(output)
    );
    assert(result.success == 1);
    assert(strstr(output, "registrations=1") != 0);
    assert(strstr(output, "visits=1") != 0);

    memset(output, 0, sizeof(output));
    result = DesktopAdapters_create_examination_record(
        &application,
        "VIS0001",
        "Blood test",
        "Laboratory",
        "2026-03-21T09:40",
        output,
        sizeof(output)
    );
    assert(result.success == 1);
    assert(strstr(output, "EXM0001") != 0);

    memset(output, 0, sizeof(output));
    result = DesktopAdapters_complete_examination_record(
        &application,
        "EXM0001",
        "No abnormalities",
        "2026-03-21T10:00",
        output,
        sizeof(output)
    );
    assert(result.success == 1);
    assert(strstr(output, "EXM0001") != 0);

    memset(output, 0, sizeof(output));
    result = DesktopAdapters_list_wards(&application, output, sizeof(output));
    assert(result.success == 1);
    assert(strstr(output, "WRD0001") != 0);

    memset(output, 0, sizeof(output));
    result = DesktopAdapters_list_beds_by_ward(
        &application,
        "WRD0001",
        output,
        sizeof(output)
    );
    assert(result.success == 1);
    assert(strstr(output, "BED0001") != 0);

    memset(output, 0, sizeof(output));
    result = DesktopAdapters_admit_patient(
        &application,
        "PATD010",
        "WRD0001",
        "BED0001",
        "2026-03-21T11:00",
        "observation",
        output,
        sizeof(output)
    );
    assert(result.success == 1);
    assert(strstr(output, "ADM0001") != 0);

    memset(output, 0, sizeof(output));
    result = DesktopAdapters_query_active_admission_by_patient(
        &application,
        "PATD010",
        output,
        sizeof(output)
    );
    assert(result.success == 1);
    assert(strstr(output, "ADM0001") != 0);

    memset(output, 0, sizeof(output));
    result = DesktopAdapters_query_current_patient_by_bed(
        &application,
        "BED0001",
        output,
        sizeof(output)
    );
    assert(result.success == 1);
    assert(strstr(output, "PATD010") != 0);

    memset(output, 0, sizeof(output));
    result = DesktopAdapters_discharge_patient(
        &application,
        "ADM0001",
        "2026-03-21T13:00",
        "stable",
        output,
        sizeof(output)
    );
    assert(result.success == 1);
    assert(strstr(output, "ADM0001") != 0);

    memset(output, 0, sizeof(output));
    result = DesktopAdapters_add_medicine(
        &application,
        &medicine,
        output,
        sizeof(output)
    );
    assert(result.success == 1);
    assert(strstr(output, "MEDD010") != 0);

    memset(output, 0, sizeof(output));
    result = DesktopAdapters_restock_medicine(
        &application,
        "MEDD010",
        4,
        output,
        sizeof(output)
    );
    assert(result.success == 1);
    assert(strstr(output, "10") != 0);

    memset(output, 0, sizeof(output));
    result = DesktopAdapters_query_medicine_stock(
        &application,
        "MEDD010",
        output,
        sizeof(output)
    );
    assert(result.success == 1);
    assert(strstr(output, "10") != 0);

    memset(output, 0, sizeof(output));
    result = DesktopAdapters_dispense_medicine(
        &application,
        "PATD010",
        "RXD010",
        "MEDD010",
        7,
        "PHA0001",
        "2026-03-21T14:00",
        output,
        sizeof(output)
    );
    assert(result.success == 1);
    assert(strstr(output, "RXD010") != 0);

    memset(output, 0, sizeof(output));
    result = DesktopAdapters_query_medicine_stock(
        &application,
        "MEDD010",
        output,
        sizeof(output)
    );
    assert(result.success == 1);
    assert(strstr(output, "3") != 0);

    memset(output, 0, sizeof(output));
    result = DesktopAdapters_find_low_stock_medicines(
        &application,
        output,
        sizeof(output)
    );
    assert(result.success == 1);
    assert(strstr(output, "MEDD010") != 0);
}

int main(void) {
    test_login_and_patient_search();
    test_dashboard_loads_recent_data();
    test_business_helpers_wrap_menu_application_apis();
    return 0;
}
