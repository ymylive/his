/**
 * @file NursingRecordRepository.h
 * @brief 护理记录数据仓储层头文件
 *
 * 提供护理记录（NursingRecord）数据的持久化存储功能，基于文本文件存储。
 * 支持护理记录的追加、按住院记录ID筛选、全量加载和全量保存操作。
 *
 * 数据文件格式：每行一条护理记录，字段以管道符 '|' 分隔。
 * 表头：nursing_id|admission_id|nurse_name|record_type|content|recorded_at
 */

#ifndef HIS_REPOSITORY_NURSING_RECORD_REPOSITORY_H
#define HIS_REPOSITORY_NURSING_RECORD_REPOSITORY_H

#include "common/LinkedList.h"
#include "common/Result.h"
#include "domain/NursingRecord.h"
#include "repository/TextFileRepository.h"

/** 护理记录的字段数量 */
#define NURSING_RECORD_REPOSITORY_FIELD_COUNT 6

/**
 * @brief 护理记录仓储结构体
 */
typedef struct NursingRecordRepository {
    TextFileRepository storage; /**< 底层文本文件仓储 */
} NursingRecordRepository;

/**
 * @brief 初始化护理记录仓储
 * @param repository 仓储实例指针
 * @param path       数据文件路径
 * @return 成功返回 success；参数无效时返回 failure
 */
Result NursingRecordRepository_init(NursingRecordRepository *repository, const char *path);

/**
 * @brief 确保存储文件已就绪（含表头校验/写入）
 * @param repository 仓储实例指针
 * @return 成功返回 success；文件操作失败或表头不匹配时返回 failure
 */
Result NursingRecordRepository_ensure_storage(const NursingRecordRepository *repository);

/**
 * @brief 追加一条护理记录到文件
 * @param repository 仓储实例指针
 * @param record     要追加的护理记录
 * @return 成功返回 success；数据无效或写入失败时返回 failure
 */
Result NursingRecordRepository_append(
    const NursingRecordRepository *repository,
    const NursingRecord *record
);

/**
 * @brief 按住院记录ID查找关联的护理记录列表
 * @param repository   仓储实例指针
 * @param admission_id 住院记录ID
 * @param out_records  输出参数，存放匹配的护理记录链表（必须为空链表）
 * @return 成功返回 success；参数无效或加载失败时返回 failure
 */
Result NursingRecordRepository_find_by_admission_id(
    const NursingRecordRepository *repository,
    const char *admission_id,
    LinkedList *out_records
);

/**
 * @brief 加载所有护理记录到链表
 * @param repository  仓储实例指针
 * @param out_records 输出参数，存放护理记录的链表（必须为空链表）
 * @return 成功返回 success；加载失败时返回 failure
 */
Result NursingRecordRepository_load_all(
    const NursingRecordRepository *repository,
    LinkedList *out_records
);

/**
 * @brief 全量保存护理记录列表到文件（覆盖写入）
 * @param repository 仓储实例指针
 * @param records    要保存的护理记录链表
 * @return 成功返回 success；数据无效或写入失败时返回 failure
 */
Result NursingRecordRepository_save_all(
    const NursingRecordRepository *repository,
    const LinkedList *records
);

/**
 * @brief 清空并释放护理记录链表中的所有元素
 * @param records 要清空的护理记录链表
 */
void NursingRecordRepository_clear_list(LinkedList *records);

#endif
