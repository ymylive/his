/**
 * @file AuthService.c
 * @brief 认证服务模块实现
 *
 * 实现用户注册和身份认证功能。包含密码加盐哈希（基于 FNV 变体算法）、
 * 旧版无盐哈希兼容、文本校验以及用户角色验证等内部逻辑。
 */

#include "service/AuthService.h"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "repository/RepositoryUtils.h"

/**
 * @brief 判断文本是否为空白（NULL、空串或全为空白字符）
 *
 * @param text  待检查的字符串
 * @return int  1=空白，0=非空白
 */
static int AuthService_is_blank_text(const char *text) {
    if (text == 0) {
        return 1;
    }

    /* 逐字符检查是否全为空白 */
    while (*text != '\0') {
        if (!isspace((unsigned char)*text)) {
            return 0;
        }

        text++;
    }

    return 1;
}

/**
 * @brief 判断用户角色是否合法
 *
 * @param role  用户角色枚举值
 * @return int  1=合法，0=非法
 */
static int AuthService_is_valid_role(UserRole role) {
    return role == USER_ROLE_PATIENT ||
           role == USER_ROLE_DOCTOR ||
           role == USER_ROLE_ADMIN ||
           role == USER_ROLE_INPATIENT_MANAGER ||
           role == USER_ROLE_PHARMACY;
}

/**
 * @brief 校验文本字段的合法性
 *
 * 检查文本是否为空白以及是否包含管道符等保留字符。
 *
 * @param text        待校验的文本
 * @param field_name  字段名称（用于错误消息）
 * @return Result     校验结果
 */
static Result AuthService_validate_text(const char *text, const char *field_name) {
    char message[RESULT_MESSAGE_CAPACITY];

    if (AuthService_is_blank_text(text)) {
        snprintf(message, sizeof(message), "%s missing", field_name);
        return Result_make_failure(message);
    }

    /* 检查是否包含数据存储中的保留字符（如管道符） */
    if (!RepositoryUtils_is_safe_field_text(text)) {
        snprintf(message, sizeof(message), "%s contains reserved characters", field_name);
        return Result_make_failure(message);
    }

    return Result_make_success("text valid");
}

/**
 * @brief 生成16字节（32个十六进制字符）的随机盐值
 *
 * 使用当前时间和CPU时钟作为随机种子，生成伪随机盐值字符串。
 *
 * @param salt_hex  输出缓冲区，至少33字节（32字符 + '\0'）
 * @param capacity  缓冲区容量（未使用，仅作安全标记）
 */
static void AuthService_generate_salt(char *salt_hex, size_t capacity) {
    unsigned int seed = (unsigned int)(time(NULL) ^ clock());
    int i = 0;

    (void)capacity;  /* 显式忽略未使用的参数 */
    srand(seed);
    /* 逐字节生成随机值并转为两位十六进制字符 */
    for (i = 0; i < 16; i++) {
        snprintf(salt_hex + i * 2, 3, "%02x", rand() % 256);
    }
}

/**
 * @brief 对密码进行加盐哈希运算
 *
 * 使用 FNV 变体算法，先将盐值混入初始状态，再对密码进行 1000 次
 * 迭代混合运算，最终输出格式为 "盐值$哈希值" 的字符串。
 *
 * @param password          明文密码
 * @param salt_hex          32字符的十六进制盐值
 * @param out_hash          输出哈希字符串的缓冲区
 * @param out_hash_capacity 输出缓冲区容量
 * @return Result           操作结果
 */
