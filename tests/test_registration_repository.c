/**
 * @file test_registration_repository.c
 * @brief 挂号仓储（RegistrationRepository）的单元测试文件
 *
 * 本文件测试挂号数据仓储层的核心功能，包括：
 * - 追加记录和按挂号ID查找
 * - 按患者ID查找（支持状态和时间范围过滤）
 * - 全量重写（save_all）
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "repository/RegistrationRepository.h"

/* 测试用的固定文件路径 */
static const char *TEST_REGISTRATION_PATH =
    "build/test_registration_repository_data/registrations.txt";

/* 预期的文件表头 */
static const char *TEST_REGISTRATION_HEADER =
    "registration_id|patient_id|doctor_id|department_id|registered_at|status|diagnosed_at|cancelled_at|registration_type|registration_fee";

/**
 * @brief 辅助函数：安全地复制字符串
 */
static void copy_text(char *destination, size_t capacity, const char *source) {
    assert(destination != 0);
    assert(capacity > 0);

    if (source == 0) {
        destination[0] = '\0';
        return;
    }

    strncpy(destination, source, capacity - 1);
    destination[capacity - 1] = '\0';
}

/**
 * @brief 辅助函数：构造一个 Registration（挂号记录）对象
 */
static Registration make_registration(
    const char *registration_id,
    const char *patient_id,
    const char *doctor_id,
    const char *department_id,
    const char *registered_at,
    RegistrationStatus status,
    const char *diagnosed_at,
    const char *cancelled_at
) {
    Registration registration;

    memset(&registration, 0, sizeof(registration));
    copy_text(registration.registration_id, sizeof(registration.registration_id), registration_id);
    copy_text(registration.patient_id, sizeof(registration.patient_id), patient_id);
    copy_text(registration.doctor_id, sizeof(registration.doctor_id), doctor_id);
    copy_text(registration.department_id, sizeof(registration.department_id), department_id);
    copy_text(registration.registered_at, sizeof(registration.registered_at), registered_at);
    registration.status = status;
    copy_text(registration.diagnosed_at, sizeof(registration.diagnosed_at), diagnosed_at);
    copy_text(registration.cancelled_at, sizeof(registration.cancelled_at), cancelled_at);
    registration.registration_type = REG_TYPE_STANDARD;
    registration.registration_fee = 5.00;
    return registration;
}

/**
 * @brief 辅助函数：在堆上分配一个 Registration 对象的副本
 */
static Registration *clone_registration(const Registration *source) {
    Registration *copy = (Registration *)malloc(sizeof(*copy));

    assert(copy != 0);
    *copy = *source;
    return copy;
}

/**
 * @brief 辅助函数：获取链表中指定索引位置的 Registration 指针
 */
static const Registration *registration_at(const LinkedList *list, size_t index) {
    LinkedListNode *node = 0;
    size_t current_index = 0;

    assert(list != 0);
    node = list->head;
    while (node != 0) {
        if (current_index == index) {
            return (const Registration *)node->data;
        }

        node = node->next;
        current_index++;
    }

    return 0;
}

/**
 * @brief 辅助函数：断言文件表头已正确写入
 */
static void assert_header_written(void) {
    FILE *file = fopen(TEST_REGISTRATION_PATH, "r");
    char line[256];

    assert(file != 0);
    assert(fgets(line, sizeof(line), file) != 0);
    fclose(file);

    line[strcspn(line, "\r\n")] = '\0'; /* 去除行尾换行符 */
    assert(strcmp(line, TEST_REGISTRATION_HEADER) == 0);
}

/**
 * @brief 辅助函数：初始化仓储并预埋三条不同状态的挂号记录
 *
 * 预埋数据：
 * - REG0001：PAT0001，待诊（PENDING）
 * - REG0002：PAT0002，已诊断（DIAGNOSED）
 * - REG0003：PAT0001，已取消（CANCELLED）
 *
 * @return 初始化好的仓储对象
 */
