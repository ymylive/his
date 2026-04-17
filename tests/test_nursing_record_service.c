/**
 * @file test_nursing_record_service.c
 * @brief 护理记录服务（NursingRecordService）的单元测试文件
 *
 * 本文件测试护理记录管理服务的核心功能，包括：
 * - 创建护理记录并自动生成ID（happy path）
 * - 按住院记录ID查询与读回校验
 * - 缺失必填字段、空查询等错误/边界路径
 * - 大文本内容（接近容量上限）的存取
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "common/LinkedList.h"
#include "domain/NursingRecord.h"
#include "repository/NursingRecordRepository.h"
#include "service/NursingRecordService.h"

/**
 * @brief 辅助函数：构建测试用的临时文件路径
 */
static void TestNursingRecordService_make_path(
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
        "build/test_nursing_record_service_data/%s_%ld_%lu_%ld_%ld.txt",
        test_name,
        (long)time(0),
        (unsigned long)clock(),
        (long)getpid(),
        sequence++
    );
    remove(buffer);
}

/**
 * @brief 辅助函数：构造一个 NursingRecord 对象
 */
static NursingRecord TestNursingRecordService_make_record(
    const char *admission_id,
    const char *nurse_name,
    const char *record_type,
    const char *content,
    const char *recorded_at
) {
    NursingRecord record;

    memset(&record, 0, sizeof(record));
    strcpy(record.admission_id, admission_id);
    strcpy(record.nurse_name, nurse_name);
    strcpy(record.record_type, record_type);
    strcpy(record.content, content);
    strcpy(record.recorded_at, recorded_at);
    return record;
}

/**
 * @brief Happy path：创建护理记录后按住院ID能读回所有字段
 */
static void test_create_and_read_back_nursing_record(void) {
    char path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    NursingRecordService service;
    NursingRecord record;
    LinkedList list;
    NursingRecord *stored;
    Result result;

    TestNursingRecordService_make_path(path, sizeof(path), "create_readback");

    result = NursingRecordService_init(&service, path);
    assert(result.success == 1);

    record = TestNursingRecordService_make_record(
        "ADM0001",
        "Nurse Li",
        "Vitals",
        "Temp 36.8C BP 120/80",
        "2026-04-01T08:00:00"
    );
    result = NursingRecordService_create_record(&service, &record);
    assert(result.success == 1);
    assert(record.nursing_id[0] != '\0');

    LinkedList_init(&list);
    result = NursingRecordService_find_by_admission_id(&service, "ADM0001", &list);
    assert(result.success == 1);
    assert(LinkedList_count(&list) == 1);
    stored = (NursingRecord *)list.head->data;
    assert(strcmp(stored->nursing_id, record.nursing_id) == 0);
    assert(strcmp(stored->admission_id, "ADM0001") == 0);
    assert(strcmp(stored->nurse_name, "Nurse Li") == 0);
    assert(strcmp(stored->record_type, "Vitals") == 0);
    assert(strcmp(stored->content, "Temp 36.8C BP 120/80") == 0);
    assert(strcmp(stored->recorded_at, "2026-04-01T08:00:00") == 0);
    NursingRecordService_clear_results(&list);
}

/**
 * @brief 错误路径：缺失必填字段应创建失败
 */
static void test_create_rejects_missing_required_fields(void) {
    char path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    NursingRecordService service;
    NursingRecord record;
    Result result;

    TestNursingRecordService_make_path(path, sizeof(path), "missing_fields");

    result = NursingRecordService_init(&service, path);
    assert(result.success == 1);

    /* admission_id 缺失 */
    record = TestNursingRecordService_make_record("", "Nurse", "Vitals", "ok", "2026-04-02T09:00:00");
    result = NursingRecordService_create_record(&service, &record);
    assert(result.success == 0);

    /* nurse_name 缺失 */
    record = TestNursingRecordService_make_record("ADM1001", "", "Vitals", "ok", "2026-04-02T09:00:00");
    result = NursingRecordService_create_record(&service, &record);
    assert(result.success == 0);

    /* record_type 缺失 */
    record = TestNursingRecordService_make_record("ADM1001", "Nurse", "", "ok", "2026-04-02T09:00:00");
    result = NursingRecordService_create_record(&service, &record);
    assert(result.success == 0);

    /* content 缺失 */
    record = TestNursingRecordService_make_record("ADM1001", "Nurse", "Vitals", "", "2026-04-02T09:00:00");
    result = NursingRecordService_create_record(&service, &record);
    assert(result.success == 0);

    /* recorded_at 缺失 */
    record = TestNursingRecordService_make_record("ADM1001", "Nurse", "Vitals", "ok", "");
    result = NursingRecordService_create_record(&service, &record);
    assert(result.success == 0);
}

