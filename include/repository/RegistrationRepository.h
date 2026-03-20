#ifndef HIS_REPOSITORY_REGISTRATION_REPOSITORY_H
#define HIS_REPOSITORY_REGISTRATION_REPOSITORY_H

#include "common/LinkedList.h"
#include "common/Result.h"
#include "domain/Registration.h"
#include "repository/TextFileRepository.h"

#define REGISTRATION_REPOSITORY_FIELD_COUNT 8

typedef struct RegistrationRepository {
    TextFileRepository storage;
} RegistrationRepository;

typedef struct RegistrationRepositoryFilter {
    int use_status;
    RegistrationStatus status;
    const char *registered_at_from;
    const char *registered_at_to;
} RegistrationRepositoryFilter;

Result RegistrationRepository_init(RegistrationRepository *repository, const char *path);
Result RegistrationRepository_ensure_storage(const RegistrationRepository *repository);
void RegistrationRepositoryFilter_init(RegistrationRepositoryFilter *filter);
Result RegistrationRepository_append(
    const RegistrationRepository *repository,
    const Registration *registration
);
Result RegistrationRepository_find_by_registration_id(
    const RegistrationRepository *repository,
    const char *registration_id,
    Registration *out_registration
);
Result RegistrationRepository_find_by_patient_id(
    const RegistrationRepository *repository,
    const char *patient_id,
    const RegistrationRepositoryFilter *filter,
    LinkedList *out_registrations
);
Result RegistrationRepository_save_all(
    const RegistrationRepository *repository,
    const LinkedList *registrations
);
void RegistrationRepository_clear_list(LinkedList *registrations);

#endif
