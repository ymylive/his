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
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "domain/Patient.h"
#include "repository/UserRepository.h"
#include "service/AuthService.h"
#include "service/AuthServiceTestHooks.h"
#include "service/PatientService.h"

/* 可控测试时钟：测试用例通过赋值 fake_now 控制当前时间 */
static long fake_now = 0;
static long TestAuthService_fake_clock(void) { return fake_now; }

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
    result = AuthService_register_user(&service, "testuser1", "safe-pass", USER_ROLE_PATIENT, "PAT9001");
    assert(result.success == 1);

    /* 验证文件中包含用户名但不包含明文密码（密码已加密） */
    TestAuthService_read_file(saved_content, sizeof(saved_content), user_path);
    assert(strstr(saved_content, "testuser1") != 0);     /* 用户名应存在 */
    assert(strstr(saved_content, "PAT9001") != 0);       /* 患者编号应存在 */
    assert(strstr(saved_content, "safe-pass") == 0);     /* 明文密码不应存在 */

    /* 使用用户名和正确密码进行认证 */
    result = AuthService_authenticate(
        &service,
        "testuser1",
        "safe-pass",
        USER_ROLE_PATIENT,
        &user
    );
    assert(result.success == 1);
    assert(strcmp(user.user_id, "testuser1") == 0);  /* 验证返回的用户名正确 */
    assert(strcmp(user.patient_id, "PAT9001") == 0); /* 验证返回的患者编号正确 */
    assert(user.role == USER_ROLE_PATIENT);           /* 验证返回的角色正确 */
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
    result = AuthService_register_user(&service, "testuser2", "safe-pass", USER_ROLE_PATIENT, "PAT9999");
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
    result = AuthService_register_user(&service, "testuser3", "safe-pass", USER_ROLE_PATIENT, "PAT9010");
    assert(result.success == 1);

    /* 使用错误密码认证，应当失败 */
    result = AuthService_authenticate(&service, "testuser3", "wrong-pass", USER_ROLE_PATIENT, &user);
    assert(result.success == 0);

    /* 使用正确密码但错误角色认证，应当失败 */
    result = AuthService_authenticate(&service, "testuser3", "safe-pass", USER_ROLE_DOCTOR, &user);
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
    result = AuthService_register_user(&service, "testuser4", "", USER_ROLE_PATIENT, "PAT9020");
    assert(result.success == 0);

    /* 无效角色注册应当失败 */
    result = AuthService_register_user(&service, "testuser4", "safe-pass", USER_ROLE_UNKNOWN, "PAT9020");
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
 * @brief 验证连续 5 次密码错误后账号被锁定；到期前即使密码正确也被拒。
 *
 * 步骤：
 * 1. 注册用户
 * 2. 注入固定的测试时钟（fake_now = 1000）
 * 3. 连续 5 次使用错误密码登录，确认全部失败
 * 4. 第 6 次使用【正确密码】登录，应被拒绝为 "account temporarily locked"
 * 5. 将时钟推进到锁定到期之后（+901 秒），再次使用正确密码登录应成功
 */
