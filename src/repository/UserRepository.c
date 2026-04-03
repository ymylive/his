#include "repository/UserRepository.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "repository/RepositoryUtils.h"

typedef struct UserRepositoryFindContext {
    const char *user_id;
    User *out_user;
    int found;
} UserRepositoryFindContext;

static int UserRepository_is_valid_role(UserRole role) {
    return role == USER_ROLE_PATIENT ||
           role == USER_ROLE_DOCTOR ||
           role == USER_ROLE_ADMIN ||
           role == USER_ROLE_INPATIENT_MANAGER ||
           role == USER_ROLE_PHARMACY;
}

static Result UserRepository_validate_user(const User *user) {
    if (user == 0) {
        return Result_make_failure("user missing");
    }

    if (user->user_id[0] == '\0') {
        return Result_make_failure("user id missing");
    }

    if (user->password_hash[0] == '\0') {
        return Result_make_failure("password hash missing");
    }

    if (!RepositoryUtils_is_safe_field_text(user->user_id) ||
        !RepositoryUtils_is_safe_field_text(user->password_hash)) {
        return Result_make_failure("user field contains reserved characters");
    }

    if (!UserRepository_is_valid_role(user->role)) {
        return Result_make_failure("user role invalid");
    }

    return Result_make_success("user valid");
}

static Result UserRepository_parse_int(const char *text, int *out_value) {
    char *end = 0;
    long value = 0;

    if (text == 0 || out_value == 0 || text[0] == '\0') {
        return Result_make_failure("integer field missing");
    }

    errno = 0;
    value = strtol(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || value < INT_MIN || value > INT_MAX) {
        return Result_make_failure("integer field invalid");
    }

    *out_value = (int)value;
    return Result_make_success("integer field parsed");
}

static Result UserRepository_parse_line(const char *line, User *out_user) {
    char buffer[TEXT_FILE_REPOSITORY_LINE_CAPACITY];
    char *fields[USER_REPOSITORY_FIELD_COUNT];
    size_t field_count = 0;
    int parsed_role = 0;
    Result result;

    if (line == 0 || out_user == 0) {
        return Result_make_failure("user line arguments missing");
    }

    if (strlen(line) >= sizeof(buffer)) {
        return Result_make_failure("user line too long");
    }

    strcpy(buffer, line);
    result = RepositoryUtils_split_pipe_line(
        buffer,
        fields,
        USER_REPOSITORY_FIELD_COUNT,
        &field_count
    );
    if (result.success == 0) {
        return result;
    }

    result = RepositoryUtils_validate_field_count(field_count, USER_REPOSITORY_FIELD_COUNT);
    if (result.success == 0) {
        return result;
    }

    memset(out_user, 0, sizeof(*out_user));
    if (strlen(fields[0]) >= sizeof(out_user->user_id) ||
        strlen(fields[1]) >= sizeof(out_user->password_hash)) {
        return Result_make_failure("user field too long");
    }

    strcpy(out_user->user_id, fields[0]);
    strcpy(out_user->password_hash, fields[1]);

    result = UserRepository_parse_int(fields[2], &parsed_role);
    if (result.success == 0) {
        return result;
    }

    out_user->role = (UserRole)parsed_role;
    return UserRepository_validate_user(out_user);
}

static Result UserRepository_format_line(const User *user, char *buffer, size_t buffer_size) {
    int written = 0;
    Result result = UserRepository_validate_user(user);

    if (result.success == 0) {
        return result;
    }

    if (buffer == 0 || buffer_size == 0) {
        return Result_make_failure("user output buffer missing");
    }

    written = snprintf(
        buffer,
        buffer_size,
        "%s|%s|%d",
        user->user_id,
        user->password_hash,
        (int)user->role
    );
    if (written < 0 || (size_t)written >= buffer_size) {
        return Result_make_failure("user line formatting failed");
    }

    return Result_make_success("user line ready");
}

