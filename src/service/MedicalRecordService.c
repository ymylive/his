/**
 * @file MedicalRecordService.c
 * @brief 病历服务模块实现
 *
 * 实现病历管理的核心业务功能，包括就诊记录和检查记录的增删改，
 * 以及患者病历历史和按时间范围查询。该服务协调挂号仓库、就诊记录仓库、
 * 检查记录仓库和住院记录仓库，通过全量加载-修改-保存的方式维护数据一致性。
 */

#include "service/MedicalRecordService.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/IdGenerator.h"
#include "common/StringUtils.h"
#include "repository/RepositoryUtils.h"

/** @brief 就诊记录ID的前缀 */
#define MEDICAL_RECORD_VISIT_ID_PREFIX "VIS"

/** @brief 就诊记录ID序号部分的位宽 */
#define MEDICAL_RECORD_VISIT_ID_WIDTH 4

/** @brief 检查记录ID的前缀 */
#define MEDICAL_RECORD_EXAM_ID_PREFIX "EXM"

/** @brief 检查记录ID序号部分的位宽 */
#define MEDICAL_RECORD_EXAM_ID_WIDTH 4

/**
 * @brief 校验文本字段
 *
 * 根据 allow_empty 参数决定是否允许空值，同时检查保留字符。
 *
 * @param text         待校验的文本
 * @param field_name   字段名称（用于错误消息）
 * @param allow_empty  是否允许空白（1=允许，0=不允许）
 * @return Result      校验结果
 */
static Result MedicalRecordService_validate_text(
    const char *text,
    const char *field_name,
    int allow_empty
) {
    char message[RESULT_MESSAGE_CAPACITY];

    if (!allow_empty && StringUtils_is_blank(text)) {
        snprintf(message, sizeof(message), "%s missing", field_name);
        return Result_make_failure(message);
    }

    if (text != 0 && !RepositoryUtils_is_safe_field_text(text)) {
        snprintf(message, sizeof(message), "%s contains reserved characters", field_name);
        return Result_make_failure(message);
    }

    return Result_make_success("medical record text valid");
}

/**
 * @brief 校验就诊记录的标志位（是否需要检查/住院/开药）
 *
 * 每个标志位只允许为 0 或 1。
 *
 * @param need_exam       是否需要检查
 * @param need_admission  是否需要住院
 * @param need_medicine   是否需要开药
 * @return Result         校验结果
 */
static Result MedicalRecordService_validate_flags(
    int need_exam,
    int need_admission,
    int need_medicine
) {
    if ((need_exam != 0 && need_exam != 1) ||
        (need_admission != 0 && need_admission != 1) ||
        (need_medicine != 0 && need_medicine != 1)) {
        return Result_make_failure("visit flags invalid");
    }

    return Result_make_success("visit flags valid");
}

/**
 * @brief 判断时间值是否在指定范围内
 *
 * 使用字符串比较来判断时间先后（要求时间格式一致，如 ISO 8601）。
 *
 * @param value      待检查的时间值
 * @param time_from  起始时间（空白表示不限制起始）
 * @param time_to    结束时间（空白表示不限制结束）
 * @return int       1=在范围内，0=不在范围内
 */
static int MedicalRecordService_is_in_time_range(
    const char *value,
    const char *time_from,
    const char *time_to
) {
    /* 空白时间值不在任何范围内 */
    if (StringUtils_is_blank(value)) {
        return 0;
    }

    /* 检查是否早于起始时间 */
    if (!StringUtils_is_blank(time_from) && strcmp(value, time_from) < 0) {
        return 0;
    }

    /* 检查是否晚于结束时间 */
    if (!StringUtils_is_blank(time_to) && strcmp(value, time_to) > 0) {
        return 0;
    }

    return 1;
}

/**
 * @brief 从挂号仓库加载所有挂号记录
 *
 * @param service            指向病历服务结构体
 * @param out_registrations  输出参数，挂号记录链表
 * @return Result            操作结果
 */
static Result MedicalRecordService_load_registrations(
    MedicalRecordService *service,
    LinkedList *out_registrations
) {
    LinkedList_init(out_registrations);
    return RegistrationRepository_load_all(&service->registration_repository, out_registrations);
}

/**
 * @brief 从就诊记录仓库加载所有就诊记录
 *
 * @param service     指向病历服务结构体
 * @param out_visits  输出参数，就诊记录链表
 * @return Result     操作结果
 */
static Result MedicalRecordService_load_visits(
    MedicalRecordService *service,
    LinkedList *out_visits
) {
    LinkedList_init(out_visits);
    return VisitRecordRepository_load_all(&service->visit_repository, out_visits);
}

/**
 * @brief 从检查记录仓库加载所有检查记录
 *
 * @param service           指向病历服务结构体
 * @param out_examinations  输出参数，检查记录链表
 * @return Result           操作结果
 */
static Result MedicalRecordService_load_examinations(
    MedicalRecordService *service,
    LinkedList *out_examinations
) {
    LinkedList_init(out_examinations);
    return ExaminationRecordRepository_load_all(
        &service->examination_repository,
        out_examinations
    );
}

/**
 * @brief 从住院记录仓库加载所有住院记录
 *
 * @param service         指向病历服务结构体
 * @param out_admissions  输出参数，住院记录链表
 * @return Result         操作结果
 */
static Result MedicalRecordService_load_admissions(
    MedicalRecordService *service,
    LinkedList *out_admissions
) {
    LinkedList_init(out_admissions);
    return AdmissionRepository_load_all(&service->admission_repository, out_admissions);
}

/**
 * @brief 在挂号记录链表中按ID查找挂号记录
 *
 * @param registrations    挂号记录链表
 * @param registration_id  待查找的挂号ID
 * @return Registration*   找到时返回指针，未找到返回 NULL
 */
static Registration *MedicalRecordService_find_registration(
    LinkedList *registrations,
    const char *registration_id
) {
    LinkedListNode *current = 0;

    if (registrations == 0 || registration_id == 0) {
        return 0;
    }

    current = registrations->head;
    while (current != 0) {
        Registration *registration = (Registration *)current->data;

        if (strcmp(registration->registration_id, registration_id) == 0) {
            return registration;
        }

        current = current->next;
    }

    return 0;
}

/**
 * @brief 在就诊记录链表中按就诊ID查找就诊记录
 *
 * @param visits    就诊记录链表
 * @param visit_id  待查找的就诊ID
 * @return VisitRecord*  找到时返回指针，未找到返回 NULL
 */
static VisitRecord *MedicalRecordService_find_visit(
    LinkedList *visits,
    const char *visit_id
) {
    LinkedListNode *current = 0;

    if (visits == 0 || visit_id == 0) {
        return 0;
    }

    current = visits->head;
    while (current != 0) {
        VisitRecord *record = (VisitRecord *)current->data;

        if (strcmp(record->visit_id, visit_id) == 0) {
            return record;
        }

        current = current->next;
    }

    return 0;
}

