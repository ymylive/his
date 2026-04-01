#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/LinkedList.h"
#include "domain/User.h"
#include "repository/AdmissionRepository.h"
#include "repository/DepartmentRepository.h"
#include "repository/DispenseRecordRepository.h"
#include "repository/DoctorRepository.h"
#include "repository/ExaminationRecordRepository.h"
#include "repository/MedicineRepository.h"
#include "repository/PatientRepository.h"
#include "repository/RegistrationRepository.h"
#include "repository/UserRepository.h"
#include "repository/VisitRecordRepository.h"
#include "repository/WardRepository.h"
#include "repository/BedRepository.h"
#include "ui/DemoData.h"

static const char *resolve_demo_data_path(const char *file_name) {
    static char path[128];
    FILE *file = 0;

    snprintf(path, sizeof(path), "../data/%s", file_name);
    file = fopen(path, "r");
    if (file != 0) {
        fclose(file);
        return path;
    }

    snprintf(path, sizeof(path), "data/%s", file_name);
    return path;
}

static const char *resolve_demo_seed_path(const char *file_name) {
    static char path[160];
    FILE *file = 0;

    snprintf(path, sizeof(path), "../data/demo_seed/%s", file_name);
    file = fopen(path, "r");
    if (file != 0) {
        fclose(file);
        return path;
    }

    snprintf(path, sizeof(path), "data/demo_seed/%s", file_name);
    return path;
}

static void copy_file_contents(const char *source_path, const char *destination_path) {
    FILE *source = fopen(source_path, "rb");
    FILE *destination = fopen(destination_path, "wb");
    char buffer[4096];
    size_t read_bytes = 0;

    assert(source != 0);
    assert(destination != 0);

    while ((read_bytes = fread(buffer, 1, sizeof(buffer), source)) > 0) {
        assert(fwrite(buffer, 1, read_bytes, destination) == read_bytes);
    }

    fclose(source);
    fclose(destination);
}

static void test_demo_accounts_exist(void) {
    UserRepository repository;
    User user;
    Result result = UserRepository_init(&repository, resolve_demo_data_path("users.txt"));

    assert(result.success == 1);
    assert(UserRepository_find_by_user_id(&repository, "ADM0001", &user).success == 1);
    assert(user.role == USER_ROLE_ADMIN);
    assert(UserRepository_find_by_user_id(&repository, "CLK0001", &user).success == 1);
    assert(user.role == USER_ROLE_REGISTRATION_CLERK);
    assert(UserRepository_find_by_user_id(&repository, "DOC0001", &user).success == 1);
    assert(user.role == USER_ROLE_DOCTOR);
    assert(UserRepository_find_by_user_id(&repository, "INP0001", &user).success == 1);
    assert(user.role == USER_ROLE_INPATIENT_REGISTRAR);
    assert(UserRepository_find_by_user_id(&repository, "WRD0001", &user).success == 1);
    assert(user.role == USER_ROLE_WARD_MANAGER);
    assert(UserRepository_find_by_user_id(&repository, "PHA0001", &user).success == 1);
    assert(user.role == USER_ROLE_PHARMACY);
    assert(UserRepository_find_by_user_id(&repository, "PAT0001", &user).success == 1);
    assert(user.role == USER_ROLE_PATIENT);
}

