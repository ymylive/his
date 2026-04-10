/**
 * @file PharmacyService.h
 * @brief 药房服务模块头文件
 *
 * 提供药品管理和发药业务功能，包括药品添加、补货、发药、库存查询、
 * 低库存药品查找以及发药记录查询。该服务依赖药品仓库和发药记录仓库。
 */

#ifndef HIS_SERVICE_PHARMACY_SERVICE_H
#define HIS_SERVICE_PHARMACY_SERVICE_H

#include "common/LinkedList.h"
#include "common/Result.h"
#include "domain/DispenseRecord.h"
#include "domain/Medicine.h"
#include "repository/DispenseRecordRepository.h"
#include "repository/MedicineRepository.h"

/**
 * @brief 药房服务结构体
 *
 * 封装药品仓库和发药记录仓库，提供统一的药房管理接口。
 */
typedef struct PharmacyService {
    MedicineRepository medicine_repository;              /* 药品数据仓库 */
    DispenseRecordRepository dispense_record_repository; /* 发药记录仓库 */
} PharmacyService;

/**
 * @brief 初始化药房服务
 *
 * 分别初始化药品仓库和发药记录仓库，并确保存储文件就绪。
 *
 * @param service              指向待初始化的药房服务结构体
 * @param medicine_path        药品数据文件路径
 * @param dispense_record_path 发药记录数据文件路径
 * @return Result              操作结果，success=1 表示成功
 */
Result PharmacyService_init(
    PharmacyService *service,
    const char *medicine_path,
    const char *dispense_record_path
);

/**
 * @brief 添加新药品
 *
 * 验证药品信息的合法性，确保药品ID不重复后保存到存储中。
 *
 * @param service   指向药房服务结构体
 * @param medicine  指向待添加的药品信息
 * @return Result   操作结果，success=1 表示添加成功
 */
Result PharmacyService_add_medicine(
    PharmacyService *service,
    const Medicine *medicine
);

/**
 * @brief 药品补货（增加库存）
 *
 * 根据药品ID查找药品并增加指定数量的库存。
 *
 * @param service      指向药房服务结构体
 * @param medicine_id  药品ID
 * @param quantity     补货数量（必须大于0）
 * @return Result      操作结果，success=1 表示补货成功
 */
Result PharmacyService_restock_medicine(
    PharmacyService *service,
    const char *medicine_id,
    int quantity
);

/**
 * @brief 发药（不指定患者ID）
 *
 * 根据处方ID和药品ID进行发药操作，扣减库存并生成发药记录。
 *
 * @param service         指向药房服务结构体
 * @param prescription_id 处方ID
 * @param medicine_id     药品ID
 * @param quantity        发药数量（必须大于0）
 * @param pharmacist_id   药剂师ID
 * @param dispensed_at    发药时间字符串
 * @param out_record      输出参数，发药成功时存放发药记录
 * @return Result         操作结果，success=1 表示发药成功
 */
Result PharmacyService_dispense_medicine(
    PharmacyService *service,
    const char *prescription_id,
    const char *medicine_id,
    int quantity,
    const char *pharmacist_id,
    const char *dispensed_at,
    DispenseRecord *out_record
);

/**
 * @brief 为指定患者发药
 *
 * 与 dispense_medicine 类似，但额外要求提供患者ID并记录到发药记录中。
 *
 * @param service         指向药房服务结构体
 * @param patient_id      患者ID（必填）
 * @param prescription_id 处方ID
 * @param medicine_id     药品ID
 * @param quantity        发药数量（必须大于0）
 * @param pharmacist_id   药剂师ID
 * @param dispensed_at    发药时间字符串
 * @param out_record      输出参数，发药成功时存放发药记录
 * @return Result         操作结果，success=1 表示发药成功
 */
Result PharmacyService_dispense_medicine_for_patient(
    PharmacyService *service,
    const char *patient_id,
    const char *prescription_id,
    const char *medicine_id,
    int quantity,
    const char *pharmacist_id,
    const char *dispensed_at,
    DispenseRecord *out_record
);

/**
 * @brief 查询药品当前库存
 *
 * @param service      指向药房服务结构体
 * @param medicine_id  药品ID
 * @param out_stock    输出参数，查询成功时存放库存数量
 * @return Result      操作结果，success=1 表示查询成功
 */
Result PharmacyService_get_stock(
    PharmacyService *service,
    const char *medicine_id,
    int *out_stock
);

/**
 * @brief 查找低库存药品
 *
 * 查找所有库存量不超过低库存阈值的药品。
 *
 * @param service        指向药房服务结构体
 * @param out_medicines  输出参数，低库存药品链表（每个节点为 Medicine*）
 * @return Result        操作结果，success=1 表示查询完成
 */
Result PharmacyService_find_low_stock_medicines(
    PharmacyService *service,
    LinkedList *out_medicines
);

/**
 * @brief 清理药品查询结果链表
 *
 * 释放链表中所有动态分配的 Medicine 节点。
 *
 * @param medicines  指向待清理的药品链表
 */
void PharmacyService_clear_medicine_results(LinkedList *medicines);

/**
 * @brief 根据处方ID查找发药记录
 *
 * @param service         指向药房服务结构体
 * @param prescription_id 处方ID
 * @param out_records     输出参数，发药记录链表（必须为空链表）
 * @return Result         操作结果，success=1 表示查询完成
 */
Result PharmacyService_find_dispense_records_by_prescription_id(
    PharmacyService *service,
    const char *prescription_id,
    LinkedList *out_records
);

/**
 * @brief 根据患者ID查找发药记录
 *
 * @param service      指向药房服务结构体
 * @param patient_id   患者ID
 * @param out_records  输出参数，发药记录链表（必须为空链表）
 * @return Result      操作结果，success=1 表示查询完成
 */
Result PharmacyService_find_dispense_records_by_patient_id(
    PharmacyService *service,
    const char *patient_id,
    LinkedList *out_records
);

/**
 * @brief 清理发药记录查询结果链表
 *
 * 释放链表中所有动态分配的 DispenseRecord 节点。
 *
 * @param records  指向待清理的发药记录链表
 */
void PharmacyService_clear_dispense_record_results(LinkedList *records);

#endif
