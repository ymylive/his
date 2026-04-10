/**
 * @file MedicineRepository.c
 * @brief 药品数据仓储层实现
 *
 * 实现药品（Medicine）数据的持久化存储操作，包括：
 * - 药品数据的序列化与反序列化（含浮点数价格和整数库存）
 * - 数据校验（必填字段、库存合法性、保留字符检测）
 * - 加载、按ID查找、追加保存、全量保存操作
 */

#include "repository/MedicineRepository.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "repository/RepositoryUtils.h"

/** 药品记录的字段数量（含别名和分类） */
#define MEDICINE_REPOSITORY_FIELD_COUNT 8

/** 旧版药品记录的字段数量（含别名，不含分类，用于向后兼容） */
#define MEDICINE_REPOSITORY_LEGACY_FIELD_COUNT_V2 7

/** 旧版药品记录的字段数量（不含别名和分类，用于向后兼容） */
#define MEDICINE_REPOSITORY_LEGACY_FIELD_COUNT_V1 6

/** 药品数据文件的表头行 */
static const char *MEDICINE_REPOSITORY_HEADER =
    "medicine_id|name|alias|category|price|stock|department_id|low_stock_threshold";

/** 加载所有药品时使用的上下文结构体 */
typedef struct MedicineRepositoryLoadContext {
    LinkedList *medicines;
} MedicineRepositoryLoadContext;

/** 按ID查找药品时使用的上下文结构体 */
typedef struct MedicineRepositoryFindContext {
    const char *medicine_id;
    Medicine *out_medicine;
    int found;
} MedicineRepositoryFindContext;

/** 判断文本是否非空 */
static int MedicineRepository_has_text(const char *text) {
    return text != 0 && text[0] != '\0';
}

/** 安全复制字符串 */
static void MedicineRepository_copy_string(
    char *destination, size_t capacity, const char *source
) {
    if (destination == 0 || capacity == 0) return;
    if (source == 0) { destination[0] = '\0'; return; }
    strncpy(destination, source, capacity - 1);
    destination[capacity - 1] = '\0';
}

/**
 * @brief 校验药品数据的合法性
 *
 * 检查药品ID和名称非空、库存数据合法、文本字段不含保留字符。
 */
static Result MedicineRepository_validate(const Medicine *medicine) {
    if (medicine == 0) return Result_make_failure("medicine missing");
    if (!MedicineRepository_has_text(medicine->medicine_id)) return Result_make_failure("medicine id missing");
    if (!MedicineRepository_has_text(medicine->name)) return Result_make_failure("medicine name missing");

    /* 通过领域层函数校验库存数据 */
    if (!Medicine_has_valid_inventory(medicine)) return Result_make_failure("medicine inventory invalid");

    if (!RepositoryUtils_is_safe_field_text(medicine->medicine_id) ||
        !RepositoryUtils_is_safe_field_text(medicine->name) ||
        !RepositoryUtils_is_safe_field_text(medicine->alias) ||
        !RepositoryUtils_is_safe_field_text(medicine->category) ||
        !RepositoryUtils_is_safe_field_text(medicine->department_id)) {
        return Result_make_failure("medicine field contains reserved character");
    }
    return Result_make_success("medicine valid");
}

/** 解析浮点数字段 */
static Result MedicineRepository_parse_double(const char *field, double *out_value) {
    char *end_pointer = 0;
    double value = 0.0;

    if (field == 0 || out_value == 0 || field[0] == '\0') return Result_make_failure("medicine number missing");
    value = strtod(field, &end_pointer);
    if (end_pointer == field || end_pointer == 0 || *end_pointer != '\0') {
        return Result_make_failure("medicine number invalid");
    }
    *out_value = value;
    return Result_make_success("medicine number parsed");
}

