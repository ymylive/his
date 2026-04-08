/**
 * @file AdmissionRepository.h
 * @brief 入院记录数据仓储层头文件
 *
 * 提供入院记录（Admission）数据的持久化存储功能，基于文本文件存储。
 * 支持入院记录的追加、按ID查找、按患者ID/床位ID查找活跃入院、
 * 全量加载和全量保存操作。
 *
 * 数据文件格式：每行一条入院记录，字段以管道符 '|' 分隔。
 * 表头：admission_id|patient_id|ward_id|bed_id|admitted_at|status|discharged_at|summary
 */

#ifndef HIS_REPOSITORY_ADMISSION_REPOSITORY_H
#define HIS_REPOSITORY_ADMISSION_REPOSITORY_H

#include "common/LinkedList.h"
#include "common/Result.h"
#include "domain/Admission.h"
#include "repository/TextFileRepository.h"

/** 入院记录数据文件的表头行 */
#define ADMISSION_REPOSITORY_HEADER \
    "admission_id|patient_id|ward_id|bed_id|admitted_at|status|discharged_at|summary"

/** 入院记录的字段数量 */
#define ADMISSION_REPOSITORY_FIELD_COUNT 8

/**
 * @brief 入院记录仓储结构体
 */
typedef struct AdmissionRepository {
    TextFileRepository storage; /**< 底层文本文件仓储 */
} AdmissionRepository;

/**
 * @brief 初始化入院记录仓储
 * @param repository 仓储实例指针
 * @param path       数据文件路径
 * @return 成功返回 success；参数无效或文件操作失败时返回 failure
 */
Result AdmissionRepository_init(AdmissionRepository *repository, const char *path);

/**
 * @brief 追加一条入院记录
 *
 * 会先加载所有记录并检查：ID唯一性、同一患者不能有多条活跃入院、
 * 同一床位不能有多条活跃入院。
 *
 * @param repository 仓储实例指针
 * @param admission  要追加的入院记录
 * @return 成功返回 success；冲突或写入失败时返回 failure
 */
Result AdmissionRepository_append(
    const AdmissionRepository *repository,
    const Admission *admission
);

/**
 * @brief 按入院记录ID查找
 * @param repository    仓储实例指针
 * @param admission_id  要查找的入院ID
 * @param out_admission 输出参数，存放找到的入院记录
 * @return 找到返回 success；未找到时返回 failure
 */
Result AdmissionRepository_find_by_id(
    const AdmissionRepository *repository,
    const char *admission_id,
    Admission *out_admission
);

/**
 * @brief 按患者ID查找活跃状态的入院记录
 * @param repository    仓储实例指针
 * @param patient_id    患者ID
 * @param out_admission 输出参数，存放找到的活跃入院记录
 * @return 找到返回 success；未找到时返回 failure
 */
Result AdmissionRepository_find_active_by_patient_id(
    const AdmissionRepository *repository,
    const char *patient_id,
    Admission *out_admission
);

/**
 * @brief 按床位ID查找活跃状态的入院记录
 * @param repository    仓储实例指针
 * @param bed_id        床位ID
 * @param out_admission 输出参数，存放找到的活跃入院记录
 * @return 找到返回 success；未找到时返回 failure
 */
Result AdmissionRepository_find_active_by_bed_id(
    const AdmissionRepository *repository,
    const char *bed_id,
    Admission *out_admission
);

/**
 * @brief 加载所有入院记录到链表
 * @param repository     仓储实例指针
 * @param out_admissions 输出参数，存放入院记录的链表
 * @return 成功返回 success；加载失败时返回 failure
 */
Result AdmissionRepository_load_all(
    const AdmissionRepository *repository,
    LinkedList *out_admissions
);

/**
 * @brief 全量保存入院记录列表到文件（覆盖写入）
 * @param repository 仓储实例指针
 * @param admissions 要保存的入院记录链表
 * @return 成功返回 success；数据无效或写入失败时返回 failure
 */
Result AdmissionRepository_save_all(
    const AdmissionRepository *repository,
    const LinkedList *admissions
);

/**
 * @brief 清空并释放入院记录链表中的所有元素
 * @param admissions 要清空的入院记录链表
 */
void AdmissionRepository_clear_loaded_list(LinkedList *admissions);

#endif