static void test_authenticate_lockout_after_failures(void) {
    char user_path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    char patient_path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    AuthService service;
    User user;
    Result result;
    int i = 0;

    TestAuthService_make_path(user_path, sizeof(user_path), "lockout_users");
    TestAuthService_make_path(patient_path, sizeof(patient_path), "lockout_patients");
    TestAuthService_seed_patient(patient_path, "PAT9101");

    /* 注入测试时钟 */
    fake_now = 1000;
    AuthService_set_clock_for_testing(TestAuthService_fake_clock);

    result = AuthService_init(&service, user_path, patient_path);
    assert(result.success == 1);

    result = AuthService_register_user(&service, "lockuser", "safe-pass",
                                       USER_ROLE_PATIENT, "PAT9101");
    assert(result.success == 1);

    /* 连续 5 次错密码 → 账号进入锁定 */
    for (i = 0; i < 5; i++) {
        result = AuthService_authenticate(&service, "lockuser", "wrong-pass",
                                          USER_ROLE_PATIENT, &user);
        assert(result.success == 0);
    }

    /* 第 6 次即便密码正确也应被拒绝为 locked */
    result = AuthService_authenticate(&service, "lockuser", "safe-pass",
                                      USER_ROLE_PATIENT, &user);
    assert(result.success == 0);
    assert(strstr(result.message, "locked") != 0);

    /* 锁定期内（900 秒），再多尝试也仍然 locked */
    fake_now = 1000 + 100;
    result = AuthService_authenticate(&service, "lockuser", "safe-pass",
                                      USER_ROLE_PATIENT, &user);
    assert(result.success == 0);
    assert(strstr(result.message, "locked") != 0);

    /* 推进时钟越过锁定期（+901 秒） → 正确密码应登录成功并清零计数 */
    fake_now = 1000 + 901;
    result = AuthService_authenticate(&service, "lockuser", "safe-pass",
                                      USER_ROLE_PATIENT, &user);
    assert(result.success == 1);
    assert(strcmp(user.user_id, "lockuser") == 0);

    /* 计数清零后，再次错误不会立即触发锁定（需要重新积累到 5 次） */
    result = AuthService_authenticate(&service, "lockuser", "wrong-pass",
                                      USER_ROLE_PATIENT, &user);
    assert(result.success == 0);
    /* 紧接着正确密码应能通过，证明失败计数已在上一次成功登录时清零 */
    result = AuthService_authenticate(&service, "lockuser", "safe-pass",
                                      USER_ROLE_PATIENT, &user);
    assert(result.success == 1);

    /* 恢复默认时钟 */
    AuthService_set_clock_for_testing(0);
}

/**
 * @brief 验证登录成功可清零之前累计的失败计数
 */
static void test_successful_login_resets_failure_counter(void) {
    char user_path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    char patient_path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    AuthService service;
    User user;
    Result result;
    int i = 0;

    TestAuthService_make_path(user_path, sizeof(user_path), "reset_counter_users");
    TestAuthService_make_path(patient_path, sizeof(patient_path), "reset_counter_patients");
    TestAuthService_seed_patient(patient_path, "PAT9102");

    fake_now = 2000;
    AuthService_set_clock_for_testing(TestAuthService_fake_clock);

    result = AuthService_init(&service, user_path, patient_path);
    assert(result.success == 1);
    result = AuthService_register_user(&service, "resetuser", "safe-pass",
                                       USER_ROLE_PATIENT, "PAT9102");
    assert(result.success == 1);

    /* 4 次失败（未触发锁定） */
    for (i = 0; i < 4; i++) {
        result = AuthService_authenticate(&service, "resetuser", "wrong-pass",
                                          USER_ROLE_PATIENT, &user);
        assert(result.success == 0);
    }

    /* 正确密码登录成功，失败计数应被清零 */
    result = AuthService_authenticate(&service, "resetuser", "safe-pass",
                                      USER_ROLE_PATIENT, &user);
    assert(result.success == 1);

    /* 再 4 次失败，仍不应锁定（因为计数已清零） */
    for (i = 0; i < 4; i++) {
        result = AuthService_authenticate(&service, "resetuser", "wrong-pass",
                                          USER_ROLE_PATIENT, &user);
        assert(result.success == 0);
    }
    /* 此时第 5 次正确密码仍然应成功（未进入锁定） */
    result = AuthService_authenticate(&service, "resetuser", "safe-pass",
                                      USER_ROLE_PATIENT, &user);
    assert(result.success == 1);

    AuthService_set_clock_for_testing(0);
}

/**
 * @brief 辅助：将十六进制字符串解析为字节数组（测试内部使用，无需常量时间）
 */
