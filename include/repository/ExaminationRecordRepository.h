/**
 * @file ExaminationRecordRepository.h
 * @brief 检查记录数据仓储层头文件
 *
 * 提供检查记录（ExaminationRecord）数据的持久化存储功能，基于文本文件存储。
 * 支持检查记录的追加、按检查ID查找、按就诊ID筛选、全量加载和全量保存操作。
 *
 * 数据文件格式：每行一条检查记录，字段以管道符 '|' 分隔，共11个字段。
 * 表头：examination_id|visit_id|patient_id|doctor_id|exam_item|exam_type|
 *       status|result|exam_fee|requested_at|completed_at
 */

#ifndef HIS_REPOSITORY_EXAMINATION_RECORD_REPOSITORY_H
#define HIS_REPOSITORY_EXAMINATION_RECORD_REPOSITORY_H

#include "common/LinkedList.h"
#include "common/Result.h"
#include "domain/ExaminationRecord.h"
#include "repository/TextFileRepository.h"

/** 检查记录的字段数量 */
#define EXAMINATION_RECORD_REPOSITORY_FIELD_COUNT 11

/**
 * @brief 检查记录仓储结构体
 */
typedef struct ExaminationRecordRepository {
    TextFileRepository storage; /**< 底层文本文件仓储 */
} ExaminationRecordRepository;

/**
 * @brief 初始化检查记录仓储
 * @param repository 仓储实例指针
 * @param path       数据文件路径
 * @return 成功返回 success；参数无效时返回 failure
 */
Result ExaminationRecordRepository_init(
    ExaminationRecordRepository *repository,
    const char *path
);

/**
 * @brief 确保存储文件已就绪（含表头校验/写入）
 * @param repository 仓储实例指针
 * @return 成功返回 success；文件操作失败或表头不匹配时返回 failure
 */
Result ExaminationRecordRepository_ensure_storage(
    const ExaminationRecordRepository *repository
);

/**
 * @brief 追加一条检查记录到文件
 * @param repository 仓储实例指针
 * @param record     要追加的检查记录
 * @return 成功返回 success；数据无效或写入失败时返回 failure
 */
Result ExaminationRecordRepository_append(
    const ExaminationRecordRepository *repository,
    const ExaminationRecord *record
);

/**
 * @brief 按检查ID查找检查记录
 * @param repository     仓储实例指针
 * @param examination_id 要查找的检查ID
 * @param out_record     输出参数，存放找到的检查记录
 * @return 找到返回 success；未找到时返回 failure
 */
Result ExaminationRecordRepository_find_by_examination_id(
    const ExaminationRecordRepository *repository,
    const char *examination_id,
    ExaminationRecord *out_record
);

/**
 * @brief 按就诊ID查找关联的检查记录列表
 * @param repository  仓储实例指针
 * @param visit_id    就诊ID
 * @param out_records 输出参数，存放匹配的检查记录链表（必须为空链表）
 * @return 成功返回 success；参数无效或加载失败时返回 failure
 */
Result ExaminationRecordRepository_find_by_visit_id(
    const ExaminationRecordRepository *repository,
    const char *visit_id,
    LinkedList *out_records
);

/**
 * @brief 加载所有检查记录到链表
 * @param repository  仓储实例指针
 * @param out_records 输出参数，存放检查记录的链表（必须为空链表）
 * @return 成功返回 success；加载失败时返回 failure
 */
Result ExaminationRecordRepository_load_all(
    const ExaminationRecordRepository *repository,
    LinkedList *out_records
);

/**
 * @brief 全量保存检查记录列表到文件（覆盖写入）
 * @param repository 仓储实例指针
 * @param records    要保存的检查记录链表
 * @return 成功返回 success；数据无效或写入失败时返回 failure
 */
Result ExaminationRecordRepository_save_all(
    const ExaminationRecordRepository *repository,
    const LinkedList *records
);

/**
 * @brief 清空并释放检查记录链表中的所有元素
 * @param records 要清空的检查记录链表
 */
void ExaminationRecordRepository_clear_list(LinkedList *records);

#endif
