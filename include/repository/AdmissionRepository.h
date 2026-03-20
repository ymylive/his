#ifndef HIS_REPOSITORY_ADMISSION_REPOSITORY_H
#define HIS_REPOSITORY_ADMISSION_REPOSITORY_H

#include "common/LinkedList.h"
#include "common/Result.h"
#include "domain/Admission.h"
#include "repository/TextFileRepository.h"

#define ADMISSION_REPOSITORY_HEADER \
    "admission_id|patient_id|ward_id|bed_id|admitted_at|status|discharged_at|summary"
#define ADMISSION_REPOSITORY_FIELD_COUNT 8

typedef struct AdmissionRepository {
    TextFileRepository storage;
} AdmissionRepository;

Result AdmissionRepository_init(AdmissionRepository *repository, const char *path);
Result AdmissionRepository_append(
    const AdmissionRepository *repository,
    const Admission *admission
);
Result AdmissionRepository_find_by_id(
    const AdmissionRepository *repository,
    const char *admission_id,
    Admission *out_admission
);
Result AdmissionRepository_find_active_by_patient_id(
    const AdmissionRepository *repository,
    const char *patient_id,
    Admission *out_admission
);
Result AdmissionRepository_find_active_by_bed_id(
    const AdmissionRepository *repository,
    const char *bed_id,
    Admission *out_admission
);
Result AdmissionRepository_load_all(
    const AdmissionRepository *repository,
    LinkedList *out_admissions
);
Result AdmissionRepository_save_all(
    const AdmissionRepository *repository,
    const LinkedList *admissions
);
void AdmissionRepository_clear_loaded_list(LinkedList *admissions);

#endif
