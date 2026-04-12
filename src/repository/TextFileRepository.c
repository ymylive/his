/**
 * @file TextFileRepository.c
 * @brief 文本文件仓储基础层实现
 *
 * 实现基于文本文件的通用数据存储功能，包括：
 * - 文件路径初始化
 * - 父目录递归创建
 * - 文件存在性检查与创建
 * - 逐行遍历（支持跳过表头）
 * - 行追加写入
 * - 全量保存（通过临时文件原子替换）
 *
 * 支持跨平台（Windows 和 Unix/macOS）的目录创建。
 */

#include "repository/TextFileRepository.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

/* 跨平台目录创建宏 */
#if defined(_WIN32)
#include <direct.h>
#define HIS_MKDIR(path) _mkdir(path)
#else
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>
#define HIS_MKDIR(path) mkdir(path, 0750) /* Unix/macOS：权限 rwxr-x--- */
#endif

#include "repository/RepositoryUtils.h"

/**
 * @brief 判断字符是否为路径分隔符（'/' 或 '\'）
 * @param character 待判断的字符
 * @return 1 表示是路径分隔符，0 表示不是
 */
static int TextFileRepository_is_separator(char character) {
    return character == '/' || character == '\\';
}

/**
 * @brief 检查仓储实例是否已设置有效路径
 * @param repository 仓储实例指针
 * @return 1 表示路径有效，0 表示无效
 */
static int TextFileRepository_has_path(const TextFileRepository *repository) {
    return repository != 0 && repository->path[0] != '\0';
}

/**
 * @brief 创建一个 failure 结果（内部辅助函数）
 * @param message 错误消息
 * @return failure 结果
 */
static Result TextFileRepository_make_message_result(const char *message) {
    return Result_make_failure(message);
}

/**
 * @brief 在需要时创建目录
 *
 * 如果目录已存在则跳过；如果路径是文件则报错；否则尝试创建目录。
 *
 * @param path 目录路径
 * @return 成功返回 success；失败返回 failure
 */
static Result TextFileRepository_create_directory_if_needed(const char *path) {
    struct stat info;

    /* 空路径直接跳过 */
    if (path == 0 || path[0] == '\0') {
        return Result_make_success("directory skipped");
    }

    /* 检查路径是否已存在 */
    if (stat(path, &info) == 0) {
        if (S_ISDIR(info.st_mode)) {
            return Result_make_success("directory exists"); /* 目录已存在 */
        }

        return TextFileRepository_make_message_result("path exists as file"); /* 路径已存在但是文件 */
    }

    /* 尝试创建目录 */
    if (HIS_MKDIR(path) == 0 || errno == EEXIST) {
        return Result_make_success("directory ready");
    }

    return TextFileRepository_make_message_result("failed to create directory");
}

/**
 * @brief 递归创建文件路径的所有父目录
 *
 * 遍历路径中的每个路径分隔符，逐层创建不存在的目录。
 * 支持 Unix 绝对路径（/开头）和 Windows 盘符路径（C:\开头）。
 *
 * @param path 文件的完整路径
 * @return 成功返回 success；失败返回 failure
 */
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

    /* 复制路径到可修改缓冲区 */
    strncpy(buffer, path, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';
    for (index = 0; index < length; index++) {
        if (!TextFileRepository_is_separator(buffer[index])) {
            continue; /* 跳过非分隔符字符 */
        }

        /* 跳过根路径（Unix 的 / 或 Windows 的 C:\） */
        if (index == 0 || (index == 2 && buffer[1] == ':')) {
            continue;
        }

        /* 临时截断路径，创建当前层级的目录 */
        buffer[index] = '\0';

        if (buffer[0] != '\0') {
            Result directory_result = TextFileRepository_create_directory_if_needed(buffer);
            if (directory_result.success == 0) {
                buffer[index] = path[index]; /* 恢复分隔符 */
                return directory_result;
            }
        }

        buffer[index] = path[index]; /* 恢复分隔符，继续处理下一层 */
    }

    return Result_make_success("parent directories ready");
}

/**
 * @brief 丢弃当前行剩余字符（用于处理过长的行）
 *
 * 逐字符读取直到遇到换行符或文件结束。
 *
 * @param file 文件指针
 */
static void TextFileRepository_discard_rest_of_line(FILE *file) {
    int character = 0;

    while ((character = fgetc(file)) != EOF) {
        if (character == '\n') {
            return;
        }
    }
}

