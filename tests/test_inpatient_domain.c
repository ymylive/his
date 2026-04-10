/**
 * @file test_inpatient_domain.c
 * @brief 住院领域模型的单元测试文件
 *
 * 本文件测试住院相关领域实体的行为和仓储层功能，包括：
 * - 病区（Ward）和床位（Bed）的状态流转
 * - 入院记录（Admission）和医嘱（InpatientOrder）的状态流转
 * - 各仓储的持久化往返（追加、查找、冲突检测、出院、再入院）
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "common/LinkedList.h"
#include "domain/Admission.h"
#include "domain/Bed.h"
#include "domain/InpatientOrder.h"
#include "domain/Ward.h"
#include "repository/AdmissionRepository.h"
#include "repository/BedRepository.h"
#include "repository/InpatientOrderRepository.h"
#include "repository/WardRepository.h"

/**
 * @brief 辅助函数：构建测试用的临时文件路径并删除已存在的文件
 */
static void make_test_path(char *buffer, size_t buffer_size, const char *file_name) {
    assert(buffer != 0);
    assert(file_name != 0);

    snprintf(
        buffer,
        buffer_size,
        "build/test_inpatient_domain_data/%s_%ld.txt",
        file_name,
        (long)time(0)
    );
    remove(buffer); /* 清理旧文件，保证测试环境干净 */
}

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
    copy_text(ward.ward_id, sizeof(ward.ward_id), ward_id);
    copy_text(ward.name, sizeof(ward.name), name);
    copy_text(ward.department_id, sizeof(ward.department_id), department_id);
    copy_text(ward.location, sizeof(ward.location), location);
    ward.capacity = capacity;
    ward.occupied_beds = occupied_beds;
    ward.status = status;
    return ward;
}

/**
 * @brief 辅助函数：构造一个 Bed（床位）对象
 */
static Bed make_bed(
    const char *bed_id,
    const char *ward_id,
    const char *room_no,
    const char *bed_no,
    BedStatus status,
    const char *current_admission_id,
    const char *occupied_at,
    const char *released_at
) {
    Bed bed;

    memset(&bed, 0, sizeof(bed));
    copy_text(bed.bed_id, sizeof(bed.bed_id), bed_id);
    copy_text(bed.ward_id, sizeof(bed.ward_id), ward_id);
    copy_text(bed.room_no, sizeof(bed.room_no), room_no);
    copy_text(bed.bed_no, sizeof(bed.bed_no), bed_no);
    bed.status = status;
    copy_text(
        bed.current_admission_id,
        sizeof(bed.current_admission_id),
        current_admission_id
    );
    copy_text(bed.occupied_at, sizeof(bed.occupied_at), occupied_at);
    copy_text(bed.released_at, sizeof(bed.released_at), released_at);
    return bed;
}

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
    copy_text(admission.admission_id, sizeof(admission.admission_id), admission_id);
    copy_text(admission.patient_id, sizeof(admission.patient_id), patient_id);
    copy_text(admission.ward_id, sizeof(admission.ward_id), ward_id);
    copy_text(admission.bed_id, sizeof(admission.bed_id), bed_id);
    copy_text(admission.admitted_at, sizeof(admission.admitted_at), admitted_at);
    admission.status = status;
    copy_text(admission.discharged_at, sizeof(admission.discharged_at), discharged_at);
    copy_text(admission.summary, sizeof(admission.summary), summary);
    return admission;
}

/**
 * @brief 辅助函数：构造一个 InpatientOrder（住院医嘱）对象
 */
static InpatientOrder make_order(
    const char *order_id,
    const char *admission_id,
    const char *order_type,
    const char *content,
    const char *ordered_at,
    InpatientOrderStatus status,
    const char *executed_at,
    const char *cancelled_at
) {
    InpatientOrder order;

    memset(&order, 0, sizeof(order));
    copy_text(order.order_id, sizeof(order.order_id), order_id);
    copy_text(order.admission_id, sizeof(order.admission_id), admission_id);
    copy_text(order.order_type, sizeof(order.order_type), order_type);
    copy_text(order.content, sizeof(order.content), content);
    copy_text(order.ordered_at, sizeof(order.ordered_at), ordered_at);
    order.status = status;
    copy_text(order.executed_at, sizeof(order.executed_at), executed_at);
    copy_text(order.cancelled_at, sizeof(order.cancelled_at), cancelled_at);
    return order;
}

