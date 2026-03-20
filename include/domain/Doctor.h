#ifndef HIS_DOMAIN_DOCTOR_H
#define HIS_DOMAIN_DOCTOR_H

#include "domain/DomainTypes.h"

typedef enum DoctorStatus {
    DOCTOR_STATUS_INACTIVE = 0,
    DOCTOR_STATUS_ACTIVE = 1
} DoctorStatus;

typedef struct Doctor {
    char doctor_id[HIS_DOMAIN_ID_CAPACITY];
    char name[HIS_DOMAIN_NAME_CAPACITY];
    char title[HIS_DOMAIN_TITLE_CAPACITY];
    char department_id[HIS_DOMAIN_ID_CAPACITY];
    char schedule[HIS_DOMAIN_TEXT_CAPACITY];
    DoctorStatus status;
} Doctor;

#endif
