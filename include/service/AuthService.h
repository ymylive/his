#ifndef HIS_SERVICE_AUTH_SERVICE_H
#define HIS_SERVICE_AUTH_SERVICE_H

#include "common/Result.h"
#include "domain/User.h"
#include "repository/PatientRepository.h"
#include "repository/UserRepository.h"

typedef struct AuthService {
    UserRepository user_repository;
    PatientRepository patient_repository;
    int patient_repository_enabled;
} AuthService;

Result AuthService_init(
    AuthService *service,
    const char *user_path,
    const char *patient_path
);
Result AuthService_register_user(
    AuthService *service,
    const char *user_id,
    const char *password,
    UserRole role
);
Result AuthService_authenticate(
    AuthService *service,
    const char *user_id,
    const char *password,
    UserRole required_role,
    User *out_user
);

#endif
