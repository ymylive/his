/**
 * @file PatientRepository.c
 * @brief 患者数据仓储层实现
 *
 * 实现患者（Patient）数据的持久化存储操作，包括：
 * - 患者数据的序列化（结构体 -> 管道符分隔文本行）与反序列化（文本行 -> 结构体）
 * - 患者数据校验（字段合法性、保留字符检测）
 * - 追加、查找、全量加载、全量保存操作
 * - 重复ID检测
 */

#include "repository/PatientRepository.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "repository/RepositoryUtils.h"

/**
 * @brief 按ID查找患者时使用的上下文结构体
 */
typedef struct PatientRepositoryFindContext {
    const char *patient_id; /**< 要查找的患者ID */
    Patient *out_patient;   /**< 输出：找到的患者数据 */
    int found;              /**< 是否已找到标志 */
} PatientRepositoryFindContext;

/**
 * @brief 加载所有患者时使用的上下文结构体
 */
typedef struct PatientRepositoryLoadContext {
    LinkedList *patients; /**< 用于存放加载结果的链表 */
} PatientRepositoryLoadContext;

/**
 * @brief 校验患者数据的合法性
 *
 * 检查项目包括：
 * - 患者指针和ID非空
 * - 各文本字段不包含保留字符（管道符、换行符等）
 * - 性别、年龄、住院标志的值域合法
 *
 * @param patient 待校验的患者数据
 * @return 合法返回 success；不合法返回 failure 并说明原因
 */
static Result PatientRepository_validate_patient(const Patient *patient) {
    if (patient == 0) {
        return Result_make_failure("patient missing");
    }

    if (patient->patient_id[0] == '\0') {
        return Result_make_failure("patient id missing");
    }

    /* 检查所有文本字段是否包含保留字符 */
    if (!RepositoryUtils_is_safe_field_text(patient->patient_id) ||
        !RepositoryUtils_is_safe_field_text(patient->name) ||
        !RepositoryUtils_is_safe_field_text(patient->contact) ||
        !RepositoryUtils_is_safe_field_text(patient->id_card) ||
        !RepositoryUtils_is_safe_field_text(patient->allergy) ||
        !RepositoryUtils_is_safe_field_text(patient->medical_history) ||
        !RepositoryUtils_is_safe_field_text(patient->remarks)) {
        return Result_make_failure("patient field contains reserved characters");
    }

    /* 校验性别枚举值范围 */
    if (patient->gender < PATIENT_GENDER_UNKNOWN || patient->gender > PATIENT_GENDER_FEMALE) {
        return Result_make_failure("patient gender invalid");
    }

    /* 校验年龄合法性 */
    if (!Patient_is_valid_age(patient->age)) {
        return Result_make_failure("patient age invalid");
    }

    /* 住院标志只能为 0 或 1 */
    if (patient->is_inpatient != 0 && patient->is_inpatient != 1) {
        return Result_make_failure("patient inpatient flag invalid");
    }

    return Result_make_success("patient valid");
}

/**
 * @brief 将文本解析为整数
 * @param text      待解析的文本
 * @param out_value 输出参数，解析得到的整数值
 * @return 成功返回 success；格式不合法时返回 failure
 */
static Result PatientRepository_parse_int(const char *text, int *out_value) {
    char *end = 0;
    long value = 0;

    if (text == 0 || out_value == 0 || text[0] == '\0') {
        return Result_make_failure("integer field missing");
    }

    errno = 0;
    value = strtol(text, &end, 10);
    /* 检查转换错误、未完整转换、溢出等情况 */
    if (errno != 0 || end == text || *end != '\0' || value < INT_MIN || value > INT_MAX) {
        return Result_make_failure("integer field invalid");
    }

    *out_value = (int)value;
    return Result_make_success("integer field parsed");
}

/**
 * @brief 安全复制字段内容到目标缓冲区
 *
 * 确保不会超出目标缓冲区容量，并自动添加终止符。
 *
 * @param destination          目标缓冲区
 * @param destination_capacity 目标缓冲区容量
 * @param source               源字符串
 * @return 成功返回 success；源字符串过长时返回 failure
 */
