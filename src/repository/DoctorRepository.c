/**
 * @file DoctorRepository.c
 * @brief 医生数据仓储层实现
 *
 * 实现医生（Doctor）数据的持久化存储操作，包括：
 * - 医生数据的序列化与反序列化
 * - 数据校验（必填字段、状态值域、保留字符检测）
 * - 加载、按ID查找、按科室ID筛选、追加保存、全量保存操作
 */

#include "repository/DoctorRepository.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "repository/RepositoryUtils.h"

/** 医生记录的字段数量 */
#define DOCTOR_REPOSITORY_FIELD_COUNT 6

/** 医生数据文件的表头行 */
static const char *DOCTOR_REPOSITORY_HEADER =
    "doctor_id|name|title|department_id|schedule|status";

/**
 * @brief 加载所有医生时使用的上下文结构体
 */
typedef struct DoctorRepositoryLoadContext {
    LinkedList *doctors; /**< 用于存放加载结果的链表 */
} DoctorRepositoryLoadContext;

/**
 * @brief 按ID查找医生时使用的上下文结构体
 */
typedef struct DoctorRepositoryFindContext {
    const char *doctor_id; /**< 要查找的医生ID */
    Doctor *out_doctor;    /**< 输出：找到的医生数据 */
    int found;             /**< 是否已找到标志 */
} DoctorRepositoryFindContext;

/**
 * @brief 按科室ID筛选医生时使用的上下文结构体
 */
typedef struct DoctorRepositoryFilterContext {
    const char *department_id; /**< 要匹配的科室ID */
    LinkedList *doctors;       /**< 用于存放匹配结果的链表 */
} DoctorRepositoryFilterContext;

/**
 * @brief 判断文本是否非空
 * @param text 待检查的文本
 * @return 1 表示非空，0 表示空指针或空字符串
 */
static int DoctorRepository_has_text(const char *text) {
    return text != 0 && text[0] != '\0';
}

/**
 * @brief 安全复制字符串到目标缓冲区
 * @param destination 目标缓冲区
 * @param capacity    目标缓冲区容量
 * @param source      源字符串（可为 NULL）
 */
static void DoctorRepository_copy_string(
    char *destination,
    size_t capacity,
    const char *source
) {
    if (destination == 0 || capacity == 0) {
        return;
    }

    if (source == 0) {
        destination[0] = '\0';
        return;
    }

    strncpy(destination, source, capacity - 1);
    destination[capacity - 1] = '\0'; /* 确保终止符 */
}

/**
 * @brief 校验医生数据的合法性
 *
 * 检查必填字段、状态枚举值域、保留字符等。
 *
 * @param doctor 待校验的医生数据
 * @return 合法返回 success；不合法返回 failure
 */
static Result DoctorRepository_validate(const Doctor *doctor) {
    if (doctor == 0) {
        return Result_make_failure("doctor missing");
    }

    if (!DoctorRepository_has_text(doctor->doctor_id)) {
        return Result_make_failure("doctor id missing");
    }

    if (!DoctorRepository_has_text(doctor->name)) {
        return Result_make_failure("doctor name missing");
    }

    if (!DoctorRepository_has_text(doctor->department_id)) {
        return Result_make_failure("doctor department missing");
    }

    /* 校验状态枚举值 */
    if (doctor->status != DOCTOR_STATUS_INACTIVE &&
        doctor->status != DOCTOR_STATUS_ACTIVE) {
        return Result_make_failure("doctor status invalid");
    }

    /* 检查所有文本字段是否包含保留字符 */
    if (!RepositoryUtils_is_safe_field_text(doctor->doctor_id) ||
        !RepositoryUtils_is_safe_field_text(doctor->name) ||
        !RepositoryUtils_is_safe_field_text(doctor->title) ||
        !RepositoryUtils_is_safe_field_text(doctor->department_id) ||
        !RepositoryUtils_is_safe_field_text(doctor->schedule)) {
        return Result_make_failure("doctor field contains reserved character");
    }

    return Result_make_success("doctor valid");
}