/** 解析整数字段 */
static Result MedicineRepository_parse_int(const char *field, int *out_value) {
    char *end_pointer = 0;
    long value = 0;

    if (field == 0 || out_value == 0 || field[0] == '\0') return Result_make_failure("medicine integer missing");
    value = strtol(field, &end_pointer, 10);
    if (end_pointer == field || end_pointer == 0 || *end_pointer != '\0') {
        return Result_make_failure("medicine integer invalid");
    }
    *out_value = (int)value;
    return Result_make_success("medicine integer parsed");
}

/** 将药品结构体格式化为文本行（价格保留两位小数） */
static Result MedicineRepository_format_line(
    const Medicine *medicine, char *line, size_t line_capacity
) {
    int written = 0;
    Result result = MedicineRepository_validate(medicine);
    if (result.success == 0) return result;
    if (line == 0 || line_capacity == 0) return Result_make_failure("medicine line buffer missing");

    written = snprintf(line, line_capacity, "%s|%s|%s|%s|%.2f|%d|%s|%d",
        medicine->medicine_id, medicine->name, medicine->alias, medicine->category,
        medicine->price, medicine->stock, medicine->department_id,
        medicine->low_stock_threshold);
    if (written < 0 || (size_t)written >= line_capacity) {
        return Result_make_failure("medicine line too long");
    }
    return Result_make_success("medicine formatted");
}

/** 将一行文本解析为药品结构体（兼容新旧两种格式） */
static Result MedicineRepository_parse_line(const char *line, Medicine *medicine) {
    char mutable_line[TEXT_FILE_REPOSITORY_LINE_CAPACITY];
    char *fields[MEDICINE_REPOSITORY_FIELD_COUNT];
    size_t field_count = 0;
    int price_idx, stock_idx, dept_idx, threshold_idx;
    Result result;

    if (line == 0 || medicine == 0) return Result_make_failure("medicine line missing");

    MedicineRepository_copy_string(mutable_line, sizeof(mutable_line), line);
    result = RepositoryUtils_split_pipe_line(mutable_line, fields, MEDICINE_REPOSITORY_FIELD_COUNT, &field_count);
    if (result.success == 0) return result;

    /* 支持新格式(8字段,含alias+category)、v2格式(7字段,含alias)和旧格式(6字段) */
    if (field_count != MEDICINE_REPOSITORY_FIELD_COUNT &&
        field_count != MEDICINE_REPOSITORY_LEGACY_FIELD_COUNT_V2 &&
        field_count != MEDICINE_REPOSITORY_LEGACY_FIELD_COUNT_V1) {
        return Result_make_failure("medicine field count mismatch");
    }

    memset(medicine, 0, sizeof(*medicine));
    MedicineRepository_copy_string(medicine->medicine_id, sizeof(medicine->medicine_id), fields[0]);
    MedicineRepository_copy_string(medicine->name, sizeof(medicine->name), fields[1]);

    if (field_count == MEDICINE_REPOSITORY_FIELD_COUNT) {
        /* 新格式: medicine_id|name|alias|category|price|stock|department_id|low_stock_threshold */
        MedicineRepository_copy_string(medicine->alias, sizeof(medicine->alias), fields[2]);
        MedicineRepository_copy_string(medicine->category, sizeof(medicine->category), fields[3]);
        price_idx = 4;
        stock_idx = 5;
        dept_idx = 6;
        threshold_idx = 7;
    } else if (field_count == MEDICINE_REPOSITORY_LEGACY_FIELD_COUNT_V2) {
        /* v2格式: medicine_id|name|alias|price|stock|department_id|low_stock_threshold */
        MedicineRepository_copy_string(medicine->alias, sizeof(medicine->alias), fields[2]);
        medicine->category[0] = '\0';
        price_idx = 3;
        stock_idx = 4;
        dept_idx = 5;
        threshold_idx = 6;
    } else {
        /* 旧格式: medicine_id|name|price|stock|department_id|low_stock_threshold */
        medicine->alias[0] = '\0';
        medicine->category[0] = '\0';
        price_idx = 2;
        stock_idx = 3;
        dept_idx = 4;
        threshold_idx = 5;
    }

    /* 解析价格（浮点数） */
    result = MedicineRepository_parse_double(fields[price_idx], &medicine->price);
    if (result.success == 0) return result;

    /* 解析库存（整数） */
    result = MedicineRepository_parse_int(fields[stock_idx], &medicine->stock);
    if (result.success == 0) return result;

    MedicineRepository_copy_string(medicine->department_id, sizeof(medicine->department_id), fields[dept_idx]);

    /* 解析低库存阈值 */
    result = MedicineRepository_parse_int(fields[threshold_idx], &medicine->low_stock_threshold);
    if (result.success == 0) return result;

    return MedicineRepository_validate(medicine);
}

