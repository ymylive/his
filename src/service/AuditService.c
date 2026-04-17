/**
 * @file AuditService.c
 * @brief 审计日志服务模块实现
 *
 * 将审计事件以 append-only 方式追加到 data/audit.log。
 * 行格式：
 *   <iso8601>|<event>|<actor>|<role>|<entity>|<id>|<result>|<detail>
 *
 * 设计要点：
 * - 时间戳使用 UTC ISO-8601（strftime %Y-%m-%dT%H:%M:%SZ）。
 * - 字段转义复用 RepositoryUtils_escape_field，避免用户输入破坏格式。
 * - 写入后 fflush 保证落盘；文件权限 0600（POSIX）。
 * - 单线程模型，无需加锁。
 */

#include "service/AuditService.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#include <sys/stat.h>
#if defined(_WIN32)
#include <direct.h>
#define AUDIT_MKDIR(path) _mkdir(path)
#else
#include <sys/types.h>
#define AUDIT_MKDIR(path) mkdir((path), 0700)
#endif

#include "repository/RepositoryUtils.h"

/** 单个日志字段转义后的最大长度（含终止符）。每个原始字符最多膨胀为两个字符。 */
#define AUDIT_FIELD_ESCAPED_CAPACITY 256

/** 整行日志缓冲区大小（8 个转义字段 + 分隔符 + 时间戳 + 余量）。 */
#define AUDIT_LINE_CAPACITY 2560

/** ISO-8601 时间戳（"YYYY-MM-DDTHH:MM:SSZ"）缓冲区大小。 */
#define AUDIT_TIMESTAMP_CAPACITY 24

/**
 * @brief 将事件类型枚举转换为日志中的文本标签
 */
static const char *AuditService_event_label(AuditEventType type) {
    switch (type) {
        case AUDIT_EVENT_LOGIN_SUCCESS:   return "LOGIN_SUCCESS";
        case AUDIT_EVENT_LOGIN_FAILURE:   return "LOGIN_FAILURE";
        case AUDIT_EVENT_LOGOUT:          return "LOGOUT";
        case AUDIT_EVENT_PASSWORD_CHANGE: return "PASSWORD_CHANGE";
        case AUDIT_EVENT_CREATE:          return "CREATE";
        case AUDIT_EVENT_UPDATE:          return "UPDATE";
        case AUDIT_EVENT_DELETE:          return "DELETE";
        case AUDIT_EVENT_READ:            return "READ";
        case AUDIT_EVENT_DISCHARGE:       return "DISCHARGE";
        case AUDIT_EVENT_DISPENSE:        return "DISPENSE";
        default:                          return "UNKNOWN";
    }
}

/**
 * @brief 写入当前 UTC 时间的 ISO-8601 时间戳
 *
 * 使用 gmtime + strftime 生成 "YYYY-MM-DDTHH:MM:SSZ"。
 *
 * @param buffer   输出缓冲区
 * @param capacity 缓冲区容量
 */
static void AuditService_format_timestamp(char *buffer, size_t capacity) {
    time_t now = time(0);
    struct tm *utc = 0;

    if (buffer == 0 || capacity == 0) {
        return;
    }

    utc = gmtime(&now);
    if (utc == 0) {
        /* 理论上不会发生；退化为空字符串以免格式错乱 */
        buffer[0] = '\0';
        return;
    }

    if (strftime(buffer, capacity, "%Y-%m-%dT%H:%M:%SZ", utc) == 0) {
        buffer[0] = '\0';
    }
}

/**
 * @brief 将源字段转义写入目标缓冲区
 *
 * 空指针视为空串。复用 RepositoryUtils_escape_field 确保 '|'、'\\'
 * 被正确转义，防止用户输入破坏管道分隔格式。
 */
static void AuditService_copy_escaped(
    char *dest,
    size_t dest_capacity,
    const char *src
) {
    if (dest == 0 || dest_capacity == 0) {
        return;
    }
    if (src == 0) {
        dest[0] = '\0';
        return;
    }
    RepositoryUtils_escape_field(dest, dest_capacity, src);
}

/**
 * @brief 判断字符是否为路径分隔符
 */
static int AuditService_is_separator(char ch) {
    return ch == '/' || ch == '\\';
}

/**
 * @brief 确保日志文件的父目录存在（逐级创建）
 *
 * 遍历路径中的每个 '/' 或 '\\'，将前缀截断为目录，如不存在则创建。
 * 失败不阻断流程（可能目录由调用方预先创建）。
 */
static void AuditService_ensure_parent_dir(const char *path) {
    char buffer[HIS_FILE_PATH_CAPACITY];
    size_t i = 0;
    size_t len = 0;

    if (path == 0 || path[0] == '\0') {
        return;
    }

    len = strlen(path);
    if (len >= sizeof(buffer)) {
        return;
    }
    memcpy(buffer, path, len);
    buffer[len] = '\0';

    for (i = 0; i < len; i++) {
        if (!AuditService_is_separator(buffer[i])) {
            continue;
        }
        /* 跳过根分隔符（Unix '/' 或 Windows 盘符 'C:\'） */
        if (i == 0 || (i == 2 && buffer[1] == ':')) {
            continue;
        }
        buffer[i] = '\0';
        if (buffer[0] != '\0') {
            struct stat info;
            if (stat(buffer, &info) != 0) {
                (void)AUDIT_MKDIR(buffer);
            }
        }
        buffer[i] = path[i];
    }
}

