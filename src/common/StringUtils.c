#include "common/StringUtils.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "repository/RepositoryUtils.h"

int StringUtils_is_blank(const char *text) {
    if (text == 0) {
        return 1;
    }

    while (*text != '\0') {
        if (!isspace((unsigned char)*text)) {
            return 0;
        }

        text++;
    }

    return 1;
}

int StringUtils_has_text(const char *text) {
    return !StringUtils_is_blank(text);
}

void StringUtils_copy(char *dest, size_t capacity, const char *src) {
    if (dest == 0 || capacity == 0) {
        return;
    }

    if (src == 0) {
        dest[0] = '\0';
        return;
    }

    strncpy(dest, src, capacity - 1);
    dest[capacity - 1] = '\0';
}

void StringUtils_copy_time(time_t *dest, const time_t *src) {
    if (dest == 0) {
        return;
    }

    if (src == 0) {
        *dest = 0;
        return;
    }

    *dest = *src;
}

Result StringUtils_validate_required(
    const char *text,
    const char *field_name,
    size_t max_capacity
) {
    char message[RESULT_MESSAGE_CAPACITY];

    if (StringUtils_is_blank(text)) {
        snprintf(message, sizeof(message), "%s missing", field_name);
        return Result_make_failure(message);
    }

    if (max_capacity > 0 && strlen(text) >= max_capacity) {
        snprintf(message, sizeof(message), "%s too long", field_name);
        return Result_make_failure(message);
    }

    if (!RepositoryUtils_is_safe_field_text(text)) {
        snprintf(message, sizeof(message), "%s contains reserved characters", field_name);
        return Result_make_failure(message);
    }

    return Result_make_success("text valid");
}
