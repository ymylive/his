/**
 * @file AuthService.c
 * @brief 认证服务模块实现
 *
 * 实现用户注册和身份认证功能。包含密码加盐哈希（基于 FNV 变体算法）、
 * 旧版无盐哈希兼容、文本校验以及用户角色验证等内部逻辑。
 */

#include "service/AuthService.h"

#include <ctype.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#endif

#include "common/StringUtils.h"
#include "repository/RepositoryUtils.h"

/**
 * @brief 默认的时钟源，返回当前 Unix 时间戳（秒）
 */
static long AuthService_default_now(void) {
    return (long)time(NULL);
}

/**
 * @brief 可注入的时钟函数指针，测试时可替换为受控时钟
 *
 * 通过 AuthService_set_clock_for_testing（声明在 AuthServiceTestHooks.h）
 * 进行注入，生产代码始终走默认的 time() 实现。
 */
static long (*auth_now_fn)(void) = AuthService_default_now;

/**
 * @brief 测试专用：注入自定义时钟函数。传入 NULL 恢复默认。
 *
 * 声明在 include/service/AuthServiceTestHooks.h 中，仅供测试代码使用。
 */
void AuthService_set_clock_for_testing(long (*now_fn)(void)) {
    auth_now_fn = (now_fn != 0) ? now_fn : AuthService_default_now;
}

/**
 * @brief 统一获取当前时间的内部接口
 */
static long AuthService_now(void) {
    return auth_now_fn();
}

/**
 * @brief 审计日志记录（若 AuditService 未就绪则输出到 stderr）
 */
#ifndef HIS_HAS_AUDIT
#define HIS_AUDIT_LOG(fmt, ...) \
    fprintf(stderr, "AUDIT: " fmt "\n", __VA_ARGS__)
#endif

/**
 * @brief 密码哈希迭代次数
 *
 * 较高的值能增强抗暴力破解能力，但会增加哈希计算时间。
 * NIST SP 800-63B 建议至少 10000 次迭代；此处使用 100000 次
 * 以提供更强的安全边际。调整此值后需重新生成所有已存储的密码哈希。
 */
#define AUTH_HASH_ITERATIONS 100000

/**
 * @brief 触发账号锁定的最大连续失败次数
 */
#ifndef HIS_LOGIN_MAX_FAILURES
#define HIS_LOGIN_MAX_FAILURES 5
#endif

/**
 * @brief 账号锁定时长（秒）
 */
#ifndef HIS_LOGIN_LOCKOUT_SECONDS
#define HIS_LOGIN_LOCKOUT_SECONDS 900
#endif

/**
 * @brief 登录失败锁定计数的旁路（sidecar）文件后缀
 *
 * 将锁定状态独立保存到与 users.txt 同目录的 .lockout 文件中，
 * 避免修改 User 记录文件格式，保持兼容性。
 * 格式：user_id|failed_count|locked_until（Unix epoch 秒，0 = 未锁定）
 */
#define AUTH_LOCKOUT_SUFFIX ".lockout"
#define AUTH_LOCKOUT_HEADER "user_id|failed_count|locked_until"
#define AUTH_LOCKOUT_LINE_CAPACITY 256
#define AUTH_LOCKOUT_FILE_CAPACITY 32768

/**
 * @brief 登录失败锁定状态
 */
typedef struct AuthLockoutState {
    int failed_count;
    long locked_until;
} AuthLockoutState;

/**
 * @brief 由 user 文件路径推导出 lockout 旁路文件路径
 *
 * 规则：在原路径末尾追加 ".lockout" 后缀。
 *
 * @param service   认证服务（读取 user_repository.file_repository.path）
 * @param buffer    输出路径缓冲区
 * @param capacity  缓冲区容量
 * @return 成功返回 1，缓冲区不足返回 0
 */
