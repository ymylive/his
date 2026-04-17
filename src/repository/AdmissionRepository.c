/**
 * @file AdmissionRepository.c
 * @brief 入院记录数据仓储层实现
 *
 * 实现入院记录（Admission）数据的持久化存储操作，包括：
 * - 入院记录的序列化与反序列化
 * - 数据校验（必填字段、状态与出院时间一致性、保留字符检测）
 * - 追加（含ID唯一性、同一患者/床位不能有多条活跃入院的业务约束检查）
 * - 按ID查找、按患者ID/床位ID查找活跃入院、全量加载、全量保存
 */

#include "repository/AdmissionRepository.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "repository/RepositoryUtils.h"

/** 加载入院记录时使用的上下文结构体 */
typedef struct AdmissionRepositoryLoadContext {
    LinkedList *admissions; /**< 用于存放加载结果的链表 */
} AdmissionRepositoryLoadContext;

/**
 * @brief 校验入院记录的合法性
 *
 * 检查必填字段、保留字符、状态值域，以及状态与出院时间的一致性：
 * - 活跃入院不能有出院时间
 * - 已出院必须有出院时间
 */
static Result AdmissionRepository_validate(const Admission *admission) {
    if (admission == 0) return Result_make_failure("admission missing");

    if (!RepositoryUtils_has_text(admission->admission_id) ||
        !RepositoryUtils_has_text(admission->patient_id) ||
        !RepositoryUtils_has_text(admission->ward_id) ||
        !RepositoryUtils_has_text(admission->bed_id) ||
        !RepositoryUtils_has_text(admission->admitted_at)) {
        return Result_make_failure("admission required field missing");
    }

    if (!RepositoryUtils_is_safe_field_text(admission->admission_id) ||
        !RepositoryUtils_is_safe_field_text(admission->patient_id) ||
        !RepositoryUtils_is_safe_field_text(admission->ward_id) ||
        !RepositoryUtils_is_safe_field_text(admission->bed_id) ||
        !RepositoryUtils_is_safe_field_text(admission->admitted_at) ||
        !RepositoryUtils_is_safe_field_text(admission->discharged_at) ||
        !RepositoryUtils_is_safe_field_text(admission->summary)) {
        return Result_make_failure("admission field contains reserved character");
    }

    if (admission->status < ADMISSION_STATUS_ACTIVE ||
        admission->status > ADMISSION_STATUS_DISCHARGED) {
        return Result_make_failure("admission status invalid");
    }

    /* 活跃入院不能有出院时间 */
    if (admission->status == ADMISSION_STATUS_ACTIVE &&
        RepositoryUtils_has_text(admission->discharged_at)) {
        return Result_make_failure("active admission has discharge time");
    }

    /* 已出院必须有出院时间 */
    if (admission->status == ADMISSION_STATUS_DISCHARGED &&
        !RepositoryUtils_has_text(admission->discharged_at)) {
        return Result_make_failure("discharged admission missing discharge time");
    }

    return Result_make_success("admission valid");
}

/** 将入院记录格式化为文本行 */
static Result AdmissionRepository_format_line(
    const Admission *admission, char *line, size_t line_capacity
) {
    int written = 0;
    Result result = AdmissionRepository_validate(admission);
    if (result.success == 0) return result;
    if (line == 0 || line_capacity == 0) return Result_make_failure("admission line buffer missing");

    written = snprintf(line, line_capacity, "%s|%s|%s|%s|%s|%d|%s|%s",
        admission->admission_id, admission->patient_id,
        admission->ward_id, admission->bed_id, admission->admitted_at,
        (int)admission->status, admission->discharged_at, admission->summary);
    if (written < 0 || (size_t)written >= line_capacity) {
        return Result_make_failure("admission line too long");
    }
    return Result_make_success("admission formatted");
}

