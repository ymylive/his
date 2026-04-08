/**
 * @file test_inpatient_service.c
 * @brief 住院服务（InpatientService）的单元测试文件
 *
 * 本文件测试住院管理服务的核心功能，包括：
 * - 入院办理：更新患者、病区、床位和入院记录
 * - 入院拒绝：重复住院患者和已占用床位
 * - 出院办理：释放床位并清除住院标记
 * - 转床：更新入院记录和新旧床位状态
 * - 转床失败：目标病区未启用时保持持久化状态不变
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "domain/Admission.h"
#include "domain/Bed.h"
#include "domain/Patient.h"
#include "domain/Ward.h"
#include "repository/AdmissionRepository.h"
#include "repository/BedRepository.h"
#include "repository/PatientRepository.h"
#include "repository/WardRepository.h"
#include "service/InpatientService.h"

/**
 * @brief 住院服务测试上下文结构体
 *
 * 封装测试所需的文件路径、仓储和服务对象。
 */
typedef struct InpatientServiceTestContext {
    char patient_path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];     /* 患者数据文件路径 */
    char ward_path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];        /* 病区数据文件路径 */
    char bed_path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];         /* 床位数据文件路径 */
    char admission_path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];   /* 入院记录数据文件路径 */
    PatientRepository patient_repository;                      /* 患者仓储 */
    WardRepository ward_repository;                            /* 病区仓储 */
    BedRepository bed_repository;                              /* 床位仓储 */
    AdmissionRepository admission_repository;                  /* 入院记录仓储 */
    InpatientService service;                                  /* 住院服务 */
} InpatientServiceTestContext;

/**
 * @brief 辅助函数：构建测试用的临时文件路径
 */
