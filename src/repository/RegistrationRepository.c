/**
 * @file RegistrationRepository.c
 * @brief 挂号记录数据仓储层实现
 *
 * 实现挂号记录（Registration）数据的持久化存储操作，包括：
 * - 挂号记录的序列化与反序列化
 * - 数据校验（必填字段、状态与时间戳一致性、保留字符检测）
 * - 追加、按ID查找、按患者ID筛选（支持过滤条件）、全量加载、全量保存
 */

#include "repository/RegistrationRepository.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "repository/RepositoryUtils.h"

/** 挂号记录数据文件的表头行 */
static const char REGISTRATION_REPOSITORY_HEADER[] =
    "registration_id|patient_id|doctor_id|department_id|registered_at|status|diagnosed_at|cancelled_at|registration_type|registration_fee";

/** 按挂号ID查找时使用的上下文结构体 */
typedef struct RegistrationFindByIdContext {
    const char *registration_id;    /**< 要查找的挂号ID */
    Registration *out_registration; /**< 输出：找到的挂号记录 */
    int found;                      /**< 是否已找到标志 */
} RegistrationFindByIdContext;

/** 按患者ID收集挂号记录时使用的上下文结构体 */
typedef struct RegistrationCollectByPatientContext {
    const char *patient_id;                  /**< 患者ID */
    const RegistrationRepositoryFilter *filter; /**< 过滤条件 */
    LinkedList *out_registrations;           /**< 输出：匹配的挂号记录链表 */
} RegistrationCollectByPatientContext;

/** 加载所有挂号记录时使用的上下文结构体 */
typedef struct RegistrationLoadContext {
    LinkedList *out_registrations; /**< 用于存放加载结果的链表 */
} RegistrationLoadContext;

/**
 * @brief 判断文本是否为空
 */
static int RegistrationRepository_is_empty_text(const char *text) {
    return text == 0 || text[0] == '\0';
}

/**
 * @brief 生成字段校验失败消息
 * @param field_name 字段名称
 * @param suffix     错误后缀描述
 * @return failure 结果
 */
static Result RegistrationRepository_make_field_failure(const char *field_name, const char *suffix) {
    char message[RESULT_MESSAGE_CAPACITY];

    snprintf(message, sizeof(message), "%s %s", field_name, suffix);
    return Result_make_failure(message);
}

/**
 * @brief 校验单个文本字段
 * @param text        字段值
 * @param field_name  字段名称（用于错误消息）
 * @param allow_empty 是否允许空值
 * @return 合法返回 success；不合法返回 failure
 */
static Result RegistrationRepository_validate_text_field(
    const char *text,
    const char *field_name,
    int allow_empty
) {
    if (text == 0) {
        return RegistrationRepository_make_field_failure(field_name, "missing");
    }

    if (!allow_empty && RegistrationRepository_is_empty_text(text)) {
        return RegistrationRepository_make_field_failure(field_name, "missing");
    }

    if (!RepositoryUtils_is_safe_field_text(text)) {
        return RegistrationRepository_make_field_failure(field_name, "contains reserved characters");
    }

    return Result_make_success("text field ok");
}

/**
 * @brief 解析挂号状态枚举值
 * @param field      文本字段
 * @param out_status 输出参数，解析得到的状态值
 * @return 成功返回 success；格式不合法时返回 failure
 */
static Result RegistrationRepository_parse_status(
    const char *field,
    RegistrationStatus *out_status
) {
    char *end = 0;
    long value = 0;

    if (field == 0 || out_status == 0 || field[0] == '\0') {
        return Result_make_failure("registration status missing");
    }

    value = strtol(field, &end, 10);
    if (end == 0 || *end != '\0') {
        return Result_make_failure("registration status invalid");
    }

    if (value < REG_STATUS_PENDING || value > REG_STATUS_CANCELLED) {
        return Result_make_failure("registration status out of range");
    }

    *out_status = (RegistrationStatus)value;
    return Result_make_success("registration status parsed");
}

/**
 * @brief 解析挂号类型枚举值
 * @param field    文本字段
 * @param out_type 输出参数，解析得到的挂号类型
 * @return 成功返回 success；格式不合法时返回 failure
 */
