#ifndef HIS_REPOSITORY_TEXT_FILE_REPOSITORY_H
#define HIS_REPOSITORY_TEXT_FILE_REPOSITORY_H

#include "common/Result.h"

#define TEXT_FILE_REPOSITORY_PATH_CAPACITY 512
#define TEXT_FILE_REPOSITORY_LINE_CAPACITY 1024

typedef struct TextFileRepository {
    char path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
} TextFileRepository;

typedef Result (*TextFileRepositoryLineHandler)(const char *line, void *context);

Result TextFileRepository_init(TextFileRepository *repository, const char *path);
Result TextFileRepository_ensure_file_exists(const TextFileRepository *repository);
Result TextFileRepository_for_each_non_empty_line(
    const TextFileRepository *repository,
    TextFileRepositoryLineHandler line_handler,
    void *context
);
Result TextFileRepository_for_each_data_line(
    const TextFileRepository *repository,
    TextFileRepositoryLineHandler line_handler,
    void *context
);
Result TextFileRepository_append_line(
    const TextFileRepository *repository,
    const char *line
);

#endif