/**
 * @brief 辅助函数：在入院记录链表中查找指定ID的记录
 * @param list 入院记录链表
 * @param admission_id 要查找的入院记录ID
 * @return 找到的入院记录指针，未找到返回 NULL
 */
static Admission *find_admission_in_list(LinkedList *list, const char *admission_id) {
    LinkedListNode *current = 0;

    assert(list != 0);
    assert(admission_id != 0);

    current = list->head;
    while (current != 0) {
        Admission *admission = (Admission *)current->data;
        if (strcmp(admission->admission_id, admission_id) == 0) {
            return admission;
        }

        current = current->next;
    }

    return 0;
}

/**
 * @brief 测试病区和床位的状态流转
 *
 * 病区状态流转验证：
 * 1. 初始容量为2，可连续占用2张床位
 * 2. 满员后再占用应失败
 * 3. 释放一张床位后，已占用数减1
 *
 * 床位状态流转验证：
 * 1. 空闲床位可分配（AVAILABLE -> OCCUPIED）
 * 2. 已占用床位不能再分配
 * 3. 释放已占用床位（OCCUPIED -> AVAILABLE）
 * 4. 空闲床位不能再释放
 */
static void test_ward_and_bed_status_flow(void) {
    Ward ward = make_ward("WRD0001", "Ward A", "DEP0001", "Floor 5", 2, 0, WARD_STATUS_ACTIVE);
    Bed bed = make_bed("BED0001", "WRD0001", "501", "01", BED_STATUS_AVAILABLE, "", "", "");

    /* 病区占用流程测试 */
    assert(Ward_has_capacity(&ward) == 1);     /* 有剩余容量 */
    assert(Ward_occupy_bed(&ward) == 1);       /* 第1次占用成功 */
    assert(ward.occupied_beds == 1);
    assert(Ward_occupy_bed(&ward) == 1);       /* 第2次占用成功 */
    assert(ward.occupied_beds == 2);
    assert(Ward_has_capacity(&ward) == 0);     /* 已满员 */
    assert(Ward_occupy_bed(&ward) == 0);       /* 满员后占用失败 */
    assert(Ward_release_bed(&ward) == 1);      /* 释放一张床位 */
    assert(ward.occupied_beds == 1);

    /* 床位分配流程测试 */
    assert(Bed_assign(&bed, "ADM0001", "2026-03-20T08:00") == 1); /* 分配成功 */
    assert(bed.status == BED_STATUS_OCCUPIED);
    assert(strcmp(bed.current_admission_id, "ADM0001") == 0);      /* 关联入院记录 */
    assert(strcmp(bed.occupied_at, "2026-03-20T08:00") == 0);      /* 占用时间已记录 */

    /* 已占用床位不能重复分配 */
    assert(Bed_assign(&bed, "ADM0002", "2026-03-20T09:00") == 0);

    /* 释放床位 */
    assert(Bed_release(&bed, "2026-03-21T10:00") == 1);
    assert(bed.status == BED_STATUS_AVAILABLE);
    assert(bed.current_admission_id[0] == '\0');                   /* 关联记录已清空 */
    assert(strcmp(bed.released_at, "2026-03-21T10:00") == 0);      /* 释放时间已记录 */

    /* 空闲床位不能重复释放 */
    assert(Bed_release(&bed, "2026-03-22T10:00") == 0);
}

/**
 * @brief 测试入院记录和住院医嘱的状态流转
 *
 * 入院记录：
 * - ACTIVE -> DISCHARGED（出院）成功
 * - 已出院不能再次出院
 *
 * 住院医嘱：
 * - PENDING -> EXECUTED（执行）成功，执行后不能取消
 * - PENDING -> CANCELLED（取消）成功，取消后不能执行
 */