static Result RegistrationRepository_parse_type(
    const char *field,
    RegistrationType *out_type
) {
    char *end = 0;
    long value = 0;

    if (field == 0 || out_type == 0 || field[0] == '\0') {
        return Result_make_failure("registration type missing");
    }

    value = strtol(field, &end, 10);
    if (end == 0 || *end != '\0') {
        return Result_make_failure("registration type invalid");
    }

    if (value < REG_TYPE_STANDARD || value > REG_TYPE_EMERGENCY) {
        return Result_make_failure("registration type out of range");
    }

    *out_type = (RegistrationType)value;
    return Result_make_success("registration type parsed");
}

/**
 * @brief 解析挂号费
 * @param field   文本字段
 * @param out_fee 输出参数，解析得到的挂号费
 * @return 成功返回 success；格式不合法时返回 failure
 */
static Result RegistrationRepository_parse_fee(
    const char *field,
    double *out_fee
) {
    char *end = 0;
    double value = 0;

    if (field == 0 || out_fee == 0 || field[0] == '\0') {
        return Result_make_failure("registration fee missing");
    }

    value = strtod(field, &end);
    if (end == 0 || *end != '\0') {
        return Result_make_failure("registration fee invalid");
    }

    if (value < 0) {
        return Result_make_failure("registration fee must be >= 0");
    }

    *out_fee = value;
    return Result_make_success("registration fee parsed");
}

/**
 * @brief 校验挂号记录的合法性
 *
 * 除了基本字段校验外，还检查状态与时间戳的一致性：
 * - 待诊状态：不能有诊断完成时间和取消时间
 * - 已诊状态：必须有诊断完成时间，不能有取消时间
 * - 已取消状态：必须有取消时间，不能有诊断完成时间
 *
 * @param registration 待校验的挂号记录
 * @return 合法返回 success；不合法返回 failure
 */
static Result RegistrationRepository_validate(const Registration *registration) {
    Result result;

    if (registration == 0) {
        return Result_make_failure("registration missing");
    }

    /* 校验各必填/可选文本字段 */
    result = RegistrationRepository_validate_text_field(
        registration->registration_id, "registration_id", 0
    );
    if (!result.success) {
        return result;
    }

    result = RegistrationRepository_validate_text_field(
        registration->patient_id, "patient_id", 0
    );
    if (!result.success) {
        return result;
    }

    result = RegistrationRepository_validate_text_field(
        registration->doctor_id, "doctor_id", 0
    );
    if (!result.success) {
        return result;
    }

    result = RegistrationRepository_validate_text_field(
        registration->department_id, "department_id", 0
    );
    if (!result.success) {
        return result;
    }

    result = RegistrationRepository_validate_text_field(
        registration->registered_at, "registered_at", 0
    );
    if (!result.success) {
        return result;
    }

    result = RegistrationRepository_validate_text_field(
        registration->diagnosed_at, "diagnosed_at", 1 /* 可为空 */
    );
    if (!result.success) {
        return result;
    }

    result = RegistrationRepository_validate_text_field(
        registration->cancelled_at, "cancelled_at", 1 /* 可为空 */
    );
    if (!result.success) {
        return result;
    }

    /* 校验挂号类型 */
    if (registration->registration_type < REG_TYPE_STANDARD ||
        registration->registration_type > REG_TYPE_EMERGENCY) {
        return Result_make_failure("registration_type out of range");
    }

    /* 校验挂号费 */
    if (registration->registration_fee < 0) {
        return Result_make_failure("registration_fee must be >= 0");
    }

    /* 校验状态与时间戳的一致性 */
    if (registration->status == REG_STATUS_PENDING) {
        /* 待诊状态：不能有任何结束时间戳 */
        if (!RegistrationRepository_is_empty_text(registration->diagnosed_at) ||
            !RegistrationRepository_is_empty_text(registration->cancelled_at)) {
            return Result_make_failure("pending registration cannot have closed timestamps");
        }
    } else if (registration->status == REG_STATUS_DIAGNOSED) {
        /* 已诊状态：必须有诊断时间，不能有取消时间 */
        if (RegistrationRepository_is_empty_text(registration->diagnosed_at) ||
            !RegistrationRepository_is_empty_text(registration->cancelled_at)) {
            return Result_make_failure("diagnosed registration timestamps invalid");
        }
    } else if (registration->status == REG_STATUS_CANCELLED) {
        /* 已取消状态：必须有取消时间，不能有诊断时间 */
        if (RegistrationRepository_is_empty_text(registration->cancelled_at) ||
            !RegistrationRepository_is_empty_text(registration->diagnosed_at)) {
            return Result_make_failure("cancelled registration timestamps invalid");
        }
    } else {
        return Result_make_failure("registration status invalid");
    }

    return Result_make_success("registration valid");
}