static Result AuthService_hash_password(
    const char *password,
    const char *salt_hex,
    char *out_hash,
    size_t out_hash_capacity
) {
    /* FNV 变体算法的四个初始状态值 */
    uint64_t h1 = UINT64_C(1469598103934665603);
    uint64_t h2 = UINT64_C(1099511628211);
    uint64_t h3 = UINT64_C(7809847782465536322);
    uint64_t h4 = UINT64_C(9650029242287828579);
    size_t index = 0;
    size_t salt_len = 0;
    int iteration = 0;
    int written = 0;
    /* 先校验密码文本合法性 */
    Result result = AuthService_validate_text(password, "password");

    if (result.success == 0) {
        return result;
    }

    /* 校验输出缓冲区是否足够 */
    if (out_hash == 0 || out_hash_capacity < HIS_USER_PASSWORD_HASH_CAPACITY) {
        return Result_make_failure("password hash buffer too small");
    }

    /* 盐值必须恰好为32个十六进制字符 */
    if (salt_hex == 0 || strlen(salt_hex) != 32) {
        return Result_make_failure("salt must be 32 hex characters");
    }

    /* 将盐值逐字节混入 h1 和 h2 的初始状态 */
    salt_len = strlen(salt_hex);
    for (index = 0; index < salt_len; index++) {
        unsigned char value = (unsigned char)salt_hex[index];
        h1 ^= (uint64_t)(value + 7U);
        h1 *= UINT64_C(1099511628211);
        h2 ^= (uint64_t)(value + 13U);
        h2 *= UINT64_C(1469598103934665603);
    }

    /* 对密码执行 1000 次迭代混合，增强抗暴力破解能力 */
    for (iteration = 0; iteration < 1000; iteration++) {
        index = 0;
        while (password[index] != '\0') {
            unsigned char value = (unsigned char)password[index];

            /* 四路并行哈希混合，各使用不同的偏移量和乘数 */
            h1 ^= (uint64_t)(value + 17U);
            h1 *= UINT64_C(1099511628211);

            h2 ^= (uint64_t)(value + 37U);
            h2 *= UINT64_C(1469598103934665603);

            h3 ^= (uint64_t)(value + 73U);
            h3 *= UINT64_C(1099511628211);

            h4 ^= (uint64_t)(value + 131U);
            h4 *= UINT64_C(1469598103934665603);

            index++;
        }

        /* 每轮迭代后进行交叉反馈，增加雪崩效应 */
        h1 ^= h4;
        h2 ^= h3;
        h3 ^= h1;
        h4 ^= h2;
    }

    /* 输出格式："32位盐值$64位哈希值"，共97字符 */
    written = snprintf(
        out_hash,
        out_hash_capacity,
        "%s$%016llx%016llx%016llx%016llx",
        salt_hex,
        (unsigned long long)h1,
        (unsigned long long)h2,
        (unsigned long long)h3,
        (unsigned long long)h4
    );
    if (written != HIS_USER_PASSWORD_HASH_CAPACITY - 1) {
        return Result_make_failure("password hash formatting failed");
    }

    return Result_make_success("password hashed");
}

/**
 * @brief 旧版无盐密码哈希（用于向后兼容）
 *
 * 与加盐版本类似的 FNV 变体算法，但不使用盐值和迭代，
 * 仅对密码单次遍历，输出64字符的十六进制哈希。
 *
 * @param password          明文密码
 * @param out_hash          输出哈希字符串的缓冲区
 * @param out_hash_capacity 输出缓冲区容量
 * @return Result           操作结果
 */
static Result AuthService_hash_password_legacy(
    const char *password,
    char *out_hash,
    size_t out_hash_capacity
) {
    uint64_t h1 = UINT64_C(1469598103934665603);
    uint64_t h2 = UINT64_C(1099511628211);
    uint64_t h3 = UINT64_C(7809847782465536322);
    uint64_t h4 = UINT64_C(9650029242287828579);
    size_t index = 0;
    int written = 0;
    Result result = AuthService_validate_text(password, "password");

    if (result.success == 0) {
        return result;
    }

    if (out_hash == 0 || out_hash_capacity < HIS_USER_PASSWORD_HASH_CAPACITY) {
        return Result_make_failure("password hash buffer too small");
    }

    /* 单次遍历密码字符，四路并行哈希混合 */
    while (password[index] != '\0') {
        unsigned char value = (unsigned char)password[index];

        h1 ^= (uint64_t)(value + 17U);
        h1 *= UINT64_C(1099511628211);

        h2 ^= (uint64_t)(value + 37U);
        h2 *= UINT64_C(1469598103934665603);

        h3 ^= (uint64_t)(value + 73U);
        h3 *= UINT64_C(1099511628211);

        h4 ^= (uint64_t)(value + 131U);
        h4 *= UINT64_C(1469598103934665603);

        index++;
    }

    /* 旧版格式：仅64字符哈希，无盐值前缀 */
    written = snprintf(
        out_hash,
        out_hash_capacity,
        "%016llx%016llx%016llx%016llx",
        (unsigned long long)h1,
        (unsigned long long)h2,
        (unsigned long long)h3,
        (unsigned long long)h4
    );
    if (written != 64) {
        return Result_make_failure("password hash formatting failed");
    }

    return Result_make_success("password hashed");
}

