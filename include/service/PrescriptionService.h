/**
 * @file PrescriptionService.h
 * @brief 处方服务模块头文件
 *
 * 提供处方管理业务功能，包括处方创建、按就诊ID查询、
 * 按处方ID查询以及全量加载。该服务依赖处方仓库。
 */

#ifndef HIS_SERVICE_PRESCRIPTION_SERVICE_H
#define HIS_SERVICE_PRESCRIPTION_SERVICE_H

#include "common/LinkedList.h"
#include "common/Result.h"
#include "domain/Prescription.h"
#include "repository/PrescriptionRepository.h"

/**
 * @brief 处方服务结构体
 *
 * 封装处方仓库，提供统一的处方管理接口。
 */
typedef struct PrescriptionService {
    PrescriptionRepository prescription_repository; /**< 处方数据仓库 */
} PrescriptionService;

/**
 * @brief 初始化处方服务
 *
 * 初始化处方仓库并确保存储文件就绪。
 *
 * @param service 指向待初始化的处方服务结构体
 * @param path    处方数据文件路径
 * @return Result 操作结果，success=1 表示成功
 */
Result PrescriptionService_init(PrescriptionService *service, const char *path);

/**
 * @brief 创建处方
 *
 * 验证处方信息的合法性后保存到存储中。
 *
 * @param service      指向处方服务结构体
 * @param prescription 指向待创建的处方信息
 * @return Result      操作结果，success=1 表示创建成功
 */
Result PrescriptionService_create_prescription(
    PrescriptionService *service,
    const Prescription *prescription
);

/**
 * @brief 按就诊ID查找处方列表
 *
 * @param service    指向处方服务结构体
 * @param visit_id   就诊ID
 * @param out_list   输出参数，处方链表（必须为空链表）
 * @return Result    操作结果，success=1 表示查询完成
 */
Result PrescriptionService_find_by_visit_id(
    PrescriptionService *service,
    const char *visit_id,
    LinkedList *out_list
);

/**
 * @brief 按处方ID查找单条处方
 *
 * @param service         指向处方服务结构体
 * @param prescription_id 处方ID
 * @param out             输出参数，存放找到的处方记录
 * @return Result         操作结果，success=1 表示找到
 */
Result PrescriptionService_find_by_id(
    PrescriptionService *service,
    const char *prescription_id,
    Prescription *out
);

/**
 * @brief 加载所有处方记录
 *
 * @param service  指向处方服务结构体
 * @param out_list 输出参数，处方链表（必须为空链表）
 * @return Result  操作结果，success=1 表示加载完成
 */
Result PrescriptionService_load_all(
    PrescriptionService *service,
    LinkedList *out_list
);

/**
 * @brief 清理处方查询结果链表
 *
 * 释放链表中所有动态分配的 Prescription 节点。
 *
 * @param list 指向待清理的处方链表
 */
void PrescriptionService_clear_results(LinkedList *list);

#endif