/**
 * @brief 遍历文件中的行并调用回调处理
 *
 * 内部实现：逐行读取文件，跳过空行，可选跳过表头行（第一个非空行）。
 * 对每个数据行调用回调函数；如果回调返回 failure，则终止遍历。
 *
 * @param repository   仓储实例指针
 * @param line_handler 行处理回调函数
 * @param context      传递给回调的上下文指针
 * @param skip_header  是否跳过第一个非空行（表头）：非0=跳过，0=不跳过
 * @return 遍历完成返回 success；错误或回调失败时返回 failure
 */
static Result TextFileRepository_iterate_lines(
    const TextFileRepository *repository,
    TextFileRepositoryLineHandler line_handler,
    void *context,
    int skip_header
) {
    char line[TEXT_FILE_REPOSITORY_LINE_CAPACITY];
    FILE *file = 0;
    Result result;
    int header_skipped = 0; /* 标记表头是否已跳过 */

    if (line_handler == 0) {
        return Result_make_failure("line handler missing");
    }

    /* 确保文件存在 */
    result = TextFileRepository_ensure_file_exists(repository);
    if (result.success == 0) {
        return result;
    }

    file = fopen(repository->path, "r");
    if (file == 0) {
        return Result_make_failure("failed to read repository file");
    }

    /* 逐行读取文件 */
    while (fgets(line, sizeof(line), file) != 0) {
        /* 检测行是否过长（缓冲区满但未到换行符且非文件末尾） */
        if (strchr(line, '\n') == 0 && !feof(file)) {
            TextFileRepository_discard_rest_of_line(file);
            fclose(file);
            return Result_make_failure("repository line too long");
        }

        /* 去除行尾换行符并跳过空行 */
        RepositoryUtils_strip_line_endings(line);
        if (RepositoryUtils_is_blank_line(line)) {
            continue;
        }

        /* 如果需要跳过表头且尚未跳过，则跳过当前行 */
        if (skip_header != 0 && header_skipped == 0) {
            header_skipped = 1;
            continue;
        }

        /* 调用回调处理当前行 */
        result = line_handler(line, context);
        if (result.success == 0) {
            fclose(file);
            return result; /* 回调返回失败，终止遍历 */
        }
    }

    /* 检查是否有读取错误 */
    if (ferror(file) != 0) {
        fclose(file);
        return Result_make_failure("repository read failed");
    }

    fclose(file);
    return Result_make_success("repository lines loaded");
}

/**
 * @brief 初始化文本文件仓储
 * @param repository 仓储实例指针
 * @param path       数据文件路径
 * @return 成功返回 success；路径为空或过长时返回 failure
 */
Result TextFileRepository_init(TextFileRepository *repository, const char *path) {
    size_t length = 0;

    if (repository == 0 || path == 0 || path[0] == '\0') {
        return Result_make_failure("repository path missing");
    }

    length = strlen(path);
    if (length >= sizeof(repository->path)) {
        return Result_make_failure("repository path too long");
    }

    /* 保存路径并标记为未初始化（文件未确认存在） */
    strncpy(repository->path, path, sizeof(repository->path) - 1);
    repository->path[sizeof(repository->path) - 1] = '\0';
    repository->initialized = 0;
    return Result_make_success("repository ready");
}

/**
 * @brief 确保数据文件存在
 *
 * 检查文件是否已确认存在。如果尚未确认，则创建父目录并以追加模式打开文件
 * （如果文件不存在则自动创建）。确认后设置 initialized 标志避免重复检查。
 *
 * @param repository 仓储实例指针
 * @return 成功返回 success；创建失败时返回 failure
 */
Result TextFileRepository_ensure_file_exists(const TextFileRepository *repository) {
    FILE *file = 0;
    Result result;

    if (!TextFileRepository_has_path(repository)) {
        return Result_make_failure("repository not initialized");
    }

    /* 已初始化则直接返回 */
    if (repository->initialized) {
        return Result_make_success("repository file ready");
    }

    /* 创建父目录 */
    result = TextFileRepository_create_parent_directories(repository->path);
    if (result.success == 0) {
        return result;
    }

    /* 以追加模式打开文件（不存在时自动创建） */
#if defined(_WIN32)
    file = fopen(repository->path, "a");
#else
    {
        int fd = open(repository->path, O_WRONLY | O_CREAT | O_APPEND, 0600);
        if (fd < 0) {
            return Result_make_failure("failed to open repository file");
        }
        file = fdopen(fd, "a");
        if (file == 0) {
            close(fd);
            return Result_make_failure("failed to open repository file");
        }
    }
#endif
    if (file == 0) {
        return Result_make_failure("failed to open repository file");
    }

    fclose(file);
    /* 通过强制类型转换去除 const 限定符，设置已初始化标志 */
    ((TextFileRepository *)repository)->initialized = 1;
    return Result_make_success("repository file ready");
}