static RegistrationRepository seed_repository(
    Registration *pending,
    Registration *diagnosed,
    Registration *cancelled
) {
    RegistrationRepository repository;
    Result result;

    remove(TEST_REGISTRATION_PATH); /* 清理旧文件 */

    /* 构造三条不同状态的挂号记录 */
    *pending = make_registration(
        "REG0001",
        "PAT0001",
        "DOC0001",
        "DEP0001",
        "2026-03-20T08:00",
        REG_STATUS_PENDING,
        "",
        ""
    );
    *diagnosed = make_registration(
        "REG0002",
        "PAT0002",
        "DOC0002",
        "DEP0001",
        "2026-03-20T09:00",
        REG_STATUS_DIAGNOSED,
        "2026-03-20T09:30",
        ""
    );
    *cancelled = make_registration(
        "REG0003",
        "PAT0001",
        "DOC0003",
        "DEP0002",
        "2026-03-21T10:00",
        REG_STATUS_CANCELLED,
        "",
        "2026-03-21T10:30"
    );

    /* 初始化仓储并追加记录 */
    result = RegistrationRepository_init(&repository, TEST_REGISTRATION_PATH);
    assert(result.success == 1);

    result = RegistrationRepository_append(&repository, pending);
    assert(result.success == 1);

    result = RegistrationRepository_append(&repository, diagnosed);
    assert(result.success == 1);

    result = RegistrationRepository_append(&repository, cancelled);
    assert(result.success == 1);

    assert_header_written(); /* 验证表头 */
    return repository;
}

/**
 * @brief 测试追加记录和按挂号ID查找
 *
 * 验证：
 * 1. 按ID查找已诊断的记录，验证各字段
 * 2. 查找不存在的ID应失败
 * 3. 查找已取消的记录，验证取消时间
 */
static void test_append_and_find_by_registration_id(void) {
    RegistrationRepository repository;
    Registration pending;
    Registration diagnosed;
    Registration cancelled;
    Registration loaded;
    Result result;

    repository = seed_repository(&pending, &diagnosed, &cancelled);

    /* 按ID查找已诊断的记录 */
    result = RegistrationRepository_find_by_registration_id(&repository, "REG0002", &loaded);
    assert(result.success == 1);
    assert(strcmp(loaded.registration_id, diagnosed.registration_id) == 0);
    assert(strcmp(loaded.patient_id, diagnosed.patient_id) == 0);
    assert(loaded.status == REG_STATUS_DIAGNOSED);
    assert(strcmp(loaded.diagnosed_at, "2026-03-20T09:30") == 0);
    assert(strcmp(loaded.cancelled_at, "") == 0); /* 未取消 */

    /* 查找不存在的ID */
    result = RegistrationRepository_find_by_registration_id(&repository, "REG9999", &loaded);
    assert(result.success == 0);

    /* 查找已取消的记录 */
    result = RegistrationRepository_find_by_registration_id(&repository, "REG0003", &loaded);
    assert(result.success == 1);
    assert(loaded.status == REG_STATUS_CANCELLED);
    assert(strcmp(loaded.cancelled_at, "2026-03-21T10:30") == 0);
}

/**
 * @brief 测试按患者ID查找（支持状态和时间范围过滤）
 *
 * 验证场景：
 * 1. 无过滤条件：PAT0001有2条记录（REG0001和REG0003）
 * 2. 按状态过滤（CANCELLED）：PAT0001只有1条已取消记录
 * 3. 按时间范围过滤：只返回指定时间段内的记录
 * 4. 同时按状态和时间范围过滤
 */