static int AuthService_lockout_path(
    const AuthService *service,
    char *buffer,
    size_t capacity
) {
    int written = 0;
    if (service == 0 || buffer == 0 || capacity == 0) {
        return 0;
    }
    written = snprintf(buffer, capacity, "%s%s",
                       service->user_repository.file_repository.path,
                       AUTH_LOCKOUT_SUFFIX);
    if (written < 0 || (size_t)written >= capacity) {
        return 0;
    }
    return 1;
}

/**
 * @brief 读取指定用户的锁定状态
 *
 * 若文件不存在或用户条目缺失，返回零值（未锁定、无失败）。
 *
 * @param service  认证服务
 * @param user_id  用户ID
 * @param out      输出锁定状态
 */
static void AuthService_load_lockout(
    const AuthService *service,
    const char *user_id,
    AuthLockoutState *out
) {
    char path[TEXT_FILE_REPOSITORY_PATH_CAPACITY + 16];
    FILE *file = 0;
    char line[AUTH_LOCKOUT_LINE_CAPACITY];

    if (out == 0) {
        return;
    }
    out->failed_count = 0;
    out->locked_until = 0;

    if (service == 0 || user_id == 0 || user_id[0] == '\0') {
        return;
    }
    if (!AuthService_lockout_path(service, path, sizeof(path))) {
        return;
    }

    file = fopen(path, "r");
    if (file == 0) {
        return; /* 未创建过 sidecar，视为干净状态 */
    }

    while (fgets(line, sizeof(line), file) != 0) {
        size_t len = strlen(line);
        char *pipe1 = 0;
        char *pipe2 = 0;
        long failed_val = 0;
        long locked_val = 0;

        /* 去除结尾换行 */
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
            line[--len] = '\0';
        }
        if (len == 0 || strcmp(line, AUTH_LOCKOUT_HEADER) == 0) {
            continue;
        }

        pipe1 = strchr(line, '|');
        if (pipe1 == 0) continue;
        pipe2 = strchr(pipe1 + 1, '|');
        if (pipe2 == 0) continue;

        *pipe1 = '\0';
        *pipe2 = '\0';
        if (strcmp(line, user_id) != 0) {
            continue;
        }

        failed_val = strtol(pipe1 + 1, 0, 10);
        locked_val = strtol(pipe2 + 1, 0, 10);
        if (failed_val < 0) failed_val = 0;
        if (failed_val > INT_MAX) failed_val = INT_MAX;
        out->failed_count = (int)failed_val;
        out->locked_until = locked_val;
        break;
    }

    fclose(file);
}

/**
 * @brief 写回指定用户的锁定状态
 *
 * 读取 sidecar 文件全文，替换/追加目标用户条目，然后整文件覆盖写入。
 * 若 sidecar 不存在会新建（含表头）。失败静默忽略，不影响登录流程。
 */
