/**
 * @file RoundRecordService.c
 * @brief 查房记录服务模块实现
 *
 * 实现查房记录的业务操作功能，包括创建查房记录（含自动ID生成）、
 * 按住院记录查询以及加载全部查房记录。
 */

#include "service/RoundRecordService.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/IdGenerator.h"
#include "common/StringUtils.h"
#include "repository/RepositoryUtils.h"

/**
 * @brief 查房记录计数上下文结构体
 *
 * 用于在逐行遍历回调中计数查房记录行数，以生成下一个查房记录ID。
 */
typedef struct RoundRecordServiceCountContext {
    int count; /**< 当前已有的查房记录数量 */
} RoundRecordServiceCountContext;

/**
 * @brief 校验必填文本字段
 *
 * @param text       待校验的文本
 * @param field_name 字段名称（用于错误消息）
 * @return Result    校验结果
 */
static Result RoundRecordService_validate_text(const char *text, const char *field_name) {
    char message[RESULT_MESSAGE_CAPACITY];

    if (StringUtils_is_blank(text)) {
        snprintf(message, sizeof(message), "%s missing", field_name);
        return Result_make_failure(message);
    }

    if (!RepositoryUtils_is_safe_field_text(text)) {
        snprintf(message, sizeof(message), "%s contains reserved characters", field_name);
        return Result_make_failure(message);
    }

    return Result_make_success("round text valid");
}

/**
 * @brief 逐行计数回调：统计查房记录行数
 *
 * @param line    数据行文本（未使用）
 * @param context 计数上下文指针
 * @return Result 操作结果
 */
static Result RoundRecordService_count_line(const char *line, void *context) {
    RoundRecordServiceCountContext *count_context =
        (RoundRecordServiceCountContext *)context;

    (void)line;
    if (count_context == 0) {
        return Result_make_failure("round count context missing");
    }

    count_context->count++;
    return Result_make_success("round counted");
}

/**
 * @brief 生成下一个查房记录ID
 *
 * 通过统计已有记录行数确定序列号，生成格式为 "RND000001" 的查房记录ID。
 *
 * @param service     指向服务结构体
 * @param buffer      输出缓冲区
 * @param buffer_size 缓冲区容量
 * @return Result     操作结果
 */
static Result RoundRecordService_generate_id(
    RoundRecordService *service,
    char *buffer,
    size_t buffer_size
) {
    RoundRecordServiceCountContext context;
    Result result;

    if (service == 0 || buffer == 0 || buffer_size == 0) {
        return Result_make_failure("round id arguments missing");
    }

    context.count = 0;
    result = TextFileRepository_for_each_data_line(
        &service->repository.storage,
        RoundRecordService_count_line,
        &context
    );
    if (result.success == 0) {
        return result;
    }

    if (!IdGenerator_format(buffer, buffer_size, "RND", context.count + 1, 6)) {
        return Result_make_failure("failed to generate round id");
    }

    return Result_make_success("round id generated");
}

/**
 * @brief 初始化查房记录服务
 */
Result RoundRecordService_init(RoundRecordService *service, const char *path) {
    if (service == 0) {
        return Result_make_failure("round record service missing");
    }

    return RoundRecordRepository_init(&service->repository, path);
}

/**
 * @brief 创建查房记录
 *
 * 流程：校验必填字段 -> 生成查房记录ID -> 追加到存储。
 */
Result RoundRecordService_create_record(
    RoundRecordService *service,
    RoundRecord *record
) {
    Result result;

    if (service == 0 || record == 0) {
        return Result_make_failure("create round record arguments missing");
    }

    /* 校验必填字段 */
    result = RoundRecordService_validate_text(record->admission_id, "admission id");
    if (result.success == 0) {
        return result;
    }

    result = RoundRecordService_validate_text(record->doctor_id, "doctor id");
    if (result.success == 0) {
        return result;
    }

    result = RoundRecordService_validate_text(record->findings, "findings");
    if (result.success == 0) {
        return result;
    }

    result = RoundRecordService_validate_text(record->plan, "plan");
    if (result.success == 0) {
        return result;
    }

    result = RoundRecordService_validate_text(record->rounded_at, "rounded at");
    if (result.success == 0) {
        return result;
    }

    /* 生成查房记录ID */
    result = RoundRecordService_generate_id(
        service,
        record->round_id,
        sizeof(record->round_id)
    );
    if (result.success == 0) {
        return result;
    }

    /* 追加到存储 */
    return RoundRecordRepository_append(&service->repository, record);
}

/**
 * @brief 按住院记录ID查找所有关联的查房记录
 */
Result RoundRecordService_find_by_admission_id(
    RoundRecordService *service,
    const char *admission_id,
    LinkedList *out_list
) {
    if (service == 0 || out_list == 0) {
        return Result_make_failure("find round records arguments missing");
    }

    LinkedList_init(out_list);
    return RoundRecordRepository_find_by_admission_id(
        &service->repository,
        admission_id,
        out_list
    );
}

/**
 * @brief 加载所有查房记录
 */
Result RoundRecordService_load_all(
    RoundRecordService *service,
    LinkedList *out_list
) {
    if (service == 0 || out_list == 0) {
        return Result_make_failure("load round records arguments missing");
    }

    LinkedList_init(out_list);
    return RoundRecordRepository_load_all(&service->repository, out_list);
}

/**
 * @brief 清理查房记录查询结果链表
 */
void RoundRecordService_clear_results(LinkedList *list) {
    RoundRecordRepository_clear_list(list);
}