/**
 * @brief 错误路径：包含行分隔符（\\n/\\r）等保留字符的字段应被拒绝
 */
static void test_create_rejects_reserved_characters(void) {
    char path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    NursingRecordService service;
    NursingRecord record;
    Result result;

    TestNursingRecordService_make_path(path, sizeof(path), "reserved_chars");

    result = NursingRecordService_init(&service, path);
    assert(result.success == 1);

    /* content 中包含换行符（行分隔符），应被校验拒绝 */
    record = TestNursingRecordService_make_record(
        "ADM2001", "Nurse", "Vitals", "bad\ncontent", "2026-04-03T09:00:00"
    );
    result = NursingRecordService_create_record(&service, &record);
    assert(result.success == 0);

    /* nurse_name 中包含回车符 */
    record = TestNursingRecordService_make_record(
        "ADM2001", "Nurse\rX", "Vitals", "ok", "2026-04-03T09:00:00"
    );
    result = NursingRecordService_create_record(&service, &record);
    assert(result.success == 0);
}

/**
 * @brief 边界条件：按不存在的住院ID查询返回空链表；自动生成ID按顺序递增
 */
static void test_find_empty_and_multiple_records(void) {
    char path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    NursingRecordService service;
    NursingRecord record;
    LinkedList list;
    LinkedList all;
    char first_id[HIS_DOMAIN_ID_CAPACITY];
    char second_id[HIS_DOMAIN_ID_CAPACITY];
    Result result;

    TestNursingRecordService_make_path(path, sizeof(path), "empty_query");

    result = NursingRecordService_init(&service, path);
    assert(result.success == 1);

    /* 未插入前查询为空 */
    LinkedList_init(&list);
    result = NursingRecordService_find_by_admission_id(&service, "ADM_NONE", &list);
    assert(result.success == 1);
    assert(LinkedList_count(&list) == 0);
    NursingRecordService_clear_results(&list);

    /* 插入两条，验证 ID 自动生成且不同 */
    record = TestNursingRecordService_make_record(
        "ADM3001", "Nurse A", "Vitals", "first", "2026-04-04T08:00:00"
    );
    result = NursingRecordService_create_record(&service, &record);
    assert(result.success == 1);
    strcpy(first_id, record.nursing_id);

    record = TestNursingRecordService_make_record(
        "ADM3001", "Nurse B", "Medication", "second", "2026-04-04T09:00:00"
    );
    result = NursingRecordService_create_record(&service, &record);
    assert(result.success == 1);
    strcpy(second_id, record.nursing_id);
    assert(strcmp(first_id, second_id) != 0);

    /* 按住院ID查询应得到两条 */
    LinkedList_init(&list);
    result = NursingRecordService_find_by_admission_id(&service, "ADM3001", &list);
    assert(result.success == 1);
    assert(LinkedList_count(&list) == 2);
    NursingRecordService_clear_results(&list);

    /* load_all 应得到两条 */
    LinkedList_init(&all);
    result = NursingRecordService_load_all(&service, &all);
    assert(result.success == 1);
    assert(LinkedList_count(&all) == 2);
    NursingRecordService_clear_results(&all);
}

/**
 * @brief 边界条件：极长 content（接近大文本上限）可成功存取
 */
static void test_create_accepts_large_content(void) {
    char path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    NursingRecordService service;
    NursingRecord record;
    LinkedList list;
    NursingRecord *stored;
    char large_content[HIS_DOMAIN_LARGE_TEXT_CAPACITY];
    size_t i;
    Result result;

    TestNursingRecordService_make_path(path, sizeof(path), "large_content");

    result = NursingRecordService_init(&service, path);
    assert(result.success == 1);

    /* 构造近上限的内容（留出终止符） */
    for (i = 0; i + 1 < sizeof(large_content); ++i) {
        large_content[i] = 'a' + (int)(i % 26);
    }
    large_content[sizeof(large_content) - 1] = '\0';

    record = TestNursingRecordService_make_record(
        "ADM4001", "Nurse X", "Observation", large_content, "2026-04-05T08:00:00"
    );
    result = NursingRecordService_create_record(&service, &record);
    assert(result.success == 1);

    LinkedList_init(&list);
    result = NursingRecordService_find_by_admission_id(&service, "ADM4001", &list);
    assert(result.success == 1);
    assert(LinkedList_count(&list) == 1);
    stored = (NursingRecord *)list.head->data;
    assert(strcmp(stored->content, large_content) == 0);
    NursingRecordService_clear_results(&list);
}

/**
 * @brief 测试主函数
 */
int main(void) {
    test_create_and_read_back_nursing_record();
    test_create_rejects_missing_required_fields();
    test_create_rejects_reserved_characters();
    test_find_empty_and_multiple_records();
    test_create_accepts_large_content();
    return 0;
}
