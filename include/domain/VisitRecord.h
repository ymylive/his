#ifndef HIS_DOMAIN_VISIT_RECORD_H
#define HIS_DOMAIN_VISIT_RECORD_H

#include "domain/DomainTypes.h"

typedef struct VisitRecord {
    char visit_id[HIS_DOMAIN_ID_CAPACITY];
    char registration_id[HIS_DOMAIN_ID_CAPACITY];
    char patient_id[HIS_DOMAIN_ID_CAPACITY];
    char doctor_id[HIS_DOMAIN_ID_CAPACITY];
    char department_id[HIS_DOMAIN_ID_CAPACITY];
    char chief_complaint[HIS_DOMAIN_TEXT_CAPACITY];
    char diagnosis[HIS_DOMAIN_TEXT_CAPACITY];
    char advice[HIS_DOMAIN_TEXT_CAPACITY];
    int need_exam;
    int need_admission;
    int need_medicine;
    char visit_time[HIS_DOMAIN_TIME_CAPACITY];
} VisitRecord;

#endif