static void TestAuthService_hex_to_bytes(const char *hex, uint8_t *out, size_t out_len) {
    size_t i = 0;
    for (i = 0; i < out_len; i++) {
        unsigned int byte = 0;
        assert(sscanf(hex + i * 2, "%2x", &byte) == 1);
        out[i] = (uint8_t)byte;
    }
}

/**
 * @brief 辅助：搜索包含 user_id 前缀的行，返回该行的 password_hash 字段副本
 */
static void TestAuthService_extract_hash_field(
    const char *file_path,
    const char *user_id,
    char *out_field,
    size_t out_capacity
) {
    FILE *file = 0;
    char line[TEXT_FILE_REPOSITORY_LINE_CAPACITY];
    size_t uid_len = strlen(user_id);

    assert(out_capacity > 0);
    out_field[0] = '\0';

    file = fopen(file_path, "r");
    assert(file != 0);
    while (fgets(line, sizeof(line), file) != 0) {
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
            line[--len] = '\0';
        }
        if (strncmp(line, user_id, uid_len) == 0 && line[uid_len] == '|') {
            const char *start = line + uid_len + 1;
            const char *end = strchr(start, '|');
            size_t copy_len = 0;
            if (end == 0) copy_len = strlen(start);
            else copy_len = (size_t)(end - start);
            if (copy_len >= out_capacity) copy_len = out_capacity - 1;
            memcpy(out_field, start, copy_len);
            out_field[copy_len] = '\0';
            break;
        }
    }
    fclose(file);
}

/**
 * @brief 验证首次登录会把旧 FNV 加盐哈希惰性迁移为 pbkdf2v1
 *
 * 场景：手工构造一条用户记录，其 password_hash 为旧 FNV 加盐格式
 *       （salt$hash，对应密码 "safe-pass"）。调用 authenticate：
 *       - 正确密码应成功
 *       - 文件中对应字段应被重写为 pbkdf2v1 开头
 *       - 再次登录仍然成功（新格式生效）
 *       - 错误密码仍然失败，且不写回文件
 */
static void test_legacy_hash_migration_on_login(void) {
    /* 旧 FNV 加盐哈希来自项目现有种子数据（"admin123" 对应 ADM0001）。
     * 为了保持测试独立于 DemoData，我们直接用一份已知有效的旧哈希。 */
    const char *legacy_admin_user_id = "ADM0001";
    const char *legacy_admin_password = "admin123";
    const char *legacy_admin_hash =
        "0a1b2c3d4e5f60718293a4b5c6d7e8f9"
        "$"
        "58e41e727b432fc54a5e66b2222d7f14a44709f5b22b6acd856d5545e55eded5";

    char user_path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    char hash_before[HIS_USER_PASSWORD_HASH_CAPACITY];
    char hash_after[HIS_USER_PASSWORD_HASH_CAPACITY];
    char hash_after_wrong[HIS_USER_PASSWORD_HASH_CAPACITY];
    FILE *seed_file = 0;
    AuthService service;
    User user;
    Result result;

    TestAuthService_make_path(user_path, sizeof(user_path), "migration_users");

    /* 手工预埋旧格式用户记录（含所有字段，匹配当前 USER_REPOSITORY_HEADER） */
    seed_file = fopen(user_path, "w");
    assert(seed_file != 0);
    fprintf(seed_file, "%s\n", USER_REPOSITORY_HEADER);
    fprintf(seed_file, "%s|%s|%d||0|0|0\n",
            legacy_admin_user_id, legacy_admin_hash, (int)USER_ROLE_ADMIN);
    fclose(seed_file);

    /* 记录旧哈希字段作为对照 */
    TestAuthService_extract_hash_field(user_path, legacy_admin_user_id,
                                       hash_before, sizeof(hash_before));
    assert(strcmp(hash_before, legacy_admin_hash) == 0);
    assert(strncmp(hash_before, "pbkdf2v1$", 9) != 0);

    result = AuthService_init(&service, user_path, "");
    assert(result.success == 1);

    /* 用正确密码登录 → 成功且触发迁移 */
    result = AuthService_authenticate(&service, legacy_admin_user_id,
                                      legacy_admin_password,
                                      USER_ROLE_ADMIN, &user);
    assert(result.success == 1);
    assert(strcmp(user.user_id, legacy_admin_user_id) == 0);

    /* 文件中该用户字段应已升级为 pbkdf2v1 格式 */
    TestAuthService_extract_hash_field(user_path, legacy_admin_user_id,
                                       hash_after, sizeof(hash_after));
    assert(strncmp(hash_after, "pbkdf2v1$", 9) == 0);
    assert(strcmp(hash_after, hash_before) != 0);

    /* 再次登录，用新格式路径仍能成功 */
    memset(&user, 0, sizeof(user));
    result = AuthService_authenticate(&service, legacy_admin_user_id,
                                      legacy_admin_password,
                                      USER_ROLE_ADMIN, &user);
    assert(result.success == 1);

    /* 错误密码 → 失败，且文件不被再次改写 */
    result = AuthService_authenticate(&service, legacy_admin_user_id,
                                      "definitely-wrong",
                                      USER_ROLE_ADMIN, &user);
    assert(result.success == 0);
    TestAuthService_extract_hash_field(user_path, legacy_admin_user_id,
                                       hash_after_wrong,
                                       sizeof(hash_after_wrong));
    assert(strcmp(hash_after_wrong, hash_after) == 0);
}

