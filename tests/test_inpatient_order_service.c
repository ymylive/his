/**
 * @file test_inpatient_order_service.c
 * @brief 住院医嘱服务（InpatientOrderService）的单元测试文件
 *
 * 本文件测试住院医嘱管理服务的核心功能，包括：
 * - 创建医嘱并自动生成ID、初始状态为 PENDING（happy path）
 * - 执行与取消医嘱的状态流转
 * - 缺失必填字段、未找到医嘱、重复执行/取消等错误路径
 * - 空结果查询、按入院ID筛选等边界条件
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "common/LinkedList.h"
#include "domain/InpatientOrder.h"
#include "repository/InpatientOrderRepository.h"
#include "service/InpatientOrderService.h"

/**
 * @brief 辅助函数：构建测试用的临时文件路径
 */
static void TestInpatientOrderService_make_path(
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
        "build/test_inpatient_order_service_data/%s_%ld_%lu_%ld_%ld.txt",
        test_name,
        (long)time(0),
        (unsigned long)clock(),
        (long)getpid(),
        sequence++
    );
    remove(buffer);
}

/**
 * @brief 辅助函数：构造一个 InpatientOrder 对象
 */
static InpatientOrder TestInpatientOrderService_make_order(
    const char *admission_id,
    const char *order_type,
    const char *content,
    const char *ordered_at
) {
    InpatientOrder order;

    memset(&order, 0, sizeof(order));
    strcpy(order.admission_id, admission_id);
    strcpy(order.order_type, order_type);
    strcpy(order.content, content);
    strcpy(order.ordered_at, ordered_at);
    order.status = INPATIENT_ORDER_STATUS_PENDING;
    return order;
}

/**
 * @brief Happy path：创建医嘱后自动生成ID、初始状态为 PENDING，按入院ID能读回
 */
static void test_create_order_and_find_by_admission(void) {
    char path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    InpatientOrderService service;
    InpatientOrder order;
    LinkedList list;
    InpatientOrder *stored;
    Result result;

    TestInpatientOrderService_make_path(path, sizeof(path), "create_readback");

    result = InpatientOrderService_init(&service, path);
    assert(result.success == 1);

    order = TestInpatientOrderService_make_order(
        "ADM0001",
        "Medication",
        "Amoxicillin 500mg tid",
        "2026-04-01T09:00:00"
    );

    result = InpatientOrderService_create_order(&service, &order);
    assert(result.success == 1);

    /* 自动生成的 order_id 非空，状态为 PENDING，执行/取消时间为空 */
    assert(order.order_id[0] != '\0');
    assert(order.status == INPATIENT_ORDER_STATUS_PENDING);
    assert(order.executed_at[0] == '\0');
    assert(order.cancelled_at[0] == '\0');

    /* 按入院ID查询 */
    LinkedList_init(&list);
    result = InpatientOrderService_find_by_admission_id(&service, "ADM0001", &list);
    assert(result.success == 1);
    assert(LinkedList_count(&list) == 1);
    stored = (InpatientOrder *)list.head->data;
    assert(strcmp(stored->order_id, order.order_id) == 0);
    assert(strcmp(stored->admission_id, "ADM0001") == 0);
    assert(strcmp(stored->content, "Amoxicillin 500mg tid") == 0);
    assert(stored->status == INPATIENT_ORDER_STATUS_PENDING);
    InpatientOrderService_clear_results(&list);
}

/**
 * @brief Happy path：执行医嘱后状态变为 EXECUTED，executed_at 被写入
 */
static void test_execute_order_updates_status(void) {
    char path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    InpatientOrderService service;
    InpatientOrder order;
    InpatientOrder fetched;
    Result result;

    TestInpatientOrderService_make_path(path, sizeof(path), "execute");

    result = InpatientOrderService_init(&service, path);
    assert(result.success == 1);

    order = TestInpatientOrderService_make_order(
        "ADM1001",
        "Examination",
        "Blood routine",
        "2026-04-02T08:30:00"
    );
    result = InpatientOrderService_create_order(&service, &order);
    assert(result.success == 1);

    result = InpatientOrderService_execute_order(
        &service,
        order.order_id,
        "2026-04-02T10:15:00"
    );
    assert(result.success == 1);

    result = InpatientOrderRepository_find_by_id(
        &service.order_repository,
        order.order_id,
        &fetched
    );
    assert(result.success == 1);
    assert(fetched.status == INPATIENT_ORDER_STATUS_EXECUTED);
    assert(strcmp(fetched.executed_at, "2026-04-02T10:15:00") == 0);

    /* 再次执行同一医嘱应失败（状态已非 PENDING） */
    result = InpatientOrderService_execute_order(
        &service,
        order.order_id,
        "2026-04-02T11:00:00"
    );
    assert(result.success == 0);
}

/**
 * @brief 错误路径：缺失必填字段应创建失败
 */