static void AuthService_save_lockout(
    const AuthService *service,
    const char *user_id,
    const AuthLockoutState *state
) {
    char path[TEXT_FILE_REPOSITORY_PATH_CAPACITY + 16];
    char tmp_path[TEXT_FILE_REPOSITORY_PATH_CAPACITY + 32];
    char content[AUTH_LOCKOUT_FILE_CAPACITY];
    FILE *file = 0;
    size_t offset = 0;
    int written = 0;
    int found_user = 0;
    char line[AUTH_LOCKOUT_LINE_CAPACITY];

    if (service == 0 || user_id == 0 || user_id[0] == '\0' || state == 0) {
        return;
    }
    if (!AuthService_lockout_path(service, path, sizeof(path))) {
        return;
    }

    memset(content, 0, sizeof(content));
    written = snprintf(content, sizeof(content), "%s\n", AUTH_LOCKOUT_HEADER);
    if (written < 0 || (size_t)written >= sizeof(content)) return;
    offset = (size_t)written;

    file = fopen(path, "r");
    if (file != 0) {
        while (fgets(line, sizeof(line), file) != 0) {
            size_t len = strlen(line);
            char id_buf[HIS_DOMAIN_ID_CAPACITY];
            const char *pipe1 = 0;
            size_t id_len = 0;

            while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
                line[--len] = '\0';
            }
            if (len == 0 || strcmp(line, AUTH_LOCKOUT_HEADER) == 0) {
                continue;
            }

            pipe1 = strchr(line, '|');
            if (pipe1 == 0) continue;
            id_len = (size_t)(pipe1 - line);
            if (id_len >= sizeof(id_buf)) id_len = sizeof(id_buf) - 1;
            memcpy(id_buf, line, id_len);
            id_buf[id_len] = '\0';

            if (strcmp(id_buf, user_id) == 0) {
                /* 目标用户：写入新状态 */
                written = snprintf(content + offset, sizeof(content) - offset,
                                   "%s|%d|%ld\n",
                                   user_id, state->failed_count, state->locked_until);
                found_user = 1;
            } else {
                /* 其他用户：原样保留 */
                written = snprintf(content + offset, sizeof(content) - offset,
                                   "%s\n", line);
            }
            if (written < 0 || (size_t)written >= sizeof(content) - offset) {
                fclose(file);
                return;
            }
            offset += (size_t)written;
        }
        fclose(file);
    }

    if (found_user == 0) {
        written = snprintf(content + offset, sizeof(content) - offset,
                           "%s|%d|%ld\n",
                           user_id, state->failed_count, state->locked_until);
        if (written < 0 || (size_t)written >= sizeof(content) - offset) {
            return;
        }
        offset += (size_t)written;
    }

    /* 原子写：先写入临时文件再重命名 */
    written = snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path);
    if (written < 0 || (size_t)written >= sizeof(tmp_path)) return;
    file = fopen(tmp_path, "w");
    if (file == 0) return;
    if (fwrite(content, 1, offset, file) != offset) {
        fclose(file);
        remove(tmp_path);
        return;
    }
    if (fflush(file) != 0 || fclose(file) != 0) {
        remove(tmp_path);
        return;
    }
    remove(path);
    if (rename(tmp_path, path) != 0) {
        remove(tmp_path);
    }
}

/**
 * @brief 安全清零敏感缓冲区
 *
 * 使用 volatile 指针防止编译器优化掉清零操作。
 */
static void auth_secure_zero(void *ptr, size_t len) {
    volatile char *p = (volatile char *)ptr;
    while (len--) *p++ = 0;
}

/**
 * @brief 常量时间字符串比较，防止时序攻击
 *
 * 无论输入内容如何，始终比较完整的 len 字节，
 * 确保攻击者无法通过测量响应时间推断匹配程度。
 *
 * @param a    第一个字符串
 * @param b    第二个字符串
 * @param len  要比较的字节数
 * @return int 1=相等，0=不相等
 */
