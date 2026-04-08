/**
 * @file UserRepository.c
 * @brief 用户账户数据仓储层实现
 *
 * 实现用户账户（User）数据的持久化存储操作，包括：
 * - 用户数据的序列化与反序列化
 * - 数据校验（用户ID、密码哈希非空、角色枚举合法、保留字符检测）
 * - 初始化、追加（含ID唯一性检查）、按用户ID查找
 */

#include "repository/UserRepository.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "repository/RepositoryUtils.h"

/**
 * @brief 按用户ID查找时使用的上下文结构体
 */
typedef struct UserRepositoryFindContext {
    const char *user_id; /**< 要查找的用户ID */
    User *out_user;      /**< 输出：找到的用户数据 */
    int found;           /**< 是否已找到标志 */
} UserRepositoryFindContext;

/**
 * @brief 判断角色枚举值是否合法
 * @param role 用户角色
 * @return 1 表示合法，0 表示不合法
 */
static int UserRepository_is_valid_role(UserRole role) {
    return role == USER_ROLE_PATIENT ||
           role == USER_ROLE_DOCTOR ||
           role == USER_ROLE_ADMIN ||
           role == USER_ROLE_INPATIENT_MANAGER ||
           role == USER_ROLE_PHARMACY;
}

/**
 * @brief 校验用户数据的合法性
 *
 * 检查用户ID和密码哈希非空、不包含保留字符、角色值合法。
 *
 * @param user 待校验的用户数据
 * @return 合法返回 success；不合法返回 failure
 */
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

    /* 检查文本字段是否包含保留字符 */
    if (!RepositoryUtils_is_safe_field_text(user->user_id) ||
        !RepositoryUtils_is_safe_field_text(user->password_hash)) {
        return Result_make_failure("user field contains reserved characters");
    }

    if (!UserRepository_is_valid_role(user->role)) {
        return Result_make_failure("user role invalid");
    }

    return Result_make_success("user valid");
}

/**
 * @brief 解析整数字段（带溢出检查）
 * @param text      待解析的文本
 * @param out_value 输出参数，解析得到的整数值
 * @return 成功返回 success；格式不合法或溢出时返回 failure
 */
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

/**
 * @brief 将一行文本解析为用户结构体
 *
 * 按管道符拆分行内容，解析 user_id、password_hash、role 三个字段。
 *
 * @param line     管道符分隔的文本行
 * @param out_user 输出参数，解析得到的用户数据
 * @return 解析成功返回 success；格式不合法时返回 failure
 */
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

    /* 复制到可修改缓冲区 */
    strcpy(buffer, line);
    result = RepositoryUtils_split_pipe_line(
        buffer, fields, USER_REPOSITORY_FIELD_COUNT, &field_count
    );
    if (result.success == 0) {
        return result;
    }

    result = RepositoryUtils_validate_field_count(field_count, USER_REPOSITORY_FIELD_COUNT);
    if (result.success == 0) {
        return result;
    }

    memset(out_user, 0, sizeof(*out_user));

    /* 检查字段长度不超过缓冲区容量 */
    if (strlen(fields[0]) >= sizeof(out_user->user_id) ||
        strlen(fields[1]) >= sizeof(out_user->password_hash)) {
        return Result_make_failure("user field too long");
    }

    /* 复制 user_id 和 password_hash */
    strcpy(out_user->user_id, fields[0]);
    strcpy(out_user->password_hash, fields[1]);

    /* 解析角色字段 */
    result = UserRepository_parse_int(fields[2], &parsed_role);
    if (result.success == 0) {
        return result;
    }

    out_user->role = (UserRole)parsed_role;
    return UserRepository_validate_user(out_user);
}

/**
 * @brief 将用户结构体格式化为管道符分隔的文本行
 * @param user        用户数据
 * @param buffer      输出缓冲区
 * @param buffer_size 缓冲区大小
 * @return 成功返回 success；数据无效或缓冲区不足时返回 failure
 */
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
        buffer, buffer_size,
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

