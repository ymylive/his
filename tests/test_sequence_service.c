/**
 * @file test_sequence_service.c
 * @brief SequenceService 单元测试
 *
 * 覆盖范围：
 * 1. 首次 init 无状态文件：next 返回 1
 * 2. 重启 service：新实例 next 返回 2（持久化生效）
 * 3. 迁移扫描：现有数据文件中 "PAT0005" 导致 next(PATIENT) 返回 6
 * 4. 未知类型调用 next 返回 failure
 * 5. sequences.txt 权限为 0600（POSIX）
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <sys/stat.h>

#include "service/SequenceService.h"

/** 测试用临时路径前缀 */
#define TEST_DIR "build/test_sequence_service_data"

static void build_test_path(char *buffer, size_t capacity, const char *name) {
    snprintf(
        buffer,
        capacity,
        "%s/%ld_%lu_%s",
        TEST_DIR,
        (long)time(0),
        (unsigned long)clock(),
        name
    );
}

/**
 * @brief 手动写入一个 "patients.txt" 种子文件，用于迁移扫描测试
 */
static void seed_patients_file(const char *path) {
    FILE *f = fopen(path, "w");
    assert(f != NULL);
    /* 写入表头 + 几行；最大 PAT 序号为 0005 */
    fprintf(f, "patient_id|name|gender|age\n");
    fprintf(f, "PAT0001|Alice|2|28\n");
    fprintf(f, "PAT0003|Carol|2|41\n");
    fprintf(f, "PAT0005|Eve|1|19\n");
    fclose(f);
}

/**
 * @brief 测试 1：无文件 + 无种子 → next 返回 1；关闭后重开 → next 返回 2
 */
static void test_init_and_next_without_file(void) {
    SequenceService svc;
    SequenceService svc_reopen;
    SequenceTypeSpec specs[1];
    char seq_path[512];
    char patient_path[512];
    int value = 0;
    Result result;

    build_test_path(seq_path, sizeof(seq_path), "sequences_basic.txt");
    build_test_path(patient_path, sizeof(patient_path), "patients_basic.txt");

    /* 确保文件不存在（父目录首次可能也没建） */
    remove(seq_path);
    /* patient file不存在时 scan 应跳过，不会报错 */

    memset(specs, 0, sizeof(specs));
    snprintf(specs[0].type_name, sizeof(specs[0].type_name), "PATIENT");
    snprintf(specs[0].id_prefix, sizeof(specs[0].id_prefix), "PAT");
    snprintf(specs[0].data_path, sizeof(specs[0].data_path), "%s", patient_path);

    result = SequenceService_init(&svc, seq_path, specs, 1);
    assert(result.success == 1);

    result = SequenceService_next(&svc, "PATIENT", &value);
    assert(result.success == 1);
    assert(value == 1);

    /* 重启：新 service 读取同一个 sequences.txt */
    result = SequenceService_init(&svc_reopen, seq_path, specs, 1);
    assert(result.success == 1);

    result = SequenceService_next(&svc_reopen, "PATIENT", &value);
    assert(result.success == 1);
    assert(value == 2); /* 上次 +1 已持久化，这次应为 2 */

    printf("PASS: test_init_and_next_without_file\n");
}

/**
 * @brief 测试 2：既有 patients.txt 含 PAT0005 → 第一次 next 应返回 6
 */
static void test_migration_scan_from_existing_data(void) {
    SequenceService svc;
    SequenceTypeSpec specs[1];
    char seq_path[512];
    char patient_path[512];
    int value = 0;
    Result result;

    build_test_path(seq_path, sizeof(seq_path), "sequences_migration.txt");
    build_test_path(patient_path, sizeof(patient_path), "patients_migration.txt");

    /* 确保 sequences.txt 不存在以触发迁移，写入 patients seed */
    remove(seq_path);
    seed_patients_file(patient_path);

    memset(specs, 0, sizeof(specs));
    snprintf(specs[0].type_name, sizeof(specs[0].type_name), "PATIENT");
    snprintf(specs[0].id_prefix, sizeof(specs[0].id_prefix), "PAT");
    snprintf(specs[0].data_path, sizeof(specs[0].data_path), "%s", patient_path);

    result = SequenceService_init(&svc, seq_path, specs, 1);
    assert(result.success == 1);

    result = SequenceService_peek(&svc, "PATIENT", &value);
    assert(result.success == 1);
    assert(value == 5); /* 扫描得到 PAT0005 */

    result = SequenceService_next(&svc, "PATIENT", &value);
    assert(result.success == 1);
    assert(value == 6); /* max+1 */

    printf("PASS: test_migration_scan_from_existing_data\n");
}

