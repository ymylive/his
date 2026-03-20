#ifndef HIS_REPOSITORY_DEPARTMENT_REPOSITORY_H
#define HIS_REPOSITORY_DEPARTMENT_REPOSITORY_H

#include "common/LinkedList.h"
#include "common/Result.h"
#include "domain/Department.h"
#include "repository/TextFileRepository.h"

typedef struct DepartmentRepository {
    TextFileRepository storage;
} DepartmentRepository;

Result DepartmentRepository_init(DepartmentRepository *repository, const char *path);
Result DepartmentRepository_load_all(
    DepartmentRepository *repository,
    LinkedList *out_departments
);
Result DepartmentRepository_find_by_department_id(
    DepartmentRepository *repository,
    const char *department_id,
    Department *out_department
);
Result DepartmentRepository_save(
    DepartmentRepository *repository,
    const Department *department
);
Result DepartmentRepository_save_all(
    DepartmentRepository *repository,
    const LinkedList *departments
);
void DepartmentRepository_clear_list(LinkedList *departments);

#endif
