/**
 * @file RoundRecordService.h
 * @brief 查房记录服务模块头文件
 *
 * 提供查房记录的业务操作功能，包括创建查房记录、
 * 按住院记录查询、加载全部查房记录。
 * 该服务依赖查房记录仓储层进行数据持久化。
 */

#ifndef HIS_SERVICE_ROUND_RECORD_SERVICE_H
#define HIS_SERVICE_ROUND_RECORD_SERVICE_H

#include "common/LinkedList.h"
#include "common/Result.h"
#include "domain/RoundRecord.h"
#include "repository/RoundRecordRepository.h"

/**
 * @brief 查房记录服务结构体
 *
 * 封装查房记录仓储，提供统一的查房记录管理接口。
 */
typedef struct RoundRecordService {
    RoundRecordRepository repository; /**< 查房记录数据仓储 */
} RoundRecordService;

/**
 * @brief 初始化查房记录服务
 *
 * 初始化底层查房记录仓储，确保存储文件就绪。
 *
 * @param service 指向待初始化的服务结构体
 * @param path    查房记录数据文件路径
 * @return Result 操作结果，success=1 表示成功
 */
Result RoundRecordService_init(RoundRecordService *service, const char *path);

/**
 * @brief 创建查房记录
 *
 * 自动生成查房记录ID，校验必填字段后追加到存储中。
 *
 * @param service 指向服务结构体
 * @param record  指向待创建的查房记录（round_id 将被自动生成覆盖）
 * @return Result 操作结果，success=1 表示创建成功
 */
Result RoundRecordService_create_record(
    RoundRecordService *service,
    RoundRecord *record
);

/**
 * @brief 按住院记录ID查找所有关联的查房记录
 *
 * @param service      指向服务结构体
 * @param admission_id 住院记录ID
 * @param out_list     输出参数，存放匹配的查房记录链表
 * @return Result      操作结果，success=1 表示查询完成
 */
Result RoundRecordService_find_by_admission_id(
    RoundRecordService *service,
    const char *admission_id,
    LinkedList *out_list
);

/**
 * @brief 加载所有查房记录
 *
 * @param service  指向服务结构体
 * @param out_list 输出参数，存放所有查房记录的链表
 * @return Result  操作结果，success=1 表示加载成功
 */
Result RoundRecordService_load_all(
    RoundRecordService *service,
    LinkedList *out_list
);

/**
 * @brief 清理查房记录查询结果链表
 *
 * 释放链表中所有动态分配的 RoundRecord 节点。
 *
 * @param list 指向待清理的查房记录链表
 */
void RoundRecordService_clear_results(LinkedList *list);

#endif
