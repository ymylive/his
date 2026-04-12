/**
 * @file test_ward.c
 * @brief 病房领域模型（Ward）的单元测试文件
 *
 * 本文件测试病房领域实体的核心行为，包括：
 * - Ward_has_capacity() 容量判断及边界条件
 * - Ward_occupy_bed() 占用床位
 * - Ward_release_bed() 释放床位
 * - Ward_close() / Ward_open() 病房状态流转
 * - 各种边界和异常参数的处理
 */

#include <assert.h>
#include <string.h>

#include "domain/Ward.h"

/**
 * @brief 辅助函数：构造一个 Ward（病区）对象
 */
static Ward make_ward(
    const char *ward_id,
    const char *name,
    const char *department_id,
    const char *location,
    int capacity,
    int occupied_beds,
    WardStatus status
) {
    Ward ward;

    memset(&ward, 0, sizeof(ward));
    strncpy(ward.ward_id, ward_id, sizeof(ward.ward_id) - 1);
    strncpy(ward.name, name, sizeof(ward.name) - 1);
    strncpy(ward.department_id, department_id, sizeof(ward.department_id) - 1);
    strncpy(ward.location, location, sizeof(ward.location) - 1);
    ward.capacity = capacity;
    ward.occupied_beds = occupied_beds;
    ward.status = status;
    return ward;
}

/**
 * @brief 测试 Ward_has_capacity：正常有剩余容量
 */
static void test_has_capacity_when_not_full(void) {
    Ward ward = make_ward("WRD0001", "Ward A", "DEP0001", "Floor 5", 10, 3, WARD_STATUS_ACTIVE);

    assert(Ward_has_capacity(&ward) == 1);
}

/**
 * @brief 测试 Ward_has_capacity：occupied = 0 时有容量
 */
static void test_has_capacity_when_empty(void) {
    Ward ward = make_ward("WRD0001", "Ward A", "DEP0001", "Floor 5", 5, 0, WARD_STATUS_ACTIVE);

    assert(Ward_has_capacity(&ward) == 1);
}

/**
 * @brief 测试 Ward_has_capacity：occupied = capacity 时无容量
 */
static void test_has_no_capacity_when_full(void) {
    Ward ward = make_ward("WRD0001", "Ward A", "DEP0001", "Floor 5", 3, 3, WARD_STATUS_ACTIVE);

    assert(Ward_has_capacity(&ward) == 0);
}

/**
 * @brief 测试 Ward_has_capacity：capacity = 0 时无容量
 */
static void test_has_no_capacity_when_zero_capacity(void) {
    Ward ward = make_ward("WRD0001", "Ward A", "DEP0001", "Floor 5", 0, 0, WARD_STATUS_ACTIVE);

    assert(Ward_has_capacity(&ward) == 0);
}

/**
 * @brief 测试 Ward_has_capacity：关闭状态无容量
 */
static void test_has_no_capacity_when_closed(void) {
    Ward ward = make_ward("WRD0001", "Ward A", "DEP0001", "Floor 5", 10, 0, WARD_STATUS_CLOSED);

    assert(Ward_has_capacity(&ward) == 0);
}

/**
 * @brief 测试 Ward_has_capacity：NULL指针返回0
 */
static void test_has_capacity_null_returns_zero(void) {
    assert(Ward_has_capacity(0) == 0);
}

/**
 * @brief 测试 Ward_has_capacity：负数 occupied_beds 视为无容量
 */
static void test_has_no_capacity_when_negative_occupied(void) {
    Ward ward = make_ward("WRD0001", "Ward A", "DEP0001", "Floor 5", 5, -1, WARD_STATUS_ACTIVE);

    assert(Ward_has_capacity(&ward) == 0);
}

/**
 * @brief 测试 Ward_occupy_bed：正常占用
 */
static void test_occupy_bed_success(void) {
    Ward ward = make_ward("WRD0001", "Ward A", "DEP0001", "Floor 5", 3, 0, WARD_STATUS_ACTIVE);

    assert(Ward_occupy_bed(&ward) == 1);
    assert(ward.occupied_beds == 1);
    assert(Ward_occupy_bed(&ward) == 1);
    assert(ward.occupied_beds == 2);
    assert(Ward_occupy_bed(&ward) == 1);
    assert(ward.occupied_beds == 3);
}

/**
 * @brief 测试 Ward_occupy_bed：满员时失败
 */
static void test_occupy_bed_fails_when_full(void) {
    Ward ward = make_ward("WRD0001", "Ward A", "DEP0001", "Floor 5", 2, 2, WARD_STATUS_ACTIVE);

    assert(Ward_occupy_bed(&ward) == 0);
    assert(ward.occupied_beds == 2); /* 不变 */
}

/**
 * @brief 测试 Ward_occupy_bed：关闭状态时失败
 */
static void test_occupy_bed_fails_when_closed(void) {
    Ward ward = make_ward("WRD0001", "Ward A", "DEP0001", "Floor 5", 10, 0, WARD_STATUS_CLOSED);

    assert(Ward_occupy_bed(&ward) == 0);
    assert(ward.occupied_beds == 0);
}

/**
 * @brief 测试 Ward_occupy_bed：NULL指针返回0
 */
static void test_occupy_bed_null_returns_zero(void) {
    assert(Ward_occupy_bed(0) == 0);
}

/**
 * @brief 测试 Ward_release_bed：正常释放
 */
