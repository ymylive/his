/**
 * @file SequenceService.h
 * @brief 单文件持久化序列号生成服务
 *
 * 为 HIS 系统提供 O(1) 摊销复杂度的 ID 序列号分配能力，替代
 * 原本在各 Service 的 create 路径中"全表加载 -> 扫描 max ID -> 全表回写"
 * 的 O(N) 写放大模式。
 *
 * 实现要点：
 * 1. 序列号按类型名（如 "REGISTRATION"）分槽，持久化到单一文本文件
 *    （默认 data/sequences.txt），每行格式 `<TYPE_NAME>|<LAST_SEQ>`。
 * 2. 内存中用定长数组缓存，查询/自增为 O(TYPE_COUNT)；TYPE_COUNT 通常 < 20。
 * 3. 每次 `next()` 后立刻将整个序列表重写回磁盘；由于表只有十几行，
 *    写入代价可视为常数（远小于重写挂号全表）。
 * 4. 首次运行（文件不存在）时，从现有数据文件扫描最大序列号作为种子，
 *    避免迁移后旧 ID 与新 ID 冲突。
 * 5. 文件权限由底层 TextFileRepository_save_file 通过 0600 mode 统一收紧。
 *
 * 线程安全：单线程假设（与 HIS 项目其余 Service 一致），不做加锁。
 */

#ifndef HIS_SERVICE_SEQUENCE_SERVICE_H
#define HIS_SERVICE_SEQUENCE_SERVICE_H

#include <stddef.h>

#include "common/Result.h"
#include "service/AuditService.h" /* 复用 HIS_FILE_PATH_CAPACITY 常量 */

/** 序列号类型名最大长度（含终止符） */
#define HIS_SEQUENCE_TYPE_CAPACITY 32

/** 序列号 ID 前缀最大长度（含终止符），如 "REG" / "PAT" */
#define HIS_SEQUENCE_PREFIX_CAPACITY 8

/** 内存中可同时管理的序列号类型最大数量 */
#define HIS_SEQUENCE_MAX_TYPES 16

/**
 * @brief 序列号类型规格
 *
 * 描述一个 ID 类型及其对应的数据文件，用于首次初始化时扫描最大序列号。
 * 若 data_path 为空则跳过扫描（直接以 0 作为起点）。
 */
typedef struct SequenceTypeSpec {
    char type_name[HIS_SEQUENCE_TYPE_CAPACITY]; /**< 类型名，如 "REGISTRATION" */
    char id_prefix[HIS_SEQUENCE_PREFIX_CAPACITY]; /**< ID 前缀，如 "REG" */
    char data_path[HIS_FILE_PATH_CAPACITY];     /**< 扫描源文件路径，可为空 */
} SequenceTypeSpec;

/**
 * @brief 单个类型的运行时条目（内部使用，暴露给调用方仅为了方便嵌入）
 */
typedef struct SequenceEntry {
    char type_name[HIS_SEQUENCE_TYPE_CAPACITY];
    int last_seq;
} SequenceEntry;

/**
 * @brief 序列号服务主结构体
 */
typedef struct SequenceService {
    char file_path[HIS_FILE_PATH_CAPACITY];        /**< sequences.txt 路径 */
    SequenceEntry entries[HIS_SEQUENCE_MAX_TYPES]; /**< 内存缓存条目 */
    size_t entry_count;                             /**< 当前有效条目数 */
    int loaded;                                     /**< 是否已从磁盘载入 */
} SequenceService;

/**
 * @brief 初始化序列号服务
 *
 * 行为：
 * - 若 file_path 指向的文件存在且格式合法，将其全部加载入内存。
 * - 否则：为传入的每个 spec 扫描 data_path 文件中最大序列号作为种子，
 *   再把整张表写入 file_path，完成一次性迁移。
 * - spec_count 可以为 0，此时即使文件不存在也只是创建一个空的 sequences.txt；
 *   后续 next() 对未知类型会失败。
 *
 * @param self       指向待初始化的服务
 * @param file_path  sequences.txt 的完整路径（不可为空）
 * @param specs      类型规格数组指针，可为 NULL 当 spec_count==0
 * @param spec_count 规格数量，不能超过 HIS_SEQUENCE_MAX_TYPES
 * @return Result    操作结果
 */
Result SequenceService_init(
    SequenceService *self,
    const char *file_path,
    const SequenceTypeSpec *specs,
    size_t spec_count
);

/**
 * @brief 原子分配下一个序列号
 *
 * 先将指定类型的内存计数 +1，再把整个 sequences.txt 重写（save-all），
 * 然后返回新序号给调用方。若类型名不存在于服务中，返回 failure。
 *
 * @param self       序列号服务
 * @param type_name  类型名（必须与 init 时注册的 spec.type_name 一致）
 * @param out_seq    输出参数：分配到的新序号（>=1）
 * @return Result    操作结果
 */
Result SequenceService_next(
    SequenceService *self,
    const char *type_name,
    int *out_seq
);

/**
 * @brief 仅供测试用：查询当前内存中指定类型的最后分配序号
 *
 * @param self      服务实例
 * @param type_name 类型名
 * @param out_seq   输出参数
 * @return Result   若类型不存在返回 failure
 */
Result SequenceService_peek(
    const SequenceService *self,
    const char *type_name,
    int *out_seq
);

#endif
