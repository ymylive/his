/**
 * @file test_round_record_service.c
 * @brief 查房记录服务（RoundRecordService）的单元测试文件
 *
 * 本文件测试查房记录管理服务的核心功能，包括：
 * - 创建查房记录并自动生成ID（happy path）
 * - 按住院记录ID查询与读回校验
 * - 缺失必填字段、保留字符等错误路径
 * - 空查询、多条记录过滤等边界条件
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "common/LinkedList.h"
#include "domain/RoundRecord.h"
#include "repository/RoundRecordRepository.h"
#include "service/RoundRecordService.h"

/**
 * @brief 辅助函数：构建测试用的临时文件路径
 */
static void TestRoundRecordService_make_path(
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
        "build/test_round_record_service_data/%s_%ld_%lu_%ld_%ld.txt",
        test_name,
        (long)time(0),
        (unsigned long)clock(),
        (long)getpid(),
        sequence++
    );
    remove(buffer);
}

/**
 * @brief 辅助函数：构造一个 RoundRecord 对象
 */
static RoundRecord TestRoundRecordService_make_record(
    const char *admission_id,
    const char *doctor_id,
    const char *findings,
    const char *plan,
    const char *rounded_at
) {
    RoundRecord record;

    memset(&record, 0, sizeof(record));
    strcpy(record.admission_id, admission_id);
    strcpy(record.doctor_id, doctor_id);
    strcpy(record.findings, findings);
    strcpy(record.plan, plan);
    strcpy(record.rounded_at, rounded_at);
    return record;
}

/**
 * @brief Happy path：创建查房记录后按住院ID能读回所有字段
 */
static void test_create_and_read_back_round_record(void) {
    char path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    RoundRecordService service;
    RoundRecord record;
    LinkedList list;
    RoundRecord *stored;
    Result result;

    TestRoundRecordService_make_path(path, sizeof(path), "create_readback");

    result = RoundRecordService_init(&service, path);
    assert(result.success == 1);

    record = TestRoundRecordService_make_record(
        "ADM0001",
        "DOC0001",
        "Patient stable, pain reduced",
        "Continue current regimen",
        "2026-04-01T09:00:00"
    );
    result = RoundRecordService_create_record(&service, &record);
    assert(result.success == 1);
    assert(record.round_id[0] != '\0');

    result = RoundRecordService_find_by_admission_id(&service, "ADM0001", &list);
    assert(result.success == 1);
    assert(LinkedList_count(&list) == 1);
    stored = (RoundRecord *)list.head->data;
    assert(strcmp(stored->round_id, record.round_id) == 0);
    assert(strcmp(stored->admission_id, "ADM0001") == 0);
    assert(strcmp(stored->doctor_id, "DOC0001") == 0);
    assert(strcmp(stored->findings, "Patient stable, pain reduced") == 0);
    assert(strcmp(stored->plan, "Continue current regimen") == 0);
    assert(strcmp(stored->rounded_at, "2026-04-01T09:00:00") == 0);
    RoundRecordService_clear_results(&list);
}

/**
 * @brief 错误路径：缺失必填字段应创建失败
 */
static void test_create_rejects_missing_required_fields(void) {
    char path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    RoundRecordService service;
    RoundRecord record;
    Result result;

    TestRoundRecordService_make_path(path, sizeof(path), "missing_fields");

    result = RoundRecordService_init(&service, path);
    assert(result.success == 1);

    /* admission_id 缺失 */
    record = TestRoundRecordService_make_record("", "DOC1001", "ok", "plan", "2026-04-02T09:00:00");
    result = RoundRecordService_create_record(&service, &record);
    assert(result.success == 0);

    /* doctor_id 缺失 */
    record = TestRoundRecordService_make_record("ADM1001", "", "ok", "plan", "2026-04-02T09:00:00");
    result = RoundRecordService_create_record(&service, &record);
    assert(result.success == 0);

    /* findings 缺失 */
    record = TestRoundRecordService_make_record("ADM1001", "DOC1001", "", "plan", "2026-04-02T09:00:00");
    result = RoundRecordService_create_record(&service, &record);
    assert(result.success == 0);

    /* plan 缺失 */
    record = TestRoundRecordService_make_record("ADM1001", "DOC1001", "ok", "", "2026-04-02T09:00:00");
    result = RoundRecordService_create_record(&service, &record);
    assert(result.success == 0);

    /* rounded_at 缺失 */
    record = TestRoundRecordService_make_record("ADM1001", "DOC1001", "ok", "plan", "");
    result = RoundRecordService_create_record(&service, &record);
    assert(result.success == 0);
}

