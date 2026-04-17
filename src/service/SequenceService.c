/**
 * @file SequenceService.c
 * @brief SequenceService 实现：单文件持久化序列号分配器
 *
 * 该实现支撑 Wave 3 性能整改 P0：
 *   原挂号/CRUD 创建路径 ~ 4*N 行读 + 1*N 行写（load_all + save_all）
 *   → 替换为 O(1) 常数 I/O：读 sequences.txt 一次、重写 sequences.txt 一次。
 *
 * 存储格式：每行 "<TYPE_NAME>|<LAST_SEQ>"，例如
 *   PATIENT|30
 *   REGISTRATION|4
 *
 * 文件权限与落盘：直接复用 TextFileRepository_save_file，
 * 保持与其它 data/*.txt 一致（0600 + fsync + rename 原子替换）。
 */

#include "service/SequenceService.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/StringUtils.h"
#include "repository/RepositoryUtils.h"
#include "repository/TextFileRepository.h"

/** sequences.txt 每行最大长度预算：TYPE(32) + '|' + INT(11) + '\n' + 余量 */
#define SEQUENCE_LINE_BUFFER 96

/**
 * @brief 扫描过程中使用的上下文：记录当前类型前缀及找到的最大序号
 */
typedef struct SequenceScanContext {
    const char *prefix;   /**< ID 前缀，如 "REG" */
    size_t prefix_len;    /**< 前缀长度（预计算） */
    int max_seq;          /**< 扫描到的最大序号 */
} SequenceScanContext;

/**
 * @brief 在内存 entries 数组中查找指定类型名
 *
 * @param self       服务实例
 * @param type_name  类型名
 * @return 找到时返回对应 entry 指针，未找到返回 NULL
 */
static SequenceEntry *SequenceService_find_entry(
    SequenceService *self,
    const char *type_name
) {
    size_t i;

    if (self == 0 || type_name == 0) {
        return 0;
    }
    for (i = 0; i < self->entry_count; i++) {
        if (strcmp(self->entries[i].type_name, type_name) == 0) {
            return &self->entries[i];
        }
    }
    return 0;
}

/**
 * @brief 在 entries 数组末尾追加一个新条目
 */
static Result SequenceService_append_entry(
    SequenceService *self,
    const char *type_name,
    int last_seq
) {
    SequenceEntry *entry = 0;

    if (self->entry_count >= HIS_SEQUENCE_MAX_TYPES) {
        return Result_make_failure("sequence types exceed capacity");
    }
    entry = &self->entries[self->entry_count];
    StringUtils_copy(entry->type_name, sizeof(entry->type_name), type_name);
    entry->last_seq = last_seq;
    self->entry_count += 1;
    return Result_make_success("sequence entry added");
}

/**
 * @brief 回调：扫描数据文件每行，提取行首 ID 的序号部分
 *
 * 行首格式假定为 "<PREFIX><digits>|...鈥�，若不匹配则跳过。
 */
static Result SequenceService_scan_line_handler(const char *line, void *context) {
    SequenceScanContext *ctx = (SequenceScanContext *)context;
    const char *suffix = 0;
    char *end = 0;
    long value = 0;

    if (ctx == 0 || line == 0) {
        return Result_make_success("scan line skipped");
    }

    /* 校验前缀 */
    if (strncmp(line, ctx->prefix, ctx->prefix_len) != 0) {
        return Result_make_success("prefix mismatch");
    }
    suffix = line + ctx->prefix_len;

    /* 解析数字部分，遇到 '|' 或 '\0' 停止 */
    errno = 0;
    value = strtol(suffix, &end, 10);
    if (end == suffix || (end != 0 && *end != '|' && *end != '\0')) {
        return Result_make_success("id format unexpected");
    }
    if (errno != 0 || value < 0 || value > 2147483000L) {
        return Result_make_success("id out of range");
    }

    if ((int)value > ctx->max_seq) {
        ctx->max_seq = (int)value;
    }
    return Result_make_success("scan line processed");
}

/**
 * @brief 扫描数据文件，返回最大已用序号（找不到则为 0）
 */
