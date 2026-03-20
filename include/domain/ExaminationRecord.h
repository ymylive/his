#ifndef HIS_DOMAIN_EXAMINATION_RECORD_H
#define HIS_DOMAIN_EXAMINATION_RECORD_H

#include "domain/DomainTypes.h"

typedef enum ExaminationStatus {
    EXAM_STATUS_PENDING = 0,
    EXAM_STATUS_COMPLETED = 1
} ExaminationStatus;

typedef struct ExaminationRecord {
    char examination_id[HIS_DOMAIN_ID_CAPACITY];
    char visit_id[HIS_DOMAIN_ID_CAPACITY];
    char patient_id[HIS_DOMAIN_ID_CAPACITY];
    char doctor_id[HIS_DOMAIN_ID_CAPACITY];
    char exam_item[HIS_DOMAIN_NAME_CAPACITY];
    char exam_type[HIS_DOMAIN_NAME_CAPACITY];
    ExaminationStatus status;
    char result[HIS_DOMAIN_TEXT_CAPACITY];
    char requested_at[HIS_DOMAIN_TIME_CAPACITY];
    char completed_at[HIS_DOMAIN_TIME_CAPACITY];
} ExaminationRecord;

#endif