/**
 * @brief 在就诊记录链表中按挂号ID查找就诊记录
 *
 * 每个挂号只能有一条就诊记录，此函数用于检查重复创建。
 *
 * @param visits           就诊记录链表
 * @param registration_id  待查找的挂号ID
 * @return VisitRecord*    找到时返回指针，未找到返回 NULL
 */
static VisitRecord *MedicalRecordService_find_visit_by_registration(
    LinkedList *visits,
    const char *registration_id
) {
    LinkedListNode *current = 0;

    if (visits == 0 || registration_id == 0) {
        return 0;
    }

    current = visits->head;
    while (current != 0) {
        VisitRecord *record = (VisitRecord *)current->data;

        if (strcmp(record->registration_id, registration_id) == 0) {
            return record;
        }

        current = current->next;
    }

    return 0;
}

/**
 * @brief 在检查记录链表中按ID查找检查记录
 *
 * @param examinations    检查记录链表
 * @param examination_id  待查找的检查ID
 * @return ExaminationRecord*  找到时返回指针，未找到返回 NULL
 */
static ExaminationRecord *MedicalRecordService_find_examination(
    LinkedList *examinations,
    const char *examination_id
) {
    LinkedListNode *current = 0;

    if (examinations == 0 || examination_id == 0) {
        return 0;
    }

    current = examinations->head;
    while (current != 0) {
        ExaminationRecord *record = (ExaminationRecord *)current->data;

        if (strcmp(record->examination_id, examination_id) == 0) {
            return record;
        }

        current = current->next;
    }

    return 0;
}

/**
 * @brief 将挂号记录的副本追加到目标链表
 *
 * @param target        目标链表
 * @param registration  待复制的挂号记录
 * @return Result       操作结果
 */
static Result MedicalRecordService_append_registration_copy(
    LinkedList *target,
    const Registration *registration
) {
    Registration *copy = (Registration *)malloc(sizeof(*copy));

    if (copy == 0) {
        return Result_make_failure("failed to allocate registration history");
    }

    *copy = *registration;
    if (!LinkedList_append(target, copy)) {
        free(copy);
        return Result_make_failure("failed to append registration history");
    }

    return Result_make_success("registration appended");
}

/**
 * @brief 将就诊记录的副本追加到目标链表
 *
 * @param target  目标链表
 * @param record  待复制的就诊记录
 * @return Result 操作结果
 */
static Result MedicalRecordService_append_visit_copy(
    LinkedList *target,
    const VisitRecord *record
) {
    VisitRecord *copy = (VisitRecord *)malloc(sizeof(*copy));

    if (copy == 0) {
        return Result_make_failure("failed to allocate visit history");
    }

    *copy = *record;
    if (!LinkedList_append(target, copy)) {
        free(copy);
        return Result_make_failure("failed to append visit history");
    }

    return Result_make_success("visit appended");
}

/**
 * @brief 将检查记录的副本追加到目标链表
 *
 * @param target  目标链表
 * @param record  待复制的检查记录
 * @return Result 操作结果
 */
static Result MedicalRecordService_append_examination_copy(
    LinkedList *target,
    const ExaminationRecord *record
) {
    ExaminationRecord *copy = (ExaminationRecord *)malloc(sizeof(*copy));

    if (copy == 0) {
        return Result_make_failure("failed to allocate exam history");
    }

    *copy = *record;
    if (!LinkedList_append(target, copy)) {
        free(copy);
        return Result_make_failure("failed to append exam history");
    }

    return Result_make_success("exam appended");
}

/**
 * @brief 将住院记录的副本追加到目标链表
 *
 * @param target     目标链表
 * @param admission  待复制的住院记录
 * @return Result    操作结果
 */
static Result MedicalRecordService_append_admission_copy(
    LinkedList *target,
    const Admission *admission
) {
    Admission *copy = (Admission *)malloc(sizeof(*copy));

    if (copy == 0) {
        return Result_make_failure("failed to allocate admission history");
    }

    *copy = *admission;
    if (!LinkedList_append(target, copy)) {
        free(copy);
        return Result_make_failure("failed to append admission history");
    }

    return Result_make_success("admission appended");
}

/**
 * @brief 从链表中移除指定节点
 *
 * 将节点从链表中断开，释放节点结构本身，返回节点中的数据指针
 * （由调用方决定是否释放数据）。
 *
 * @param list      目标链表
 * @param previous  待移除节点的前一个节点（头节点时为 NULL）
 * @param current   待移除的节点
 * @return void*    被移除节点中的数据指针
 */
static void *MedicalRecordService_remove_node(
    LinkedList *list,
    LinkedListNode *previous,
    LinkedListNode *current
) {
    void *data = 0;

    if (list == 0 || current == 0) {
        return 0;
    }

    data = current->data;
    /* 调整前后指针 */
    if (previous == 0) {
        list->head = current->next;  /* 移除的是头节点 */
    } else {
        previous->next = current->next;
    }

    /* 更新尾指针 */
    if (list->tail == current) {
        list->tail = previous;
    }

    if (list->count > 0) {
        list->count--;
    }

    free(current);
    return data;
}

/**
 * @brief 从已加载的就诊记录列表中计算下一个序列号
 *
 * @param visits         已加载的就诊记录链表
 * @param out_sequence   输出参数，下一个可用序列号
 * @return Result        操作结果
 */
static Result MedicalRecordService_next_visit_sequence_from_list(
    const LinkedList *visits,
    int *out_sequence
) {
    LinkedListNode *current = 0;
    int max_sequence = 0;

    if (out_sequence == 0) {
        return Result_make_failure("visit sequence output missing");
    }

    /* 遍历所有就诊记录，提取ID中的序列号并找到最大值 */
    current = visits->head;
    while (current != 0) {
        const VisitRecord *record = (const VisitRecord *)current->data;
        const char *suffix = record->visit_id + strlen(MEDICAL_RECORD_VISIT_ID_PREFIX);
        char *end_pointer = 0;
        long value = 0;

        if (strncmp(
                record->visit_id,
                MEDICAL_RECORD_VISIT_ID_PREFIX,
                strlen(MEDICAL_RECORD_VISIT_ID_PREFIX)
            ) != 0) {
            return Result_make_failure("visit id format invalid");
        }

        errno = 0;
        value = strtol(suffix, &end_pointer, 10);
        if (errno != 0 || end_pointer == suffix || end_pointer == 0 || *end_pointer != '\0') {
            return Result_make_failure("visit id format invalid");
        }

        if ((int)value > max_sequence) {
            max_sequence = (int)value;
        }

        current = current->next;
    }

    *out_sequence = max_sequence + 1;
    return Result_make_success("visit sequence ready");
}

/**
 * @brief 从存储中计算下一个就诊记录序列号
 *
 * @param service       指向病历服务结构体
 * @param out_sequence  输出参数，下一个可用序列号
 * @return Result       操作结果
 */
