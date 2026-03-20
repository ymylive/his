#ifndef HIS_SERVICE_PATIENT_SERVICE_H
#define HIS_SERVICE_PATIENT_SERVICE_H

#include "common/LinkedList.h"
#include "common/Result.h"
#include "domain/Patient.h"
#include "repository/PatientRepository.h"

typedef struct PatientService {
    PatientRepository repository;
} PatientService;

Result PatientService_init(PatientService *service, const char *path);
Result PatientService_create_patient(PatientService *service, const Patient *patient);
Result PatientService_update_patient(PatientService *service, const Patient *patient);
Result PatientService_delete_patient(PatientService *service, const char *patient_id);
Result PatientService_find_patient_by_id(
    PatientService *service,
    const char *patient_id,
    Patient *out_patient
);
Result PatientService_find_patients_by_name(
    PatientService *service,
    const char *name,
    LinkedList *out_patients
);
Result PatientService_find_patient_by_contact(
    PatientService *service,
    const char *contact,
    Patient *out_patient
);
Result PatientService_find_patient_by_id_card(
    PatientService *service,
    const char *id_card,
    Patient *out_patient
);
void PatientService_clear_search_results(LinkedList *patients);

#endif
