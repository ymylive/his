/**
 * @file test_department_service.c
 * @brief 科室服务（DepartmentService）的单元测试文件
 *
 * 本文件测试科室管理服务的核心功能，包括：
 * - 科室的添加、更新和按ID查询
 * - 拒绝无效科室（名称为空）、重复科室ID和不存在的更新目标
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "service/DepartmentService.h"

/**
 * @brief 辅助函数：构建测试用的临时文件路径
 *
 * 使用时间戳和时钟周期生成唯一文件名。
 */
static void build_test_path(
    char *buffer,
    size_t buffer_size,
    const char *file_name
) {
    snprintf(
        buffer,
        buffer_size,
        "build/test_department_service_data/%ld_%lu_%s",
        (long)time(0),
        (unsigned long)clock(),
        file_name
    );
}

/**
 * @brief 测试科室的添加、更新和按ID查询功能
 *
 * 验证流程：
 * 1. 添加一个科室（Cardiology）
 * 2. 按ID查询验证名称和位置
 * 3. 更新科室信息
 * 4. 再次查询验证更新生效
 */
static void test_department_service_add_update_and_find(void) {
    char path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    DepartmentService service;
    Department created = {
        "DEP5001",
        "Cardiology",
        "Building A",
        "Heart disease clinic"
    };
    Department updated = {
        "DEP5001",
        "Cardiology Center",
        "Building B",
        "Expanded cardiology services"
    };
    Department loaded;
    Result result;

    build_test_path(path, sizeof(path), "departments.txt");

    /* 初始化科室服务 */
    result = DepartmentService_init(&service, path);
    assert(result.success == 1);

    /* 添加科室 */
    result = DepartmentService_add(&service, &created);
    assert(result.success == 1);

    /* 按ID查询，验证添加成功 */
    memset(&loaded, 0, sizeof(loaded));
    result = DepartmentService_get_by_id(&service, "DEP5001", &loaded);
    assert(result.success == 1);
    assert(strcmp(loaded.name, "Cardiology") == 0);      /* 验证科室名称 */
    assert(strcmp(loaded.location, "Building A") == 0);   /* 验证科室位置 */

    /* 更新科室信息 */
    result = DepartmentService_update(&service, &updated);
    assert(result.success == 1);

    /* 再次查询，验证更新生效 */
    memset(&loaded, 0, sizeof(loaded));
    result = DepartmentService_get_by_id(&service, "DEP5001", &loaded);
    assert(result.success == 1);
    assert(strcmp(loaded.name, "Cardiology Center") == 0);              /* 名称已更新 */
    assert(strcmp(loaded.location, "Building B") == 0);                 /* 位置已更新 */
    assert(strcmp(loaded.description, "Expanded cardiology services") == 0); /* 描述已更新 */
}

/**
 * @brief 测试拒绝无效科室、重复ID和不存在的更新目标
 *
 * 验证场景：
 * 1. 科室名称为空白时添加应失败
 * 2. 正常添加一个科室后，重复ID添加应失败
 * 3. 更新不存在的科室应失败
 */
static void test_department_service_rejects_invalid_duplicate_and_missing_update(void) {
    char path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    DepartmentService service;
    /* 名称为空白的科室 */
    Department missing_name = {
        "DEP5002",
        "   ",
        "Building C",
        "Missing name must fail"
    };
    /* 有效的科室 */
    Department valid = {
        "DEP5002",
        "Neurology",
        "Building C",
        "Brain and nerve clinic"
    };
    /* 重复ID的科室 */
    Department duplicate = {
        "DEP5002",
        "Neurology Annex",
        "Building D",
        "Duplicate id must fail"
    };
    /* 不存在的科室（用于更新） */
    Department missing = {
        "DEP5999",
        "Unknown",
        "Building X",
        "Update target missing"
    };
    Result result;

    build_test_path(path, sizeof(path), "departments_invalid.txt");

    result = DepartmentService_init(&service, path);
    assert(result.success == 1);

    /* 名称为空白，添加应失败 */
    result = DepartmentService_add(&service, &missing_name);
    assert(result.success == 0);

    /* 正常添加 */
    result = DepartmentService_add(&service, &valid);
    assert(result.success == 1);

    /* 重复ID，添加应失败 */
    result = DepartmentService_add(&service, &duplicate);
    assert(result.success == 0);

    /* 更新不存在的科室，应失败 */
    result = DepartmentService_update(&service, &missing);
    assert(result.success == 0);
}

/**
 * @brief 科室服务测试入口函数（供外部调用）
 *
 * 依次运行所有科室服务测试用例。
 */
void test_department_service_run(void) {
    test_department_service_add_update_and_find();
    test_department_service_rejects_invalid_duplicate_and_missing_update();
}