static Result MedicalRecordService_next_visit_sequence(
    MedicalRecordService *service,
    int *out_sequence
) {
    LinkedList visits;
    LinkedListNode *current = 0;
    int max_sequence = 0;
    Result result;

    if (out_sequence == 0) {
        return Result_make_failure("visit sequence output missing");
    }

    result = MedicalRecordService_load_visits(service, &visits);
    if (result.success == 0) {
        return result;
    }

    /* 遍历所有就诊记录提取最大序列号 */
    current = visits.head;
    while (current != 0) {
        const VisitRecord *record = (const VisitRecord *)current->data;
        const char *suffix = record->visit_id + strlen(MEDICAL_RECORD_VISIT_ID_PREFIX);
        char *end_pointer = 0;
        long value = 0;

        if (strncmp(
                record->visit_id,
                MEDICAL_RECORD_VISIT_ID_PREFIX,
                strlen(MEDICAL_RECORD_VISIT_ID_PREFIX)
            ) != 0) {
            VisitRecordRepository_clear_list(&visits);
            return Result_make_failure("visit id format invalid");
        }

        errno = 0;
        value = strtol(suffix, &end_pointer, 10);
        if (errno != 0 || end_pointer == suffix || end_pointer == 0 || *end_pointer != '\0') {
            VisitRecordRepository_clear_list(&visits);
            return Result_make_failure("visit id format invalid");
        }

        if ((int)value > max_sequence) {
            max_sequence = (int)value;
        }

        current = current->next;
    }

    VisitRecordRepository_clear_list(&visits);
    *out_sequence = max_sequence + 1;
    return Result_make_success("visit sequence ready");
}

/**
 * @brief 从已加载的检查记录列表中计算下一个序列号
 *
 * @param examinations   已加载的检查记录链表
 * @param out_sequence   输出参数，下一个可用序列号
 * @return Result        操作结果
 */
static Result MedicalRecordService_next_examination_sequence_from_list(
    const LinkedList *examinations,
    int *out_sequence
) {
    LinkedListNode *current = 0;
    int max_sequence = 0;

    if (out_sequence == 0) {
        return Result_make_failure("exam sequence output missing");
    }

    /* 遍历所有检查记录，提取ID中的序列号并找到最大值 */
    current = examinations->head;
    while (current != 0) {
        const ExaminationRecord *record = (const ExaminationRecord *)current->data;
        const char *suffix = record->examination_id + strlen(MEDICAL_RECORD_EXAM_ID_PREFIX);
        char *end_pointer = 0;
        long value = 0;

        if (strncmp(
                record->examination_id,
                MEDICAL_RECORD_EXAM_ID_PREFIX,
                strlen(MEDICAL_RECORD_EXAM_ID_PREFIX)
            ) != 0) {
            return Result_make_failure("exam id format invalid");
        }

        errno = 0;
        value = strtol(suffix, &end_pointer, 10);
        if (errno != 0 || end_pointer == suffix || end_pointer == 0 || *end_pointer != '\0') {
            return Result_make_failure("exam id format invalid");
        }

        if ((int)value > max_sequence) {
            max_sequence = (int)value;
        }

        current = current->next;
    }

    *out_sequence = max_sequence + 1;
    return Result_make_success("exam sequence ready");
}

/**
 * @brief 从存储中计算下一个检查记录序列号
 *
 * @param service       指向病历服务结构体
 * @param out_sequence  输出参数，下一个可用序列号
 * @return Result       操作结果
 */
static Result MedicalRecordService_next_examination_sequence(
    MedicalRecordService *service,
    int *out_sequence
) {
    LinkedList examinations;
    LinkedListNode *current = 0;
    int max_sequence = 0;
    Result result;

    if (out_sequence == 0) {
        return Result_make_failure("exam sequence output missing");
    }

    result = MedicalRecordService_load_examinations(service, &examinations);
    if (result.success == 0) {
        return result;
    }

    /* 遍历所有检查记录提取最大序列号 */
    current = examinations.head;
    while (current != 0) {
        const ExaminationRecord *record = (const ExaminationRecord *)current->data;
        const char *suffix = record->examination_id + strlen(MEDICAL_RECORD_EXAM_ID_PREFIX);
        char *end_pointer = 0;
        long value = 0;

        if (strncmp(
                record->examination_id,
                MEDICAL_RECORD_EXAM_ID_PREFIX,
                strlen(MEDICAL_RECORD_EXAM_ID_PREFIX)
            ) != 0) {
            ExaminationRecordRepository_clear_list(&examinations);
            return Result_make_failure("exam id format invalid");
        }

        errno = 0;
        value = strtol(suffix, &end_pointer, 10);
        if (errno != 0 || end_pointer == suffix || end_pointer == 0 || *end_pointer != '\0') {
            ExaminationRecordRepository_clear_list(&examinations);
            return Result_make_failure("exam id format invalid");
        }

        if ((int)value > max_sequence) {
            max_sequence = (int)value;
        }

        current = current->next;
    }

    ExaminationRecordRepository_clear_list(&examinations);
    *out_sequence = max_sequence + 1;
    return Result_make_success("exam sequence ready");
}

/**
 * @brief 初始化病历历史结构体
 *
 * 将结构体中的四个链表全部初始化为空。
 *
 * @param history  指向待初始化的病历历史结构体
 */
static void MedicalRecordHistory_init(MedicalRecordHistory *history) {
    LinkedList_init(&history->registrations);
    LinkedList_init(&history->visits);
    LinkedList_init(&history->examinations);
    LinkedList_init(&history->admissions);
}

/**
 * @brief 初始化病历服务
 *
 * 分别初始化挂号、就诊、检查和住院记录仓库，
 * 并确保各存储文件就绪且可正常加载。
 *
 * @param service            指向待初始化的病历服务结构体
 * @param registration_path  挂号数据文件路径
 * @param visit_path         就诊记录数据文件路径
 * @param examination_path   检查记录数据文件路径
 * @param admission_path     住院记录数据文件路径
 * @return Result            操作结果
 */
Result MedicalRecordService_init(
    MedicalRecordService *service,
    const char *registration_path,
    const char *visit_path,
    const char *examination_path,
    const char *admission_path
) {
    LinkedList admissions;
    Result result;

    if (service == 0) {
        return Result_make_failure("medical record service missing");
    }

    /* 依次初始化四个仓库 */
    result = RegistrationRepository_init(&service->registration_repository, registration_path);
    if (result.success == 0) {
        return result;
    }

    result = VisitRecordRepository_init(&service->visit_repository, visit_path);
    if (result.success == 0) {
        return result;
    }

    result = ExaminationRecordRepository_init(
        &service->examination_repository,
        examination_path
    );
    if (result.success == 0) {
        return result;
    }

    result = AdmissionRepository_init(&service->admission_repository, admission_path);
    if (result.success == 0) {
        return result;
    }

    /* 确保各存储文件就绪 */
    result = RegistrationRepository_ensure_storage(&service->registration_repository);
    if (result.success == 0) {
        return result;
    }

    result = VisitRecordRepository_ensure_storage(&service->visit_repository);
    if (result.success == 0) {
        return result;
    }

    result = ExaminationRecordRepository_ensure_storage(&service->examination_repository);
    if (result.success == 0) {
        return result;
    }

    /* 验证住院记录仓库可正常加载 */
    LinkedList_init(&admissions);
    result = AdmissionRepository_load_all(&service->admission_repository, &admissions);
    if (result.success == 0) {
        return result;
    }

    AdmissionRepository_clear_loaded_list(&admissions);
    return Result_make_success("medical record service ready");
}