/**
 * @brief 新用户注册时直接写入 pbkdf2v1 格式，验证后续登录也走新路径
 */
static void test_new_user_uses_pbkdf2_format(void) {
    char user_path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    char patient_path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    char hash_field[HIS_USER_PASSWORD_HASH_CAPACITY];
    AuthService service;
    User user;
    Result result;

    TestAuthService_make_path(user_path, sizeof(user_path), "pbkdf2_new_users");
    TestAuthService_make_path(patient_path, sizeof(patient_path),
                              "pbkdf2_new_patients");
    TestAuthService_seed_patient(patient_path, "PAT9201");

    result = AuthService_init(&service, user_path, patient_path);
    assert(result.success == 1);

    result = AuthService_register_user(&service, "pbkdf2user", "safe-pass",
                                       USER_ROLE_PATIENT, "PAT9201");
    assert(result.success == 1);

    TestAuthService_extract_hash_field(user_path, "pbkdf2user",
                                       hash_field, sizeof(hash_field));
    /* 必须是 pbkdf2v1 开头，且包含迭代次数与两段 $ 分隔的字段 */
    assert(strncmp(hash_field, "pbkdf2v1$", 9) == 0);
    {
        const char *iter_sep = strchr(hash_field + 9, '$');
        const char *salt_sep = 0;
        assert(iter_sep != 0);
        salt_sep = strchr(iter_sep + 1, '$');
        assert(salt_sep != 0);
        /* 盐值 32 个十六进制字符，哈希 64 个十六进制字符 */
        assert((size_t)(salt_sep - iter_sep - 1) == 32);
        assert(strlen(salt_sep + 1) == 64);
    }

    /* 登录同样应成功 */
    result = AuthService_authenticate(&service, "pbkdf2user", "safe-pass",
                                      USER_ROLE_PATIENT, &user);
    assert(result.success == 1);

    /* 改密后仍是新格式 */
    result = AuthService_change_password(&service, "pbkdf2user",
                                         "safe-pass", "new-pass");
    assert(result.success == 1);
    TestAuthService_extract_hash_field(user_path, "pbkdf2user",
                                       hash_field, sizeof(hash_field));
    assert(strncmp(hash_field, "pbkdf2v1$", 9) == 0);

    /* 新密码可登录 */
    result = AuthService_authenticate(&service, "pbkdf2user", "new-pass",
                                      USER_ROLE_PATIENT, &user);
    assert(result.success == 1);
    /* 旧密码不可登录 */
    result = AuthService_authenticate(&service, "pbkdf2user", "safe-pass",
                                      USER_ROLE_PATIENT, &user);
    assert(result.success == 0);
}

