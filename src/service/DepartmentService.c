#include "service/DepartmentService.h"

#include <ctype.h>
#include <string.h>

static int DepartmentService_has_non_empty_text(const char *text) {
    const unsigned char *current = (const unsigned char *)text;

    if (current == 0) {
        return 0;
    }

    while (*current != '\0') {
        if (isspace(*current) == 0) {
            return 1;
        }

        current++;
    }

    return 0;
}

static Result DepartmentService_validate(const Department *department) {
    if (department == 0) {
        return Result_make_failure("department missing");
    }

    if (!DepartmentService_has_non_empty_text(department->department_id)) {
        return Result_make_failure("department id missing");
    }

    if (!DepartmentService_has_non_empty_text(department->name)) {
        return Result_make_failure("department name missing");
    }

    return Result_make_success("department valid");
}

static Department *DepartmentService_find_in_list(
    LinkedList *departments,
    const char *department_id
) {
    LinkedListNode *current = 0;

    if (departments == 0 || department_id == 0) {
        return 0;
    }

    current = departments->head;
    while (current != 0) {
        Department *department = (Department *)current->data;

        if (department != 0 &&
            strcmp(department->department_id, department_id) == 0) {
            return department;
        }

        current = current->next;
    }

    return 0;
}

Result DepartmentService_init(DepartmentService *service, const char *path) {
    if (service == 0) {
        return Result_make_failure("department service missing");
    }

    return DepartmentRepository_init(&service->repository, path);
}

Result DepartmentService_add(
    DepartmentService *service,
    const Department *department
) {
    LinkedList departments;
    Department *existing = 0;
    Result result = DepartmentService_validate(department);

    if (result.success == 0) {
        return result;
    }

    if (service == 0) {
        return Result_make_failure("department service missing");
    }

    result = DepartmentRepository_load_all(&service->repository, &departments);
    if (result.success == 0) {
        return result;
    }

    existing = DepartmentService_find_in_list(
        &departments,
        department->department_id
    );
    DepartmentRepository_clear_list(&departments);
    if (existing != 0) {
        return Result_make_failure("department already exists");
    }

    return DepartmentRepository_save(&service->repository, department);
}

Result DepartmentService_update(
    DepartmentService *service,
    const Department *department
) {
    LinkedList departments;
    Department *existing = 0;
    Result result = DepartmentService_validate(department);

    if (result.success == 0) {
        return result;
    }

    if (service == 0) {
        return Result_make_failure("department service missing");
    }

    result = DepartmentRepository_load_all(&service->repository, &departments);
    if (result.success == 0) {
        return result;
    }

    existing = DepartmentService_find_in_list(
        &departments,
        department->department_id
    );
    if (existing == 0) {
        DepartmentRepository_clear_list(&departments);
        return Result_make_failure("department not found");
    }

    *existing = *department;
    result = DepartmentRepository_save_all(&service->repository, &departments);
    DepartmentRepository_clear_list(&departments);
    return result;
}

Result DepartmentService_get_by_id(
    DepartmentService *service,
    const char *department_id,
    Department *out_department
) {
    if (service == 0) {
        return Result_make_failure("department service missing");
    }

    if (!DepartmentService_has_non_empty_text(department_id) ||
        out_department == 0) {
        return Result_make_failure("department query arguments missing");
    }

    return DepartmentRepository_find_by_department_id(
        &service->repository,
        department_id,
        out_department
    );
}