static void test_create_rejects_missing_required_fields(void) {
    char path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    InpatientOrderService service;
    InpatientOrder order;
    Result result;

    TestInpatientOrderService_make_path(path, sizeof(path), "missing_fields");

    result = InpatientOrderService_init(&service, path);
    assert(result.success == 1);

    /* admission_id 缺失 */
    order = TestInpatientOrderService_make_order("", "Medication", "content", "2026-04-03T09:00:00");
    result = InpatientOrderService_create_order(&service, &order);
    assert(result.success == 0);

    /* order_type 缺失 */
    order = TestInpatientOrderService_make_order("ADM2001", "", "content", "2026-04-03T09:00:00");
    result = InpatientOrderService_create_order(&service, &order);
    assert(result.success == 0);

    /* content 缺失 */
    order = TestInpatientOrderService_make_order("ADM2001", "Medication", "", "2026-04-03T09:00:00");
    result = InpatientOrderService_create_order(&service, &order);
    assert(result.success == 0);

    /* ordered_at 缺失 */
    order = TestInpatientOrderService_make_order("ADM2001", "Medication", "content", "");
    result = InpatientOrderService_create_order(&service, &order);
    assert(result.success == 0);
}

/**
 * @brief 错误路径：执行或取消不存在的医嘱应失败
 */
static void test_execute_and_cancel_nonexistent_order_fails(void) {
    char path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    InpatientOrderService service;
    Result result;

    TestInpatientOrderService_make_path(path, sizeof(path), "missing_order");

    result = InpatientOrderService_init(&service, path);
    assert(result.success == 1);

    result = InpatientOrderService_execute_order(
        &service, "ORD999999", "2026-04-04T10:00:00"
    );
    assert(result.success == 0);

    result = InpatientOrderService_cancel_order(
        &service, "ORD999999", "2026-04-04T10:00:00"
    );
    assert(result.success == 0);
}

/**
 * @brief Happy path+错误路径：取消 PENDING 医嘱成功；再次取消已取消的医嘱失败
 */
static void test_cancel_order_and_reject_double_cancel(void) {
    char path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    InpatientOrderService service;
    InpatientOrder order;
    InpatientOrder fetched;
    Result result;

    TestInpatientOrderService_make_path(path, sizeof(path), "cancel");

    result = InpatientOrderService_init(&service, path);
    assert(result.success == 1);

    order = TestInpatientOrderService_make_order(
        "ADM3001", "Nursing", "Observe vital signs", "2026-04-05T07:00:00"
    );
    result = InpatientOrderService_create_order(&service, &order);
    assert(result.success == 1);

    result = InpatientOrderService_cancel_order(
        &service, order.order_id, "2026-04-05T07:30:00"
    );
    assert(result.success == 1);

    result = InpatientOrderRepository_find_by_id(
        &service.order_repository, order.order_id, &fetched
    );
    assert(result.success == 1);
    assert(fetched.status == INPATIENT_ORDER_STATUS_CANCELLED);
    assert(strcmp(fetched.cancelled_at, "2026-04-05T07:30:00") == 0);

    /* 对已取消医嘱再次取消应失败 */
    result = InpatientOrderService_cancel_order(
        &service, order.order_id, "2026-04-05T08:00:00"
    );
    assert(result.success == 0);
}

/**
 * @brief 边界条件：按入院ID查询不存在时返回空链表；多条医嘱下只返回匹配项
 */
static void test_find_by_admission_empty_and_filter(void) {
    char path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    InpatientOrderService service;
    InpatientOrder order;
    LinkedList list;
    LinkedList all;
    Result result;

    TestInpatientOrderService_make_path(path, sizeof(path), "filter");

    result = InpatientOrderService_init(&service, path);
    assert(result.success == 1);

    /* 空仓储，查询应返回空链表 */
    LinkedList_init(&list);
    result = InpatientOrderService_find_by_admission_id(&service, "ADM_NONE", &list);
    assert(result.success == 1);
    assert(LinkedList_count(&list) == 0);
    InpatientOrderService_clear_results(&list);

    /* 插入两个不同入院记录的医嘱 */
    order = TestInpatientOrderService_make_order(
        "ADM4001", "Medication", "Order A", "2026-04-06T09:00:00"
    );
    result = InpatientOrderService_create_order(&service, &order);
    assert(result.success == 1);

    order = TestInpatientOrderService_make_order(
        "ADM4001", "Nursing", "Order B", "2026-04-06T09:30:00"
    );
    result = InpatientOrderService_create_order(&service, &order);
    assert(result.success == 1);

    order = TestInpatientOrderService_make_order(
        "ADM4099", "Examination", "Order C", "2026-04-06T10:00:00"
    );
    result = InpatientOrderService_create_order(&service, &order);
    assert(result.success == 1);

    LinkedList_init(&list);
    result = InpatientOrderService_find_by_admission_id(&service, "ADM4001", &list);
    assert(result.success == 1);
    assert(LinkedList_count(&list) == 2);
    InpatientOrderService_clear_results(&list);

    /* load_all 应得到 3 条 */
    result = InpatientOrderService_load_all(&service, &all);
    assert(result.success == 1);
    assert(LinkedList_count(&all) == 3);
    InpatientOrderService_clear_results(&all);
}

/**
 * @brief 测试主函数
 */
int main(void) {
    test_create_order_and_find_by_admission();
    test_execute_order_updates_status();
    test_create_rejects_missing_required_fields();
    test_execute_and_cancel_nonexistent_order_fails();
    test_cancel_order_and_reject_double_cancel();
    test_find_by_admission_empty_and_filter();
    return 0;
}