/**
 * @brief 判断存储的密码哈希是否为加盐格式
 *
 * 加盐格式为 "32位盐值$64位哈希值"，通过检测 '$' 分隔符的位置来判断。
 *
 * @param stored_hash  存储的密码哈希字符串
 * @return int         1=加盐格式，0=非加盐格式
 */
static int AuthService_is_salted_password_hash(const char *stored_hash) {
    const char *dollar_pos = 0;

    if (stored_hash == 0) {
        return 0;
    }

    /* '$' 应出现在第32个字符位置（盐值长度） */
    dollar_pos = strchr(stored_hash, '$');
    return dollar_pos != 0 && (dollar_pos - stored_hash) == 32;
}

/**
 * @brief 判断存储的密码哈希是否为旧版无盐格式
 *
 * 旧版格式为64个十六进制字符，不包含 '$' 分隔符。
 *
 * @param stored_hash  存储的密码哈希字符串
 * @return int         1=旧版格式，0=非旧版格式
 */
static int AuthService_is_legacy_password_hash(const char *stored_hash) {
    return stored_hash != 0 && strchr(stored_hash, '$') == 0 && strlen(stored_hash) == 64;
}

/**
 * @brief 判断文件路径是否已提供（非NULL且非空串）
 *
 * @param path  文件路径
 * @return int  1=已提供，0=未提供
 */
static int AuthService_is_path_provided(const char *path) {
    return path != 0 && path[0] != '\0';
}

/**
 * @brief 初始化认证服务
 *
 * 初始化用户仓库，并根据患者数据路径是否提供来决定是否启用患者仓库。
 *
 * @param service       指向待初始化的认证服务结构体
 * @param user_path     用户数据文件路径
 * @param patient_path  患者数据文件路径（可为 NULL 或空字符串）
 * @return Result       操作结果
 */
Result AuthService_init(
    AuthService *service,
    const char *user_path,
    const char *patient_path
) {
    Result result;

    if (service == 0) {
        return Result_make_failure("auth service missing");
    }

    /* 清零服务结构体，确保初始状态干净 */
    memset(service, 0, sizeof(*service));

    /* 初始化用户仓库 */
    result = UserRepository_init(&service->user_repository, user_path);
    if (result.success == 0) {
        return result;
    }

    /* 如果未提供患者数据路径，则不启用患者仓库 */
    if (!AuthService_is_path_provided(patient_path)) {
        service->patient_repository_enabled = 0;
        return Result_make_success("auth service initialized");
    }

    /* 初始化患者仓库 */
    result = PatientRepository_init(&service->patient_repository, patient_path);
    if (result.success == 0) {
        return result;
    }

    service->patient_repository_enabled = 1;
    return Result_make_success("auth service initialized");
}

/**
 * @brief 注册新用户
 *
 * 流程：校验用户ID和密码 -> 校验角色 -> 若为患者角色则验证患者记录存在 ->
 * 生成盐值 -> 计算加盐哈希 -> 追加用户记录到存储。
 *
 * @param service   指向认证服务结构体
 * @param user_id   用户ID
 * @param password  用户密码
 * @param role      用户角色
 * @return Result   操作结果
 */
Result AuthService_register_user(
    AuthService *service,
    const char *user_id,
    const char *password,
    UserRole role
) {
    User user;
    Patient patient;
    char salt_hex[33];  /* 32个十六进制字符 + '\0' */
    Result result;

    if (service == 0) {
        return Result_make_failure("auth service missing");
    }

    /* 校验用户ID合法性 */
    result = AuthService_validate_text(user_id, "user id");
    if (result.success == 0) {
        return result;
    }

    /* 校验密码合法性 */
    result = AuthService_validate_text(password, "password");
    if (result.success == 0) {
        return result;
    }

    /* 校验角色枚举值是否有效 */
    if (!AuthService_is_valid_role(role)) {
        return Result_make_failure("user role invalid");
    }

    /* 患者角色用户必须有对应的患者记录存在 */
    if (role == USER_ROLE_PATIENT) {
        if (service->patient_repository_enabled == 0) {
            return Result_make_failure("patient repository not configured");
        }

        /* 用户ID需与患者ID一致 */
        result = PatientRepository_find_by_id(&service->patient_repository, user_id, &patient);
        if (result.success == 0) {
            return Result_make_failure("patient account requires existing patient id");
        }
    }

    /* 构造用户结构体 */
    memset(&user, 0, sizeof(user));
    strncpy(user.user_id, user_id, sizeof(user.user_id) - 1);
    user.user_id[sizeof(user.user_id) - 1] = '\0';
    user.role = role;

    /* 生成随机盐值并计算加盐密码哈希 */
    memset(salt_hex, 0, sizeof(salt_hex));
    AuthService_generate_salt(salt_hex, sizeof(salt_hex));

    result = AuthService_hash_password(password, salt_hex, user.password_hash, sizeof(user.password_hash));
    if (result.success == 0) {
        return result;
    }

    /* 将用户记录追加到存储文件 */
    return UserRepository_append(&service->user_repository, &user);
}

