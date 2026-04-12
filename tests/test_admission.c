/**
 * @file test_admission.c
 * @brief 入院记录领域模型（Admission）的单元测试文件
 *
 * 本文件测试入院记录领域实体的核心行为，包括：
 * - Admission_is_active() 住院状态判断
 * - Admission_discharge() 出院处理
 * - 重复出院的拒绝
 * - 各种边界和异常参数的处理
 */

#include <assert.h>
#include <string.h>

#include "domain/Admission.h"

/**
 * @brief 辅助函数：构造一个 Admission（入院记录）对象
 */
static Admission make_admission(
    const char *admission_id,
    const char *patient_id,
    const char *ward_id,
    const char *bed_id,
    const char *admitted_at,
    AdmissionStatus status,
    const char *discharged_at,
    const char *summary
) {
    Admission admission;

    memset(&admission, 0, sizeof(admission));
    strncpy(admission.admission_id, admission_id, sizeof(admission.admission_id) - 1);
    strncpy(admission.patient_id, patient_id, sizeof(admission.patient_id) - 1);
    strncpy(admission.ward_id, ward_id, sizeof(admission.ward_id) - 1);
    strncpy(admission.bed_id, bed_id, sizeof(admission.bed_id) - 1);
    strncpy(admission.admitted_at, admitted_at, sizeof(admission.admitted_at) - 1);
    admission.status = status;
    if (discharged_at != 0 && discharged_at[0] != '\0') {
        strncpy(admission.discharged_at, discharged_at, sizeof(admission.discharged_at) - 1);
    }
    if (summary != 0 && summary[0] != '\0') {
        strncpy(admission.summary, summary, sizeof(admission.summary) - 1);
    }
    return admission;
}

/**
 * @brief 测试 Admission_is_active：住院中状态返回1
 */
static void test_is_active_returns_true_for_active(void) {
    Admission admission = make_admission(
        "ADM0001", "PAT0001", "WRD0001", "BED0001",
        "2026-03-20T08:00", ADMISSION_STATUS_ACTIVE, "", "observation"
    );

    assert(Admission_is_active(&admission) == 1);
}

/**
 * @brief 测试 Admission_is_active：已出院状态返回0
 */
static void test_is_active_returns_false_for_discharged(void) {
    Admission admission = make_admission(
        "ADM0001", "PAT0001", "WRD0001", "BED0001",
        "2026-03-20T08:00", ADMISSION_STATUS_DISCHARGED,
        "2026-03-23T10:00", "discharged normally"
    );

    assert(Admission_is_active(&admission) == 0);
}

/**
 * @brief 测试 Admission_is_active：NULL指针返回0
 */
static void test_is_active_null_returns_false(void) {
    assert(Admission_is_active(0) == 0);
}

/**
 * @brief 测试 Admission_discharge：正常出院成功
 */
static void test_discharge_success(void) {
    Admission admission = make_admission(
        "ADM0001", "PAT0001", "WRD0001", "BED0001",
        "2026-03-20T08:00", ADMISSION_STATUS_ACTIVE, "", "initial admission"
    );

    assert(Admission_discharge(&admission, "2026-03-23T10:00") == 1);
    assert(admission.status == ADMISSION_STATUS_DISCHARGED);
    assert(strcmp(admission.discharged_at, "2026-03-23T10:00") == 0);
}

/**
 * @brief 测试 Admission_discharge：重复出院被拒绝
 */
static void test_discharge_rejects_already_discharged(void) {
    Admission admission = make_admission(
        "ADM0001", "PAT0001", "WRD0001", "BED0001",
        "2026-03-20T08:00", ADMISSION_STATUS_ACTIVE, "", "initial admission"
    );

    /* 第一次出院成功 */
    assert(Admission_discharge(&admission, "2026-03-23T10:00") == 1);
    assert(admission.status == ADMISSION_STATUS_DISCHARGED);

    /* 第二次出院失败 */
    assert(Admission_discharge(&admission, "2026-03-24T10:00") == 0);
    assert(admission.status == ADMISSION_STATUS_DISCHARGED); /* 状态不变 */
    assert(strcmp(admission.discharged_at, "2026-03-23T10:00") == 0); /* 时间不变 */
}

/**
 * @brief 测试 Admission_discharge：NULL 出院时间被拒绝
 */
