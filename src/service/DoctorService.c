/**
 * @file DoctorService.c
 * @brief 医生服务模块实现
 *
 * 实现医生信息的添加和查询功能。在添加医生时会校验医生信息的完整性，
 * 并验证所属科室是否存在，确保医生ID不重复。
 */

#include "service/DoctorService.h"

#include <ctype.h>
#include <string.h>

#include "common/StringUtils.h"


/**
 * @brief 校验医生信息的完整性
 *
 * 检查医生ID、姓名、职称、所属科室ID、排班信息是否非空，
 * 以及医生状态是否为有效的枚举值。
 *
 * @param doctor  指向待校验的医生结构体
 * @return Result 校验结果
 */
static Result DoctorService_validate(const Doctor *doctor) {
    if (doctor == 0) {
        return Result_make_failure("doctor missing");
    }

    if (!StringUtils_has_text(doctor->doctor_id)) {
        return Result_make_failure("doctor id missing");
    }

    if (!StringUtils_has_text(doctor->name)) {
        return Result_make_failure("doctor name missing");
    }

    if (!StringUtils_has_text(doctor->title)) {
        return Result_make_failure("doctor title missing");
    }

    if (!StringUtils_has_text(doctor->department_id)) {
        return Result_make_failure("doctor department missing");
    }

    if (!StringUtils_has_text(doctor->schedule)) {
        return Result_make_failure("doctor schedule missing");
    }

    /* 状态只允许 INACTIVE 或 ACTIVE */
    if (doctor->status != DOCTOR_STATUS_INACTIVE &&
        doctor->status != DOCTOR_STATUS_ACTIVE) {
        return Result_make_failure("doctor status invalid");
    }

    return Result_make_success("doctor valid");
}

/**
 * @brief 在医生链表中按ID查找医生
 *
 * @param doctors    医生链表
 * @param doctor_id  待查找的医生ID
 * @return Doctor*   找到时返回指向该医生的指针，未找到返回 NULL
 */
static Doctor *DoctorService_find_in_list(
    LinkedList *doctors,
    const char *doctor_id
) {
    LinkedListNode *current = 0;

    if (doctors == 0 || doctor_id == 0) {
        return 0;
    }

    current = doctors->head;
    while (current != 0) {
        Doctor *doctor = (Doctor *)current->data;

        if (doctor != 0 && strcmp(doctor->doctor_id, doctor_id) == 0) {
            return doctor;
        }

        current = current->next;
    }

    return 0;
}

/**
 * @brief 验证科室是否存在
 *
 * 通过科室仓库查找指定科室ID，确认科室记录存在。
 *
 * @param service        指向医生服务结构体
 * @param department_id  科室ID
 * @return Result        验证结果
 */
static Result DoctorService_ensure_department_exists(
    DoctorService *service,
    const char *department_id
) {
    Department department;

    if (service == 0) {
        return Result_make_failure("doctor service missing");
    }

    if (!StringUtils_has_text(department_id)) {
        return Result_make_failure("doctor department missing");
    }

    /* 从科室仓库中查找，验证科室是否存在 */
    if (DepartmentRepository_find_by_department_id(
            &service->department_repository,
            department_id,
            &department
        ).success == 0) {
        return Result_make_failure("doctor department not found");
    }

    return Result_make_success("doctor department exists");
}

/**
 * @brief 初始化医生服务
 *
 * 分别初始化医生仓库和科室仓库。
 *
 * @param service          指向待初始化的医生服务结构体
 * @param doctor_path      医生数据文件路径
 * @param department_path  科室数据文件路径
 * @return Result          操作结果
 */
Result DoctorService_init(
    DoctorService *service,
    const char *doctor_path,
    const char *department_path
) {
    Result result;

    if (service == 0) {
        return Result_make_failure("doctor service missing");
    }

    /* 先初始化医生仓库 */
    result = DoctorRepository_init(&service->doctor_repository, doctor_path);
    if (result.success == 0) {
        return result;
    }

    /* 再初始化科室仓库 */
    return DepartmentRepository_init(
        &service->department_repository,
        department_path
    );
}

/**
 * @brief 添加新医生
 *
 * 流程：校验医生信息 -> 验证科室存在 -> 加载已有医生列表 ->
 * 检查医生ID唯一性 -> 保存新医生记录。
 *
 * @param service  指向医生服务结构体
 * @param doctor   指向待添加的医生信息
 * @return Result  操作结果
 */
Result DoctorService_add(DoctorService *service, const Doctor *doctor) {
    Result result = DoctorService_validate(doctor);

    if (result.success == 0) {
        return result;
    }

    if (service == 0) {
        return Result_make_failure("doctor service missing");
    }

    /* 验证所属科室是否存在 */
    result = DoctorService_ensure_department_exists(
        service,
        doctor->department_id
    );
    if (result.success == 0) {
        return result;
    }

    /* 使用仓库层按ID直接查找，避免加载全表 */
    {
        Doctor found;
        result = DoctorRepository_find_by_doctor_id(
            &service->doctor_repository,
            doctor->doctor_id,
            &found
        );
        if (result.success != 0) {
            return Result_make_failure("doctor already exists");
        }
    }

    /* ID唯一性校验通过，保存新医生记录 */
    return DoctorRepository_save(&service->doctor_repository, doctor);
}

/**
 * @brief 根据医生ID查找医生
 *
 * @param service     指向医生服务结构体
 * @param doctor_id   待查找的医生ID
 * @param out_doctor  输出参数，查找成功时存放医生信息
 * @return Result     操作结果
 */
Result DoctorService_get_by_id(
    DoctorService *service,
    const char *doctor_id,
    Doctor *out_doctor
) {
    if (service == 0) {
        return Result_make_failure("doctor service missing");
    }

    if (!StringUtils_has_text(doctor_id) || out_doctor == 0) {
        return Result_make_failure("doctor query arguments missing");
    }

    /* 委托仓库层按ID查找 */
    return DoctorRepository_find_by_doctor_id(
        &service->doctor_repository,
        doctor_id,
        out_doctor
    );
}

/**
 * @brief 根据科室ID列出该科室下的所有医生
 *
 * 先验证科室存在，再从仓库中按科室ID筛选医生。
 *
 * @param service        指向医生服务结构体
 * @param department_id  科室ID
 * @param out_doctors    输出参数，查找结果链表
 * @return Result        操作结果
 */
Result DoctorService_list_by_department(
    DoctorService *service,
    const char *department_id,
    LinkedList *out_doctors
) {
    Result result;

    if (service == 0) {
        return Result_make_failure("doctor service missing");
    }

    if (out_doctors == 0) {
        return Result_make_failure("doctor output list missing");
    }

    /* 先验证科室是否存在 */
    result = DoctorService_ensure_department_exists(service, department_id);
    if (result.success == 0) {
        return result;
    }

    /* 从仓库中按科室ID筛选 */
    return DoctorRepository_find_by_department_id(
        &service->doctor_repository,
        department_id,
        out_doctors
    );
}

/**
 * @brief 清理医生列表
 *
 * 释放链表中所有动态分配的 Doctor 节点。
 *
 * @param doctors  指向待清理的医生链表
 */
void DoctorService_clear_list(LinkedList *doctors) {
    DoctorRepository_clear_list(doctors);
}
