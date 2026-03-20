#ifndef HIS_SERVICE_INPATIENT_SERVICE_H
#define HIS_SERVICE_INPATIENT_SERVICE_H

#include "common/Result.h"
#include "domain/Admission.h"
#include "repository/AdmissionRepository.h"
#include "repository/BedRepository.h"
#include "repository/PatientRepository.h"
#include "repository/WardRepository.h"

typedef struct InpatientService {
    PatientRepository patient_repository;
    WardRepository ward_repository;
    BedRepository bed_repository;
    AdmissionRepository admission_repository;
} InpatientService;

Result InpatientService_init(
    InpatientService *service,
    const char *patient_path,
    const char *ward_path,
    const char *bed_path,
    const char *admission_path
);
Result InpatientService_admit_patient(
    InpatientService *service,
    const char *patient_id,
    const char *ward_id,
    const char *bed_id,
    const char *admitted_at,
    const char *summary,
    Admission *out_admission
);
Result InpatientService_discharge_patient(
    InpatientService *service,
    const char *admission_id,
    const char *discharged_at,
    const char *summary,
    Admission *out_admission
);
Result InpatientService_transfer_bed(
    InpatientService *service,
    const char *admission_id,
    const char *target_bed_id,
    const char *transferred_at,
    Admission *out_admission
);
Result InpatientService_find_by_id(
    InpatientService *service,
    const char *admission_id,
    Admission *out_admission
);
Result InpatientService_find_active_by_patient_id(
    InpatientService *service,
    const char *patient_id,
    Admission *out_admission
);

#endif
