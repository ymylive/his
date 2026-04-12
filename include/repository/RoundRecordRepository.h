/**
 * @file RoundRecordRepository.h
 * @brief 查房记录数据仓储层头文件
 *
 * 提供查房记录（RoundRecord）数据的持久化存储功能，基于文本文件存储。
 * 支持查房记录的追加、按住院记录ID筛选、全量加载和全量保存操作。
 *
 * 数据文件格式：每行一条查房记录，字段以管道符 '|' 分隔。
 * 表头：round_id|admission_id|doctor_id|findings|plan|rounded_at
 */

#ifndef HIS_REPOSITORY_ROUND_RECORD_REPOSITORY_H
#define HIS_REPOSITORY_ROUND_RECORD_REPOSITORY_H

#include "common/LinkedList.h"
#include "common/Result.h"
#include "domain/RoundRecord.h"
#include "repository/TextFileRepository.h"

/** 查房记录的字段数量 */
#define ROUND_RECORD_REPOSITORY_FIELD_COUNT 6

/**
 * @brief 查房记录仓储结构体
 */
typedef struct RoundRecordRepository {
    TextFileRepository storage; /**< 底层文本文件仓储 */
} RoundRecordRepository;

/**
 * @brief 初始化查房记录仓储
 * @param repository 仓储实例指针
 * @param path       数据文件路径
 * @return 成功返回 success；参数无效时返回 failure
 */
Result RoundRecordRepository_init(RoundRecordRepository *repository, const char *path);

/**
 * @brief 确保存储文件已就绪（含表头校验/写入）
 * @param repository 仓储实例指针
 * @return 成功返回 success；文件操作失败或表头不匹配时返回 failure
 */
Result RoundRecordRepository_ensure_storage(const RoundRecordRepository *repository);

/**
 * @brief 追加一条查房记录到文件
 * @param repository 仓储实例指针
 * @param record     要追加的查房记录
 * @return 成功返回 success；数据无效或写入失败时返回 failure
 */
Result RoundRecordRepository_append(
    const RoundRecordRepository *repository,
    const RoundRecord *record
);

/**
 * @brief 按住院记录ID查找关联的查房记录列表
 * @param repository   仓储实例指针
 * @param admission_id 住院记录ID
 * @param out_records  输出参数，存放匹配的查房记录链表（必须为空链表）
 * @return 成功返回 success；参数无效或加载失败时返回 failure
 */
Result RoundRecordRepository_find_by_admission_id(
    const RoundRecordRepository *repository,
    const char *admission_id,
    LinkedList *out_records
);

/**
 * @brief 加载所有查房记录到链表
 * @param repository  仓储实例指针
 * @param out_records 输出参数，存放查房记录的链表（必须为空链表）
 * @return 成功返回 success；加载失败时返回 failure
 */
Result RoundRecordRepository_load_all(
    const RoundRecordRepository *repository,
    LinkedList *out_records
);

/**
 * @brief 全量保存查房记录列表到文件（覆盖写入）
 * @param repository 仓储实例指针
 * @param records    要保存的查房记录链表
 * @return 成功返回 success；数据无效或写入失败时返回 failure
 */
Result RoundRecordRepository_save_all(
    const RoundRecordRepository *repository,
    const LinkedList *records
);

/**
 * @brief 清空并释放查房记录链表中的所有元素
 * @param records 要清空的查房记录链表
 */
void RoundRecordRepository_clear_list(LinkedList *records);

#endif
