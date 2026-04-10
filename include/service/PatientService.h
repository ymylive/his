/**
 * @file PatientService.h
 * @brief 患者服务模块头文件
 *
 * 提供患者信息的增删改查业务逻辑，包括按ID、姓名、联系方式、
 * 身份证号等多种方式查找患者。该服务依赖患者仓库（PatientRepository）
 * 进行数据持久化。
 */

#ifndef HIS_SERVICE_PATIENT_SERVICE_H
#define HIS_SERVICE_PATIENT_SERVICE_H

#include "common/LinkedList.h"
#include "common/Result.h"
#include "domain/Patient.h"
#include "repository/PatientRepository.h"

/**
 * @brief 患者服务结构体
 *
 * 封装患者数据仓库，提供统一的患者管理接口。
 */
typedef struct PatientService {
    PatientRepository repository;  /* 患者数据仓库 */
} PatientService;

/**
 * @brief 初始化患者服务
 *
 * @param service  指向待初始化的患者服务结构体
 * @param path     患者数据文件路径
 * @return Result  操作结果，success=1 表示成功
 */
Result PatientService_init(PatientService *service, const char *path);

/**
 * @brief 创建新患者记录
 *
 * 验证患者信息的合法性，确保患者ID、联系方式和身份证号唯一后追加到存储中。
 *
 * @param service  指向患者服务结构体
 * @param patient  指向待创建的患者信息
 * @return Result  操作结果，success=1 表示创建成功
 */
Result PatientService_create_patient(PatientService *service, const Patient *patient);

/**
 * @brief 更新患者记录
 *
 * 根据患者ID查找已有记录并更新，同时确保联系方式和身份证号不与其他患者冲突。
 *
 * @param service  指向患者服务结构体
 * @param patient  指向包含更新后信息的患者结构体（patient_id 用于定位记录）
 * @return Result  操作结果，success=1 表示更新成功
 */
Result PatientService_update_patient(PatientService *service, const Patient *patient);

/**
 * @brief 删除患者记录
 *
 * 根据患者ID从存储中删除对应的患者记录。
 *
 * @param service     指向患者服务结构体
 * @param patient_id  待删除的患者ID
 * @return Result     操作结果，success=1 表示删除成功
 */
Result PatientService_delete_patient(PatientService *service, const char *patient_id);

/**
 * @brief 根据患者ID查找患者
 *
 * @param service      指向患者服务结构体
 * @param patient_id   待查找的患者ID
 * @param out_patient  输出参数，查找成功时存放患者信息
 * @return Result      操作结果，success=1 表示找到患者
 */
Result PatientService_find_patient_by_id(
    PatientService *service,
    const char *patient_id,
    Patient *out_patient
);

/**
 * @brief 根据姓名查找患者（可能返回多条记录）
 *
 * @param service       指向患者服务结构体
 * @param name          待查找的患者姓名
 * @param out_patients  输出参数，查找结果链表（每个节点为 Patient*）
 * @return Result       操作结果，success=1 表示查找完成
 */
Result PatientService_find_patients_by_name(
    PatientService *service,
    const char *name,
    LinkedList *out_patients
);

/**
 * @brief 根据联系方式查找患者
 *
 * @param service      指向患者服务结构体
 * @param contact      联系方式（6-20位数字）
 * @param out_patient  输出参数，查找成功时存放患者信息
 * @return Result      操作结果，success=1 表示找到唯一匹配的患者
 */
Result PatientService_find_patient_by_contact(
    PatientService *service,
    const char *contact,
    Patient *out_patient
);

/**
 * @brief 根据身份证号查找患者
 *
 * @param service      指向患者服务结构体
 * @param id_card      身份证号（15位或18位）
 * @param out_patient  输出参数，查找成功时存放患者信息
 * @return Result      操作结果，success=1 表示找到唯一匹配的患者
 */
Result PatientService_find_patient_by_id_card(
    PatientService *service,
    const char *id_card,
    Patient *out_patient
);

/**
 * @brief 清理按姓名查找返回的搜索结果链表
 *
 * 释放链表中所有动态分配的 Patient 节点。
 *
 * @param patients  指向待清理的患者链表
 */
void PatientService_clear_search_results(LinkedList *patients);

#endif