/** 将一行文本解析为入院记录结构体 */
static Result AdmissionRepository_parse_line(const char *line, Admission *admission) {
    char mutable_line[TEXT_FILE_REPOSITORY_LINE_CAPACITY];
    char *fields[ADMISSION_REPOSITORY_FIELD_COUNT];
    size_t field_count = 0;
    int status_value = 0;
    Result result;

    if (line == 0 || admission == 0) return Result_make_failure("admission line missing");

    RepositoryUtils_copy_text(mutable_line, sizeof(mutable_line), line);
    result = RepositoryUtils_split_pipe_line(
        mutable_line, fields, ADMISSION_REPOSITORY_FIELD_COUNT, &field_count
    );
    if (result.success == 0) return result;

    result = RepositoryUtils_validate_field_count(field_count, ADMISSION_REPOSITORY_FIELD_COUNT);
    if (result.success == 0) return result;

    memset(admission, 0, sizeof(*admission));
    RepositoryUtils_copy_text(admission->admission_id, sizeof(admission->admission_id), fields[0]);
    RepositoryUtils_copy_text(admission->patient_id, sizeof(admission->patient_id), fields[1]);
    RepositoryUtils_copy_text(admission->ward_id, sizeof(admission->ward_id), fields[2]);
    RepositoryUtils_copy_text(admission->bed_id, sizeof(admission->bed_id), fields[3]);
    RepositoryUtils_copy_text(admission->admitted_at, sizeof(admission->admitted_at), fields[4]);

    result = RepositoryUtils_parse_int(fields[5], &status_value, "admission status");
    if (result.success == 0) return result;
    admission->status = (AdmissionStatus)status_value;

    RepositoryUtils_copy_text(admission->discharged_at, sizeof(admission->discharged_at), fields[6]);
    RepositoryUtils_copy_text(admission->summary, sizeof(admission->summary), fields[7]);

    return AdmissionRepository_validate(admission);
}

/** 确保入院记录文件包含正确的表头 */
static Result AdmissionRepository_ensure_header(const AdmissionRepository *repository) {
    char line[TEXT_FILE_REPOSITORY_LINE_CAPACITY];
    FILE *file = 0;
    Result result;

    if (repository == 0) return Result_make_failure("admission repository missing");

    result = TextFileRepository_ensure_file_exists(&repository->storage);
    if (result.success == 0) return result;

    file = fopen(repository->storage.path, "r");
    if (file == 0) return Result_make_failure("failed to inspect admission repository");

    while (fgets(line, sizeof(line), file) != 0) {
        RepositoryUtils_strip_line_endings(line);
        if (RepositoryUtils_is_blank_line(line)) continue;
        fclose(file);
        if (strcmp(line, ADMISSION_REPOSITORY_HEADER) != 0) {
            return Result_make_failure("admission repository header mismatch");
        }
        return Result_make_success("admission header ready");
    }

    fclose(file);
    return TextFileRepository_append_line(&repository->storage, ADMISSION_REPOSITORY_HEADER);
}

/** 加载入院记录的行处理回调 */
static Result AdmissionRepository_collect_line(const char *line, void *context) {
    AdmissionRepositoryLoadContext *load_context = (AdmissionRepositoryLoadContext *)context;
    Admission *admission = 0;
    Result result;

    if (load_context == 0 || load_context->admissions == 0) {
        return Result_make_failure("admission load context missing");
    }

    admission = (Admission *)malloc(sizeof(*admission));
    if (admission == 0) return Result_make_failure("failed to allocate admission");

    result = AdmissionRepository_parse_line(line, admission);
    if (result.success == 0) { free(admission); return result; }

    if (!LinkedList_append(load_context->admissions, admission)) {
        free(admission);
        return Result_make_failure("failed to append admission");
    }
    return Result_make_success("admission loaded");
}

/**
 * @brief 校验入院记录链表的合法性
 *
 * 除了基本数据合法性和ID唯一性外，还检查业务约束：
 * 同一患者不能有多条活跃入院，同一床位不能有多条活跃入院。
 *
 * @return 1 表示合法，0 表示不合法
 */
static int AdmissionRepository_validate_list(const LinkedList *admissions) {
    const LinkedListNode *outer = 0;
    if (admissions == 0) return 0;

    outer = admissions->head;
    while (outer != 0) {
        const LinkedListNode *inner = outer->next;
        const Admission *admission = (const Admission *)outer->data;
        Result result = AdmissionRepository_validate(admission);
        if (result.success == 0) return 0;

        while (inner != 0) {
            const Admission *other = (const Admission *)inner->data;

            /* 检查ID唯一性 */
            if (strcmp(admission->admission_id, other->admission_id) == 0) return 0;

            /* 同一患者或床位不能有多条活跃入院 */
            if (admission->status == ADMISSION_STATUS_ACTIVE &&
                other->status == ADMISSION_STATUS_ACTIVE) {
                if (strcmp(admission->patient_id, other->patient_id) == 0 ||
                    strcmp(admission->bed_id, other->bed_id) == 0) {
                    return 0;
                }
            }

            inner = inner->next;
        }
        outer = outer->next;
    }
    return 1;
}