/**
 * @brief 清理病历历史记录
 *
 * 释放 MedicalRecordHistory 中所有链表的动态分配内存。
 *
 * @param history  指向待清理的病历历史结构体
 */
void MedicalRecordHistory_clear(MedicalRecordHistory *history) {
    if (history == 0) {
        return;
    }

    RegistrationRepository_clear_list(&history->registrations);
    VisitRecordRepository_clear_list(&history->visits);
    ExaminationRecordRepository_clear_list(&history->examinations);
    AdmissionRepository_clear_loaded_list(&history->admissions);
}

/**
 * @brief 构建就诊记录结构体
 *
 * 校验各文本字段和标志位后，从挂号记录中提取关联信息填充就诊记录。
 *
 * @param record           输出参数，待填充的就诊记录
 * @param visit_id         就诊ID
 * @param registration     关联的挂号记录
 * @param chief_complaint  主诉（可为空）
 * @param diagnosis        诊断（可为空）
 * @param advice           医嘱建议（可为空）
 * @param need_exam        是否需要检查
 * @param need_admission   是否需要住院
 * @param need_medicine    是否需要开药
 * @param visit_time       就诊时间
 * @return Result          操作结果
 */
static Result MedicalRecordService_build_visit_record(
    VisitRecord *record,
    const char *visit_id,
    const Registration *registration,
    const char *chief_complaint,
    const char *diagnosis,
    const char *advice,
    int need_exam,
    int need_admission,
    int need_medicine,
    const char *visit_time
) {
    Result result;

    /* 校验各文本字段 */
    result = MedicalRecordService_validate_text(chief_complaint, "chief complaint", 1);
    if (result.success == 0) {
        return result;
    }

    result = MedicalRecordService_validate_text(diagnosis, "diagnosis", 1);
    if (result.success == 0) {
        return result;
    }

    result = MedicalRecordService_validate_text(advice, "advice", 1);
    if (result.success == 0) {
        return result;
    }

    result = MedicalRecordService_validate_text(visit_time, "visit time", 0);
    if (result.success == 0) {
        return result;
    }

    /* 校验标志位 */
    result = MedicalRecordService_validate_flags(need_exam, need_admission, need_medicine);
    if (result.success == 0) {
        return result;
    }

    /* 填充就诊记录各字段 */
    memset(record, 0, sizeof(*record));
    StringUtils_copy(record->visit_id, sizeof(record->visit_id), visit_id);
    StringUtils_copy(
        record->registration_id,
        sizeof(record->registration_id),
        registration->registration_id
    );
    StringUtils_copy(record->patient_id, sizeof(record->patient_id), registration->patient_id);
    StringUtils_copy(record->doctor_id, sizeof(record->doctor_id), registration->doctor_id);
    StringUtils_copy(
        record->department_id,
        sizeof(record->department_id),
        registration->department_id
    );
    StringUtils_copy(
        record->chief_complaint,
        sizeof(record->chief_complaint),
        chief_complaint
    );
    StringUtils_copy(record->diagnosis, sizeof(record->diagnosis), diagnosis);
    StringUtils_copy(record->advice, sizeof(record->advice), advice);
    record->need_exam = need_exam;
    record->need_admission = need_admission;
    record->need_medicine = need_medicine;
    StringUtils_copy(record->visit_time, sizeof(record->visit_time), visit_time);
    return Result_make_success("visit record built");
}

/**
 * @brief 创建就诊记录
 *
 * 流程：校验参数 -> 加载挂号和就诊记录 -> 查找挂号记录 ->
 * 检查挂号未取消且无重复就诊 -> 生成就诊ID -> 构建就诊记录 ->
 * 追加到链表 -> 将挂号标记为已诊 -> 保存就诊和挂号数据。
 *
 * @param service          指向病历服务结构体
 * @param registration_id  关联的挂号ID
 * @param chief_complaint  主诉
 * @param diagnosis        诊断
 * @param advice           医嘱建议
 * @param need_exam        是否需要检查
 * @param need_admission   是否需要住院
 * @param need_medicine    是否需要开药
 * @param visit_time       就诊时间
 * @param out_record       输出参数，创建成功时存放就诊记录
 * @return Result          操作结果
 */
