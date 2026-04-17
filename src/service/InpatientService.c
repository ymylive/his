/**
 * @file InpatientService.c
 * @brief 住院服务模块实现
 *
 * 实现住院管理的核心业务功能，包括患者入院、出院、转床以及住院记录查询。
 * 该服务需要协调患者、病房、床位和住院记录四个仓库，
 * 通过全量加载-修改-保存的事务性方式确保多实体状态的一致性。
 */

#include "service/InpatientService.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/IdGenerator.h"
#include "common/StringUtils.h"
#include "repository/RepositoryUtils.h"

/** @brief 住院记录ID的前缀 */
#define INPATIENT_SERVICE_ID_PREFIX "ADM"

/** @brief 住院记录ID序号部分的位宽 */
#define INPATIENT_SERVICE_ID_WIDTH 4


/**
 * @brief 校验必填文本字段
 *
 * 检查文本是否为空白以及是否包含保留字符。
 *
 * @param text        待校验的文本
 * @param field_name  字段名称（用于错误消息）
 * @return Result     校验结果
 */
static Result InpatientService_validate_required_text(const char *text, const char *field_name) {
    char message[RESULT_MESSAGE_CAPACITY];

    if (StringUtils_is_blank(text)) {
        snprintf(message, sizeof(message), "%s missing", field_name);
        return Result_make_failure(message);
    }

    if (!RepositoryUtils_is_safe_field_text(text)) {
        snprintf(message, sizeof(message), "%s contains reserved characters", field_name);
        return Result_make_failure(message);
    }

    return Result_make_success("inpatient text valid");
}

/**
 * @brief 从患者仓库加载所有患者数据
 *
 * @param service       指向住院服务结构体
 * @param out_patients  输出参数，患者链表
 * @return Result       操作结果
 */
static Result InpatientService_load_patients(
    InpatientService *service,
    LinkedList *out_patients
) {
    return PatientRepository_load_all(&service->patient_repository, out_patients);
}

/**
 * @brief 从病房仓库加载所有病房数据
 *
 * @param service    指向住院服务结构体
 * @param out_wards  输出参数，病房链表
 * @return Result    操作结果
 */
static Result InpatientService_load_wards(InpatientService *service, LinkedList *out_wards) {
    return WardRepository_load_all(&service->ward_repository, out_wards);
}

/**
 * @brief 从床位仓库加载所有床位数据
 *
 * @param service   指向住院服务结构体
 * @param out_beds  输出参数，床位链表
 * @return Result   操作结果
 */
static Result InpatientService_load_beds(InpatientService *service, LinkedList *out_beds) {
    return BedRepository_load_all(&service->bed_repository, out_beds);
}

/**
 * @brief 从住院记录仓库加载所有住院记录
 *
 * @param service         指向住院服务结构体
 * @param out_admissions  输出参数，住院记录链表
 * @return Result         操作结果
 */
static Result InpatientService_load_admissions(
    InpatientService *service,
    LinkedList *out_admissions
) {
    return AdmissionRepository_load_all(&service->admission_repository, out_admissions);
}

/**
 * @brief 在患者链表中按ID查找患者
 *
 * @param patients    患者链表
 * @param patient_id  待查找的患者ID
 * @return Patient*   找到时返回指针，未找到返回 NULL
 */
static Patient *InpatientService_find_patient(LinkedList *patients, const char *patient_id) {
    LinkedListNode *current = 0;

    if (patients == 0 || patient_id == 0) {
        return 0;
    }

    current = patients->head;
    while (current != 0) {
        Patient *patient = (Patient *)current->data;

        if (strcmp(patient->patient_id, patient_id) == 0) {
            return patient;
        }

        current = current->next;
    }

    return 0;
}

/**
 * @brief 在病房链表中按ID查找病房
 *
 * @param wards    病房链表
 * @param ward_id  待查找的病房ID
 * @return Ward*   找到时返回指针，未找到返回 NULL
 */
static Ward *InpatientService_find_ward(LinkedList *wards, const char *ward_id) {
    LinkedListNode *current = 0;

    if (wards == 0 || ward_id == 0) {
        return 0;
    }

    current = wards->head;
    while (current != 0) {
        Ward *ward = (Ward *)current->data;

        if (strcmp(ward->ward_id, ward_id) == 0) {
            return ward;
        }

        current = current->next;
    }

    return 0;
}

/**
 * @brief 在床位链表中按ID查找床位
 *
 * @param beds    床位链表
 * @param bed_id  待查找的床位ID
 * @return Bed*   找到时返回指针，未找到返回 NULL
 */
static Bed *InpatientService_find_bed(LinkedList *beds, const char *bed_id) {
    LinkedListNode *current = 0;

    if (beds == 0 || bed_id == 0) {
        return 0;
    }

    current = beds->head;
    while (current != 0) {
        Bed *bed = (Bed *)current->data;

        if (strcmp(bed->bed_id, bed_id) == 0) {
            return bed;
        }

        current = current->next;
    }

    return 0;
}

/**
 * @brief 在住院记录链表中按ID查找住院记录
 *
 * @param admissions    住院记录链表
 * @param admission_id  待查找的住院记录ID
 * @return Admission*   找到时返回指针，未找到返回 NULL
 */
static Admission *InpatientService_find_admission(
    LinkedList *admissions,
    const char *admission_id
) {
    LinkedListNode *current = 0;

    if (admissions == 0 || admission_id == 0) {
        return 0;
    }

    current = admissions->head;
    while (current != 0) {
        Admission *admission = (Admission *)current->data;

        if (strcmp(admission->admission_id, admission_id) == 0) {
            return admission;
        }

        current = current->next;
    }

    return 0;
}

/**
 * @brief 检查指定患者是否有活跃的住院记录
 *
 * @param admissions  住院记录链表
 * @param patient_id  患者ID
 * @return int        1=有活跃记录，0=无
 */