/** 在链表中按入院ID查找 */
static Admission *AdmissionRepository_find_in_list(
    LinkedList *admissions, const char *admission_id
) {
    LinkedListNode *current = 0;
    if (admissions == 0 || admission_id == 0) return 0;

    current = admissions->head;
    while (current != 0) {
        Admission *admission = (Admission *)current->data;
        if (strcmp(admission->admission_id, admission_id) == 0) return admission;
        current = current->next;
    }
    return 0;
}

/**
 * @brief 在链表中按指定字段值查找活跃状态的入院记录
 * @param admissions    入院记录链表
 * @param value         要匹配的字段值
 * @param match_patient 1=匹配 patient_id，0=匹配 bed_id
 * @return 找到返回指向记录的指针；未找到返回 NULL
 */
static Admission *AdmissionRepository_find_active_by_field(
    LinkedList *admissions, const char *value, int match_patient
) {
    LinkedListNode *current = 0;
    if (admissions == 0 || value == 0) return 0;

    current = admissions->head;
    while (current != 0) {
        Admission *admission = (Admission *)current->data;
        /* 根据 match_patient 选择比较 patient_id 或 bed_id */
        const char *field_value = match_patient ? admission->patient_id : admission->bed_id;

        if (admission->status == ADMISSION_STATUS_ACTIVE && strcmp(field_value, value) == 0) {
            return admission;
        }
        current = current->next;
    }
    return 0;
}

/** 初始化入院记录仓储 */
Result AdmissionRepository_init(AdmissionRepository *repository, const char *path) {
    Result result;
    if (repository == 0) return Result_make_failure("admission repository missing");

    result = TextFileRepository_init(&repository->storage, path);
    if (result.success == 0) return result;

    return AdmissionRepository_ensure_header(repository);
}

/**
 * @brief 追加一条入院记录
 *
 * 业务约束检查：
 * 1. 入院ID不能重复
 * 2. 活跃入院时，同一患者不能已有活跃入院
 * 3. 活跃入院时，同一床位不能已被占用
 */
Result AdmissionRepository_append(
    const AdmissionRepository *repository, const Admission *admission
) {
    LinkedList admissions;
    Admission *copy = 0;
    Result result = AdmissionRepository_validate(admission);
    if (result.success == 0) return result;

    result = AdmissionRepository_load_all(repository, &admissions);
    if (result.success == 0) return result;

    /* 检查ID唯一性 */
    if (AdmissionRepository_find_in_list(&admissions, admission->admission_id) != 0) {
        AdmissionRepository_clear_loaded_list(&admissions);
        return Result_make_failure("admission id already exists");
    }

    /* 活跃入院的业务约束检查 */
    if (admission->status == ADMISSION_STATUS_ACTIVE) {
        if (AdmissionRepository_find_active_by_field(&admissions, admission->patient_id, 1) != 0) {
            AdmissionRepository_clear_loaded_list(&admissions);
            return Result_make_failure("patient already admitted");
        }
        if (AdmissionRepository_find_active_by_field(&admissions, admission->bed_id, 0) != 0) {
            AdmissionRepository_clear_loaded_list(&admissions);
            return Result_make_failure("bed already occupied");
        }
    }

    copy = (Admission *)malloc(sizeof(*copy));
    if (copy == 0) {
        AdmissionRepository_clear_loaded_list(&admissions);
        return Result_make_failure("failed to allocate admission copy");
    }
    *copy = *admission;
    if (!LinkedList_append(&admissions, copy)) {
        free(copy);
        AdmissionRepository_clear_loaded_list(&admissions);
        return Result_make_failure("failed to append admission copy");
    }

    result = AdmissionRepository_save_all(repository, &admissions);
    AdmissionRepository_clear_loaded_list(&admissions);
    return result;
}

/** 按入院ID查找入院记录 */
Result AdmissionRepository_find_by_id(
    const AdmissionRepository *repository, const char *admission_id, Admission *out_admission
) {
    LinkedList admissions;
    Admission *admission = 0;
    Result result;

    if (!RepositoryUtils_has_text(admission_id) || out_admission == 0) {
        return Result_make_failure("admission query arguments missing");
    }

    result = AdmissionRepository_load_all(repository, &admissions);
    if (result.success == 0) return result;

    admission = AdmissionRepository_find_in_list(&admissions, admission_id);
    if (admission == 0) {
        AdmissionRepository_clear_loaded_list(&admissions);
        return Result_make_failure("admission not found");
    }

    *out_admission = *admission;
    AdmissionRepository_clear_loaded_list(&admissions);
    return Result_make_success("admission found");
}

