/**
 * @file DepartmentRepository.h
 * @brief 科室数据仓储层头文件
 *
 * 提供科室（Department）数据的持久化存储功能，基于文本文件存储。
 * 支持科室信息的加载、按ID查找、追加保存和全量保存操作。
 *
 * 数据文件格式：每行一条科室记录，字段以管道符 '|' 分隔。
 * 表头：department_id|name|location|description
 */

#ifndef HIS_REPOSITORY_DEPARTMENT_REPOSITORY_H
#define HIS_REPOSITORY_DEPARTMENT_REPOSITORY_H

#include "common/LinkedList.h"
#include "common/Result.h"
#include "domain/Department.h"
#include "repository/TextFileRepository.h"

/**
 * @brief 科室仓储结构体
 */
typedef struct DepartmentRepository {
    TextFileRepository storage; /**< 底层文本文件仓储 */
} DepartmentRepository;

/**
 * @brief 初始化科室仓储
 * @param repository 科室仓储实例指针
 * @param path       数据文件路径
 * @return 成功返回 success；参数无效或文件操作失败时返回 failure
 */
Result DepartmentRepository_init(DepartmentRepository *repository, const char *path);

/**
 * @brief 加载所有科室记录到链表
 * @param repository      科室仓储实例指针
 * @param out_departments 输出参数，存放科室数据的链表
 * @return 成功返回 success；加载失败时返回 failure
 */
Result DepartmentRepository_load_all(
    DepartmentRepository *repository,
    LinkedList *out_departments
);

/**
 * @brief 按科室ID查找科室
 * @param repository     科室仓储实例指针
 * @param department_id  要查找的科室ID
 * @param out_department 输出参数，存放找到的科室数据
 * @return 找到返回 success；未找到时返回 failure
 */
Result DepartmentRepository_find_by_department_id(
    DepartmentRepository *repository,
    const char *department_id,
    Department *out_department
);

/**
 * @brief 追加保存一条科室记录到文件末尾
 * @param repository 科室仓储实例指针
 * @param department 要保存的科室数据
 * @return 成功返回 success；数据无效或写入失败时返回 failure
 */
Result DepartmentRepository_save(
    DepartmentRepository *repository,
    const Department *department
);

/**
 * @brief 全量保存科室列表到文件（覆盖写入）
 * @param repository  科室仓储实例指针
 * @param departments 要保存的科室链表
 * @return 成功返回 success；数据无效或写入失败时返回 failure
 */
Result DepartmentRepository_save_all(
    DepartmentRepository *repository,
    const LinkedList *departments
);

/**
 * @brief 清空并释放科室链表中的所有元素
 * @param departments 要清空的科室链表
 */
void DepartmentRepository_clear_list(LinkedList *departments);

#endif
