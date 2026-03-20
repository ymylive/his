#ifndef HIS_DOMAIN_ADMISSION_H
#define HIS_DOMAIN_ADMISSION_H

#include "domain/DomainTypes.h"

typedef enum AdmissionStatus {
    ADMISSION_STATUS_ACTIVE = 0,
    ADMISSION_STATUS_DISCHARGED = 1
} AdmissionStatus;

typedef struct Admission {
    char admission_id[HIS_DOMAIN_ID_CAPACITY];
    char patient_id[HIS_DOMAIN_ID_CAPACITY];
    char ward_id[HIS_DOMAIN_ID_CAPACITY];
    char bed_id[HIS_DOMAIN_ID_CAPACITY];
    char admitted_at[HIS_DOMAIN_TIME_CAPACITY];
    AdmissionStatus status;
    char discharged_at[HIS_DOMAIN_TIME_CAPACITY];
    char summary[HIS_DOMAIN_LARGE_TEXT_CAPACITY];
} Admission;

int Admission_is_active(const Admission *admission);
int Admission_discharge(Admission *admission, const char *discharged_at);

#endif
