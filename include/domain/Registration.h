#ifndef HIS_DOMAIN_REGISTRATION_H
#define HIS_DOMAIN_REGISTRATION_H

#include "domain/DomainTypes.h"

typedef enum RegistrationStatus {
    REG_STATUS_PENDING = 0,
    REG_STATUS_DIAGNOSED = 1,
    REG_STATUS_CANCELLED = 2
} RegistrationStatus;

typedef struct Registration {
    char registration_id[HIS_DOMAIN_ID_CAPACITY];
    char patient_id[HIS_DOMAIN_ID_CAPACITY];
    char doctor_id[HIS_DOMAIN_ID_CAPACITY];
    char department_id[HIS_DOMAIN_ID_CAPACITY];
    char registered_at[HIS_DOMAIN_TIME_CAPACITY];
    RegistrationStatus status;
    char diagnosed_at[HIS_DOMAIN_TIME_CAPACITY];
    char cancelled_at[HIS_DOMAIN_TIME_CAPACITY];
} Registration;

int Registration_mark_diagnosed(Registration *registration, const char *diagnosed_at);
int Registration_cancel(Registration *registration, const char *cancelled_at);

#endif
