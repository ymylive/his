#ifndef HIS_REPOSITORY_PATIENT_REPOSITORY_H
#define HIS_REPOSITORY_PATIENT_REPOSITORY_H

#include "common/LinkedList.h"
#include "common/Result.h"
#include "domain/Patient.h"
#include "repository/TextFileRepository.h"

#define PATIENT_REPOSITORY_HEADER \
    "patient_id|name|gender|age|contact|id_card|allergy|medical_history|is_inpatient|remarks"
#define PATIENT_REPOSITORY_FIELD_COUNT 10

typedef struct PatientRepository {
    TextFileRepository file_repository;
} PatientRepository;

Result PatientRepository_init(PatientRepository *repository, const char *path);
Result PatientRepository_append(const PatientRepository *repository, const Patient *patient);
Result PatientRepository_find_by_id(
    const PatientRepository *repository,
    const char *patient_id,
    Patient *out_patient
);
Result PatientRepository_load_all(
    const PatientRepository *repository,
    LinkedList *out_patients
);
Result PatientRepository_save_all(
    const PatientRepository *repository,
    const LinkedList *patients
);
void PatientRepository_clear_loaded_list(LinkedList *patients);

#endif
