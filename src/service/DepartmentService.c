/**
 * @file DepartmentService.c
 * @brief 科室服务模块实现
 *
 * 实现科室信息的添加、更新和查询功能。包含科室信息校验、
 * 科室ID唯一性检查以及科室数据的持久化操作。
 */

#include "service/DepartmentService.h"

#include <ctype.h>
#include <string.h>

/**
 * @brief 判断文本是否非空（至少包含一个非空白字符）
 *
 * @param text  待检查的字符串
 * @return int  1=非空，0=空白或NULL
 */
static int DepartmentService_has_non_empty_text(const char *text) {
    const unsigned char *current = (const unsigned char *)text;

    if (current == 0) {
        return 0;
    }

    /* 逐字符检查，找到任意非空白字符即返回1 */
    while (*current != '\0') {
        if (isspace(*current) == 0) {
            return 1;
        }

        current++;
    }

    return 0;
}

/**
 * @brief 校验科室信息的完整性
 *
 * 检查科室ID和科室名称是否非空。
 *
 * @param department  指向待校验的科室结构体
 * @return Result     校验结果
 */
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

/**
 * @brief 在科室链表中按ID查找科室
 *
 * @param departments    科室链表
 * @param department_id  待查找的科室ID
 * @return Department*   找到时返回指向该科室的指针，未找到返回 NULL
 */
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

/**
 * @brief 初始化科室服务
 *
 * @param service  指向待初始化的科室服务结构体
 * @param path     科室数据文件路径
 * @return Result  操作结果
 */
Result DepartmentService_init(DepartmentService *service, const char *path) {
    if (service == 0) {
        return Result_make_failure("department service missing");
    }

    return DepartmentRepository_init(&service->repository, path);
}

/**
 * @brief 添加新科室
 *
 * 流程：校验科室信息 -> 加载已有科室列表 -> 检查科室ID唯一性 -> 保存新科室记录。
 *
 * @param service     指向科室服务结构体
 * @param department  指向待添加的科室信息
 * @return Result     操作结果
 */
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

    /* 加载已有科室列表 */
    result = DepartmentRepository_load_all(&service->repository, &departments);
    if (result.success == 0) {
        return result;
    }

    /* 检查科室ID是否已存在 */
    existing = DepartmentService_find_in_list(
        &departments,
        department->department_id
    );
    DepartmentRepository_clear_list(&departments);
    if (existing != 0) {
        return Result_make_failure("department already exists");
    }

    /* ID唯一性校验通过，保存新科室记录 */
    return DepartmentRepository_save(&service->repository, department);
}

/**
 * @brief 更新科室信息
 *
 * 流程：校验科室信息 -> 加载已有科室列表 -> 查找目标记录 ->
 * 覆盖更新 -> 保存全量数据。
 *
 * @param service     指向科室服务结构体
 * @param department  指向包含更新后信息的科室结构体
 * @return Result     操作结果
 */
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

    /* 加载已有科室列表 */
    result = DepartmentRepository_load_all(&service->repository, &departments);
    if (result.success == 0) {
        return result;
    }

    /* 查找待更新的科室记录 */
    existing = DepartmentService_find_in_list(
        &departments,
        department->department_id
    );
    if (existing == 0) {
        DepartmentRepository_clear_list(&departments);
        return Result_make_failure("department not found");
    }

    /* 用新数据覆盖旧记录，然后保存整个链表 */
    *existing = *department;
    result = DepartmentRepository_save_all(&service->repository, &departments);
    DepartmentRepository_clear_list(&departments);
    return result;
}

/**
 * @brief 根据科室ID查找科室
 *
 * @param service         指向科室服务结构体
 * @param department_id   待查找的科室ID
 * @param out_department  输出参数，查找成功时存放科室信息
 * @return Result         操作结果
 */
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

    /* 委托仓库层按ID查找 */
    return DepartmentRepository_find_by_department_id(
        &service->repository,
        department_id,
        out_department
    );
}
