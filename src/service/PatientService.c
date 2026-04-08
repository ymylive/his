/**
 * @file PatientService.c
 * @brief 患者服务模块实现
 *
 * 实现患者信息的增删改查业务逻辑，包含数据校验（姓名、联系方式、
 * 身份证号等）、唯一性约束检查以及多种查询方式的具体实现。
 */

#include "service/PatientService.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "repository/RepositoryUtils.h"

/**
 * @brief 患者查找字段类型枚举
 *
 * 用于 PatientService_find_patient_in_loaded_list 函数中指定按哪个字段匹配。
 */
enum {
    PATIENT_SERVICE_MATCH_CONTACT = 1,  /* 按联系方式匹配 */
    PATIENT_SERVICE_MATCH_ID_CARD = 2   /* 按身份证号匹配 */
};

/**
 * @brief 判断文本是否为空白（NULL、空串或全为空白字符）
 *
 * @param text  待检查的字符串
 * @return int  1=空白，0=非空白
 */
static int PatientService_is_blank_text(const char *text) {
    if (text == 0) {
        return 1;
    }

    while (*text != '\0') {
        if (!isspace((unsigned char)*text)) {
            return 0;
        }

        text++;
    }

    return 1;
}

/**
 * @brief 校验必填文本字段
 *
 * 检查文本是否为空白以及是否包含保留字符。
 *
 * @param text        待校验的文本
 * @param field_name  字段名称（用于错误消息）
 * @return Result     校验结果
 */
static Result PatientService_validate_required_text(const char *text, const char *field_name) {
    char message[RESULT_MESSAGE_CAPACITY];

    if (PatientService_is_blank_text(text)) {
        snprintf(message, sizeof(message), "%s missing", field_name);
        return Result_make_failure(message);
    }

    if (!RepositoryUtils_is_safe_field_text(text)) {
        snprintf(message, sizeof(message), "%s contains reserved characters", field_name);
        return Result_make_failure(message);
    }

    return Result_make_success("text valid");
}

/**
 * @brief 校验联系方式的合法性
 *
 * 联系方式必须为6到20位的纯数字字符串。
 *
 * @param contact  联系方式字符串
 * @return Result  校验结果
 */
static Result PatientService_validate_contact(const char *contact) {
    size_t length = 0;
    Result result = PatientService_validate_required_text(contact, "patient contact");

    if (result.success == 0) {
        return result;
    }

    /* 检查长度范围 */
    length = strlen(contact);
    if (length < 6 || length > 20) {
        return Result_make_failure("patient contact invalid");
    }

    /* 逐字符检查是否全为数字 */
    while (*contact != '\0') {
        if (!isdigit((unsigned char)*contact)) {
            return Result_make_failure("patient contact invalid");
        }

        contact++;
    }

    return Result_make_success("patient contact valid");
}

/**
 * @brief 判断身份证号某一位字符是否合法
 *
 * 数字字符始终合法；18位身份证的最后一位允许 'X' 或 'x'（校验码）。
 *
 * @param character  待检查的字符
 * @param index      字符在身份证号中的位置索引
 * @param length     身份证号总长度
 * @return int       1=合法，0=非法
 */
static int PatientService_is_valid_id_card_character(char character, size_t index, size_t length) {
    if (isdigit((unsigned char)character)) {
        return 1;
    }

    /* 18位身份证号最后一位可以是 'X' 或 'x' */
    if (length == 18 && index == length - 1 && (character == 'X' || character == 'x')) {
        return 1;
    }

    return 0;
}

/**
 * @brief 校验身份证号的合法性
 *
 * 身份证号必须为15位或18位，且每一位字符符合规则。
 *
 * @param id_card  身份证号字符串
 * @return Result  校验结果
 */
static Result PatientService_validate_id_card(const char *id_card) {
    size_t index = 0;
    size_t length = 0;
    Result result = PatientService_validate_required_text(id_card, "patient id card");

    if (result.success == 0) {
        return result;
    }

    /* 仅支持15位（一代）和18位（二代）身份证 */
    length = strlen(id_card);
    if (length != 15 && length != 18) {
        return Result_make_failure("patient id card invalid");
    }

    /* 逐字符校验合法性 */
    for (index = 0; index < length; index++) {
        if (!PatientService_is_valid_id_card_character(id_card[index], index, length)) {
            return Result_make_failure("patient id card invalid");
        }
    }

    return Result_make_success("patient id card valid");
}