static Result UserRepository_ensure_header(const UserRepository *repository) {
    FILE *file = 0;
    char line[TEXT_FILE_REPOSITORY_LINE_CAPACITY];
    int has_content = 0;
    Result result;

    if (repository == 0) {
        return Result_make_failure("user repository missing");
    }

    result = TextFileRepository_ensure_file_exists(&repository->file_repository);
    if (result.success == 0) {
        return result;
    }

    file = fopen(repository->file_repository.path, "r");
    if (file == 0) {
        return Result_make_failure("failed to read user repository");
    }

    while (fgets(line, sizeof(line), file) != 0) {
        RepositoryUtils_strip_line_endings(line);
        if (RepositoryUtils_is_blank_line(line)) {
            continue;
        }

        has_content = 1;
        break;
    }

    fclose(file);

    if (has_content == 0) {
        return TextFileRepository_append_line(&repository->file_repository, USER_REPOSITORY_HEADER);
    }

    if (strcmp(line, USER_REPOSITORY_HEADER) != 0) {
        return Result_make_failure("user repository header invalid");
    }

    return Result_make_success("user repository header ready");
}

static Result UserRepository_find_line_handler(const char *line, void *context) {
    UserRepositoryFindContext *find_context = (UserRepositoryFindContext *)context;
    User parsed_user;
    Result result = UserRepository_parse_line(line, &parsed_user);

    if (result.success == 0) {
        return result;
    }

    if (strcmp(parsed_user.user_id, find_context->user_id) != 0) {
        return Result_make_success("user id mismatch");
    }

    *(find_context->out_user) = parsed_user;
    find_context->found = 1;
    return Result_make_failure("user found");
}

static Result UserRepository_find_internal(
    const UserRepository *repository,
    const char *user_id,
    User *out_user,
    int *out_found
) {
    UserRepositoryFindContext context;
    Result result;

    if (out_found != 0) {
        *out_found = 0;
    }

    if (repository == 0 || user_id == 0 || user_id[0] == '\0' || out_user == 0) {
        return Result_make_failure("user find arguments missing");
    }

    result = UserRepository_ensure_header(repository);
    if (result.success == 0) {
        return result;
    }

    memset(&context, 0, sizeof(context));
    context.user_id = user_id;
    context.out_user = out_user;

    result = TextFileRepository_for_each_data_line(
        &repository->file_repository,
        UserRepository_find_line_handler,
        &context
    );

    if (context.found != 0) {
        if (out_found != 0) {
            *out_found = 1;
        }
        return Result_make_success("user found");
    }

    if (result.success == 0 && strcmp(result.message, "user found") != 0) {
        return result;
    }

    return Result_make_success("user not found");
}

Result UserRepository_init(UserRepository *repository, const char *path) {
    Result result;

    if (repository == 0) {
        return Result_make_failure("user repository missing");
    }

    result = TextFileRepository_init(&repository->file_repository, path);
    if (result.success == 0) {
        return result;
    }

    return UserRepository_ensure_header(repository);
}

Result UserRepository_append(const UserRepository *repository, const User *user) {
    User existing_user;
    int found = 0;
    char line[TEXT_FILE_REPOSITORY_LINE_CAPACITY];
    Result result = UserRepository_validate_user(user);

    if (result.success == 0) {
        return result;
    }

    result = UserRepository_find_internal(repository, user->user_id, &existing_user, &found);
    if (result.success == 0) {
        return result;
    }

    if (found != 0) {
        return Result_make_failure("user id already exists");
    }

    result = UserRepository_format_line(user, line, sizeof(line));
    if (result.success == 0) {
        return result;
    }

    return TextFileRepository_append_line(&repository->file_repository, line);
}

Result UserRepository_find_by_user_id(
    const UserRepository *repository,
    const char *user_id,
    User *out_user
) {
    int found = 0;
    Result result = UserRepository_find_internal(repository, user_id, out_user, &found);

    if (result.success == 0) {
        return result;
    }

    if (found == 0) {
        return Result_make_failure("user not found");
    }

    return Result_make_success("user found");
}