/**
 * @brief 丢弃当前行剩余字符
 */
static void RegistrationRepository_discard_rest_of_line(FILE *file) {
    int character = 0;

    while ((character = fgetc(file)) != EOF) {
        if (character == '\n') {
            return;
        }
    }
}

/**
 * @brief 向文件写入挂号记录表头（覆盖写入）
 */
static Result RegistrationRepository_write_header(const RegistrationRepository *repository) {
    char content[TEXT_FILE_REPOSITORY_LINE_CAPACITY];
    int written = 0;

    if (repository == 0) {
        return Result_make_failure("registration repository missing");
    }

    written = snprintf(content, sizeof(content), "%s\n", REGISTRATION_REPOSITORY_HEADER);
    if (written < 0 || (size_t)written >= sizeof(content)) {
        return Result_make_failure("registration header too long");
    }

    return TextFileRepository_save_file(&repository->storage, content);
}

/**
 * @brief 将挂号记录序列化为管道符分隔的文本行
 * @param registration 挂号记录
 * @param line         输出缓冲区
 * @param capacity     缓冲区容量
 * @return 成功返回 success；数据无效或缓冲区不足时返回 failure
 */
static Result RegistrationRepository_serialize(
    const Registration *registration,
    char *line,
    size_t capacity
) {
    int written = 0;
    Result result = RegistrationRepository_validate(registration);

    if (!result.success) {
        return result;
    }

    if (line == 0 || capacity == 0) {
        return Result_make_failure("registration line buffer missing");
    }

    written = snprintf(
        line,
        capacity,
        "%s|%s|%s|%s|%s|%d|%s|%s|%d|%.2f",
        registration->registration_id,
        registration->patient_id,
        registration->doctor_id,
        registration->department_id,
        registration->registered_at,
        (int)registration->status,
        registration->diagnosed_at,
        registration->cancelled_at,
        (int)registration->registration_type,
        registration->registration_fee
    );
    if (written < 0 || (size_t)written >= capacity) {
        return Result_make_failure("registration line too long");
    }

    return Result_make_success("registration serialized");
}

/**
 * @brief 将一行文本解析为挂号记录结构体
 * @param line             管道符分隔的文本行
 * @param out_registration 输出参数，解析得到的挂号记录
 * @return 解析成功返回 success；格式不合法时返回 failure
 */
