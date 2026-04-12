/**
 * @file test_registration_service.c
 * @brief 挂号服务（RegistrationService）的单元测试文件
 *
 * 本文件测试挂号管理服务的核心功能，包括：
 * - 创建挂号、按ID查找和按患者查询
 * - 拒绝不存在的患者、医生、科室及科室不匹配的挂号
 * - 取消待诊状态的挂号
 * - 拒绝取消已诊断的挂号
 * - 按医生查询（支持状态和时间范围过滤）
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "common/LinkedList.h"
#include "domain/Department.h"
#include "domain/Doctor.h"
#include "domain/Patient.h"
#include "domain/Registration.h"
#include "repository/DepartmentRepository.h"
#include "repository/DoctorRepository.h"
#include "repository/PatientRepository.h"
#include "repository/RegistrationRepository.h"
#include "service/RegistrationService.h"

/**
 * @brief 挂号服务测试上下文结构体
 *
 * 封装测试所需的文件路径、仓储和服务对象。
 */
typedef struct RegistrationServiceTestContext {
    char patient_path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];        /* 患者数据文件路径 */
    char doctor_path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];         /* 医生数据文件路径 */
    char department_path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];     /* 科室数据文件路径 */
    char registration_path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];   /* 挂号数据文件路径 */
    PatientRepository patient_repository;                         /* 患者仓储 */
    DoctorRepository doctor_repository;                           /* 医生仓储 */
    DepartmentRepository department_repository;                   /* 科室仓储 */
    RegistrationRepository registration_repository;               /* 挂号仓储 */
    RegistrationService service;                                  /* 挂号服务 */
} RegistrationServiceTestContext;

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
        "build/test_registration_service_data/%s_%ld_%lu_%s",
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
 * @brief 辅助函数：获取链表中指定索引位置的 Registration 指针
 */