static int InpatientService_has_active_admission_for_patient(
    const LinkedList *admissions,
    const char *patient_id
) {
    const LinkedListNode *current = 0;

    if (admissions == 0 || patient_id == 0) {
        return 0;
    }

    current = admissions->head;
    while (current != 0) {
        const Admission *admission = (const Admission *)current->data;

        if (admission->status == ADMISSION_STATUS_ACTIVE &&
            strcmp(admission->patient_id, patient_id) == 0) {
            return 1;
        }

        current = current->next;
    }

    return 0;
}

/**
 * @brief 检查指定床位是否有活跃的住院记录
 *
 * @param admissions  住院记录链表
 * @param bed_id      床位ID
 * @return int        1=有活跃记录，0=无
 */
static int InpatientService_has_active_admission_for_bed(
    const LinkedList *admissions,
    const char *bed_id
) {
    const LinkedListNode *current = 0;

    if (admissions == 0 || bed_id == 0) {
        return 0;
    }

    current = admissions->head;
    while (current != 0) {
        const Admission *admission = (const Admission *)current->data;

        if (admission->status == ADMISSION_STATUS_ACTIVE &&
            strcmp(admission->bed_id, bed_id) == 0) {
            return 1;
        }

        current = current->next;
    }

    return 0;
}

/**
 * @brief 从已加载的住院记录列表中计算下一个序列号
 *
 * @param admissions    已加载的住院记录链表
 * @param out_sequence  输出参数，下一个可用序列号
 * @return Result       操作结果
 */
static Result InpatientService_next_admission_sequence_from_list(
    const LinkedList *admissions,
    int *out_sequence
) {
    LinkedListNode *current = 0;
    int max_sequence = 0;
    const size_t prefix_len = strlen(INPATIENT_SERVICE_ID_PREFIX);

    if (out_sequence == 0) {
        return Result_make_failure("admission sequence output missing");
    }

    /* 遍历所有住院记录，提取ID中的序列号并找到最大值 */
    current = admissions->head;
    while (current != 0) {
        const Admission *admission = (const Admission *)current->data;
        const char *suffix = admission->admission_id + prefix_len;
        char *end_pointer = 0;
        long value = 0;

        /* 验证ID前缀格式 */
        if (strncmp(
                admission->admission_id,
                INPATIENT_SERVICE_ID_PREFIX,
                prefix_len
            ) != 0) {
            return Result_make_failure("admission id format invalid");
        }

        /* 解析序列号 */
        value = strtol(suffix, &end_pointer, 10);
        if (end_pointer == suffix || end_pointer == 0 || *end_pointer != '\0' || value < 0) {
            return Result_make_failure("admission id format invalid");
        }

        if ((int)value > max_sequence) {
            max_sequence = (int)value;
        }

        current = current->next;
    }

    *out_sequence = max_sequence + 1;
    return Result_make_success("admission sequence ready");
}

/**
 * @brief 从存储中计算下一个住院记录序列号
 *
 * @param service       指向住院服务结构体
 * @param out_sequence  输出参数，下一个可用序列号
 * @return Result       操作结果
 */
static Result InpatientService_next_admission_sequence(
    InpatientService *service,
    int *out_sequence
) {
    LinkedList admissions;
    Result result;

    if (out_sequence == 0) {
        return Result_make_failure("admission sequence output missing");
    }

    result = InpatientService_load_admissions(service, &admissions);
    if (result.success == 0) {
        return result;
    }

    /* Delegate to the list-based sequence extraction */
    result = InpatientService_next_admission_sequence_from_list(
        &admissions, out_sequence
    );
    AdmissionRepository_clear_loaded_list(&admissions);
    return result;
}

/** @brief Number of repositories coordinated by save_all */
#define INPATIENT_SERVICE_REPO_COUNT 4

/** @brief Initial capacity for content buffers used during save */
#define INPATIENT_SERVICE_CONTENT_INITIAL_CAPACITY 4096

/**
 * @brief Append a string to a dynamically growing buffer
 *
 * Grows the buffer as needed. On failure the caller must free *buffer.
 *
 * @param buffer    Pointer to the heap-allocated buffer (may be reallocated)
 * @param size      Current used size (updated on success)
 * @param capacity  Current buffer capacity (updated on growth)
 * @param text      String to append
 * @return 1 on success, 0 on allocation failure
 */
static int InpatientService_append_to_buffer(
    char **buffer, size_t *size, size_t *capacity, const char *text
) {
    size_t text_len = strlen(text);
    size_t needed = *size + text_len + 1;

    while (needed > *capacity) {
        size_t new_capacity = *capacity * 2;
        char *new_buffer = (char *)realloc(*buffer, new_capacity);
        if (new_buffer == 0) {
            return 0;
        }
        *buffer = new_buffer;
        *capacity = new_capacity;
    }

    memcpy(*buffer + *size, text, text_len);
    *size += text_len;
    (*buffer)[*size] = '\0';
    return 1;
}

/**
 * @brief Build the complete file content for the patient repository
 *
 * Formats header + one line per patient into a single heap-allocated string.
 *
 * @param patients   Patient linked list
 * @param out_content  Output: heap-allocated content string (caller must free)
 * @return Result
 */
