/**
 * @file DepartmentRepository.c
 * @brief 科室数据仓储层实现
 *
 * 实现科室（Department）数据的持久化存储操作，包括：
 * - 科室数据的序列化与反序列化
 * - 数据校验（必填字段、保留字符检测）
 * - 加载、按ID查找、追加保存、全量保存操作
 */

#include "repository/DepartmentRepository.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "repository/RepositoryUtils.h"

/** 科室记录的字段数量 */
#define DEPARTMENT_REPOSITORY_FIELD_COUNT 4

/** 科室数据文件的表头行 */
static const char *DEPARTMENT_REPOSITORY_HEADER =
    "department_id|name|location|description";

/**
 * @brief 加载所有科室时使用的上下文结构体
 */
typedef struct DepartmentRepositoryLoadContext {
    LinkedList *departments; /**< 用于存放加载结果的链表 */
} DepartmentRepositoryLoadContext;

/**
 * @brief 按ID查找科室时使用的上下文结构体
 */
typedef struct DepartmentRepositoryFindContext {
    const char *department_id;  /**< 要查找的科室ID */
    Department *out_department; /**< 输出：找到的科室数据 */
    int found;                  /**< 是否已找到标志 */
} DepartmentRepositoryFindContext;

/**
 * @brief 判断文本是否非空
 */
static int DepartmentRepository_has_text(const char *text) {
    return text != 0 && text[0] != '\0';
}

/**
 * @brief 安全复制字符串到目标缓冲区
 */
static void DepartmentRepository_copy_string(
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
    destination[capacity - 1] = '\0';
}

/**
 * @brief 校验科室数据的合法性
 * @param department 待校验的科室数据
 * @return 合法返回 success；不合法返回 failure
 */
static Result DepartmentRepository_validate(const Department *department) {
    if (department == 0) {
        return Result_make_failure("department missing");
    }

    if (!DepartmentRepository_has_text(department->department_id)) {
        return Result_make_failure("department id missing");
    }

    if (!DepartmentRepository_has_text(department->name)) {
        return Result_make_failure("department name missing");
    }

    /* 检查所有字段是否包含保留字符 */
    if (!RepositoryUtils_is_safe_field_text(department->department_id) ||
        !RepositoryUtils_is_safe_field_text(department->name) ||
        !RepositoryUtils_is_safe_field_text(department->location) ||
        !RepositoryUtils_is_safe_field_text(department->description)) {
        return Result_make_failure("department field contains reserved character");
    }

    return Result_make_success("department valid");
}

/**
 * @brief 将科室结构体格式化为管道符分隔的文本行
 * @param department    科室数据
 * @param line          输出缓冲区
 * @param line_capacity 缓冲区容量
 * @return 成功返回 success；数据无效或缓冲区不足时返回 failure
 */
static Result DepartmentRepository_format_line(
    const Department *department,
    char *line,
    size_t line_capacity
) {
    int written = 0;
    Result result = DepartmentRepository_validate(department);

    if (result.success == 0) {
        return result;
    }

    if (line == 0 || line_capacity == 0) {
        return Result_make_failure("department line buffer missing");
    }

    written = snprintf(
        line,
        line_capacity,
        "%s|%s|%s|%s",
        department->department_id,
        department->name,
        department->location,
        department->description
    );
    if (written < 0 || (size_t)written >= line_capacity) {
        return Result_make_failure("department line too long");
    }

    return Result_make_success("department formatted");
}

/**
 * @brief 将一行文本解析为科室结构体
 * @param line       管道符分隔的文本行
 * @param department 输出参数，解析得到的科室数据
 * @return 解析成功返回 success；格式不合法时返回 failure
 */
static Result DepartmentRepository_parse_line(
    const char *line,
    Department *department
) {
    char mutable_line[TEXT_FILE_REPOSITORY_LINE_CAPACITY];
    char *fields[DEPARTMENT_REPOSITORY_FIELD_COUNT];
    size_t field_count = 0;
    Result result;

    if (line == 0 || department == 0) {
        return Result_make_failure("department line missing");
    }

    DepartmentRepository_copy_string(mutable_line, sizeof(mutable_line), line);
    result = RepositoryUtils_split_pipe_line(
        mutable_line,
        fields,
        DEPARTMENT_REPOSITORY_FIELD_COUNT,
        &field_count
    );
    if (result.success == 0) {
        return result;
    }

    result = RepositoryUtils_validate_field_count(
        field_count,
        DEPARTMENT_REPOSITORY_FIELD_COUNT
    );
    if (result.success == 0) {
        return result;
    }

    /* 逐字段复制 */
    memset(department, 0, sizeof(*department));
    DepartmentRepository_copy_string(
        department->department_id, sizeof(department->department_id), fields[0]
    );
    DepartmentRepository_copy_string(
        department->name, sizeof(department->name), fields[1]
    );
    DepartmentRepository_copy_string(
        department->location, sizeof(department->location), fields[2]
    );
    DepartmentRepository_copy_string(
        department->description, sizeof(department->description), fields[3]
    );

    return DepartmentRepository_validate(department);
}