Result MedicalRecordService_create_visit_record(
    MedicalRecordService *service,
    const char *registration_id,
    const char *chief_complaint,
    const char *diagnosis,
    const char *advice,
    int need_exam,
    int need_admission,
    int need_medicine,
    const char *visit_time,
    VisitRecord *out_record
) {
    LinkedList registrations;
    LinkedList visits;
    Registration *registration = 0;
    VisitRecord *existing = 0;
    VisitRecord *created = 0;
    VisitRecord new_record;
    char generated_visit_id[HIS_DOMAIN_ID_CAPACITY];
    int next_sequence = 0;
    Result result;

    if (service == 0 || out_record == 0) {
        return Result_make_failure("visit create arguments invalid");
    }

    result = MedicalRecordService_validate_text(registration_id, "registration id", 0);
    if (result.success == 0) {
        return result;
    }

    /* 加载挂号和就诊记录 */
    result = MedicalRecordService_load_registrations(service, &registrations);
    if (result.success == 0) {
        return result;
    }

    result = MedicalRecordService_load_visits(service, &visits);
    if (result.success == 0) {
        RegistrationRepository_clear_list(&registrations);
        return result;
    }

    /* 查找关联的挂号记录 */
    registration = MedicalRecordService_find_registration(&registrations, registration_id);
    if (registration == 0) {
        VisitRecordRepository_clear_list(&visits);
        RegistrationRepository_clear_list(&registrations);
        return Result_make_failure("registration not found");
    }

    /* 检查挂号是否已取消 */
    if (registration->status == REG_STATUS_CANCELLED) {
        VisitRecordRepository_clear_list(&visits);
        RegistrationRepository_clear_list(&registrations);
        return Result_make_failure("registration cancelled");
    }

    /* 检查该挂号是否已有就诊记录（每个挂号只能有一条） */
    existing = MedicalRecordService_find_visit_by_registration(&visits, registration_id);
    if (existing != 0) {
        VisitRecordRepository_clear_list(&visits);
        RegistrationRepository_clear_list(&registrations);
        return Result_make_failure("registration already has visit record");
    }

    /* 计算下一个就诊序列号 */
    result = MedicalRecordService_next_visit_sequence_from_list(&visits, &next_sequence);
    if (result.success == 0) {
        VisitRecordRepository_clear_list(&visits);
        RegistrationRepository_clear_list(&registrations);
        return result;
    }

    /* 生成格式化的就诊ID（如 VIS0001） */
    if (!IdGenerator_format(
            generated_visit_id,
            sizeof(generated_visit_id),
            MEDICAL_RECORD_VISIT_ID_PREFIX,
            next_sequence,
            MEDICAL_RECORD_VISIT_ID_WIDTH)) {
        VisitRecordRepository_clear_list(&visits);
        RegistrationRepository_clear_list(&registrations);
        return Result_make_failure("failed to generate visit id");
    }

    /* 构建就诊记录 */
    result = MedicalRecordService_build_visit_record(
        &new_record,
        generated_visit_id,
        registration,
        chief_complaint,
        diagnosis,
        advice,
        need_exam,
        need_admission,
        need_medicine,
        visit_time
    );
    if (result.success == 0) {
        VisitRecordRepository_clear_list(&visits);
        RegistrationRepository_clear_list(&registrations);
        return result;
    }

    /* 分配副本并追加到链表 */
    created = (VisitRecord *)malloc(sizeof(*created));
    if (created == 0) {
        VisitRecordRepository_clear_list(&visits);
        RegistrationRepository_clear_list(&registrations);
        return Result_make_failure("failed to allocate visit record");
    }

    *created = new_record;
    if (!LinkedList_append(&visits, created)) {
        free(created);
        VisitRecordRepository_clear_list(&visits);
        RegistrationRepository_clear_list(&registrations);
        return Result_make_failure("failed to append visit record");
    }

    /* 将挂号状态从待诊标记为已诊 */
    if (registration->status == REG_STATUS_PENDING &&
        !Registration_mark_diagnosed(registration, visit_time)) {
        VisitRecordRepository_clear_list(&visits);
        RegistrationRepository_clear_list(&registrations);
        return Result_make_failure("failed to mark registration diagnosed");
    }

    /* 保存就诊记录和挂号记录 */
    result = VisitRecordRepository_save_all(&service->visit_repository, &visits);
    if (result.success == 0) {
        VisitRecordRepository_clear_list(&visits);
        RegistrationRepository_clear_list(&registrations);
        return result;
    }

    result = RegistrationRepository_save_all(&service->registration_repository, &registrations);
    if (result.success == 0) {
        VisitRecordRepository_clear_list(&visits);
        RegistrationRepository_clear_list(&registrations);
        return result;
    }

    *out_record = new_record;
    VisitRecordRepository_clear_list(&visits);
    RegistrationRepository_clear_list(&registrations);
    return Result_make_success("visit record created");
}

/**
 * @brief 更新就诊记录
 *
 * 流程：校验参数 -> 加载挂号和就诊记录 -> 查找目标就诊记录 ->
 * 查找关联挂号记录 -> 构建更新后的记录 -> 覆盖旧记录 -> 保存。
 *
 * @param service          指向病历服务结构体
 * @param visit_id         待更新的就诊ID
 * @param chief_complaint  主诉
 * @param diagnosis        诊断
 * @param advice           医嘱建议
 * @param need_exam        是否需要检查
 * @param need_admission   是否需要住院
 * @param need_medicine    是否需要开药
 * @param visit_time       就诊时间
 * @param out_record       输出参数，更新成功时存放就诊记录
 * @return Result          操作结果
 */
Result MedicalRecordService_update_visit_record(
    MedicalRecordService *service,
    const char *visit_id,
    const char *chief_complaint,
    const char *diagnosis,
    const char *advice,
    int need_exam,
    int need_admission,
    int need_medicine,
    const char *visit_time,
    VisitRecord *out_record
) {
    LinkedList registrations;
    LinkedList visits;
    VisitRecord *existing = 0;
    Registration *registration = 0;
    VisitRecord updated_record;
    Result result;

    if (service == 0 || out_record == 0) {
        return Result_make_failure("visit update arguments invalid");
    }

    result = MedicalRecordService_validate_text(visit_id, "visit id", 0);
    if (result.success == 0) {
        return result;
    }

    /* 加载挂号和就诊记录 */
    result = MedicalRecordService_load_registrations(service, &registrations);
    if (result.success == 0) {
        return result;
    }

    result = MedicalRecordService_load_visits(service, &visits);
    if (result.success == 0) {
        RegistrationRepository_clear_list(&registrations);
        return result;
    }

    /* 查找目标就诊记录 */
    existing = MedicalRecordService_find_visit(&visits, visit_id);
    if (existing == 0) {
        VisitRecordRepository_clear_list(&visits);
        RegistrationRepository_clear_list(&registrations);
        return Result_make_failure("visit not found");
    }

    /* 查找关联的挂号记录（用于获取患者/医生/科室信息） */
    registration = MedicalRecordService_find_registration(&registrations, existing->registration_id);
    if (registration == 0) {
        VisitRecordRepository_clear_list(&visits);
        RegistrationRepository_clear_list(&registrations);
        return Result_make_failure("registration not found");
    }

    /* 构建更新后的就诊记录 */
    result = MedicalRecordService_build_visit_record(
        &updated_record,
        visit_id,
        registration,
        chief_complaint,
        diagnosis,
        advice,
        need_exam,
        need_admission,
        need_medicine,
        visit_time
    );
    if (result.success == 0) {
        VisitRecordRepository_clear_list(&visits);
        RegistrationRepository_clear_list(&registrations);
        return result;
    }

    /* 覆盖旧记录并保存 */
    *existing = updated_record;
    result = VisitRecordRepository_save_all(&service->visit_repository, &visits);
    VisitRecordRepository_clear_list(&visits);
    RegistrationRepository_clear_list(&registrations);
    if (result.success == 0) {
        return result;
    }

    *out_record = updated_record;
    return Result_make_success("visit record updated");
}

/**
 * @brief 删除就诊记录
 *
 * 流程：校验参数 -> 加载挂号和就诊记录 -> 查找目标记录 ->
 * 检查是否有关联的检查记录（有则不允许删除） ->
 * 将关联挂号状态回退为待诊 -> 从链表中移除 -> 保存。
 *
 * @param service   指向病历服务结构体
 * @param visit_id  待删除的就诊ID
 * @return Result   操作结果
 */
