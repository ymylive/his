#ifndef HIS_SERVICE_DEPARTMENT_SERVICE_H
#define HIS_SERVICE_DEPARTMENT_SERVICE_H

#include "repository/DepartmentRepository.h"

typedef struct DepartmentService {
    DepartmentRepository repository;
} DepartmentService;

Result DepartmentService_init(DepartmentService *service, const char *path);
Result DepartmentService_add(
    DepartmentService *service,
    const Department *department
);
Result DepartmentService_update(
    DepartmentService *service,
    const Department *department
);
Result DepartmentService_get_by_id(
    DepartmentService *service,
    const char *department_id,
    Department *out_department
);

#endif
