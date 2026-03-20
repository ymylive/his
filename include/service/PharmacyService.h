#ifndef HIS_SERVICE_PHARMACY_SERVICE_H
#define HIS_SERVICE_PHARMACY_SERVICE_H

#include "common/LinkedList.h"
#include "common/Result.h"
#include "domain/DispenseRecord.h"
#include "domain/Medicine.h"
#include "repository/DispenseRecordRepository.h"
#include "repository/MedicineRepository.h"

typedef struct PharmacyService {
    MedicineRepository medicine_repository;
    DispenseRecordRepository dispense_record_repository;
} PharmacyService;

Result PharmacyService_init(
    PharmacyService *service,
    const char *medicine_path,
    const char *dispense_record_path
);
Result PharmacyService_add_medicine(
    PharmacyService *service,
    const Medicine *medicine
);
Result PharmacyService_restock_medicine(
    PharmacyService *service,
    const char *medicine_id,
    int quantity
);
Result PharmacyService_dispense_medicine(
    PharmacyService *service,
    const char *prescription_id,
    const char *medicine_id,
    int quantity,
    const char *pharmacist_id,
    const char *dispensed_at,
    DispenseRecord *out_record
);
Result PharmacyService_get_stock(
    PharmacyService *service,
    const char *medicine_id,
    int *out_stock
);
Result PharmacyService_find_low_stock_medicines(
    PharmacyService *service,
    LinkedList *out_medicines
);
void PharmacyService_clear_medicine_results(LinkedList *medicines);

#endif