Result MedicalRecordService_delete_visit_record(
    MedicalRecordService *service,
    const char *visit_id
) {
    LinkedList registrations;
    LinkedList visits;
    LinkedList examinations;
    LinkedListNode *previous = 0;
    LinkedListNode *current = 0;
    VisitRecord *target = 0;
    Registration *registration = 0;
    Result result;

    if (service == 0) {
        return Result_make_failure("visit delete arguments invalid");
    }

    result = MedicalRecordService_validate_text(visit_id, "visit id", 0);
    if (result.success == 0) {
        return result;
    }

    /* 加载挂号和就诊记录 */
    result = MedicalRecordService_load_registrations(service, &registrations);
    if (result.success == 0) {
        return result;
    }

    result = MedicalRecordService_load_visits(service, &visits);
    if (result.success == 0) {
        RegistrationRepository_clear_list(&registrations);
        return result;
    }

    /* 在链表中查找目标就诊记录（同时记录前驱节点用于删除） */
    current = visits.head;
    while (current != 0) {
        VisitRecord *record = (VisitRecord *)current->data;

        if (strcmp(record->visit_id, visit_id) == 0) {
            target = record;
            break;
        }

        previous = current;
        current = current->next;
    }

    if (target == 0) {
        VisitRecordRepository_clear_list(&visits);
        RegistrationRepository_clear_list(&registrations);
        return Result_make_failure("visit not found");
    }

    /* 检查该就诊是否有关联的检查记录 */
    LinkedList_init(&examinations);
    result = ExaminationRecordRepository_find_by_visit_id(
        &service->examination_repository,
        visit_id,
        &examinations
    );
    if (result.success == 0) {
        VisitRecordRepository_clear_list(&visits);
        RegistrationRepository_clear_list(&registrations);
        return result;
    }

    /* 如果有检查记录则不允许删除 */
    if (LinkedList_count(&examinations) != 0) {
        ExaminationRecordRepository_clear_list(&examinations);
        VisitRecordRepository_clear_list(&visits);
        RegistrationRepository_clear_list(&registrations);
        return Result_make_failure("visit has examination records");
    }

    ExaminationRecordRepository_clear_list(&examinations);

    /* 将关联挂号状态回退为待诊 */
    registration = MedicalRecordService_find_registration(&registrations, target->registration_id);
    if (registration != 0) {
        registration->status = REG_STATUS_PENDING;
        registration->diagnosed_at[0] = '\0';
        registration->cancelled_at[0] = '\0';
    }

    /* 从链表中移除目标就诊记录并释放内存 */
    free(MedicalRecordService_remove_node(&visits, previous, current));

    /* 保存就诊和挂号数据 */
    result = VisitRecordRepository_save_all(&service->visit_repository, &visits);
    if (result.success == 0) {
        VisitRecordRepository_clear_list(&visits);
        RegistrationRepository_clear_list(&registrations);
        return result;
    }

    result = RegistrationRepository_save_all(&service->registration_repository, &registrations);
    VisitRecordRepository_clear_list(&visits);
    RegistrationRepository_clear_list(&registrations);
    if (result.success == 0) {
        return result;
    }

    return Result_make_success("visit record deleted");
}

/**
 * @brief 创建检查记录
 *
 * 流程：校验参数 -> 加载就诊和检查记录 -> 验证就诊记录存在 ->
 * 生成检查ID -> 填充检查记录 -> 追加到链表 -> 保存。
 *
 * @param service       指向病历服务结构体
 * @param visit_id      关联的就诊ID
 * @param exam_item     检查项目名称
 * @param exam_type     检查类型
 * @param requested_at  申请检查的时间
 * @param out_record    输出参数，创建成功时存放检查记录
 * @return Result       操作结果
 */
Result MedicalRecordService_create_examination_record(
    MedicalRecordService *service,
    const char *visit_id,
    const char *exam_item,
    const char *exam_type,
    const char *requested_at,
    ExaminationRecord *out_record
) {
    LinkedList visits;
    LinkedList examinations;
    VisitRecord *visit = 0;
    ExaminationRecord *created = 0;
    ExaminationRecord new_record;
    int next_sequence = 0;
    Result result;

    if (service == 0 || out_record == 0) {
        return Result_make_failure("exam create arguments invalid");
    }

    /* 校验各文本参数 */
    result = MedicalRecordService_validate_text(visit_id, "visit id", 0);
    if (result.success == 0) {
        return result;
    }

    result = MedicalRecordService_validate_text(exam_item, "exam item", 0);
    if (result.success == 0) {
        return result;
    }

    result = MedicalRecordService_validate_text(exam_type, "exam type", 0);
    if (result.success == 0) {
        return result;
    }

    result = MedicalRecordService_validate_text(requested_at, "requested at", 0);
    if (result.success == 0) {
        return result;
    }

    /* 加载就诊和检查记录 */
    result = MedicalRecordService_load_visits(service, &visits);
    if (result.success == 0) {
        return result;
    }

    result = MedicalRecordService_load_examinations(service, &examinations);
    if (result.success == 0) {
        VisitRecordRepository_clear_list(&visits);
        return result;
    }

    /* 验证关联的就诊记录存在 */
    visit = MedicalRecordService_find_visit(&visits, visit_id);
    if (visit == 0) {
        ExaminationRecordRepository_clear_list(&examinations);
        VisitRecordRepository_clear_list(&visits);
        return Result_make_failure("visit not found");
    }

    /* 计算下一个检查序列号 */
    result = MedicalRecordService_next_examination_sequence_from_list(&examinations, &next_sequence);
    if (result.success == 0) {
        ExaminationRecordRepository_clear_list(&examinations);
        VisitRecordRepository_clear_list(&visits);
        return result;
    }

    /* 填充检查记录 */
    memset(&new_record, 0, sizeof(new_record));
    /* 生成格式化的检查ID（如 EXM0001） */
    if (!IdGenerator_format(
            new_record.examination_id,
            sizeof(new_record.examination_id),
            MEDICAL_RECORD_EXAM_ID_PREFIX,
            next_sequence,
            MEDICAL_RECORD_EXAM_ID_WIDTH)) {
        ExaminationRecordRepository_clear_list(&examinations);
        VisitRecordRepository_clear_list(&visits);
        return Result_make_failure("failed to generate exam id");
    }

    /* 从就诊记录中提取患者ID和医生ID */
    StringUtils_copy(new_record.visit_id, sizeof(new_record.visit_id), visit_id);
    StringUtils_copy(new_record.patient_id, sizeof(new_record.patient_id), visit->patient_id);
    StringUtils_copy(new_record.doctor_id, sizeof(new_record.doctor_id), visit->doctor_id);
    StringUtils_copy(new_record.exam_item, sizeof(new_record.exam_item), exam_item);
    StringUtils_copy(new_record.exam_type, sizeof(new_record.exam_type), exam_type);
    new_record.status = EXAM_STATUS_PENDING;  /* 初始状态为待检查 */
    new_record.result[0] = '\0';
    StringUtils_copy(
        new_record.requested_at,
        sizeof(new_record.requested_at),
        requested_at
    );
    new_record.completed_at[0] = '\0';

    /* 分配副本并追加到链表 */
    created = (ExaminationRecord *)malloc(sizeof(*created));
    if (created == 0) {
        ExaminationRecordRepository_clear_list(&examinations);
        VisitRecordRepository_clear_list(&visits);
        return Result_make_failure("failed to allocate exam record");
    }

    *created = new_record;
    if (!LinkedList_append(&examinations, created)) {
        free(created);
        ExaminationRecordRepository_clear_list(&examinations);
        VisitRecordRepository_clear_list(&visits);
        return Result_make_failure("failed to append exam record");
    }

    /* 保存检查记录 */
    result = ExaminationRecordRepository_save_all(&service->examination_repository, &examinations);
    ExaminationRecordRepository_clear_list(&examinations);
    VisitRecordRepository_clear_list(&visits);
    if (result.success == 0) {
        return result;
    }

    *out_record = new_record;
    return Result_make_success("exam record created");
}