static Result InpatientService_build_patient_content(
    const LinkedList *patients, char **out_content
) {
    char line[TEXT_FILE_REPOSITORY_LINE_CAPACITY];
    size_t capacity = INPATIENT_SERVICE_CONTENT_INITIAL_CAPACITY;
    size_t size = 0;
    char *buffer = 0;
    const LinkedListNode *current = 0;

    buffer = (char *)malloc(capacity);
    if (buffer == 0) {
        return Result_make_failure("failed to allocate patient content buffer");
    }
    buffer[0] = '\0';

    if (!InpatientService_append_to_buffer(
            &buffer, &size, &capacity, PATIENT_REPOSITORY_HEADER "\n")) {
        free(buffer);
        return Result_make_failure("failed to build patient content");
    }

    current = patients->head;
    while (current != 0) {
        const Patient *p = (const Patient *)current->data;
        int written = snprintf(line, sizeof(line),
            "%s|%s|%d|%d|%s|%s|%s|%s|%d|%s\n",
            p->patient_id, p->name, (int)p->gender, p->age,
            p->contact, p->id_card, p->allergy, p->medical_history,
            p->is_inpatient, p->remarks);
        if (written < 0 || (size_t)written >= sizeof(line)) {
            free(buffer);
            return Result_make_failure("patient line formatting failed");
        }
        if (!InpatientService_append_to_buffer(&buffer, &size, &capacity, line)) {
            free(buffer);
            return Result_make_failure("failed to build patient content");
        }
        current = current->next;
    }

    *out_content = buffer;
    return Result_make_success("patient content built");
}

/**
 * @brief Build the complete file content for the ward repository
 *
 * @param wards       Ward linked list
 * @param out_content Output: heap-allocated content string (caller must free)
 * @return Result
 */
static Result InpatientService_build_ward_content(
    const LinkedList *wards, char **out_content
) {
    char line[TEXT_FILE_REPOSITORY_LINE_CAPACITY];
    size_t capacity = INPATIENT_SERVICE_CONTENT_INITIAL_CAPACITY;
    size_t size = 0;
    char *buffer = 0;
    const LinkedListNode *current = 0;

    buffer = (char *)malloc(capacity);
    if (buffer == 0) {
        return Result_make_failure("failed to allocate ward content buffer");
    }
    buffer[0] = '\0';

    if (!InpatientService_append_to_buffer(
            &buffer, &size, &capacity, WARD_REPOSITORY_HEADER "\n")) {
        free(buffer);
        return Result_make_failure("failed to build ward content");
    }

    current = wards->head;
    while (current != 0) {
        const Ward *w = (const Ward *)current->data;
        int written = snprintf(line, sizeof(line),
            "%s|%s|%s|%s|%d|%d|%d|%d|%.2f\n",
            w->ward_id, w->name, w->department_id, w->location,
            w->capacity, w->occupied_beds, (int)w->status,
            (int)w->ward_type, w->daily_fee);
        if (written < 0 || (size_t)written >= sizeof(line)) {
            free(buffer);
            return Result_make_failure("ward line formatting failed");
        }
        if (!InpatientService_append_to_buffer(&buffer, &size, &capacity, line)) {
            free(buffer);
            return Result_make_failure("failed to build ward content");
        }
        current = current->next;
    }

    *out_content = buffer;
    return Result_make_success("ward content built");
}

/**
 * @brief Build the complete file content for the bed repository
 *
 * @param beds        Bed linked list
 * @param out_content Output: heap-allocated content string (caller must free)
 * @return Result
 */
static Result InpatientService_build_bed_content(
    const LinkedList *beds, char **out_content
) {
    char line[TEXT_FILE_REPOSITORY_LINE_CAPACITY];
    size_t capacity = INPATIENT_SERVICE_CONTENT_INITIAL_CAPACITY;
    size_t size = 0;
    char *buffer = 0;
    const LinkedListNode *current = 0;

    buffer = (char *)malloc(capacity);
    if (buffer == 0) {
        return Result_make_failure("failed to allocate bed content buffer");
    }
    buffer[0] = '\0';

    if (!InpatientService_append_to_buffer(
            &buffer, &size, &capacity, BED_REPOSITORY_HEADER "\n")) {
        free(buffer);
        return Result_make_failure("failed to build bed content");
    }

    current = beds->head;
    while (current != 0) {
        const Bed *b = (const Bed *)current->data;
        int written = snprintf(line, sizeof(line),
            "%s|%s|%s|%s|%d|%s|%s|%s\n",
            b->bed_id, b->ward_id, b->room_no, b->bed_no,
            (int)b->status, b->current_admission_id,
            b->occupied_at, b->released_at);
        if (written < 0 || (size_t)written >= sizeof(line)) {
            free(buffer);
            return Result_make_failure("bed line formatting failed");
        }
        if (!InpatientService_append_to_buffer(&buffer, &size, &capacity, line)) {
            free(buffer);
            return Result_make_failure("failed to build bed content");
        }
        current = current->next;
    }

    *out_content = buffer;
    return Result_make_success("bed content built");
}

/**
 * @brief Build the complete file content for the admission repository
 *
 * @param admissions  Admission linked list
 * @param out_content Output: heap-allocated content string (caller must free)
 * @return Result
 */
static Result InpatientService_build_admission_content(
    const LinkedList *admissions, char **out_content
) {
    char line[TEXT_FILE_REPOSITORY_LINE_CAPACITY];
    size_t capacity = INPATIENT_SERVICE_CONTENT_INITIAL_CAPACITY;
    size_t size = 0;
    char *buffer = 0;
    const LinkedListNode *current = 0;

    buffer = (char *)malloc(capacity);
    if (buffer == 0) {
        return Result_make_failure("failed to allocate admission content buffer");
    }
    buffer[0] = '\0';

    if (!InpatientService_append_to_buffer(
            &buffer, &size, &capacity, ADMISSION_REPOSITORY_HEADER "\n")) {
        free(buffer);
        return Result_make_failure("failed to build admission content");
    }

    current = admissions->head;
    while (current != 0) {
        const Admission *a = (const Admission *)current->data;
        int written = snprintf(line, sizeof(line),
            "%s|%s|%s|%s|%s|%d|%s|%s\n",
            a->admission_id, a->patient_id, a->ward_id, a->bed_id,
            a->admitted_at, (int)a->status, a->discharged_at, a->summary);
        if (written < 0 || (size_t)written >= sizeof(line)) {
            free(buffer);
            return Result_make_failure("admission line formatting failed");
        }
        if (!InpatientService_append_to_buffer(&buffer, &size, &capacity, line)) {
            free(buffer);
            return Result_make_failure("failed to build admission content");
        }
        current = current->next;
    }

    *out_content = buffer;
    return Result_make_success("admission content built");
}

