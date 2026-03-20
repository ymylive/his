#include "repository/TextFileRepository.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#if defined(_WIN32)
#include <direct.h>
#define HIS_MKDIR(path) _mkdir(path)
#else
#include <sys/types.h>
#define HIS_MKDIR(path) mkdir(path, 0777)
#endif

#include "repository/RepositoryUtils.h"

static int TextFileRepository_is_separator(char character) {
    return character == '/' || character == '\\';
}

static int TextFileRepository_has_path(const TextFileRepository *repository) {
    return repository != 0 && repository->path[0] != '\0';
}

static Result TextFileRepository_make_message_result(const char *message) {
    return Result_make_failure(message);
}

static Result TextFileRepository_create_directory_if_needed(const char *path) {
    struct stat info;

    if (path == 0 || path[0] == '\0') {
        return Result_make_success("directory skipped");
    }

    if (stat(path, &info) == 0) {
        if ((info.st_mode & S_IFDIR) != 0) {
            return Result_make_success("directory exists");
        }

        return TextFileRepository_make_message_result("path exists as file");
    }

    if (HIS_MKDIR(path) == 0 || errno == EEXIST) {
        return Result_make_success("directory ready");
    }

    return TextFileRepository_make_message_result("failed to create directory");
}

static Result TextFileRepository_create_parent_directories(const char *path) {
    char buffer[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    size_t index = 0;
    size_t length = 0;

    if (path == 0 || path[0] == '\0') {
        return TextFileRepository_make_message_result("repository path missing");
    }

    length = strlen(path);
    if (length >= sizeof(buffer)) {
        return TextFileRepository_make_message_result("repository path too long");
    }

    strcpy(buffer, path);
    for (index = 0; index < length; index++) {
        if (!TextFileRepository_is_separator(buffer[index])) {
            continue;
        }

        if (index == 0 || (index == 2 && buffer[1] == ':')) {
            continue;
        }

        buffer[index] = '\0';

        if (buffer[0] != '\0') {
            Result directory_result = TextFileRepository_create_directory_if_needed(buffer);
            if (directory_result.success == 0) {
                buffer[index] = path[index];
                return directory_result;
            }
        }

        buffer[index] = path[index];
    }

    return Result_make_success("parent directories ready");
}

static void TextFileRepository_discard_rest_of_line(FILE *file) {
    int character = 0;

    while ((character = fgetc(file)) != EOF) {
        if (character == '\n') {
            return;
        }
    }
}

static Result TextFileRepository_iterate_lines(
    const TextFileRepository *repository,
    TextFileRepositoryLineHandler line_handler,
    void *context,
    int skip_header
) {
    char line[TEXT_FILE_REPOSITORY_LINE_CAPACITY];
    FILE *file = 0;
    Result result;
    int header_skipped = 0;

    if (line_handler == 0) {
        return Result_make_failure("line handler missing");
    }

    result = TextFileRepository_ensure_file_exists(repository);
    if (result.success == 0) {
        return result;
    }

    file = fopen(repository->path, "r");
    if (file == 0) {
        return Result_make_failure("failed to read repository file");
    }

    while (fgets(line, sizeof(line), file) != 0) {
        if (strchr(line, '\n') == 0 && !feof(file)) {
            TextFileRepository_discard_rest_of_line(file);
            fclose(file);
            return Result_make_failure("repository line too long");
        }

        RepositoryUtils_strip_line_endings(line);
        if (RepositoryUtils_is_blank_line(line)) {
            continue;
        }

        if (skip_header != 0 && header_skipped == 0) {
            header_skipped = 1;
            continue;
        }

        result = line_handler(line, context);
        if (result.success == 0) {
            fclose(file);
            return result;
        }
    }

    if (ferror(file) != 0) {
        fclose(file);
        return Result_make_failure("repository read failed");
    }

    fclose(file);
    return Result_make_success("repository lines loaded");
}

Result TextFileRepository_init(TextFileRepository *repository, const char *path) {
    size_t length = 0;

    if (repository == 0 || path == 0 || path[0] == '\0') {
        return Result_make_failure("repository path missing");
    }

    length = strlen(path);
    if (length >= sizeof(repository->path)) {
        return Result_make_failure("repository path too long");
    }

    strcpy(repository->path, path);
    return Result_make_success("repository ready");
}

Result TextFileRepository_ensure_file_exists(const TextFileRepository *repository) {
    FILE *file = 0;
    Result result;

    if (!TextFileRepository_has_path(repository)) {
        return Result_make_failure("repository not initialized");
    }

    result = TextFileRepository_create_parent_directories(repository->path);
    if (result.success == 0) {
        return result;
    }

    file = fopen(repository->path, "a");
    if (file == 0) {
        return Result_make_failure("failed to open repository file");
    }

    fclose(file);
    return Result_make_success("repository file ready");
}

Result TextFileRepository_for_each_non_empty_line(
    const TextFileRepository *repository,
    TextFileRepositoryLineHandler line_handler,
    void *context
) {
    return TextFileRepository_iterate_lines(repository, line_handler, context, 0);
}

Result TextFileRepository_for_each_data_line(
    const TextFileRepository *repository,
    TextFileRepositoryLineHandler line_handler,
    void *context
) {
    return TextFileRepository_iterate_lines(repository, line_handler, context, 1);
}

Result TextFileRepository_append_line(
    const TextFileRepository *repository,
    const char *line
) {
    FILE *file = 0;
    size_t length = 0;
    Result result;

    if (line == 0) {
        return Result_make_failure("line missing");
    }

    result = TextFileRepository_ensure_file_exists(repository);
    if (result.success == 0) {
        return result;
    }

    file = fopen(repository->path, "a");
    if (file == 0) {
        return Result_make_failure("failed to append repository file");
    }

    if (fputs(line, file) == EOF) {
        fclose(file);
        return Result_make_failure("failed to write repository line");
    }

    length = strlen(line);
    if (length == 0 || (line[length - 1] != '\n' && line[length - 1] != '\r')) {
        if (fputc('\n', file) == EOF) {
            fclose(file);
            return Result_make_failure("failed to finalize repository line");
        }
    }

    fclose(file);
    return Result_make_success("repository line appended");
}