static void test_admission_and_order_status_flow(void) {
    Admission admission = make_admission(
        "ADM0001",
        "PAT0001",
        "WRD0001",
        "BED0001",
        "2026-03-20T08:00",
        ADMISSION_STATUS_ACTIVE,
        "",
        "initial admission"
    );
    /* 待执行的医嘱 */
    InpatientOrder pending = make_order(
        "ORD0001",
        "ADM0001",
        "NURSING",
        "Monitor blood pressure",
        "2026-03-20T08:10",
        INPATIENT_ORDER_STATUS_PENDING,
        "",
        ""
    );
    /* 将被取消的医嘱 */
    InpatientOrder cancelled = make_order(
        "ORD0002",
        "ADM0001",
        "DIET",
        "Low sodium diet",
        "2026-03-20T08:20",
        INPATIENT_ORDER_STATUS_PENDING,
        "",
        ""
    );

    /* 出院操作 */
    assert(Admission_discharge(&admission, "2026-03-23T10:00") == 1);     /* 出院成功 */
    assert(admission.status == ADMISSION_STATUS_DISCHARGED);
    assert(strcmp(admission.discharged_at, "2026-03-23T10:00") == 0);
    assert(Admission_discharge(&admission, "2026-03-24T10:00") == 0);     /* 重复出院失败 */

    /* 医嘱执行操作 */
    assert(InpatientOrder_mark_executed(&pending, "2026-03-20T09:00") == 1); /* 执行成功 */
    assert(pending.status == INPATIENT_ORDER_STATUS_EXECUTED);
    assert(strcmp(pending.executed_at, "2026-03-20T09:00") == 0);
    assert(InpatientOrder_cancel(&pending, "2026-03-20T09:10") == 0);       /* 已执行不能取消 */

    /* 医嘱取消操作 */
    assert(InpatientOrder_cancel(&cancelled, "2026-03-20T08:30") == 1);     /* 取消成功 */
    assert(cancelled.status == INPATIENT_ORDER_STATUS_CANCELLED);
    assert(strcmp(cancelled.cancelled_at, "2026-03-20T08:30") == 0);
    assert(InpatientOrder_mark_executed(&cancelled, "2026-03-20T08:40") == 0); /* 已取消不能执行 */
}

/**
 * @brief 测试仓储层的持久化往返和约束检查
 *
 * 验证流程：
 * 1. 初始化四种仓储（病区、床位、入院记录、医嘱）
 * 2. 追加数据后按ID查找，验证持久化正确
 * 3. 按患者ID和床位ID查找活跃入院记录
 * 4. 检测同一床位和同一患者的重复入院冲突
 * 5. 通过 load_all + 修改 + save_all 完成出院操作
 * 6. 出院后允许同一患者再次入院
 * 7. 医嘱仓储的追加和按ID查找
 */
