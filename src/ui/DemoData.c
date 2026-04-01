#include "ui/DemoData.h"

#include <stdio.h>
#include <string.h>

typedef struct DemoDataFilePath {
    const char *runtime_path;
} DemoDataFilePath;

static int DemoData_copy_text(char *destination, size_t capacity, const char *source) {
    if (destination == 0 || capacity == 0) {
        return 0;
    }

    if (source == 0) {
        destination[0] = '\0';
        return 1;
    }

    strncpy(destination, source, capacity - 1);
    destination[capacity - 1] = '\0';
    return 1;
}

static const char *DemoData_find_last_separator(const char *path) {
    const char *last_slash = 0;
    const char *last_backslash = 0;

    if (path == 0) {
        return 0;
    }

    last_slash = strrchr(path, '/');
    last_backslash = strrchr(path, '\\');
    if (last_slash == 0) {
        return last_backslash;
    }
    if (last_backslash == 0) {
        return last_slash;
    }
    return last_slash > last_backslash ? last_slash : last_backslash;
}

Result DemoData_resolve_seed_path(
    const char *runtime_path,
    char *seed_path,
    size_t seed_path_capacity
) {
    const char *separator = 0;
    size_t prefix_length = 0;
    const char *file_name = 0;
    int written = 0;

    if (runtime_path == 0 || seed_path == 0 || seed_path_capacity == 0) {
        return Result_make_failure("demo data path arguments missing");
    }

    separator = DemoData_find_last_separator(runtime_path);
    if (separator == 0) {
        return Result_make_failure("demo data runtime path invalid");
    }

    prefix_length = (size_t)(separator - runtime_path);
    file_name = separator + 1;
    written = snprintf(
        seed_path,
        seed_path_capacity,
        "%.*s/demo_seed/%s",
        (int)prefix_length,
        runtime_path,
        file_name
    );
    if (written < 0 || (size_t)written >= seed_path_capacity) {
        seed_path[0] = '\0';
        return Result_make_failure("demo data seed path too long");
    }

    return Result_make_success("demo data seed path ready");
}

static Result DemoData_copy_file(const char *source_path, const char *destination_path) {
    FILE *source = 0;
    FILE *destination = 0;
    char buffer[4096];
    size_t read_bytes = 0;

    if (source_path == 0 || destination_path == 0) {
        return Result_make_failure("demo data copy arguments missing");
    }

    source = fopen(source_path, "rb");
    if (source == 0) {
        return Result_make_failure("demo seed file missing");
    }

    destination = fopen(destination_path, "wb");
    if (destination == 0) {
        fclose(source);
        return Result_make_failure("demo runtime file not writable");
    }

    while ((read_bytes = fread(buffer, 1, sizeof(buffer), source)) > 0) {
        if (fwrite(buffer, 1, read_bytes, destination) != read_bytes) {
            fclose(source);
            fclose(destination);
            return Result_make_failure("demo runtime file write failed");
        }
    }

    fclose(source);
    fclose(destination);
    return Result_make_success("demo data file copied");
}

Result DemoData_reset(const MenuApplicationPaths *paths, char *buffer, size_t capacity) {
    DemoDataFilePath files[] = {
        { paths != 0 ? paths->user_path : 0 },
        { paths != 0 ? paths->patient_path : 0 },
        { paths != 0 ? paths->department_path : 0 },
        { paths != 0 ? paths->doctor_path : 0 },
        { paths != 0 ? paths->registration_path : 0 },
        { paths != 0 ? paths->visit_path : 0 },
        { paths != 0 ? paths->examination_path : 0 },
        { paths != 0 ? paths->ward_path : 0 },
        { paths != 0 ? paths->bed_path : 0 },
        { paths != 0 ? paths->admission_path : 0 },
        { paths != 0 ? paths->medicine_path : 0 },
        { paths != 0 ? paths->dispense_record_path : 0 }
    };
    char seed_path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    size_t index = 0;

    if (buffer != 0 && capacity > 0) {
        buffer[0] = '\0';
    }

    if (paths == 0) {
        return Result_make_failure("demo data paths missing");
    }

    for (index = 0; index < sizeof(files) / sizeof(files[0]); index++) {
        Result result = DemoData_resolve_seed_path(
            files[index].runtime_path,
            seed_path,
            sizeof(seed_path)
        );
        if (result.success == 0) {
            DemoData_copy_text(buffer, capacity, result.message);
            return result;
        }

        result = DemoData_copy_file(seed_path, files[index].runtime_path);
        if (result.success == 0) {
            if (buffer != 0 && capacity > 0) {
                snprintf(buffer, capacity, "%s: %s", result.message, seed_path);
            }
            return result;
        }
    }

    if (buffer != 0 && capacity > 0) {
        DemoData_copy_text(buffer, capacity, "演示数据已重置");
    }

    return Result_make_success("demo data reset complete");
}