/**
 * @brief 解析文本字段为医生状态枚举值
 * @param field      文本字段
 * @param out_status 输出参数，解析得到的状态值
 * @return 成功返回 success；格式不合法时返回 failure
 */
static Result DoctorRepository_parse_status(
    const char *field,
    DoctorStatus *out_status
) {
    char *end_pointer = 0;
    long value = 0;

    if (field == 0 || out_status == 0 || field[0] == '\0') {
        return Result_make_failure("doctor status missing");
    }

    value = strtol(field, &end_pointer, 10);
    if (end_pointer == field || end_pointer == 0 || *end_pointer != '\0') {
        return Result_make_failure("doctor status invalid");
    }

    /* 检查状态值是否在合法范围内 */
    if (value != DOCTOR_STATUS_INACTIVE && value != DOCTOR_STATUS_ACTIVE) {
        return Result_make_failure("doctor status invalid");
    }

    *out_status = (DoctorStatus)value;
    return Result_make_success("doctor status parsed");
}

/**
 * @brief 将医生结构体格式化为管道符分隔的文本行
 * @param doctor        医生数据
 * @param line          输出缓冲区
 * @param line_capacity 缓冲区容量
 * @return 成功返回 success；数据无效或缓冲区不足时返回 failure
 */
static Result DoctorRepository_format_line(
    const Doctor *doctor,
    char *line,
    size_t line_capacity
) {
    int written = 0;
    Result result = DoctorRepository_validate(doctor);

    if (result.success == 0) {
        return result;
    }

    if (line == 0 || line_capacity == 0) {
        return Result_make_failure("doctor line buffer missing");
    }

    written = snprintf(
        line,
        line_capacity,
        "%s|%s|%s|%s|%s|%d",
        doctor->doctor_id,
        doctor->name,
        doctor->title,
        doctor->department_id,
        doctor->schedule,
        (int)doctor->status
    );
    if (written < 0 || (size_t)written >= line_capacity) {
        return Result_make_failure("doctor line too long");
    }

    return Result_make_success("doctor formatted");
}

/**
 * @brief 将一行文本解析为医生结构体
 * @param line   管道符分隔的文本行
 * @param doctor 输出参数，解析得到的医生数据
 * @return 解析成功返回 success；格式不合法时返回 failure
 */
static Result DoctorRepository_parse_line(const char *line, Doctor *doctor) {
    char mutable_line[TEXT_FILE_REPOSITORY_LINE_CAPACITY];
    char *fields[DOCTOR_REPOSITORY_FIELD_COUNT];
    size_t field_count = 0;
    Result result;

    if (line == 0 || doctor == 0) {
        return Result_make_failure("doctor line missing");
    }

    /* 复制到可修改缓冲区 */
    DoctorRepository_copy_string(mutable_line, sizeof(mutable_line), line);
    result = RepositoryUtils_split_pipe_line(
        mutable_line,
        fields,
        DOCTOR_REPOSITORY_FIELD_COUNT,
        &field_count
    );
    if (result.success == 0) {
        return result;
    }

    result = RepositoryUtils_validate_field_count(
        field_count,
        DOCTOR_REPOSITORY_FIELD_COUNT
    );
    if (result.success == 0) {
        return result;
    }

    /* 逐字段解析 */
    memset(doctor, 0, sizeof(*doctor));
    DoctorRepository_copy_string(doctor->doctor_id, sizeof(doctor->doctor_id), fields[0]);
    DoctorRepository_copy_string(doctor->name, sizeof(doctor->name), fields[1]);
    DoctorRepository_copy_string(doctor->title, sizeof(doctor->title), fields[2]);
    DoctorRepository_copy_string(doctor->department_id, sizeof(doctor->department_id), fields[3]);
    DoctorRepository_copy_string(doctor->schedule, sizeof(doctor->schedule), fields[4]);

    /* 解析状态字段 */
    result = DoctorRepository_parse_status(fields[5], &doctor->status);
    if (result.success == 0) {
        return result;
    }

    return DoctorRepository_validate(doctor);
}

