#ifndef HIS_REPOSITORY_DOCTOR_REPOSITORY_H
#define HIS_REPOSITORY_DOCTOR_REPOSITORY_H

#include "common/LinkedList.h"
#include "common/Result.h"
#include "domain/Doctor.h"
#include "repository/TextFileRepository.h"

typedef struct DoctorRepository {
    TextFileRepository storage;
} DoctorRepository;

Result DoctorRepository_init(DoctorRepository *repository, const char *path);
Result DoctorRepository_load_all(
    DoctorRepository *repository,
    LinkedList *out_doctors
);
Result DoctorRepository_find_by_doctor_id(
    DoctorRepository *repository,
    const char *doctor_id,
    Doctor *out_doctor
);
Result DoctorRepository_find_by_department_id(
    DoctorRepository *repository,
    const char *department_id,
    LinkedList *out_doctors
);
Result DoctorRepository_save(
    DoctorRepository *repository,
    const Doctor *doctor
);
Result DoctorRepository_save_all(
    DoctorRepository *repository,
    const LinkedList *doctors
);
void DoctorRepository_clear_list(LinkedList *doctors);

#endif
