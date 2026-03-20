#ifndef HIS_REPOSITORY_REPOSITORY_UTILS_H
#define HIS_REPOSITORY_REPOSITORY_UTILS_H

#include <stddef.h>

#include "common/Result.h"

#define REPOSITORY_UTILS_MAX_FIELDS 32

void RepositoryUtils_strip_line_endings(char *line);
int RepositoryUtils_is_blank_line(const char *line);
int RepositoryUtils_is_safe_field_text(const char *text);
Result RepositoryUtils_validate_field_count(size_t actual_count, size_t expected_count);
Result RepositoryUtils_split_pipe_line(
    char *line,
    char **fields,
    size_t max_fields,
    size_t *out_field_count
);

#endif