/**
 * @brief 使用 RFC 7914 附录 A.2 与 stackexchange 公开核对的 PBKDF2-HMAC-SHA256
 *        测试向量校验 pbkdf2_hmac_sha256 内部实现的正确性
 *
 * 向量来源（RFC 7914 §11 PBKDF2-HMAC-SHA256 测试向量）：
 *   password="passwd", salt="salt", c=1, dkLen=64 →
 *     55ac046e56e3089fec1691c22544b605f94185216dde0465e68b9d57c20dacbc
 *     49ca9cccf179b645991664b39d77ef317c71b845b1e30bd509112041d3a19783
 *
 *   password="Password", salt="NaCl", c=80000, dkLen=64 →
 *     4ddcd8f60b98be21830cee5ef22701f9641a4418d04c0414aeff08876b34ab56
 *     a1d425a1225833549adb841b51c9b3176a272bdebba1d078478f62b397f33c8d
 *
 * 我们只取前 32 字节（PBKDF2-HMAC-SHA256 在 dkLen=32 时仍走同一分块路径）。
 */
static void test_pbkdf2_hmac_sha256_nist_vectors(void) {
    uint8_t derived[32];
    uint8_t expected[32];

    /* Vector 1: passwd / salt / c=1 */
    AuthService_pbkdf2_hmac_sha256_for_testing(
        (const uint8_t *)"passwd", 6,
        (const uint8_t *)"salt", 4,
        1,
        derived, sizeof(derived));
    TestAuthService_hex_to_bytes(
        "55ac046e56e3089fec1691c22544b605f94185216dde0465e68b9d57c20dacbc",
        expected, sizeof(expected));
    assert(memcmp(derived, expected, sizeof(derived)) == 0);

    /* Vector 2: Password / NaCl / c=80000（与参考值首 32 字节） */
    AuthService_pbkdf2_hmac_sha256_for_testing(
        (const uint8_t *)"Password", 8,
        (const uint8_t *)"NaCl", 4,
        80000,
        derived, sizeof(derived));
    TestAuthService_hex_to_bytes(
        "4ddcd8f60b98be21830cee5ef22701f9641a4418d04c0414aeff08876b34ab56",
        expected, sizeof(expected));
    assert(memcmp(derived, expected, sizeof(derived)) == 0);
}

/**
 * @brief 验证 PBKDF2 实现对 dkLen > HLEN 的分块逻辑正确（跨 T_1/T_2 边界）
 */
static void test_pbkdf2_hmac_sha256_multi_block(void) {
    uint8_t derived[64];
    uint8_t expected[64];

    AuthService_pbkdf2_hmac_sha256_for_testing(
        (const uint8_t *)"passwd", 6,
        (const uint8_t *)"salt", 4,
        1,
        derived, sizeof(derived));
    TestAuthService_hex_to_bytes(
        "55ac046e56e3089fec1691c22544b605"
        "f94185216dde0465e68b9d57c20dacbc"
        "49ca9cccf179b645991664b39d77ef31"
        "7c71b845b1e30bd509112041d3a19783",
        expected, sizeof(expected));
    assert(memcmp(derived, expected, sizeof(derived)) == 0);
}

/**
 * @brief 测试主函数，依次运行所有认证服务测试用例
 */
int main(void) {
    test_pbkdf2_hmac_sha256_nist_vectors();
    test_pbkdf2_hmac_sha256_multi_block();
    test_patient_binding_and_login_success();
    test_patient_registration_requires_existing_patient_record();
    test_authenticate_rejects_wrong_password_and_role_mismatch();
    test_register_rejects_blank_password_and_invalid_role();
    test_authenticate_accepts_seeded_demo_accounts();
    test_authenticate_lockout_after_failures();
    test_successful_login_resets_failure_counter();
    test_legacy_hash_migration_on_login();
    test_new_user_uses_pbkdf2_format();
    return 0;
}
