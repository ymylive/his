/**
 * @file WardRepository.c
 * @brief 病区数据仓储层实现
 *
 * 实现病区（Ward）数据的持久化存储操作，包括：
 * - 病区数据的序列化与反序列化
 * - 数据校验（必填字段、容量合法性、状态值域、保留字符检测）
 * - 追加（含ID唯一性检查）、按ID查找、全量加载、全量保存
 */

#include "repository/WardRepository.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "repository/RepositoryUtils.h"

/** 加载病区时使用的上下文结构体 */
typedef struct WardRepositoryLoadContext {
    LinkedList *wards; /**< 用于存放加载结果的链表 */
} WardRepositoryLoadContext;

/** 判断文本是否非空 */
static int WardRepository_has_text(const char *text) {
    return text != 0 && text[0] != '\0';
}

/** 安全复制字符串 */
static void WardRepository_copy_text(
    char *destination, size_t capacity, const char *source
) {
    if (destination == 0 || capacity == 0) return;
    if (source == 0) { destination[0] = '\0'; return; }
    strncpy(destination, source, capacity - 1);
    destination[capacity - 1] = '\0';
}

/** 解析整数字段 */
static Result WardRepository_parse_int(const char *field, int *out_value) {
    char *end_pointer = 0;
    long value = 0;

    if (field == 0 || out_value == 0 || field[0] == '\0') {
        return Result_make_failure("ward integer missing");
    }
    value = strtol(field, &end_pointer, 10);
    if (end_pointer == field || end_pointer == 0 || *end_pointer != '\0') {
        return Result_make_failure("ward integer invalid");
    }
    *out_value = (int)value;
    return Result_make_success("ward integer parsed");
}

/**
 * @brief 校验病区数据的合法性
 *
 * 检查项目：必填字段非空、保留字符、容量>0、已占用数>=0且不超过容量、状态值域。
 */
static Result WardRepository_validate(const Ward *ward) {
    if (ward == 0) return Result_make_failure("ward missing");

    if (!WardRepository_has_text(ward->ward_id) ||
        !WardRepository_has_text(ward->name) ||
        !WardRepository_has_text(ward->department_id) ||
        !WardRepository_has_text(ward->location)) {
        return Result_make_failure("ward required field missing");
    }

    if (!RepositoryUtils_is_safe_field_text(ward->ward_id) ||
        !RepositoryUtils_is_safe_field_text(ward->name) ||
        !RepositoryUtils_is_safe_field_text(ward->department_id) ||
        !RepositoryUtils_is_safe_field_text(ward->location)) {
        return Result_make_failure("ward field contains reserved character");
    }

    /* 容量合法性：capacity > 0，occupied_beds 在 [0, capacity] 范围内 */
    if (ward->capacity <= 0 || ward->occupied_beds < 0 || ward->occupied_beds > ward->capacity) {
        return Result_make_failure("ward capacity invalid");
    }

    if (ward->status < WARD_STATUS_ACTIVE || ward->status > WARD_STATUS_CLOSED) {
        return Result_make_failure("ward status invalid");
    }

    return Result_make_success("ward valid");
}

/** 将病区结构体格式化为文本行 */
static Result WardRepository_format_line(
    const Ward *ward, char *line, size_t line_capacity
) {
    int written = 0;
    Result result = WardRepository_validate(ward);
    if (result.success == 0) return result;
    if (line == 0 || line_capacity == 0) return Result_make_failure("ward line buffer missing");

    written = snprintf(line, line_capacity, "%s|%s|%s|%s|%d|%d|%d",
        ward->ward_id, ward->name, ward->department_id, ward->location,
        ward->capacity, ward->occupied_beds, (int)ward->status);
    if (written < 0 || (size_t)written >= line_capacity) {
        return Result_make_failure("ward line too long");
    }
    return Result_make_success("ward formatted");
}

