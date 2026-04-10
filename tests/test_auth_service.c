/**
 * @file test_auth_service.c
 * @brief 认证服务（AuthService）的单元测试文件
 *
 * 本文件测试用户认证服务的核心功能，包括：
 * - 患者账号绑定与登录验证
 * - 注册时要求患者记录已存在
 * - 密码错误和角色不匹配时拒绝认证
 * - 空密码和无效角色的注册拒绝
 * - 预置演示账号的认证验证
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "domain/Patient.h"
#include "service/AuthService.h"
#include "service/PatientService.h"

/**
 * @brief 辅助函数：从文件中读取内容到缓冲区
 * @param buffer 目标缓冲区
 * @param buffer_size 缓冲区大小
 * @param path 文件路径
 */
static void TestAuthService_read_file(char *buffer, size_t buffer_size, const char *path) {
    FILE *file = 0;
    size_t read_size = 0;

    assert(buffer != 0);
    assert(path != 0);
    memset(buffer, 0, buffer_size); /* 清空缓冲区 */

    file = fopen(path, "r");
    assert(file != 0);
    read_size = fread(buffer, 1, buffer_size - 1, file); /* 读取文件内容 */
    buffer[read_size] = '\0'; /* 确保字符串以空字符结尾 */
    fclose(file);
}

/**
 * @brief 辅助函数：构建测试用的临时文件路径
 *
 * 使用当前时间戳生成唯一文件名，避免测试之间互相干扰。
 * @param buffer 目标路径缓冲区
 * @param buffer_size 缓冲区大小
 * @param name 文件名前缀
 */
static void TestAuthService_make_path(char *buffer, size_t buffer_size, const char *name) {
    assert(buffer != 0);
    assert(name != 0);

    snprintf(
        buffer,
        buffer_size,
        "build/test_auth_service_data/%s_%ld.txt",
        name,
        (long)time(0)
    );
    remove(buffer); /* 若文件已存在则先删除，保证测试环境干净 */
}

/**
 * @brief 辅助函数：构建测试夹具（fixture）文件的绝对路径
 *
 * 通过 __FILE__ 宏获取当前源文件路径，再向上回退两级目录，
 * 拼接出相对于项目根目录的夹具文件路径。
 * @param buffer 目标路径缓冲区
 * @param buffer_size 缓冲区大小
 * @param relative_path 相对路径
 */
