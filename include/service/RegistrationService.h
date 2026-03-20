#ifndef HIS_SERVICE_REGISTRATION_SERVICE_H
#define HIS_SERVICE_REGISTRATION_SERVICE_H

#include "common/LinkedList.h"
#include "common/Result.h"
#include "domain/Registration.h"
#include "repository/DepartmentRepository.h"
#include "repository/DoctorRepository.h"
#include "repository/PatientRepository.h"
#include "repository/RegistrationRepository.h"

typedef struct RegistrationService {
    RegistrationRepository *registration_repository;
    PatientRepository *patient_repository;
    DoctorRepository *doctor_repository;
    DepartmentRepository *department_repository;
} RegistrationService;

Result RegistrationService_init(
    RegistrationService *service,
    RegistrationRepository *registration_repository,
    PatientRepository *patient_repository,
    DoctorRepository *doctor_repository,
    DepartmentRepository *department_repository
);
Result RegistrationService_create(
    RegistrationService *service,
    const char *patient_id,
    const char *doctor_id,
    const char *department_id,
    const char *registered_at,
    Registration *out_registration
);
Result RegistrationService_find_by_registration_id(
    RegistrationService *service,
    const char *registration_id,
    Registration *out_registration
);
Result RegistrationService_cancel(
    RegistrationService *service,
    const char *registration_id,
    const char *cancelled_at,
    Registration *out_registration
);
Result RegistrationService_find_by_patient_id(
    RegistrationService *service,
    const char *patient_id,
    const RegistrationRepositoryFilter *filter,
    LinkedList *out_registrations
);
Result RegistrationService_find_by_doctor_id(
    RegistrationService *service,
    const char *doctor_id,
    const RegistrationRepositoryFilter *filter,
    LinkedList *out_registrations
);

#endif