/**
 * @brief 收紧日志文件权限到 0600（仅所有者可读写）
 *
 * 合规：审计日志可能包含操作者与目标信息，必须防止其他用户读取。
 * Windows 下 POSIX 权限位语义不同，留给未来 ACL 扩展。
 */
static void AuditService_secure_mode(const char *path) {
    if (path == 0 || path[0] == '\0') {
        return;
    }
#if defined(_WIN32)
    (void)path;
#else
    (void)chmod(path, 0600);
#endif
}

Result AuditService_init(AuditService *self, const char *data_dir) {
    size_t dir_len = 0;
    int written = 0;
    FILE *file = 0;

    if (self == 0) {
        return Result_make_failure("audit service missing");
    }

    memset(self, 0, sizeof(*self));

    /* data_dir 为空/NULL 时，日志写入当前目录下的 audit.log */
    if (data_dir == 0) {
        data_dir = "";
    }

    dir_len = strlen(data_dir);
    /* 若 data_dir 非空且未以分隔符结尾，补一个 '/'；保证 "data" / "data/" 同样工作 */
    if (dir_len > 0 && !AuditService_is_separator(data_dir[dir_len - 1])) {
        written = snprintf(self->log_path, sizeof(self->log_path), "%s/audit.log", data_dir);
    } else {
        written = snprintf(self->log_path, sizeof(self->log_path), "%saudit.log", data_dir);
    }

    if (written < 0 || (size_t)written >= sizeof(self->log_path)) {
        self->log_path[0] = '\0';
        return Result_make_failure("audit log path too long");
    }

    /* 确保父目录存在（首次启动可能目录尚未创建） */
    AuditService_ensure_parent_dir(self->log_path);

    /* 以追加模式打开一次，触发文件创建（不存在时） */
    file = fopen(self->log_path, "a");
    if (file == 0) {
        return Result_make_failure("failed to open audit log");
    }
    fclose(file);

    /* 收紧权限；失败不阻止初始化，但应尽力而为 */
    AuditService_secure_mode(self->log_path);

    return Result_make_success("audit service ready");
}

Result AuditService_record(
    AuditService *self,
    AuditEventType type,
    const char *actor_user_id,
    const char *actor_role,
    const char *target_entity,
    const char *target_id,
    const char *result_tag,
    const char *detail_utf8
) {
    char timestamp[AUDIT_TIMESTAMP_CAPACITY];
    char esc_actor[AUDIT_FIELD_ESCAPED_CAPACITY];
    char esc_role[AUDIT_FIELD_ESCAPED_CAPACITY];
    char esc_entity[AUDIT_FIELD_ESCAPED_CAPACITY];
    char esc_target[AUDIT_FIELD_ESCAPED_CAPACITY];
    char esc_result[AUDIT_FIELD_ESCAPED_CAPACITY];
    char esc_detail[AUDIT_FIELD_ESCAPED_CAPACITY];
    char line[AUDIT_LINE_CAPACITY];
    FILE *file = 0;
    int written = 0;

    if (self == 0 || self->log_path[0] == '\0') {
        return Result_make_failure("audit service not initialized");
    }

    AuditService_format_timestamp(timestamp, sizeof(timestamp));

    AuditService_copy_escaped(esc_actor, sizeof(esc_actor), actor_user_id);
    AuditService_copy_escaped(esc_role, sizeof(esc_role), actor_role);
    AuditService_copy_escaped(esc_entity, sizeof(esc_entity), target_entity);
    AuditService_copy_escaped(esc_target, sizeof(esc_target), target_id);
    AuditService_copy_escaped(esc_result, sizeof(esc_result), result_tag);
    AuditService_copy_escaped(esc_detail, sizeof(esc_detail), detail_utf8);

    written = snprintf(
        line,
        sizeof(line),
        "%s|%s|%s|%s|%s|%s|%s|%s\n",
        timestamp,
        AuditService_event_label(type),
        esc_actor,
        esc_role,
        esc_entity,
        esc_target,
        esc_result,
        esc_detail
    );
    if (written < 0 || (size_t)written >= sizeof(line)) {
        return Result_make_failure("audit line too long");
    }

    file = fopen(self->log_path, "a");
    if (file == 0) {
        return Result_make_failure("failed to open audit log");
    }

    if (fputs(line, file) == EOF) {
        fclose(file);
        return Result_make_failure("failed to write audit line");
    }

    /* fflush 保证日志即时落到内核缓冲区；合规要求事件不可丢失 */
    fflush(file);
    fclose(file);

    /* 每次写入后重申 0600，防御外部 chmod 或 umask */
    AuditService_secure_mode(self->log_path);

    return Result_make_success("audit recorded");
}
