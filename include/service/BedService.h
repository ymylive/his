/**
 * @file BedService.h
 * @brief 床位服务模块头文件
 *
 * 提供病房和床位信息的查询功能，包括列出所有病房、按病房列出床位、
 * 以及查找某个床位当前住院患者。该服务依赖病房仓库、床位仓库和
 * 住院记录仓库。
 */

#ifndef HIS_SERVICE_BED_SERVICE_H
#define HIS_SERVICE_BED_SERVICE_H

#include <stddef.h>

#include "common/LinkedList.h"
#include "common/Result.h"
#include "domain/Bed.h"
#include "domain/Ward.h"
#include "repository/AdmissionRepository.h"
#include "repository/BedRepository.h"
#include "repository/WardRepository.h"

/**
 * @brief 床位服务结构体
 *
 * 封装病房仓库、床位仓库和住院记录仓库，提供统一的床位管理接口。
 */
typedef struct BedService {
    WardRepository ward_repository;            /* 病房数据仓库 */
    BedRepository bed_repository;              /* 床位数据仓库 */
    AdmissionRepository admission_repository;  /* 住院记录仓库 */
} BedService;

/**
 * @brief 初始化床位服务
 *
 * 分别初始化病房仓库、床位仓库和住院记录仓库。
 *
 * @param service         指向待初始化的床位服务结构体
 * @param ward_path       病房数据文件路径
 * @param bed_path        床位数据文件路径
 * @param admission_path  住院记录数据文件路径
 * @return Result         操作结果，success=1 表示成功
 */
Result BedService_init(
    BedService *service,
    const char *ward_path,
    const char *bed_path,
    const char *admission_path
);

/**
 * @brief 列出所有病房
 *
 * @param service    指向床位服务结构体
 * @param out_wards  输出参数，病房列表链表（每个节点为 Ward*）
 * @return Result    操作结果，success=1 表示加载完成
 */
Result BedService_list_wards(BedService *service, LinkedList *out_wards);

/**
 * @brief 按病房ID列出该病房下的所有床位
 *
 * 先验证病房是否存在，然后筛选出属于该病房的所有床位。
 *
 * @param service   指向床位服务结构体
 * @param ward_id   病房ID
 * @param out_beds  输出参数，床位列表链表（每个节点为 Bed*）
 * @return Result   操作结果，success=1 表示加载完成
 */
Result BedService_list_beds_by_ward(
    BedService *service,
    const char *ward_id,
    LinkedList *out_beds
);

/**
 * @brief 查找某个床位当前住院的患者ID
 *
 * 检查床位是否被占用，并通过住院记录获取当前患者ID。
 *
 * @param service                   指向床位服务结构体
 * @param bed_id                    床位ID
 * @param out_patient_id            输出参数，当前患者ID字符串缓冲区
 * @param out_patient_id_capacity   输出缓冲区容量
 * @return Result                   操作结果，success=1 表示找到当前患者
 */
Result BedService_find_current_patient_by_bed_id(
    BedService *service,
    const char *bed_id,
    char *out_patient_id,
    size_t out_patient_id_capacity
);

/**
 * @brief 清理病房列表
 *
 * 释放链表中所有动态分配的 Ward 节点。
 *
 * @param wards  指向待清理的病房链表
 */
void BedService_clear_wards(LinkedList *wards);

/**
 * @brief 清理床位列表
 *
 * 释放链表中所有动态分配的 Bed 节点。
 *
 * @param beds  指向待清理的床位链表
 */
void BedService_clear_beds(LinkedList *beds);

#endif