/** 将一行文本解析为病区结构体 */
static Result WardRepository_parse_line(const char *line, Ward *ward) {
    char mutable_line[TEXT_FILE_REPOSITORY_LINE_CAPACITY];
    char *fields[WARD_REPOSITORY_FIELD_COUNT];
    size_t field_count = 0;
    int status_value = 0;
    Result result;

    if (line == 0 || ward == 0) return Result_make_failure("ward line missing");

    WardRepository_copy_text(mutable_line, sizeof(mutable_line), line);
    result = RepositoryUtils_split_pipe_line(mutable_line, fields, WARD_REPOSITORY_FIELD_COUNT, &field_count);
    if (result.success == 0) return result;

    result = RepositoryUtils_validate_field_count(field_count, WARD_REPOSITORY_FIELD_COUNT);
    if (result.success == 0) return result;

    memset(ward, 0, sizeof(*ward));
    WardRepository_copy_text(ward->ward_id, sizeof(ward->ward_id), fields[0]);
    WardRepository_copy_text(ward->name, sizeof(ward->name), fields[1]);
    WardRepository_copy_text(ward->department_id, sizeof(ward->department_id), fields[2]);
    WardRepository_copy_text(ward->location, sizeof(ward->location), fields[3]);

    result = WardRepository_parse_int(fields[4], &ward->capacity);
    if (result.success == 0) return result;

    result = WardRepository_parse_int(fields[5], &ward->occupied_beds);
    if (result.success == 0) return result;

    result = WardRepository_parse_int(fields[6], &status_value);
    if (result.success == 0) return result;
    ward->status = (WardStatus)status_value;

    return WardRepository_validate(ward);
}

/** 确保病区数据文件包含正确的表头 */
static Result WardRepository_ensure_header(const WardRepository *repository) {
    char line[TEXT_FILE_REPOSITORY_LINE_CAPACITY];
    FILE *file = 0;
    Result result;

    if (repository == 0) return Result_make_failure("ward repository missing");

    result = TextFileRepository_ensure_file_exists(&repository->storage);
    if (result.success == 0) return result;

    file = fopen(repository->storage.path, "r");
    if (file == 0) return Result_make_failure("failed to inspect ward repository");

    while (fgets(line, sizeof(line), file) != 0) {
        RepositoryUtils_strip_line_endings(line);
        if (RepositoryUtils_is_blank_line(line)) continue;
        fclose(file);
        if (strcmp(line, WARD_REPOSITORY_HEADER) != 0) {
            return Result_make_failure("ward repository header mismatch");
        }
        return Result_make_success("ward header ready");
    }

    fclose(file);
    return TextFileRepository_append_line(&repository->storage, WARD_REPOSITORY_HEADER);
}

/** 加载病区的行处理回调 */
static Result WardRepository_collect_line(const char *line, void *context) {
    WardRepositoryLoadContext *load_context = (WardRepositoryLoadContext *)context;
    Ward *ward = 0;
    Result result;

    if (load_context == 0 || load_context->wards == 0) {
        return Result_make_failure("ward load context missing");
    }

    ward = (Ward *)malloc(sizeof(*ward));
    if (ward == 0) return Result_make_failure("failed to allocate ward");

    result = WardRepository_parse_line(line, ward);
    if (result.success == 0) { free(ward); return result; }

    if (!LinkedList_append(load_context->wards, ward)) {
        free(ward);
        return Result_make_failure("failed to append ward");
    }
    return Result_make_success("ward loaded");
}

/** 校验病区链表（含ID唯一性检查） */
static int WardRepository_validate_list(const LinkedList *wards) {
    const LinkedListNode *outer = 0;
    if (wards == 0) return 0;

    outer = wards->head;
    while (outer != 0) {
        const LinkedListNode *inner = outer->next;
        const Ward *ward = (const Ward *)outer->data;
        Result result = WardRepository_validate(ward);
        if (result.success == 0) return 0;

        while (inner != 0) {
            const Ward *other = (const Ward *)inner->data;
            if (strcmp(ward->ward_id, other->ward_id) == 0) return 0;
            inner = inner->next;
        }
        outer = outer->next;
    }
    return 1;
}

/** 在链表中按病区ID查找 */
static Ward *WardRepository_find_in_list(LinkedList *wards, const char *ward_id) {
    LinkedListNode *current = 0;
    if (wards == 0 || ward_id == 0) return 0;

    current = wards->head;
    while (current != 0) {
        Ward *ward = (Ward *)current->data;
        if (strcmp(ward->ward_id, ward_id) == 0) return ward;
        current = current->next;
    }
    return 0;
}

