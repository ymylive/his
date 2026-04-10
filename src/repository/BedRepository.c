/**
 * @file BedRepository.c
 * @brief 床位数据仓储层实现
 *
 * 实现床位（Bed）数据的持久化存储操作，包括：
 * - 床位数据的序列化与反序列化
 * - 数据校验（必填字段、状态一致性、保留字符检测）
 * - 追加（含ID唯一性检查）、按ID查找、全量加载、全量保存
 */

#include "repository/BedRepository.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "repository/RepositoryUtils.h"

/** 加载床位时使用的上下文结构体 */
typedef struct BedRepositoryLoadContext {
    LinkedList *beds; /**< 用于存放加载结果的链表 */
} BedRepositoryLoadContext;

/** 判断文本是否非空 */
static int BedRepository_has_text(const char *text) {
    return text != 0 && text[0] != '\0';
}

/** 安全复制字符串 */
static void BedRepository_copy_text(
    char *destination, size_t capacity, const char *source
) {
    if (destination == 0 || capacity == 0) {
        return;
    }
    if (source == 0) {
        destination[0] = '\0';
        return;
    }
    strncpy(destination, source, capacity - 1);
    destination[capacity - 1] = '\0';
}

/** 解析整数字段 */
static Result BedRepository_parse_int(const char *field, int *out_value) {
    char *end_pointer = 0;
    long value = 0;

    if (field == 0 || out_value == 0 || field[0] == '\0') {
        return Result_make_failure("bed integer missing");
    }
    value = strtol(field, &end_pointer, 10);
    if (end_pointer == field || end_pointer == 0 || *end_pointer != '\0') {
        return Result_make_failure("bed integer invalid");
    }
    *out_value = (int)value;
    return Result_make_success("bed integer parsed");
}

/**
 * @brief 校验床位数据的合法性
 *
 * 检查必填字段、保留字符、状态值域，以及状态与入院数据的一致性：
 * - 占用状态时必须有入院ID和占用时间
 * - 非占用状态时不能有入院数据
 */
static Result BedRepository_validate(const Bed *bed) {
    if (bed == 0) {
        return Result_make_failure("bed missing");
    }

    if (!BedRepository_has_text(bed->bed_id) ||
        !BedRepository_has_text(bed->ward_id) ||
        !BedRepository_has_text(bed->room_no) ||
        !BedRepository_has_text(bed->bed_no)) {
        return Result_make_failure("bed required field missing");
    }

    if (!RepositoryUtils_is_safe_field_text(bed->bed_id) ||
        !RepositoryUtils_is_safe_field_text(bed->ward_id) ||
        !RepositoryUtils_is_safe_field_text(bed->room_no) ||
        !RepositoryUtils_is_safe_field_text(bed->bed_no) ||
        !RepositoryUtils_is_safe_field_text(bed->current_admission_id) ||
        !RepositoryUtils_is_safe_field_text(bed->occupied_at) ||
        !RepositoryUtils_is_safe_field_text(bed->released_at)) {
        return Result_make_failure("bed field contains reserved character");
    }

    if (bed->status < BED_STATUS_AVAILABLE || bed->status > BED_STATUS_MAINTENANCE) {
        return Result_make_failure("bed status invalid");
    }

    /* 占用状态必须有入院信息 */
    if (bed->status == BED_STATUS_OCCUPIED) {
        if (!BedRepository_has_text(bed->current_admission_id) ||
            !BedRepository_has_text(bed->occupied_at)) {
            return Result_make_failure("occupied bed missing admission data");
        }
    } else if (BedRepository_has_text(bed->current_admission_id) ||
               BedRepository_has_text(bed->occupied_at)) {
        return Result_make_failure("non-occupied bed has admission data");
    }

    return Result_make_success("bed valid");
}

/** 将床位结构体格式化为文本行 */
static Result BedRepository_format_line(
    const Bed *bed, char *line, size_t line_capacity
) {
    int written = 0;
    Result result = BedRepository_validate(bed);
    if (result.success == 0) return result;
    if (line == 0 || line_capacity == 0) return Result_make_failure("bed line buffer missing");

    written = snprintf(line, line_capacity, "%s|%s|%s|%s|%d|%s|%s|%s",
        bed->bed_id, bed->ward_id, bed->room_no, bed->bed_no,
        (int)bed->status, bed->current_admission_id,
        bed->occupied_at, bed->released_at);
    if (written < 0 || (size_t)written >= line_capacity) {
        return Result_make_failure("bed line too long");
    }
    return Result_make_success("bed formatted");
}