/**
 * @brief 错误路径：包含行分隔符（\\n/\\r）的字段应被拒绝（保留字符）
 */
static void test_create_rejects_reserved_characters(void) {
    char path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    RoundRecordService service;
    RoundRecord record;
    Result result;

    TestRoundRecordService_make_path(path, sizeof(path), "reserved_chars");

    result = RoundRecordService_init(&service, path);
    assert(result.success == 1);

    record = TestRoundRecordService_make_record(
        "ADM2001", "DOC2001", "bad\nfindings", "plan", "2026-04-03T09:00:00"
    );
    result = RoundRecordService_create_record(&service, &record);
    assert(result.success == 0);

    record = TestRoundRecordService_make_record(
        "ADM2001", "DOC2001", "findings", "bad\rplan", "2026-04-03T09:00:00"
    );
    result = RoundRecordService_create_record(&service, &record);
    assert(result.success == 0);
}

/**
 * @brief 边界条件：空查询返回空链表；多条记录按住院ID正确筛选
 */
static void test_find_empty_and_filter_by_admission(void) {
    char path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    RoundRecordService service;
    RoundRecord record;
    LinkedList list;
    LinkedList all;
    Result result;

    TestRoundRecordService_make_path(path, sizeof(path), "filter");

    result = RoundRecordService_init(&service, path);
    assert(result.success == 1);

    /* 空仓储 */
    result = RoundRecordService_find_by_admission_id(&service, "ADM_NONE", &list);
    assert(result.success == 1);
    assert(LinkedList_count(&list) == 0);
    RoundRecordService_clear_results(&list);

    /* 插入三条：两条 ADM3001，一条 ADM3099 */
    record = TestRoundRecordService_make_record(
        "ADM3001", "DOC3001", "Day1 obs", "Continue", "2026-04-04T08:00:00"
    );
    result = RoundRecordService_create_record(&service, &record);
    assert(result.success == 1);

    record = TestRoundRecordService_make_record(
        "ADM3001", "DOC3001", "Day2 obs", "Adjust dose", "2026-04-05T08:00:00"
    );
    result = RoundRecordService_create_record(&service, &record);
    assert(result.success == 1);

    record = TestRoundRecordService_make_record(
        "ADM3099", "DOC3099", "Other", "plan", "2026-04-05T09:00:00"
    );
    result = RoundRecordService_create_record(&service, &record);
    assert(result.success == 1);

    result = RoundRecordService_find_by_admission_id(&service, "ADM3001", &list);
    assert(result.success == 1);
    assert(LinkedList_count(&list) == 2);
    RoundRecordService_clear_results(&list);

    result = RoundRecordService_load_all(&service, &all);
    assert(result.success == 1);
    assert(LinkedList_count(&all) == 3);
    RoundRecordService_clear_results(&all);
}

/**
 * @brief 边界条件：极长 findings/plan 字段（接近大文本容量上限）可成功存取
 */
static void test_create_accepts_large_text_fields(void) {
    char path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    RoundRecordService service;
    RoundRecord record;
    LinkedList list;
    RoundRecord *stored;
    char large_text[HIS_DOMAIN_LARGE_TEXT_CAPACITY];
    size_t i;
    Result result;

    TestRoundRecordService_make_path(path, sizeof(path), "large_text");

    result = RoundRecordService_init(&service, path);
    assert(result.success == 1);

    for (i = 0; i + 1 < sizeof(large_text); ++i) {
        large_text[i] = 'A' + (int)(i % 26);
    }
    large_text[sizeof(large_text) - 1] = '\0';

    record = TestRoundRecordService_make_record(
        "ADM4001", "DOC4001", large_text, "plan_ok", "2026-04-06T10:00:00"
    );
    result = RoundRecordService_create_record(&service, &record);
    assert(result.success == 1);

    result = RoundRecordService_find_by_admission_id(&service, "ADM4001", &list);
    assert(result.success == 1);
    assert(LinkedList_count(&list) == 1);
    stored = (RoundRecord *)list.head->data;
    assert(strcmp(stored->findings, large_text) == 0);
    assert(strcmp(stored->plan, "plan_ok") == 0);
    RoundRecordService_clear_results(&list);
}

/**
 * @brief 测试主函数
 */
int main(void) {
    test_create_and_read_back_round_record();
    test_create_rejects_missing_required_fields();
    test_create_rejects_reserved_characters();
    test_find_empty_and_filter_by_admission();
    test_create_accepts_large_text_fields();
    return 0;
}