/**
 * @brief 确保科室数据文件包含正确的表头
 */
static Result DepartmentRepository_ensure_header(DepartmentRepository *repository) {
    char line[TEXT_FILE_REPOSITORY_LINE_CAPACITY];
    FILE *file = 0;
    Result result;

    if (repository == 0) {
        return Result_make_failure("department repository missing");
    }

    result = TextFileRepository_ensure_file_exists(&repository->storage);
    if (result.success == 0) {
        return result;
    }

    file = fopen(repository->storage.path, "r");
    if (file == 0) {
        return Result_make_failure("failed to inspect department repository");
    }

    /* 读取第一个非空行作为表头 */
    while (fgets(line, sizeof(line), file) != 0) {
        RepositoryUtils_strip_line_endings(line);
        if (RepositoryUtils_is_blank_line(line)) {
            continue;
        }

        fclose(file);
        if (strcmp(line, DEPARTMENT_REPOSITORY_HEADER) != 0) {
            return Result_make_failure("department repository header mismatch");
        }

        return Result_make_success("department header ready");
    }

    if (ferror(file) != 0) {
        fclose(file);
        return Result_make_failure("failed to read department repository");
    }

    /* 空文件：写入表头 */
    fclose(file);
    return TextFileRepository_append_line(
        &repository->storage,
        DEPARTMENT_REPOSITORY_HEADER
    );
}

/**
 * @brief 加载科室的行处理回调
 */
static Result DepartmentRepository_collect_line(
    const char *line,
    void *context
) {
    DepartmentRepositoryLoadContext *load_context =
        (DepartmentRepositoryLoadContext *)context;
    Department *department = 0;
    Result result;

    if (load_context == 0 || load_context->departments == 0) {
        return Result_make_failure("department load context missing");
    }

    department = (Department *)malloc(sizeof(Department));
    if (department == 0) {
        return Result_make_failure("failed to allocate department");
    }

    result = DepartmentRepository_parse_line(line, department);
    if (result.success == 0) {
        free(department);
        return result;
    }

    if (!LinkedList_append(load_context->departments, department)) {
        free(department);
        return Result_make_failure("failed to append department");
    }

    return Result_make_success("department loaded");
}

/**
 * @brief 按ID查找科室的行处理回调
 */
static Result DepartmentRepository_find_line(
    const char *line,
    void *context
) {
    DepartmentRepositoryFindContext *find_context =
        (DepartmentRepositoryFindContext *)context;
    Department department;
    Result result;

    if (find_context == 0 || find_context->department_id == 0 ||
        find_context->out_department == 0) {
        return Result_make_failure("department find context missing");
    }

    result = DepartmentRepository_parse_line(line, &department);
    if (result.success == 0) {
        return result;
    }

    /* ID不匹配，继续遍历 */
    if (strcmp(department.department_id, find_context->department_id) != 0) {
        return Result_make_success("department inspected");
    }

    /* 找到匹配的科室，复制数据并标记 */
    *(find_context->out_department) = department;
    find_context->found = 1;
    return Result_make_failure("department found"); /* 返回 failure 以终止遍历 */
}

/**
 * @brief 校验科室链表中每个元素的合法性
 * @param departments 待校验的科室链表
 * @return 1 表示全部合法，0 表示存在不合法项
 */
static int DepartmentRepository_validate_list(const LinkedList *departments) {
    LinkedListNode *current = 0;

    if (departments == 0) {
        return 0;
    }

    current = departments->head;
    while (current != 0) {
        Result result = DepartmentRepository_validate(
            (const Department *)current->data
        );
        if (result.success == 0) {
            return 0;
        }

        current = current->next;
    }

    return 1;
}

/**
 * @brief 释放链表节点中的数据
 */
static void DepartmentRepository_free_item(void *data) {
    free(data);
}

/**
 * @brief 初始化科室仓储
 */
