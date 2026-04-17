/**
 * @file test_prescription_service.c
 * @brief 处方服务（PrescriptionService）的单元测试文件
 *
 * 本文件测试处方管理服务的核心功能，包括：
 * - 处方创建与按ID/就诊ID读取回显（happy path）
 * - 缺失必填字段、重复处方ID等错误路径
 * - 空列表查询、多条处方按就诊ID聚合等边界条件
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "common/LinkedList.h"
#include "domain/Prescription.h"
#include "repository/PrescriptionRepository.h"
#include "service/PrescriptionService.h"

/**
 * @brief 辅助函数：构建测试用的临时文件路径
 *
 * 结合 time()+clock()+静态序列+进程ID，确保不同用例及并发运行时路径唯一。
 */
static void TestPrescriptionService_make_path(
    char *buffer,
    size_t buffer_size,
    const char *test_name
) {
    static long sequence = 1;

    assert(buffer != 0);
    assert(test_name != 0);

    snprintf(
        buffer,
        buffer_size,
        "build/test_prescription_service_data/%s_%ld_%lu_%ld_%ld.txt",
        test_name,
        (long)time(0),
        (unsigned long)clock(),
        (long)getpid(),
        sequence++
    );
    remove(buffer);
}

/**
 * @brief 辅助函数：构造一个 Prescription 对象
 */
static Prescription TestPrescriptionService_make_prescription(
    const char *prescription_id,
    const char *visit_id,
    const char *medicine_id,
    int quantity,
    const char *usage
) {
    Prescription prescription;

    memset(&prescription, 0, sizeof(prescription));
    strcpy(prescription.prescription_id, prescription_id);
    strcpy(prescription.visit_id, visit_id);
    strcpy(prescription.medicine_id, medicine_id);
    prescription.quantity = quantity;
    strcpy(prescription.usage, usage);
    return prescription;
}

/**
 * @brief Happy path：创建处方后按ID与按就诊ID查询均能回显
 */
static void test_create_and_read_back_prescription(void) {
    char path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    PrescriptionService service;
    Prescription prescription;
    Prescription fetched;
    LinkedList list;
    Prescription *stored;
    Result result;

    TestPrescriptionService_make_path(path, sizeof(path), "create_readback");

    result = PrescriptionService_init(&service, path);
    assert(result.success == 1);

    prescription = TestPrescriptionService_make_prescription(
        "RX0001", "VIS0001", "MED0001", 3, "bid po"
    );

    result = PrescriptionService_create_prescription(&service, &prescription);
    assert(result.success == 1);

    /* 按 ID 查询 */
    memset(&fetched, 0, sizeof(fetched));
    result = PrescriptionService_find_by_id(&service, "RX0001", &fetched);
    assert(result.success == 1);
    assert(strcmp(fetched.prescription_id, "RX0001") == 0);
    assert(strcmp(fetched.visit_id, "VIS0001") == 0);
    assert(strcmp(fetched.medicine_id, "MED0001") == 0);
    assert(fetched.quantity == 3);
    assert(strcmp(fetched.usage, "bid po") == 0);

    /* 按就诊ID查询 */
    LinkedList_init(&list);
    result = PrescriptionService_find_by_visit_id(&service, "VIS0001", &list);
    assert(result.success == 1);
    assert(LinkedList_count(&list) == 1);
    stored = (Prescription *)list.head->data;
    assert(strcmp(stored->prescription_id, "RX0001") == 0);
    assert(stored->quantity == 3);
    PrescriptionService_clear_results(&list);
}

/**
 * @brief 错误路径：缺失必填字段应创建失败
 */
static void test_create_rejects_missing_required_fields(void) {
    char path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    PrescriptionService service;
    Prescription bad;
    Result result;

    TestPrescriptionService_make_path(path, sizeof(path), "missing_fields");

    result = PrescriptionService_init(&service, path);
    assert(result.success == 1);

    /* visit_id 缺失 */
    bad = TestPrescriptionService_make_prescription("RX1001", "", "MED1001", 1, "qd");
    result = PrescriptionService_create_prescription(&service, &bad);
    assert(result.success == 0);

    /* medicine_id 缺失 */
    bad = TestPrescriptionService_make_prescription("RX1001", "VIS1001", "", 1, "qd");
    result = PrescriptionService_create_prescription(&service, &bad);
    assert(result.success == 0);

    /* quantity 非法（0） */
    bad = TestPrescriptionService_make_prescription("RX1001", "VIS1001", "MED1001", 0, "qd");
    result = PrescriptionService_create_prescription(&service, &bad);
    assert(result.success == 0);

    /* quantity 非法（负数） */
    bad = TestPrescriptionService_make_prescription("RX1001", "VIS1001", "MED1001", -5, "qd");
    result = PrescriptionService_create_prescription(&service, &bad);
    assert(result.success == 0);
}

