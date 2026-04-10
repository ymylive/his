/**
 * @file VisitRecordRepository.h
 * @brief 就诊记录数据仓储层头文件
 *
 * 提供就诊记录（VisitRecord）数据的持久化存储功能，基于文本文件存储。
 * 支持就诊记录的追加、按就诊ID查找、按挂号ID筛选、全量加载和全量保存操作。
 *
 * 数据文件格式：每行一条就诊记录，字段以管道符 '|' 分隔，共12个字段。
 * 表头：visit_id|registration_id|patient_id|doctor_id|department_id|
 *       chief_complaint|diagnosis|advice|need_exam|need_admission|need_medicine|visit_time
 */

#ifndef HIS_REPOSITORY_VISIT_RECORD_REPOSITORY_H
#define HIS_REPOSITORY_VISIT_RECORD_REPOSITORY_H

#include "common/LinkedList.h"
#include "common/Result.h"
#include "domain/VisitRecord.h"
#include "repository/TextFileRepository.h"

/** 就诊记录的字段数量 */
#define VISIT_RECORD_REPOSITORY_FIELD_COUNT 12

/**
 * @brief 就诊记录仓储结构体
 */
typedef struct VisitRecordRepository {
    TextFileRepository storage; /**< 底层文本文件仓储 */
} VisitRecordRepository;

/**
 * @brief 初始化就诊记录仓储
 * @param repository 仓储实例指针
 * @param path       数据文件路径
 * @return 成功返回 success；参数无效时返回 failure
 */
Result VisitRecordRepository_init(VisitRecordRepository *repository, const char *path);

/**
 * @brief 确保存储文件已就绪（含表头校验/写入）
 * @param repository 仓储实例指针
 * @return 成功返回 success；文件操作失败或表头不匹配时返回 failure
 */
Result VisitRecordRepository_ensure_storage(const VisitRecordRepository *repository);

/**
 * @brief 追加一条就诊记录到文件
 * @param repository 仓储实例指针
 * @param record     要追加的就诊记录
 * @return 成功返回 success；数据无效或写入失败时返回 failure
 */
Result VisitRecordRepository_append(
    const VisitRecordRepository *repository,
    const VisitRecord *record
);

/**
 * @brief 按就诊ID查找就诊记录
 * @param repository 仓储实例指针
 * @param visit_id   要查找的就诊ID
 * @param out_record 输出参数，存放找到的就诊记录
 * @return 找到返回 success；未找到时返回 failure
 */
Result VisitRecordRepository_find_by_visit_id(
    const VisitRecordRepository *repository,
    const char *visit_id,
    VisitRecord *out_record
);

/**
 * @brief 按挂号ID查找关联的就诊记录列表
 * @param repository      仓储实例指针
 * @param registration_id 挂号ID
 * @param out_records     输出参数，存放匹配的就诊记录链表（必须为空链表）
 * @return 成功返回 success；参数无效或加载失败时返回 failure
 */
Result VisitRecordRepository_find_by_registration_id(
    const VisitRecordRepository *repository,
    const char *registration_id,
    LinkedList *out_records
);

/**
 * @brief 加载所有就诊记录到链表
 * @param repository  仓储实例指针
 * @param out_records 输出参数，存放就诊记录的链表（必须为空链表）
 * @return 成功返回 success；加载失败时返回 failure
 */
Result VisitRecordRepository_load_all(
    const VisitRecordRepository *repository,
    LinkedList *out_records
);

/**
 * @brief 全量保存就诊记录列表到文件（覆盖写入）
 * @param repository 仓储实例指针
 * @param records    要保存的就诊记录链表
 * @return 成功返回 success；数据无效或写入失败时返回 failure
 */
Result VisitRecordRepository_save_all(
    const VisitRecordRepository *repository,
    const LinkedList *records
);

/**
 * @brief 清空并释放就诊记录链表中的所有元素
 * @param records 要清空的就诊记录链表
 */
void VisitRecordRepository_clear_list(LinkedList *records);

#endif