static Result SequenceService_scan_data_file(
    const char *data_path,
    const char *id_prefix,
    int *out_max
) {
    TextFileRepository repo;
    SequenceScanContext ctx;
    Result result;
    FILE *probe = 0;

    if (out_max == 0) {
        return Result_make_failure("scan output missing");
    }
    *out_max = 0;

    if (data_path == 0 || data_path[0] == '\0') {
        return Result_make_success("scan skipped (no path)");
    }
    if (id_prefix == 0 || id_prefix[0] == '\0') {
        return Result_make_success("scan skipped (no prefix)");
    }

    /* 仅当文件存在时扫描；不存在直接返回 0 避免 ensure_file_exists 创建空文件 */
    probe = fopen(data_path, "r");
    if (probe == 0) {
        return Result_make_success("data file absent");
    }
    fclose(probe);

    result = TextFileRepository_init(&repo, data_path);
    if (!result.success) {
        return result;
    }

    ctx.prefix = id_prefix;
    ctx.prefix_len = strlen(id_prefix);
    ctx.max_seq = 0;

    result = TextFileRepository_for_each_data_line(
        &repo, SequenceService_scan_line_handler, &ctx
    );
    if (!result.success) {
        return result;
    }

    *out_max = ctx.max_seq;
    return Result_make_success("data file scanned");
}

/**
 * @brief 将服务内存状态序列化为完整文本并调用 save_file 落盘
 */
static Result SequenceService_persist(SequenceService *self) {
    TextFileRepository repo;
    char *buffer = 0;
    size_t capacity = 0;
    size_t used = 0;
    size_t i;
    Result result;

    if (self == 0 || self->file_path[0] == '\0') {
        return Result_make_failure("sequence service not initialized");
    }

    result = TextFileRepository_init(&repo, self->file_path);
    if (!result.success) {
        return result;
    }

    /* 预估容量：每条目最多 SEQUENCE_LINE_BUFFER 字节 */
    capacity = (self->entry_count + 1) * SEQUENCE_LINE_BUFFER;
    if (capacity < SEQUENCE_LINE_BUFFER) {
        capacity = SEQUENCE_LINE_BUFFER;
    }
    buffer = (char *)malloc(capacity);
    if (buffer == 0) {
        return Result_make_failure("sequence buffer alloc failed");
    }
    buffer[0] = '\0';

    for (i = 0; i < self->entry_count; i++) {
        int written = snprintf(
            buffer + used,
            capacity - used,
            "%s|%d\n",
            self->entries[i].type_name,
            self->entries[i].last_seq
        );
        if (written < 0 || (size_t)written >= capacity - used) {
            free(buffer);
            return Result_make_failure("sequence line overflow");
        }
        used += (size_t)written;
    }

    result = TextFileRepository_save_file(&repo, buffer);
    free(buffer);
    return result;
}

/**
 * @brief 回调：加载 sequences.txt 每一行
 */