/**
 * @brief 遍历文件中所有非空行（不跳过表头）
 * @param repository   仓储实例指针
 * @param line_handler 行处理回调函数
 * @param context      传递给回调的上下文指针
 * @return 遍历完成返回 success；错误时返回 failure
 */
Result TextFileRepository_for_each_non_empty_line(
    const TextFileRepository *repository,
    TextFileRepositoryLineHandler line_handler,
    void *context
) {
    return TextFileRepository_iterate_lines(repository, line_handler, context, 0);
}

/**
 * @brief 遍历文件中的数据行（跳过表头）
 * @param repository   仓储实例指针
 * @param line_handler 行处理回调函数
 * @param context      传递给回调的上下文指针
 * @return 遍历完成返回 success；错误时返回 failure
 */
Result TextFileRepository_for_each_data_line(
    const TextFileRepository *repository,
    TextFileRepositoryLineHandler line_handler,
    void *context
) {
    return TextFileRepository_iterate_lines(repository, line_handler, context, 1);
}

/**
 * @brief 向文件末尾追加一行数据
 * @param repository 仓储实例指针
 * @param line       要追加的行文本
 * @return 成功返回 success；写入失败时返回 failure
 */
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

    /* 确保文件存在 */
    result = TextFileRepository_ensure_file_exists(repository);
    if (result.success == 0) {
        return result;
    }

    /* 以追加模式打开文件 */
#if defined(_WIN32)
    file = fopen(repository->path, "a");
#else
    {
        int fd = open(repository->path, O_WRONLY | O_CREAT | O_APPEND, 0600);
        if (fd < 0) {
            return Result_make_failure("failed to append repository file");
        }
        file = fdopen(fd, "a");
        if (file == 0) {
            close(fd);
            return Result_make_failure("failed to append repository file");
        }
    }
#endif
    if (file == 0) {
        return Result_make_failure("failed to append repository file");
    }

    /* 写入行内容 */
    if (fputs(line, file) == EOF) {
        fclose(file);
        return Result_make_failure("failed to write repository line");
    }

    /* 如果行末尾没有换行符，则自动补充 */
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

/**
 * @brief 全量覆盖保存文件内容
 *
 * 采用"先写临时文件再原子替换"的策略，避免写入中断导致数据损坏。
 * Windows 平台需要先删除目标文件再重命名（rename 不支持覆盖已有文件）。
 *
 * @param repository 仓储实例指针
 * @param content    要写入的完整文件内容
 * @return 成功返回 success；写入或替换失败时返回 failure
 */
Result TextFileRepository_save_file(
    const TextFileRepository *repository,
    const char *content
) {
    char tmp_path[TEXT_FILE_REPOSITORY_PATH_CAPACITY + 8]; /* 额外空间存放 ".tmp" 后缀 */
    FILE *file = 0;
    Result result;

    if (content == 0) {
        return Result_make_failure("content missing");
    }

    result = TextFileRepository_ensure_file_exists(repository);
    if (result.success == 0) {
        return result;
    }

    /* 构造临时文件路径：原路径 + ".tmp" */
    if (snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", repository->path) >= (int)sizeof(tmp_path)) {
        return Result_make_failure("repository path too long for temp file");
    }

    /* 写入临时文件 */
#if defined(_WIN32)
    file = fopen(tmp_path, "w");
#else
    {
        int fd = open(tmp_path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
        if (fd < 0) {
            return Result_make_failure("failed to open temp file");
        }
        file = fdopen(fd, "w");
        if (file == 0) {
            close(fd);
            return Result_make_failure("failed to open temp file");
        }
    }
#endif
    if (file == 0) {
        return Result_make_failure("failed to open temp file");
    }

    if (fputs(content, file) == EOF) {
        fclose(file);
        remove(tmp_path); /* 清理失败的临时文件 */
        return Result_make_failure("failed to write temp file");
    }

    if (ferror(file) != 0) {
        fclose(file);
        remove(tmp_path);
        return Result_make_failure("temp file write error");
    }

    fclose(file);

    /* Windows 平台上 rename 不支持覆盖已有文件，需先删除 */
#if defined(_WIN32)
    remove(repository->path);
#endif

    /* 原子替换：将临时文件重命名为正式文件 */
    if (rename(tmp_path, repository->path) != 0) {
        remove(tmp_path);
        return Result_make_failure("failed to replace repository file");
    }

    return Result_make_success("repository file saved");
}
