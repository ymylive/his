/**
 * @file test_pharmacy_service.c
 * @brief 药房服务（PharmacyService）的单元测试文件
 *
 * 本文件测试药房管理服务的核心功能，包括：
 * - 药品添加、补货和库存查询
 * - 发药操作：扣减库存并生成发药记录
 * - 低库存预警和库存不足时拒绝发药
 * - 按患者ID查询发药历史
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "common/LinkedList.h"
#include "domain/DispenseRecord.h"
#include "domain/Medicine.h"
#include "repository/DispenseRecordRepository.h"
#include "service/PharmacyService.h"

/**
 * @brief 辅助函数：构建测试用的临时文件路径并删除已存在的文件
 *
 * 使用静态序列号确保同一测试内多个文件路径唯一。
 * @param test_name 测试名称
 * @param kind 文件类型（如"medicine"或"record"）
 */
static void TestPharmacyService_make_path(
    char *buffer,
    size_t buffer_size,
    const char *test_name,
    const char *kind
) {
    static long sequence = 1;

    assert(buffer != 0);
    assert(test_name != 0);
    assert(kind != 0);

    snprintf(
        buffer,
        buffer_size,
        "build/test_pharmacy_service_data/%s_%s_%ld_%ld.txt",
        test_name,
        kind,
        (long)time(0),
        sequence++
    );
    remove(buffer);
}

/**
 * @brief 辅助函数：构造一个 Medicine（药品）对象
 */
static Medicine TestPharmacyService_make_medicine(
    const char *medicine_id,
    const char *name,
    double price,
    int stock,
    const char *department_id,
    int low_stock_threshold
) {
    Medicine medicine;

    memset(&medicine, 0, sizeof(medicine));
    strcpy(medicine.medicine_id, medicine_id);
    strcpy(medicine.name, name);
    medicine.price = price;
    medicine.stock = stock;
    strcpy(medicine.department_id, department_id);
    medicine.low_stock_threshold = low_stock_threshold;
    return medicine;
}

/**
 * @brief 辅助函数：链表查找谓词 - 判断药品ID是否匹配
 */
static int TestPharmacyService_has_medicine_id(const void *data, const void *context) {
    const Medicine *medicine = (const Medicine *)data;
    const char *medicine_id = (const char *)context;

    return strcmp(medicine->medicine_id, medicine_id) == 0;
}

/**
 * @brief 测试药品添加、补货和库存查询
 *
 * 验证流程：
 * 1. 添加一种药品（初始库存10）
 * 2. 查询库存验证为10
 * 3. 补货15件
 * 4. 再次查询库存验证为25
 */
static void test_add_restock_and_query_inventory(void) {
    char medicine_path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    char record_path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    PharmacyService service;
    Medicine medicine = TestPharmacyService_make_medicine(
        "MED1001",
        "Amoxicillin",
        12.50,
        10,
        "DEP01",
        5
    );
    int stock = 0;
    Result result;

    TestPharmacyService_make_path(medicine_path, sizeof(medicine_path), "inventory", "medicine");
    TestPharmacyService_make_path(record_path, sizeof(record_path), "inventory", "record");

    /* 初始化药房服务 */
    result = PharmacyService_init(&service, medicine_path, record_path);
    assert(result.success == 1);

    /* 添加药品 */
    result = PharmacyService_add_medicine(&service, &medicine);
    assert(result.success == 1);

    /* 查询库存 */
    result = PharmacyService_get_stock(&service, "MED1001", &stock);
    assert(result.success == 1);
    assert(stock == 10); /* 初始库存为10 */

    /* 补货15件 */
    result = PharmacyService_restock_medicine(&service, "MED1001", 15);
    assert(result.success == 1);

    /* 验证补货后库存为25 */
    result = PharmacyService_get_stock(&service, "MED1001", &stock);
    assert(result.success == 1);
    assert(stock == 25);
}

/**
 * @brief 测试发药操作：扣减库存并生成发药记录
 *
 * 验证流程：
 * 1. 添加药品（初始库存20）
 * 2. 为患者发药6件
 * 3. 验证发药记录各字段正确
 * 4. 验证库存从20减为14
 * 5. 按患者ID查询发药记录，验证有1条记录
 */
