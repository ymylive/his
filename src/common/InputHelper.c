#include "common/InputHelper.h"

#include <ctype.h>

static const char *InputHelper_skip_spaces(const char *text) {
    while (text != 0 && *text != '\0' && isspace((unsigned char)*text) != 0) {
        text++;
    }

    return text;
}

static const char *InputHelper_trim_end(const char *start) {
    const char *end = start;

    while (*end != '\0') {
        end++;
    }

    while (end > start && isspace((unsigned char)*(end - 1)) != 0) {
        end--;
    }

    return end;
}

int InputHelper_is_non_empty(const char *text) {
    const char *start = 0;
    const char *end = 0;

    if (text == 0) {
        return 0;
    }

    start = InputHelper_skip_spaces(text);
    end = InputHelper_trim_end(start);
    return end > start ? 1 : 0;
}

int InputHelper_is_positive_integer(const char *text) {
    const char *current = 0;
    int has_digit = 0;
    int non_zero_found = 0;

    if (InputHelper_is_non_empty(text) == 0) {
        return 0;
    }

    current = InputHelper_skip_spaces(text);
    while (*current != '\0' && isspace((unsigned char)*current) == 0) {
        if (isdigit((unsigned char)*current) == 0) {
            return 0;
        }

        if (*current != '0') {
            non_zero_found = 1;
        }

        has_digit = 1;
        current++;
    }

    while (*current != '\0') {
        if (isspace((unsigned char)*current) == 0) {
            return 0;
        }

        current++;
    }

    return has_digit != 0 && non_zero_found != 0 ? 1 : 0;
}

int InputHelper_is_non_negative_amount(const char *text) {
    const char *current = 0;
    int has_digit = 0;
    int dot_count = 0;

    if (InputHelper_is_non_empty(text) == 0) {
        return 0;
    }

    current = InputHelper_skip_spaces(text);
    while (*current != '\0' && isspace((unsigned char)*current) == 0) {
        if (*current == '-') {
            return 0;
        }

        if (*current == '.') {
            dot_count++;
            if (dot_count > 1) {
                return 0;
            }
        } else if (isdigit((unsigned char)*current) == 0) {
            return 0;
        } else {
            has_digit = 1;
        }

        current++;
    }

    while (*current != '\0') {
        if (isspace((unsigned char)*current) == 0) {
            return 0;
        }

        current++;
    }

    return has_digit;
}