/**
 * @brief 确保医生数据文件包含正确的表头
 * @param repository 医生仓储实例
 * @return 表头正确返回 success；不匹配或操作失败时返回 failure
 */
static Result DoctorRepository_ensure_header(DoctorRepository *repository) {
    char line[TEXT_FILE_REPOSITORY_LINE_CAPACITY];
    FILE *file = 0;
    Result result;

    if (repository == 0) {
        return Result_make_failure("doctor repository missing");
    }

    result = TextFileRepository_ensure_file_exists(&repository->storage);
    if (result.success == 0) {
        return result;
    }

    /* 读取第一个非空行，检查是否为正确的表头 */
    file = fopen(repository->storage.path, "r");
    if (file == 0) {
        return Result_make_failure("failed to inspect doctor repository");
    }

    while (fgets(line, sizeof(line), file) != 0) {
        RepositoryUtils_strip_line_endings(line);
        if (RepositoryUtils_is_blank_line(line)) {
            continue;
        }

        fclose(file);
        if (strcmp(line, DOCTOR_REPOSITORY_HEADER) != 0) {
            return Result_make_failure("doctor repository header mismatch");
        }

        return Result_make_success("doctor header ready");
    }

    /* 检查读取错误 */
    if (ferror(file) != 0) {
        fclose(file);
        return Result_make_failure("failed to read doctor repository");
    }

    /* 空文件：写入表头 */
    fclose(file);
    return TextFileRepository_append_line(&repository->storage, DOCTOR_REPOSITORY_HEADER);
}

/**
 * @brief 加载所有医生的行处理回调
 * @param line    当前行文本
 * @param context 加载上下文
 * @return 成功返回 success；内存分配失败或解析错误返回 failure
 */
static Result DoctorRepository_collect_line(const char *line, void *context) {
    DoctorRepositoryLoadContext *load_context = (DoctorRepositoryLoadContext *)context;
    Doctor *doctor = 0;
    Result result;

    if (load_context == 0 || load_context->doctors == 0) {
        return Result_make_failure("doctor load context missing");
    }

    /* 在堆上分配医生对象 */
    doctor = (Doctor *)malloc(sizeof(Doctor));
    if (doctor == 0) {
        return Result_make_failure("failed to allocate doctor");
    }

    result = DoctorRepository_parse_line(line, doctor);
    if (result.success == 0) {
        free(doctor);
        return result;
    }

    if (!LinkedList_append(load_context->doctors, doctor)) {
        free(doctor);
        return Result_make_failure("failed to append doctor");
    }

    return Result_make_success("doctor loaded");
}

/**
 * @brief 按ID查找医生的行处理回调
 * @param line    当前行文本
 * @param context 查找上下文
 * @return 始终返回 success（遍历所有行）
 */
static Result DoctorRepository_find_line(const char *line, void *context) {
    DoctorRepositoryFindContext *find_context = (DoctorRepositoryFindContext *)context;
    Doctor doctor;
    Result result;

    if (find_context == 0 || find_context->doctor_id == 0 ||
        find_context->out_doctor == 0) {
        return Result_make_failure("doctor find context missing");
    }

    result = DoctorRepository_parse_line(line, &doctor);
    if (result.success == 0) {
        return result;
    }

    /* ID匹配时复制数据并标记 */
    if (strcmp(doctor.doctor_id, find_context->doctor_id) == 0) {
        *(find_context->out_doctor) = doctor;
        find_context->found = 1;
    }

    return Result_make_success("doctor inspected");
}

/**
 * @brief 按科室ID筛选医生的行处理回调
 *
 * 科室ID匹配时，创建医生副本加入结果链表。
 *
 * @param line    当前行文本
 * @param context 筛选上下文
 * @return 成功返回 success；内存分配失败或解析错误返回 failure
 */