/**
 * @brief 错误路径：重复的处方ID应被拒绝
 */
static void test_create_rejects_duplicate_prescription_id(void) {
    char path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    PrescriptionService service;
    Prescription first;
    Prescription duplicate;
    Result result;

    TestPrescriptionService_make_path(path, sizeof(path), "duplicate_id");

    result = PrescriptionService_init(&service, path);
    assert(result.success == 1);

    first = TestPrescriptionService_make_prescription("RX2001", "VIS2001", "MED2001", 2, "bid");
    result = PrescriptionService_create_prescription(&service, &first);
    assert(result.success == 1);

    /* 第二次使用相同 prescription_id，即便其他字段不同也应失败 */
    duplicate = TestPrescriptionService_make_prescription("RX2001", "VIS2099", "MED2099", 5, "tid");
    result = PrescriptionService_create_prescription(&service, &duplicate);
    assert(result.success == 0);
}

/**
 * @brief 边界条件：未查询到处方时返回空链表
 */
static void test_find_by_visit_id_returns_empty_when_no_match(void) {
    char path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    PrescriptionService service;
    LinkedList list;
    Result result;

    TestPrescriptionService_make_path(path, sizeof(path), "empty_query");

    result = PrescriptionService_init(&service, path);
    assert(result.success == 1);

    LinkedList_init(&list);
    result = PrescriptionService_find_by_visit_id(&service, "VIS_NONE", &list);
    assert(result.success == 1);
    assert(LinkedList_count(&list) == 0);
    PrescriptionService_clear_results(&list);
}

/**
 * @brief 边界条件：同一就诊ID下多条处方应全部返回，不同就诊ID不串联
 */
static void test_find_by_visit_id_filters_correctly(void) {
    char path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    PrescriptionService service;
    Prescription rx;
    LinkedList list;
    LinkedList all;
    Result result;

    TestPrescriptionService_make_path(path, sizeof(path), "multi_visit");

    result = PrescriptionService_init(&service, path);
    assert(result.success == 1);

    /* 同一就诊记录下两条处方 */
    rx = TestPrescriptionService_make_prescription("RX3001", "VIS3001", "MED3001", 1, "qd");
    result = PrescriptionService_create_prescription(&service, &rx);
    assert(result.success == 1);

    rx = TestPrescriptionService_make_prescription("RX3002", "VIS3001", "MED3002", 2, "bid");
    result = PrescriptionService_create_prescription(&service, &rx);
    assert(result.success == 1);

    /* 不同就诊记录的第三条 */
    rx = TestPrescriptionService_make_prescription("RX3003", "VIS3099", "MED3099", 3, "tid");
    result = PrescriptionService_create_prescription(&service, &rx);
    assert(result.success == 1);

    LinkedList_init(&list);
    result = PrescriptionService_find_by_visit_id(&service, "VIS3001", &list);
    assert(result.success == 1);
    assert(LinkedList_count(&list) == 2);
    PrescriptionService_clear_results(&list);

    /* load_all 应得到 3 条记录 */
    LinkedList_init(&all);
    result = PrescriptionService_load_all(&service, &all);
    assert(result.success == 1);
    assert(LinkedList_count(&all) == 3);
    PrescriptionService_clear_results(&all);
}

/**
 * @brief 测试主函数，依次运行所有处方服务测试用例
 */
int main(void) {
    test_create_and_read_back_prescription();
    test_create_rejects_missing_required_fields();
    test_create_rejects_duplicate_prescription_id();
    test_find_by_visit_id_returns_empty_when_no_match();
    test_find_by_visit_id_filters_correctly();
    return 0;
}
