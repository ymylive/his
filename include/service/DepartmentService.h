/**
 * @file DepartmentService.h
 * @brief 科室服务模块头文件
 *
 * 提供科室信息的添加、更新和查询功能。
 * 该服务依赖科室仓库（DepartmentRepository）进行数据持久化。
 */

#ifndef HIS_SERVICE_DEPARTMENT_SERVICE_H
#define HIS_SERVICE_DEPARTMENT_SERVICE_H

#include "repository/DepartmentRepository.h"

/**
 * @brief 科室服务结构体
 *
 * 封装科室数据仓库，提供统一的科室管理接口。
 */
typedef struct DepartmentService {
    DepartmentRepository repository;  /* 科室数据仓库 */
} DepartmentService;

/**
 * @brief 初始化科室服务
 *
 * @param service  指向待初始化的科室服务结构体
 * @param path     科室数据文件路径
 * @return Result  操作结果，success=1 表示成功
 */
Result DepartmentService_init(DepartmentService *service, const char *path);

/**
 * @brief 添加新科室
 *
 * 验证科室信息合法性，确保科室ID不重复后保存到存储中。
 *
 * @param service     指向科室服务结构体
 * @param department  指向待添加的科室信息
 * @return Result     操作结果，success=1 表示添加成功
 */
Result DepartmentService_add(
    DepartmentService *service,
    const Department *department
);

/**
 * @brief 更新科室信息
 *
 * 根据科室ID查找已有记录并更新。
 *
 * @param service     指向科室服务结构体
 * @param department  指向包含更新后信息的科室结构体（department_id 用于定位记录）
 * @return Result     操作结果，success=1 表示更新成功
 */
Result DepartmentService_update(
    DepartmentService *service,
    const Department *department
);

/**
 * @brief 根据科室ID查找科室
 *
 * @param service         指向科室服务结构体
 * @param department_id   待查找的科室ID
 * @param out_department  输出参数，查找成功时存放科室信息
 * @return Result         操作结果，success=1 表示找到科室
 */
Result DepartmentService_get_by_id(
    DepartmentService *service,
    const char *department_id,
    Department *out_department
);

#endif
