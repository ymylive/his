/**
 * @file PatientRepository.h
 * @brief 患者数据仓储层头文件
 *
 * 提供患者（Patient）数据的持久化存储功能，基于文本文件存储。
 * 支持患者信息的增加、按ID查找、全量加载和全量保存操作。
 *
 * 数据文件格式：每行一条患者记录，字段以管道符 '|' 分隔。
 * 表头：patient_id|name|gender|age|contact|id_card|allergy|medical_history|is_inpatient|remarks
 */

#ifndef HIS_REPOSITORY_PATIENT_REPOSITORY_H
#define HIS_REPOSITORY_PATIENT_REPOSITORY_H

#include "common/LinkedList.h"
#include "common/Result.h"
#include "domain/Patient.h"
#include "repository/TextFileRepository.h"

/** 患者数据文件的表头行 */
#define PATIENT_REPOSITORY_HEADER \
    "patient_id|name|gender|age|contact|id_card|allergy|medical_history|is_inpatient|remarks"

/** 患者记录的字段数量 */
#define PATIENT_REPOSITORY_FIELD_COUNT 10

/**
 * @brief 患者仓储结构体
 *
 * 封装底层的文本文件仓储，提供患者数据的持久化操作。
 */
typedef struct PatientRepository {
    TextFileRepository file_repository; /**< 底层文本文件仓储 */
} PatientRepository;

/**
 * @brief 初始化患者仓储
 *
 * 初始化底层文件仓储，并确保文件中包含正确的表头。
 *
 * @param repository 患者仓储实例指针
 * @param path       数据文件路径
 * @return 成功返回 success；参数无效或文件操作失败时返回 failure
 */
Result PatientRepository_init(PatientRepository *repository, const char *path);

/**
 * @brief 追加一条患者记录
 *
 * 将患者信息追加到文件末尾。会先检查是否已存在相同ID的患者。
 *
 * @param repository 患者仓储实例指针
 * @param patient    要追加的患者数据
 * @return 成功返回 success；ID重复或写入失败时返回 failure
 */
Result PatientRepository_append(const PatientRepository *repository, const Patient *patient);

/**
 * @brief 按患者ID查找患者
 * @param repository  患者仓储实例指针
 * @param patient_id  要查找的患者ID
 * @param out_patient 输出参数，存放找到的患者数据
 * @return 找到返回 success；未找到或参数无效时返回 failure
 */
Result PatientRepository_find_by_id(
    const PatientRepository *repository,
    const char *patient_id,
    Patient *out_patient
);

/**
 * @brief 加载所有患者记录到链表
 * @param repository   患者仓储实例指针
 * @param out_patients 输出参数，存放患者数据的链表（链表中的元素需通过 clear 函数释放）
 * @return 成功返回 success；加载失败时返回 failure 并清空链表
 */
Result PatientRepository_load_all(
    const PatientRepository *repository,
    LinkedList *out_patients
);

/**
 * @brief 全量保存患者列表到文件
 *
 * 用传入的患者列表覆盖整个数据文件。会先验证列表中的数据合法性和ID唯一性。
 *
 * @param repository 患者仓储实例指针
 * @param patients   要保存的患者链表
 * @return 成功返回 success；数据无效或写入失败时返回 failure
 */
Result PatientRepository_save_all(
    const PatientRepository *repository,
    const LinkedList *patients
);

/**
 * @brief 清空并释放患者链表中的所有元素
 * @param patients 要清空的患者链表
 */
void PatientRepository_clear_loaded_list(LinkedList *patients);

#endif