/**
 * @brief Write a content string to a temporary file
 *
 * @param tmp_path  Path of the temporary file to write
 * @param content   Content string to write
 * @return Result
 */
static Result InpatientService_write_temp_file(const char *tmp_path, const char *content) {
    FILE *file = fopen(tmp_path, "w");
    if (file == 0) {
        return Result_make_failure("failed to open temp file for save");
    }
    if (fputs(content, file) == EOF) {
        fclose(file);
        remove(tmp_path);
        return Result_make_failure("failed to write temp file for save");
    }
    if (ferror(file) != 0) {
        fclose(file);
        remove(tmp_path);
        return Result_make_failure("temp file write error during save");
    }
    fclose(file);
    return Result_make_success("temp file written");
}

/**
 * @brief 检查文件是否存在
 *
 * @param path  文件路径
 * @return 1=存在，0=不存在
 */
static int InpatientService_file_exists(const char *path) {
    FILE *file = fopen(path, "rb");
    if (file == 0) {
        return 0;
    }
    fclose(file);
    return 1;
}

/**
 * @brief 将已提交的正式文件用对应的 .bak 备份回滚
 *
 * 用于 Phase 3 提交中途失败时恢复已替换的文件。committed_count 指示
 * 已成功 rename 的索引数量（[0, committed_count)）。
 *
 * @param paths            正式文件路径数组
 * @param bak_paths        对应的 .bak 备份路径数组
 * @param had_backup       每个索引是否产生过备份（原文件存在时为 1）
 * @param committed_count  已成功提交的正式文件数量
 */
static void InpatientService_rollback_committed(
    const char *paths[],
    char bak_paths[][TEXT_FILE_REPOSITORY_PATH_CAPACITY + 8],
    const int had_backup[],
    int committed_count
) {
    int k = 0;
    for (k = 0; k < committed_count; k++) {
        if (had_backup[k]) {
            /* 删掉已覆盖的正式文件，再把 .bak 改回正式文件名 */
            remove(paths[k]);
            rename(bak_paths[k], paths[k]);
        } else {
            /* 原先无此文件，直接删掉新写入的即可 */
            remove(paths[k]);
        }
    }
}

/**
 * @brief 保存所有仓库数据（患者、病房、床位、住院记录）
 *
 * 使用"prepare-backup-commit-rollback"模式确保跨 4 个文件的一致性：
 * 1. 在内存中构建所有 4 个文件的完整内容
 * 2. 将所有内容写入临时文件（path.tmp）
 * 3. 备份：把每个已存在的正式文件 rename 为 .bak
 * 4. 提交：依次将临时文件重命名为正式文件
 *    - 任一步失败：用 .bak 恢复已提交的正式文件，删除剩余 temp，返回失败
 * 5. 全部成功后删除所有 .bak
 *
 * @param service     指向住院服务结构体
 * @param patients    患者链表
 * @param wards       病房链表
 * @param beds        床位链表
 * @param admissions  住院记录链表
 * @return Result     操作结果
 */
