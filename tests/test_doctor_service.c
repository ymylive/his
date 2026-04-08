/**
 * @file test_doctor_service.c
 * @brief 医生服务（DoctorService）的单元测试文件
 *
 * 本文件测试医生管理服务的核心功能，包括：
 * - 添加医生、按ID查找、按科室列表查询
 * - 拒绝各种无效输入（缺少ID、姓名、职称、科室、排班、无效状态）
 * - 拒绝重复ID和不存在的科室
 *
 * 同时集成运行科室服务（DepartmentService）的测试。
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "service/DepartmentService.h"
#include "service/DoctorService.h"

/* 声明科室服务测试的入口函数（定义在 test_department_service.c） */
void test_department_service_run(void);

/**
 * @brief 辅助函数：构建测试用的临时文件路径
 */
static void build_test_path(
    char *buffer,
    size_t buffer_size,
    const char *file_name
) {
    snprintf(
        buffer,
        buffer_size,
        "build/test_doctor_service_data/%ld_%lu_%s",
        (long)time(0),
        (unsigned long)clock(),
        file_name
    );
}

/**
 * @brief 辅助函数：在科室数据文件中预埋一条科室记录
 * @param department_path 科室数据文件路径
 * @param department 要添加的科室对象
 */
static void seed_department(
    const char *department_path,
    const Department *department
) {
    DepartmentService department_service;
    Result result = DepartmentService_init(&department_service, department_path);

    assert(result.success == 1);
    result = DepartmentService_add(&department_service, department);
    assert(result.success == 1);
}

/**
 * @brief 测试添加医生、按ID查找和按科室列表查询
 *
 * 验证流程：
 * 1. 预埋两个科室（心内科和外科）
 * 2. 添加两名医生到不同科室
 * 3. 按ID查找验证医生详细信息
 * 4. 按科室ID查询，验证只返回该科室下的医生
 */
static void test_doctor_service_add_find_and_list_by_department(void) {
    char department_path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    char doctor_path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    DoctorService service;
    Department cardiology = {
        "DEP6001",
        "Cardiology",
        "Block A",
        "Cardiology clinic"
    };
    Department surgery = {
        "DEP6002",
        "Surgery",
        "Block B",
        "Surgical clinic"
    };
    Doctor doctor_a = {
        "DOC6001",
        "Dr.Alice",
        "Chief Physician",
        "DEP6001",
        "Mon AM;Wed PM",
        DOCTOR_STATUS_ACTIVE
    };
    Doctor doctor_b = {
        "DOC6002",
        "Dr.Bob",
        "Attending",
        "DEP6002",
        "Tue AM;Thu PM",
        DOCTOR_STATUS_ACTIVE
    };
    Doctor loaded;
    LinkedList doctors_in_cardiology;
    Result result;

    build_test_path(department_path, sizeof(department_path), "departments.txt");
    build_test_path(doctor_path, sizeof(doctor_path), "doctors.txt");

    /* 预埋科室数据 */
    seed_department(department_path, &cardiology);
    seed_department(department_path, &surgery);

    /* 初始化医生服务 */
    result = DoctorService_init(&service, doctor_path, department_path);
    assert(result.success == 1);

    /* 添加两名医生 */
    result = DoctorService_add(&service, &doctor_a);
    assert(result.success == 1);
    result = DoctorService_add(&service, &doctor_b);
    assert(result.success == 1);

    /* 按医生ID查找，验证详细信息 */
    memset(&loaded, 0, sizeof(loaded));
    result = DoctorService_get_by_id(&service, "DOC6001", &loaded);
    assert(result.success == 1);
    assert(strcmp(loaded.name, "Dr.Alice") == 0);           /* 验证姓名 */
    assert(strcmp(loaded.title, "Chief Physician") == 0);   /* 验证职称 */
    assert(strcmp(loaded.schedule, "Mon AM;Wed PM") == 0);  /* 验证排班 */

    /* 按科室查询心内科医生 */
    result = DoctorService_list_by_department(
        &service,
        "DEP6001",
        &doctors_in_cardiology
    );
    assert(result.success == 1);
    assert(LinkedList_count(&doctors_in_cardiology) == 1); /* 心内科只有一名医生 */
    assert(
        strcmp(
            ((Doctor *)doctors_in_cardiology.head->data)->doctor_id,
            "DOC6001"
        ) == 0
    );
    DoctorService_clear_list(&doctors_in_cardiology);
}

