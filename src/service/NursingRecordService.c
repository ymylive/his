/**
 * @file NursingRecordService.c
 * @brief 护理记录服务模块实现
 *
 * 实现护理记录的业务操作功能，包括创建护理记录（含自动ID生成）、
 * 按住院记录查询、加载全部护理记录以及清理查询结果。
 */

#include "service/NursingRecordService.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/IdGenerator.h"
#include "common/StringUtils.h"
#include "repository/RepositoryUtils.h"

/**
 * @brief 护理记录计数上下文结构体
 *
 * 用于在逐行遍历回调中计数护理记录行数，以生成下一个护理记录ID。
 */
typedef struct NursingRecordServiceCountContext {
    int count; /**< 当前已有的护理记录数量 */
} NursingRecordServiceCountContext;

/**
 * @brief 校验必填文本字段
 *
 * @param text       待校验的文本
 * @param field_name 字段名称（用于错误消息）
 * @return Result    校验结果
 */
static Result NursingRecordService_validate_text(const char *text, const char *field_name) {
    char message[RESULT_MESSAGE_CAPACITY];

    if (StringUtils_is_blank(text)) {
        snprintf(message, sizeof(message), "%s missing", field_name);
        return Result_make_failure(message);
    }

    if (!RepositoryUtils_is_safe_field_text(text)) {
        snprintf(message, sizeof(message), "%s contains reserved characters", field_name);
        return Result_make_failure(message);
    }

    return Result_make_success("nursing text valid");
}

/**
 * @brief 逐行计数回调：统计护理记录行数
 *
 * @param line    数据行文本（未使用）
 * @param context 计数上下文指针
 * @return Result 操作结果
 */
static Result NursingRecordService_count_line(const char *line, void *context) {
    NursingRecordServiceCountContext *count_context =
        (NursingRecordServiceCountContext *)context;

    (void)line;
    if (count_context == 0) {
        return Result_make_failure("nursing count context missing");
    }

    count_context->count++;
    return Result_make_success("nursing counted");
}

/**
 * @brief 生成下一个护理记录ID
 *
 * 通过统计已有记录行数确定序列号，生成格式为 "NRS000001" 的护理记录ID。
 *
 * @param service     指向服务结构体
 * @param buffer      输出缓冲区
 * @param buffer_size 缓冲区容量
 * @return Result     操作结果
 */
static Result NursingRecordService_generate_id(
    NursingRecordService *service,
    char *buffer,
    size_t buffer_size
) {
    NursingRecordServiceCountContext context;
    Result result;

    if (service == 0 || buffer == 0 || buffer_size == 0) {
        return Result_make_failure("nursing id arguments missing");
    }

    result = NursingRecordRepository_ensure_storage(&service->repository);
    if (result.success == 0) {
        return result;
    }

    context.count = 0;
    result = TextFileRepository_for_each_data_line(
        &service->repository.storage,
        NursingRecordService_count_line,
        &context
    );
    if (result.success == 0) {
        return result;
    }

    if (!IdGenerator_format(buffer, buffer_size, "NRS", context.count + 1, 6)) {
        return Result_make_failure("failed to generate nursing id");
    }

    return Result_make_success("nursing id generated");
}

/**
 * @brief 初始化护理记录服务
 */
Result NursingRecordService_init(NursingRecordService *service, const char *path) {
    Result result;

    if (service == 0) {
        return Result_make_failure("nursing record service missing");
    }

    result = NursingRecordRepository_init(&service->repository, path);
    if (result.success == 0) {
        return result;
    }

    return NursingRecordRepository_ensure_storage(&service->repository);
}

/**
 * @brief 创建护理记录
 *
 * 自动生成ID，校验必填字段后追加到存储。
 */
Result NursingRecordService_create_record(
    NursingRecordService *service,
    NursingRecord *record
) {
    Result result;

    if (service == 0 || record == 0) {
        return Result_make_failure("nursing create arguments missing");
    }

    /* 校验必填字段 */
    result = NursingRecordService_validate_text(record->admission_id, "admission id");
    if (result.success == 0) return result;

    result = NursingRecordService_validate_text(record->nurse_name, "nurse name");
    if (result.success == 0) return result;

    result = NursingRecordService_validate_text(record->record_type, "record type");
    if (result.success == 0) return result;

    result = NursingRecordService_validate_text(record->content, "content");
    if (result.success == 0) return result;

    result = NursingRecordService_validate_text(record->recorded_at, "recorded at");
    if (result.success == 0) return result;

    /* 生成护理记录ID */
    result = NursingRecordService_generate_id(
        service,
        record->nursing_id,
        sizeof(record->nursing_id)
    );
    if (result.success == 0) {
        return result;
    }

    /* 追加到存储 */
    return NursingRecordRepository_append(&service->repository, record);
}

/**
 * @brief 按住院记录ID查找护理记录
 */
Result NursingRecordService_find_by_admission_id(
    NursingRecordService *service,
    const char *admission_id,
    LinkedList *out_list
) {
    if (service == 0 || out_list == 0) {
        return Result_make_failure("nursing query arguments missing");
    }

    return NursingRecordRepository_find_by_admission_id(
        &service->repository,
        admission_id,
        out_list
    );
}

/**
 * @brief 加载所有护理记录
 */
Result NursingRecordService_load_all(
    NursingRecordService *service,
    LinkedList *out_list
) {
    if (service == 0 || out_list == 0) {
        return Result_make_failure("nursing load arguments missing");
    }

    return NursingRecordRepository_load_all(&service->repository, out_list);
}

/**
 * @brief 清理护理记录查询结果链表
 */
void NursingRecordService_clear_results(LinkedList *list) {
    NursingRecordRepository_clear_list(list);
}