static const Registration *registration_at(const LinkedList *list, size_t index) {
    const LinkedListNode *node = 0;
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
 * @brief 辅助函数：初始化挂号服务测试上下文
 *
 * 预埋数据：
 * - 2个科室（Internal、Surgery）
 * - 2名医生（Dr.Alice属DEP0001、Dr.Bob属DEP0002）
 * - 2名患者（Alice、Bob）
 */
static void setup_context(RegistrationServiceTestContext *context, const char *test_name) {
    Department department_a = {"DEP0001", "Internal", "Floor1", "Internal medicine"};
    Department department_b = {"DEP0002", "Surgery", "Floor2", "General surgery"};
    Doctor doctor_a = {
        "DOC0001",
        "Dr.Alice",
        "Chief",
        "DEP0001",
        "Mon-Fri AM",
        DOCTOR_STATUS_ACTIVE
    };
    Doctor doctor_b = {
        "DOC0002",
        "Dr.Bob",
        "Attending",
        "DEP0002",
        "Tue-Thu PM",
        DOCTOR_STATUS_ACTIVE
    };
    Patient patient_a = {
        "PAT0001",
        "Alice",
        PATIENT_GENDER_FEMALE,
        28,
        "13800000000",
        "ID0001",
        "None",
        "Healthy",
        0,
        "First visit"
    };
    Patient patient_b = {
        "PAT0002",
        "Bob",
        PATIENT_GENDER_MALE,
        35,
        "13800000001",
        "ID0002",
        "Penicillin",
        "Asthma",
        0,
        "Review"
    };
    Result result;

    memset(context, 0, sizeof(*context));
    build_test_path(context->patient_path, sizeof(context->patient_path), test_name, "patients.txt");
    build_test_path(context->doctor_path, sizeof(context->doctor_path), test_name, "doctors.txt");
    build_test_path(
        context->department_path,
        sizeof(context->department_path),
        test_name,
        "departments.txt"
    );
    build_test_path(
        context->registration_path,
        sizeof(context->registration_path),
        test_name,
        "registrations.txt"
    );

    /* 初始化各仓储 */
    result = PatientRepository_init(&context->patient_repository, context->patient_path);
    assert(result.success == 1);
    result = DoctorRepository_init(&context->doctor_repository, context->doctor_path);
    assert(result.success == 1);
    result = DepartmentRepository_init(&context->department_repository, context->department_path);
    assert(result.success == 1);
    result = RegistrationRepository_init(
        &context->registration_repository,
        context->registration_path
    );
    assert(result.success == 1);

    /* 预埋基础数据 */
    assert(DepartmentRepository_save(&context->department_repository, &department_a).success == 1);
    assert(DepartmentRepository_save(&context->department_repository, &department_b).success == 1);
    assert(DoctorRepository_save(&context->doctor_repository, &doctor_a).success == 1);
    assert(DoctorRepository_save(&context->doctor_repository, &doctor_b).success == 1);
    assert(PatientRepository_append(&context->patient_repository, &patient_a).success == 1);
    assert(PatientRepository_append(&context->patient_repository, &patient_b).success == 1);

    /* 初始化挂号服务 */
    result = RegistrationService_init(
        &context->service,
        &context->registration_repository,
        &context->patient_repository,
        &context->doctor_repository,
        &context->department_repository
    );
    assert(result.success == 1);
}

/**
 * @brief 测试创建挂号、按ID查找和按患者查询
 *
 * 验证流程：
 * 1. 创建两条挂号记录（同一患者、同一医生）
 * 2. 验证自动分配的ID递增
 * 3. 按挂号ID查找验证各字段
 * 4. 按患者ID查询，验证返回2条记录
 */
static void test_create_find_and_query_by_patient(void) {
    RegistrationServiceTestContext context;
    Registration first_created;
    Registration second_created;
    Registration loaded;
    LinkedList patient_records;
    Result result;

    setup_context(&context, "create_find_query");

    /* 创建第一条挂号 */
    result = RegistrationService_create(
        &context.service,
        "PAT0001",
        "DOC0001",
        "DEP0001",
        "2026-03-20T08:15",
        REG_TYPE_STANDARD,
        5.00,
        &first_created
    );
    assert(result.success == 1);
    assert(strcmp(first_created.registration_id, "REG0001") == 0);       /* 自动分配ID */
    assert(strcmp(first_created.registered_at, "2026-03-20T08:15") == 0);
    assert(first_created.status == REG_STATUS_PENDING);                   /* 初始状态为待诊 */
    assert(strcmp(first_created.diagnosed_at, "") == 0);                   /* 诊断时间为空 */
    assert(strcmp(first_created.cancelled_at, "") == 0);                   /* 取消时间为空 */
    assert(first_created.registration_type == REG_TYPE_STANDARD);         /* 挂号类型为普通号 */
    assert(first_created.registration_fee == 5.00);                       /* 挂号费为5元 */

    /* 创建第二条挂号 */
    result = RegistrationService_create(
        &context.service,
        "PAT0001",
        "DOC0001",
        "DEP0001",
        "2026-03-20T08:30",
        REG_TYPE_SPECIALIST,
        20.00,
        &second_created
    );
    assert(result.success == 1);
    assert(strcmp(second_created.registration_id, "REG0002") == 0);      /* ID递增 */

    /* 按挂号ID查找 */
    result = RegistrationService_find_by_registration_id(
        &context.service,
        "REG0002",
        &loaded
    );
    assert(result.success == 1);
    assert(strcmp(loaded.patient_id, "PAT0001") == 0);
    assert(strcmp(loaded.doctor_id, "DOC0001") == 0);
    assert(strcmp(loaded.department_id, "DEP0001") == 0);
    assert(strcmp(loaded.registered_at, "2026-03-20T08:30") == 0);

    /* 按患者ID查询 */
    LinkedList_init(&patient_records);
    result = RegistrationService_find_by_patient_id(
        &context.service,
        "PAT0001",
        0,             /* 无过滤条件 */
        &patient_records
    );
    assert(result.success == 1);
    assert(LinkedList_count(&patient_records) == 2); /* PAT0001有2条挂号记录 */
    assert(strcmp(registration_at(&patient_records, 0)->registration_id, "REG0001") == 0);
    assert(strcmp(registration_at(&patient_records, 1)->registration_id, "REG0002") == 0);
    RegistrationRepository_clear_list(&patient_records);
}

/**
 * @brief 测试拒绝不存在的关联实体
 *
 * 验证以下场景均应创建失败：
 * 1. 患者不存在（PAT9999）
 * 2. 医生不存在（DOC9999）
 * 3. 科室不存在（DEP9999）
 * 4. 医生不属于指定科室（DOC0001属DEP0001，但指定DEP0002）
 */
static void test_create_rejects_invalid_related_entities(void) {
    RegistrationServiceTestContext context;
    Registration created;
    Result result;

    setup_context(&context, "invalid_related_entities");

    /* 患者不存在 */
    result = RegistrationService_create(
        &context.service,
        "PAT9999",
        "DOC0001",
        "DEP0001",
        "2026-03-20T09:00",
        REG_TYPE_STANDARD,
        5.00,
        &created
    );
    assert(result.success == 0);

    /* 医生不存在 */
    result = RegistrationService_create(
        &context.service,
        "PAT0001",
        "DOC9999",
        "DEP0001",
        "2026-03-20T09:05",
        REG_TYPE_STANDARD,
        5.00,
        &created
    );
    assert(result.success == 0);

    /* 科室不存在 */
    result = RegistrationService_create(
        &context.service,
        "PAT0001",
        "DOC0001",
        "DEP9999",
        "2026-03-20T09:10",
        REG_TYPE_STANDARD,
        5.00,
        &created
    );
    assert(result.success == 0);

    /* 医生不属于指定科室（DOC0001属DEP0001，但这里指定DEP0002） */
    result = RegistrationService_create(
        &context.service,
        "PAT0001",
        "DOC0001",
        "DEP0002",
        "2026-03-20T09:15",
        REG_TYPE_STANDARD,
        5.00,
        &created
    );
    assert(result.success == 0);
}

/**
 * @brief 测试取消待诊状态的挂号
 *
 * 验证流程：
 * 1. 创建一条待诊挂号
 * 2. 取消该挂号
 * 3. 验证状态变为已取消且取消时间正确
 * 4. 从仓储重新加载验证持久化
 */
static void test_cancel_pending_registration(void) {
    RegistrationServiceTestContext context;
    Registration created;
    Registration cancelled;
    Registration loaded;
    Result result;

    setup_context(&context, "cancel_pending");

    /* 创建挂号 */
    result = RegistrationService_create(
        &context.service,
        "PAT0002",
        "DOC0002",
        "DEP0002",
        "2026-03-20T10:00",
        REG_TYPE_EMERGENCY,
        10.00,
        &created
    );
    assert(result.success == 1);

    /* 取消挂号 */
    result = RegistrationService_cancel(
        &context.service,
        created.registration_id,
        "2026-03-20T10:15",
        &cancelled
    );
    assert(result.success == 1);
    assert(cancelled.status == REG_STATUS_CANCELLED);                    /* 状态为已取消 */
    assert(strcmp(cancelled.cancelled_at, "2026-03-20T10:15") == 0);    /* 取消时间正确 */

    /* 从仓储重新加载验证 */
    result = RegistrationService_find_by_registration_id(
        &context.service,
        created.registration_id,
        &loaded
    );
    assert(result.success == 1);
    assert(loaded.status == REG_STATUS_CANCELLED);
    assert(strcmp(loaded.cancelled_at, "2026-03-20T10:15") == 0);
}

/**
 * @brief 测试拒绝取消已诊断的挂号
 *
 * 已诊断的挂号不允许取消，且取消操作后状态不变。
 */
static void test_cancel_rejects_diagnosed_registration(void) {
    RegistrationServiceTestContext context;
    Registration diagnosed;
    Registration loaded;
    Result result;

    setup_context(&context, "cancel_diagnosed");

    /* 手动插入一条已诊断的挂号记录 */
    diagnosed = make_registration(
        "REG0007",
        "PAT0001",
        "DOC0001",
        "DEP0001",
        "2026-03-20T11:00",
        REG_STATUS_DIAGNOSED,
        "2026-03-20T11:20",
        ""
    );
    result = RegistrationRepository_append(&context.registration_repository, &diagnosed);
    assert(result.success == 1);

    /* 尝试取消已诊断的挂号，应失败 */
    result = RegistrationService_cancel(
        &context.service,
        "REG0007",
        "2026-03-20T11:30",
        &loaded
    );
    assert(result.success == 0);

    /* 验证状态未改变 */
    result = RegistrationService_find_by_registration_id(&context.service, "REG0007", &loaded);
    assert(result.success == 1);
    assert(loaded.status == REG_STATUS_DIAGNOSED);                 /* 仍为已诊断 */
    assert(strcmp(loaded.diagnosed_at, "2026-03-20T11:20") == 0);  /* 诊断时间未变 */
    assert(strcmp(loaded.cancelled_at, "") == 0);                   /* 取消时间仍为空 */
}

/**
 * @brief 测试按医生查询（支持状态和时间范围过滤）
 *
 * 预埋3条挂号记录（同一医生），使用过滤条件：
 * - 状态=待诊
 * - 时间范围=08:10~08:40
 * 应只返回1条记录（REG0002，08:30）。
 */
static void test_query_by_doctor_with_status_and_time_filter(void) {
    RegistrationServiceTestContext context;
    RegistrationRepositoryFilter filter;
    Registration first;
    Registration second;
    Registration third;
    LinkedList registrations;
    Result result;

    setup_context(&context, "query_by_doctor");

    /* 预埋3条挂号记录 */
    first = make_registration(
        "REG0001",
        "PAT0001",
        "DOC0001",
        "DEP0001",
        "2026-03-20T08:00",      /* 在时间范围外 */
        REG_STATUS_PENDING,
        "",
        ""
    );
    second = make_registration(
        "REG0002",
        "PAT0002",
        "DOC0001",
        "DEP0001",
        "2026-03-20T08:30",      /* 在时间范围内，状态为待诊 */
        REG_STATUS_PENDING,
        "",
        ""
    );
    third = make_registration(
        "REG0003",
        "PAT0001",
        "DOC0001",
        "DEP0001",
        "2026-03-20T09:00",      /* 在时间范围外，状态为已诊断 */
        REG_STATUS_DIAGNOSED,
        "2026-03-20T09:20",
        ""
    );

    assert(RegistrationRepository_append(&context.registration_repository, &first).success == 1);
    assert(RegistrationRepository_append(&context.registration_repository, &second).success == 1);
    assert(RegistrationRepository_append(&context.registration_repository, &third).success == 1);

    /* 设置过滤条件：待诊状态 + 时间范围 08:10~08:40 */
    RegistrationRepositoryFilter_init(&filter);
    filter.use_status = 1;
    filter.status = REG_STATUS_PENDING;
    filter.registered_at_from = "2026-03-20T08:10";
    filter.registered_at_to = "2026-03-20T08:40";

    /* 按医生ID查询 */
    LinkedList_init(&registrations);
    result = RegistrationService_find_by_doctor_id(
        &context.service,
        "DOC0001",
        &filter,
        &registrations
    );
    assert(result.success == 1);
    assert(LinkedList_count(&registrations) == 1);                              /* 只有1条符合条件 */
    assert(strcmp(registration_at(&registrations, 0)->registration_id, "REG0002") == 0);
    RegistrationRepository_clear_list(&registrations);
}

/**
 * @brief 测试主函数，依次运行所有挂号服务测试用例
 */
int main(void) {
    test_create_find_and_query_by_patient();
    test_create_rejects_invalid_related_entities();
    test_cancel_pending_registration();
    test_cancel_rejects_diagnosed_registration();
    test_query_by_doctor_with_status_and_time_filter();
    return 0;
}