/** 按患者ID查找活跃入院记录 */
Result AdmissionRepository_find_active_by_patient_id(
    const AdmissionRepository *repository, const char *patient_id, Admission *out_admission
) {
    LinkedList admissions;
    Admission *admission = 0;
    Result result;

    if (!RepositoryUtils_has_text(patient_id) || out_admission == 0) {
        return Result_make_failure("active admission query arguments missing");
    }

    result = AdmissionRepository_load_all(repository, &admissions);
    if (result.success == 0) return result;

    admission = AdmissionRepository_find_active_by_field(&admissions, patient_id, 1);
    if (admission == 0) {
        AdmissionRepository_clear_loaded_list(&admissions);
        return Result_make_failure("active admission not found");
    }

    *out_admission = *admission;
    AdmissionRepository_clear_loaded_list(&admissions);
    return Result_make_success("active admission found");
}

/** 按床位ID查找活跃入院记录 */
Result AdmissionRepository_find_active_by_bed_id(
    const AdmissionRepository *repository, const char *bed_id, Admission *out_admission
) {
    LinkedList admissions;
    Admission *admission = 0;
    Result result;

    if (!RepositoryUtils_has_text(bed_id) || out_admission == 0) {
        return Result_make_failure("active admission query arguments missing");
    }

    result = AdmissionRepository_load_all(repository, &admissions);
    if (result.success == 0) return result;

    admission = AdmissionRepository_find_active_by_field(&admissions, bed_id, 0);
    if (admission == 0) {
        AdmissionRepository_clear_loaded_list(&admissions);
        return Result_make_failure("active admission not found");
    }

    *out_admission = *admission;
    AdmissionRepository_clear_loaded_list(&admissions);
    return Result_make_success("active admission found");
}

/** 加载所有入院记录到链表 */
Result AdmissionRepository_load_all(
    const AdmissionRepository *repository, LinkedList *out_admissions
) {
    AdmissionRepositoryLoadContext context;
    Result result;

    if (out_admissions == 0) return Result_make_failure("admission output list missing");

    LinkedList_init(out_admissions);
    context.admissions = out_admissions;

    result = AdmissionRepository_ensure_header(repository);
    if (result.success == 0) return result;

    result = TextFileRepository_for_each_data_line(
        &repository->storage, AdmissionRepository_collect_line, &context
    );
    if (result.success == 0) {
        AdmissionRepository_clear_loaded_list(out_admissions);
        return result;
    }
    return Result_make_success("admissions loaded");
}

/** 全量保存入院记录列表到文件（覆盖写入） */
Result AdmissionRepository_save_all(
    const AdmissionRepository *repository, const LinkedList *admissions
) {
    const LinkedListNode *current = 0;
    Result result;

    if (repository == 0 || admissions == 0) {
        return Result_make_failure("admission save arguments missing");
    }
    if (!AdmissionRepository_validate_list(admissions)) {
        return Result_make_failure("admission list invalid");
    }

    {
        char line[TEXT_FILE_REPOSITORY_LINE_CAPACITY];
        char *content = 0;
        size_t capacity = (admissions->count + 2) * TEXT_FILE_REPOSITORY_LINE_CAPACITY;
        size_t used = 0;
        size_t len = 0;

        content = (char *)malloc(capacity);
        if (content == 0) {
            return Result_make_failure("failed to allocate admission content buffer");
        }

        len = strlen(ADMISSION_REPOSITORY_HEADER);
        memcpy(content + used, ADMISSION_REPOSITORY_HEADER, len);
        used += len;
        content[used++] = '\n';

        current = admissions->head;
        while (current != 0) {
            result = AdmissionRepository_format_line(
                (const Admission *)current->data, line, sizeof(line)
            );
            if (result.success == 0) { free(content); return result; }

            len = strlen(line);
            if (used + len + 2 > capacity) {
                capacity *= 2;
                content = (char *)realloc(content, capacity);
                if (content == 0) {
                    return Result_make_failure("failed to grow admission content buffer");
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

/** 清空并释放入院记录链表中的所有元素 */
void AdmissionRepository_clear_loaded_list(LinkedList *admissions) {
    LinkedList_clear(admissions, free);
}