static Result InpatientService_save_all(
    InpatientService *service,
    LinkedList *patients,
    LinkedList *wards,
    LinkedList *beds,
    LinkedList *admissions
) {
    /* File paths, temp paths and backup paths for all 4 repositories */
    const char *paths[INPATIENT_SERVICE_REPO_COUNT];
    char tmp_paths[INPATIENT_SERVICE_REPO_COUNT][TEXT_FILE_REPOSITORY_PATH_CAPACITY + 8];
    char bak_paths[INPATIENT_SERVICE_REPO_COUNT][TEXT_FILE_REPOSITORY_PATH_CAPACITY + 8];
    int had_backup[INPATIENT_SERVICE_REPO_COUNT];
    char *contents[INPATIENT_SERVICE_REPO_COUNT];
    int i = 0;
    int j = 0;
    Result result;

    memset(contents, 0, sizeof(contents));
    memset(had_backup, 0, sizeof(had_backup));

    paths[0] = service->patient_repository.file_repository.path;
    paths[1] = service->ward_repository.storage.path;
    paths[2] = service->bed_repository.storage.path;
    paths[3] = service->admission_repository.storage.path;

    /* Build temp and backup file paths */
    for (i = 0; i < INPATIENT_SERVICE_REPO_COUNT; i++) {
        if (snprintf(tmp_paths[i], sizeof(tmp_paths[i]), "%s.tmp", paths[i])
                >= (int)sizeof(tmp_paths[i])) {
            return Result_make_failure("repository path too long for temp file");
        }
        if (snprintf(bak_paths[i], sizeof(bak_paths[i]), "%s.bak", paths[i])
                >= (int)sizeof(bak_paths[i])) {
            return Result_make_failure("repository path too long for backup file");
        }
    }

    /* Phase 1: Build all content strings in memory */
    result = InpatientService_build_patient_content(patients, &contents[0]);
    if (result.success == 0) {
        return result;
    }

    result = InpatientService_build_ward_content(wards, &contents[1]);
    if (result.success == 0) {
        free(contents[0]);
        return result;
    }

    result = InpatientService_build_bed_content(beds, &contents[2]);
    if (result.success == 0) {
        free(contents[0]);
        free(contents[1]);
        return result;
    }

    result = InpatientService_build_admission_content(admissions, &contents[3]);
    if (result.success == 0) {
        free(contents[0]);
        free(contents[1]);
        free(contents[2]);
        return result;
    }

    /* Phase 2: Write all content to temp files */
    for (i = 0; i < INPATIENT_SERVICE_REPO_COUNT; i++) {
        result = InpatientService_write_temp_file(tmp_paths[i], contents[i]);
        if (result.success == 0) {
            /* Clean up: remove any temp files already written */
            for (j = 0; j < i; j++) {
                remove(tmp_paths[j]);
            }
            for (j = 0; j < INPATIENT_SERVICE_REPO_COUNT; j++) {
                free(contents[j]);
            }
            return result;
        }
    }

    /* Phase 3a: Backup every existing final file to path.bak so we can roll
     * back any partial commit. Files that don't yet exist (first save) have
     * no backup; their rollback is simply `remove(paths[i])`. */
    for (i = 0; i < INPATIENT_SERVICE_REPO_COUNT; i++) {
        if (InpatientService_file_exists(paths[i])) {
            /* Stale leftover from a previous crash would block rename on
             * Windows and confuse rollback; clear it first. */
            remove(bak_paths[i]);
            if (rename(paths[i], bak_paths[i]) != 0) {
                /* Backup failed: restore any prior backups and drop temps. */
                for (j = 0; j < i; j++) {
                    if (had_backup[j]) {
                        remove(paths[j]);
                        rename(bak_paths[j], paths[j]);
                    }
                }
                for (j = 0; j < INPATIENT_SERVICE_REPO_COUNT; j++) {
                    remove(tmp_paths[j]);
                    free(contents[j]);
                }
                return Result_make_failure("failed to backup repository file");
            }
            had_backup[i] = 1;
        }
    }

    /* Phase 3b: Commit - rename all temp files to final paths. If any rename
     * fails, roll back fully using the .bak files produced in Phase 3a. */
    for (i = 0; i < INPATIENT_SERVICE_REPO_COUNT; i++) {
#if defined(_WIN32)
        /* On Windows rename cannot overwrite; after Phase 3a the final path
         * should already be free, but remove defensively in case a stale file
         * reappeared (e.g. read-sharing tools). */
        remove(paths[i]);
#endif
        if (rename(tmp_paths[i], paths[i]) != 0) {
            /* Roll back the i files already committed */
            InpatientService_rollback_committed(paths, bak_paths, had_backup, i);
            /* Remove remaining temps (including the one that failed) */
            for (j = i; j < INPATIENT_SERVICE_REPO_COUNT; j++) {
                remove(tmp_paths[j]);
            }
            /* Clean up any un-restored backups (indices >= i that still have
             * their .bak sitting on disk because their final file was never
             * touched). Their originals are already intact at paths[j], so
             * just delete the stale backup. */
            for (j = i; j < INPATIENT_SERVICE_REPO_COUNT; j++) {
                if (had_backup[j]) {
                    remove(bak_paths[j]);
                }
            }
            for (j = 0; j < INPATIENT_SERVICE_REPO_COUNT; j++) {
                free(contents[j]);
            }
            return Result_make_failure("failed to commit repository files");
        }
    }

    /* Phase 4: All commits succeeded - discard backups */
    for (i = 0; i < INPATIENT_SERVICE_REPO_COUNT; i++) {
        if (had_backup[i]) {
            remove(bak_paths[i]);
        }
    }

    /* Free all content buffers */
    for (i = 0; i < INPATIENT_SERVICE_REPO_COUNT; i++) {
        free(contents[i]);
    }

    return Result_make_success("all repositories saved");
}

/**
 * @brief 释放所有已加载的链表数据
 *
 * @param patients    患者链表
 * @param wards       病房链表
 * @param beds        床位链表
 * @param admissions  住院记录链表
 */
static void InpatientService_clear_all(
    LinkedList *patients,
    LinkedList *wards,
    LinkedList *beds,
    LinkedList *admissions
) {
    PatientRepository_clear_loaded_list(patients);
    WardRepository_clear_loaded_list(wards);
    BedRepository_clear_loaded_list(beds);
    AdmissionRepository_clear_loaded_list(admissions);
}

/**
 * @brief 初始化住院服务
 *
 * 分别初始化患者、病房、床位和住院记录仓库。
 *
 * @param service         指向待初始化的住院服务结构体
 * @param patient_path    患者数据文件路径
 * @param ward_path       病房数据文件路径
 * @param bed_path        床位数据文件路径
 * @param admission_path  住院记录数据文件路径
 * @return Result         操作结果
 */
Result InpatientService_init(
    InpatientService *service,
    const char *patient_path,
    const char *ward_path,
    const char *bed_path,
    const char *admission_path
) {
    Result result;

    if (service == 0) {
        return Result_make_failure("inpatient service missing");
    }

    result = PatientRepository_init(&service->patient_repository, patient_path);
    if (result.success == 0) {
        return result;
    }

    result = WardRepository_init(&service->ward_repository, ward_path);
    if (result.success == 0) {
        return result;
    }

    result = BedRepository_init(&service->bed_repository, bed_path);
    if (result.success == 0) {
        return result;
    }

    return AdmissionRepository_init(&service->admission_repository, admission_path);
}

/**
 * @brief 患者入院
 *
 * 流程：校验所有必填参数 -> 加载四个仓库的全量数据 ->
 * 验证患者、病房、床位的存在性和状态 -> 检查患者未住院、病房有容量、床位可分配 ->
 * 生成住院ID -> 创建住院记录 -> 更新患者住院状态、病房占用数、床位状态 ->
 * 保存全量数据。
 *
 * @param service        指向住院服务结构体
 * @param patient_id     患者ID
 * @param ward_id        病房ID
 * @param bed_id         床位ID
 * @param admitted_at    入院时间字符串
 * @param summary        入院摘要
 * @param out_admission  输出参数，入院成功时存放住院记录
 * @return Result        操作结果
 */