/** 将一行文本解析为床位结构体 */
static Result BedRepository_parse_line(const char *line, Bed *bed) {
    char mutable_line[TEXT_FILE_REPOSITORY_LINE_CAPACITY];
    char *fields[BED_REPOSITORY_FIELD_COUNT];
    size_t field_count = 0;
    int status_value = 0;
    Result result;

    if (line == 0 || bed == 0) return Result_make_failure("bed line missing");

    BedRepository_copy_text(mutable_line, sizeof(mutable_line), line);
    result = RepositoryUtils_split_pipe_line(mutable_line, fields, BED_REPOSITORY_FIELD_COUNT, &field_count);
    if (result.success == 0) return result;

    result = RepositoryUtils_validate_field_count(field_count, BED_REPOSITORY_FIELD_COUNT);
    if (result.success == 0) return result;

    /* 逐字段解析 */
    memset(bed, 0, sizeof(*bed));
    BedRepository_copy_text(bed->bed_id, sizeof(bed->bed_id), fields[0]);
    BedRepository_copy_text(bed->ward_id, sizeof(bed->ward_id), fields[1]);
    BedRepository_copy_text(bed->room_no, sizeof(bed->room_no), fields[2]);
    BedRepository_copy_text(bed->bed_no, sizeof(bed->bed_no), fields[3]);

    result = BedRepository_parse_int(fields[4], &status_value);
    if (result.success == 0) return result;
    bed->status = (BedStatus)status_value;

    BedRepository_copy_text(bed->current_admission_id, sizeof(bed->current_admission_id), fields[5]);
    BedRepository_copy_text(bed->occupied_at, sizeof(bed->occupied_at), fields[6]);
    BedRepository_copy_text(bed->released_at, sizeof(bed->released_at), fields[7]);

    return BedRepository_validate(bed);
}

/** 确保床位数据文件包含正确的表头 */
static Result BedRepository_ensure_header(const BedRepository *repository) {
    char line[TEXT_FILE_REPOSITORY_LINE_CAPACITY];
    FILE *file = 0;
    Result result;

    if (repository == 0) return Result_make_failure("bed repository missing");

    result = TextFileRepository_ensure_file_exists(&repository->storage);
    if (result.success == 0) return result;

    file = fopen(repository->storage.path, "r");
    if (file == 0) return Result_make_failure("failed to inspect bed repository");

    while (fgets(line, sizeof(line), file) != 0) {
        RepositoryUtils_strip_line_endings(line);
        if (RepositoryUtils_is_blank_line(line)) continue;

        fclose(file);
        if (strcmp(line, BED_REPOSITORY_HEADER) != 0) {
            return Result_make_failure("bed repository header mismatch");
        }
        return Result_make_success("bed header ready");
    }

    fclose(file);
    return TextFileRepository_append_line(&repository->storage, BED_REPOSITORY_HEADER);
}

/** 加载床位的行处理回调 */
static Result BedRepository_collect_line(const char *line, void *context) {
    BedRepositoryLoadContext *load_context = (BedRepositoryLoadContext *)context;
    Bed *bed = 0;
    Result result;

    if (load_context == 0 || load_context->beds == 0) {
        return Result_make_failure("bed load context missing");
    }

    bed = (Bed *)malloc(sizeof(*bed));
    if (bed == 0) return Result_make_failure("failed to allocate bed");

    result = BedRepository_parse_line(line, bed);
    if (result.success == 0) { free(bed); return result; }

    if (!LinkedList_append(load_context->beds, bed)) {
        free(bed);
        return Result_make_failure("failed to append bed");
    }
    return Result_make_success("bed loaded");
}

/**
 * @brief 校验床位链表的合法性（含ID唯一性检查）
 * @return 1 表示合法，0 表示不合法
 */
static int BedRepository_validate_list(const LinkedList *beds) {
    const LinkedListNode *outer = 0;
    if (beds == 0) return 0;

    outer = beds->head;
    while (outer != 0) {
        const LinkedListNode *inner = outer->next;
        const Bed *bed = (const Bed *)outer->data;
        Result result = BedRepository_validate(bed);
        if (result.success == 0) return 0;

        /* 检查ID唯一性 */
        while (inner != 0) {
            const Bed *other = (const Bed *)inner->data;
            if (strcmp(bed->bed_id, other->bed_id) == 0) return 0;
            inner = inner->next;
        }
        outer = outer->next;
    }
    return 1;
}

/** 在链表中按床位ID查找 */
static Bed *BedRepository_find_in_list(LinkedList *beds, const char *bed_id) {
    LinkedListNode *current = 0;
    if (beds == 0 || bed_id == 0) return 0;

    current = beds->head;
    while (current != 0) {
        Bed *bed = (Bed *)current->data;
        if (strcmp(bed->bed_id, bed_id) == 0) return bed;
        current = current->next;
    }
    return 0;
}

