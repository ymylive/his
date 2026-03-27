#include <assert.h>
#include <stdio.h>
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

    assert(LinkedList_count(&patients) >= 10);
    assert(LinkedList_count(&departments) >= 4);
    assert(LinkedList_count(&doctors) >= 7);
    assert(LinkedList_count(&registrations) >= 8);
    assert(LinkedList_count(&visits) >= 5);
    assert(LinkedList_count(&examinations) >= 4);
    assert(LinkedList_count(&wards) >= 3);
    assert(LinkedList_count(&beds) >= 8);
    assert(LinkedList_count(&admissions) >= 2);
    assert(LinkedList_count(&medicines) >= 6);
    assert(LinkedList_count(&dispenses) >= 4);

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

int main(void) {
    test_demo_accounts_exist();
    test_demo_domain_data_has_answering_scale();
    return 0;
}