static void test_demo_domain_data_has_answering_scale(void) {
    PatientRepository patient_repository;
    DepartmentRepository department_repository;
    DoctorRepository doctor_repository;
    RegistrationRepository registration_repository;
    VisitRecordRepository visit_repository;
    ExaminationRecordRepository examination_repository;
    WardRepository ward_repository;
    BedRepository bed_repository;
    AdmissionRepository admission_repository;
    MedicineRepository medicine_repository;
    DispenseRecordRepository dispense_repository;
    LinkedList patients;
    LinkedList departments;
    LinkedList doctors;
    LinkedList registrations;
    LinkedList visits;
    LinkedList examinations;
    LinkedList wards;
    LinkedList beds;
    LinkedList admissions;
    LinkedList medicines;
    LinkedList dispenses;
    Result result;

    LinkedList_init(&patients);
    LinkedList_init(&departments);
    LinkedList_init(&doctors);
    LinkedList_init(&registrations);
    LinkedList_init(&visits);
    LinkedList_init(&examinations);
    LinkedList_init(&wards);
    LinkedList_init(&beds);
    LinkedList_init(&admissions);
    LinkedList_init(&medicines);
    LinkedList_init(&dispenses);

    assert(PatientRepository_init(&patient_repository, resolve_demo_data_path("patients.txt")).success == 1);
    assert(DepartmentRepository_init(&department_repository, resolve_demo_data_path("departments.txt")).success == 1);
    assert(DoctorRepository_init(&doctor_repository, resolve_demo_data_path("doctors.txt")).success == 1);
    assert(RegistrationRepository_init(&registration_repository, resolve_demo_data_path("registrations.txt")).success == 1);
    assert(VisitRecordRepository_init(&visit_repository, resolve_demo_data_path("visits.txt")).success == 1);
    assert(ExaminationRecordRepository_init(&examination_repository, resolve_demo_data_path("examinations.txt")).success == 1);
    assert(WardRepository_init(&ward_repository, resolve_demo_data_path("wards.txt")).success == 1);
    assert(BedRepository_init(&bed_repository, resolve_demo_data_path("beds.txt")).success == 1);
    assert(AdmissionRepository_init(&admission_repository, resolve_demo_data_path("admissions.txt")).success == 1);
    assert(MedicineRepository_init(&medicine_repository, resolve_demo_data_path("medicines.txt")).success == 1);
    assert(DispenseRecordRepository_init(&dispense_repository, resolve_demo_data_path("dispense_records.txt")).success == 1);

    result = PatientRepository_load_all(&patient_repository, &patients);
    assert(result.success == 1);
    result = DepartmentRepository_load_all(&department_repository, &departments);
    assert(result.success == 1);
    result = DoctorRepository_load_all(&doctor_repository, &doctors);
    assert(result.success == 1);
    result = RegistrationRepository_load_all(&registration_repository, &registrations);
    assert(result.success == 1);
    result = VisitRecordRepository_load_all(&visit_repository, &visits);
    assert(result.success == 1);
    result = ExaminationRecordRepository_load_all(&examination_repository, &examinations);
    assert(result.success == 1);
    result = WardRepository_load_all(&ward_repository, &wards);
    assert(result.success == 1);
    result = BedRepository_load_all(&bed_repository, &beds);
    assert(result.success == 1);
    result = AdmissionRepository_load_all(&admission_repository, &admissions);
    assert(result.success == 1);
    result = MedicineRepository_load_all(&medicine_repository, &medicines);
    assert(result.success == 1);
    result = DispenseRecordRepository_load_all(&dispense_repository, &dispenses);
    assert(result.success == 1);

    assert(LinkedList_count(&patients) >= 20);
    assert(LinkedList_count(&departments) >= 6);
    assert(LinkedList_count(&doctors) >= 10);
    assert(LinkedList_count(&registrations) >= 20);
    assert(LinkedList_count(&visits) >= 14);
    assert(LinkedList_count(&examinations) >= 9);
    assert(LinkedList_count(&wards) >= 5);
    assert(LinkedList_count(&beds) >= 18);
    assert(LinkedList_count(&admissions) >= 5);
    assert(LinkedList_count(&medicines) >= 10);
    assert(LinkedList_count(&dispenses) >= 8);

    PatientRepository_clear_loaded_list(&patients);
    DepartmentRepository_clear_list(&departments);
    DoctorRepository_clear_list(&doctors);
    RegistrationRepository_clear_list(&registrations);
    VisitRecordRepository_clear_list(&visits);
    ExaminationRecordRepository_clear_list(&examinations);
    WardRepository_clear_loaded_list(&wards);
    BedRepository_clear_loaded_list(&beds);
    AdmissionRepository_clear_loaded_list(&admissions);
    MedicineRepository_clear_list(&medicines);
    DispenseRecordRepository_clear_list(&dispenses);
}