/**
 * @brief 综合校验患者信息的合法性
 *
 * 依次校验患者ID、姓名、性别、年龄、联系方式、身份证号、
 * 过敏史/病史/备注中的保留字符以及住院标志。
 *
 * @param patient  指向待校验的患者结构体
 * @return Result  校验结果
 */
static Result PatientService_validate_patient(const Patient *patient) {
    Result result;

    if (patient == 0) {
        return Result_make_failure("patient missing");
    }

    /* 校验患者ID */
    result = PatientService_validate_required_text(patient->patient_id, "patient id");
    if (result.success == 0) {
        return result;
    }

    /* 校验患者姓名 */
    result = PatientService_validate_required_text(patient->name, "patient name");
    if (result.success == 0) {
        return result;
    }

    /* 校验性别枚举范围 */
    if (patient->gender < PATIENT_GENDER_UNKNOWN || patient->gender > PATIENT_GENDER_FEMALE) {
        return Result_make_failure("patient gender invalid");
    }

    /* 校验年龄合法性 */
    if (!Patient_is_valid_age(patient->age)) {
        return Result_make_failure("patient age invalid");
    }

    /* 校验联系方式 */
    result = PatientService_validate_contact(patient->contact);
    if (result.success == 0) {
        return result;
    }

    /* 校验身份证号 */
    result = PatientService_validate_id_card(patient->id_card);
    if (result.success == 0) {
        return result;
    }

    /* 检查可选文本字段是否包含保留字符 */
    if (!RepositoryUtils_is_safe_field_text(patient->allergy) ||
        !RepositoryUtils_is_safe_field_text(patient->medical_history) ||
        !RepositoryUtils_is_safe_field_text(patient->remarks)) {
        return Result_make_failure("patient field contains reserved characters");
    }

    /* 住院标志只能为0或1 */
    if (patient->is_inpatient != 0 && patient->is_inpatient != 1) {
        return Result_make_failure("patient inpatient flag invalid");
    }

    return Result_make_success("patient valid");
}

/**
 * @brief 链表查找回调：按患者ID匹配
 *
 * @param data     链表节点中的患者数据
 * @param context  待匹配的患者ID字符串
 * @return int     1=匹配，0=不匹配
 */
static int PatientService_matches_patient_id(const void *data, const void *context) {
    const Patient *patient = (const Patient *)data;
    const char *patient_id = (const char *)context;

    return strcmp(patient->patient_id, patient_id) == 0;
}

/**
 * @brief 从仓库加载所有患者数据到链表
 *
 * @param service   指向患者服务结构体
 * @param patients  输出参数，加载结果链表
 * @return Result   操作结果
 */
static Result PatientService_load_patients(PatientService *service, LinkedList *patients) {
    if (service == 0 || patients == 0) {
        return Result_make_failure("patient service arguments missing");
    }

    return PatientRepository_load_all(&service->repository, patients);
}

/**
 * @brief 将患者信息的副本追加到搜索结果链表
 *
 * 分配新的 Patient 结构体并复制数据，追加到输出链表。
 *
 * @param out_patients  输出结果链表
 * @param patient       待复制的源患者信息
 * @return Result       操作结果
 */
static Result PatientService_append_match(LinkedList *out_patients, const Patient *patient) {
    Patient *copy = (Patient *)malloc(sizeof(Patient));

    if (copy == 0) {
        return Result_make_failure("failed to allocate patient match");
    }

    *copy = *patient;
    if (!LinkedList_append(out_patients, copy)) {
        free(copy);
        return Result_make_failure("failed to append patient match");
    }

    return Result_make_success("patient match appended");
}

/**
 * @brief 确保候选患者的唯一标识字段不与已有记录冲突
 *
 * 遍历已有患者列表，检查患者ID、联系方式和身份证号是否已被占用。
 * 可通过 ignore_patient_id 参数排除更新操作中的自身记录。
 *
 * @param patients           已有患者链表
 * @param candidate          候选患者信息
 * @param ignore_patient_id  需跳过的患者ID（更新时传自身ID，新增时传 NULL）
 * @return Result            校验结果
 */