static void test_dispense_updates_inventory_and_writes_record(void) {
    char medicine_path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    char record_path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    PharmacyService service;
    DispenseRecord record;
    LinkedList records;
    DispenseRecord *stored_record = 0;
    int stock = 0;
    Result result;

    TestPharmacyService_make_path(medicine_path, sizeof(medicine_path), "dispense", "medicine");
    TestPharmacyService_make_path(record_path, sizeof(record_path), "dispense", "record");

    result = PharmacyService_init(&service, medicine_path, record_path);
    assert(result.success == 1);

    /* 添加药品（库存20） */
    result = PharmacyService_add_medicine(
        &service,
        &(Medicine){
            "MED2001",
            "Ibuprofen",
            "",
            "",
            18.00,
            20,
            "DEP02",
            5
        }
    );
    assert(result.success == 1);

    /* 为患者发药 */
    memset(&record, 0, sizeof(record));
    result = PharmacyService_dispense_medicine_for_patient(
        &service,
        "PAT2001",       /* 患者ID */
        "RX2001",        /* 处方ID */
        "MED2001",       /* 药品ID */
        6,               /* 数量 */
        "PHA2001",       /* 药剂师ID */
        "2026-03-20T10:30:00",
        &record
    );
    assert(result.success == 1);

    /* 验证发药记录各字段 */
    assert(record.dispense_id[0] != '\0');                            /* ID已自动分配 */
    assert(strcmp(record.patient_id, "PAT2001") == 0);                /* 患者ID */
    assert(strcmp(record.prescription_id, "RX2001") == 0);            /* 处方ID */
    assert(strcmp(record.medicine_id, "MED2001") == 0);               /* 药品ID */
    assert(record.quantity == 6);                                      /* 数量 */
    assert(strcmp(record.pharmacist_id, "PHA2001") == 0);             /* 药剂师ID */
    assert(strcmp(record.dispensed_at, "2026-03-20T10:30:00") == 0);  /* 发药时间 */
    assert(record.status == DISPENSE_STATUS_COMPLETED);               /* 状态为已完成 */

    /* 验证库存已扣减 */
    result = PharmacyService_get_stock(&service, "MED2001", &stock);
    assert(result.success == 1);
    assert(stock == 14); /* 20 - 6 = 14 */

    /* 按患者ID查询发药记录 */
    LinkedList_init(&records);
    result = PharmacyService_find_dispense_records_by_patient_id(&service, "PAT2001", &records);
    assert(result.success == 1);
    assert(LinkedList_count(&records) == 1); /* 应有1条记录 */

    /* 验证存储的记录与返回的一致 */
    stored_record = (DispenseRecord *)records.head->data;
    assert(stored_record != 0);
    assert(strcmp(stored_record->dispense_id, record.dispense_id) == 0);
    assert(strcmp(stored_record->patient_id, "PAT2001") == 0);
    assert(stored_record->quantity == 6);
    DispenseRecordRepository_clear_list(&records);
}

/**
 * @brief 测试低库存预警和库存不足时拒绝发药
 *
 * 验证流程：
 * 1. 添加两种药品（VitaminC库存4/阈值5，Paracetamol库存12/阈值2）
 * 2. 查询低库存药品，应只有VitaminC
 * 3. 发药使Paracetamol库存降至1（低于阈值2）
 * 4. 再次查询低库存药品，应有两种
 * 5. 库存不足时发药应失败（VitaminC库存4，请求10）
 * 6. 发药失败不应生成发药记录
 */