static void test_repository_roundtrip_and_guards(void) {
    char ward_path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    char bed_path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    char admission_path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    char order_path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    WardRepository ward_repository;
    BedRepository bed_repository;
    AdmissionRepository admission_repository;
    InpatientOrderRepository order_repository;
    Ward ward = make_ward("WRD0001", "Ward A", "DEP0001", "Floor 5", 10, 0, WARD_STATUS_ACTIVE);
    Bed first_bed = make_bed("BED0001", "WRD0001", "501", "01", BED_STATUS_AVAILABLE, "", "", "");
    Bed second_bed = make_bed("BED0002", "WRD0001", "501", "02", BED_STATUS_AVAILABLE, "", "", "");
    Admission first_admission = make_admission(
        "ADM0001",
        "PAT0001",
        "WRD0001",
        "BED0001",
        "2026-03-20T08:00",
        ADMISSION_STATUS_ACTIVE,
        "",
        "admit for observation"
    );
    /* 同一床位的冲突入院记录 */
    Admission same_bed_conflict = make_admission(
        "ADM0002",
        "PAT0002",
        "WRD0001",
        "BED0001",
        "2026-03-20T08:10",
        ADMISSION_STATUS_ACTIVE,
        "",
        "bed conflict"
    );
    /* 同一患者的冲突入院记录 */
    Admission same_patient_conflict = make_admission(
        "ADM0003",
        "PAT0001",
        "WRD0001",
        "BED0002",
        "2026-03-20T08:20",
        ADMISSION_STATUS_ACTIVE,
        "",
        "patient conflict"
    );
    /* 出院后再入院的记录 */
    Admission readmit = make_admission(
        "ADM0004",
        "PAT0001",
        "WRD0001",
        "BED0002",
        "2026-03-24T08:00",
        ADMISSION_STATUS_ACTIVE,
        "",
        "readmission"
    );
    InpatientOrder order = make_order(
        "ORD0001",
        "ADM0001",
        "MEDICATION",
        "Aspirin 100mg",
        "2026-03-20T08:30",
        INPATIENT_ORDER_STATUS_PENDING,
        "",
        ""
    );
    Ward loaded_ward;
    Bed loaded_bed;
    Admission loaded_admission;
    Admission active_admission;
    InpatientOrder loaded_order;
    LinkedList admissions;
    Result result;

    /* 构建各仓储的临时文件路径 */
    make_test_path(ward_path, sizeof(ward_path), "wards");
    make_test_path(bed_path, sizeof(bed_path), "beds");
    make_test_path(admission_path, sizeof(admission_path), "admissions");
    make_test_path(order_path, sizeof(order_path), "orders");

    /* 初始化各仓储 */
    result = WardRepository_init(&ward_repository, ward_path);
    assert(result.success == 1);
    result = BedRepository_init(&bed_repository, bed_path);
    assert(result.success == 1);
    result = AdmissionRepository_init(&admission_repository, admission_path);
    assert(result.success == 1);
    result = InpatientOrderRepository_init(&order_repository, order_path);
    assert(result.success == 1);

    /* 追加基础数据 */
    assert(WardRepository_append(&ward_repository, &ward).success == 1);
    assert(BedRepository_append(&bed_repository, &first_bed).success == 1);
    assert(BedRepository_append(&bed_repository, &second_bed).success == 1);
    assert(AdmissionRepository_append(&admission_repository, &first_admission).success == 1);
    assert(InpatientOrderRepository_append(&order_repository, &order).success == 1);

    /* 按ID查找病区 */
    result = WardRepository_find_by_id(&ward_repository, "WRD0001", &loaded_ward);
    assert(result.success == 1);
    assert(strcmp(loaded_ward.name, "Ward A") == 0);

    /* 按ID查找床位 */
    result = BedRepository_find_by_id(&bed_repository, "BED0001", &loaded_bed);
    assert(result.success == 1);
    assert(loaded_bed.status == BED_STATUS_AVAILABLE);

    /* 按患者ID查找活跃入院记录 */
    result = AdmissionRepository_find_active_by_patient_id(
        &admission_repository,
        "PAT0001",
        &active_admission
    );
    assert(result.success == 1);
    assert(strcmp(active_admission.admission_id, "ADM0001") == 0);

    /* 按床位ID查找活跃入院记录 */
    result = AdmissionRepository_find_active_by_bed_id(
        &admission_repository,
        "BED0001",
        &active_admission
    );
    assert(result.success == 1);
    assert(strcmp(active_admission.patient_id, "PAT0001") == 0);

    /* 冲突检测：同一床位已有活跃入院，再次入院应失败 */
    assert(AdmissionRepository_append(&admission_repository, &same_bed_conflict).success == 0);
    /* 冲突检测：同一患者已有活跃入院，再次入院应失败 */
    assert(AdmissionRepository_append(&admission_repository, &same_patient_conflict).success == 0);

    /* 通过 load_all + 修改 + save_all 实现出院操作 */
    LinkedList_init(&admissions);
    result = AdmissionRepository_load_all(&admission_repository, &admissions);
    assert(result.success == 1);
    assert(find_admission_in_list(&admissions, "ADM0001") != 0);

    /* 标记 ADM0001 出院 */
    assert(Admission_discharge(find_admission_in_list(&admissions, "ADM0001"), "2026-03-23T10:00") == 1);
    assert(AdmissionRepository_save_all(&admission_repository, &admissions).success == 1);
    AdmissionRepository_clear_loaded_list(&admissions);

    /* 验证出院状态已持久化 */
    result = AdmissionRepository_find_by_id(&admission_repository, "ADM0001", &loaded_admission);
    assert(result.success == 1);
    assert(loaded_admission.status == ADMISSION_STATUS_DISCHARGED);
    assert(strcmp(loaded_admission.discharged_at, "2026-03-23T10:00") == 0);

    /* 出院后允许同一患者再入院 */
    assert(AdmissionRepository_append(&admission_repository, &readmit).success == 1);

    /* 医嘱仓储按ID查找 */
    result = InpatientOrderRepository_find_by_id(&order_repository, "ORD0001", &loaded_order);
    assert(result.success == 1);
    assert(strcmp(loaded_order.content, "Aspirin 100mg") == 0);
    assert(loaded_order.status == INPATIENT_ORDER_STATUS_PENDING);
}

/**
 * @brief 测试主函数，依次运行所有住院领域模型测试用例
 */
int main(void) {
    test_ward_and_bed_status_flow();
    test_admission_and_order_status_flow();
    test_repository_roundtrip_and_guards();
    return 0;
}