static Result PatientService_ensure_unique_identity_fields(
    const LinkedList *patients,
    const Patient *candidate,
    const char *ignore_patient_id
) {
    const LinkedListNode *current = 0;

    current = patients->head;
    while (current != 0) {
        const Patient *stored_patient = (const Patient *)current->data;

        /* 跳过自身记录（更新操作时） */
        if (ignore_patient_id != 0 && strcmp(stored_patient->patient_id, ignore_patient_id) == 0) {
            current = current->next;
            continue;
        }

        /* 检查患者ID唯一性 */
        if (strcmp(stored_patient->patient_id, candidate->patient_id) == 0) {
            return Result_make_failure("patient id already exists");
        }

        /* 检查联系方式唯一性 */
        if (strcmp(stored_patient->contact, candidate->contact) == 0) {
            return Result_make_failure("patient contact already exists");
        }

        /* 检查身份证号唯一性 */
        if (strcmp(stored_patient->id_card, candidate->id_card) == 0) {
            return Result_make_failure("patient id card already exists");
        }

        current = current->next;
    }

    return Result_make_success("patient identity unique");
}

/**
 * @brief 从链表中按患者ID删除节点
 *
 * 遍历链表查找匹配节点，从链表中移除并释放内存。
 *
 * @param patients    患者链表
 * @param patient_id  待删除的患者ID
 * @return Result     操作结果
 */
static Result PatientService_remove_patient_from_list(
    LinkedList *patients,
    const char *patient_id
) {
    LinkedListNode *current = 0;
    LinkedListNode *previous = 0;

    current = patients->head;
    while (current != 0) {
        Patient *patient = (Patient *)current->data;

        if (strcmp(patient->patient_id, patient_id) == 0) {
            /* 调整链表前后指针 */
            if (previous == 0) {
                patients->head = current->next;  /* 删除的是头节点 */
            } else {
                previous->next = current->next;
            }

            /* 更新尾指针 */
            if (patients->tail == current) {
                patients->tail = previous;
            }

            /* 释放节点数据和节点本身 */
            free(current->data);
            free(current);
            patients->count--;
            return Result_make_success("patient deleted");
        }

        previous = current;
        current = current->next;
    }

    return Result_make_failure("patient not found");
}

/**
 * @brief 在已加载的患者链表中按指定字段查找唯一匹配的患者
 *
 * 支持按联系方式或身份证号查找，要求匹配结果唯一（恰好一条）。
 *
 * @param patients     已加载的患者链表
 * @param value        待匹配的字段值
 * @param field_type   字段类型（PATIENT_SERVICE_MATCH_CONTACT 或 PATIENT_SERVICE_MATCH_ID_CARD）
 * @param out_patient  输出参数，匹配成功时存放患者信息
 * @return Result      操作结果
 */
static Result PatientService_find_patient_in_loaded_list(
    const LinkedList *patients,
    const char *value,
    int field_type,
    Patient *out_patient
) {
    const LinkedListNode *current = 0;
    int matched_count = 0;

    current = patients->head;
    while (current != 0) {
        const Patient *patient = (const Patient *)current->data;
        /* 根据字段类型选择比较的字段 */
        const char *field_value = field_type == PATIENT_SERVICE_MATCH_CONTACT
            ? patient->contact
            : patient->id_card;

        if (strcmp(field_value, value) == 0) {
            *out_patient = *patient;
            matched_count++;
        }

        current = current->next;
    }

    /* 无匹配结果 */
    if (matched_count == 0) {
        return Result_make_failure(
            field_type == PATIENT_SERVICE_MATCH_CONTACT
                ? "patient contact not found"
                : "patient id card not found"
        );
    }

    /* 存在重复记录（数据异常） */
    if (matched_count > 1) {
        return Result_make_failure(
            field_type == PATIENT_SERVICE_MATCH_CONTACT
                ? "duplicate patient contact found"
                : "duplicate patient id card found"
        );
    }

    return Result_make_success("patient found");
}