Result InpatientService_admit_patient(
    InpatientService *service,
    const char *patient_id,
    const char *ward_id,
    const char *bed_id,
    const char *admitted_at,
    const char *summary,
    Admission *out_admission
) {
    LinkedList patients;
    LinkedList wards;
    LinkedList beds;
    LinkedList admissions;
    Patient *patient = 0;
    Ward *ward = 0;
    Bed *bed = 0;
    Admission *admission = 0;
    int next_sequence = 0;
    Result result = InpatientService_validate_required_text(patient_id, "patient id");

    if (result.success == 0) {
        return result;
    }

    result = InpatientService_validate_required_text(ward_id, "ward id");
    if (result.success == 0) {
        return result;
    }

    result = InpatientService_validate_required_text(bed_id, "bed id");
    if (result.success == 0) {
        return result;
    }

    result = InpatientService_validate_required_text(admitted_at, "admitted at");
    if (result.success == 0) {
        return result;
    }

    result = InpatientService_validate_required_text(summary, "admission summary");
    if (result.success == 0) {
        return result;
    }

    if (service == 0 || out_admission == 0) {
        return Result_make_failure("inpatient service arguments missing");
    }

    /* 依次加载四个仓库的全量数据 */
    result = InpatientService_load_patients(service, &patients);
    if (result.success == 0) {
        return result;
    }

    result = InpatientService_load_wards(service, &wards);
    if (result.success == 0) {
        PatientRepository_clear_loaded_list(&patients);
        return result;
    }

    result = InpatientService_load_beds(service, &beds);
    if (result.success == 0) {
        PatientRepository_clear_loaded_list(&patients);
        WardRepository_clear_loaded_list(&wards);
        return result;
    }

    result = InpatientService_load_admissions(service, &admissions);
    if (result.success == 0) {
        PatientRepository_clear_loaded_list(&patients);
        WardRepository_clear_loaded_list(&wards);
        BedRepository_clear_loaded_list(&beds);
        return result;
    }

    /* 查找并验证患者、病房、床位是否存在 */
    patient = InpatientService_find_patient(&patients, patient_id);
    ward = InpatientService_find_ward(&wards, ward_id);
    bed = InpatientService_find_bed(&beds, bed_id);
    if (patient == 0 || ward == 0 || bed == 0) {
        InpatientService_clear_all(&patients, &wards, &beds, &admissions);
        return Result_make_failure("inpatient related entity not found");
    }

    /* 检查患者是否已经住院 */
    if (patient->is_inpatient != 0 || InpatientService_has_active_admission_for_patient(&admissions, patient_id)) {
        InpatientService_clear_all(&patients, &wards, &beds, &admissions);
        return Result_make_failure("patient already admitted");
    }

    /* 检查病房是否有容量（状态活跃且有空余床位） */
    if (ward->status != WARD_STATUS_ACTIVE || !Ward_has_capacity(ward)) {
        InpatientService_clear_all(&patients, &wards, &beds, &admissions);
        return Result_make_failure("ward has no capacity");
    }

    /* 检查床位是否可分配（属于目标病房、状态可用、无活跃住院记录） */
    if (strcmp(bed->ward_id, ward_id) != 0 || !Bed_is_assignable(bed) ||
        InpatientService_has_active_admission_for_bed(&admissions, bed_id)) {
        InpatientService_clear_all(&patients, &wards, &beds, &admissions);
        return Result_make_failure("bed not assignable");
    }

    /* 生成住院记录序列号 */
    result = InpatientService_next_admission_sequence_from_list(&admissions, &next_sequence);
    if (result.success == 0) {
        InpatientService_clear_all(&patients, &wards, &beds, &admissions);
        return result;
    }

    /* 分配并初始化住院记录 */
    admission = (Admission *)malloc(sizeof(*admission));
    if (admission == 0) {
        InpatientService_clear_all(&patients, &wards, &beds, &admissions);
        return Result_make_failure("failed to allocate admission");
    }

    memset(admission, 0, sizeof(*admission));
    /* 生成格式化的住院ID（如 ADM0001） */
    if (!IdGenerator_format(
            admission->admission_id,
            sizeof(admission->admission_id),
            INPATIENT_SERVICE_ID_PREFIX,
            next_sequence,
            INPATIENT_SERVICE_ID_WIDTH
        )) {
        free(admission);
        InpatientService_clear_all(&patients, &wards, &beds, &admissions);
        return Result_make_failure("failed to generate admission id");
    }

    /* 填充住院记录各字段 */
    strncpy(admission->patient_id, patient_id, sizeof(admission->patient_id) - 1);
    strncpy(admission->ward_id, ward_id, sizeof(admission->ward_id) - 1);
    strncpy(admission->bed_id, bed_id, sizeof(admission->bed_id) - 1);
    strncpy(admission->admitted_at, admitted_at, sizeof(admission->admitted_at) - 1);
    strncpy(admission->summary, summary, sizeof(admission->summary) - 1);
    admission->status = ADMISSION_STATUS_ACTIVE;

    /* 同步更新关联实体状态：患者标记为住院、病房占用+1、床位标记为已占用 */
    patient->is_inpatient = 1;
    if (!Ward_occupy_bed(ward) || !Bed_assign(bed, admission->admission_id, admitted_at)) {
        free(admission);
        InpatientService_clear_all(&patients, &wards, &beds, &admissions);
        return Result_make_failure("failed to assign inpatient resources");
    }

    /* 将新住院记录追加到链表 */
    if (!LinkedList_append(&admissions, admission)) {
        free(admission);
        InpatientService_clear_all(&patients, &wards, &beds, &admissions);
        return Result_make_failure("failed to append admission");
    }

    /* 保存所有仓库的全量数据 */
    result = InpatientService_save_all(service, &patients, &wards, &beds, &admissions);
    if (result.success == 0) {
        InpatientService_clear_all(&patients, &wards, &beds, &admissions);
        return result;
    }

    *out_admission = *admission;
    InpatientService_clear_all(&patients, &wards, &beds, &admissions);
    return Result_make_success("patient admitted");
}

