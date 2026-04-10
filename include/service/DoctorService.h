/**
 * @file DoctorService.h
 * @brief 医生服务模块头文件
 *
 * 提供医生信息的添加和查询功能。该服务依赖医生仓库（DoctorRepository）
 * 和科室仓库（DepartmentRepository），在添加医生时会验证所属科室是否存在。
 */

#ifndef HIS_SERVICE_DOCTOR_SERVICE_H
#define HIS_SERVICE_DOCTOR_SERVICE_H

#include "repository/DepartmentRepository.h"
#include "repository/DoctorRepository.h"

/**
 * @brief 医生服务结构体
 *
 * 封装医生数据仓库和科室数据仓库，提供统一的医生管理接口。
 */
typedef struct DoctorService {
    DoctorRepository doctor_repository;          /* 医生数据仓库 */
    DepartmentRepository department_repository;  /* 科室数据仓库 */
} DoctorService;

/**
 * @brief 初始化医生服务
 *
 * 分别初始化医生仓库和科室仓库。
 *
 * @param service          指向待初始化的医生服务结构体
 * @param doctor_path      医生数据文件路径
 * @param department_path  科室数据文件路径
 * @return Result          操作结果，success=1 表示成功
 */
Result DoctorService_init(
    DoctorService *service,
    const char *doctor_path,
    const char *department_path
);

/**
 * @brief 添加新医生
 *
 * 验证医生信息的合法性，检查所属科室是否存在且医生ID不重复后保存。
 *
 * @param service  指向医生服务结构体
 * @param doctor   指向待添加的医生信息
 * @return Result  操作结果，success=1 表示添加成功
 */
Result DoctorService_add(DoctorService *service, const Doctor *doctor);

/**
 * @brief 根据医生ID查找医生
 *
 * @param service     指向医生服务结构体
 * @param doctor_id   待查找的医生ID
 * @param out_doctor  输出参数，查找成功时存放医生信息
 * @return Result     操作结果，success=1 表示找到医生
 */
Result DoctorService_get_by_id(
    DoctorService *service,
    const char *doctor_id,
    Doctor *out_doctor
);

/**
 * @brief 根据科室ID列出该科室下的所有医生
 *
 * @param service        指向医生服务结构体
 * @param department_id  科室ID
 * @param out_doctors    输出参数，查找结果链表（每个节点为 Doctor*）
 * @return Result        操作结果，success=1 表示查找完成
 */
Result DoctorService_list_by_department(
    DoctorService *service,
    const char *department_id,
    LinkedList *out_doctors
);

/**
 * @brief 清理医生列表
 *
 * 释放链表中所有动态分配的 Doctor 节点。
 *
 * @param doctors  指向待清理的医生链表
 */
void DoctorService_clear_list(LinkedList *doctors);

#endif