/**
 * @brief 初始化患者服务
 *
 * @param service  指向待初始化的患者服务结构体
 * @param path     患者数据文件路径
 * @return Result  操作结果
 */
Result PatientService_init(PatientService *service, const char *path) {
    if (service == 0) {
        return Result_make_failure("patient service missing");
    }

    return PatientRepository_init(&service->repository, path);
}

/**
 * @brief 创建新患者记录
 *
 * 流程：校验患者信息 -> 加载已有数据 -> 检查唯一性约束 -> 追加新记录。
 *
 * @param service  指向患者服务结构体
 * @param patient  指向待创建的患者信息
 * @return Result  操作结果
 */
Result PatientService_create_patient(PatientService *service, const Patient *patient) {
    LinkedList patients;
    Result result = PatientService_validate_patient(patient);

    if (result.success == 0) {
        return result;
    }

    /* 加载所有已有患者数据 */
    result = PatientService_load_patients(service, &patients);
    if (result.success == 0) {
        return result;
    }

    /* 检查患者ID、联系方式、身份证号的唯一性 */
    result = PatientService_ensure_unique_identity_fields(&patients, patient, 0);
    PatientRepository_clear_loaded_list(&patients);
    if (result.success == 0) {
        return result;
    }

    /* 唯一性校验通过，追加新患者记录 */
    return PatientRepository_append(&service->repository, patient);
}

/**
 * @brief 更新患者记录
 *
 * 流程：校验患者信息 -> 加载已有数据 -> 查找目标记录 ->
 * 检查唯一性约束（排除自身） -> 覆盖更新 -> 保存全量数据。
 *
 * @param service  指向患者服务结构体
 * @param patient  指向包含更新后信息的患者结构体
 * @return Result  操作结果
 */
Result PatientService_update_patient(PatientService *service, const Patient *patient) {
    LinkedList patients;
    Patient *stored_patient = 0;
    Result result = PatientService_validate_patient(patient);

    if (result.success == 0) {
        return result;
    }

    result = PatientService_load_patients(service, &patients);
    if (result.success == 0) {
        return result;
    }

    /* 在链表中查找待更新的患者记录 */
    stored_patient = (Patient *)LinkedList_find_first(
        &patients,
        PatientService_matches_patient_id,
        patient->patient_id
    );
    if (stored_patient == 0) {
        PatientRepository_clear_loaded_list(&patients);
        return Result_make_failure("patient not found");
    }

    /* 检查唯一性约束（排除自身记录） */
    result = PatientService_ensure_unique_identity_fields(&patients, patient, patient->patient_id);
    if (result.success == 0) {
        PatientRepository_clear_loaded_list(&patients);
        return result;
    }

    /* 用新数据覆盖旧记录，然后保存整个链表 */
    *stored_patient = *patient;
    result = PatientRepository_save_all(&service->repository, &patients);
    PatientRepository_clear_loaded_list(&patients);
    return result;
}

/**
 * @brief 删除患者记录
 *
 * 流程：校验患者ID -> 加载已有数据 -> 从链表中移除 -> 保存全量数据。
 *
 * @param service     指向患者服务结构体
 * @param patient_id  待删除的患者ID
 * @return Result     操作结果
 */
Result PatientService_delete_patient(PatientService *service, const char *patient_id) {
    LinkedList patients;
    Result result = PatientService_validate_required_text(patient_id, "patient id");

    if (result.success == 0) {
        return result;
    }

    result = PatientService_load_patients(service, &patients);
    if (result.success == 0) {
        return result;
    }

    /* 从链表中移除目标患者节点 */
    result = PatientService_remove_patient_from_list(&patients, patient_id);
    if (result.success == 0) {
        PatientRepository_clear_loaded_list(&patients);
        return result;
    }

    /* 将更新后的链表保存回文件 */
    result = PatientRepository_save_all(&service->repository, &patients);
    PatientRepository_clear_loaded_list(&patients);
    return result;
}

/**
 * @brief 根据患者ID查找患者
 *
 * @param service      指向患者服务结构体
 * @param patient_id   待查找的患者ID
 * @param out_patient  输出参数，查找成功时存放患者信息
 * @return Result      操作结果
 */