/**
 * @brief 患者出院
 *
 * 流程：校验参数 -> 加载四个仓库数据 -> 查找活跃住院记录 ->
 * 查找关联的患者、病房、床位 -> 验证状态一致性 ->
 * 更新患者住院标志、住院记录状态、释放床位和病房资源 -> 保存全量数据。
 *
 * @param service        指向住院服务结构体
 * @param admission_id   住院记录ID
 * @param discharged_at  出院时间字符串
 * @param summary        出院摘要
 * @param out_admission  输出参数，出院成功时存放更新后的住院记录
 * @return Result        操作结果
 */
Result InpatientService_discharge_patient(
    InpatientService *service,
    const char *admission_id,
    const char *discharged_at,
    const char *summary,
    Admission *out_admission
) {
    LinkedList patients;
    LinkedList wards;
    LinkedList beds;
    LinkedList admissions;
    Admission *admission = 0;
    Patient *patient = 0;
    Ward *ward = 0;
    Bed *bed = 0;
    Result result = InpatientService_validate_required_text(admission_id, "admission id");

    if (result.success == 0) {
        return result;
    }

    result = InpatientService_validate_required_text(discharged_at, "discharged at");
    if (result.success == 0) {
        return result;
    }

    result = InpatientService_validate_required_text(summary, "discharge summary");
    if (result.success == 0) {
        return result;
    }

    if (service == 0 || out_admission == 0) {
        return Result_make_failure("inpatient service arguments missing");
    }

    /* 加载四个仓库的全量数据 */
    result = InpatientService_load_patients(service, &patients);
    if (result.success == 0) {
        return result;
    }

    result = InpatientService_load_wards(service, &wards);
    if (result.success == 0) {
        PatientRepository_clear_loaded_list(&patients);
        return result;
    }

    result = InpatientService_load_beds(service, &beds);
    if (result.success == 0) {
        PatientRepository_clear_loaded_list(&patients);
        WardRepository_clear_loaded_list(&wards);
        return result;
    }

    result = InpatientService_load_admissions(service, &admissions);
    if (result.success == 0) {
        PatientRepository_clear_loaded_list(&patients);
        WardRepository_clear_loaded_list(&wards);
        BedRepository_clear_loaded_list(&beds);
        return result;
    }

    /* 查找活跃的住院记录 */
    admission = InpatientService_find_admission(&admissions, admission_id);
    if (admission == 0 || admission->status != ADMISSION_STATUS_ACTIVE) {
        InpatientService_clear_all(&patients, &wards, &beds, &admissions);
        return Result_make_failure("active admission not found");
    }

    /* 查找关联的患者、病房、床位 */
    patient = InpatientService_find_patient(&patients, admission->patient_id);
    ward = InpatientService_find_ward(&wards, admission->ward_id);
    bed = InpatientService_find_bed(&beds, admission->bed_id);
    if (patient == 0 || ward == 0 || bed == 0) {
        InpatientService_clear_all(&patients, &wards, &beds, &admissions);
        return Result_make_failure("inpatient related entity not found");
    }

    /* 验证状态一致性：患者应为住院状态、床位应为已占用状态 */
    if (patient->is_inpatient == 0 || bed->status != BED_STATUS_OCCUPIED) {
        InpatientService_clear_all(&patients, &wards, &beds, &admissions);
        return Result_make_failure("inpatient state invalid");
    }

    /* 执行出院操作：更新患者住院标志、住院记录状态、释放床位和病房 */
    patient->is_inpatient = 0;
    strncpy(admission->summary, summary, sizeof(admission->summary) - 1);
    admission->summary[sizeof(admission->summary) - 1] = '\0';
    if (!Admission_discharge(admission, discharged_at) ||
        !Bed_release(bed, discharged_at) ||
        !Ward_release_bed(ward)) {
        InpatientService_clear_all(&patients, &wards, &beds, &admissions);
        return Result_make_failure("failed to discharge patient");
    }

    /* 保存全量数据 */
    result = InpatientService_save_all(service, &patients, &wards, &beds, &admissions);
    if (result.success == 0) {
        InpatientService_clear_all(&patients, &wards, &beds, &admissions);
        return result;
    }

    *out_admission = *admission;
    InpatientService_clear_all(&patients, &wards, &beds, &admissions);
    return Result_make_success("patient discharged");
}

/**
 * @brief 转床
 *
 * 流程：校验参数 -> 加载四个仓库数据 -> 查找住院记录和目标床位 ->
 * 验证当前状态（住院记录活跃、目标床位可分配、非同一床位） ->
 * 释放当前床位 -> 分配目标床位 -> 若跨病房则同步更新病房占用数 ->
 * 更新住院记录中的病房和床位ID -> 保存全量数据。
 *
 * @param service          指向住院服务结构体
 * @param admission_id     住院记录ID
 * @param target_bed_id    目标床位ID
 * @param transferred_at   转床时间字符串
 * @param out_admission    输出参数，转床成功时存放更新后的住院记录
 * @return Result          操作结果
 */