static Result RegistrationRepository_parse_line(const char *line, Registration *out_registration) {
    char buffer[TEXT_FILE_REPOSITORY_LINE_CAPACITY];
    char *fields[REGISTRATION_REPOSITORY_FIELD_COUNT];
    size_t field_count = 0;
    Result result;

    if (line == 0 || out_registration == 0) {
        return Result_make_failure("registration parse arguments invalid");
    }

    if (strlen(line) >= sizeof(buffer)) {
        return Result_make_failure("registration line too long");
    }

    strncpy(buffer, line, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';
    result = RepositoryUtils_split_pipe_line(
        buffer, fields, REGISTRATION_REPOSITORY_FIELD_COUNT, &field_count
    );
    if (!result.success) {
        return result;
    }

    result = RepositoryUtils_validate_field_count(
        field_count, REGISTRATION_REPOSITORY_FIELD_COUNT
    );
    if (!result.success) {
        return result;
    }

    /* 逐字段解析 */
    memset(out_registration, 0, sizeof(*out_registration));
    RepositoryUtils_copy_text(
        out_registration->registration_id, sizeof(out_registration->registration_id), fields[0]
    );
    RepositoryUtils_copy_text(
        out_registration->patient_id, sizeof(out_registration->patient_id), fields[1]
    );
    RepositoryUtils_copy_text(
        out_registration->doctor_id, sizeof(out_registration->doctor_id), fields[2]
    );
    RepositoryUtils_copy_text(
        out_registration->department_id, sizeof(out_registration->department_id), fields[3]
    );
    RepositoryUtils_copy_text(
        out_registration->registered_at, sizeof(out_registration->registered_at), fields[4]
    );

    /* 解析状态字段 */
    result = RegistrationRepository_parse_status(fields[5], &out_registration->status);
    if (!result.success) {
        return result;
    }

    RepositoryUtils_copy_text(
        out_registration->diagnosed_at, sizeof(out_registration->diagnosed_at), fields[6]
    );
    RepositoryUtils_copy_text(
        out_registration->cancelled_at, sizeof(out_registration->cancelled_at), fields[7]
    );

    /* 解析挂号类型字段 */
    result = RegistrationRepository_parse_type(fields[8], &out_registration->registration_type);
    if (!result.success) {
        return result;
    }

    /* 解析挂号费字段 */
    result = RegistrationRepository_parse_fee(fields[9], &out_registration->registration_fee);
    if (!result.success) {
        return result;
    }

    return RegistrationRepository_validate(out_registration);
}

/**
 * @brief 判断时间值是否在指定范围内
 *
 * 通过字符串比较实现时间范围判断（要求时间格式为可排序的字符串，如 ISO 8601）。
 *
 * @param value 待检查的时间值
 * @param from  范围起始（NULL 或空表示不限）
 * @param to    范围结束（NULL 或空表示不限）
 * @return 1 表示在范围内，0 表示不在范围内
 */
static int RegistrationRepository_is_in_time_range(
    const char *value,
    const char *from,
    const char *to
) {
    if (value == 0) {
        return 0;
    }

    /* 检查下界 */
    if (from != 0 && from[0] != '\0' && strcmp(value, from) < 0) {
        return 0;
    }

    /* 检查上界 */
    if (to != 0 && to[0] != '\0' && strcmp(value, to) > 0) {
        return 0;
    }

    return 1;
}

/**
 * @brief 判断挂号记录是否匹配患者ID和过滤条件
 * @param registration 待检查的挂号记录
 * @param patient_id   要匹配的患者ID
 * @param filter       过滤条件（可为 NULL）
 * @return 1 表示匹配，0 表示不匹配
 */
static int RegistrationRepository_matches_patient_filter(
    const Registration *registration,
    const char *patient_id,
    const RegistrationRepositoryFilter *filter
) {
    if (registration == 0 || patient_id == 0) {
        return 0;
    }

    /* 患者ID必须匹配 */
    if (strcmp(registration->patient_id, patient_id) != 0) {
        return 0;
    }

    /* 无过滤条件时直接匹配 */
    if (filter == 0) {
        return 1;
    }

    /* 状态过滤 */
    if (filter->use_status && registration->status != filter->status) {
        return 0;
    }

    /* 时间范围过滤 */
    return RegistrationRepository_is_in_time_range(
        registration->registered_at,
        filter->registered_at_from,
        filter->registered_at_to
    );
}

/**
 * @brief 释放挂号记录
 */
static void RegistrationRepository_free_record(void *data) {
    free(data);
}

/**
 * @brief 按挂号ID查找的行处理回调
 */
static Result RegistrationRepository_find_by_id_line_handler(const char *line, void *context) {
    RegistrationFindByIdContext *find_context = (RegistrationFindByIdContext *)context;
    Registration registration;
    Result result = RegistrationRepository_parse_line(line, &registration);

    if (!result.success) {
        return result;
    }

    /* ID不匹配，跳过 */
    if (strcmp(registration.registration_id, find_context->registration_id) != 0) {
        return Result_make_success("registration skipped");
    }

    /* 检查是否已找到（检测重复ID） */
    if (find_context->found) {
        return Result_make_failure("duplicate registration id");
    }

    *find_context->out_registration = registration;
    find_context->found = 1;
    return Result_make_success("registration matched");
}

/**
 * @brief 按患者ID收集挂号记录的行处理回调
 *
 * 对每行进行解析，匹配患者ID和过滤条件后加入结果链表。
 */
static Result RegistrationRepository_collect_by_patient_line_handler(
    const char *line,
    void *context
) {
    RegistrationCollectByPatientContext *collect_context =
        (RegistrationCollectByPatientContext *)context;
    Registration registration;
    Registration *copy = 0;
    Result result = RegistrationRepository_parse_line(line, &registration);

    if (!result.success) {
        return result;
    }

    /* 不匹配过滤条件则跳过 */
    if (!RegistrationRepository_matches_patient_filter(
            &registration,
            collect_context->patient_id,
            collect_context->filter)) {
        return Result_make_success("registration skipped");
    }

    /* 匹配：创建副本加入结果链表 */
    copy = (Registration *)malloc(sizeof(*copy));
    if (copy == 0) {
        return Result_make_failure("failed to allocate registration result");
    }

    *copy = registration;
    if (!LinkedList_append(collect_context->out_registrations, copy)) {
        free(copy);
        return Result_make_failure("failed to append registration result");
    }

    return Result_make_success("registration collected");
}

/**
 * @brief 加载所有挂号记录的行处理回调
 */
static Result RegistrationRepository_load_line_handler(const char *line, void *context) {
    RegistrationLoadContext *load_context = (RegistrationLoadContext *)context;
    Registration registration;
    Registration *copy = 0;
    Result result = RegistrationRepository_parse_line(line, &registration);

    if (!result.success) {
        return result;
    }

    if (load_context == 0 || load_context->out_registrations == 0) {
        return Result_make_failure("registration load context missing");
    }

    copy = (Registration *)malloc(sizeof(*copy));
    if (copy == 0) {
        return Result_make_failure("failed to allocate registration result");
    }

    *copy = registration;
    if (!LinkedList_append(load_context->out_registrations, copy)) {
        free(copy);
        return Result_make_failure("failed to append registration result");
    }

    return Result_make_success("registration loaded");
}

/**
 * @brief 初始化挂号记录仓储
 */
Result RegistrationRepository_init(RegistrationRepository *repository, const char *path) {
    if (repository == 0) {
        return Result_make_failure("registration repository missing");
    }

    return TextFileRepository_init(&repository->storage, path);
}

/**
 * @brief 确保存储文件已就绪（含表头校验/写入）
 *
 * 检查文件是否存在，读取表头并验证。空文件时写入表头。
 */
Result RegistrationRepository_ensure_storage(const RegistrationRepository *repository) {
    char line[TEXT_FILE_REPOSITORY_LINE_CAPACITY];
    FILE *file = 0;
    Result result;

    if (repository == 0) {
        return Result_make_failure("registration repository missing");
    }

    result = TextFileRepository_ensure_file_exists(&repository->storage);
    if (!result.success) {
        return result;
    }

    file = fopen(repository->storage.path, "r");
    if (file == 0) {
        return Result_make_failure("failed to inspect registration storage");
    }

    /* 读取第一个非空行并验证表头 */
    while (fgets(line, sizeof(line), file) != 0) {
        /* 检测行过长 */
        if (strchr(line, '\n') == 0 && !feof(file)) {
            RegistrationRepository_discard_rest_of_line(file);
            fclose(file);
            return Result_make_failure("registration header line too long");
        }

        RepositoryUtils_strip_line_endings(line);
        if (RepositoryUtils_is_blank_line(line)) {
            continue;
        }

        fclose(file);
        if (strcmp(line, REGISTRATION_REPOSITORY_HEADER) != 0) {
            return Result_make_failure("unexpected registration header");
        }

        return Result_make_success("registration storage ready");
    }

    /* 空文件：写入表头 */
    fclose(file);
    return RegistrationRepository_write_header(repository);
}

/**
 * @brief 初始化查询过滤条件为默认值（不启用任何过滤）
 */
void RegistrationRepositoryFilter_init(RegistrationRepositoryFilter *filter) {
    if (filter == 0) {
        return;
    }

    filter->use_status = 0;
    filter->status = REG_STATUS_PENDING;
    filter->registered_at_from = 0;
    filter->registered_at_to = 0;
}

/**
 * @brief 追加一条挂号记录到文件
 */
Result RegistrationRepository_append(
    const RegistrationRepository *repository,
    const Registration *registration
) {
    char line[TEXT_FILE_REPOSITORY_LINE_CAPACITY];
    Result result = RegistrationRepository_ensure_storage(repository);

    if (!result.success) {
        return result;
    }

    /* 序列化并追加写入 */
    result = RegistrationRepository_serialize(registration, line, sizeof(line));
    if (!result.success) {
        return result;
    }

    return TextFileRepository_append_line(&repository->storage, line);
}

/**
 * @brief 按挂号ID查找挂号记录
 */
Result RegistrationRepository_find_by_registration_id(
    const RegistrationRepository *repository,
    const char *registration_id,
    Registration *out_registration
) {
    RegistrationFindByIdContext context;
    Result result;

    if (registration_id == 0 || registration_id[0] == '\0' || out_registration == 0) {
        return Result_make_failure("registration query arguments invalid");
    }

    result = RegistrationRepository_ensure_storage(repository);
    if (!result.success) {
        return result;
    }

    context.registration_id = registration_id;
    context.out_registration = out_registration;
    context.found = 0;
    result = TextFileRepository_for_each_data_line(
        &repository->storage,
        RegistrationRepository_find_by_id_line_handler,
        &context
    );
    if (!result.success) {
        return result;
    }

    if (!context.found) {
        return Result_make_failure("registration not found");
    }

    return Result_make_success("registration found");
}

/**
 * @brief 按患者ID查找挂号记录（支持过滤条件）
 */
Result RegistrationRepository_find_by_patient_id(
    const RegistrationRepository *repository,
    const char *patient_id,
    const RegistrationRepositoryFilter *filter,
    LinkedList *out_registrations
) {
    RegistrationCollectByPatientContext context;
    LinkedList matches;
    Result result;

    if (patient_id == 0 || patient_id[0] == '\0' || out_registrations == 0) {
        return Result_make_failure("patient registration query arguments invalid");
    }

    /* 输出链表必须为空 */
    if (LinkedList_count(out_registrations) != 0) {
        return Result_make_failure("output registration list must be empty");
    }

    result = RegistrationRepository_ensure_storage(repository);
    if (!result.success) {
        return result;
    }

    /* 使用临时链表收集匹配结果 */
    LinkedList_init(&matches);
    context.patient_id = patient_id;
    context.filter = filter;
    context.out_registrations = &matches;
    result = TextFileRepository_for_each_data_line(
        &repository->storage,
        RegistrationRepository_collect_by_patient_line_handler,
        &context
    );
    if (!result.success) {
        RegistrationRepository_clear_list(&matches);
        return result;
    }

    /* 将结果赋值到输出参数 */
    *out_registrations = matches;
    return Result_make_success("patient registrations loaded");
}

/**
 * @brief 加载所有挂号记录到链表
 */
Result RegistrationRepository_load_all(
    const RegistrationRepository *repository,
    LinkedList *out_registrations
) {
    RegistrationLoadContext context;
    LinkedList loaded;
    Result result;

    if (out_registrations == 0) {
        return Result_make_failure("registration output list missing");
    }

    if (LinkedList_count(out_registrations) != 0) {
        return Result_make_failure("output registration list must be empty");
    }

    LinkedList_init(&loaded);
    result = RegistrationRepository_ensure_storage(repository);
    if (result.success == 0) {
        return result;
    }

    context.out_registrations = &loaded;
    result = TextFileRepository_for_each_data_line(
        &repository->storage,
        RegistrationRepository_load_line_handler,
        &context
    );
    if (result.success == 0) {
        RegistrationRepository_clear_list(&loaded);
        return result;
    }

    *out_registrations = loaded;
    return Result_make_success("registrations loaded");
}

/**
 * @brief 全量保存挂号记录列表到文件（覆盖写入）
 */
Result RegistrationRepository_save_all(
    const RegistrationRepository *repository,
    const LinkedList *registrations
) {
    LinkedListNode *current = 0;
    Result result;

    if (repository == 0 || registrations == 0) {
        return Result_make_failure("registration save arguments invalid");
    }

    {
        char line[TEXT_FILE_REPOSITORY_LINE_CAPACITY];
        char *content = 0;
        size_t capacity = (registrations->count + 2) * TEXT_FILE_REPOSITORY_LINE_CAPACITY;
        size_t used = 0;
        size_t len = 0;

        content = (char *)malloc(capacity);
        if (content == 0) {
            return Result_make_failure("failed to allocate registration content buffer");
        }

        len = strlen(REGISTRATION_REPOSITORY_HEADER);
        memcpy(content + used, REGISTRATION_REPOSITORY_HEADER, len);
        used += len;
        content[used++] = '\n';

        current = registrations->head;
        while (current != 0) {
            result = RegistrationRepository_serialize((const Registration *)current->data, line, sizeof(line));
            if (!result.success) {
                free(content);
                return result;
            }

            len = strlen(line);
            if (used + len + 2 > capacity) {
                capacity *= 2;
                content = (char *)realloc(content, capacity);
                if (content == 0) {
                    return Result_make_failure("failed to grow registration content buffer");
                }
            }
            memcpy(content + used, line, len);
            used += len;
            content[used++] = '\n';

            current = current->next;
        }

        content[used] = '\0';
        result = TextFileRepository_save_file(&repository->storage, content);
        free(content);
        return result;
    }
}

/**
 * @brief 清空并释放挂号记录链表中的所有元素
 */
void RegistrationRepository_clear_list(LinkedList *registrations) {
    LinkedList_clear(registrations, RegistrationRepository_free_record);
}