/**
 * @brief 确保用户数据文件包含正确的表头
 *
 * 空文件时写入表头；非空文件时验证表头是否匹配。
 */
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

    /* 读取第一个非空行 */
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

    /* 空文件：写入表头 */
    if (has_content == 0) {
        return TextFileRepository_append_line(&repository->file_repository, USER_REPOSITORY_HEADER);
    }

    /* 验证表头 */
    if (strcmp(line, USER_REPOSITORY_HEADER) != 0) {
        return Result_make_failure("user repository header invalid");
    }

    return Result_make_success("user repository header ready");
}

/**
 * @brief 按用户ID查找的行处理回调
 *
 * 找到匹配的用户后，通过返回 failure 终止遍历（特殊的回调机制用法）。
 */
static Result UserRepository_find_line_handler(const char *line, void *context) {
    UserRepositoryFindContext *find_context = (UserRepositoryFindContext *)context;
    User parsed_user;
    Result result = UserRepository_parse_line(line, &parsed_user);

    if (result.success == 0) {
        return result;
    }

    /* ID不匹配，继续遍历 */
    if (strcmp(parsed_user.user_id, find_context->user_id) != 0) {
        return Result_make_success("user id mismatch");
    }

    /* 找到匹配用户 */
    *(find_context->out_user) = parsed_user;
    find_context->found = 1;
    return Result_make_failure("user found"); /* 返回 failure 以终止遍历 */
}

/**
 * @brief 按用户ID查找用户的内部实现
 *
 * 遍历文件查找指定ID的用户。无论是否找到，操作本身成功都返回 success。
 *
 * @param repository 用户仓储实例
 * @param user_id    要查找的用户ID
 * @param out_user   输出参数，找到的用户数据
 * @param out_found  输出参数，是否找到
 * @return 操作成功返回 success；出错返回 failure
 */
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

    /* 检查是否通过回调找到了用户 */
    if (context.found != 0) {
        if (out_found != 0) {
            *out_found = 1;
        }
        return Result_make_success("user found");
    }

    /* 区分"未找到"和"真正的错误" */
    if (result.success == 0 && strcmp(result.message, "user found") != 0) {
        return result; /* 真正的错误 */
    }

    return Result_make_success("user not found");
}

/**
 * @brief 初始化用户仓储
 * @param repository 用户仓储实例指针
 * @param path       数据文件路径
 * @return 成功返回 success；参数无效或文件操作失败时返回 failure
 */
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

/**
 * @brief 追加一条用户记录
 *
 * 先检查用户ID是否已存在，确认不重复后格式化并追加写入。
 *
 * @param repository 用户仓储实例指针
 * @param user       要追加的用户数据
 * @return 成功返回 success；ID重复或写入失败时返回 failure
 */
Result UserRepository_append(const UserRepository *repository, const User *user) {
    User existing_user;
    int found = 0;
    char line[TEXT_FILE_REPOSITORY_LINE_CAPACITY];
    Result result = UserRepository_validate_user(user);

    if (result.success == 0) {
        return result;
    }

    /* 检查ID唯一性 */
    result = UserRepository_find_internal(repository, user->user_id, &existing_user, &found);
    if (result.success == 0) {
        return result;
    }

    if (found != 0) {
        return Result_make_failure("user id already exists");
    }

    /* 格式化并追加写入 */
    result = UserRepository_format_line(user, line, sizeof(line));
    if (result.success == 0) {
        return result;
    }

    return TextFileRepository_append_line(&repository->file_repository, line);
}

/**
 * @brief 按用户ID查找用户
 * @param repository 用户仓储实例指针
 * @param user_id    要查找的用户ID
 * @param out_user   输出参数，存放找到的用户数据
 * @return 找到返回 success；未找到时返回 failure
 */
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