/**
 * @brief 用户身份认证（登录验证）
 *
 * 流程：校验输入 -> 查找用户记录 -> 根据存储的哈希格式选择对应的哈希算法 ->
 * 比对密码哈希 -> 可选地验证角色匹配。
 * 采用常量时间风格处理（无论用户是否存在都执行哈希运算），减少时序攻击风险。
 *
 * @param service        指向认证服务结构体
 * @param user_id        用户ID
 * @param password       用户密码
 * @param required_role  要求的角色（USER_ROLE_UNKNOWN 表示不限制）
 * @param out_user       输出参数，认证成功时存放用户信息
 * @return Result        操作结果
 */
Result AuthService_authenticate(
    AuthService *service,
    const char *user_id,
    const char *password,
    UserRole required_role,
    User *out_user
) {
    User loaded_user;
    char password_hash[HIS_USER_PASSWORD_HASH_CAPACITY];
    /* 默认盐值，当用户不存在时使用，避免时序差异 */
    char extracted_salt[33] = "00000000000000000000000000000000";
    int user_hash_format_known = 0;  /* 是否成功识别了存储哈希的格式 */
    Result result;
    Result find_result;

    if (service == 0 || out_user == 0) {
        return Result_make_failure("auth arguments missing");
    }

    /* 校验用户ID */
    result = AuthService_validate_text(user_id, "user id");
    if (result.success == 0) {
        return result;
    }

    /* 校验密码 */
    result = AuthService_validate_text(password, "password");
    if (result.success == 0) {
        return result;
    }

    /* 校验所需角色的合法性 */
    if (required_role != USER_ROLE_UNKNOWN && !AuthService_is_valid_role(required_role)) {
        return Result_make_failure("required role invalid");
    }

    /* 从存储中查找用户 */
    memset(&loaded_user, 0, sizeof(loaded_user));
    find_result = UserRepository_find_by_user_id(&service->user_repository, user_id, &loaded_user);
    memset(password_hash, 0, sizeof(password_hash));

    if (find_result.success != 0) {
        /* 用户存在，根据存储的哈希格式选择对应的哈希算法 */
        if (AuthService_is_salted_password_hash(loaded_user.password_hash)) {
            /* 加盐格式：提取前32字符作为盐值 */
            memcpy(extracted_salt, loaded_user.password_hash, 32);
            extracted_salt[32] = '\0';
            result = AuthService_hash_password(password, extracted_salt, password_hash, sizeof(password_hash));
            user_hash_format_known = 1;
        } else if (AuthService_is_legacy_password_hash(loaded_user.password_hash)) {
            /* 旧版无盐格式 */
            result = AuthService_hash_password_legacy(password, password_hash, sizeof(password_hash));
            user_hash_format_known = 1;
        } else {
            /* 无法识别的哈希格式，仍执行哈希运算避免时序差异 */
            result = AuthService_hash_password(password, extracted_salt, password_hash, sizeof(password_hash));
        }
    } else {
        /* 用户不存在，仍执行哈希运算以防止通过时序判断用户是否存在 */
        result = AuthService_hash_password(password, extracted_salt, password_hash, sizeof(password_hash));
    }
    if (result.success == 0) {
        return result;
    }

    /* 用户不存在或哈希格式无法识别，统一返回凭证无效 */
    if (find_result.success == 0 || user_hash_format_known == 0) {
        return Result_make_failure("invalid credentials");
    }

    /* 比对密码哈希 */
    if (strcmp(loaded_user.password_hash, password_hash) != 0) {
        return Result_make_failure("invalid credentials");
    }

    /* 可选的角色匹配验证 */
    if (required_role != USER_ROLE_UNKNOWN && loaded_user.role != required_role) {
        return Result_make_failure("user role mismatch");
    }

    /* 认证成功，输出用户信息 */
    *out_user = loaded_user;
    return Result_make_success("user authenticated");
}
