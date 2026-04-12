/**
 * @file NursingRecordService.h
 * @brief 护理记录服务模块头文件
 *
 * 提供护理记录的业务操作功能，包括创建护理记录、按住院记录查询、
 * 加载全部护理记录以及清理查询结果。
 * 该服务依赖护理记录仓储层进行数据持久化。
 */

#ifndef HIS_SERVICE_NURSING_RECORD_SERVICE_H
#define HIS_SERVICE_NURSING_RECORD_SERVICE_H

#include "common/LinkedList.h"
#include "common/Result.h"
#include "domain/NursingRecord.h"
#include "repository/NursingRecordRepository.h"

/**
 * @brief 护理记录服务结构体
 *
 * 封装护理记录仓储，提供统一的护理记录管理接口。
 */
typedef struct NursingRecordService {
    NursingRecordRepository repository; /**< 护理记录数据仓储 */
} NursingRecordService;

/**
 * @brief 初始化护理记录服务
 *
 * 初始化底层仓储，确保存储文件就绪。
 *
 * @param service 指向待初始化的服务结构体
 * @param path    护理记录数据文件路径
 * @return Result 操作结果，success=1 表示成功
 */
Result NursingRecordService_init(NursingRecordService *service, const char *path);

/**
 * @brief 创建护理记录
 *
 * 自动生成护理记录ID，校验必填字段后追加到存储中。
 *
 * @param service 指向服务结构体
 * @param record  指向待创建的护理记录（nursing_id 将被自动生成覆盖）
 * @return Result 操作结果，success=1 表示创建成功
 */
Result NursingRecordService_create_record(
    NursingRecordService *service,
    NursingRecord *record
);

/**
 * @brief 按住院记录ID查找所有关联的护理记录
 *
 * @param service      指向服务结构体
 * @param admission_id 住院记录ID
 * @param out_list     输出参数，存放匹配的护理记录链表
 * @return Result      操作结果，success=1 表示查询完成
 */
Result NursingRecordService_find_by_admission_id(
    NursingRecordService *service,
    const char *admission_id,
    LinkedList *out_list
);

/**
 * @brief 加载所有护理记录
 *
 * @param service  指向服务结构体
 * @param out_list 输出参数，存放所有护理记录的链表
 * @return Result  操作结果，success=1 表示加载成功
 */
Result NursingRecordService_load_all(
    NursingRecordService *service,
    LinkedList *out_list
);

/**
 * @brief 清理护理记录查询结果链表
 *
 * 释放链表中所有动态分配的 NursingRecord 节点。
 *
 * @param list 指向待清理的护理记录链表
 */
void NursingRecordService_clear_results(LinkedList *list);

#endif