Result InpatientService_transfer_bed(
    InpatientService *service,
    const char *admission_id,
    const char *target_bed_id,
    const char *transferred_at,
    Admission *out_admission
) {
    LinkedList patients;
    LinkedList wards;
    LinkedList beds;
    LinkedList admissions;
    Admission *admission = 0;
    Bed *current_bed = 0;
    Bed *target_bed = 0;
    Ward *current_ward = 0;
    Ward *target_ward = 0;
    Result result = InpatientService_validate_required_text(admission_id, "admission id");

    if (result.success == 0) {
        return result;
    }

    result = InpatientService_validate_required_text(target_bed_id, "target bed id");
    if (result.success == 0) {
        return result;
    }

    result = InpatientService_validate_required_text(transferred_at, "transferred at");
    if (result.success == 0) {
        return result;
    }

    if (service == 0 || out_admission == 0) {
        return Result_make_failure("inpatient service arguments missing");
    }

    /* 加载四个仓库的全量数据 */
    result = InpatientService_load_patients(service, &patients);
    if (result.success == 0) {
        return result;
    }

    result = InpatientService_load_wards(service, &wards);
    if (result.success == 0) {
        PatientRepository_clear_loaded_list(&patients);
        return result;
    }

    result = InpatientService_load_beds(service, &beds);
    if (result.success == 0) {
        PatientRepository_clear_loaded_list(&patients);
        WardRepository_clear_loaded_list(&wards);
        return result;
    }

    result = InpatientService_load_admissions(service, &admissions);
    if (result.success == 0) {
        PatientRepository_clear_loaded_list(&patients);
        WardRepository_clear_loaded_list(&wards);
        BedRepository_clear_loaded_list(&beds);
        return result;
    }

    /* 查找住院记录和目标床位 */
    admission = InpatientService_find_admission(&admissions, admission_id);
    target_bed = InpatientService_find_bed(&beds, target_bed_id);
    if (admission == 0 || admission->status != ADMISSION_STATUS_ACTIVE || target_bed == 0) {
        InpatientService_clear_all(&patients, &wards, &beds, &admissions);
        return Result_make_failure("transfer target not found");
    }

    /* 查找当前床位和相关病房 */
    current_bed = InpatientService_find_bed(&beds, admission->bed_id);
    current_ward = InpatientService_find_ward(&wards, admission->ward_id);
    target_ward = InpatientService_find_ward(&wards, target_bed->ward_id);
    if (current_bed == 0 || current_ward == 0 || target_ward == 0) {
        InpatientService_clear_all(&patients, &wards, &beds, &admissions);
        return Result_make_failure("transfer related entity not found");
    }

    /* 不能转到同一张床位 */
    if (strcmp(current_bed->bed_id, target_bed->bed_id) == 0) {
        InpatientService_clear_all(&patients, &wards, &beds, &admissions);
        return Result_make_failure("target bed unchanged");
    }

    /* 检查目标床位是否可分配 */
    if (current_bed->status != BED_STATUS_OCCUPIED || !Bed_is_assignable(target_bed) ||
        InpatientService_has_active_admission_for_bed(&admissions, target_bed_id)) {
        InpatientService_clear_all(&patients, &wards, &beds, &admissions);
        return Result_make_failure("target bed not assignable");
    }

    /* 释放当前床位并分配目标床位 */
    if (!Bed_release(current_bed, transferred_at) ||
        !Bed_assign(target_bed, admission_id, transferred_at)) {
        InpatientService_clear_all(&patients, &wards, &beds, &admissions);
        return Result_make_failure("failed to transfer bed");
    }

    /* 跨病房转床时需同步更新病房占用数 */
    if (strcmp(current_ward->ward_id, target_ward->ward_id) != 0) {
        if (!Ward_release_bed(current_ward) || !Ward_occupy_bed(target_ward)) {
            InpatientService_clear_all(&patients, &wards, &beds, &admissions);
            return Result_make_failure("failed to transfer ward occupancy");
        }

        /* 更新住院记录中的病房ID */
        strncpy(admission->ward_id, target_ward->ward_id, sizeof(admission->ward_id) - 1);
        admission->ward_id[sizeof(admission->ward_id) - 1] = '\0';
    }

    /* 更新住院记录中的床位ID */
    strncpy(admission->bed_id, target_bed_id, sizeof(admission->bed_id) - 1);
    admission->bed_id[sizeof(admission->bed_id) - 1] = '\0';

    /* 保存全量数据 */
    result = InpatientService_save_all(service, &patients, &wards, &beds, &admissions);
    if (result.success == 0) {
        InpatientService_clear_all(&patients, &wards, &beds, &admissions);
        return result;
    }

    *out_admission = *admission;
    InpatientService_clear_all(&patients, &wards, &beds, &admissions);
    return Result_make_success("bed transferred");
}

/**
 * @brief 根据住院记录ID查找住院记录
 *
 * @param service        指向住院服务结构体
 * @param admission_id   住院记录ID
 * @param out_admission  输出参数，查找成功时存放住院记录
 * @return Result        操作结果
 */
Result InpatientService_find_by_id(
    InpatientService *service,
    const char *admission_id,
    Admission *out_admission
) {
    if (service == 0 || out_admission == 0) {
        return Result_make_failure("inpatient service arguments missing");
    }

    /* 委托仓库层按ID查找 */
    return AdmissionRepository_find_by_id(
        &service->admission_repository,
        admission_id,
        out_admission
    );
}

/**
 * @brief 根据患者ID查找当前活跃的住院记录
 *
 * @param service        指向住院服务结构体
 * @param patient_id     患者ID
 * @param out_admission  输出参数，查找成功时存放住院记录
 * @return Result        操作结果
 */
Result InpatientService_find_active_by_patient_id(
    InpatientService *service,
    const char *patient_id,
    Admission *out_admission
) {
    if (service == 0 || out_admission == 0) {
        return Result_make_failure("inpatient service arguments missing");
    }

    /* 委托仓库层按患者ID查找活跃住院记录 */
    return AdmissionRepository_find_active_by_patient_id(
        &service->admission_repository,
        patient_id,
        out_admission
    );
}