static Result PatientRepository_copy_field_bounded(
    char *destination,
    size_t destination_capacity,
    const char *source
) {
    size_t source_length = 0;

    if (destination == 0 || destination_capacity == 0 || source == 0) {
        return Result_make_failure("patient field copy arguments missing");
    }

    source_length = strlen(source);
    if (source_length >= destination_capacity) {
        return Result_make_failure("patient field too long");
    }

    memcpy(destination, source, source_length + 1); /* +1 包含终止符 */
    return Result_make_success("patient field copied");
}

/**
 * @brief 将一行文本解析为患者结构体
 *
 * 按管道符拆分行内容，依次解析各字段并填充到 Patient 结构体中。
 *
 * @param line        管道符分隔的文本行
 * @param out_patient 输出参数，解析得到的患者数据
 * @return 解析成功返回 success；格式不合法时返回 failure
 */
static Result PatientRepository_parse_line(const char *line, Patient *out_patient) {
    char buffer[TEXT_FILE_REPOSITORY_LINE_CAPACITY];
    char *fields[PATIENT_REPOSITORY_FIELD_COUNT];
    size_t field_count = 0;
    int parsed_value = 0;
    Result result;

    if (line == 0 || out_patient == 0) {
        return Result_make_failure("patient line arguments missing");
    }

    if (strlen(line) >= sizeof(buffer)) {
        return Result_make_failure("patient line too long");
    }

    /* 复制到可修改缓冲区（split 会原地修改） */
    strncpy(buffer, line, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';
    result = RepositoryUtils_split_pipe_line(
        buffer,
        fields,
        PATIENT_REPOSITORY_FIELD_COUNT,
        &field_count
    );
    if (result.success == 0) {
        return result;
    }

    /* 校验字段数量 */
    result = RepositoryUtils_validate_field_count(field_count, PATIENT_REPOSITORY_FIELD_COUNT);
    if (result.success == 0) {
        return result;
    }

    /* 初始化结构体并逐字段解析 */
    memset(out_patient, 0, sizeof(*out_patient));

    /* 字段0: patient_id */
    result = PatientRepository_copy_field_bounded(
        out_patient->patient_id,
        sizeof(out_patient->patient_id),
        fields[0]
    );
    if (result.success == 0) {
        return result;
    }

    /* 字段1: name */
    result = PatientRepository_copy_field_bounded(
        out_patient->name,
        sizeof(out_patient->name),
        fields[1]
    );
    if (result.success == 0) {
        return result;
    }

    /* 字段2: gender（整数枚举） */
    result = PatientRepository_parse_int(fields[2], &parsed_value);
    if (result.success == 0) {
        return result;
    }
    out_patient->gender = (PatientGender)parsed_value;

    /* 字段3: age */
    result = PatientRepository_parse_int(fields[3], &parsed_value);
    if (result.success == 0) {
        return result;
    }
    out_patient->age = parsed_value;

    /* 字段4: contact */
    result = PatientRepository_copy_field_bounded(
        out_patient->contact,
        sizeof(out_patient->contact),
        fields[4]
    );
    if (result.success == 0) {
        return result;
    }

    /* 字段5: id_card */
    result = PatientRepository_copy_field_bounded(
        out_patient->id_card,
        sizeof(out_patient->id_card),
        fields[5]
    );
    if (result.success == 0) {
        return result;
    }

    /* 字段6: allergy（过敏史） */
    result = PatientRepository_copy_field_bounded(
        out_patient->allergy,
        sizeof(out_patient->allergy),
        fields[6]
    );
    if (result.success == 0) {
        return result;
    }

    /* 字段7: medical_history（病史） */
    result = PatientRepository_copy_field_bounded(
        out_patient->medical_history,
        sizeof(out_patient->medical_history),
        fields[7]
    );
    if (result.success == 0) {
        return result;
    }

    /* 字段8: is_inpatient（住院标志） */
    result = PatientRepository_parse_int(fields[8], &parsed_value);
    if (result.success == 0) {
        return result;
    }
    out_patient->is_inpatient = parsed_value;

    /* 字段9: remarks（备注） */
    result = PatientRepository_copy_field_bounded(
        out_patient->remarks,
        sizeof(out_patient->remarks),
        fields[9]
    );
    if (result.success == 0) {
        return result;
    }

    /* 最终校验解析后的数据是否合法 */
    return PatientRepository_validate_patient(out_patient);
}

/**
 * @brief 将患者结构体格式化为管道符分隔的文本行
 * @param patient     患者数据
 * @param buffer      输出缓冲区
 * @param buffer_size 缓冲区大小
 * @return 成功返回 success；数据无效或缓冲区不足时返回 failure
 */
static Result PatientRepository_format_line(const Patient *patient, char *buffer, size_t buffer_size) {
    /* 转义缓冲区：每个字段最大长度翻倍（最坏情况每个字符都需要转义） */
    char esc_patient_id[sizeof(((Patient *)0)->patient_id) * 2];
    char esc_name[sizeof(((Patient *)0)->name) * 2];
    char esc_contact[sizeof(((Patient *)0)->contact) * 2];
    char esc_id_card[sizeof(((Patient *)0)->id_card) * 2];
    char esc_allergy[sizeof(((Patient *)0)->allergy) * 2];
    char esc_medical_history[sizeof(((Patient *)0)->medical_history) * 2];
    char esc_remarks[sizeof(((Patient *)0)->remarks) * 2];
    int written = 0;
    Result result = PatientRepository_validate_patient(patient);

    if (result.success == 0) {
        return result;
    }

    if (buffer == 0 || buffer_size == 0) {
        return Result_make_failure("patient buffer missing");
    }

    /* 对所有文本字段进行转义 */
    RepositoryUtils_escape_field(esc_patient_id, sizeof(esc_patient_id), patient->patient_id);
    RepositoryUtils_escape_field(esc_name, sizeof(esc_name), patient->name);
    RepositoryUtils_escape_field(esc_contact, sizeof(esc_contact), patient->contact);
    RepositoryUtils_escape_field(esc_id_card, sizeof(esc_id_card), patient->id_card);
    RepositoryUtils_escape_field(esc_allergy, sizeof(esc_allergy), patient->allergy);
    RepositoryUtils_escape_field(esc_medical_history, sizeof(esc_medical_history), patient->medical_history);
    RepositoryUtils_escape_field(esc_remarks, sizeof(esc_remarks), patient->remarks);

    /* 将转义后的字段按管道符分隔格式化 */
    written = snprintf(
        buffer,
        buffer_size,
        "%s|%s|%d|%d|%s|%s|%s|%s|%d|%s",
        esc_patient_id,
        esc_name,
        (int)patient->gender,
        patient->age,
        esc_contact,
        esc_id_card,
        esc_allergy,
        esc_medical_history,
        patient->is_inpatient,
        esc_remarks
    );
    if (written < 0 || (size_t)written >= buffer_size) {
        return Result_make_failure("patient line formatting failed");
    }

    return Result_make_success("patient line ready");
}

/**
 * @brief 确保患者数据文件包含正确的表头
 *
 * 如果文件为空则写入表头；如果已有内容则验证表头是否匹配。
 *
 * @param repository 患者仓储实例指针
 * @return 表头正确返回 success；不匹配或操作失败时返回 failure
 */
static Result PatientRepository_ensure_header(const PatientRepository *repository) {
    FILE *file = 0;
    char line[TEXT_FILE_REPOSITORY_LINE_CAPACITY];
    int has_content = 0;
    Result result;

    if (repository == 0) {
        return Result_make_failure("patient repository missing");
    }

    result = TextFileRepository_ensure_file_exists(&repository->file_repository);
    if (result.success == 0) {
        return result;
    }

    /* 读取文件第一个非空行 */
    file = fopen(repository->file_repository.path, "r");
    if (file == 0) {
        return Result_make_failure("failed to read patient repository");
    }

    while (fgets(line, sizeof(line), file) != 0) {
        RepositoryUtils_strip_line_endings(line);
        if (RepositoryUtils_is_blank_line(line)) {
            continue;
        }

        has_content = 1;
        break;
    }

    fclose(file);

    /* 空文件：写入表头 */
    if (!has_content) {
        return TextFileRepository_append_line(
            &repository->file_repository,
            PATIENT_REPOSITORY_HEADER
        );
    }

    /* 非空文件：验证表头 */
    if (strcmp(line, PATIENT_REPOSITORY_HEADER) != 0) {
        return Result_make_failure("patient repository header invalid");
    }

    return Result_make_success("patient repository header ready");
}

/**
 * @brief 按ID查找患者的行处理回调
 *
 * 解析每一行，匹配到目标ID时将数据存入上下文并标记找到。
 * 注意：找到后返回 failure 以终止遍历（利用回调机制的特殊用法）。
 *
 * @param line    当前行文本
 * @param context 查找上下文（PatientRepositoryFindContext）
 * @return 未匹配返回 success（继续）；匹配返回 failure（终止遍历）
 */
static Result PatientRepository_find_line_handler(const char *line, void *context) {
    PatientRepositoryFindContext *find_context = (PatientRepositoryFindContext *)context;
    Patient patient;
    Result result = PatientRepository_parse_line(line, &patient);

    if (result.success == 0) {
        return result;
    }

    /* ID不匹配，继续遍历 */
    if (strcmp(patient.patient_id, find_context->patient_id) != 0) {
        return Result_make_success("patient id mismatch");
    }

    /* 找到匹配的患者，复制数据并标记 */
    *(find_context->out_patient) = patient;
    find_context->found = 1;
    return Result_make_failure("patient found"); /* 返回 failure 以终止遍历 */
}

/**
 * @brief 按ID查找患者的内部实现
 *
 * 通过遍历文件查找指定ID的患者。
 *
 * @param repository 患者仓储实例
 * @param patient_id 要查找的患者ID
 * @param out_patient 输出参数，找到的患者数据
 * @param out_found   输出参数，是否找到（0=未找到，1=已找到）
 * @return 操作成功返回 success（无论是否找到）；出错返回 failure
 */
static Result PatientRepository_find_internal(
    const PatientRepository *repository,
    const char *patient_id,
    Patient *out_patient,
    int *out_found
) {
    PatientRepositoryFindContext context;
    Result result;

    if (out_found != 0) {
        *out_found = 0;
    }

    if (repository == 0 || patient_id == 0 || patient_id[0] == '\0' || out_patient == 0) {
        return Result_make_failure("patient find arguments missing");
    }

    result = PatientRepository_ensure_header(repository);
    if (result.success == 0) {
        return result;
    }

    memset(&context, 0, sizeof(context));
    context.patient_id = patient_id;
    context.out_patient = out_patient;

    result = TextFileRepository_for_each_data_line(
        &repository->file_repository,
        PatientRepository_find_line_handler,
        &context
    );

    /* 检查是否通过回调找到了患者 */
    if (context.found != 0) {
        if (out_found != 0) {
            *out_found = 1;
        }
        return Result_make_success("patient found");
    }

    /* 区分"未找到"和"遍历出错"两种情况 */
    if (result.success == 0 && strcmp(result.message, "patient found") != 0) {
        return result; /* 真正的错误 */
    }

    return Result_make_success("patient not found");
}

/**
 * @brief 加载所有患者的行处理回调
 *
 * 解析每一行并创建患者对象加入链表。
 *
 * @param line    当前行文本
 * @param context 加载上下文（PatientRepositoryLoadContext）
 * @return 成功返回 success；内存分配失败或解析错误返回 failure
 */
static Result PatientRepository_load_line_handler(const char *line, void *context) {
    PatientRepositoryLoadContext *load_context = (PatientRepositoryLoadContext *)context;
    Patient parsed_patient;
    Patient *stored_patient = 0;
    Result result = PatientRepository_parse_line(line, &parsed_patient);

    if (result.success == 0) {
        return result;
    }

    /* 在堆上分配患者对象 */
    stored_patient = (Patient *)malloc(sizeof(Patient));
    if (stored_patient == 0) {
        return Result_make_failure("failed to allocate patient");
    }

    *stored_patient = parsed_patient;
    /* 将患者对象加入链表 */
    if (!LinkedList_append(load_context->patients, stored_patient)) {
        free(stored_patient);
        return Result_make_failure("failed to append patient to list");
    }

    return Result_make_success("patient loaded");
}

/**
 * @brief 校验患者链表的合法性
 *
 * 检查每个患者数据是否合法，以及是否有重复的患者ID。
 *
 * @param patients 待校验的患者链表
 * @return 合法返回 success；不合法返回 failure
 */
static int PatientRepository_compare_ids(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

static Result PatientRepository_validate_patient_list(const LinkedList *patients) {
    const LinkedListNode *current = 0;
    const char **id_array = 0;
    size_t count = 0;
    size_t index = 0;

    if (patients == 0) {
        return Result_make_failure("patient list missing");
    }

    current = patients->head;
    while (current != 0) {
        const Patient *patient = (const Patient *)current->data;
        Result result = PatientRepository_validate_patient(patient);

        if (result.success == 0) {
            return result;
        }

        count++;
        current = current->next;
    }

    if (count < 2) {
        return Result_make_success("patient list valid");
    }

    id_array = (const char **)malloc(count * sizeof(const char *));
    if (id_array == 0) {
        return Result_make_failure("failed to allocate id array for validation");
    }

    current = patients->head;
    index = 0;
    while (current != 0) {
        const Patient *patient = (const Patient *)current->data;
        id_array[index] = patient->patient_id;
        index++;
        current = current->next;
    }

    qsort(id_array, count, sizeof(const char *), PatientRepository_compare_ids);

    for (index = 1; index < count; index++) {
        if (strcmp(id_array[index - 1], id_array[index]) == 0) {
            free(id_array);
            return Result_make_failure("duplicate patient id in list");
        }
    }

    free(id_array);
    return Result_make_success("patient list valid");
}

/**
 * @brief 将患者链表的全部数据写入文件（覆盖写入）
 *
 * 先写入表头行，再逐个写入患者记录。
 *
 * @param repository 患者仓储实例
 * @param patients   要写入的患者链表
 * @return 成功返回 success；写入失败时返回 failure
 */
static Result PatientRepository_write_all_lines(
    const PatientRepository *repository,
    const LinkedList *patients
) {
    const LinkedListNode *current = 0;
    char line[TEXT_FILE_REPOSITORY_LINE_CAPACITY];
    char *content = 0;
    size_t capacity = 0;
    size_t used = 0;
    size_t len = 0;
    Result result;

    capacity = (patients->count + 2) * TEXT_FILE_REPOSITORY_LINE_CAPACITY;
    content = (char *)malloc(capacity);
    if (content == 0) {
        return Result_make_failure("failed to allocate patient content buffer");
    }
    used = 0;

    /* 写入表头行 */
    len = strlen(PATIENT_REPOSITORY_HEADER);
    memcpy(content + used, PATIENT_REPOSITORY_HEADER, len);
    used += len;
    content[used++] = '\n';

    /* 逐个写入患者记录 */
    current = patients->head;
    while (current != 0) {
        result = PatientRepository_format_line((const Patient *)current->data, line, sizeof(line));
        if (result.success == 0) {
            free(content);
            return result;
        }

        len = strlen(line);
        if (used + len + 2 > capacity) {
            capacity *= 2;
            content = (char *)realloc(content, capacity);
            if (content == 0) {
                return Result_make_failure("failed to grow patient content buffer");
            }
        }
        memcpy(content + used, line, len);
        used += len;
        content[used++] = '\n';

        current = current->next;
    }

    content[used] = '\0';
    result = TextFileRepository_save_file(&repository->file_repository, content);
    free(content);
    return result;
}

/**
 * @brief 初始化患者仓储
 * @param repository 患者仓储实例指针
 * @param path       数据文件路径
 * @return 成功返回 success；参数无效或文件操作失败时返回 failure
 */
Result PatientRepository_init(PatientRepository *repository, const char *path) {
    Result result;

    if (repository == 0) {
        return Result_make_failure("patient repository missing");
    }

    /* 初始化底层文件仓储 */
    result = TextFileRepository_init(&repository->file_repository, path);
    if (result.success == 0) {
        return result;
    }

    /* 确保表头存在 */
    return PatientRepository_ensure_header(repository);
}

/**
 * @brief 追加一条患者记录
 * @param repository 患者仓储实例指针
 * @param patient    要追加的患者数据
 * @return 成功返回 success；ID重复或写入失败时返回 failure
 */
Result PatientRepository_append(const PatientRepository *repository, const Patient *patient) {
    Patient existing_patient;
    int found = 0;
    char line[TEXT_FILE_REPOSITORY_LINE_CAPACITY];
    Result result = PatientRepository_validate_patient(patient);

    if (result.success == 0) {
        return result;
    }

    /* 检查ID是否已存在 */
    result = PatientRepository_find_internal(
        repository,
        patient->patient_id,
        &existing_patient,
        &found
    );
    if (result.success == 0) {
        return result;
    }

    if (found != 0) {
        return Result_make_failure("patient id already exists");
    }

    /* 格式化并追加写入 */
    result = PatientRepository_format_line(patient, line, sizeof(line));
    if (result.success == 0) {
        return result;
    }

    return TextFileRepository_append_line(&repository->file_repository, line);
}

/**
 * @brief 按患者ID查找患者
 * @param repository  患者仓储实例指针
 * @param patient_id  要查找的患者ID
 * @param out_patient 输出参数，存放找到的患者数据
 * @return 找到返回 success；未找到时返回 failure
 */
Result PatientRepository_find_by_id(
    const PatientRepository *repository,
    const char *patient_id,
    Patient *out_patient
) {
    int found = 0;
    Result result = PatientRepository_find_internal(repository, patient_id, out_patient, &found);

    if (result.success == 0) {
        return result;
    }

    if (found == 0) {
        return Result_make_failure("patient not found");
    }

    return Result_make_success("patient found");
}

/**
 * @brief 加载所有患者记录到链表
 * @param repository   患者仓储实例指针
 * @param out_patients 输出参数，存放患者数据的链表
 * @return 成功返回 success；加载失败时返回 failure 并清空链表
 */
Result PatientRepository_load_all(
    const PatientRepository *repository,
    LinkedList *out_patients
) {
    PatientRepositoryLoadContext context;
    Result result;

    if (repository == 0 || out_patients == 0) {
        return Result_make_failure("patient load arguments missing");
    }

    LinkedList_init(out_patients);
    result = PatientRepository_ensure_header(repository);
    if (result.success == 0) {
        return result;
    }

    context.patients = out_patients;
    result = TextFileRepository_for_each_data_line(
        &repository->file_repository,
        PatientRepository_load_line_handler,
        &context
    );

    /* 加载失败时清空已部分加载的数据 */
    if (result.success == 0) {
        PatientRepository_clear_loaded_list(out_patients);
        return result;
    }

    return Result_make_success("patients loaded");
}

/**
 * @brief 全量保存患者列表到文件
 * @param repository 患者仓储实例指针
 * @param patients   要保存的患者链表
 * @return 成功返回 success；数据无效或写入失败时返回 failure
 */
Result PatientRepository_save_all(
    const PatientRepository *repository,
    const LinkedList *patients
) {
    Result result;

    if (repository == 0) {
        return Result_make_failure("patient repository missing");
    }

    /* 先验证整个列表的合法性 */
    result = PatientRepository_validate_patient_list(patients);
    if (result.success == 0) {
        return result;
    }

    return PatientRepository_write_all_lines(repository, patients);
}

/**
 * @brief 清空并释放患者链表中的所有元素
 * @param patients 要清空的患者链表
 */
void PatientRepository_clear_loaded_list(LinkedList *patients) {
    LinkedList_clear(patients, free);
}
