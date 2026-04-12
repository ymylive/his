/**
 * @file PrescriptionService.c
 * @brief 处方服务模块实现
 *
 * 实现处方管理业务功能，包括处方创建、按就诊ID查询、
 * 按处方ID查询以及全量加载。
 */

#include "service/PrescriptionService.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/StringUtils.h"
#include "repository/RepositoryUtils.h"

/**
 * @brief 校验必填文本字段
 *
 * 检查文本是否为空白以及是否包含保留字符。
 *
 * @param text        待校验的文本
 * @param field_name  字段名称（用于错误消息）
 * @return Result     校验结果
 */
static Result PrescriptionService_validate_required_text(const char *text, const char *field_name) {
    char message[RESULT_MESSAGE_CAPACITY];

    if (StringUtils_is_blank(text)) {
        snprintf(message, sizeof(message), "%s missing", field_name);
        return Result_make_failure(message);
    }

    if (!RepositoryUtils_is_safe_field_text(text)) {
        snprintf(message, sizeof(message), "%s contains reserved characters", field_name);
        return Result_make_failure(message);
    }

    return Result_make_success("prescription text valid");
}

/**
 * @brief 校验可选文本字段
 *
 * 仅检查是否包含保留字符，允许空字符串。
 *
 * @param text        待校验的文本
 * @param field_name  字段名称（用于错误消息）
 * @return Result     校验结果
 */
static Result PrescriptionService_validate_optional_text(const char *text, const char *field_name) {
    char message[RESULT_MESSAGE_CAPACITY];

    if (text == 0) {
        return Result_make_failure("optional text missing");
    }

    if (!RepositoryUtils_is_safe_field_text(text)) {
        snprintf(message, sizeof(message), "%s contains reserved characters", field_name);
        return Result_make_failure(message);
    }

    return Result_make_success("prescription optional text valid");
}

/**
 * @brief 综合校验处方信息的合法性
 *
 * 依次校验处方ID、就诊ID、药品ID、数量和用法。
 *
 * @param prescription 指向待校验的处方结构体
 * @return Result      校验结果
 */
static Result PrescriptionService_validate_prescription(const Prescription *prescription) {
    Result result;

    if (prescription == 0) {
        return Result_make_failure("prescription missing");
    }

    result = PrescriptionService_validate_required_text(prescription->prescription_id, "prescription id");
    if (result.success == 0) {
        return result;
    }

    result = PrescriptionService_validate_required_text(prescription->visit_id, "visit id");
    if (result.success == 0) {
        return result;
    }

    result = PrescriptionService_validate_required_text(prescription->medicine_id, "medicine id");
    if (result.success == 0) {
        return result;
    }

    if (prescription->quantity <= 0) {
        return Result_make_failure("prescription quantity invalid");
    }

    result = PrescriptionService_validate_optional_text(prescription->usage, "usage");
    if (result.success == 0) {
        return result;
    }

    return Result_make_success("prescription valid");
}

/**
 * @brief 初始化处方服务
 *
 * @param service 指向待初始化的处方服务结构体
 * @param path    处方数据文件路径
 * @return Result 操作结果
 */
Result PrescriptionService_init(PrescriptionService *service, const char *path) {
    Result result;

    if (service == 0) {
        return Result_make_failure("prescription service missing");
    }

    result = PrescriptionRepository_init(&service->prescription_repository, path);
    if (result.success == 0) {
        return result;
    }

    return PrescriptionRepository_ensure_storage(&service->prescription_repository);
}

/**
 * @brief 创建处方
 *
 * 流程：校验处方信息 -> 检查处方ID唯一性 -> 保存处方。
 *
 * @param service      指向处方服务结构体
 * @param prescription 指向待创建的处方信息
 * @return Result      操作结果
 */
Result PrescriptionService_create_prescription(
    PrescriptionService *service,
    const Prescription *prescription
) {
    Prescription existing;
    Result result;

    if (service == 0) {
        return Result_make_failure("prescription service missing");
    }

    result = PrescriptionService_validate_prescription(prescription);
    if (result.success == 0) {
        return result;
    }

    /* 检查处方ID是否已存在 */
    result = PrescriptionRepository_find_by_id(
        &service->prescription_repository,
        prescription->prescription_id,
        &existing
    );
    if (result.success != 0) {
        return Result_make_failure("prescription id already exists");
    }

    /* ID唯一性校验通过，追加处方 */
    return PrescriptionRepository_append(&service->prescription_repository, prescription);
}

/**
 * @brief 按就诊ID查找处方列表
 *
 * @param service    指向处方服务结构体
 * @param visit_id   就诊ID
 * @param out_list   输出参数，处方链表（必须为空链表）
 * @return Result    操作结果
 */
Result PrescriptionService_find_by_visit_id(
    PrescriptionService *service,
    const char *visit_id,
    LinkedList *out_list
) {
    Result result = PrescriptionService_validate_required_text(visit_id, "visit id");

    if (result.success == 0) {
        return result;
    }

    if (service == 0 || out_list == 0) {
        return Result_make_failure("prescription query arguments missing");
    }

    if (LinkedList_count(out_list) != 0) {
        return Result_make_failure("prescription output must be empty");
    }

    return PrescriptionRepository_find_by_visit_id(
        &service->prescription_repository,
        visit_id,
        out_list
    );
}

/**
 * @brief 按处方ID查找单条处方
 *
 * @param service         指向处方服务结构体
 * @param prescription_id 处方ID
 * @param out             输出参数，存放找到的处方记录
 * @return Result         操作结果
 */
Result PrescriptionService_find_by_id(
    PrescriptionService *service,
    const char *prescription_id,
    Prescription *out
) {
    Result result = PrescriptionService_validate_required_text(prescription_id, "prescription id");

    if (result.success == 0) {
        return result;
    }

    if (service == 0 || out == 0) {
        return Result_make_failure("prescription query arguments missing");
    }

    return PrescriptionRepository_find_by_id(
        &service->prescription_repository,
        prescription_id,
        out
    );
}

/**
 * @brief 加载所有处方记录
 *
 * @param service  指向处方服务结构体
 * @param out_list 输出参数，处方链表（必须为空链表）
 * @return Result  操作结果
 */
Result PrescriptionService_load_all(
    PrescriptionService *service,
    LinkedList *out_list
) {
    if (service == 0 || out_list == 0) {
        return Result_make_failure("prescription load arguments missing");
    }

    return PrescriptionRepository_load_all(
        &service->prescription_repository,
        out_list
    );
}

/**
 * @brief 清理处方查询结果链表
 *
 * @param list 指向待清理的处方链表
 */
void PrescriptionService_clear_results(LinkedList *list) {
    PrescriptionRepository_clear_list(list);
}
