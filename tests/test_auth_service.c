#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "domain/Patient.h"
#include "service/AuthService.h"
#include "service/PatientService.h"

static void TestAuthService_read_file(char *buffer, size_t buffer_size, const char *path) {
    FILE *file = 0;
    size_t read_size = 0;

    assert(buffer != 0);
    assert(path != 0);
    memset(buffer, 0, buffer_size);

    file = fopen(path, "r");
    assert(file != 0);
    read_size = fread(buffer, 1, buffer_size - 1, file);
    buffer[read_size] = '\0';
    fclose(file);
}

static void TestAuthService_make_path(char *buffer, size_t buffer_size, const char *name) {
    assert(buffer != 0);
    assert(name != 0);

    snprintf(
        buffer,
        buffer_size,
        "build/test_auth_service_data/%s_%ld.txt",
        name,
        (long)time(0)
    );
    remove(buffer);
}

static void TestAuthService_seed_patient(const char *patient_path, const char *patient_id) {
    PatientService patient_service;
    Patient patient;
    Result result = PatientService_init(&patient_service, patient_path);

    assert(result.success == 1);
    memset(&patient, 0, sizeof(patient));
    strcpy(patient.patient_id, patient_id);
    strcpy(patient.name, "Auth Patient");
    patient.gender = PATIENT_GENDER_FEMALE;
    patient.age = 30;
    strcpy(patient.contact, "13800009999");
    strcpy(patient.id_card, "110101199001010011");
    strcpy(patient.allergy, "None");
    strcpy(patient.medical_history, "Healthy");
    patient.is_inpatient = 0;
    strcpy(patient.remarks, "");

    result = PatientService_create_patient(&patient_service, &patient);
    assert(result.success == 1);
}

static void test_patient_binding_and_login_success(void) {
    char user_path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    char patient_path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    char saved_content[TEXT_FILE_REPOSITORY_LINE_CAPACITY];
    AuthService service;
    User user;
    Result result;

    TestAuthService_make_path(user_path, sizeof(user_path), "users");
    TestAuthService_make_path(patient_path, sizeof(patient_path), "patients");
    TestAuthService_seed_patient(patient_path, "PAT9001");

    result = AuthService_init(&service, user_path, patient_path);
    assert(result.success == 1);

    result = AuthService_register_user(&service, "PAT9001", "safe-pass", USER_ROLE_PATIENT);
    assert(result.success == 1);
    TestAuthService_read_file(saved_content, sizeof(saved_content), user_path);
    assert(strstr(saved_content, "PAT9001") != 0);
    assert(strstr(saved_content, "safe-pass") == 0);

    result = AuthService_authenticate(
        &service,
        "PAT9001",
        "safe-pass",
        USER_ROLE_PATIENT,
        &user
    );
    assert(result.success == 1);
    assert(strcmp(user.user_id, "PAT9001") == 0);
    assert(user.role == USER_ROLE_PATIENT);
}

static void test_patient_registration_requires_existing_patient_record(void) {
    char user_path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    char patient_path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    AuthService service;
    Result result;

    TestAuthService_make_path(user_path, sizeof(user_path), "missing_patient_users");
    TestAuthService_make_path(patient_path, sizeof(patient_path), "missing_patient_patients");

    result = AuthService_init(&service, user_path, patient_path);
    assert(result.success == 1);

    result = AuthService_register_user(&service, "PAT9999", "safe-pass", USER_ROLE_PATIENT);
    assert(result.success == 0);
}

static void test_authenticate_rejects_wrong_password_and_role_mismatch(void) {
    char user_path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    char patient_path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    AuthService service;
    User user;
    Result result;

    TestAuthService_make_path(user_path, sizeof(user_path), "mismatch_users");
    TestAuthService_make_path(patient_path, sizeof(patient_path), "mismatch_patients");
    TestAuthService_seed_patient(patient_path, "PAT9010");

    result = AuthService_init(&service, user_path, patient_path);
    assert(result.success == 1);

    result = AuthService_register_user(&service, "PAT9010", "safe-pass", USER_ROLE_PATIENT);
    assert(result.success == 1);

    result = AuthService_authenticate(&service, "PAT9010", "wrong-pass", USER_ROLE_PATIENT, &user);
    assert(result.success == 0);

    result = AuthService_authenticate(&service, "PAT9010", "safe-pass", USER_ROLE_DOCTOR, &user);
    assert(result.success == 0);
}

static void test_register_rejects_blank_password_and_invalid_role(void) {
    char user_path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    char patient_path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    AuthService service;
    Result result;

    TestAuthService_make_path(user_path, sizeof(user_path), "validate_users");
    TestAuthService_make_path(patient_path, sizeof(patient_path), "validate_patients");
    TestAuthService_seed_patient(patient_path, "PAT9020");

    result = AuthService_init(&service, user_path, patient_path);
    assert(result.success == 1);

    result = AuthService_register_user(&service, "PAT9020", "", USER_ROLE_PATIENT);
    assert(result.success == 0);

    result = AuthService_register_user(&service, "PAT9020", "safe-pass", USER_ROLE_UNKNOWN);
    assert(result.success == 0);
}

int main(void) {
    test_patient_binding_and_login_success();
    test_patient_registration_requires_existing_patient_record();
    test_authenticate_rejects_wrong_password_and_role_mismatch();
    test_register_rejects_blank_password_and_invalid_role();
    return 0;
}
