#ifndef HIS_COMMON_STRING_UTILS_H
#define HIS_COMMON_STRING_UTILS_H

#include <stddef.h>
#include <time.h>

#include "common/Result.h"

/* Check whether text is NULL or all whitespace */
int StringUtils_is_blank(const char *text);

/* Check whether text is non-NULL and contains non-whitespace */
int StringUtils_has_text(const char *text);

/* Safe copy into a fixed-size buffer (NULL src clears dest) */
void StringUtils_copy(char *dest, size_t capacity, const char *src);

/* Copy a time_t value (NULL src zeroes dest) */
void StringUtils_copy_time(time_t *dest, const time_t *src);

/* Validate a required text field */
Result StringUtils_validate_required(
    const char *text,
    const char *field_name,
    size_t max_capacity
);

#endif