static void test_find_by_patient_id_filters(void) {
    RegistrationRepository repository;
    Registration pending;
    Registration diagnosed;
    Registration cancelled;
    LinkedList matches;
    RegistrationRepositoryFilter filter;
    Result result;
    const Registration *record = 0;

    repository = seed_repository(&pending, &diagnosed, &cancelled);

    /* 无过滤条件查询 PAT0001 */
    LinkedList_init(&matches);
    result = RegistrationRepository_find_by_patient_id(&repository, "PAT0001", 0, &matches);
    assert(result.success == 1);
    assert(LinkedList_count(&matches) == 2); /* PAT0001有2条记录 */
    assert(strcmp(registration_at(&matches, 0)->registration_id, "REG0001") == 0);
    assert(strcmp(registration_at(&matches, 1)->registration_id, "REG0003") == 0);
    RegistrationRepository_clear_list(&matches);

    /* 按状态过滤：只查已取消的 */
    LinkedList_init(&matches);
    RegistrationRepositoryFilter_init(&filter);
    filter.use_status = 1;
    filter.status = REG_STATUS_CANCELLED;
    result = RegistrationRepository_find_by_patient_id(&repository, "PAT0001", &filter, &matches);
    assert(result.success == 1);
    assert(LinkedList_count(&matches) == 1);
    record = registration_at(&matches, 0);
    assert(record != 0);
    assert(record->status == REG_STATUS_CANCELLED);
    assert(strcmp(record->registration_id, "REG0003") == 0);
    RegistrationRepository_clear_list(&matches);

    /* 按时间范围过滤：只查 2026-03-20T08:00 */
    LinkedList_init(&matches);
    RegistrationRepositoryFilter_init(&filter);
    filter.registered_at_from = "2026-03-20T08:00";
    filter.registered_at_to = "2026-03-20T08:00";
    result = RegistrationRepository_find_by_patient_id(&repository, "PAT0001", &filter, &matches);
    assert(result.success == 1);
    assert(LinkedList_count(&matches) == 1);
    record = registration_at(&matches, 0);
    assert(record != 0);
    assert(strcmp(record->registration_id, "REG0001") == 0);
    RegistrationRepository_clear_list(&matches);

    /* 同时按状态和时间范围过滤 */
    LinkedList_init(&matches);
    RegistrationRepositoryFilter_init(&filter);
    filter.use_status = 1;
    filter.status = REG_STATUS_CANCELLED;
    filter.registered_at_from = "2026-03-21T00:00";
    filter.registered_at_to = "2026-03-21T23:59";
    result = RegistrationRepository_find_by_patient_id(&repository, "PAT0001", &filter, &matches);
    assert(result.success == 1);
    assert(LinkedList_count(&matches) == 1);
    record = registration_at(&matches, 0);
    assert(record != 0);
    assert(strcmp(record->cancelled_at, "2026-03-21T10:30") == 0);
    RegistrationRepository_clear_list(&matches);
}

/**
 * @brief 测试全量重写（save_all）
 *
 * 验证流程：
 * 1. 预埋3条记录后，使用save_all重写为2条记录
 * 2. 验证被替换掉的记录查不到
 * 3. 验证新加入的记录可以查到
 * 4. 验证保留的记录按患者ID查询正确
 */
static void test_save_all_rewrites_table(void) {
    RegistrationRepository repository;
    Registration pending;
    Registration diagnosed;
    Registration cancelled;
    Registration replacement;
    Registration loaded;
    LinkedList rewrite_records;
    LinkedList patient_records;
    Result result;

    repository = seed_repository(&pending, &diagnosed, &cancelled);

    /* 构造一条新的替换记录 */
    replacement = make_registration(
        "REG0009",
        "PAT0009",
        "DOC0009",
        "DEP0009",
        "2026-03-22T11:00",
        REG_STATUS_PENDING,
        "",
        ""
    );

    /* 重写为：保留 cancelled + 新增 replacement */
    LinkedList_init(&rewrite_records);
    assert(LinkedList_append(&rewrite_records, clone_registration(&cancelled)) == 1);
    assert(LinkedList_append(&rewrite_records, clone_registration(&replacement)) == 1);

    result = RegistrationRepository_save_all(&repository, &rewrite_records);
    assert(result.success == 1);
    RegistrationRepository_clear_list(&rewrite_records);
    assert_header_written(); /* 验证表头仍正确 */

    /* 被替换掉的 REG0002（diagnosed）查不到了 */
    result = RegistrationRepository_find_by_registration_id(&repository, "REG0002", &loaded);
    assert(result.success == 0);

    /* 新加入的 REG0009 可以查到 */
    result = RegistrationRepository_find_by_registration_id(&repository, "REG0009", &loaded);
    assert(result.success == 1);
    assert(strcmp(loaded.patient_id, "PAT0009") == 0);
    assert(loaded.status == REG_STATUS_PENDING);

    /* PAT0001 现在只剩1条记录（REG0003） */
    LinkedList_init(&patient_records);
    result = RegistrationRepository_find_by_patient_id(&repository, "PAT0001", 0, &patient_records);
    assert(result.success == 1);
    assert(LinkedList_count(&patient_records) == 1);
    assert(strcmp(registration_at(&patient_records, 0)->registration_id, "REG0003") == 0);
    RegistrationRepository_clear_list(&patient_records);
}

/**
 * @brief 测试主函数，依次运行所有挂号仓储测试用例
 */
int main(void) {
    test_append_and_find_by_registration_id();
    test_find_by_patient_id_filters();
    test_save_all_rewrites_table();
    return 0;
}