static void build_test_path(
    char *buffer,
    size_t buffer_size,
    const char *test_name,
    const char *file_name
) {
    assert(buffer != 0);
    assert(test_name != 0);
    assert(file_name != 0);

    snprintf(
        buffer,
        buffer_size,
        "build/test_inpatient_service_data/%s_%ld_%lu_%s",
        test_name,
        (long)time(0),
        (unsigned long)clock(),
        file_name
    );
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
 * @brief 辅助函数：构造一个 Patient（患者）对象
 * @param patient_id 患者ID
 * @param name 姓名
 * @param is_inpatient 是否为住院患者（0=否，1=是）
 */
static Patient make_patient(const char *patient_id, const char *name, int is_inpatient) {
    Patient patient;

    memset(&patient, 0, sizeof(patient));
    copy_text(patient.patient_id, sizeof(patient.patient_id), patient_id);
    copy_text(patient.name, sizeof(patient.name), name);
    patient.gender = PATIENT_GENDER_UNKNOWN;
    patient.age = 30;
    copy_text(patient.contact, sizeof(patient.contact), "13800000000");
    copy_text(patient.id_card, sizeof(patient.id_card), "110101199001011234");
    copy_text(patient.allergy, sizeof(patient.allergy), "None");
    copy_text(patient.medical_history, sizeof(patient.medical_history), "None");
    patient.is_inpatient = is_inpatient;
    copy_text(patient.remarks, sizeof(patient.remarks), "test");
    return patient;
}

/**
 * @brief 辅助函数：构造一个 Ward（病区）对象
 */
static Ward make_ward(
    const char *ward_id,
    int capacity,
    int occupied_beds,
    WardStatus status
) {
    Ward ward;

    memset(&ward, 0, sizeof(ward));
    copy_text(ward.ward_id, sizeof(ward.ward_id), ward_id);
    copy_text(ward.name, sizeof(ward.name), "Ward A");
    copy_text(ward.department_id, sizeof(ward.department_id), "DEP0001");
    copy_text(ward.location, sizeof(ward.location), "Floor 5");
    ward.capacity = capacity;
    ward.occupied_beds = occupied_beds;
    ward.status = status;
    return ward;
}

/**
 * @brief 辅助函数：构造一个 Bed（床位）对象
 * @param bed_id 床位ID
 * @param ward_id 所属病区ID
 * @param status 床位状态
 * @param admission_id 当前关联的入院记录ID（空字符串表示无关联）
 */
static Bed make_bed(
    const char *bed_id,
    const char *ward_id,
    BedStatus status,
    const char *admission_id
) {
    Bed bed;

    memset(&bed, 0, sizeof(bed));
    copy_text(bed.bed_id, sizeof(bed.bed_id), bed_id);
    copy_text(bed.ward_id, sizeof(bed.ward_id), ward_id);
    copy_text(bed.room_no, sizeof(bed.room_no), "501");
    copy_text(bed.bed_no, sizeof(bed.bed_no), "01");
    bed.status = status;
    copy_text(bed.current_admission_id, sizeof(bed.current_admission_id), admission_id);
    /* 已占用床位设置占用时间 */
    if (status == BED_STATUS_OCCUPIED) {
        copy_text(bed.occupied_at, sizeof(bed.occupied_at), "2026-03-20T08:00");
    }
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
 * @brief 辅助函数：初始化住院服务测试上下文
 *
 * 创建临时文件路径，初始化所有仓储和住院服务。
 */
static void setup_context(InpatientServiceTestContext *context, const char *test_name) {
    Result result;

    memset(context, 0, sizeof(*context));
    build_test_path(context->patient_path, sizeof(context->patient_path), test_name, "patients.txt");
    build_test_path(context->ward_path, sizeof(context->ward_path), test_name, "wards.txt");
    build_test_path(context->bed_path, sizeof(context->bed_path), test_name, "beds.txt");
    build_test_path(
        context->admission_path,
        sizeof(context->admission_path),
        test_name,
        "admissions.txt"
    );

    /* 初始化各仓储 */
    result = PatientRepository_init(&context->patient_repository, context->patient_path);
    assert(result.success == 1);
    result = WardRepository_init(&context->ward_repository, context->ward_path);
    assert(result.success == 1);
    result = BedRepository_init(&context->bed_repository, context->bed_path);
    assert(result.success == 1);
    result = AdmissionRepository_init(&context->admission_repository, context->admission_path);
    assert(result.success == 1);

    /* 初始化住院服务 */
    result = InpatientService_init(
        &context->service,
        context->patient_path,
        context->ward_path,
        context->bed_path,
        context->admission_path
    );
    assert(result.success == 1);
}

/**
 * @brief 测试入院办理：验证患者、病区、床位和入院记录均被正确更新
 *
 * 验证流程：
 * 1. 预埋患者（非住院）、病区（空闲）、床位（可用）
 * 2. 调用入院接口
 * 3. 验证入院记录创建成功，ID为自动分配的 ADM0001
 * 4. 验证患者的住院标记变为1
 * 5. 验证病区已占用床位数增加
 * 6. 验证床位状态变为已占用且关联入院记录
 * 7. 验证入院记录可按ID查到
 */
static void test_admit_patient_updates_patient_ward_bed_and_admission(void) {
    InpatientServiceTestContext context;
    Patient patient_seed = make_patient("PAT0001", "Alice", 0);
    Ward ward_seed = make_ward("WRD0001", 2, 0, WARD_STATUS_ACTIVE);
    Bed bed_seed = make_bed("BED0001", "WRD0001", BED_STATUS_AVAILABLE, "");
    Admission created;
    Patient patient;
    Ward ward;
    Bed bed;
    Admission admission;
    Result result;

    setup_context(&context, "admit");

    /* 预埋测试数据 */
    assert(PatientRepository_append(&context.patient_repository, &patient_seed).success == 1);
    assert(WardRepository_append(&context.ward_repository, &ward_seed).success == 1);
    assert(BedRepository_append(&context.bed_repository, &bed_seed).success == 1);

    /* 执行入院操作 */
    result = InpatientService_admit_patient(
        &context.service,
        "PAT0001",
        "WRD0001",
        "BED0001",
        "2026-03-20T08:00",
        "observation",
        &created
    );
    assert(result.success == 1);
    assert(strcmp(created.admission_id, "ADM0001") == 0);  /* 验证自动分配的ID */
    assert(created.status == ADMISSION_STATUS_ACTIVE);     /* 入院状态为活跃 */

    /* 验证患者住院标记已更新 */
    assert(PatientRepository_find_by_id(&context.patient_repository, "PAT0001", &patient).success == 1);
    assert(patient.is_inpatient == 1);

    /* 验证病区已占用床位数已更新 */
    assert(WardRepository_find_by_id(&context.ward_repository, "WRD0001", &ward).success == 1);
    assert(ward.occupied_beds == 1);

    /* 验证床位状态已更新 */
    assert(BedRepository_find_by_id(&context.bed_repository, "BED0001", &bed).success == 1);
    assert(bed.status == BED_STATUS_OCCUPIED);
    assert(strcmp(bed.current_admission_id, "ADM0001") == 0);

    /* 验证入院记录已持久化 */
    assert(
        AdmissionRepository_find_by_id(&context.admission_repository, "ADM0001", &admission).success == 1
    );
    assert(strcmp(admission.patient_id, "PAT0001") == 0);
    assert(strcmp(admission.summary, "observation") == 0);
}

/**
 * @brief 测试入院拒绝：已住院患者和已占用床位
 *
 * 验证场景：
 * 1. 同一患者重复入院（使用不同床位）应失败
 * 2. 不同患者入院到已占用的床位应失败
 */
static void test_admit_patient_rejects_duplicate_active_patient_and_occupied_bed(void) {
    InpatientServiceTestContext context;
    Patient first_patient = make_patient("PAT0001", "Alice", 0);
    Patient second_patient = make_patient("PAT0002", "Bob", 0);
    Ward ward_seed = make_ward("WRD0001", 2, 0, WARD_STATUS_ACTIVE);
    Bed first_bed = make_bed("BED0001", "WRD0001", BED_STATUS_AVAILABLE, "");
    Bed second_bed = make_bed("BED0002", "WRD0001", BED_STATUS_AVAILABLE, "");
    Admission created;
    Result result;

    setup_context(&context, "duplicate");

    /* 预埋测试数据 */
    assert(PatientRepository_append(&context.patient_repository, &first_patient).success == 1);
    assert(PatientRepository_append(&context.patient_repository, &second_patient).success == 1);
    assert(WardRepository_append(&context.ward_repository, &ward_seed).success == 1);
    assert(BedRepository_append(&context.bed_repository, &first_bed).success == 1);
    assert(BedRepository_append(&context.bed_repository, &second_bed).success == 1);

    /* 第一次入院成功 */
    result = InpatientService_admit_patient(
        &context.service,
        "PAT0001",
        "WRD0001",
        "BED0001",
        "2026-03-20T08:00",
        "observation",
        &created
    );
    assert(result.success == 1);

    /* 同一患者重复入院应失败 */
    result = InpatientService_admit_patient(
        &context.service,
        "PAT0001",
        "WRD0001",
        "BED0002",
        "2026-03-20T09:00",
        "repeat",
        &created
    );
    assert(result.success == 0);

    /* 不同患者入住已占用的床位应失败 */
    result = InpatientService_admit_patient(
        &context.service,
        "PAT0002",
        "WRD0001",
        "BED0001",
        "2026-03-20T09:10",
        "occupied bed",
        &created
    );
    assert(result.success == 0);
}

/**
 * @brief 测试出院办理：释放床位并清除住院标记
 *
 * 验证流程：
 * 1. 预埋住院患者、占用床位和活跃入院记录
 * 2. 执行出院操作
 * 3. 验证入院记录状态变为已出院
 * 4. 验证患者住院标记清除
 * 5. 验证病区已占用数减少
 * 6. 验证床位释放为可用状态
 */
static void test_discharge_patient_releases_bed_and_clears_inpatient_flag(void) {
    InpatientServiceTestContext context;
    Patient patient_seed = make_patient("PAT0001", "Alice", 1);         /* 住院患者 */
    Ward ward_seed = make_ward("WRD0001", 2, 1, WARD_STATUS_ACTIVE);   /* 已占1床 */
    Bed bed_seed = make_bed("BED0001", "WRD0001", BED_STATUS_OCCUPIED, "ADM0001");
    Admission admission_seed = make_admission(
        "ADM0001",
        "PAT0001",
        "WRD0001",
        "BED0001",
        "2026-03-20T08:00",
        ADMISSION_STATUS_ACTIVE,
        "",
        "observation"
    );
    Admission discharged;
    Patient patient;
    Ward ward;
    Bed bed;
    Admission admission;
    Result result;

    setup_context(&context, "discharge");

    /* 预埋测试数据 */
    assert(PatientRepository_append(&context.patient_repository, &patient_seed).success == 1);
    assert(WardRepository_append(&context.ward_repository, &ward_seed).success == 1);
    assert(BedRepository_append(&context.bed_repository, &bed_seed).success == 1);
    assert(AdmissionRepository_append(&context.admission_repository, &admission_seed).success == 1);

    /* 执行出院操作 */
    result = InpatientService_discharge_patient(
        &context.service,
        "ADM0001",
        "2026-03-22T10:00",
        "stable",
        &discharged
    );
    assert(result.success == 1);
    assert(discharged.status == ADMISSION_STATUS_DISCHARGED);           /* 状态为已出院 */
    assert(strcmp(discharged.discharged_at, "2026-03-22T10:00") == 0);  /* 出院时间正确 */
    assert(strcmp(discharged.summary, "stable") == 0);                  /* 出院小结已更新 */

    /* 验证患者住院标记已清除 */
    assert(PatientRepository_find_by_id(&context.patient_repository, "PAT0001", &patient).success == 1);
    assert(patient.is_inpatient == 0);

    /* 验证病区已占用数已减少 */
    assert(WardRepository_find_by_id(&context.ward_repository, "WRD0001", &ward).success == 1);
    assert(ward.occupied_beds == 0);

    /* 验证床位已释放 */
    assert(BedRepository_find_by_id(&context.bed_repository, "BED0001", &bed).success == 1);
    assert(bed.status == BED_STATUS_AVAILABLE);
    assert(strcmp(bed.current_admission_id, "") == 0);              /* 关联记录已清空 */
    assert(strcmp(bed.released_at, "2026-03-22T10:00") == 0);      /* 释放时间已设置 */

    /* 验证入院记录已持久化更新 */
    assert(
        AdmissionRepository_find_by_id(&context.admission_repository, "ADM0001", &admission).success == 1
    );
    assert(admission.status == ADMISSION_STATUS_DISCHARGED);
    assert(strcmp(admission.summary, "stable") == 0);
}

/**
 * @brief 测试转床：更新入院记录和新旧床位状态
 *
 * 验证流程：
 * 1. 患者从 BED0001 转到 BED0002
 * 2. 原床位释放为可用状态
 * 3. 目标床位变为已占用
 * 4. 入院记录的床位ID更新
 */
static void test_transfer_bed_updates_admission_and_bed_states(void) {
    InpatientServiceTestContext context;
    Patient patient_seed = make_patient("PAT0001", "Alice", 1);
    Ward ward_seed = make_ward("WRD0001", 3, 1, WARD_STATUS_ACTIVE);
    Bed occupied_bed = make_bed("BED0001", "WRD0001", BED_STATUS_OCCUPIED, "ADM0001");
    Bed target_bed = make_bed("BED0002", "WRD0001", BED_STATUS_AVAILABLE, "");
    Admission admission_seed = make_admission(
        "ADM0001",
        "PAT0001",
        "WRD0001",
        "BED0001",
        "2026-03-20T08:00",
        ADMISSION_STATUS_ACTIVE,
        "",
        "observation"
    );
    Admission transferred;
    Bed first_bed;
    Bed second_bed;
    Admission admission;
    Result result;

    setup_context(&context, "transfer");

    /* 预埋测试数据 */
    assert(PatientRepository_append(&context.patient_repository, &patient_seed).success == 1);
    assert(WardRepository_append(&context.ward_repository, &ward_seed).success == 1);
    assert(BedRepository_append(&context.bed_repository, &occupied_bed).success == 1);
    assert(BedRepository_append(&context.bed_repository, &target_bed).success == 1);
    assert(AdmissionRepository_append(&context.admission_repository, &admission_seed).success == 1);

    /* 执行转床操作 */
    result = InpatientService_transfer_bed(
        &context.service,
        "ADM0001",
        "BED0002",
        "2026-03-21T09:00",
        &transferred
    );
    assert(result.success == 1);
    assert(strcmp(transferred.bed_id, "BED0002") == 0); /* 入院记录的床位已更新 */

    /* 验证原床位已释放 */
    assert(BedRepository_find_by_id(&context.bed_repository, "BED0001", &first_bed).success == 1);
    assert(first_bed.status == BED_STATUS_AVAILABLE);
    assert(strcmp(first_bed.current_admission_id, "") == 0);
    assert(strcmp(first_bed.released_at, "2026-03-21T09:00") == 0);

    /* 验证目标床位已占用 */
    assert(BedRepository_find_by_id(&context.bed_repository, "BED0002", &second_bed).success == 1);
    assert(second_bed.status == BED_STATUS_OCCUPIED);
    assert(strcmp(second_bed.current_admission_id, "ADM0001") == 0);
    assert(strcmp(second_bed.occupied_at, "2026-03-21T09:00") == 0);

    /* 验证入院记录的床位ID已更新 */
    assert(
        AdmissionRepository_find_by_id(&context.admission_repository, "ADM0001", &admission).success == 1
    );
    assert(strcmp(admission.bed_id, "BED0002") == 0);
}

/**
 * @brief 测试转床失败：目标病区未启用时保持持久化状态不变
 *
 * 验证：当目标床位所属病区已关闭（CLOSED），转床操作应失败，
 * 且所有持久化数据（病区、床位、入院记录）均保持原状。
 */
static void test_transfer_bed_fails_when_target_ward_not_active_and_keeps_persisted_state(void) {
    InpatientServiceTestContext context;
    Patient patient_seed = make_patient("PAT0001", "Alice", 1);
    Ward current_ward_seed = make_ward("WRD0001", 2, 1, WARD_STATUS_ACTIVE);
    Ward target_ward_seed = make_ward("WRD0002", 2, 0, WARD_STATUS_CLOSED); /* 目标病区已关闭 */
    Bed occupied_bed = make_bed("BED0001", "WRD0001", BED_STATUS_OCCUPIED, "ADM0001");
    Bed target_bed = make_bed("BED0002", "WRD0002", BED_STATUS_AVAILABLE, "");
    Admission admission_seed = make_admission(
        "ADM0001",
        "PAT0001",
        "WRD0001",
        "BED0001",
        "2026-03-20T08:00",
        ADMISSION_STATUS_ACTIVE,
        "",
        "observation"
    );
    Admission transferred;
    Ward current_ward;
    Ward target_ward;
    Bed first_bed;
    Bed second_bed;
    Admission admission;
    Result result;

    setup_context(&context, "transfer_fail_target_ward");

    /* 预埋测试数据 */
    assert(PatientRepository_append(&context.patient_repository, &patient_seed).success == 1);
    assert(WardRepository_append(&context.ward_repository, &current_ward_seed).success == 1);
    assert(WardRepository_append(&context.ward_repository, &target_ward_seed).success == 1);
    assert(BedRepository_append(&context.bed_repository, &occupied_bed).success == 1);
    assert(BedRepository_append(&context.bed_repository, &target_bed).success == 1);
    assert(AdmissionRepository_append(&context.admission_repository, &admission_seed).success == 1);

    /* 转床应失败（目标病区已关闭） */
    result = InpatientService_transfer_bed(
        &context.service,
        "ADM0001",
        "BED0002",
        "2026-03-21T09:00",
        &transferred
    );
    assert(result.success == 0);

    /* 验证所有持久化状态未改变 */

    /* 当前病区已占用数未变 */
    assert(WardRepository_find_by_id(&context.ward_repository, "WRD0001", &current_ward).success == 1);
    assert(current_ward.occupied_beds == 1);
    /* 目标病区已占用数未变 */
    assert(WardRepository_find_by_id(&context.ward_repository, "WRD0002", &target_ward).success == 1);
    assert(target_ward.occupied_beds == 0);

    /* 原床位仍为占用状态 */
    assert(BedRepository_find_by_id(&context.bed_repository, "BED0001", &first_bed).success == 1);
    assert(first_bed.status == BED_STATUS_OCCUPIED);
    assert(strcmp(first_bed.current_admission_id, "ADM0001") == 0);
    /* 目标床位仍为可用状态 */
    assert(BedRepository_find_by_id(&context.bed_repository, "BED0002", &second_bed).success == 1);
    assert(second_bed.status == BED_STATUS_AVAILABLE);
    assert(strcmp(second_bed.current_admission_id, "") == 0);

    /* 入院记录的病区和床位未变 */
    assert(AdmissionRepository_find_by_id(&context.admission_repository, "ADM0001", &admission).success == 1);
    assert(strcmp(admission.ward_id, "WRD0001") == 0);
    assert(strcmp(admission.bed_id, "BED0001") == 0);
}

/**
 * @brief 测试主函数，依次运行所有住院服务测试用例
 */
int main(void) {
    test_admit_patient_updates_patient_ward_bed_and_admission();
    test_admit_patient_rejects_duplicate_active_patient_and_occupied_bed();
    test_discharge_patient_releases_bed_and_clears_inpatient_flag();
    test_transfer_bed_updates_admission_and_bed_states();
    test_transfer_bed_fails_when_target_ward_not_active_and_keeps_persisted_state();
    return 0;
}
