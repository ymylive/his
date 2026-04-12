/**
 * @file WardRepository.h
 * @brief 病区数据仓储层头文件
 *
 * 提供病区（Ward）数据的持久化存储功能，基于文本文件存储。
 * 支持病区信息的追加、按ID查找、全量加载和全量保存操作。
 *
 * 数据文件格式：每行一条病区记录，字段以管道符 '|' 分隔。
 * 表头：ward_id|name|department_id|location|capacity|occupied_beds|status
 */

#ifndef HIS_REPOSITORY_WARD_REPOSITORY_H
#define HIS_REPOSITORY_WARD_REPOSITORY_H

#include "common/LinkedList.h"
#include "common/Result.h"
#include "domain/Ward.h"
#include "repository/TextFileRepository.h"

/** 病区数据文件的表头行 */
#define WARD_REPOSITORY_HEADER \
    "ward_id|name|department_id|location|capacity|occupied_beds|status|ward_type|daily_fee"

/** 病区记录的字段数量 */
#define WARD_REPOSITORY_FIELD_COUNT 9

/**
 * @brief 病区仓储结构体
 */
typedef struct WardRepository {
    TextFileRepository storage; /**< 底层文本文件仓储 */
} WardRepository;

/**
 * @brief 初始化病区仓储
 * @param repository 病区仓储实例指针
 * @param path       数据文件路径
 * @return 成功返回 success；参数无效或文件操作失败时返回 failure
 */
Result WardRepository_init(WardRepository *repository, const char *path);

/**
 * @brief 追加一条病区记录
 *
 * 会先加载所有现有记录，检查ID唯一性后再追加保存。
 *
 * @param repository 病区仓储实例指针
 * @param ward       要追加的病区数据
 * @return 成功返回 success；ID重复或写入失败时返回 failure
 */
Result WardRepository_append(const WardRepository *repository, const Ward *ward);

/**
 * @brief 按病区ID查找病区
 * @param repository 病区仓储实例指针
 * @param ward_id    要查找的病区ID
 * @param out_ward   输出参数，存放找到的病区数据
 * @return 找到返回 success；未找到时返回 failure
 */
Result WardRepository_find_by_id(
    const WardRepository *repository,
    const char *ward_id,
    Ward *out_ward
);

/**
 * @brief 加载所有病区记录到链表
 * @param repository 病区仓储实例指针
 * @param out_wards  输出参数，存放病区数据的链表
 * @return 成功返回 success；加载失败时返回 failure
 */
Result WardRepository_load_all(
    const WardRepository *repository,
    LinkedList *out_wards
);

/**
 * @brief 全量保存病区列表到文件（覆盖写入）
 * @param repository 病区仓储实例指针
 * @param wards      要保存的病区链表
 * @return 成功返回 success；数据无效或写入失败时返回 failure
 */
Result WardRepository_save_all(
    const WardRepository *repository,
    const LinkedList *wards
);

/**
 * @brief 清空并释放病区链表中的所有元素
 * @param wards 要清空的病区链表
 */
void WardRepository_clear_loaded_list(LinkedList *wards);

#endif
