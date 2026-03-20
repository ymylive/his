#ifndef HIS_SERVICE_DOCTOR_SERVICE_H
#define HIS_SERVICE_DOCTOR_SERVICE_H

#include "repository/DepartmentRepository.h"
#include "repository/DoctorRepository.h"

typedef struct DoctorService {
    DoctorRepository doctor_repository;
    DepartmentRepository department_repository;
} DoctorService;

Result DoctorService_init(
    DoctorService *service,
    const char *doctor_path,
    const char *department_path
);
Result DoctorService_add(DoctorService *service, const Doctor *doctor);
Result DoctorService_get_by_id(
    DoctorService *service,
    const char *doctor_id,
    Doctor *out_doctor
);
Result DoctorService_list_by_department(
    DoctorService *service,
    const char *department_id,
    LinkedList *out_doctors
);
void DoctorService_clear_list(LinkedList *doctors);

#endif