static void test_discharge_rejects_null_time(void) {
    Admission admission = make_admission(
        "ADM0001", "PAT0001", "WRD0001", "BED0001",
        "2026-03-20T08:00", ADMISSION_STATUS_ACTIVE, "", "admission"
    );

    assert(Admission_discharge(&admission, 0) == 0);
    assert(admission.status == ADMISSION_STATUS_ACTIVE); /* 状态不变 */
}

/**
 * @brief 测试 Admission_discharge：空字符串出院时间被拒绝
 */
static void test_discharge_rejects_empty_time(void) {
    Admission admission = make_admission(
        "ADM0001", "PAT0001", "WRD0001", "BED0001",
        "2026-03-20T08:00", ADMISSION_STATUS_ACTIVE, "", "admission"
    );

    assert(Admission_discharge(&admission, "") == 0);
    assert(admission.status == ADMISSION_STATUS_ACTIVE);
}

/**
 * @brief 测试 Admission_discharge：NULL 入院记录指针被拒绝
 */
static void test_discharge_null_admission_returns_zero(void) {
    assert(Admission_discharge(0, "2026-03-23T10:00") == 0);
}

/**
 * @brief 测试入院记录字段验证：创建后字段值正确
 */
static void test_admission_fields_are_correct(void) {
    Admission admission = make_admission(
        "ADM0001", "PAT0001", "WRD0001", "BED0001",
        "2026-03-20T08:00", ADMISSION_STATUS_ACTIVE, "", "for observation"
    );

    assert(strcmp(admission.admission_id, "ADM0001") == 0);
    assert(strcmp(admission.patient_id, "PAT0001") == 0);
    assert(strcmp(admission.ward_id, "WRD0001") == 0);
    assert(strcmp(admission.bed_id, "BED0001") == 0);
    assert(strcmp(admission.admitted_at, "2026-03-20T08:00") == 0);
    assert(admission.status == ADMISSION_STATUS_ACTIVE);
    assert(admission.discharged_at[0] == '\0');
    assert(strcmp(admission.summary, "for observation") == 0);
}

/**
 * @brief 测试出院后入院记录的所有状态一致性
 *
 * 出院后：status 变为 DISCHARGED，discharged_at 被填写，
 * 其他字段（patient_id、ward_id 等）保持不变。
 */
static void test_discharge_preserves_other_fields(void) {
    Admission admission = make_admission(
        "ADM0002", "PAT0002", "WRD0002", "BED0002",
        "2026-04-01T09:00", ADMISSION_STATUS_ACTIVE, "", "routine check"
    );

    assert(Admission_discharge(&admission, "2026-04-05T14:00") == 1);

    /* 验证出院后其他字段未被修改 */
    assert(strcmp(admission.admission_id, "ADM0002") == 0);
    assert(strcmp(admission.patient_id, "PAT0002") == 0);
    assert(strcmp(admission.ward_id, "WRD0002") == 0);
    assert(strcmp(admission.bed_id, "BED0002") == 0);
    assert(strcmp(admission.admitted_at, "2026-04-01T09:00") == 0);
    assert(strcmp(admission.summary, "routine check") == 0);
    assert(admission.status == ADMISSION_STATUS_DISCHARGED);
    assert(strcmp(admission.discharged_at, "2026-04-05T14:00") == 0);
}

/**
 * @brief 测试从 DISCHARGED 状态构造的入院记录，is_active 返回0
 */
static void test_created_as_discharged_is_not_active(void) {
    Admission admission = make_admission(
        "ADM0003", "PAT0003", "WRD0001", "BED0003",
        "2026-01-10T08:00", ADMISSION_STATUS_DISCHARGED,
        "2026-01-15T10:00", "completed"
    );

    assert(Admission_is_active(&admission) == 0);
    assert(Admission_discharge(&admission, "2026-01-20T10:00") == 0); /* 不可再出院 */
}

/**
 * @brief 测试主函数，依次运行所有入院记录领域模型测试用例
 */
int main(void) {
    test_is_active_returns_true_for_active();
    test_is_active_returns_false_for_discharged();
    test_is_active_null_returns_false();
    test_discharge_success();
    test_discharge_rejects_already_discharged();
    test_discharge_rejects_null_time();
    test_discharge_rejects_empty_time();
    test_discharge_null_admission_returns_zero();
    test_admission_fields_are_correct();
    test_discharge_preserves_other_fields();
    test_created_as_discharged_is_not_active();
    return 0;
}