static void TestAuthService_make_fixture_path(
    char *buffer,
    size_t buffer_size,
    const char *relative_path
) {
    char path_buffer[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    char *last_separator = 0;
    char *parent_separator = 0;

    assert(buffer != 0);
    assert(relative_path != 0);

    /* 获取当前源文件的完整路径 */
    snprintf(path_buffer, sizeof(path_buffer), "%s", __FILE__);

    /* 查找最后一个路径分隔符（兼容 Windows 和 Unix） */
    last_separator = strrchr(path_buffer, '\\');
    if (last_separator == 0) {
        last_separator = strrchr(path_buffer, '/');
    }
    assert(last_separator != 0);
    *last_separator = '\0'; /* 截断到当前文件所在目录 */

    /* 再向上一级目录 */
    parent_separator = strrchr(path_buffer, '\\');
    if (parent_separator == 0) {
        parent_separator = strrchr(path_buffer, '/');
    }
    assert(parent_separator != 0);
    *parent_separator = '\0'; /* 截断到父目录 */

    /* 拼接最终路径 */
    snprintf(buffer, buffer_size, "%s/%s", path_buffer, relative_path);
}

/**
 * @brief 辅助函数：复制文件内容
 * @param source_path 源文件路径
 * @param target_path 目标文件路径
 */
static void TestAuthService_copy_file(const char *source_path, const char *target_path) {
    FILE *source = 0;
    FILE *target = 0;
    char buffer[256];
    size_t read_size = 0;

    assert(source_path != 0);
    assert(target_path != 0);

    source = fopen(source_path, "rb");
    assert(source != 0);
    target = fopen(target_path, "wb");
    assert(target != 0);

    /* 循环读取源文件并写入目标文件 */
    while ((read_size = fread(buffer, 1, sizeof(buffer), source)) > 0) {
        assert(fwrite(buffer, 1, read_size, target) == read_size);
    }

    fclose(target);
    fclose(source);
}

/**
 * @brief 辅助函数：在患者数据文件中预埋一条患者记录
 *
 * 用于测试前准备数据，确保认证服务所需的患者记录已存在。
 * @param patient_path 患者数据文件路径
 * @param patient_id 患者ID
 */
static void TestAuthService_seed_patient(const char *patient_path, const char *patient_id) {
    PatientService patient_service;
    Patient patient;
    Result result = PatientService_init(&patient_service, patient_path);

    assert(result.success == 1);
    memset(&patient, 0, sizeof(patient));
    strcpy(patient.patient_id, patient_id);
    strcpy(patient.name, "Auth Patient");
    patient.gender = PATIENT_GENDER_FEMALE;
    patient.age = 30;
    strcpy(patient.contact, "13800009999");
    strcpy(patient.id_card, "110101199001010011");
    strcpy(patient.allergy, "None");
    strcpy(patient.medical_history, "Healthy");
    patient.is_inpatient = 0;
    strcpy(patient.remarks, "");

    result = PatientService_create_patient(&patient_service, &patient);
    assert(result.success == 1);
}

/**
 * @brief 测试患者账号绑定及登录成功的完整流程
 *
 * 验证流程：
 * 1. 先创建患者记录
 * 2. 注册用户（绑定患者ID与密码）
 * 3. 验证密码已加密存储（文件中不含明文密码）
 * 4. 使用正确密码登录，验证认证成功
 */
static void test_patient_binding_and_login_success(void) {
    char user_path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    char patient_path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    char saved_content[TEXT_FILE_REPOSITORY_LINE_CAPACITY];
    AuthService service;
    User user;
    Result result;

    /* 构建测试用临时文件路径 */
    TestAuthService_make_path(user_path, sizeof(user_path), "users");
    TestAuthService_make_path(patient_path, sizeof(patient_path), "patients");
    /* 预埋患者记录 */
    TestAuthService_seed_patient(patient_path, "PAT9001");

    /* 初始化认证服务 */
    result = AuthService_init(&service, user_path, patient_path);
    assert(result.success == 1);

    /* 注册用户，绑定患者ID */
    result = AuthService_register_user(&service, "PAT9001", "safe-pass", USER_ROLE_PATIENT);
    assert(result.success == 1);

    /* 验证文件中包含用户ID但不包含明文密码（密码已加密） */
    TestAuthService_read_file(saved_content, sizeof(saved_content), user_path);
    assert(strstr(saved_content, "PAT9001") != 0);      /* 用户ID应存在 */
    assert(strstr(saved_content, "safe-pass") == 0);     /* 明文密码不应存在 */

    /* 使用正确密码和角色进行认证 */
    result = AuthService_authenticate(
        &service,
        "PAT9001",
        "safe-pass",
        USER_ROLE_PATIENT,
        &user
    );
    assert(result.success == 1);
    assert(strcmp(user.user_id, "PAT9001") == 0);  /* 验证返回的用户ID正确 */
    assert(user.role == USER_ROLE_PATIENT);         /* 验证返回的角色正确 */
}

/**
 * @brief 测试患者注册时要求患者记录已存在
 *
 * 当患者记录不存在时，注册应当失败。
 */
static void test_patient_registration_requires_existing_patient_record(void) {
    char user_path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    char patient_path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    AuthService service;
    Result result;

    TestAuthService_make_path(user_path, sizeof(user_path), "missing_patient_users");
    TestAuthService_make_path(patient_path, sizeof(patient_path), "missing_patient_patients");

    result = AuthService_init(&service, user_path, patient_path);
    assert(result.success == 1);

    /* 尝试为不存在的患者注册账号，应当失败 */
    result = AuthService_register_user(&service, "PAT9999", "safe-pass", USER_ROLE_PATIENT);
    assert(result.success == 0);
}

/**
 * @brief 测试认证拒绝错误密码和角色不匹配
 *
 * 验证场景：
 * 1. 使用错误密码登录，应当失败
 * 2. 使用正确密码但错误角色登录，应当失败
 */
static void test_authenticate_rejects_wrong_password_and_role_mismatch(void) {
    char user_path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    char patient_path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    AuthService service;
    User user;
    Result result;

    TestAuthService_make_path(user_path, sizeof(user_path), "mismatch_users");
    TestAuthService_make_path(patient_path, sizeof(patient_path), "mismatch_patients");
    TestAuthService_seed_patient(patient_path, "PAT9010");

    result = AuthService_init(&service, user_path, patient_path);
    assert(result.success == 1);

    /* 先注册用户 */
    result = AuthService_register_user(&service, "PAT9010", "safe-pass", USER_ROLE_PATIENT);
    assert(result.success == 1);

    /* 使用错误密码认证，应当失败 */
    result = AuthService_authenticate(&service, "PAT9010", "wrong-pass", USER_ROLE_PATIENT, &user);
    assert(result.success == 0);

    /* 使用正确密码但错误角色认证，应当失败 */
    result = AuthService_authenticate(&service, "PAT9010", "safe-pass", USER_ROLE_DOCTOR, &user);
    assert(result.success == 0);
}

/**
 * @brief 测试注册拒绝空密码和无效角色
 *
 * 验证场景：
 * 1. 空密码注册应当失败
 * 2. 未知角色（USER_ROLE_UNKNOWN）注册应当失败
 */
static void test_register_rejects_blank_password_and_invalid_role(void) {
    char user_path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    char patient_path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    AuthService service;
    Result result;

    TestAuthService_make_path(user_path, sizeof(user_path), "validate_users");
    TestAuthService_make_path(patient_path, sizeof(patient_path), "validate_patients");
    TestAuthService_seed_patient(patient_path, "PAT9020");

    result = AuthService_init(&service, user_path, patient_path);
    assert(result.success == 1);

    /* 空密码注册应当失败 */
    result = AuthService_register_user(&service, "PAT9020", "", USER_ROLE_PATIENT);
    assert(result.success == 0);

    /* 无效角色注册应当失败 */
    result = AuthService_register_user(&service, "PAT9020", "safe-pass", USER_ROLE_UNKNOWN);
    assert(result.success == 0);
}

/**
 * @brief 测试预置演示账号能够正常认证
 *
 * 从测试夹具目录复制预置的用户数据文件，验证：
 * 1. 文件中包含加密标记（'$'）
 * 2. 预置的管理员账号可以正常登录
 */
static void test_authenticate_accepts_seeded_demo_accounts(void) {
    char fixture_user_path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    char copied_user_path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    char saved_content[TEXT_FILE_REPOSITORY_LINE_CAPACITY * 8];
    AuthService service;
    User user;
    Result result;

    /* 获取夹具文件路径并复制到测试目录 */
    TestAuthService_make_fixture_path(fixture_user_path, sizeof(fixture_user_path), "data/users.txt");
    TestAuthService_make_path(copied_user_path, sizeof(copied_user_path), "seeded_demo_users");
    TestAuthService_copy_file(fixture_user_path, copied_user_path);

    /* 验证复制后的文件包含密码哈希标记 '$' */
    TestAuthService_read_file(saved_content, sizeof(saved_content), copied_user_path);
    assert(strstr(saved_content, "$") != 0);

    /* 使用复制的用户文件初始化认证服务（不需要患者文件） */
    result = AuthService_init(&service, copied_user_path, "");

    assert(result.success == 1);

    /* 验证预置管理员账号可以正常认证 */
    result = AuthService_authenticate(&service, "ADM0001", "admin123", USER_ROLE_ADMIN, &user);
    assert(result.success == 1);
    assert(strcmp(user.user_id, "ADM0001") == 0);
    assert(user.role == USER_ROLE_ADMIN);
}

/**
 * @brief 测试主函数，依次运行所有认证服务测试用例
 */
int main(void) {
    test_patient_binding_and_login_success();
    test_patient_registration_requires_existing_patient_record();
    test_authenticate_rejects_wrong_password_and_role_mismatch();
    test_register_rejects_blank_password_and_invalid_role();
    test_authenticate_accepts_seeded_demo_accounts();
    return 0;
}