/** 初始化床位仓储 */
Result BedRepository_init(BedRepository *repository, const char *path) {
    Result result;
    if (repository == 0) return Result_make_failure("bed repository missing");

    result = TextFileRepository_init(&repository->storage, path);
    if (result.success == 0) return result;

    return BedRepository_ensure_header(repository);
}

/**
 * @brief 追加一条床位记录
 *
 * 先加载所有记录检查ID唯一性，通过后追加新记录并全量保存。
 */
Result BedRepository_append(const BedRepository *repository, const Bed *bed) {
    LinkedList beds;
    Bed *copy = 0;
    Result result = BedRepository_validate(bed);
    if (result.success == 0) return result;

    /* 加载现有记录 */
    result = BedRepository_load_all(repository, &beds);
    if (result.success == 0) return result;

    /* 检查ID唯一性 */
    if (BedRepository_find_in_list(&beds, bed->bed_id) != 0) {
        BedRepository_clear_loaded_list(&beds);
        return Result_make_failure("bed id already exists");
    }

    /* 创建副本加入链表 */
    copy = (Bed *)malloc(sizeof(*copy));
    if (copy == 0) {
        BedRepository_clear_loaded_list(&beds);
        return Result_make_failure("failed to allocate bed copy");
    }
    *copy = *bed;
    if (!LinkedList_append(&beds, copy)) {
        free(copy);
        BedRepository_clear_loaded_list(&beds);
        return Result_make_failure("failed to append bed copy");
    }

    /* 全量保存 */
    result = BedRepository_save_all(repository, &beds);
    BedRepository_clear_loaded_list(&beds);
    return result;
}

/** 按床位ID查找床位 */
Result BedRepository_find_by_id(
    const BedRepository *repository, const char *bed_id, Bed *out_bed
) {
    LinkedList beds;
    Bed *bed = 0;
    Result result;

    if (!BedRepository_has_text(bed_id) || out_bed == 0) {
        return Result_make_failure("bed query arguments missing");
    }

    result = BedRepository_load_all(repository, &beds);
    if (result.success == 0) return result;

    bed = BedRepository_find_in_list(&beds, bed_id);
    if (bed == 0) {
        BedRepository_clear_loaded_list(&beds);
        return Result_make_failure("bed not found");
    }

    *out_bed = *bed;
    BedRepository_clear_loaded_list(&beds);
    return Result_make_success("bed found");
}

/** 加载所有床位记录到链表 */
Result BedRepository_load_all(
    const BedRepository *repository, LinkedList *out_beds
) {
    BedRepositoryLoadContext context;
    Result result;

    if (out_beds == 0) return Result_make_failure("bed output list missing");

    LinkedList_init(out_beds);
    context.beds = out_beds;

    result = BedRepository_ensure_header(repository);
    if (result.success == 0) return result;

    result = TextFileRepository_for_each_data_line(
        &repository->storage, BedRepository_collect_line, &context
    );
    if (result.success == 0) {
        BedRepository_clear_loaded_list(out_beds);
        return result;
    }
    return Result_make_success("beds loaded");
}

/** 全量保存床位列表到文件（覆盖写入） */
Result BedRepository_save_all(
    const BedRepository *repository, const LinkedList *beds
) {
    const LinkedListNode *current = 0;
    FILE *file = 0;
    Result result;

    if (repository == 0 || beds == 0) {
        return Result_make_failure("bed save arguments missing");
    }
    if (!BedRepository_validate_list(beds)) {
        return Result_make_failure("bed list invalid");
    }

    result = TextFileRepository_ensure_file_exists(&repository->storage);
    if (result.success == 0) return result;

    file = fopen(repository->storage.path, "w");
    if (file == 0) return Result_make_failure("failed to rewrite bed repository");

    if (fprintf(file, "%s\n", BED_REPOSITORY_HEADER) < 0) {
        fclose(file);
        return Result_make_failure("failed to write bed header");
    }

    current = beds->head;
    while (current != 0) {
        char line[TEXT_FILE_REPOSITORY_LINE_CAPACITY];
        result = BedRepository_format_line((const Bed *)current->data, line, sizeof(line));
        if (result.success == 0) { fclose(file); return result; }

        if (fprintf(file, "%s\n", line) < 0) {
            fclose(file);
            return Result_make_failure("failed to write bed line");
        }
        current = current->next;
    }

    fclose(file);
    return Result_make_success("beds saved");
}

/** 清空并释放床位链表中的所有元素 */
void BedRepository_clear_loaded_list(LinkedList *beds) {
    LinkedList_clear(beds, free);
}
