/**
 * @file BedService.c
 * @brief 床位服务模块实现
 *
 * 实现病房和床位信息的查询功能，包括列出所有病房、按病房列出床位、
 * 以及查找某个床位当前住院患者。通过协调病房仓库、床位仓库和
 * 住院记录仓库完成跨实体的关联查询。
 */

#include "service/BedService.h"

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "common/StringUtils.h"


/**
 * @brief 校验必填文本字段
 *
 * @param text        待校验的文本
 * @param field_name  字段名称（用于错误消息）
 * @return Result     校验结果
 */
static Result BedService_validate_required_text(const char *text, const char *field_name) {
    char message[RESULT_MESSAGE_CAPACITY];

    if (StringUtils_is_blank(text)) {
        snprintf(message, sizeof(message), "%s missing", field_name);
        return Result_make_failure(message);
    }

    return Result_make_success("bed service text valid");
}

/**
 * @brief 将床位信息的副本追加到结果链表
 *
 * 分配新的 Bed 结构体并复制数据，追加到输出链表。
 *
 * @param out_beds  输出结果链表
 * @param bed       待复制的源床位信息
 * @return Result   操作结果
 */
static Result BedService_append_bed_copy(LinkedList *out_beds, const Bed *bed) {
    Bed *copy = 0;

    if (out_beds == 0 || bed == 0) {
        return Result_make_failure("bed copy arguments missing");
    }

    copy = (Bed *)malloc(sizeof(*copy));
    if (copy == 0) {
        return Result_make_failure("failed to allocate bed result");
    }

    *copy = *bed;
    if (!LinkedList_append(out_beds, copy)) {
        free(copy);
        return Result_make_failure("failed to append bed result");
    }

    return Result_make_success("bed result appended");
}

/**
 * @brief 初始化床位服务
 *
 * 分别初始化病房仓库、床位仓库和住院记录仓库。
 *
 * @param service         指向待初始化的床位服务结构体
 * @param ward_path       病房数据文件路径
 * @param bed_path        床位数据文件路径
 * @param admission_path  住院记录数据文件路径
 * @return Result         操作结果
 */
Result BedService_init(
    BedService *service,
    const char *ward_path,
    const char *bed_path,
    const char *admission_path
) {
    Result result;

    if (service == 0) {
        return Result_make_failure("bed service missing");
    }

    /* 依次初始化三个仓库 */
    result = WardRepository_init(&service->ward_repository, ward_path);
    if (result.success == 0) {
        return result;
    }

    result = BedRepository_init(&service->bed_repository, bed_path);
    if (result.success == 0) {
        return result;
    }

    return AdmissionRepository_init(&service->admission_repository, admission_path);
}

/**
 * @brief 列出所有病房
 *
 * @param service    指向床位服务结构体
 * @param out_wards  输出参数，病房列表链表
 * @return Result    操作结果
 */
Result BedService_list_wards(BedService *service, LinkedList *out_wards) {
    if (service == 0 || out_wards == 0) {
        return Result_make_failure("bed service arguments missing");
    }

    return WardRepository_load_all(&service->ward_repository, out_wards);
}

/**
 * @brief 按病房ID列出该病房下的所有床位
 *
 * 流程：校验病房ID -> 验证病房存在 -> 加载所有床位 -> 筛选属于该病房的床位。
 *
 * @param service   指向床位服务结构体
 * @param ward_id   病房ID
 * @param out_beds  输出参数，床位列表链表
 * @return Result   操作结果
 */
Result BedService_list_beds_by_ward(
    BedService *service,
    const char *ward_id,
    LinkedList *out_beds
) {
    LinkedList loaded_beds;
    const LinkedListNode *current = 0;
    Result result = BedService_validate_required_text(ward_id, "ward id");

    if (result.success == 0) {
        return result;
    }

    if (service == 0 || out_beds == 0) {
        return Result_make_failure("bed service arguments missing");
    }

    /* 验证病房是否存在 */
    result = WardRepository_find_by_id(&service->ward_repository, ward_id, &(Ward){0});
    if (result.success == 0) {
        return result;
    }

    /* 加载所有床位数据 */
    LinkedList_init(out_beds);
    result = BedRepository_load_all(&service->bed_repository, &loaded_beds);
    if (result.success == 0) {
        return result;
    }

    /* 遍历所有床位，筛选属于目标病房的床位 */
    current = loaded_beds.head;
    while (current != 0) {
        const Bed *bed = (const Bed *)current->data;

        if (strcmp(bed->ward_id, ward_id) == 0) {
            result = BedService_append_bed_copy(out_beds, bed);
            if (result.success == 0) {
                BedRepository_clear_loaded_list(&loaded_beds);
                BedService_clear_beds(out_beds);
                return result;
            }
        }

        current = current->next;
    }

    BedRepository_clear_loaded_list(&loaded_beds);
    return Result_make_success("beds loaded by ward");
}

/**
 * @brief 查找某个床位当前住院的患者ID
 *
 * 流程：校验床位ID -> 查找床位 -> 确认床位已被占用 ->
 * 通过住院记录获取当前患者ID -> 验证住院记录与床位记录的一致性。
 *
 * @param service                   指向床位服务结构体
 * @param bed_id                    床位ID
 * @param out_patient_id            输出参数，当前患者ID字符串缓冲区
 * @param out_patient_id_capacity   输出缓冲区容量
 * @return Result                   操作结果
 */
Result BedService_find_current_patient_by_bed_id(
    BedService *service,
    const char *bed_id,
    char *out_patient_id,
    size_t out_patient_id_capacity
) {
    Bed bed;
    Admission admission;
    Result result = BedService_validate_required_text(bed_id, "bed id");

    if (result.success == 0) {
        return result;
    }

    if (service == 0 || out_patient_id == 0 || out_patient_id_capacity == 0) {
        return Result_make_failure("bed service arguments missing");
    }

    /* 查找床位信息 */
    result = BedRepository_find_by_id(&service->bed_repository, bed_id, &bed);
    if (result.success == 0) {
        return result;
    }

    /* 检查床位是否被占用 */
    if (bed.status != BED_STATUS_OCCUPIED || bed.current_admission_id[0] == '\0') {
        return Result_make_failure("bed has no current patient");
    }

    /* 通过床位ID查找活跃的住院记录 */
    result = AdmissionRepository_find_active_by_bed_id(
        &service->admission_repository,
        bed_id,
        &admission
    );
    if (result.success == 0) {
        return result;
    }

    /* 验证住院记录ID与床位记录的关联住院ID一致 */
    if (strcmp(admission.admission_id, bed.current_admission_id) != 0) {
        return Result_make_failure("bed admission mismatch");
    }

    /* 输出患者ID */
    strncpy(out_patient_id, admission.patient_id, out_patient_id_capacity - 1);
    out_patient_id[out_patient_id_capacity - 1] = '\0';
    return Result_make_success("current patient found");
}

/**
 * @brief 清理病房列表
 *
 * 释放链表中所有动态分配的 Ward 节点。
 *
 * @param wards  指向待清理的病房链表
 */
void BedService_clear_wards(LinkedList *wards) {
    WardRepository_clear_loaded_list(wards);
}

/**
 * @brief 清理床位列表
 *
 * 释放链表中所有动态分配的 Bed 节点。
 *
 * @param beds  指向待清理的床位链表
 */
void BedService_clear_beds(LinkedList *beds) {
    LinkedList_clear(beds, free);
}
