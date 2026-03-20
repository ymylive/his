#ifndef HIS_SERVICE_BED_SERVICE_H
#define HIS_SERVICE_BED_SERVICE_H

#include <stddef.h>

#include "common/LinkedList.h"
#include "common/Result.h"
#include "domain/Bed.h"
#include "domain/Ward.h"
#include "repository/AdmissionRepository.h"
#include "repository/BedRepository.h"
#include "repository/WardRepository.h"

typedef struct BedService {
    WardRepository ward_repository;
    BedRepository bed_repository;
    AdmissionRepository admission_repository;
} BedService;

Result BedService_init(
    BedService *service,
    const char *ward_path,
    const char *bed_path,
    const char *admission_path
);
Result BedService_list_wards(BedService *service, LinkedList *out_wards);
Result BedService_list_beds_by_ward(
    BedService *service,
    const char *ward_id,
    LinkedList *out_beds
);
Result BedService_find_current_patient_by_bed_id(
    BedService *service,
    const char *bed_id,
    char *out_patient_id,
    size_t out_patient_id_capacity
);
void BedService_clear_wards(LinkedList *wards);
void BedService_clear_beds(LinkedList *beds);

#endif
