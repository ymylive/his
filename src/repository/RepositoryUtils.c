#include "repository/RepositoryUtils.h"

#include <ctype.h>
#include <stdio.h>

void RepositoryUtils_strip_line_endings(char *line) {
    size_t length = 0;

    if (line == 0) {
        return;
    }

    while (line[length] != '\0') {
        length++;
    }

    while (length > 0 && (line[length - 1] == '\n' || line[length - 1] == '\r')) {
        line[length - 1] = '\0';
        length--;
    }
}

int RepositoryUtils_is_blank_line(const char *line) {
    if (line == 0) {
        return 1;
    }

    while (*line != '\0') {
        if (!isspace((unsigned char)*line)) {
            return 0;
        }

        line++;
    }

    return 1;
}

int RepositoryUtils_is_safe_field_text(const char *text) {
    if (text == 0) {
        return 0;
    }

    while (*text != '\0') {
        if (*text == '|' || *text == '\r' || *text == '\n') {
            return 0;
        }

        text++;
    }

    return 1;
}

Result RepositoryUtils_validate_field_count(size_t actual_count, size_t expected_count) {
    char message[RESULT_MESSAGE_CAPACITY];

    if (actual_count == expected_count) {
        return Result_make_success("field count ok");
    }

    snprintf(
        message,
        sizeof(message),
        "expected %u fields, got %u",
        (unsigned int)expected_count,
        (unsigned int)actual_count
    );
    return Result_make_failure(message);
}

Result RepositoryUtils_split_pipe_line(
    char *line,
    char **fields,
    size_t max_fields,
    size_t *out_field_count
) {
    size_t field_count = 1;
    char *cursor = 0;

    if (out_field_count != 0) {
        *out_field_count = 0;
    }

    if (line == 0 || fields == 0 || out_field_count == 0 || max_fields == 0) {
        return Result_make_failure("invalid split arguments");
    }

    RepositoryUtils_strip_line_endings(line);
    fields[0] = line;
    cursor = line;

    while (*cursor != '\0') {
        if (*cursor == '|') {
            if (field_count >= max_fields) {
                return Result_make_failure("too many fields");
            }

            *cursor = '\0';
            fields[field_count] = cursor + 1;
            field_count++;
        }

        cursor++;
    }

    *out_field_count = field_count;
    return Result_make_success("split ok");
}