static void test_demo_seed_files_exist_and_reset_runtime_files(void) {
    MenuApplicationPaths paths;
    char runtime_users[256];
    char runtime_patients[256];
    char runtime_departments[256];
    char runtime_doctors[256];
    char runtime_registrations[256];
    char runtime_visits[256];
    char runtime_examinations[256];
    char runtime_wards[256];
    char runtime_beds[256];
    char runtime_admissions[256];
    char runtime_medicines[256];
    char runtime_dispenses[256];
    char buffer[256];
    FILE *file = 0;
    char line[256];
    Result result;

#ifdef _WIN32
    system("mkdir build\\demo_seed >NUL 2>NUL");
#else
    system("mkdir -p build/demo_seed");
#endif

    assert(fopen(resolve_demo_seed_path("users.txt"), "r") != 0);
    assert(fopen(resolve_demo_seed_path("patients.txt"), "r") != 0);
    assert(fopen(resolve_demo_seed_path("departments.txt"), "r") != 0);
    assert(fopen(resolve_demo_seed_path("doctors.txt"), "r") != 0);
    assert(fopen(resolve_demo_seed_path("registrations.txt"), "r") != 0);
    assert(fopen(resolve_demo_seed_path("visits.txt"), "r") != 0);
    assert(fopen(resolve_demo_seed_path("examinations.txt"), "r") != 0);
    assert(fopen(resolve_demo_seed_path("wards.txt"), "r") != 0);
    assert(fopen(resolve_demo_seed_path("beds.txt"), "r") != 0);
    assert(fopen(resolve_demo_seed_path("admissions.txt"), "r") != 0);
    assert(fopen(resolve_demo_seed_path("medicines.txt"), "r") != 0);
    assert(fopen(resolve_demo_seed_path("dispense_records.txt"), "r") != 0);

    snprintf(runtime_users, sizeof(runtime_users), "build/users.txt");
    snprintf(runtime_patients, sizeof(runtime_patients), "build/patients.txt");
    snprintf(runtime_departments, sizeof(runtime_departments), "build/departments.txt");
    snprintf(runtime_doctors, sizeof(runtime_doctors), "build/doctors.txt");
    snprintf(runtime_registrations, sizeof(runtime_registrations), "build/registrations.txt");
    snprintf(runtime_visits, sizeof(runtime_visits), "build/visits.txt");
    snprintf(runtime_examinations, sizeof(runtime_examinations), "build/examinations.txt");
    snprintf(runtime_wards, sizeof(runtime_wards), "build/wards.txt");
    snprintf(runtime_beds, sizeof(runtime_beds), "build/beds.txt");
    snprintf(runtime_admissions, sizeof(runtime_admissions), "build/admissions.txt");
    snprintf(runtime_medicines, sizeof(runtime_medicines), "build/medicines.txt");
    snprintf(runtime_dispenses, sizeof(runtime_dispenses), "build/dispense_records.txt");

    copy_file_contents(resolve_demo_seed_path("users.txt"), "build/demo_seed/users.txt");
    copy_file_contents(resolve_demo_seed_path("patients.txt"), "build/demo_seed/patients.txt");
    copy_file_contents(resolve_demo_seed_path("departments.txt"), "build/demo_seed/departments.txt");
    copy_file_contents(resolve_demo_seed_path("doctors.txt"), "build/demo_seed/doctors.txt");
    copy_file_contents(resolve_demo_seed_path("registrations.txt"), "build/demo_seed/registrations.txt");
    copy_file_contents(resolve_demo_seed_path("visits.txt"), "build/demo_seed/visits.txt");
    copy_file_contents(resolve_demo_seed_path("examinations.txt"), "build/demo_seed/examinations.txt");
    copy_file_contents(resolve_demo_seed_path("wards.txt"), "build/demo_seed/wards.txt");
    copy_file_contents(resolve_demo_seed_path("beds.txt"), "build/demo_seed/beds.txt");
    copy_file_contents(resolve_demo_seed_path("admissions.txt"), "build/demo_seed/admissions.txt");
    copy_file_contents(resolve_demo_seed_path("medicines.txt"), "build/demo_seed/medicines.txt");
    copy_file_contents(resolve_demo_seed_path("dispense_records.txt"), "build/demo_seed/dispense_records.txt");

    file = fopen(runtime_users, "w");
    assert(file != 0);
    fputs("corrupted\n", file);
    fclose(file);

    file = fopen(runtime_patients, "w");
    assert(file != 0);
    fputs("corrupted\n", file);
    fclose(file);

    file = fopen(runtime_departments, "w");
    assert(file != 0);
    fputs("corrupted\n", file);
    fclose(file);

    file = fopen(runtime_doctors, "w");
    assert(file != 0);
    fputs("corrupted\n", file);
    fclose(file);

    file = fopen(runtime_registrations, "w");
    assert(file != 0);
    fputs("corrupted\n", file);
    fclose(file);

    file = fopen(runtime_visits, "w");
    assert(file != 0);
    fputs("corrupted\n", file);
    fclose(file);

    file = fopen(runtime_examinations, "w");
    assert(file != 0);
    fputs("corrupted\n", file);
    fclose(file);

    file = fopen(runtime_wards, "w");
    assert(file != 0);
    fputs("corrupted\n", file);
    fclose(file);

    file = fopen(runtime_beds, "w");
    assert(file != 0);
    fputs("corrupted\n", file);
    fclose(file);

    file = fopen(runtime_admissions, "w");
    assert(file != 0);
    fputs("corrupted\n", file);
    fclose(file);

    file = fopen(runtime_medicines, "w");
    assert(file != 0);
    fputs("corrupted\n", file);
    fclose(file);

    file = fopen(runtime_dispenses, "w");
    assert(file != 0);
    fputs("corrupted\n", file);
    fclose(file);

    paths.user_path = runtime_users;
    paths.patient_path = runtime_patients;
    paths.department_path = runtime_departments;
    paths.doctor_path = runtime_doctors;
    paths.registration_path = runtime_registrations;
    paths.visit_path = runtime_visits;
    paths.examination_path = runtime_examinations;
    paths.ward_path = runtime_wards;
    paths.bed_path = runtime_beds;
    paths.admission_path = runtime_admissions;
    paths.medicine_path = runtime_medicines;
    paths.dispense_record_path = runtime_dispenses;

    result = DemoData_reset(&paths, buffer, sizeof(buffer));
    assert(result.success == 1);
    assert(strstr(buffer, "演示数据已重置") != 0);

    file = fopen(runtime_patients, "r");
    assert(file != 0);
    assert(fgets(line, sizeof(line), file) != 0);
    assert(strstr(line, "patient_id|name|gender") != 0);
    fclose(file);

    file = fopen(runtime_registrations, "r");
    assert(file != 0);
    assert(fgets(line, sizeof(line), file) != 0);
    assert(strstr(line, "registration_id|patient_id|doctor_id") != 0);
    fclose(file);
}

int main(void) {
    test_demo_accounts_exist();
    test_demo_domain_data_has_answering_scale();
    test_demo_seed_files_exist_and_reset_runtime_files();
    return 0;
}