static Result SequenceService_load_line_handler(const char *line, void *context) {
    SequenceService *self = (SequenceService *)context;
    char buffer[SEQUENCE_LINE_BUFFER];
    char *fields[2];
    size_t field_count = 0;
    int value = 0;
    Result result;

    if (line == 0 || self == 0) {
        return Result_make_failure("sequence load context invalid");
    }
    if (strlen(line) >= sizeof(buffer)) {
        return Result_make_failure("sequence line too long");
    }
    strncpy(buffer, line, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';

    result = RepositoryUtils_split_pipe_line(buffer, fields, 2, &field_count);
    if (!result.success) {
        return result;
    }
    result = RepositoryUtils_validate_field_count(field_count, 2);
    if (!result.success) {
        return result;
    }
    result = RepositoryUtils_parse_int(fields[1], &value, "sequence last_seq");
    if (!result.success) {
        return result;
    }
    if (value < 0) {
        return Result_make_failure("sequence last_seq negative");
    }
    return SequenceService_append_entry(self, fields[0], value);
}

/**
 * @brief 尝试从现有 sequences.txt 加载；文件不存在或为空则返回 "empty"
 *
 * @param self         服务实例
 * @param out_empty    输出：1=文件不存在或空，0=成功加载
 */
static Result SequenceService_try_load_file(SequenceService *self, int *out_empty) {
    TextFileRepository repo;
    FILE *probe = 0;
    long size = 0;
    Result result;

    *out_empty = 1;
    probe = fopen(self->file_path, "r");
    if (probe == 0) {
        return Result_make_success("sequence file absent");
    }
    /* 粗略判断空文件（无内容则视为首次运行） */
    if (fseek(probe, 0, SEEK_END) == 0) {
        size = ftell(probe);
    }
    fclose(probe);
    if (size <= 0) {
        return Result_make_success("sequence file empty");
    }

    result = TextFileRepository_init(&repo, self->file_path);
    if (!result.success) {
        return result;
    }
    /* sequences.txt 不使用表头，全部行都是数据 */
    result = TextFileRepository_for_each_non_empty_line(
        &repo, SequenceService_load_line_handler, self
    );
    if (!result.success) {
        return result;
    }
    *out_empty = 0;
    return Result_make_success("sequence file loaded");
}

Result SequenceService_init(
    SequenceService *self,
    const char *file_path,
    const SequenceTypeSpec *specs,
    size_t spec_count
) {
    Result result;
    int file_empty = 1;
    size_t i;

    if (self == 0 || file_path == 0 || file_path[0] == '\0') {
        return Result_make_failure("sequence service arguments invalid");
    }
    if (spec_count > HIS_SEQUENCE_MAX_TYPES) {
        return Result_make_failure("sequence types exceed capacity");
    }
    if (spec_count > 0 && specs == 0) {
        return Result_make_failure("sequence specs missing");
    }

    memset(self, 0, sizeof(*self));
    StringUtils_copy(self->file_path, sizeof(self->file_path), file_path);

    /* 步骤 1：尝试从既有 sequences.txt 读取 */
    result = SequenceService_try_load_file(self, &file_empty);
    if (!result.success) {
        return result;
    }

    /* 步骤 2：对 specs 中尚未出现在 entries 的类型，初始化（含迁移扫描） */
    for (i = 0; i < spec_count; i++) {
        const SequenceTypeSpec *spec = &specs[i];
        int seed = 0;

        if (spec->type_name[0] == '\0') {
            return Result_make_failure("sequence spec type_name empty");
        }
        if (SequenceService_find_entry(self, spec->type_name) != 0) {
            /* 文件中已有该类型，保留现值；新迁移数据在 next 前已持久化 */
            continue;
        }

        /* 文件里没有这个类型：从业务数据文件扫描一次 */
        result = SequenceService_scan_data_file(spec->data_path, spec->id_prefix, &seed);
        if (!result.success) {
            return result;
        }
        result = SequenceService_append_entry(self, spec->type_name, seed);
        if (!result.success) {
            return result;
        }
    }

    /* 步骤 3：若文件原本不存在/为空，或新增了类型，写回一次确保持久化 */
    if (file_empty || spec_count > 0) {
        result = SequenceService_persist(self);
        if (!result.success) {
            return result;
        }
    }

    self->loaded = 1;
    return Result_make_success("sequence service ready");
}

Result SequenceService_next(
    SequenceService *self,
    const char *type_name,
    int *out_seq
) {
    SequenceEntry *entry = 0;
    Result result;

    if (self == 0 || !self->loaded || type_name == 0 || type_name[0] == '\0' || out_seq == 0) {
        return Result_make_failure("sequence next arguments invalid");
    }

    entry = SequenceService_find_entry(self, type_name);
    if (entry == 0) {
        return Result_make_failure("sequence type not registered");
    }
    if (entry->last_seq >= 2147483000) {
        return Result_make_failure("sequence overflow");
    }

    entry->last_seq += 1;
    result = SequenceService_persist(self);
    if (!result.success) {
        /* 回滚内存，保持状态一致 */
        entry->last_seq -= 1;
        return result;
    }

    *out_seq = entry->last_seq;
    return Result_make_success("sequence next allocated");
}

Result SequenceService_peek(
    const SequenceService *self,
    const char *type_name,
    int *out_seq
) {
    const SequenceEntry *entry = 0;
    size_t i;

    if (self == 0 || type_name == 0 || type_name[0] == '\0' || out_seq == 0) {
        return Result_make_failure("sequence peek arguments invalid");
    }

    for (i = 0; i < self->entry_count; i++) {
        if (strcmp(self->entries[i].type_name, type_name) == 0) {
            entry = &self->entries[i];
            break;
        }
    }
    if (entry == 0) {
        return Result_make_failure("sequence type not registered");
    }

    *out_seq = entry->last_seq;
    return Result_make_success("sequence peek ok");
}