/**
 * @brief 更新检查记录
 *
 * 流程：校验参数和状态一致性 -> 加载检查记录 -> 查找目标记录 ->
 * 更新状态、结果和完成时间 -> 保存。
 *
 * @param service         指向病历服务结构体
 * @param examination_id  检查记录ID
 * @param status          新的检查状态
 * @param result_text     检查结果（已完成时必填）
 * @param completed_at    完成时间（已完成时必填）
 * @param out_record      输出参数，更新成功时存放检查记录
 * @return Result         操作结果
 */
Result MedicalRecordService_update_examination_record(
    MedicalRecordService *service,
    const char *examination_id,
    ExaminationStatus status,
    const char *result_text,
    const char *completed_at,
    ExaminationRecord *out_record
) {
    LinkedList examinations;
    ExaminationRecord *existing = 0;
    Result result;

    if (service == 0 || out_record == 0) {
        return Result_make_failure("exam update arguments invalid");
    }

    result = MedicalRecordService_validate_text(examination_id, "examination id", 0);
    if (result.success == 0) {
        return result;
    }

    result = MedicalRecordService_validate_text(result_text, "exam result", 1);
    if (result.success == 0) {
        return result;
    }

    result = MedicalRecordService_validate_text(completed_at, "completed at", 1);
    if (result.success == 0) {
        return result;
    }

    /* 检查状态枚举值的合法性 */
    if (status != EXAM_STATUS_PENDING && status != EXAM_STATUS_COMPLETED) {
        return Result_make_failure("exam status invalid");
    }

    /* 状态和字段的一致性校验 */
    if (status == EXAM_STATUS_PENDING && !StringUtils_is_blank(completed_at)) {
        return Result_make_failure("pending exam cannot have completed_at");
    }

    if (status == EXAM_STATUS_COMPLETED && StringUtils_is_blank(completed_at)) {
        return Result_make_failure("completed exam missing completed_at");
    }

    if (status == EXAM_STATUS_COMPLETED && StringUtils_is_blank(result_text)) {
        return Result_make_failure("completed exam missing result");
    }

    /* 加载检查记录 */
    result = MedicalRecordService_load_examinations(service, &examinations);
    if (result.success == 0) {
        return result;
    }

    /* 查找目标检查记录 */
    existing = MedicalRecordService_find_examination(&examinations, examination_id);
    if (existing == 0) {
        ExaminationRecordRepository_clear_list(&examinations);
        return Result_make_failure("exam not found");
    }

    /* 更新检查记录的状态、结果和完成时间 */
    existing->status = status;
    StringUtils_copy(existing->result, sizeof(existing->result), result_text);
    StringUtils_copy(
        existing->completed_at,
        sizeof(existing->completed_at),
        status == EXAM_STATUS_COMPLETED ? completed_at : ""
    );

    /* 保存检查记录 */
    result = ExaminationRecordRepository_save_all(&service->examination_repository, &examinations);
    if (result.success == 0) {
        ExaminationRecordRepository_clear_list(&examinations);
        return result;
    }

    *out_record = *existing;
    ExaminationRecordRepository_clear_list(&examinations);
    return Result_make_success("exam record updated");
}

/**
 * @brief 删除检查记录
 *
 * 流程：校验参数 -> 加载检查记录 -> 查找目标记录 ->
 * 从链表中移除 -> 保存。
 *
 * @param service         指向病历服务结构体
 * @param examination_id  待删除的检查记录ID
 * @return Result         操作结果
 */
Result MedicalRecordService_delete_examination_record(
    MedicalRecordService *service,
    const char *examination_id
) {
    LinkedList examinations;
    LinkedListNode *previous = 0;
    LinkedListNode *current = 0;
    Result result;

    if (service == 0) {
        return Result_make_failure("exam delete arguments invalid");
    }

    result = MedicalRecordService_validate_text(examination_id, "examination id", 0);
    if (result.success == 0) {
        return result;
    }

    /* 加载检查记录 */
    result = MedicalRecordService_load_examinations(service, &examinations);
    if (result.success == 0) {
        return result;
    }

    /* 在链表中查找目标检查记录（同时记录前驱节点） */
    current = examinations.head;
    while (current != 0) {
        ExaminationRecord *record = (ExaminationRecord *)current->data;

        if (strcmp(record->examination_id, examination_id) == 0) {
            break;
        }

        previous = current;
        current = current->next;
    }

    if (current == 0) {
        ExaminationRecordRepository_clear_list(&examinations);
        return Result_make_failure("exam not found");
    }

    /* 从链表中移除并释放内存 */
    free(MedicalRecordService_remove_node(&examinations, previous, current));

    /* 保存更新后的检查记录 */
    result = ExaminationRecordRepository_save_all(&service->examination_repository, &examinations);
    ExaminationRecordRepository_clear_list(&examinations);
    if (result.success == 0) {
        return result;
    }

    return Result_make_success("exam record deleted");
}

/**
 * @brief 查询患者的完整病历历史
 *
 * 聚合该患者的所有挂号记录、就诊记录、检查记录和住院记录，
 * 分别从各仓库加载并按患者ID筛选。
 *
 * @param service      指向病历服务结构体
 * @param patient_id   患者ID
 * @param out_history  输出参数，查找成功时存放病历历史
 * @return Result      操作结果
 */
