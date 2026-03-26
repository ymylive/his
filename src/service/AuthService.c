#include "service/AuthService.h"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "repository/RepositoryUtils.h"

static int AuthService_is_blank_text(const char *text) {
    if (text == 0) {
        return 1;
    }

    while (*text != '\0') {
        if (!isspace((unsigned char)*text)) {
            return 0;
        }

        text++;
    }

    return 1;
}

static int AuthService_is_valid_role(UserRole role) {
    return role == USER_ROLE_PATIENT ||
           role == USER_ROLE_DOCTOR ||
           role == USER_ROLE_ADMIN ||
           role == USER_ROLE_REGISTRATION_CLERK ||
           role == USER_ROLE_INPATIENT_REGISTRAR ||
           role == USER_ROLE_WARD_MANAGER ||
           role == USER_ROLE_PHARMACY;
}

static Result AuthService_validate_text(const char *text, const char *field_name) {
    char message[RESULT_MESSAGE_CAPACITY];

    if (AuthService_is_blank_text(text)) {
        snprintf(message, sizeof(message), "%s missing", field_name);
        return Result_make_failure(message);
    }

    if (!RepositoryUtils_is_safe_field_text(text)) {
        snprintf(message, sizeof(message), "%s contains reserved characters", field_name);
        return Result_make_failure(message);
    }

    return Result_make_success("text valid");
}

static void AuthService_generate_salt(char *salt_hex, size_t capacity) {
    unsigned int seed = (unsigned int)(time(NULL) ^ clock());
    int i = 0;

    (void)capacity;
    srand(seed);
    for (i = 0; i < 16; i++) {
        snprintf(salt_hex + i * 2, 3, "%02x", rand() % 256);
    }
}

static Result AuthService_hash_password(
    const char *password,
    const char *salt_hex,
    char *out_hash,
    size_t out_hash_capacity
) {
    uint64_t h1 = UINT64_C(1469598103934665603);
    uint64_t h2 = UINT64_C(1099511628211);
    uint64_t h3 = UINT64_C(7809847782465536322);
    uint64_t h4 = UINT64_C(9650029242287828579);
    size_t index = 0;
    size_t salt_len = 0;
    int iteration = 0;
    int written = 0;
    Result result = AuthService_validate_text(password, "password");

    if (result.success == 0) {
        return result;
    }

    if (out_hash == 0 || out_hash_capacity < HIS_USER_PASSWORD_HASH_CAPACITY) {
        return Result_make_failure("password hash buffer too small");
    }

    if (salt_hex == 0 || strlen(salt_hex) != 32) {
        return Result_make_failure("salt must be 32 hex characters");
    }

    /* Mix salt into initial state */
    salt_len = strlen(salt_hex);
    for (index = 0; index < salt_len; index++) {
        unsigned char value = (unsigned char)salt_hex[index];
        h1 ^= (uint64_t)(value + 7U);
        h1 *= UINT64_C(1099511628211);
        h2 ^= (uint64_t)(value + 13U);
        h2 *= UINT64_C(1469598103934665603);
    }

    /* 1000 iterations of password mixing */
    for (iteration = 0; iteration < 1000; iteration++) {
        index = 0;
        while (password[index] != '\0') {
            unsigned char value = (unsigned char)password[index];

            h1 ^= (uint64_t)(value + 17U);
            h1 *= UINT64_C(1099511628211);

            h2 ^= (uint64_t)(value + 37U);
            h2 *= UINT64_C(1469598103934665603);

            h3 ^= (uint64_t)(value + 73U);
            h3 *= UINT64_C(1099511628211);

            h4 ^= (uint64_t)(value + 131U);
            h4 *= UINT64_C(1469598103934665603);

            index++;
        }

        /* Feed back between rounds */
        h1 ^= h4;
        h2 ^= h3;
        h3 ^= h1;
        h4 ^= h2;
    }

    written = snprintf(
        out_hash,
        out_hash_capacity,
        "%s$%016llx%016llx%016llx%016llx",
        salt_hex,
        (unsigned long long)h1,
        (unsigned long long)h2,
        (unsigned long long)h3,
        (unsigned long long)h4
    );
    if (written != HIS_USER_PASSWORD_HASH_CAPACITY - 1) {
        return Result_make_failure("password hash formatting failed");
    }

    return Result_make_success("password hashed");
}

static Result AuthService_hash_password_legacy(
    const char *password,
    char *out_hash,
    size_t out_hash_capacity
) {
    uint64_t h1 = UINT64_C(1469598103934665603);
    uint64_t h2 = UINT64_C(1099511628211);
    uint64_t h3 = UINT64_C(7809847782465536322);
    uint64_t h4 = UINT64_C(9650029242287828579);
    size_t index = 0;
    int written = 0;
    Result result = AuthService_validate_text(password, "password");

    if (result.success == 0) {
        return result;
    }

    if (out_hash == 0 || out_hash_capacity < HIS_USER_PASSWORD_HASH_CAPACITY) {
        return Result_make_failure("password hash buffer too small");
    }

    while (password[index] != '\0') {
        unsigned char value = (unsigned char)password[index];

        h1 ^= (uint64_t)(value + 17U);
        h1 *= UINT64_C(1099511628211);

        h2 ^= (uint64_t)(value + 37U);
        h2 *= UINT64_C(1469598103934665603);

        h3 ^= (uint64_t)(value + 73U);
        h3 *= UINT64_C(1099511628211);

        h4 ^= (uint64_t)(value + 131U);
        h4 *= UINT64_C(1469598103934665603);

        index++;
    }

    written = snprintf(
        out_hash,
        out_hash_capacity,
        "%016llx%016llx%016llx%016llx",
        (unsigned long long)h1,
        (unsigned long long)h2,
        (unsigned long long)h3,
        (unsigned long long)h4
    );
    if (written != 64) {
        return Result_make_failure("password hash formatting failed");
    }

    return Result_make_success("password hashed");
}

