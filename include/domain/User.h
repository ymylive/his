#ifndef HIS_DOMAIN_USER_H
#define HIS_DOMAIN_USER_H

#include "domain/DomainTypes.h"

#define HIS_USER_PASSWORD_HASH_CAPACITY 98

typedef enum UserRole {
    USER_ROLE_UNKNOWN = 0,
    USER_ROLE_PATIENT = 1,
    USER_ROLE_DOCTOR = 2,
    USER_ROLE_ADMIN = 3,
    USER_ROLE_REGISTRATION_CLERK = 4,
    USER_ROLE_INPATIENT_REGISTRAR = 5,
    USER_ROLE_WARD_MANAGER = 6,
    USER_ROLE_PHARMACY = 7
} UserRole;

typedef struct User {
    char user_id[HIS_DOMAIN_ID_CAPACITY];
    char password_hash[HIS_USER_PASSWORD_HASH_CAPACITY];
    UserRole role;
} User;

#endif