/**
 * @brief 测试 3：未注册的类型调用 next 返回 failure
 */
static void test_unknown_type_returns_failure(void) {
    SequenceService svc;
    SequenceTypeSpec specs[1];
    char seq_path[512];
    int value = 0;
    Result result;

    build_test_path(seq_path, sizeof(seq_path), "sequences_unknown.txt");
    remove(seq_path);

    memset(specs, 0, sizeof(specs));
    snprintf(specs[0].type_name, sizeof(specs[0].type_name), "REGISTRATION");
    snprintf(specs[0].id_prefix, sizeof(specs[0].id_prefix), "REG");

    result = SequenceService_init(&svc, seq_path, specs, 1);
    assert(result.success == 1);

    result = SequenceService_next(&svc, "DOCTOR", &value);
    assert(result.success == 0); /* DOCTOR 未注册 */

    printf("PASS: test_unknown_type_returns_failure\n");
}

/**
 * @brief 测试 4：init/next 后 sequences.txt 的权限为 0600 (POSIX)
 */
static void test_sequences_file_mode_is_0600(void) {
#if defined(_WIN32)
    /* Windows 下 POSIX 权限位语义不同，跳过 */
    printf("SKIP (Windows): test_sequences_file_mode_is_0600\n");
    return;
#else
    SequenceService svc;
    SequenceTypeSpec specs[1];
    char seq_path[512];
    struct stat info;
    int value = 0;
    Result result;

    build_test_path(seq_path, sizeof(seq_path), "sequences_mode.txt");
    remove(seq_path);

    memset(specs, 0, sizeof(specs));
    snprintf(specs[0].type_name, sizeof(specs[0].type_name), "REGISTRATION");
    snprintf(specs[0].id_prefix, sizeof(specs[0].id_prefix), "REG");

    result = SequenceService_init(&svc, seq_path, specs, 1);
    assert(result.success == 1);

    assert(stat(seq_path, &info) == 0);
    assert((info.st_mode & 0777) == 0600);

    result = SequenceService_next(&svc, "REGISTRATION", &value);
    assert(result.success == 1);
    assert(value == 1);

    assert(stat(seq_path, &info) == 0);
    assert((info.st_mode & 0777) == 0600);

    printf("PASS: test_sequences_file_mode_is_0600\n");
#endif
}

/**
 * @brief 测试 5：序列号分配连续（1..N），重启后从 N+1 继续
 */
static void test_sequence_monotonic_across_restart(void) {
    SequenceService svc;
    SequenceService svc_reopen;
    SequenceTypeSpec specs[1];
    char seq_path[512];
    int value = 0;
    int i;
    Result result;

    build_test_path(seq_path, sizeof(seq_path), "sequences_monotonic.txt");
    remove(seq_path);

    memset(specs, 0, sizeof(specs));
    snprintf(specs[0].type_name, sizeof(specs[0].type_name), "REGISTRATION");
    snprintf(specs[0].id_prefix, sizeof(specs[0].id_prefix), "REG");

    result = SequenceService_init(&svc, seq_path, specs, 1);
    assert(result.success == 1);

    for (i = 1; i <= 5; i++) {
        result = SequenceService_next(&svc, "REGISTRATION", &value);
        assert(result.success == 1);
        assert(value == i);
    }

    result = SequenceService_init(&svc_reopen, seq_path, specs, 1);
    assert(result.success == 1);

    result = SequenceService_next(&svc_reopen, "REGISTRATION", &value);
    assert(result.success == 1);
    assert(value == 6);

    printf("PASS: test_sequence_monotonic_across_restart\n");
}

int main(void) {
    test_init_and_next_without_file();
    test_migration_scan_from_existing_data();
    test_unknown_type_returns_failure();
    test_sequences_file_mode_is_0600();
    test_sequence_monotonic_across_restart();
    printf("ALL SEQUENCE SERVICE TESTS PASSED\n");
    return 0;
}