Result PatientService_find_patient_by_id(
    PatientService *service,
    const char *patient_id,
    Patient *out_patient
) {
    Result result = PatientService_validate_required_text(patient_id, "patient id");

    if (result.success == 0) {
        return result;
    }

    if (service == 0 || out_patient == 0) {
        return Result_make_failure("patient service arguments missing");
    }

    /* 委托仓库层按ID查找 */
    return PatientRepository_find_by_id(&service->repository, patient_id, out_patient);
}

/**
 * @brief 根据姓名查找患者（可能返回多条记录）
 *
 * 加载所有患者数据，遍历匹配姓名相同的记录并复制到输出链表。
 *
 * @param service       指向患者服务结构体
 * @param name          待查找的患者姓名
 * @param out_patients  输出参数，查找结果链表
 * @return Result       操作结果
 */
Result PatientService_find_patients_by_name(
    PatientService *service,
    const char *name,
    LinkedList *out_patients
) {
    LinkedList patients;
    const LinkedListNode *current = 0;
    Result result = PatientService_validate_required_text(name, "patient name");

    if (result.success == 0) {
        return result;
    }

    if (out_patients == 0) {
        return Result_make_failure("patient search results missing");
    }

    /* 初始化输出链表 */
    LinkedList_init(out_patients);
    result = PatientService_load_patients(service, &patients);
    if (result.success == 0) {
        return result;
    }

    /* 遍历所有患者，按姓名精确匹配 */
    current = patients.head;
    while (current != 0) {
        const Patient *patient = (const Patient *)current->data;

        if (strcmp(patient->name, name) == 0) {
            result = PatientService_append_match(out_patients, patient);
            if (result.success == 0) {
                PatientRepository_clear_loaded_list(&patients);
                PatientService_clear_search_results(out_patients);
                return result;
            }
        }

        current = current->next;
    }

    PatientRepository_clear_loaded_list(&patients);
    return Result_make_success("patient name search complete");
}

/**
 * @brief 根据联系方式查找患者
 *
 * @param service      指向患者服务结构体
 * @param contact      联系方式
 * @param out_patient  输出参数，查找成功时存放患者信息
 * @return Result      操作结果
 */
Result PatientService_find_patient_by_contact(
    PatientService *service,
    const char *contact,
    Patient *out_patient
) {
    LinkedList patients;
    Result result = PatientService_validate_contact(contact);

    if (result.success == 0) {
        return result;
    }

    if (out_patient == 0) {
        return Result_make_failure("patient output missing");
    }

    result = PatientService_load_patients(service, &patients);
    if (result.success == 0) {
        return result;
    }

    /* 在已加载列表中按联系方式查找唯一匹配 */
    result = PatientService_find_patient_in_loaded_list(
        &patients,
        contact,
        PATIENT_SERVICE_MATCH_CONTACT,
        out_patient
    );
    PatientRepository_clear_loaded_list(&patients);
    return result;
}

/**
 * @brief 根据身份证号查找患者
 *
 * @param service      指向患者服务结构体
 * @param id_card      身份证号
 * @param out_patient  输出参数，查找成功时存放患者信息
 * @return Result      操作结果
 */
Result PatientService_find_patient_by_id_card(
    PatientService *service,
    const char *id_card,
    Patient *out_patient
) {
    LinkedList patients;
    Result result = PatientService_validate_id_card(id_card);

    if (result.success == 0) {
        return result;
    }

    if (out_patient == 0) {
        return Result_make_failure("patient output missing");
    }

    result = PatientService_load_patients(service, &patients);
    if (result.success == 0) {
        return result;
    }

    /* 在已加载列表中按身份证号查找唯一匹配 */
    result = PatientService_find_patient_in_loaded_list(
        &patients,
        id_card,
        PATIENT_SERVICE_MATCH_ID_CARD,
        out_patient
    );
    PatientRepository_clear_loaded_list(&patients);
    return result;
}

/**
 * @brief 清理按姓名查找返回的搜索结果链表
 *
 * 释放链表中所有动态分配的 Patient 节点。
 *
 * @param patients  指向待清理的患者链表
 */
void PatientService_clear_search_results(LinkedList *patients) {
    LinkedList_clear(patients, free);
}