/**
 * @brief 测试拒绝各种无效医生数据、重复ID和不存在的科室
 *
 * 验证以下场景均应添加失败：
 * 1. 医生ID为空
 * 2. 姓名为空白
 * 3. 职称为空
 * 4. 科室ID为空白
 * 5. 排班为空白
 * 6. 无效的状态枚举值
 * 7. 重复的医生ID
 * 8. 关联不存在的科室ID
 */
static void test_doctor_service_rejects_invalid_duplicate_and_unknown_department(void) {
    char department_path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    char doctor_path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    DoctorService service;
    Department department = {
        "DEP6101",
        "Pediatrics",
        "Block C",
        "Children clinic"
    };
    /* 各种无效医生数据 */
    Doctor missing_id = {
        "",
        "Dr.Carol",
        "Attending",
        "DEP6101",
        "Fri AM",
        DOCTOR_STATUS_ACTIVE
    };
    Doctor missing_name = {
        "DOC6101",
        "   ",
        "Attending",
        "DEP6101",
        "Fri AM",
        DOCTOR_STATUS_ACTIVE
    };
    Doctor missing_title = {
        "DOC6102",
        "Dr.David",
        "",
        "DEP6101",
        "Fri PM",
        DOCTOR_STATUS_ACTIVE
    };
    Doctor missing_department = {
        "DOC6103",
        "Dr.Erin",
        "Resident",
        " ",
        "Sat AM",
        DOCTOR_STATUS_ACTIVE
    };
    Doctor missing_schedule = {
        "DOC6104",
        "Dr.Frank",
        "Resident",
        "DEP6101",
        "  ",
        DOCTOR_STATUS_ACTIVE
    };
    Doctor invalid_status = {
        "DOC6105",
        "Dr.Grace",
        "Resident",
        "DEP6101",
        "Sun AM",
        (DoctorStatus)99
    };
    Doctor valid = {
        "DOC6106",
        "Dr.Helen",
        "Chief Physician",
        "DEP6101",
        "Mon-Fri AM",
        DOCTOR_STATUS_ACTIVE
    };
    Doctor duplicate_id = {
        "DOC6106",
        "Dr.Ian",
        "Attending",
        "DEP6101",
        "Tue PM",
        DOCTOR_STATUS_ACTIVE
    };
    Doctor unknown_department = {
        "DOC6107",
        "Dr.Jane",
        "Attending",
        "DEP6199",
        "Wed AM",
        DOCTOR_STATUS_ACTIVE
    };
    Result result;

    build_test_path(department_path, sizeof(department_path), "departments_invalid.txt");
    build_test_path(doctor_path, sizeof(doctor_path), "doctors_invalid.txt");

    /* 预埋科室数据 */
    seed_department(department_path, &department);

    result = DoctorService_init(&service, doctor_path, department_path);
    assert(result.success == 1);

    /* 逐一验证各种无效输入均被拒绝 */
    result = DoctorService_add(&service, &missing_id);          /* 缺少ID */
    assert(result.success == 0);
    result = DoctorService_add(&service, &missing_name);        /* 缺少姓名 */
    assert(result.success == 0);
    result = DoctorService_add(&service, &missing_title);       /* 缺少职称 */
    assert(result.success == 0);
    result = DoctorService_add(&service, &missing_department);  /* 缺少科室 */
    assert(result.success == 0);
    result = DoctorService_add(&service, &missing_schedule);    /* 缺少排班 */
    assert(result.success == 0);
    result = DoctorService_add(&service, &invalid_status);      /* 无效状态 */
    assert(result.success == 0);

    /* 添加一个有效医生 */
    result = DoctorService_add(&service, &valid);
    assert(result.success == 1);

    /* 重复ID，添加应失败 */
    result = DoctorService_add(&service, &duplicate_id);
    assert(result.success == 0);

    /* 关联不存在的科室，添加应失败 */
    result = DoctorService_add(&service, &unknown_department);
    assert(result.success == 0);
}

/**
 * @brief 医生服务测试入口函数（供外部调用）
 */
void test_doctor_service_run(void) {
    test_doctor_service_add_find_and_list_by_department();
    test_doctor_service_rejects_invalid_duplicate_and_unknown_department();
}

/**
 * @brief 测试主函数
 *
 * 先运行科室服务测试，再运行医生服务测试。
 */
int main(void) {
    test_department_service_run();  /* 运行科室服务测试 */
    test_doctor_service_run();     /* 运行医生服务测试 */
    return 0;
}
