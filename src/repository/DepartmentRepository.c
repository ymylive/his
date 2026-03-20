#include "repository/DepartmentRepository.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "repository/RepositoryUtils.h"

#define DEPARTMENT_REPOSITORY_FIELD_COUNT 4

static const char *DEPARTMENT_REPOSITORY_HEADER =
    "department_id|name|location|description";

typedef struct DepartmentRepositoryLoadContext {
    LinkedList *departments;
} DepartmentRepositoryLoadContext;

typedef struct DepartmentRepositoryFindContext {
    const char *department_id;
    Department *out_department;
    int found;
} DepartmentRepositoryFindContext;

static int DepartmentRepository_has_text(const char *text) {
    return text != 0 && text[0] != '\0';
}

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

    if (!RepositoryUtils_is_safe_field_text(department->department_id) ||
        !RepositoryUtils_is_safe_field_text(department->name) ||
        !RepositoryUtils_is_safe_field_text(department->location) ||
        !RepositoryUtils_is_safe_field_text(department->description)) {
        return Result_make_failure("department field contains reserved character");
    }

    return Result_make_success("department valid");
}

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

    memset(department, 0, sizeof(*department));
    DepartmentRepository_copy_string(
        department->department_id,
        sizeof(department->department_id),
        fields[0]
    );
    DepartmentRepository_copy_string(
        department->name,
        sizeof(department->name),
        fields[1]
    );
    DepartmentRepository_copy_string(
        department->location,
        sizeof(department->location),
        fields[2]
    );
    DepartmentRepository_copy_string(
        department->description,
        sizeof(department->description),
        fields[3]
    );

    return DepartmentRepository_validate(department);
}

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

    fclose(file);
    return TextFileRepository_append_line(
        &repository->storage,
        DEPARTMENT_REPOSITORY_HEADER
    );
}

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

    if (strcmp(department.department_id, find_context->department_id) == 0) {
        *(find_context->out_department) = department;
        find_context->found = 1;
    }

    return Result_make_success("department inspected");
}

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

static void DepartmentRepository_free_item(void *data) {
    free(data);
}

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
    if (result.success == 0) {
        return result;
    }

    if (context.found == 0) {
        return Result_make_failure("department not found");
    }

    return Result_make_success("department found");
}

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

Result DepartmentRepository_save_all(
    DepartmentRepository *repository,
    const LinkedList *departments
) {
    LinkedListNode *current = 0;
    FILE *file = 0;
    Result result;

    if (repository == 0 || departments == 0) {
        return Result_make_failure("department save all arguments missing");
    }

    if (!DepartmentRepository_validate_list(departments)) {
        return Result_make_failure("department list contains invalid item");
    }

    result = TextFileRepository_ensure_file_exists(&repository->storage);
    if (result.success == 0) {
        return result;
    }

    file = fopen(repository->storage.path, "w");
    if (file == 0) {
        return Result_make_failure("failed to rewrite department repository");
    }

    if (fprintf(file, "%s\n", DEPARTMENT_REPOSITORY_HEADER) < 0) {
        fclose(file);
        return Result_make_failure("failed to write department header");
    }

    current = departments->head;
    while (current != 0) {
        char line[TEXT_FILE_REPOSITORY_LINE_CAPACITY];
        result = DepartmentRepository_format_line(
            (const Department *)current->data,
            line,
            sizeof(line)
        );
        if (result.success == 0) {
            fclose(file);
            return result;
        }

        if (fprintf(file, "%s\n", line) < 0) {
            fclose(file);
            return Result_make_failure("failed to write department line");
        }

        current = current->next;
    }

    fclose(file);
    return Result_make_success("departments saved");
}

void DepartmentRepository_clear_list(LinkedList *departments) {
    LinkedList_clear(departments, DepartmentRepository_free_item);
}
