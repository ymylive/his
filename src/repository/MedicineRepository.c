#include "repository/MedicineRepository.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "repository/RepositoryUtils.h"

#define MEDICINE_REPOSITORY_FIELD_COUNT 6

static const char *MEDICINE_REPOSITORY_HEADER =
    "medicine_id|name|price|stock|department_id|low_stock_threshold";

typedef struct MedicineRepositoryLoadContext {
    LinkedList *medicines;
} MedicineRepositoryLoadContext;

typedef struct MedicineRepositoryFindContext {
    const char *medicine_id;
    Medicine *out_medicine;
    int found;
} MedicineRepositoryFindContext;

static int MedicineRepository_has_text(const char *text) {
    return text != 0 && text[0] != '\0';
}

static void MedicineRepository_copy_string(
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

static Result MedicineRepository_validate(const Medicine *medicine) {
    if (medicine == 0) {
        return Result_make_failure("medicine missing");
    }

    if (!MedicineRepository_has_text(medicine->medicine_id)) {
        return Result_make_failure("medicine id missing");
    }

    if (!MedicineRepository_has_text(medicine->name)) {
        return Result_make_failure("medicine name missing");
    }

    if (!Medicine_has_valid_inventory(medicine)) {
        return Result_make_failure("medicine inventory invalid");
    }

    if (!RepositoryUtils_is_safe_field_text(medicine->medicine_id) ||
        !RepositoryUtils_is_safe_field_text(medicine->name) ||
        !RepositoryUtils_is_safe_field_text(medicine->department_id)) {
        return Result_make_failure("medicine field contains reserved character");
    }

    return Result_make_success("medicine valid");
}

static Result MedicineRepository_parse_double(
    const char *field,
    double *out_value
) {
    char *end_pointer = 0;
    double value = 0.0;

    if (field == 0 || out_value == 0 || field[0] == '\0') {
        return Result_make_failure("medicine number missing");
    }

    value = strtod(field, &end_pointer);
    if (end_pointer == field || end_pointer == 0 || *end_pointer != '\0') {
        return Result_make_failure("medicine number invalid");
    }

    *out_value = value;
    return Result_make_success("medicine number parsed");
}

static Result MedicineRepository_parse_int(
    const char *field,
    int *out_value
) {
    char *end_pointer = 0;
    long value = 0;

    if (field == 0 || out_value == 0 || field[0] == '\0') {
        return Result_make_failure("medicine integer missing");
    }

    value = strtol(field, &end_pointer, 10);
    if (end_pointer == field || end_pointer == 0 || *end_pointer != '\0') {
        return Result_make_failure("medicine integer invalid");
    }

    *out_value = (int)value;
    return Result_make_success("medicine integer parsed");
}

static Result MedicineRepository_format_line(
    const Medicine *medicine,
    char *line,
    size_t line_capacity
) {
    int written = 0;
    Result result = MedicineRepository_validate(medicine);

    if (result.success == 0) {
        return result;
    }

    if (line == 0 || line_capacity == 0) {
        return Result_make_failure("medicine line buffer missing");
    }

    written = snprintf(
        line,
        line_capacity,
        "%s|%s|%.2f|%d|%s|%d",
        medicine->medicine_id,
        medicine->name,
        medicine->price,
        medicine->stock,
        medicine->department_id,
        medicine->low_stock_threshold
    );
    if (written < 0 || (size_t)written >= line_capacity) {
        return Result_make_failure("medicine line too long");
    }

    return Result_make_success("medicine formatted");
}

static Result MedicineRepository_parse_line(
    const char *line,
    Medicine *medicine
) {
    char mutable_line[TEXT_FILE_REPOSITORY_LINE_CAPACITY];
    char *fields[MEDICINE_REPOSITORY_FIELD_COUNT];
    size_t field_count = 0;
    Result result;

    if (line == 0 || medicine == 0) {
        return Result_make_failure("medicine line missing");
    }

    MedicineRepository_copy_string(mutable_line, sizeof(mutable_line), line);
    result = RepositoryUtils_split_pipe_line(
        mutable_line,
        fields,
        MEDICINE_REPOSITORY_FIELD_COUNT,
        &field_count
    );
    if (result.success == 0) {
        return result;
    }

    result = RepositoryUtils_validate_field_count(
        field_count,
        MEDICINE_REPOSITORY_FIELD_COUNT
    );
    if (result.success == 0) {
        return result;
    }

    memset(medicine, 0, sizeof(*medicine));
    MedicineRepository_copy_string(
        medicine->medicine_id,
        sizeof(medicine->medicine_id),
        fields[0]
    );
    MedicineRepository_copy_string(
        medicine->name,
        sizeof(medicine->name),
        fields[1]
    );

    result = MedicineRepository_parse_double(fields[2], &medicine->price);
    if (result.success == 0) {
        return result;
    }

    result = MedicineRepository_parse_int(fields[3], &medicine->stock);
    if (result.success == 0) {
        return result;
    }

    MedicineRepository_copy_string(
        medicine->department_id,
        sizeof(medicine->department_id),
        fields[4]
    );

    result = MedicineRepository_parse_int(
        fields[5],
        &medicine->low_stock_threshold
    );
    if (result.success == 0) {
        return result;
    }

    return MedicineRepository_validate(medicine);
}

static Result MedicineRepository_ensure_header(MedicineRepository *repository) {
    char line[TEXT_FILE_REPOSITORY_LINE_CAPACITY];
    FILE *file = 0;
    Result result;

    if (repository == 0) {
        return Result_make_failure("medicine repository missing");
    }

    result = TextFileRepository_ensure_file_exists(&repository->storage);
    if (result.success == 0) {
        return result;
    }

    file = fopen(repository->storage.path, "r");
    if (file == 0) {
        return Result_make_failure("failed to inspect medicine repository");
    }

    while (fgets(line, sizeof(line), file) != 0) {
        RepositoryUtils_strip_line_endings(line);
        if (RepositoryUtils_is_blank_line(line)) {
            continue;
        }

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
    return TextFileRepository_append_line(
        &repository->storage,
        MEDICINE_REPOSITORY_HEADER
    );
}

static Result MedicineRepository_collect_line(const char *line, void *context) {
    MedicineRepositoryLoadContext *load_context =
        (MedicineRepositoryLoadContext *)context;
    Medicine *medicine = 0;
    Result result;

    if (load_context == 0 || load_context->medicines == 0) {
        return Result_make_failure("medicine load context missing");
    }

    medicine = (Medicine *)malloc(sizeof(Medicine));
    if (medicine == 0) {
        return Result_make_failure("failed to allocate medicine");
    }

    result = MedicineRepository_parse_line(line, medicine);
    if (result.success == 0) {
        free(medicine);
        return result;
    }

    if (!LinkedList_append(load_context->medicines, medicine)) {
        free(medicine);
        return Result_make_failure("failed to append medicine");
    }

    return Result_make_success("medicine loaded");
}

static Result MedicineRepository_find_line(const char *line, void *context) {
    MedicineRepositoryFindContext *find_context =
        (MedicineRepositoryFindContext *)context;
    Medicine medicine;
    Result result;

    if (find_context == 0 || find_context->medicine_id == 0 ||
        find_context->out_medicine == 0) {
        return Result_make_failure("medicine find context missing");
    }

    result = MedicineRepository_parse_line(line, &medicine);
    if (result.success == 0) {
        return result;
    }

    if (strcmp(medicine.medicine_id, find_context->medicine_id) == 0) {
        *(find_context->out_medicine) = medicine;
        find_context->found = 1;
    }

    return Result_make_success("medicine inspected");
}

static int MedicineRepository_validate_list(const LinkedList *medicines) {
    LinkedListNode *current = 0;

    if (medicines == 0) {
        return 0;
    }

    current = medicines->head;
    while (current != 0) {
        Result result = MedicineRepository_validate((const Medicine *)current->data);
        if (result.success == 0) {
            return 0;
        }

        current = current->next;
    }

    return 1;
}

static void MedicineRepository_free_item(void *data) {
    free(data);
}

Result MedicineRepository_init(MedicineRepository *repository, const char *path) {
    Result result;

    if (repository == 0) {
        return Result_make_failure("medicine repository missing");
    }

    result = TextFileRepository_init(&repository->storage, path);
    if (result.success == 0) {
        return result;
    }

    return MedicineRepository_ensure_header(repository);
}

Result MedicineRepository_load_all(
    MedicineRepository *repository,
    LinkedList *out_medicines
) {
    MedicineRepositoryLoadContext context;
    Result result;

    if (out_medicines == 0) {
        return Result_make_failure("medicine output list missing");
    }

    LinkedList_init(out_medicines);
    context.medicines = out_medicines;

    result = MedicineRepository_ensure_header(repository);
    if (result.success == 0) {
        return result;
    }

    result = TextFileRepository_for_each_data_line(
        &repository->storage,
        MedicineRepository_collect_line,
        &context
    );
    if (result.success == 0) {
        MedicineRepository_clear_list(out_medicines);
        return result;
    }

    return Result_make_success("medicines loaded");
}

Result MedicineRepository_find_by_medicine_id(
    MedicineRepository *repository,
    const char *medicine_id,
    Medicine *out_medicine
) {
    MedicineRepositoryFindContext context;
    Result result;

    if (!MedicineRepository_has_text(medicine_id) || out_medicine == 0) {
        return Result_make_failure("medicine query arguments missing");
    }

    result = MedicineRepository_ensure_header(repository);
    if (result.success == 0) {
        return result;
    }

    memset(out_medicine, 0, sizeof(*out_medicine));
    context.medicine_id = medicine_id;
    context.out_medicine = out_medicine;
    context.found = 0;

    result = TextFileRepository_for_each_data_line(
        &repository->storage,
        MedicineRepository_find_line,
        &context
    );
    if (result.success == 0) {
        return result;
    }

    if (context.found == 0) {
        return Result_make_failure("medicine not found");
    }

    return Result_make_success("medicine found");
}

Result MedicineRepository_save(
    MedicineRepository *repository,
    const Medicine *medicine
) {
    char line[TEXT_FILE_REPOSITORY_LINE_CAPACITY];
    Result result = MedicineRepository_ensure_header(repository);

    if (result.success == 0) {
        return result;
    }

    result = MedicineRepository_format_line(medicine, line, sizeof(line));
    if (result.success == 0) {
        return result;
    }

    return TextFileRepository_append_line(&repository->storage, line);
}

Result MedicineRepository_save_all(
    MedicineRepository *repository,
    const LinkedList *medicines
) {
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
    if (result.success == 0) {
        return result;
    }

    file = fopen(repository->storage.path, "w");
    if (file == 0) {
        return Result_make_failure("failed to rewrite medicine repository");
    }

    if (fprintf(file, "%s\n", MEDICINE_REPOSITORY_HEADER) < 0) {
        fclose(file);
        return Result_make_failure("failed to write medicine header");
    }

    current = medicines->head;
    while (current != 0) {
        char line[TEXT_FILE_REPOSITORY_LINE_CAPACITY];
        result = MedicineRepository_format_line(
            (const Medicine *)current->data,
            line,
            sizeof(line)
        );
        if (result.success == 0) {
            fclose(file);
            return result;
        }

        if (fprintf(file, "%s\n", line) < 0) {
            fclose(file);
            return Result_make_failure("failed to write medicine line");
        }

        current = current->next;
    }

    fclose(file);
    return Result_make_success("medicines saved");
}

void MedicineRepository_clear_list(LinkedList *medicines) {
    LinkedList_clear(medicines, MedicineRepository_free_item);
}
