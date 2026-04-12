/**
 * @file PrescriptionRepository.h
 * @brief 处方数据仓储层头文件
 *
 * 提供处方（Prescription）数据的持久化存储功能，基于文本文件存储。
 * 支持处方的追加、按处方ID查找、按就诊ID筛选、
 * 全量加载和全量保存操作。
 *
 * 数据文件格式：每行一条处方记录，字段以管道符 '|' 分隔。
 * 表头：prescription_id|visit_id|medicine_id|quantity|usage
 */

#ifndef HIS_REPOSITORY_PRESCRIPTION_REPOSITORY_H
#define HIS_REPOSITORY_PRESCRIPTION_REPOSITORY_H

#include "common/LinkedList.h"
#include "common/Result.h"
#include "domain/Prescription.h"
#include "repository/TextFileRepository.h"

/** 处方记录的字段数量 */
#define PRESCRIPTION_REPOSITORY_FIELD_COUNT 5

/**
 * @brief 处方仓储结构体
 */
typedef struct PrescriptionRepository {
    TextFileRepository storage; /**< 底层文本文件仓储 */
} PrescriptionRepository;

/**
 * @brief 初始化处方仓储
 * @param repository 仓储实例指针
 * @param path       数据文件路径
 * @return 成功返回 success；参数无效时返回 failure
 */
Result PrescriptionRepository_init(PrescriptionRepository *repository, const char *path);

/**
 * @brief 确保存储文件已就绪（含表头校验/写入）
 * @param repository 仓储实例指针
 * @return 成功返回 success；文件操作失败或表头不匹配时返回 failure
 */
Result PrescriptionRepository_ensure_storage(const PrescriptionRepository *repository);

/**
 * @brief 追加一条处方记录到文件
 * @param repository   仓储实例指针
 * @param prescription 要追加的处方记录
 * @return 成功返回 success；数据无效或写入失败时返回 failure
 */
Result PrescriptionRepository_append(
    const PrescriptionRepository *repository,
    const Prescription *prescription
);

/**
 * @brief 按处方ID查找
 * @param repository      仓储实例指针
 * @param prescription_id 要查找的处方ID
 * @param out_prescription 输出参数，存放找到的处方记录
 * @return 找到返回 success；未找到时返回 failure
 */
Result PrescriptionRepository_find_by_id(
    const PrescriptionRepository *repository,
    const char *prescription_id,
    Prescription *out_prescription
);

/**
 * @brief 按就诊ID查找关联的处方列表
 * @param repository       仓储实例指针
 * @param visit_id         就诊ID
 * @param out_prescriptions 输出参数，存放匹配的处方链表（必须为空链表）
 * @return 成功返回 success；参数无效或加载失败时返回 failure
 */
Result PrescriptionRepository_find_by_visit_id(
    const PrescriptionRepository *repository,
    const char *visit_id,
    LinkedList *out_prescriptions
);

/**
 * @brief 加载所有处方记录到链表
 * @param repository       仓储实例指针
 * @param out_prescriptions 输出参数，存放处方记录的链表（必须为空链表）
 * @return 成功返回 success；加载失败时返回 failure
 */
Result PrescriptionRepository_load_all(
    const PrescriptionRepository *repository,
    LinkedList *out_prescriptions
);

/**
 * @brief 全量保存处方列表到文件（覆盖写入）
 * @param repository    仓储实例指针
 * @param prescriptions 要保存的处方链表
 * @return 成功返回 success；数据无效或写入失败时返回 failure
 */
Result PrescriptionRepository_save_all(
    const PrescriptionRepository *repository,
    const LinkedList *prescriptions
);

/**
 * @brief 清空并释放处方链表中的所有元素
 * @param prescriptions 要清空的处方链表
 */
void PrescriptionRepository_clear_list(LinkedList *prescriptions);

#endif
