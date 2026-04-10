/**
 * @file DoctorRepository.h
 * @brief 医生数据仓储层头文件
 *
 * 提供医生（Doctor）数据的持久化存储功能，基于文本文件存储。
 * 支持医生信息的加载、按ID查找、按科室ID筛选、追加保存和全量保存操作。
 *
 * 数据文件格式：每行一条医生记录，字段以管道符 '|' 分隔。
 * 表头：doctor_id|name|title|department_id|schedule|status
 */

#ifndef HIS_REPOSITORY_DOCTOR_REPOSITORY_H
#define HIS_REPOSITORY_DOCTOR_REPOSITORY_H

#include "common/LinkedList.h"
#include "common/Result.h"
#include "domain/Doctor.h"
#include "repository/TextFileRepository.h"

/**
 * @brief 医生仓储结构体
 */
typedef struct DoctorRepository {
    TextFileRepository storage; /**< 底层文本文件仓储 */
} DoctorRepository;

/**
 * @brief 初始化医生仓储
 * @param repository 医生仓储实例指针
 * @param path       数据文件路径
 * @return 成功返回 success；参数无效或文件操作失败时返回 failure
 */
Result DoctorRepository_init(DoctorRepository *repository, const char *path);

/**
 * @brief 加载所有医生记录到链表
 * @param repository  医生仓储实例指针
 * @param out_doctors 输出参数，存放医生数据的链表
 * @return 成功返回 success；加载失败时返回 failure
 */
Result DoctorRepository_load_all(
    DoctorRepository *repository,
    LinkedList *out_doctors
);

/**
 * @brief 按医生ID查找医生
 * @param repository 医生仓储实例指针
 * @param doctor_id  要查找的医生ID
 * @param out_doctor 输出参数，存放找到的医生数据
 * @return 找到返回 success；未找到时返回 failure
 */
Result DoctorRepository_find_by_doctor_id(
    DoctorRepository *repository,
    const char *doctor_id,
    Doctor *out_doctor
);

/**
 * @brief 按科室ID筛选医生列表
 * @param repository    医生仓储实例指针
 * @param department_id 科室ID
 * @param out_doctors   输出参数，存放匹配的医生链表
 * @return 成功返回 success；参数无效或加载失败时返回 failure
 */
Result DoctorRepository_find_by_department_id(
    DoctorRepository *repository,
    const char *department_id,
    LinkedList *out_doctors
);

/**
 * @brief 追加保存一条医生记录到文件末尾
 * @param repository 医生仓储实例指针
 * @param doctor     要保存的医生数据
 * @return 成功返回 success；数据无效或写入失败时返回 failure
 */
Result DoctorRepository_save(
    DoctorRepository *repository,
    const Doctor *doctor
);

/**
 * @brief 全量保存医生列表到文件（覆盖写入）
 * @param repository 医生仓储实例指针
 * @param doctors    要保存的医生链表
 * @return 成功返回 success；数据无效或写入失败时返回 failure
 */
Result DoctorRepository_save_all(
    DoctorRepository *repository,
    const LinkedList *doctors
);

/**
 * @brief 清空并释放医生链表中的所有元素
 * @param doctors 要清空的医生链表
 */
void DoctorRepository_clear_list(LinkedList *doctors);

#endif