/** 确保药品数据文件包含正确的表头 */
static Result MedicineRepository_ensure_header(MedicineRepository *repository) {
    char line[TEXT_FILE_REPOSITORY_LINE_CAPACITY];
    FILE *file = 0;
    Result result;

    if (repository == 0) return Result_make_failure("medicine repository missing");

    result = TextFileRepository_ensure_file_exists(&repository->storage);
    if (result.success == 0) return result;

    file = fopen(repository->storage.path, "r");
    if (file == 0) return Result_make_failure("failed to inspect medicine repository");

    while (fgets(line, sizeof(line), file) != 0) {
        RepositoryUtils_strip_line_endings(line);
        if (RepositoryUtils_is_blank_line(line)) continue;

        fclose(file);
        if (strcmp(line, MEDICINE_REPOSITORY_HEADER) != 0) {
            return Result_make_failure("medicine repository header mismatch");
        }
        return Result_make_success("medicine header ready");
    }

    if (ferror(file) != 0) {
        fclose(file);
        return Result_make_failure("failed to read medicine repository");
    }

    fclose(file);
    return TextFileRepository_append_line(&repository->storage, MEDICINE_REPOSITORY_HEADER);
}

/** 加载药品的行处理回调 */
static Result MedicineRepository_collect_line(const char *line, void *context) {
    MedicineRepositoryLoadContext *load_context = (MedicineRepositoryLoadContext *)context;
    Medicine *medicine = 0;
    Result result;

    if (load_context == 0 || load_context->medicines == 0) {
        return Result_make_failure("medicine load context missing");
    }

    medicine = (Medicine *)malloc(sizeof(Medicine));
    if (medicine == 0) return Result_make_failure("failed to allocate medicine");

    result = MedicineRepository_parse_line(line, medicine);
    if (result.success == 0) { free(medicine); return result; }

    if (!LinkedList_append(load_context->medicines, medicine)) {
        free(medicine);
        return Result_make_failure("failed to append medicine");
    }
    return Result_make_success("medicine loaded");
}

/** 按ID查找药品的行处理回调 */
static Result MedicineRepository_find_line(const char *line, void *context) {
    MedicineRepositoryFindContext *find_context = (MedicineRepositoryFindContext *)context;
    Medicine medicine;
    Result result;

    if (find_context == 0 || find_context->medicine_id == 0 || find_context->out_medicine == 0) {
        return Result_make_failure("medicine find context missing");
    }

    result = MedicineRepository_parse_line(line, &medicine);
    if (result.success == 0) return result;

    if (strcmp(medicine.medicine_id, find_context->medicine_id) == 0) {
        *(find_context->out_medicine) = medicine;
        find_context->found = 1;
    }
    return Result_make_success("medicine inspected");
}

/** 校验药品链表中每个元素的合法性 */
static int MedicineRepository_validate_list(const LinkedList *medicines) {
    LinkedListNode *current = 0;
    if (medicines == 0) return 0;

    current = medicines->head;
    while (current != 0) {
        Result result = MedicineRepository_validate((const Medicine *)current->data);
        if (result.success == 0) return 0;
        current = current->next;
    }
    return 1;
}

/** 释放链表节点中的数据 */
static void MedicineRepository_free_item(void *data) {
    free(data);
}