Result DepartmentRepository_init(DepartmentRepository *repository, const char *path) {
    Result result;

    if (repository == 0) {
        return Result_make_failure("department repository missing");
    }

    result = TextFileRepository_init(&repository->storage, path);
    if (result.success == 0) {
        return result;
    }

    return DepartmentRepository_ensure_header(repository);
}

/**
 * @brief 加载所有科室记录到链表
 */
Result DepartmentRepository_load_all(
    DepartmentRepository *repository,
    LinkedList *out_departments
) {
    DepartmentRepositoryLoadContext context;
    Result result;

    if (out_departments == 0) {
        return Result_make_failure("department output list missing");
    }

    LinkedList_init(out_departments);
    context.departments = out_departments;

    result = DepartmentRepository_ensure_header(repository);
    if (result.success == 0) {
        return result;
    }

    result = TextFileRepository_for_each_data_line(
        &repository->storage,
        DepartmentRepository_collect_line,
        &context
    );
    if (result.success == 0) {
        DepartmentRepository_clear_list(out_departments);
        return result;
    }

    return Result_make_success("departments loaded");
}

/**
 * @brief 按科室ID查找科室
 */
Result DepartmentRepository_find_by_department_id(
    DepartmentRepository *repository,
    const char *department_id,
    Department *out_department
) {
    DepartmentRepositoryFindContext context;
    Result result;

    if (!DepartmentRepository_has_text(department_id) || out_department == 0) {
        return Result_make_failure("department query arguments missing");
    }

    result = DepartmentRepository_ensure_header(repository);
    if (result.success == 0) {
        return result;
    }

    memset(out_department, 0, sizeof(*out_department));
    context.department_id = department_id;
    context.out_department = out_department;
    context.found = 0;

    result = TextFileRepository_for_each_data_line(
        &repository->storage,
        DepartmentRepository_find_line,
        &context
    );

    /* 检查是否通过回调找到了科室 */
    if (context.found != 0) {
        return Result_make_success("department found");
    }

    /* 区分"未找到"和"遍历出错"两种情况 */
    if (result.success == 0 && strcmp(result.message, "department found") != 0) {
        return result; /* 真正的错误 */
    }

    if (context.found == 0) {
        return Result_make_failure("department not found");
    }

    return Result_make_success("department found");
}

/**
 * @brief 追加保存一条科室记录到文件末尾
 */
Result DepartmentRepository_save(
    DepartmentRepository *repository,
    const Department *department
) {
    char line[TEXT_FILE_REPOSITORY_LINE_CAPACITY];
    Result result = DepartmentRepository_ensure_header(repository);

    if (result.success == 0) {
        return result;
    }

    result = DepartmentRepository_format_line(department, line, sizeof(line));
    if (result.success == 0) {
        return result;
    }

    return TextFileRepository_append_line(&repository->storage, line);
}

/**
 * @brief 全量保存科室列表到文件（覆盖写入）
 */
Result DepartmentRepository_save_all(
    DepartmentRepository *repository,
    const LinkedList *departments
) {
    LinkedListNode *current = 0;
    Result result;

    if (repository == 0 || departments == 0) {
        return Result_make_failure("department save all arguments missing");
    }

    if (!DepartmentRepository_validate_list(departments)) {
        return Result_make_failure("department list contains invalid item");
    }

    {
        char line[TEXT_FILE_REPOSITORY_LINE_CAPACITY];
        char *content = 0;
        size_t capacity = (departments->count + 2) * TEXT_FILE_REPOSITORY_LINE_CAPACITY;
        size_t used = 0;
        size_t len = 0;

        content = (char *)malloc(capacity);
        if (content == 0) {
            return Result_make_failure("failed to allocate department content buffer");
        }

        len = strlen(DEPARTMENT_REPOSITORY_HEADER);
        memcpy(content + used, DEPARTMENT_REPOSITORY_HEADER, len);
        used += len;
        content[used++] = '\n';

        current = departments->head;
        while (current != 0) {
            result = DepartmentRepository_format_line(
                (const Department *)current->data,
                line,
                sizeof(line)
            );
            if (result.success == 0) {
                free(content);
                return result;
            }

            len = strlen(line);
            if (used + len + 2 > capacity) {
                char *tmp;
                capacity *= 2;
                tmp = (char *)realloc(content, capacity);
                if (tmp == 0) {
                    free(content);
                    content = 0;
                    return Result_make_failure("failed to grow department content buffer");
                }
                content = tmp;
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

/**
 * @brief 清空并释放科室链表中的所有元素
 */
void DepartmentRepository_clear_list(LinkedList *departments) {
    LinkedList_clear(departments, DepartmentRepository_free_item);
}