static void test_release_bed_success(void) {
    Ward ward = make_ward("WRD0001", "Ward A", "DEP0001", "Floor 5", 5, 3, WARD_STATUS_ACTIVE);

    assert(Ward_release_bed(&ward) == 1);
    assert(ward.occupied_beds == 2);
    assert(Ward_release_bed(&ward) == 1);
    assert(ward.occupied_beds == 1);
}

/**
 * @brief 测试 Ward_release_bed：occupied = 0 时失败
 */
static void test_release_bed_fails_when_zero_occupied(void) {
    Ward ward = make_ward("WRD0001", "Ward A", "DEP0001", "Floor 5", 5, 0, WARD_STATUS_ACTIVE);

    assert(Ward_release_bed(&ward) == 0);
    assert(ward.occupied_beds == 0); /* 不变 */
}

/**
 * @brief 测试 Ward_release_bed：NULL指针返回0
 */
static void test_release_bed_null_returns_zero(void) {
    assert(Ward_release_bed(0) == 0);
}

/**
 * @brief 测试病房状态流转：ACTIVE -> CLOSED -> ACTIVE
 */
static void test_ward_status_flow_active_closed_active(void) {
    Ward ward = make_ward("WRD0001", "Ward A", "DEP0001", "Floor 5", 5, 0, WARD_STATUS_ACTIVE);

    /* ACTIVE -> CLOSED */
    assert(ward.status == WARD_STATUS_ACTIVE);
    assert(Ward_close(&ward) == 1);
    assert(ward.status == WARD_STATUS_CLOSED);

    /* CLOSED -> ACTIVE */
    assert(Ward_open(&ward) == 1);
    assert(ward.status == WARD_STATUS_ACTIVE);
}

/**
 * @brief 测试 Ward_close：有占用床位时不能关闭
 */
static void test_close_fails_when_beds_occupied(void) {
    Ward ward = make_ward("WRD0001", "Ward A", "DEP0001", "Floor 5", 5, 2, WARD_STATUS_ACTIVE);

    assert(Ward_close(&ward) == 0);
    assert(ward.status == WARD_STATUS_ACTIVE); /* 状态未变 */
}

/**
 * @brief 测试 Ward_close：已关闭状态不能重复关闭
 */
static void test_close_fails_when_already_closed(void) {
    Ward ward = make_ward("WRD0001", "Ward A", "DEP0001", "Floor 5", 5, 0, WARD_STATUS_CLOSED);

    assert(Ward_close(&ward) == 0);
    assert(ward.status == WARD_STATUS_CLOSED);
}

/**
 * @brief 测试 Ward_open：开放状态不能重复开放
 */
static void test_open_fails_when_already_active(void) {
    Ward ward = make_ward("WRD0001", "Ward A", "DEP0001", "Floor 5", 5, 0, WARD_STATUS_ACTIVE);

    assert(Ward_open(&ward) == 0);
    assert(ward.status == WARD_STATUS_ACTIVE);
}

/**
 * @brief 测试 Ward_close / Ward_open：NULL 指针返回0
 */
static void test_close_and_open_null_returns_zero(void) {
    assert(Ward_close(0) == 0);
    assert(Ward_open(0) == 0);
}

/**
 * @brief 测试完整生命周期：占用 -> 释放 -> 关闭 -> 开放 -> 再占用
 */
static void test_full_lifecycle(void) {
    Ward ward = make_ward("WRD0001", "Ward A", "DEP0001", "Floor 5", 2, 0, WARD_STATUS_ACTIVE);

    /* 占用2张床位至满员 */
    assert(Ward_occupy_bed(&ward) == 1);
    assert(Ward_occupy_bed(&ward) == 1);
    assert(Ward_has_capacity(&ward) == 0);

    /* 满员时不能关闭 */
    assert(Ward_close(&ward) == 0);

    /* 释放所有床位 */
    assert(Ward_release_bed(&ward) == 1);
    assert(Ward_release_bed(&ward) == 1);
    assert(ward.occupied_beds == 0);

    /* 关闭病房 */
    assert(Ward_close(&ward) == 1);
    assert(ward.status == WARD_STATUS_CLOSED);

    /* 关闭状态不能占用 */
    assert(Ward_occupy_bed(&ward) == 0);

    /* 重新开放 */
    assert(Ward_open(&ward) == 1);
    assert(ward.status == WARD_STATUS_ACTIVE);

    /* 开放后可再占用 */
    assert(Ward_occupy_bed(&ward) == 1);
    assert(ward.occupied_beds == 1);
}

/**
 * @brief 测试主函数，依次运行所有病房领域模型测试用例
 */
int main(void) {
    test_has_capacity_when_not_full();
    test_has_capacity_when_empty();
    test_has_no_capacity_when_full();
    test_has_no_capacity_when_zero_capacity();
    test_has_no_capacity_when_closed();
    test_has_capacity_null_returns_zero();
    test_has_no_capacity_when_negative_occupied();
    test_occupy_bed_success();
    test_occupy_bed_fails_when_full();
    test_occupy_bed_fails_when_closed();
    test_occupy_bed_null_returns_zero();
    test_release_bed_success();
    test_release_bed_fails_when_zero_occupied();
    test_release_bed_null_returns_zero();
    test_ward_status_flow_active_closed_active();
    test_close_fails_when_beds_occupied();
    test_close_fails_when_already_closed();
    test_open_fails_when_already_active();
    test_close_and_open_null_returns_zero();
    test_full_lifecycle();
    return 0;
}