/** 初始化病区仓储 */
Result WardRepository_init(WardRepository *repository, const char *path) {
    Result result;
    if (repository == 0) return Result_make_failure("ward repository missing");

    result = TextFileRepository_init(&repository->storage, path);
    if (result.success == 0) return result;

    return WardRepository_ensure_header(repository);
}

/** 追加一条病区记录（含ID唯一性检查） */
Result WardRepository_append(const WardRepository *repository, const Ward *ward) {
    LinkedList wards;
    Ward *copy = 0;
    Result result = WardRepository_validate(ward);
    if (result.success == 0) return result;

    result = WardRepository_load_all(repository, &wards);
    if (result.success == 0) return result;

    if (WardRepository_find_in_list(&wards, ward->ward_id) != 0) {
        WardRepository_clear_loaded_list(&wards);
        return Result_make_failure("ward id already exists");
    }

    copy = (Ward *)malloc(sizeof(*copy));
    if (copy == 0) {
        WardRepository_clear_loaded_list(&wards);
        return Result_make_failure("failed to allocate ward copy");
    }
    *copy = *ward;
    if (!LinkedList_append(&wards, copy)) {
        free(copy);
        WardRepository_clear_loaded_list(&wards);
        return Result_make_failure("failed to append ward copy");
    }

    result = WardRepository_save_all(repository, &wards);
    WardRepository_clear_loaded_list(&wards);
    return result;
}

/** 按病区ID查找病区 */
Result WardRepository_find_by_id(
    const WardRepository *repository, const char *ward_id, Ward *out_ward
) {
    LinkedList wards;
    Ward *ward = 0;
    Result result;

    if (!WardRepository_has_text(ward_id) || out_ward == 0) {
        return Result_make_failure("ward query arguments missing");
    }

    result = WardRepository_load_all(repository, &wards);
    if (result.success == 0) return result;

    ward = WardRepository_find_in_list(&wards, ward_id);
    if (ward == 0) {
        WardRepository_clear_loaded_list(&wards);
        return Result_make_failure("ward not found");
    }

    *out_ward = *ward;
    WardRepository_clear_loaded_list(&wards);
    return Result_make_success("ward found");
}

/** 加载所有病区记录到链表 */
Result WardRepository_load_all(
    const WardRepository *repository, LinkedList *out_wards
) {
    WardRepositoryLoadContext context;
    Result result;

    if (out_wards == 0) return Result_make_failure("ward output list missing");

    LinkedList_init(out_wards);
    context.wards = out_wards;

    result = WardRepository_ensure_header(repository);
    if (result.success == 0) return result;

    result = TextFileRepository_for_each_data_line(
        &repository->storage, WardRepository_collect_line, &context
    );
    if (result.success == 0) {
        WardRepository_clear_loaded_list(out_wards);
        return result;
    }
    return Result_make_success("wards loaded");
}

/** 全量保存病区列表到文件（覆盖写入） */
Result WardRepository_save_all(
    const WardRepository *repository, const LinkedList *wards
) {
    const LinkedListNode *current = 0;
    FILE *file = 0;
    Result result;

    if (repository == 0 || wards == 0) {
        return Result_make_failure("ward save arguments missing");
    }
    if (!WardRepository_validate_list(wards)) {
        return Result_make_failure("ward list invalid");
    }

    result = TextFileRepository_ensure_file_exists(&repository->storage);
    if (result.success == 0) return result;

    file = fopen(repository->storage.path, "w");
    if (file == 0) return Result_make_failure("failed to rewrite ward repository");

    if (fprintf(file, "%s\n", WARD_REPOSITORY_HEADER) < 0) {
        fclose(file);
        return Result_make_failure("failed to write ward header");
    }

    current = wards->head;
    while (current != 0) {
        char line[TEXT_FILE_REPOSITORY_LINE_CAPACITY];
        result = WardRepository_format_line((const Ward *)current->data, line, sizeof(line));
        if (result.success == 0) { fclose(file); return result; }

        if (fprintf(file, "%s\n", line) < 0) {
            fclose(file);
            return Result_make_failure("failed to write ward line");
        }
        current = current->next;
    }

    fclose(file);
    return Result_make_success("wards saved");
}

/** 清空并释放病区链表中的所有元素 */
void WardRepository_clear_loaded_list(LinkedList *wards) {
    LinkedList_clear(wards, free);
}