static Result DoctorRepository_filter_line(const char *line, void *context) {
    DoctorRepositoryFilterContext *filter_context =
        (DoctorRepositoryFilterContext *)context;
    Doctor doctor;
    Doctor *copy = 0;
    Result result;

    if (filter_context == 0 || filter_context->department_id == 0 ||
        filter_context->doctors == 0) {
        return Result_make_failure("doctor filter context missing");
    }

    result = DoctorRepository_parse_line(line, &doctor);
    if (result.success == 0) {
        return result;
    }

    /* 科室ID不匹配，跳过 */
    if (strcmp(doctor.department_id, filter_context->department_id) != 0) {
        return Result_make_success("doctor skipped");
    }

    /* 匹配：创建副本加入结果链表 */
    copy = (Doctor *)malloc(sizeof(Doctor));
    if (copy == 0) {
        return Result_make_failure("failed to allocate doctor");
    }

    *copy = doctor;
    if (!LinkedList_append(filter_context->doctors, copy)) {
        free(copy);
        return Result_make_failure("failed to append doctor");
    }

    return Result_make_success("doctor matched");
}

/**
 * @brief 校验医生链表中每个元素的合法性
 * @param doctors 待校验的医生链表
 * @return 1 表示全部合法，0 表示存在不合法项
 */
static int DoctorRepository_validate_list(const LinkedList *doctors) {
    LinkedListNode *current = 0;

    if (doctors == 0) {
        return 0;
    }

    current = doctors->head;
    while (current != 0) {
        Result result = DoctorRepository_validate((const Doctor *)current->data);
        if (result.success == 0) {
            return 0;
        }

        current = current->next;
    }

    return 1;
}

/**
 * @brief 释放链表节点中的数据（free 包装）
 * @param data 要释放的数据指针
 */
static void DoctorRepository_free_item(void *data) {
    free(data);
}

/**
 * @brief 初始化医生仓储
 * @param repository 医生仓储实例指针
 * @param path       数据文件路径
 * @return 成功返回 success；参数无效或文件操作失败时返回 failure
 */
Result DoctorRepository_init(DoctorRepository *repository, const char *path) {
    Result result;

    if (repository == 0) {
        return Result_make_failure("doctor repository missing");
    }

    result = TextFileRepository_init(&repository->storage, path);
    if (result.success == 0) {
        return result;
    }

    return DoctorRepository_ensure_header(repository);
}

/**
 * @brief 加载所有医生记录到链表
 * @param repository  医生仓储实例指针
 * @param out_doctors 输出参数，存放医生数据的链表
 * @return 成功返回 success；加载失败时返回 failure
 */
Result DoctorRepository_load_all(
    DoctorRepository *repository,
    LinkedList *out_doctors
) {
    DoctorRepositoryLoadContext context;
    Result result;

    if (out_doctors == 0) {
        return Result_make_failure("doctor output list missing");
    }

    LinkedList_init(out_doctors);
    context.doctors = out_doctors;

    result = DoctorRepository_ensure_header(repository);
    if (result.success == 0) {
        return result;
    }

    result = TextFileRepository_for_each_data_line(
        &repository->storage,
        DoctorRepository_collect_line,
        &context
    );
    if (result.success == 0) {
        DoctorRepository_clear_list(out_doctors);
        return result;
    }

    return Result_make_success("doctors loaded");
}

/**
 * @brief 按医生ID查找医生
 * @param repository 医生仓储实例指针
 * @param doctor_id  要查找的医生ID
 * @param out_doctor 输出参数，存放找到的医生数据
 * @return 找到返回 success；未找到时返回 failure
 */
Result DoctorRepository_find_by_doctor_id(
    DoctorRepository *repository,
    const char *doctor_id,
    Doctor *out_doctor
) {
    DoctorRepositoryFindContext context;
    Result result;

    if (!DoctorRepository_has_text(doctor_id) || out_doctor == 0) {
        return Result_make_failure("doctor query arguments missing");
    }

    result = DoctorRepository_ensure_header(repository);
    if (result.success == 0) {
        return result;
    }

    memset(out_doctor, 0, sizeof(*out_doctor));
    context.doctor_id = doctor_id;
    context.out_doctor = out_doctor;
    context.found = 0;

    result = TextFileRepository_for_each_data_line(
        &repository->storage,
        DoctorRepository_find_line,
        &context
    );
    if (result.success == 0) {
        return result;
    }

    if (context.found == 0) {
        return Result_make_failure("doctor not found");
    }

    return Result_make_success("doctor found");
}