/** 初始化药品仓储 */
Result MedicineRepository_init(MedicineRepository *repository, const char *path) {
    Result result;
    if (repository == 0) return Result_make_failure("medicine repository missing");

    result = TextFileRepository_init(&repository->storage, path);
    if (result.success == 0) return result;

    return MedicineRepository_ensure_header(repository);
}

/** 加载所有药品记录到链表 */
Result MedicineRepository_load_all(MedicineRepository *repository, LinkedList *out_medicines) {
    MedicineRepositoryLoadContext context;
    Result result;

    if (out_medicines == 0) return Result_make_failure("medicine output list missing");

    LinkedList_init(out_medicines);
    context.medicines = out_medicines;

    result = MedicineRepository_ensure_header(repository);
    if (result.success == 0) return result;

    result = TextFileRepository_for_each_data_line(
        &repository->storage, MedicineRepository_collect_line, &context
    );
    if (result.success == 0) {
        MedicineRepository_clear_list(out_medicines);
        return result;
    }
    return Result_make_success("medicines loaded");
}

/** 按药品ID查找药品 */
Result MedicineRepository_find_by_medicine_id(
    MedicineRepository *repository, const char *medicine_id, Medicine *out_medicine
) {
    MedicineRepositoryFindContext context;
    Result result;

    if (!MedicineRepository_has_text(medicine_id) || out_medicine == 0) {
        return Result_make_failure("medicine query arguments missing");
    }

    result = MedicineRepository_ensure_header(repository);
    if (result.success == 0) return result;

    memset(out_medicine, 0, sizeof(*out_medicine));
    context.medicine_id = medicine_id;
    context.out_medicine = out_medicine;
    context.found = 0;

    result = TextFileRepository_for_each_data_line(
        &repository->storage, MedicineRepository_find_line, &context
    );
    if (result.success == 0) return result;
    if (context.found == 0) return Result_make_failure("medicine not found");

    return Result_make_success("medicine found");
}

/** 追加保存一条药品记录到文件末尾 */
Result MedicineRepository_save(MedicineRepository *repository, const Medicine *medicine) {
    char line[TEXT_FILE_REPOSITORY_LINE_CAPACITY];
    Result result = MedicineRepository_ensure_header(repository);
    if (result.success == 0) return result;

    result = MedicineRepository_format_line(medicine, line, sizeof(line));
    if (result.success == 0) return result;

    return TextFileRepository_append_line(&repository->storage, line);
}

/** 全量保存药品列表到文件（覆盖写入） */
Result MedicineRepository_save_all(MedicineRepository *repository, const LinkedList *medicines) {
    LinkedListNode *current = 0;
    FILE *file = 0;
    Result result;

    if (repository == 0 || medicines == 0) {
        return Result_make_failure("medicine save all arguments missing");
    }
    if (!MedicineRepository_validate_list(medicines)) {
        return Result_make_failure("medicine list contains invalid item");
    }

    result = TextFileRepository_ensure_file_exists(&repository->storage);
    if (result.success == 0) return result;

    file = fopen(repository->storage.path, "w");
    if (file == 0) return Result_make_failure("failed to rewrite medicine repository");

    if (fprintf(file, "%s\n", MEDICINE_REPOSITORY_HEADER) < 0) {
        fclose(file);
        return Result_make_failure("failed to write medicine header");
    }

    current = medicines->head;
    while (current != 0) {
        char line[TEXT_FILE_REPOSITORY_LINE_CAPACITY];
        result = MedicineRepository_format_line(
            (const Medicine *)current->data, line, sizeof(line)
        );
        if (result.success == 0) { fclose(file); return result; }

        if (fprintf(file, "%s\n", line) < 0) {
            fclose(file);
            return Result_make_failure("failed to write medicine line");
        }
        current = current->next;
    }

    fclose(file);
    return Result_make_success("medicines saved");
}

/** 清空并释放药品链表中的所有元素 */
void MedicineRepository_clear_list(LinkedList *medicines) {
    LinkedList_clear(medicines, MedicineRepository_free_item);
}