Result MedicalRecordService_find_patient_history(
    MedicalRecordService *service,
    const char *patient_id,
    MedicalRecordHistory *out_history
) {
    RegistrationRepositoryFilter filter;
    LinkedList visits;
    LinkedList examinations;
    LinkedList admissions;
    LinkedListNode *current = 0;
    Result result;

    if (service == 0 || out_history == 0) {
        return Result_make_failure("patient history arguments invalid");
    }

    result = MedicalRecordService_validate_text(patient_id, "patient id", 0);
    if (result.success == 0) {
        return result;
    }

    memset(out_history, 0, sizeof(*out_history));
    MedicalRecordHistory_init(out_history);

    /* 加载该患者的所有挂号记录（通过仓库层按患者ID筛选） */
    RegistrationRepositoryFilter_init(&filter);
    result = RegistrationRepository_find_by_patient_id(
        &service->registration_repository,
        patient_id,
        &filter,
        &out_history->registrations
    );
    if (result.success == 0) {
        MedicalRecordHistory_clear(out_history);
        return result;
    }

    /* 加载所有就诊记录，按患者ID筛选 */
    result = MedicalRecordService_load_visits(service, &visits);
    if (result.success == 0) {
        MedicalRecordHistory_clear(out_history);
        return result;
    }

    current = visits.head;
    while (current != 0) {
        const VisitRecord *record = (const VisitRecord *)current->data;

        if (strcmp(record->patient_id, patient_id) == 0) {
            result = MedicalRecordService_append_visit_copy(&out_history->visits, record);
            if (result.success == 0) {
                VisitRecordRepository_clear_list(&visits);
                MedicalRecordHistory_clear(out_history);
                return result;
            }
        }

        current = current->next;
    }
    VisitRecordRepository_clear_list(&visits);

    /* 加载所有检查记录，按患者ID筛选 */
    result = MedicalRecordService_load_examinations(service, &examinations);
    if (result.success == 0) {
        MedicalRecordHistory_clear(out_history);
        return result;
    }

    current = examinations.head;
    while (current != 0) {
        const ExaminationRecord *record = (const ExaminationRecord *)current->data;

        if (strcmp(record->patient_id, patient_id) == 0) {
            result = MedicalRecordService_append_examination_copy(
                &out_history->examinations,
                record
            );
            if (result.success == 0) {
                ExaminationRecordRepository_clear_list(&examinations);
                MedicalRecordHistory_clear(out_history);
                return result;
            }
        }

        current = current->next;
    }
    ExaminationRecordRepository_clear_list(&examinations);

    /* 加载所有住院记录，按患者ID筛选 */
    result = MedicalRecordService_load_admissions(service, &admissions);
    if (result.success == 0) {
        MedicalRecordHistory_clear(out_history);
        return result;
    }

    current = admissions.head;
    while (current != 0) {
        const Admission *admission = (const Admission *)current->data;

        if (strcmp(admission->patient_id, patient_id) == 0) {
            result = MedicalRecordService_append_admission_copy(&out_history->admissions, admission);
            if (result.success == 0) {
                AdmissionRepository_clear_loaded_list(&admissions);
                MedicalRecordHistory_clear(out_history);
                return result;
            }
        }

        current = current->next;
    }
    AdmissionRepository_clear_loaded_list(&admissions);

    return Result_make_success("patient history loaded");
}

/**
 * @brief 按时间范围查询病历记录
 *
 * 在指定时间范围内查找所有挂号、就诊、检查和住院记录，
 * 分别使用各记录的时间字段进行范围匹配。
 *
 * @param service      指向病历服务结构体
 * @param time_from    起始时间字符串（空白表示不限制起始）
 * @param time_to      结束时间字符串（空白表示不限制结束）
 * @param out_history  输出参数，查找成功时存放病历历史
 * @return Result      操作结果
 */
Result MedicalRecordService_find_records_by_time_range(
    MedicalRecordService *service,
    const char *time_from,
    const char *time_to,
    MedicalRecordHistory *out_history
) {
    LinkedList registrations;
    LinkedList visits;
    LinkedList examinations;
    LinkedList admissions;
    LinkedListNode *current = 0;
    Result result;

    if (service == 0 || out_history == 0) {
        return Result_make_failure("time range query arguments invalid");
    }

    /* 校验时间范围的合法性（起始时间不能晚于结束时间） */
    if (!StringUtils_is_blank(time_from) &&
        !StringUtils_is_blank(time_to) &&
        strcmp(time_from, time_to) > 0) {
        return Result_make_failure("time range invalid");
    }

    memset(out_history, 0, sizeof(*out_history));
    MedicalRecordHistory_init(out_history);

    /* 按挂号时间筛选挂号记录 */
    result = MedicalRecordService_load_registrations(service, &registrations);
    if (result.success == 0) {
        return result;
    }

    current = registrations.head;
    while (current != 0) {
        const Registration *registration = (const Registration *)current->data;

        if (MedicalRecordService_is_in_time_range(
                registration->registered_at,
                time_from,
                time_to
            )) {
            result = MedicalRecordService_append_registration_copy(
                &out_history->registrations,
                registration
            );
            if (result.success == 0) {
                RegistrationRepository_clear_list(&registrations);
                MedicalRecordHistory_clear(out_history);
                return result;
            }
        }

        current = current->next;
    }
    RegistrationRepository_clear_list(&registrations);

    /* 按就诊时间筛选就诊记录 */
    result = MedicalRecordService_load_visits(service, &visits);
    if (result.success == 0) {
        MedicalRecordHistory_clear(out_history);
        return result;
    }

    current = visits.head;
    while (current != 0) {
        const VisitRecord *record = (const VisitRecord *)current->data;

        if (MedicalRecordService_is_in_time_range(record->visit_time, time_from, time_to)) {
            result = MedicalRecordService_append_visit_copy(&out_history->visits, record);
            if (result.success == 0) {
                VisitRecordRepository_clear_list(&visits);
                MedicalRecordHistory_clear(out_history);
                return result;
            }
        }

        current = current->next;
    }
    VisitRecordRepository_clear_list(&visits);

    /* 按申请时间筛选检查记录 */
    result = MedicalRecordService_load_examinations(service, &examinations);
    if (result.success == 0) {
        MedicalRecordHistory_clear(out_history);
        return result;
    }

    current = examinations.head;
    while (current != 0) {
        const ExaminationRecord *record = (const ExaminationRecord *)current->data;

        if (MedicalRecordService_is_in_time_range(record->requested_at, time_from, time_to)) {
            result = MedicalRecordService_append_examination_copy(
                &out_history->examinations,
                record
            );
            if (result.success == 0) {
                ExaminationRecordRepository_clear_list(&examinations);
                MedicalRecordHistory_clear(out_history);
                return result;
            }
        }

        current = current->next;
    }
    ExaminationRecordRepository_clear_list(&examinations);

    /* 按入院时间筛选住院记录 */
    result = MedicalRecordService_load_admissions(service, &admissions);
    if (result.success == 0) {
        MedicalRecordHistory_clear(out_history);
        return result;
    }

    current = admissions.head;
    while (current != 0) {
        const Admission *admission = (const Admission *)current->data;

        if (MedicalRecordService_is_in_time_range(admission->admitted_at, time_from, time_to)) {
            result = MedicalRecordService_append_admission_copy(&out_history->admissions, admission);
            if (result.success == 0) {
                AdmissionRepository_clear_loaded_list(&admissions);
                MedicalRecordHistory_clear(out_history);
                return result;
            }
        }

        current = current->next;
    }
    AdmissionRepository_clear_loaded_list(&admissions);

    return Result_make_success("time range records loaded");
}
