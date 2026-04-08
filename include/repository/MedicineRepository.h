/**
 * @file MedicineRepository.h
 * @brief 药品数据仓储层头文件
 *
 * 提供药品（Medicine）数据的持久化存储功能，基于文本文件存储。
 * 支持药品信息的加载、按ID查找、追加保存和全量保存操作。
 *
 * 数据文件格式：每行一条药品记录，字段以管道符 '|' 分隔。
 * 表头：medicine_id|name|price|stock|department_id|low_stock_threshold
 */

#ifndef HIS_REPOSITORY_MEDICINE_REPOSITORY_H
#define HIS_REPOSITORY_MEDICINE_REPOSITORY_H

#include "common/LinkedList.h"
#include "common/Result.h"
#include "domain/Medicine.h"
#include "repository/TextFileRepository.h"

/**
 * @brief 药品仓储结构体
 */
typedef struct MedicineRepository {
    TextFileRepository storage; /**< 底层文本文件仓储 */
} MedicineRepository;

/**
 * @brief 初始化药品仓储
 * @param repository 药品仓储实例指针
 * @param path       数据文件路径
 * @return 成功返回 success；参数无效或文件操作失败时返回 failure
 */
Result MedicineRepository_init(MedicineRepository *repository, const char *path);

/**
 * @brief 加载所有药品记录到链表
 * @param repository    药品仓储实例指针
 * @param out_medicines 输出参数，存放药品数据的链表
 * @return 成功返回 success；加载失败时返回 failure
 */
Result MedicineRepository_load_all(
    MedicineRepository *repository,
    LinkedList *out_medicines
);

/**
 * @brief 按药品ID查找药品
 * @param repository   药品仓储实例指针
 * @param medicine_id  要查找的药品ID
 * @param out_medicine 输出参数，存放找到的药品数据
 * @return 找到返回 success；未找到时返回 failure
 */
Result MedicineRepository_find_by_medicine_id(
    MedicineRepository *repository,
    const char *medicine_id,
    Medicine *out_medicine
);

/**
 * @brief 追加保存一条药品记录到文件末尾
 * @param repository 药品仓储实例指针
 * @param medicine   要保存的药品数据
 * @return 成功返回 success；数据无效或写入失败时返回 failure
 */
Result MedicineRepository_save(
    MedicineRepository *repository,
    const Medicine *medicine
);

/**
 * @brief 全量保存药品列表到文件（覆盖写入）
 * @param repository 药品仓储实例指针
 * @param medicines  要保存的药品链表
 * @return 成功返回 success；数据无效或写入失败时返回 failure
 */
Result MedicineRepository_save_all(
    MedicineRepository *repository,
    const LinkedList *medicines
);

/**
 * @brief 清空并释放药品链表中的所有元素
 * @param medicines 要清空的药品链表
 */
void MedicineRepository_clear_list(LinkedList *medicines);

#endif
