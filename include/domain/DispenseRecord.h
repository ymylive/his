#ifndef HIS_DOMAIN_DISPENSE_RECORD_H
#define HIS_DOMAIN_DISPENSE_RECORD_H

#include "domain/DomainTypes.h"

typedef enum DispenseStatus {
    DISPENSE_STATUS_PENDING = 0,
    DISPENSE_STATUS_COMPLETED = 1
} DispenseStatus;

typedef struct DispenseRecord {
    char dispense_id[HIS_DOMAIN_ID_CAPACITY];
    char patient_id[HIS_DOMAIN_ID_CAPACITY];
    char prescription_id[HIS_DOMAIN_ID_CAPACITY];
    char medicine_id[HIS_DOMAIN_ID_CAPACITY];
    int quantity;
    char pharmacist_id[HIS_DOMAIN_ID_CAPACITY];
    char dispensed_at[HIS_DOMAIN_TIME_CAPACITY];
    DispenseStatus status;
} DispenseRecord;

#endif
