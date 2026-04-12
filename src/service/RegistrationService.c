/**
 * @file RegistrationService.c
 * @brief 挂号服务模块实现
 *
 * 实现门诊挂号的创建、取消和查询功能。挂号创建时会验证关联实体
 * （患者、医生、科室）的存在性和状态有效性，自动生成挂号ID，
 * 并通过全量加载-修改-保存的方式维护数据一致性。
 */

#include "service/RegistrationService.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "common/IdGenerator.h"
#include "common/StringUtils.h"
#include "repository/RepositoryUtils.h"

/** @brief 挂号记录数据行的字段数量 */
#define REGISTRATION_SERVICE_FIELD_COUNT 8

/** @brief 挂号ID的前缀 */
#define REGISTRATION_SERVICE_ID_PREFIX "REG"

/** @brief 挂号ID序号部分的位宽 */
#define REGISTRATION_SERVICE_ID_WIDTH 4

/**
 * @brief 挂号记录加载上下文结构体
 *
 * 用于在逐行回调加载过程中传递目标链表指针。
 */
typedef struct RegistrationServiceLoadContext {
    LinkedList *registrations;  /* 目标挂号记录链表 */
} RegistrationServiceLoadContext;

/**
 * @brief 判断文本是否非空（非NULL且非空串）
 *
 * @param text  待检查的字符串
 * @return int  1=非空，0=空或NULL
 */
static int RegistrationService_has_text(const char *text) {
    return text != 0 && text[0] != '\0';
}


/**
 * @brief 解析挂号状态字符串为枚举值
 *
 * @param field       状态字段字符串
 * @param out_status  输出参数，解析成功时存放状态枚举值
 * @return Result     操作结果
 */
static Result RegistrationService_parse_status(
    const char *field,
    RegistrationStatus *out_status
) {
    char *end = 0;
    long value = 0;

    if (!RegistrationService_has_text(field) || out_status == 0) {
        return Result_make_failure("registration status missing");
    }

    /* 将字符串转换为整数 */
    errno = 0;
    value = strtol(field, &end, 10);
    if (errno != 0 || end == field || end == 0 || *end != '\0') {
        return Result_make_failure("registration status invalid");
    }

    /* 检查状态值是否在有效范围内 */
    if (value < REG_STATUS_PENDING || value > REG_STATUS_CANCELLED) {
        return Result_make_failure("registration status invalid");
    }

    *out_status = (RegistrationStatus)value;
    return Result_make_success("registration status parsed");
}

/**
 * @brief 解析一行文本为挂号记录结构体
 *
 * 将管道符分隔的数据行解析为 Registration 结构体。
 * 字段顺序：挂号ID|患者ID|医生ID|科室ID|挂号时间|状态|诊断时间|取消时间
 *
 * @param line              数据行文本
 * @param out_registration  输出参数，解析成功时存放挂号记录
 * @return Result           操作结果
 */
