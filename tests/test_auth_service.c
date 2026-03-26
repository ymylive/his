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

static void TestAuthService_make_fixture_path(
    char *buffer,
    size_t buffer_size,
    const char *relative_path
) {
    char path_buffer[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    char *last_separator = 0;
    char *parent_separator = 0;

    assert(buffer != 0);
    assert(relative_path != 0);

    snprintf(path_buffer, sizeof(path_buffer), "%s", __FILE__);
    last_separator = strrchr(path_buffer, '\\');
    if (last_separator == 0) {
        last_separator = strrchr(path_buffer, '/');
    }
    assert(last_separator != 0);
    *last_separator = '\0';

    parent_separator = strrchr(path_buffer, '\\');
    if (parent_separator == 0) {
        parent_separator = strrchr(path_buffer, '/');
    }
    assert(parent_separator != 0);
    *parent_separator = '\0';

    snprintf(buffer, buffer_size, "%s/%s", path_buffer, relative_path);
}

static void TestAuthService_copy_file(const char *source_path, const char *target_path) {
    FILE *source = 0;
    FILE *target = 0;
    char buffer[256];
    size_t read_size = 0;

    assert(source_path != 0);
    assert(target_path != 0);

    source = fopen(source_path, "rb");
    assert(source != 0);
    target = fopen(target_path, "wb");
    assert(target != 0);

    while ((read_size = fread(buffer, 1, sizeof(buffer), source)) > 0) {
        assert(fwrite(buffer, 1, read_size, target) == read_size);
    }

    fclose(target);
    fclose(source);
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

static void test_authenticate_accepts_seeded_demo_accounts(void) {
    char fixture_user_path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    char copied_user_path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    char saved_content[TEXT_FILE_REPOSITORY_LINE_CAPACITY * 8];
    AuthService service;
    User user;
    Result result;

    TestAuthService_make_fixture_path(fixture_user_path, sizeof(fixture_user_path), "data/users.txt");
    TestAuthService_make_path(copied_user_path, sizeof(copied_user_path), "seeded_demo_users");
    TestAuthService_copy_file(fixture_user_path, copied_user_path);
    TestAuthService_read_file(saved_content, sizeof(saved_content), copied_user_path);
    assert(strstr(saved_content, "$") != 0);

    result = AuthService_init(&service, copied_user_path, "");

    assert(result.success == 1);

    result = AuthService_authenticate(&service, "ADM0001", "admin123", USER_ROLE_ADMIN, &user);
    assert(result.success == 1);
    assert(strcmp(user.user_id, "ADM0001") == 0);
    assert(user.role == USER_ROLE_ADMIN);
}

int main(void) {
    test_patient_binding_and_login_success();
    test_patient_registration_requires_existing_patient_record();
    test_authenticate_rejects_wrong_password_and_role_mismatch();
    test_register_rejects_blank_password_and_invalid_role();
    test_authenticate_accepts_seeded_demo_accounts();
    return 0;
}
