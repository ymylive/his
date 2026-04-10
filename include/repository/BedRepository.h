/**
 * @file BedRepository.h
 * @brief 床位数据仓储层头文件
 *
 * 提供床位（Bed）数据的持久化存储功能，基于文本文件存储。
 * 支持床位信息的追加、按ID查找、全量加载和全量保存操作。
 *
 * 数据文件格式：每行一条床位记录，字段以管道符 '|' 分隔。
 * 表头：bed_id|ward_id|room_no|bed_no|status|current_admission_id|occupied_at|released_at
 */

#ifndef HIS_REPOSITORY_BED_REPOSITORY_H
#define HIS_REPOSITORY_BED_REPOSITORY_H

#include "common/LinkedList.h"
#include "common/Result.h"
#include "domain/Bed.h"
#include "repository/TextFileRepository.h"

/** 床位数据文件的表头行 */
#define BED_REPOSITORY_HEADER \
    "bed_id|ward_id|room_no|bed_no|status|current_admission_id|occupied_at|released_at"

/** 床位记录的字段数量 */
#define BED_REPOSITORY_FIELD_COUNT 8

/**
 * @brief 床位仓储结构体
 */
typedef struct BedRepository {
    TextFileRepository storage; /**< 底层文本文件仓储 */
} BedRepository;

/**
 * @brief 初始化床位仓储
 * @param repository 床位仓储实例指针
 * @param path       数据文件路径
 * @return 成功返回 success；参数无效或文件操作失败时返回 failure
 */
Result BedRepository_init(BedRepository *repository, const char *path);

/**
 * @brief 追加一条床位记录
 *
 * 会先加载所有现有记录，检查ID唯一性后再追加保存。
 *
 * @param repository 床位仓储实例指针
 * @param bed        要追加的床位数据
 * @return 成功返回 success；ID重复或写入失败时返回 failure
 */
Result BedRepository_append(const BedRepository *repository, const Bed *bed);

/**
 * @brief 按床位ID查找床位
 * @param repository 床位仓储实例指针
 * @param bed_id     要查找的床位ID
 * @param out_bed    输出参数，存放找到的床位数据
 * @return 找到返回 success；未找到时返回 failure
 */
Result BedRepository_find_by_id(
    const BedRepository *repository,
    const char *bed_id,
    Bed *out_bed
);

/**
 * @brief 加载所有床位记录到链表
 * @param repository 床位仓储实例指针
 * @param out_beds   输出参数，存放床位数据的链表
 * @return 成功返回 success；加载失败时返回 failure
 */
Result BedRepository_load_all(
    const BedRepository *repository,
    LinkedList *out_beds
);

/**
 * @brief 全量保存床位列表到文件（覆盖写入）
 * @param repository 床位仓储实例指针
 * @param beds       要保存的床位链表
 * @return 成功返回 success；数据无效或写入失败时返回 failure
 */
Result BedRepository_save_all(
    const BedRepository *repository,
    const LinkedList *beds
);

/**
 * @brief 清空并释放床位链表中的所有元素
 * @param beds 要清空的床位链表
 */
void BedRepository_clear_loaded_list(LinkedList *beds);

#endif