static Result RegistrationService_parse_registration_line(
    const char *line,
    Registration *out_registration
) {
    char buffer[TEXT_FILE_REPOSITORY_LINE_CAPACITY];
    char *fields[REGISTRATION_SERVICE_FIELD_COUNT];
    size_t field_count = 0;
    Result result;

    if (line == 0 || out_registration == 0) {
        return Result_make_failure("registration parse arguments invalid");
    }

    if (strlen(line) >= sizeof(buffer)) {
        return Result_make_failure("registration line too long");
    }

    /* 复制到缓冲区后进行分割（分割会修改字符串内容） */
    strncpy(buffer, line, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';
    result = RepositoryUtils_split_pipe_line(
        buffer,
        fields,
        REGISTRATION_SERVICE_FIELD_COUNT,
        &field_count
    );
    if (!result.success) {
        return result;
    }

    /* 校验字段数量 */
    result = RepositoryUtils_validate_field_count(
        field_count,
        REGISTRATION_SERVICE_FIELD_COUNT
    );
    if (!result.success) {
        return result;
    }

    /* 将各字段复制到结构体对应成员 */
    memset(out_registration, 0, sizeof(*out_registration));
    StringUtils_copy(
        out_registration->registration_id,
        sizeof(out_registration->registration_id),
        fields[0]
    );
    StringUtils_copy(
        out_registration->patient_id,
        sizeof(out_registration->patient_id),
        fields[1]
    );
    StringUtils_copy(
        out_registration->doctor_id,
        sizeof(out_registration->doctor_id),
        fields[2]
    );
    StringUtils_copy(
        out_registration->department_id,
        sizeof(out_registration->department_id),
        fields[3]
    );
    StringUtils_copy(
        out_registration->registered_at,
        sizeof(out_registration->registered_at),
        fields[4]
    );

    /* 解析状态枚举 */
    result = RegistrationService_parse_status(fields[5], &out_registration->status);
    if (!result.success) {
        return result;
    }

    StringUtils_copy(
        out_registration->diagnosed_at,
        sizeof(out_registration->diagnosed_at),
        fields[6]
    );
    StringUtils_copy(
        out_registration->cancelled_at,
        sizeof(out_registration->cancelled_at),
        fields[7]
    );

    return Result_make_success("registration parsed");
}

/**
 * @brief 逐行加载回调：解析并收集挂号记录
 *
 * 作为 TextFileRepository_for_each_data_line 的回调函数，
 * 解析每一行挂号数据并追加到上下文中的链表。
 *
 * @param line     数据行文本
 * @param context  加载上下文（包含目标链表指针）
 * @return Result  操作结果
 */
static Result RegistrationService_collect_registration_line(
    const char *line,
    void *context
) {
    RegistrationServiceLoadContext *load_context =
        (RegistrationServiceLoadContext *)context;
    Registration *copy = 0;
    Result result;

    if (load_context == 0 || load_context->registrations == 0) {
        return Result_make_failure("registration load context missing");
    }

    /* 分配新的挂号记录 */
    copy = (Registration *)malloc(sizeof(*copy));
    if (copy == 0) {
        return Result_make_failure("failed to allocate registration");
    }

    /* 解析当前行 */
    result = RegistrationService_parse_registration_line(line, copy);
    if (!result.success) {
        free(copy);
        return result;
    }

    /* 追加到链表 */
    if (!LinkedList_append(load_context->registrations, copy)) {
        free(copy);
        return Result_make_failure("failed to append registration");
    }

    return Result_make_success("registration loaded");
}

/**
 * @brief 从存储文件中加载所有挂号记录
 *
 * @param service            指向挂号服务结构体
 * @param out_registrations  输出参数，加载结果链表
 * @return Result            操作结果
 */
static Result RegistrationService_load_all_registrations(
    RegistrationService *service,
    LinkedList *out_registrations
) {
    RegistrationServiceLoadContext context;
    Result result;

    if (service == 0 || service->registration_repository == 0 || out_registrations == 0) {
        return Result_make_failure("registration service missing");
    }

    LinkedList_init(out_registrations);
    result = RegistrationRepository_ensure_storage(service->registration_repository);
    if (!result.success) {
        return result;
    }

    /* 通过逐行回调方式加载所有挂号记录 */
    context.registrations = out_registrations;
    result = TextFileRepository_for_each_data_line(
        &service->registration_repository->storage,
        RegistrationService_collect_registration_line,
        &context
    );
    if (!result.success) {
        RegistrationRepository_clear_list(out_registrations);
        return result;
    }

    return Result_make_success("registrations loaded");
}

/**
 * @brief 判断挂号记录是否匹配过滤条件
 *
 * 支持按状态和挂号时间范围进行过滤。
 *
 * @param registration  待匹配的挂号记录
 * @param filter        过滤条件（可为 NULL 表示不过滤）
 * @return int          1=匹配，0=不匹配
 */
static int RegistrationService_matches_filter(
    const Registration *registration,
    const RegistrationRepositoryFilter *filter
) {
    if (registration == 0) {
        return 0;
    }

    /* 无过滤条件时全部匹配 */
    if (filter == 0) {
        return 1;
    }

    /* 按状态过滤 */
    if (filter->use_status && registration->status != filter->status) {
        return 0;
    }

    /* 按时间范围过滤（起始时间） */
    if (RegistrationService_has_text(filter->registered_at_from) &&
        strcmp(registration->registered_at, filter->registered_at_from) < 0) {
        return 0;
    }

    /* 按时间范围过滤（结束时间） */
    if (RegistrationService_has_text(filter->registered_at_to) &&
        strcmp(registration->registered_at, filter->registered_at_to) > 0) {
        return 0;
    }

    return 1;
}

/**
 * @brief 将挂号记录的副本追加到输出链表
 *
 * @param out_registrations  输出结果链表
 * @param registration       待复制的源挂号记录
 * @return Result            操作结果
 */
static Result RegistrationService_append_registration_copy(
    LinkedList *out_registrations,
    const Registration *registration
) {
    Registration *copy = 0;

    if (out_registrations == 0 || registration == 0) {
        return Result_make_failure("registration copy arguments invalid");
    }

    copy = (Registration *)malloc(sizeof(*copy));
    if (copy == 0) {
        return Result_make_failure("failed to allocate registration copy");
    }

    *copy = *registration;
    if (!LinkedList_append(out_registrations, copy)) {
        free(copy);
        return Result_make_failure("failed to append registration copy");
    }

    return Result_make_success("registration copy appended");
}

/**
 * @brief 验证挂号相关实体（患者、医生、科室）的存在性和有效性
 *
 * 检查患者是否存在、医生是否存在且处于活跃状态、科室是否存在、
 * 医生是否属于指定科室。
 *
 * @param service        指向挂号服务结构体
 * @param patient_id     患者ID
 * @param doctor_id      医生ID
 * @param department_id  科室ID
 * @return Result        验证结果
 */
static Result RegistrationService_validate_related_entities(
    RegistrationService *service,
    const char *patient_id,
    const char *doctor_id,
    const char *department_id
) {
    Patient patient;
    Doctor doctor;
    Department department;
    Result result;

    /* 验证患者是否存在 */
    result = PatientRepository_find_by_id(
        service->patient_repository,
        patient_id,
        &patient
    );
    if (!result.success) {
        return Result_make_failure("patient not found");
    }

    /* 验证医生是否存在 */
    result = DoctorRepository_find_by_doctor_id(
        service->doctor_repository,
        doctor_id,
        &doctor
    );
    if (!result.success) {
        return Result_make_failure("doctor not found");
    }

    /* 验证科室是否存在 */
    result = DepartmentRepository_find_by_department_id(
        service->department_repository,
        department_id,
        &department
    );
    if (!result.success) {
        return Result_make_failure("department not found");
    }

    /* 检查医生是否处于活跃状态 */
    if (doctor.status != DOCTOR_STATUS_ACTIVE) {
        return Result_make_failure("doctor inactive");
    }

    /* 检查医生是否属于指定科室 */
    if (strcmp(doctor.department_id, department_id) != 0) {
        return Result_make_failure("doctor department mismatch");
    }

    return Result_make_success("related entities valid");
}

/**
 * @brief 从已加载的挂号列表中计算下一个序列号
 *
 * 遍历所有挂号记录，找到最大的序列号并加1。
 *
 * @param registrations  已加载的挂号记录链表
 * @param out_sequence   输出参数，下一个可用序列号
 * @return Result        操作结果
 */
static Result RegistrationService_next_sequence_from_list(
    const LinkedList *registrations,
    int *out_sequence
) {
    LinkedListNode *current = 0;
    int max_sequence = 0;
    const size_t prefix_len = strlen(REGISTRATION_SERVICE_ID_PREFIX);

    if (out_sequence == 0) {
        return Result_make_failure("registration sequence output missing");
    }

    current = registrations->head;
    while (current != 0) {
        const Registration *registration = (const Registration *)current->data;
        const char *suffix = registration->registration_id;
        char *end = 0;
        long value = 0;

        /* 验证ID前缀格式 */
        if (strncmp(
                registration->registration_id,
                REGISTRATION_SERVICE_ID_PREFIX,
                prefix_len) != 0) {
            return Result_make_failure("registration id format invalid");
        }

        /* 提取前缀后的数字部分 */
        suffix += prefix_len;
        if (!RegistrationService_has_text(suffix)) {
            return Result_make_failure("registration id format invalid");
        }

        /* 解析序列号 */
        errno = 0;
        value = strtol(suffix, &end, 10);
        if (errno != 0 || end == suffix || end == 0 || *end != '\0' || value < 0) {
            return Result_make_failure("registration id format invalid");
        }

        /* 记录最大序列号 */
        if ((int)value > max_sequence) {
            max_sequence = (int)value;
        }

        current = current->next;
    }

    *out_sequence = max_sequence + 1;
    return Result_make_success("registration sequence ready");
}

/**
 * @brief 从存储中计算下一个挂号序列号
 *
 * 加载所有挂号记录并找到最大的序列号，加1后作为新的序列号。
 *
 * @param service       指向挂号服务结构体
 * @param out_sequence  输出参数，下一个可用序列号
 * @return Result       操作结果
 */
static Result RegistrationService_next_registration_sequence(
    RegistrationService *service,
    int *out_sequence
) {
    LinkedList registrations;
    Result result;

    if (out_sequence == 0) {
        return Result_make_failure("registration sequence output missing");
    }

    result = RegistrationService_load_all_registrations(service, &registrations);
    if (!result.success) {
        return result;
    }

    /* Delegate to the list-based sequence extraction */
    result = RegistrationService_next_sequence_from_list(&registrations, out_sequence);
    RegistrationRepository_clear_list(&registrations);
    return result;
}

/**
 * @brief 填充新挂号记录的各字段
 *
 * 将各参数复制到 Registration 结构体中，初始状态设为待诊。
 *
 * @param registration     输出参数，待填充的挂号记录
 * @param registration_id  挂号ID
 * @param patient_id       患者ID
 * @param doctor_id        医生ID
 * @param department_id    科室ID
 * @param registered_at    挂号时间
 * @return Result          操作结果
 */
static Result RegistrationService_fill_new_registration(
    Registration *registration,
    const char *registration_id,
    const char *patient_id,
    const char *doctor_id,
    const char *department_id,
    const char *registered_at
) {
    if (registration == 0) {
        return Result_make_failure("registration output missing");
    }

    memset(registration, 0, sizeof(*registration));
    StringUtils_copy(
        registration->registration_id,
        sizeof(registration->registration_id),
        registration_id
    );
    StringUtils_copy(
        registration->patient_id,
        sizeof(registration->patient_id),
        patient_id
    );
    StringUtils_copy(
        registration->doctor_id,
        sizeof(registration->doctor_id),
        doctor_id
    );
    StringUtils_copy(
        registration->department_id,
        sizeof(registration->department_id),
        department_id
    );
    StringUtils_copy(
        registration->registered_at,
        sizeof(registration->registered_at),
        registered_at
    );
    registration->status = REG_STATUS_PENDING;  /* 初始状态为待诊 */
    registration->diagnosed_at[0] = '\0';
    registration->cancelled_at[0] = '\0';
    return Result_make_success("registration filled");
}

/**
 * @brief 在挂号记录链表中按ID查找挂号记录
 *
 * @param registrations      挂号记录链表
 * @param registration_id    待查找的挂号ID
 * @param out_registration   输出参数，找到时指向链表中的记录指针
 * @return Result            操作结果
 */
static Result RegistrationService_find_record_in_list(
    LinkedList *registrations,
    const char *registration_id,
    Registration **out_registration
) {
    LinkedListNode *current = 0;

    if (registrations == 0 || !RegistrationService_has_text(registration_id) ||
        out_registration == 0) {
        return Result_make_failure("registration list arguments invalid");
    }

    current = registrations->head;
    while (current != 0) {
        Registration *registration = (Registration *)current->data;
        if (strcmp(registration->registration_id, registration_id) == 0) {
            *out_registration = registration;
            return Result_make_success("registration found in list");
        }

        current = current->next;
    }

    return Result_make_failure("registration not found");
}

/**
 * @brief 初始化挂号服务
 *
 * 将外部创建的各仓库指针注入到挂号服务中，并确保存储已就绪。
 *
 * @param service                   指向待初始化的挂号服务结构体
 * @param registration_repository   挂号仓库指针
 * @param patient_repository        患者仓库指针
 * @param doctor_repository         医生仓库指针
 * @param department_repository     科室仓库指针
 * @return Result                   操作结果
 */
Result RegistrationService_init(
    RegistrationService *service,
    RegistrationRepository *registration_repository,
    PatientRepository *patient_repository,
    DoctorRepository *doctor_repository,
    DepartmentRepository *department_repository
) {
    Result result;

    if (service == 0 || registration_repository == 0 || patient_repository == 0 ||
        doctor_repository == 0 || department_repository == 0) {
        return Result_make_failure("registration service dependencies missing");
    }

    /* 确保挂号存储文件已就绪 */
    result = RegistrationRepository_ensure_storage(registration_repository);
    if (!result.success) {
        return result;
    }

    /* 注入各仓库依赖 */
    service->registration_repository = registration_repository;
    service->patient_repository = patient_repository;
    service->doctor_repository = doctor_repository;
    service->department_repository = department_repository;
    return Result_make_success("registration service ready");
}

/**
 * @brief 创建新挂号记录
 *
 * 流程：校验参数 -> 验证关联实体 -> 加载已有记录 ->
 * 生成挂号ID -> 填充记录 -> 追加到链表 -> 保存全量数据。
 *
 * @param service           指向挂号服务结构体
 * @param patient_id        患者ID
 * @param doctor_id         医生ID
 * @param department_id     科室ID
 * @param registered_at     挂号时间字符串
 * @param out_registration  输出参数，创建成功时存放挂号信息
 * @return Result           操作结果
 */
Result RegistrationService_create(
    RegistrationService *service,
    const char *patient_id,
    const char *doctor_id,
    const char *department_id,
    const char *registered_at,
    Registration *out_registration
) {
    LinkedList registrations;
    Registration *copy = 0;
    char registration_id[HIS_DOMAIN_ID_CAPACITY];
    int next_sequence = 0;
    Result result;

    /* 校验所有必填参数 */
    if (service == 0 || !RegistrationService_has_text(patient_id) ||
        !RegistrationService_has_text(doctor_id) ||
        !RegistrationService_has_text(department_id) ||
        !RegistrationService_has_text(registered_at) ||
        out_registration == 0) {
        return Result_make_failure("registration create arguments invalid");
    }

    /* 验证关联实体的存在性和有效性 */
    result = RegistrationService_validate_related_entities(
        service,
        patient_id,
        doctor_id,
        department_id
    );
    if (!result.success) {
        return result;
    }

    /* 加载所有已有挂号记录 */
    result = RegistrationService_load_all_registrations(service, &registrations);
    if (!result.success) {
        return result;
    }

    /* 从已有记录中计算下一个可用序列号 */
    result = RegistrationService_next_sequence_from_list(&registrations, &next_sequence);
    if (!result.success) {
        RegistrationRepository_clear_list(&registrations);
        return result;
    }

    /* 生成格式化的挂号ID（如 REG0001） */
    if (!IdGenerator_format(
            registration_id,
            sizeof(registration_id),
            REGISTRATION_SERVICE_ID_PREFIX,
            next_sequence,
            REGISTRATION_SERVICE_ID_WIDTH)) {
        RegistrationRepository_clear_list(&registrations);
        return Result_make_failure("failed to generate registration id");
    }

    /* 填充新挂号记录 */
    result = RegistrationService_fill_new_registration(
        out_registration,
        registration_id,
        patient_id,
        doctor_id,
        department_id,
        registered_at
    );
    if (!result.success) {
        RegistrationRepository_clear_list(&registrations);
        return result;
    }

    /* 分配副本并追加到链表 */
    copy = (Registration *)malloc(sizeof(*copy));
    if (copy == 0) {
        RegistrationRepository_clear_list(&registrations);
        return Result_make_failure("failed to allocate registration");
    }

    *copy = *out_registration;
    if (!LinkedList_append(&registrations, copy)) {
        free(copy);
        RegistrationRepository_clear_list(&registrations);
        return Result_make_failure("failed to append registration");
    }

    /* 保存全量数据到文件 */
    result = RegistrationRepository_save_all(
        service->registration_repository,
        &registrations
    );
    RegistrationRepository_clear_list(&registrations);
    return result;
}

/**
 * @brief 根据挂号ID查找挂号记录
 *
 * @param service           指向挂号服务结构体
 * @param registration_id   挂号ID
 * @param out_registration  输出参数，查找成功时存放挂号信息
 * @return Result           操作结果
 */
Result RegistrationService_find_by_registration_id(
    RegistrationService *service,
    const char *registration_id,
    Registration *out_registration
) {
    if (service == 0 || !RegistrationService_has_text(registration_id) ||
        out_registration == 0) {
        return Result_make_failure("registration query arguments invalid");
    }

    /* 委托仓库层按ID查找 */
    return RegistrationRepository_find_by_registration_id(
        service->registration_repository,
        registration_id,
        out_registration
    );
}

/**
 * @brief 取消挂号
 *
 * 流程：校验参数 -> 加载所有记录 -> 查找目标记录 ->
 * 标记为已取消 -> 保存全量数据。
 *
 * @param service           指向挂号服务结构体
 * @param registration_id   待取消的挂号ID
 * @param cancelled_at      取消时间字符串
 * @param out_registration  输出参数，取消成功时存放更新后的挂号信息
 * @return Result           操作结果
 */
Result RegistrationService_cancel(
    RegistrationService *service,
    const char *registration_id,
    const char *cancelled_at,
    Registration *out_registration
) {
    LinkedList registrations;
    Registration *target = 0;
    Result result;

    if (service == 0 || !RegistrationService_has_text(registration_id) ||
        !RegistrationService_has_text(cancelled_at) ||
        out_registration == 0) {
        return Result_make_failure("registration cancel arguments invalid");
    }

    /* 加载所有挂号记录 */
    result = RegistrationService_load_all_registrations(service, &registrations);
    if (!result.success) {
        return result;
    }

    /* 在链表中查找目标记录 */
    result = RegistrationService_find_record_in_list(
        &registrations,
        registration_id,
        &target
    );
    if (!result.success) {
        RegistrationRepository_clear_list(&registrations);
        return result;
    }

    /* 调用领域模型方法标记为取消 */
    if (!Registration_cancel(target, cancelled_at)) {
        RegistrationRepository_clear_list(&registrations);
        return Result_make_failure("registration cannot be cancelled");
    }

    /* 保存更新后的全量数据 */
    result = RegistrationRepository_save_all(
        service->registration_repository,
        &registrations
    );
    if (!result.success) {
        RegistrationRepository_clear_list(&registrations);
        return result;
    }

    *out_registration = *target;
    RegistrationRepository_clear_list(&registrations);
    return Result_make_success("registration cancelled");
}

/**
 * @brief 根据患者ID查找挂号记录
 *
 * 先验证患者是否存在，然后委托仓库层按患者ID和过滤条件查找。
 *
 * @param service            指向挂号服务结构体
 * @param patient_id         患者ID
 * @param filter             过滤条件（可为 NULL）
 * @param out_registrations  输出参数，查找结果链表
 * @return Result            操作结果
 */
Result RegistrationService_find_by_patient_id(
    RegistrationService *service,
    const char *patient_id,
    const RegistrationRepositoryFilter *filter,
    LinkedList *out_registrations
) {
    Patient patient;
    Result result;

    if (service == 0 || !RegistrationService_has_text(patient_id) ||
        out_registrations == 0) {
        return Result_make_failure("patient registration query arguments invalid");
    }

    /* 验证患者是否存在 */
    result = PatientRepository_find_by_id(
        service->patient_repository,
        patient_id,
        &patient
    );
    if (!result.success) {
        return Result_make_failure("patient not found");
    }

    /* 委托仓库层查找 */
    return RegistrationRepository_find_by_patient_id(
        service->registration_repository,
        patient_id,
        filter,
        out_registrations
    );
}

/**
 * @brief 根据医生ID查找挂号记录
 *
 * 先验证医生是否存在，然后从全量数据中按医生ID和过滤条件筛选。
 *
 * @param service            指向挂号服务结构体
 * @param doctor_id          医生ID
 * @param filter             过滤条件（可为 NULL）
 * @param out_registrations  输出参数，查找结果链表
 * @return Result            操作结果
 */
Result RegistrationService_find_by_doctor_id(
    RegistrationService *service,
    const char *doctor_id,
    const RegistrationRepositoryFilter *filter,
    LinkedList *out_registrations
) {
    Doctor doctor;
    LinkedList registrations;
    const LinkedListNode *current = 0;
    Result result;

    if (service == 0 || !RegistrationService_has_text(doctor_id) || out_registrations == 0) {
        return Result_make_failure("doctor registration query arguments invalid");
    }

    /* 验证医生是否存在 */
    result = DoctorRepository_find_by_doctor_id(
        service->doctor_repository,
        doctor_id,
        &doctor
    );
    if (!result.success) {
        return Result_make_failure("doctor not found");
    }

    /* 加载所有挂号记录 */
    LinkedList_init(out_registrations);
    result = RegistrationService_load_all_registrations(service, &registrations);
    if (!result.success) {
        return result;
    }

    /* 遍历筛选匹配医生ID且满足过滤条件的记录 */
    current = registrations.head;
    while (current != 0) {
        const Registration *registration = (const Registration *)current->data;

        if (strcmp(registration->doctor_id, doctor_id) == 0 &&
            RegistrationService_matches_filter(registration, filter)) {
            result = RegistrationService_append_registration_copy(
                out_registrations,
                registration
            );
            if (!result.success) {
                RegistrationRepository_clear_list(&registrations);
                RegistrationRepository_clear_list(out_registrations);
                return result;
            }
        }

        current = current->next;
    }

    RegistrationRepository_clear_list(&registrations);
    return Result_make_success("doctor registrations loaded");
}