static int constant_time_compare(const char *a, const char *b, size_t len) {
    volatile unsigned char result = 0;
    size_t i;
    for (i = 0; i < len; i++) {
        result |= (unsigned char)a[i] ^ (unsigned char)b[i];
    }
    return result == 0 ? 1 : 0;
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

    if (StringUtils_is_blank(text)) {
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
 * 使用操作系统提供的密码学安全随机数生成器（CSPRNG）生成盐值。
 * 如果系统不支持安全随机源，则返回错误，不会回退到不安全的 srand/rand。
 *
 * @param salt_hex  输出缓冲区，至少33字节（32字符 + '\0'）
 * @param capacity  缓冲区容量（未使用，仅作安全标记）
 * @return Result   操作结果，CSPRNG 不可用时返回 failure
 */
static Result AuthService_generate_salt(char *salt_hex, size_t capacity) {
    unsigned char raw_bytes[16];
    int i = 0;
    int csprng_ok = 0;

    (void)capacity;  /* 显式忽略未使用的参数 */
    memset(raw_bytes, 0, sizeof(raw_bytes));

#ifdef _WIN32
    /* Windows: 使用 BCryptGenRandom 生成密码学安全随机数 */
    {
        typedef long NTSTATUS;
        typedef NTSTATUS (*BCryptGenRandomFunc)(void*, unsigned char*, unsigned long, unsigned long);
        HMODULE bcrypt_lib = LoadLibraryA("bcrypt.dll");
        if (bcrypt_lib) {
            BCryptGenRandomFunc gen_random = (BCryptGenRandomFunc)GetProcAddress(bcrypt_lib, "BCryptGenRandom");
            if (gen_random && gen_random(NULL, raw_bytes, sizeof(raw_bytes), 0x00000002) == 0) {
                csprng_ok = 1;
            }
            FreeLibrary(bcrypt_lib);
        }
    }
#else
    /* POSIX: 使用 /dev/urandom 生成密码学安全随机数 */
    {
        FILE *urand = fopen("/dev/urandom", "rb");
        if (urand) {
            size_t read_count = fread(raw_bytes, 1, sizeof(raw_bytes), urand);
            fclose(urand);
            if (read_count == sizeof(raw_bytes)) {
                csprng_ok = 1;
            }
        }
    }
#endif

    if (!csprng_ok) {
        return Result_make_failure("no secure random source available");
    }

    /* 将原始字节转换为十六进制字符串 */
    for (i = 0; i < 16; i++) {
        snprintf(salt_hex + i * 2, 3, "%02x", raw_bytes[i]);
    }

    return Result_make_success("salt generated");
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

    /* 对密码执行多次迭代混合，增强抗暴力破解能力 */
    for (iteration = 0; iteration < AUTH_HASH_ITERATIONS; iteration++) {
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
    UserRole role,
    const char *patient_id
) {
    User user;
    Patient patient;
    char salt_hex[33];  /* 32个十六进制字符 + '\0' */
    Result result;

    if (service == 0) {
        return Result_make_failure("auth service missing");
    }

    /* 校验用户名合法性 */
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

    /* 患者角色用户必须提供有效的患者编号 */
    if (role == USER_ROLE_PATIENT) {
        if (service->patient_repository_enabled == 0) {
            return Result_make_failure("patient repository not configured");
        }

        if (patient_id == 0 || patient_id[0] == '\0') {
            return Result_make_failure("patient id required for patient role");
        }

        /* 验证患者记录存在 */
        result = PatientRepository_find_by_id(&service->patient_repository, patient_id, &patient);
        if (result.success == 0) {
            return Result_make_failure("patient account requires existing patient id");
        }
    }

    /* 构造用户结构体 */
    memset(&user, 0, sizeof(user));
    strncpy(user.user_id, user_id, sizeof(user.user_id) - 1);
    user.user_id[sizeof(user.user_id) - 1] = '\0';
    user.role = role;

    /* 存储关联的患者编号 */
    if (patient_id != 0 && patient_id[0] != '\0') {
        strncpy(user.patient_id, patient_id, sizeof(user.patient_id) - 1);
        user.patient_id[sizeof(user.patient_id) - 1] = '\0';
    }

    /* 生成随机盐值并计算加盐密码哈希 */
    memset(salt_hex, 0, sizeof(salt_hex));
    result = AuthService_generate_salt(salt_hex, sizeof(salt_hex));
    if (result.success == 0) {
        return result;
    }

    result = AuthService_hash_password(password, salt_hex, user.password_hash, sizeof(user.password_hash));
    if (result.success == 0) {
        return result;
    }

    /* 将用户记录追加到存储文件 */
    return UserRepository_append(&service->user_repository, &user);
}

/**
 * @brief 用于更新用户密码哈希时重建文件内容的上下文
 */
typedef struct AuthUpdateContext {
    const User *updated_user;  /**< 要更新的用户（已含新哈希） */
    char *buffer;              /**< 输出缓冲区 */
    size_t capacity;           /**< 缓冲区容量 */
    size_t offset;             /**< 当前写入偏移 */
    int updated;               /**< 是否已成功替换 */
} AuthUpdateContext;

/**
 * @brief 重建文件行回调：替换匹配用户的记录
 */
static Result AuthService_update_line_handler(const char *line, void *context) {
    AuthUpdateContext *ctx = (AuthUpdateContext *)context;
    char user_id_buf[64];
    const char *sep = 0;
    size_t id_len = 0;
    int written = 0;

    /* 防御性检查：offset 不得超过 capacity，否则 capacity-offset 会下溢 */
    if (ctx->offset >= ctx->capacity) {
        return Result_make_failure("user file content overflow");
    }

    /* 提取行中的 user_id（第一个 '|' 之前的部分） */
    sep = strchr(line, '|');
    if (sep == 0) {
        /* 无法解析，原样保留 */
        written = snprintf(ctx->buffer + ctx->offset,
                           ctx->capacity - ctx->offset, "%s\n", line);
        if (written < 0 || (size_t)written >= ctx->capacity - ctx->offset) {
            return Result_make_failure("user file content overflow");
        }
        ctx->offset += (size_t)written;
        return Result_make_success("line kept");
    }

    id_len = (size_t)(sep - line);
    if (id_len >= sizeof(user_id_buf)) {
        id_len = sizeof(user_id_buf) - 1;
    }
    memcpy(user_id_buf, line, id_len);
    user_id_buf[id_len] = '\0';

    if (strcmp(user_id_buf, ctx->updated_user->user_id) == 0) {
        /* 写入更新后的记录 */
        written = snprintf(ctx->buffer + ctx->offset,
                           ctx->capacity - ctx->offset,
                           "%s|%s|%d|%s|%d\n",
                           ctx->updated_user->user_id,
                           ctx->updated_user->password_hash,
                           (int)ctx->updated_user->role,
                           ctx->updated_user->patient_id,
                           ctx->updated_user->force_password_change);
        if (written < 0 || (size_t)written >= ctx->capacity - ctx->offset) {
            return Result_make_failure("user file content overflow");
        }
        ctx->offset += (size_t)written;
        ctx->updated = 1;
    } else {
        /* 原样保留 */
        written = snprintf(ctx->buffer + ctx->offset,
                           ctx->capacity - ctx->offset, "%s\n", line);
        if (written < 0 || (size_t)written >= ctx->capacity - ctx->offset) {
            return Result_make_failure("user file content overflow");
        }
        ctx->offset += (size_t)written;
    }

    return Result_make_success("line processed");
}

/**
 * @brief 更新用户的密码哈希（用于旧版哈希自动升级）
 *
 * 读取全部用户记录，替换匹配用户的行，然后覆盖写入文件。
 * 失败时不影响调用方（静默忽略错误）。
 *
 * @param service       认证服务
 * @param updated_user  已更新哈希的用户记录
 */
static void AuthService_update_user_hash(AuthService *service, const User *updated_user) {
    /* 使用固定大小的缓冲区重建文件内容 */
    char file_content[32768];
    AuthUpdateContext ctx;
    int written = 0;
    Result result;

    memset(file_content, 0, sizeof(file_content));

    /* 先写入表头 */
    written = snprintf(file_content, sizeof(file_content), "%s\n", USER_REPOSITORY_HEADER);
    if (written <= 0 || (size_t)written >= sizeof(file_content)) return;

    ctx.updated_user = updated_user;
    ctx.buffer = file_content;
    ctx.capacity = sizeof(file_content);
    ctx.offset = (size_t)written;
    ctx.updated = 0;

    result = TextFileRepository_for_each_data_line(
        &service->user_repository.file_repository,
        AuthService_update_line_handler,
        &ctx
    );

    if (result.success == 0 || ctx.updated == 0) {
        return; /* 遍历失败或未找到用户，放弃更新 */
    }

    TextFileRepository_save_file(&service->user_repository.file_repository, file_content);
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
    AuthLockoutState lockout;
    long now_ts = 0;
    int account_locked = 0;
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
    auth_secure_zero(password_hash, sizeof(password_hash));

    /* 加载锁定状态（找不到用户时返回零值） */
    lockout.failed_count = 0;
    lockout.locked_until = 0;
    if (find_result.success != 0) {
        AuthService_load_lockout(service, user_id, &lockout);
    }
    now_ts = AuthService_now();
    if (lockout.locked_until > now_ts) {
        account_locked = 1;
    } else if (lockout.locked_until != 0 && lockout.locked_until <= now_ts) {
        /* 锁定已过期，清零以便下一轮重新计数 */
        lockout.failed_count = 0;
        lockout.locked_until = 0;
    }

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
        auth_secure_zero(password_hash, sizeof(password_hash));
        auth_secure_zero(extracted_salt, sizeof(extracted_salt));
        auth_secure_zero(&loaded_user, sizeof(loaded_user));
        return result;
    }

    /* 用户不存在或哈希格式无法识别，统一返回凭证无效 */
    if (find_result.success == 0 || user_hash_format_known == 0) {
        auth_secure_zero(password_hash, sizeof(password_hash));
        auth_secure_zero(extracted_salt, sizeof(extracted_salt));
        auth_secure_zero(&loaded_user, sizeof(loaded_user));
        HIS_AUDIT_LOG("LOGIN_FAILURE user=%s reason=unknown_user",
                      user_id != 0 ? user_id : "<null>");
        return Result_make_failure("invalid credentials");
    }

    /* 比对密码哈希（使用常量时间比较，防止时序攻击） */
    {
        size_t stored_len = strlen(loaded_user.password_hash);
        size_t computed_len = strlen(password_hash);
        int password_match = (stored_len == computed_len) &&
            constant_time_compare(loaded_user.password_hash, password_hash, stored_len);

        if (account_locked) {
            /* 即便密码正确，在锁定期内也拒绝登录 */
            auth_secure_zero(password_hash, sizeof(password_hash));
            auth_secure_zero(extracted_salt, sizeof(extracted_salt));
            auth_secure_zero(&loaded_user, sizeof(loaded_user));
            HIS_AUDIT_LOG("LOGIN_BLOCKED user=%s reason=account_locked locked_until=%ld",
                          user_id, lockout.locked_until);
            return Result_make_failure("account temporarily locked");
        }

        if (password_match == 0) {
            /* 密码错误：递增失败计数并持久化，必要时锁定 */
            lockout.failed_count += 1;
            if (lockout.failed_count >= HIS_LOGIN_MAX_FAILURES) {
                lockout.locked_until = now_ts + HIS_LOGIN_LOCKOUT_SECONDS;
                HIS_AUDIT_LOG("ACCOUNT_LOCKED user=%s failed_count=%d locked_until=%ld",
                              user_id, lockout.failed_count, lockout.locked_until);
            } else {
                HIS_AUDIT_LOG("LOGIN_FAILURE user=%s reason=wrong_password failed_count=%d",
                              user_id, lockout.failed_count);
            }
            AuthService_save_lockout(service, user_id, &lockout);

            auth_secure_zero(password_hash, sizeof(password_hash));
            auth_secure_zero(extracted_salt, sizeof(extracted_salt));
            auth_secure_zero(&loaded_user, sizeof(loaded_user));
            return Result_make_failure("invalid credentials");
        }
    }

    /* 盐值不再需要，立即清零 */
    auth_secure_zero(extracted_salt, sizeof(extracted_salt));

    /* 可选的角色匹配验证 */
    if (required_role != USER_ROLE_UNKNOWN && loaded_user.role != required_role) {
        auth_secure_zero(password_hash, sizeof(password_hash));
        auth_secure_zero(&loaded_user, sizeof(loaded_user));
        HIS_AUDIT_LOG("LOGIN_FAILURE user=%s reason=role_mismatch", user_id);
        return Result_make_failure("user role mismatch");
    }

    /* 登录成功：清零失败计数 */
    if (lockout.failed_count != 0 || lockout.locked_until != 0) {
        lockout.failed_count = 0;
        lockout.locked_until = 0;
        AuthService_save_lockout(service, user_id, &lockout);
    }

    /* 旧版哈希自动升级：使用新的加盐格式重新哈希并更新存储 */
    if (AuthService_is_legacy_password_hash(loaded_user.password_hash)) {
        char new_salt[33];
        char new_hash[HIS_USER_PASSWORD_HASH_CAPACITY];
        Result salt_result = AuthService_generate_salt(new_salt, sizeof(new_salt));
        if (salt_result.success != 0) {
            Result hash_result = AuthService_hash_password(
                password, new_salt, new_hash, sizeof(new_hash));
            if (hash_result.success != 0) {
                /* 更新用户记录中的密码哈希 */
                strncpy(loaded_user.password_hash, new_hash,
                        sizeof(loaded_user.password_hash) - 1);
                loaded_user.password_hash[sizeof(loaded_user.password_hash) - 1] = '\0';
                /* 尝试保存更新后的用户（失败不影响本次登录） */
                AuthService_update_user_hash(service, &loaded_user);
            }
            auth_secure_zero(new_hash, sizeof(new_hash));
        }
        auth_secure_zero(new_salt, sizeof(new_salt));
    }

    /* 清零密码哈希缓冲区 */
    auth_secure_zero(password_hash, sizeof(password_hash));

    /* 认证成功，输出用户信息 */
    *out_user = loaded_user;
    auth_secure_zero(&loaded_user, sizeof(loaded_user));
    return Result_make_success("user authenticated");
}

/**
 * @brief 修改用户密码
 *
 * 验证旧密码后，使用新密码重新生成加盐哈希并更新存储。
 * 同时将 force_password_change 标志清零。
 */
Result AuthService_change_password(
    AuthService *service,
    const char *user_id,
    const char *old_password,
    const char *new_password
) {
    User user;
    char salt_hex[33];
    char new_hash[HIS_USER_PASSWORD_HASH_CAPACITY];
    Result result;

    if (service == 0) {
        return Result_make_failure("auth service missing");
    }

    /* 用旧密码验证身份 */
    result = AuthService_authenticate(service, user_id, old_password, USER_ROLE_UNKNOWN, &user);
    if (result.success == 0) {
        return Result_make_failure("old password incorrect");
    }

    /* 校验新密码合法性 */
    result = AuthService_validate_text(new_password, "new password");
    if (result.success == 0) {
        return result;
    }

    /* 生成新盐值并计算新哈希 */
    memset(salt_hex, 0, sizeof(salt_hex));
    result = AuthService_generate_salt(salt_hex, sizeof(salt_hex));
    if (result.success == 0) {
        return result;
    }

    memset(new_hash, 0, sizeof(new_hash));
    result = AuthService_hash_password(new_password, salt_hex, new_hash, sizeof(new_hash));
    auth_secure_zero(salt_hex, sizeof(salt_hex));
    if (result.success == 0) {
        auth_secure_zero(new_hash, sizeof(new_hash));
        return result;
    }

    /* 更新用户记录 */
    strncpy(user.password_hash, new_hash, sizeof(user.password_hash) - 1);
    user.password_hash[sizeof(user.password_hash) - 1] = '\0';
    user.force_password_change = 0;
    auth_secure_zero(new_hash, sizeof(new_hash));

    /* 写回存储 */
    AuthService_update_user_hash(service, &user);
    auth_secure_zero(&user, sizeof(user));

    return Result_make_success("password changed");
}
