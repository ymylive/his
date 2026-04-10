/**
 * @file DispenseRecordRepository.h
 * @brief 发药记录数据仓储层头文件
 *
 * 提供发药记录（DispenseRecord）数据的持久化存储功能，基于文本文件存储。
 * 支持发药记录的追加、按发药ID查找、按处方ID筛选、按患者ID筛选、
 * 全量加载和全量保存操作。
 *
 * 数据文件格式：每行一条发药记录，字段以管道符 '|' 分隔。
 * 表头：dispense_id|patient_id|prescription_id|medicine_id|quantity|pharmacist_id|dispensed_at|status
 */

#ifndef HIS_REPOSITORY_DISPENSE_RECORD_REPOSITORY_H
#define HIS_REPOSITORY_DISPENSE_RECORD_REPOSITORY_H

#include "common/LinkedList.h"
#include "common/Result.h"
#include "domain/DispenseRecord.h"
#include "repository/TextFileRepository.h"

/** 发药记录的字段数量 */
#define DISPENSE_RECORD_REPOSITORY_FIELD_COUNT 8

/**
 * @brief 发药记录仓储结构体
 */
typedef struct DispenseRecordRepository {
    TextFileRepository storage; /**< 底层文本文件仓储 */
} DispenseRecordRepository;

/**
 * @brief 初始化发药记录仓储
 * @param repository 仓储实例指针
 * @param path       数据文件路径
 * @return 成功返回 success；参数无效时返回 failure
 */
Result DispenseRecordRepository_init(DispenseRecordRepository *repository, const char *path);

/**
 * @brief 确保存储文件已就绪（含表头校验/写入）
 * @param repository 仓储实例指针
 * @return 成功返回 success；文件操作失败或表头不匹配时返回 failure
 */
Result DispenseRecordRepository_ensure_storage(const DispenseRecordRepository *repository);

/**
 * @brief 追加一条发药记录到文件
 * @param repository 仓储实例指针
 * @param record     要追加的发药记录
 * @return 成功返回 success；数据无效或写入失败时返回 failure
 */
Result DispenseRecordRepository_append(
    const DispenseRecordRepository *repository,
    const DispenseRecord *record
);

/**
 * @brief 按发药记录ID查找
 * @param repository 仓储实例指针
 * @param dispense_id 要查找的发药记录ID
 * @param out_record  输出参数，存放找到的发药记录
 * @return 找到返回 success；未找到时返回 failure
 */
Result DispenseRecordRepository_find_by_dispense_id(
    const DispenseRecordRepository *repository,
    const char *dispense_id,
    DispenseRecord *out_record
);

/**
 * @brief 按处方ID查找关联的发药记录列表
 * @param repository      仓储实例指针
 * @param prescription_id 处方ID
 * @param out_records     输出参数，存放匹配的发药记录链表（必须为空链表）
 * @return 成功返回 success；参数无效或加载失败时返回 failure
 */
Result DispenseRecordRepository_find_by_prescription_id(
    const DispenseRecordRepository *repository,
    const char *prescription_id,
    LinkedList *out_records
);

/**
 * @brief 按患者ID查找关联的发药记录列表
 * @param repository  仓储实例指针
 * @param patient_id  患者ID
 * @param out_records 输出参数，存放匹配的发药记录链表（必须为空链表）
 * @return 成功返回 success；参数无效或加载失败时返回 failure
 */
Result DispenseRecordRepository_find_by_patient_id(
    const DispenseRecordRepository *repository,
    const char *patient_id,
    LinkedList *out_records
);

/**
 * @brief 加载所有发药记录到链表
 * @param repository  仓储实例指针
 * @param out_records 输出参数，存放发药记录的链表（必须为空链表）
 * @return 成功返回 success；加载失败时返回 failure
 */
Result DispenseRecordRepository_load_all(
    const DispenseRecordRepository *repository,
    LinkedList *out_records
);

/**
 * @brief 全量保存发药记录列表到文件（覆盖写入）
 * @param repository 仓储实例指针
 * @param records    要保存的发药记录链表
 * @return 成功返回 success；数据无效或写入失败时返回 failure
 */
Result DispenseRecordRepository_save_all(
    const DispenseRecordRepository *repository,
    const LinkedList *records
);

/**
 * @brief 清空并释放发药记录链表中的所有元素
 * @param records 要清空的发药记录链表
 */
void DispenseRecordRepository_clear_list(LinkedList *records);

#endif