/**
 * @brief 按科室ID筛选医生列表
 * @param repository    医生仓储实例指针
 * @param department_id 科室ID
 * @param out_doctors   输出参数，存放匹配的医生链表
 * @return 成功返回 success；参数无效或加载失败时返回 failure
 */
Result DoctorRepository_find_by_department_id(
    DoctorRepository *repository,
    const char *department_id,
    LinkedList *out_doctors
) {
    DoctorRepositoryFilterContext context;
    Result result;

    if (!DoctorRepository_has_text(department_id) || out_doctors == 0) {
        return Result_make_failure("doctor filter arguments missing");
    }

    LinkedList_init(out_doctors);
    context.department_id = department_id;
    context.doctors = out_doctors;

    result = DoctorRepository_ensure_header(repository);
    if (result.success == 0) {
        return result;
    }

    result = TextFileRepository_for_each_data_line(
        &repository->storage,
        DoctorRepository_filter_line,
        &context
    );
    if (result.success == 0) {
        DoctorRepository_clear_list(out_doctors);
        return result;
    }

    return Result_make_success("doctors filtered");
}

/**
 * @brief 追加保存一条医生记录到文件末尾
 * @param repository 医生仓储实例指针
 * @param doctor     要保存的医生数据
 * @return 成功返回 success；数据无效或写入失败时返回 failure
 */
Result DoctorRepository_save(
    DoctorRepository *repository,
    const Doctor *doctor
) {
    char line[TEXT_FILE_REPOSITORY_LINE_CAPACITY];
    Result result = DoctorRepository_ensure_header(repository);

    if (result.success == 0) {
        return result;
    }

    result = DoctorRepository_format_line(doctor, line, sizeof(line));
    if (result.success == 0) {
        return result;
    }

    return TextFileRepository_append_line(&repository->storage, line);
}

/**
 * @brief 全量保存医生列表到文件（覆盖写入）
 * @param repository 医生仓储实例指针
 * @param doctors    要保存的医生链表
 * @return 成功返回 success；数据无效或写入失败时返回 failure
 */
Result DoctorRepository_save_all(
    DoctorRepository *repository,
    const LinkedList *doctors
) {
    LinkedListNode *current = 0;
    FILE *file = 0;
    Result result;

    if (repository == 0 || doctors == 0) {
        return Result_make_failure("doctor save all arguments missing");
    }

    if (!DoctorRepository_validate_list(doctors)) {
        return Result_make_failure("doctor list contains invalid item");
    }

    result = TextFileRepository_ensure_file_exists(&repository->storage);
    if (result.success == 0) {
        return result;
    }

    /* 以写模式打开（覆盖） */
    file = fopen(repository->storage.path, "w");
    if (file == 0) {
        return Result_make_failure("failed to rewrite doctor repository");
    }

    /* 写入表头 */
    if (fprintf(file, "%s\n", DOCTOR_REPOSITORY_HEADER) < 0) {
        fclose(file);
        return Result_make_failure("failed to write doctor header");
    }

    /* 逐个写入医生记录 */
    current = doctors->head;
    while (current != 0) {
        char line[TEXT_FILE_REPOSITORY_LINE_CAPACITY];
        result = DoctorRepository_format_line(
            (const Doctor *)current->data,
            line,
            sizeof(line)
        );
        if (result.success == 0) {
            fclose(file);
            return result;
        }

        if (fprintf(file, "%s\n", line) < 0) {
            fclose(file);
            return Result_make_failure("failed to write doctor line");
        }

        current = current->next;
    }

    fclose(file);
    return Result_make_success("doctors saved");
}

/**
 * @brief 清空并释放医生链表中的所有元素
 * @param doctors 要清空的医生链表
 */
void DoctorRepository_clear_list(LinkedList *doctors) {
    LinkedList_clear(doctors, DoctorRepository_free_item);
}