static void test_low_stock_alerts_and_insufficient_stock_rejected(void) {
    char medicine_path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    char record_path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    PharmacyService service;
    LinkedList low_stock_medicines;
    LinkedList failed_records;
    Result result;

    TestPharmacyService_make_path(medicine_path, sizeof(medicine_path), "low_stock", "medicine");
    TestPharmacyService_make_path(record_path, sizeof(record_path), "low_stock", "record");

    result = PharmacyService_init(&service, medicine_path, record_path);
    assert(result.success == 1);

    /* 添加VitaminC：库存4，低于阈值5 */
    result = PharmacyService_add_medicine(
        &service,
        &(Medicine){
            "MED3001",
            "VitaminC",
            "",
            "",
            6.80,
            4,
            "DEP03",
            5
        }
    );
    assert(result.success == 1);

    /* 添加Paracetamol：库存12，高于阈值2 */
    result = PharmacyService_add_medicine(
        &service,
        &(Medicine){
            "MED3002",
            "Paracetamol",
            "",
            "",
            9.90,
            12,
            "DEP03",
            2
        }
    );
    assert(result.success == 1);

    /* 查询低库存药品，应只有VitaminC */
    LinkedList_init(&low_stock_medicines);
    result = PharmacyService_find_low_stock_medicines(&service, &low_stock_medicines);
    assert(result.success == 1);
    assert(LinkedList_count(&low_stock_medicines) == 1);
    assert(
        LinkedList_find_first(
            &low_stock_medicines,
            TestPharmacyService_has_medicine_id,
            "MED3001"
        ) != 0
    );
    PharmacyService_clear_medicine_results(&low_stock_medicines);

    /* 发药11件Paracetamol，库存从12降至1（低于阈值2） */
    result = PharmacyService_dispense_medicine(
        &service,
        "RX3001",
        "MED3002",
        11,
        "PHA3001",
        "2026-03-20T11:00:00",
        0
    );
    assert(result.success == 1);

    /* 再次查询低库存药品，应有两种 */
    LinkedList_init(&low_stock_medicines);
    result = PharmacyService_find_low_stock_medicines(&service, &low_stock_medicines);
    assert(result.success == 1);
    assert(LinkedList_count(&low_stock_medicines) == 2);
    assert(
        LinkedList_find_first(
            &low_stock_medicines,
            TestPharmacyService_has_medicine_id,
            "MED3001"
        ) != 0
    );
    assert(
        LinkedList_find_first(
            &low_stock_medicines,
            TestPharmacyService_has_medicine_id,
            "MED3002"
        ) != 0
    );
    PharmacyService_clear_medicine_results(&low_stock_medicines);

    /* 库存不足时发药应失败（VitaminC库存4，请求10） */
    result = PharmacyService_dispense_medicine(
        &service,
        "RX3002",
        "MED3001",
        10,
        "PHA3001",
        "2026-03-20T11:10:00",
        0
    );
    assert(result.success == 0);

    /* 验证发药失败未生成发药记录 */
    LinkedList_init(&failed_records);
    result = DispenseRecordRepository_find_by_prescription_id(
        &service.dispense_record_repository,
        "RX3002",
        &failed_records
    );
    assert(result.success == 1);
    assert(LinkedList_count(&failed_records) == 0); /* 无记录 */
    DispenseRecordRepository_clear_list(&failed_records);
}

/**
 * @brief 测试按患者ID查询发药历史
 *
 * 为两个不同患者各发一次药，按 PAT9001 查询，
 * 验证只返回该患者的发药记录且各字段正确。
 */
static void test_query_dispense_history_by_patient_id(void) {
    char medicine_path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    char record_path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    PharmacyService service;
    LinkedList records;
    DispenseRecord *stored_record = 0;
    Result result;

    TestPharmacyService_make_path(medicine_path, sizeof(medicine_path), "history", "medicine");
    TestPharmacyService_make_path(record_path, sizeof(record_path), "history", "record");

    result = PharmacyService_init(&service, medicine_path, record_path);
    assert(result.success == 1);

    /* 添加药品 */
    result = PharmacyService_add_medicine(
        &service,
        &(Medicine){
            "MED9001",
            "HistoryMed",
            "",
            "",
            10.00,
            10,
            "DEP09",
            1
        }
    );
    assert(result.success == 1);

    /* 为 PAT9001 发药 */
    result = PharmacyService_dispense_medicine_for_patient(
        &service,
        "PAT9001",
        "RX9001",
        "MED9001",
        2,
        "PHA9001",
        "2026-03-20T19:00:00",
        0
    );
    assert(result.success == 1);

    /* 为 PAT9002 发药 */
    result = PharmacyService_dispense_medicine_for_patient(
        &service,
        "PAT9002",
        "RX9002",
        "MED9001",
        1,
        "PHA9001",
        "2026-03-20T19:10:00",
        0
    );
    assert(result.success == 1);

    /* 按 PAT9001 查询发药历史 */
    LinkedList_init(&records);
    result = PharmacyService_find_dispense_records_by_patient_id(&service, "PAT9001", &records);
    assert(result.success == 1);
    assert(LinkedList_count(&records) == 1); /* 只有PAT9001的1条记录 */

    /* 验证记录各字段 */
    stored_record = (DispenseRecord *)records.head->data;
    assert(stored_record != 0);
    assert(strcmp(stored_record->patient_id, "PAT9001") == 0);
    assert(strcmp(stored_record->prescription_id, "RX9001") == 0);
    assert(strcmp(stored_record->medicine_id, "MED9001") == 0);
    assert(stored_record->quantity == 2);
    assert(strcmp(stored_record->pharmacist_id, "PHA9001") == 0);
    assert(strcmp(stored_record->dispensed_at, "2026-03-20T19:00:00") == 0);
    assert(stored_record->status == DISPENSE_STATUS_COMPLETED);

    PharmacyService_clear_dispense_record_results(&records);
}

/**
 * @brief 测试主函数，依次运行所有药房服务测试用例
 */
int main(void) {
    test_add_restock_and_query_inventory();
    test_dispense_updates_inventory_and_writes_record();
    test_low_stock_alerts_and_insufficient_stock_rejected();
    test_query_dispense_history_by_patient_id();
    return 0;
}