static int AuthService_is_salted_password_hash(const char *stored_hash) {
    const char *dollar_pos = 0;

    if (stored_hash == 0) {
        return 0;
    }

    dollar_pos = strchr(stored_hash, '$');
    return dollar_pos != 0 && (dollar_pos - stored_hash) == 32;
}

static int AuthService_is_legacy_password_hash(const char *stored_hash) {
    return stored_hash != 0 && strchr(stored_hash, '$') == 0 && strlen(stored_hash) == 64;
}

static int AuthService_is_path_provided(const char *path) {
    return path != 0 && path[0] != '\0';
}

Result AuthService_init(
    AuthService *service,
    const char *user_path,
    const char *patient_path
) {
    Result result;

    if (service == 0) {
        return Result_make_failure("auth service missing");
    }

    memset(service, 0, sizeof(*service));
    result = UserRepository_init(&service->user_repository, user_path);
    if (result.success == 0) {
        return result;
    }

    if (!AuthService_is_path_provided(patient_path)) {
        service->patient_repository_enabled = 0;
        return Result_make_success("auth service initialized");
    }

    result = PatientRepository_init(&service->patient_repository, patient_path);
    if (result.success == 0) {
        return result;
    }

    service->patient_repository_enabled = 1;
    return Result_make_success("auth service initialized");
}

Result AuthService_register_user(
    AuthService *service,
    const char *user_id,
    const char *password,
    UserRole role
) {
    User user;
    Patient patient;
    char salt_hex[33];
    Result result;

    if (service == 0) {
        return Result_make_failure("auth service missing");
    }

    result = AuthService_validate_text(user_id, "user id");
    if (result.success == 0) {
        return result;
    }

    result = AuthService_validate_text(password, "password");
    if (result.success == 0) {
        return result;
    }

    if (!AuthService_is_valid_role(role)) {
        return Result_make_failure("user role invalid");
    }

    if (role == USER_ROLE_PATIENT) {
        if (service->patient_repository_enabled == 0) {
            return Result_make_failure("patient repository not configured");
        }

        result = PatientRepository_find_by_id(&service->patient_repository, user_id, &patient);
        if (result.success == 0) {
            return Result_make_failure("patient account requires existing patient id");
        }
    }

    memset(&user, 0, sizeof(user));
    strncpy(user.user_id, user_id, sizeof(user.user_id) - 1);
    user.user_id[sizeof(user.user_id) - 1] = '\0';
    user.role = role;

    memset(salt_hex, 0, sizeof(salt_hex));
    AuthService_generate_salt(salt_hex, sizeof(salt_hex));

    result = AuthService_hash_password(password, salt_hex, user.password_hash, sizeof(user.password_hash));
    if (result.success == 0) {
        return result;
    }

    return UserRepository_append(&service->user_repository, &user);
}

Result AuthService_authenticate(
    AuthService *service,
    const char *user_id,
    const char *password,
    UserRole required_role,
    User *out_user
) {
    User loaded_user;
    char password_hash[HIS_USER_PASSWORD_HASH_CAPACITY];
    char extracted_salt[33] = "00000000000000000000000000000000";
    int user_hash_format_known = 0;
    Result result;
    Result find_result;

    if (service == 0 || out_user == 0) {
        return Result_make_failure("auth arguments missing");
    }

    result = AuthService_validate_text(user_id, "user id");
    if (result.success == 0) {
        return result;
    }

    result = AuthService_validate_text(password, "password");
    if (result.success == 0) {
        return result;
    }

    if (required_role != USER_ROLE_UNKNOWN && !AuthService_is_valid_role(required_role)) {
        return Result_make_failure("required role invalid");
    }

    memset(&loaded_user, 0, sizeof(loaded_user));
    find_result = UserRepository_find_by_user_id(&service->user_repository, user_id, &loaded_user);
    memset(password_hash, 0, sizeof(password_hash));
    if (find_result.success != 0) {
        if (AuthService_is_salted_password_hash(loaded_user.password_hash)) {
            memcpy(extracted_salt, loaded_user.password_hash, 32);
            extracted_salt[32] = '\0';
            result = AuthService_hash_password(password, extracted_salt, password_hash, sizeof(password_hash));
            user_hash_format_known = 1;
        } else if (AuthService_is_legacy_password_hash(loaded_user.password_hash)) {
            result = AuthService_hash_password_legacy(password, password_hash, sizeof(password_hash));
            user_hash_format_known = 1;
        } else {
            result = AuthService_hash_password(password, extracted_salt, password_hash, sizeof(password_hash));
        }
    } else {
        result = AuthService_hash_password(password, extracted_salt, password_hash, sizeof(password_hash));
    }
    if (result.success == 0) {
        return result;
    }

    if (find_result.success == 0 || user_hash_format_known == 0) {
        return Result_make_failure("invalid credentials");
    }

    if (strcmp(loaded_user.password_hash, password_hash) != 0) {
        return Result_make_failure("invalid credentials");
    }

    if (required_role != USER_ROLE_UNKNOWN && loaded_user.role != required_role) {
        return Result_make_failure("user role mismatch");
    }

    *out_user = loaded_user;
    return Result_make_success("user authenticated");
}
