/**
 * @file test_audit_service.c
 * @brief 审计日志服务（AuditService）的单元测试
 *
 * 覆盖场景：
 * - 初始化后日志文件可创建且可写
 * - record 写入一行，格式为 8 个管道分隔字段
 * - 字段中的 '|' 被转义为 "\\|"，不破坏行格式
 * - actor_user_id 为 NULL 时（如登录前失败）字段为空但分隔符保留
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "service/AuditService.h"

/** 全局计数器，保证每次 make_test_data_dir 产生唯一目录（即使同一秒内多次调用） */
static int g_test_sequence = 0;

/**
 * @brief 构造测试用 audit.log 所在的目录路径
 *
 * 使用时间戳避免并发运行冲突。调用方需确保目录已存在；
 * 这里简单复用 build/ 下的临时子目录（不同测试互不干扰）。
 */
static void make_test_data_dir(char *buffer, size_t buffer_size, const char *suffix) {
    char log_path[384];
    assert(buffer != 0);
    assert(suffix != 0);
    g_test_sequence++;
    snprintf(
        buffer,
        buffer_size,
        "build/test_audit_service_data_%ld_%d_%s/",
        (long)time(0),
        g_test_sequence,
        suffix
    );
    /* 删除潜在残留文件，保证每次测试从空白状态开始 */
    snprintf(log_path, sizeof(log_path), "%saudit.log", buffer);
    remove(log_path);
}

/**
 * @brief 读取文件全部内容到缓冲区（以 '\0' 结尾）
 */
static size_t read_file(const char *path, char *buffer, size_t buffer_size) {
    FILE *f = fopen(path, "r");
    size_t n = 0;
    assert(f != 0);
    n = fread(buffer, 1, buffer_size - 1, f);
    buffer[n] = '\0';
    fclose(f);
    return n;
}

/**
 * @brief 统计字符串中字符 ch 的出现次数
 */
static int count_char(const char *s, char ch) {
    int n = 0;
    while (*s != '\0') {
        if (*s == ch) n++;
        s++;
    }
    return n;
}

/**
 * @brief 测试初始化 + 写入一行登录成功事件
 */
static void test_init_and_record_login_success(void) {
    char dir[256];
    char log_path[384];
    char content[1024];
    AuditService service;
    Result r;

    make_test_data_dir(dir, sizeof(dir), "login");
    r = AuditService_init(&service, dir);
    assert(r.success == 1);

    /* 验证 log_path 拼接正确 */
    snprintf(log_path, sizeof(log_path), "%saudit.log", dir);
    assert(strcmp(service.log_path, log_path) == 0);

    r = AuditService_record(
        &service,
        AUDIT_EVENT_LOGIN_SUCCESS,
        "ADM0001", "ADMIN",
        0, 0,
        "OK", 0);
    assert(r.success == 1);

    read_file(log_path, content, sizeof(content));

    /* 行中应包含事件类型 LOGIN_SUCCESS、用户 ADM0001 及角色 ADMIN */
    assert(strstr(content, "LOGIN_SUCCESS") != 0);
    assert(strstr(content, "ADM0001") != 0);
    assert(strstr(content, "ADMIN") != 0);
    assert(strstr(content, "OK") != 0);

    /* 格式：8 字段 = 7 个管道符，再加结尾换行 */
    assert(count_char(content, '|') == 7);
    assert(content[strlen(content) - 1] == '\n');
}

/**
 * @brief 测试 actor_user_id 为 NULL（登录前失败尝试）时行仍然合法
 */
static void test_record_with_null_actor(void) {
    char dir[256];
    char log_path[384];
    char content[1024];
    AuditService service;
    Result r;

    make_test_data_dir(dir, sizeof(dir), "null_actor");
    r = AuditService_init(&service, dir);
    assert(r.success == 1);

    r = AuditService_record(
        &service,
        AUDIT_EVENT_LOGIN_FAILURE,
        0, 0, 0, 0, "FAIL", "invalid credentials");
    assert(r.success == 1);

    snprintf(log_path, sizeof(log_path), "%saudit.log", dir);
    read_file(log_path, content, sizeof(content));

    /* 仍然恰好 7 个管道符 */
    assert(count_char(content, '|') == 7);
    assert(strstr(content, "LOGIN_FAILURE") != 0);
    assert(strstr(content, "FAIL") != 0);
    assert(strstr(content, "invalid credentials") != 0);

    /* 连续两个管道符确认空 actor 字段存在（形如 "LOGIN_FAILURE|||") */
    assert(strstr(content, "LOGIN_FAILURE|||") != 0);
}

/**
 * @brief 测试字段中的 '|' 被转义为 "\\|"，不会破坏行格式
 */
static void test_record_escapes_pipe_in_detail(void) {
    char dir[256];
    char log_path[384];
    char content[1024];
    AuditService service;
    Result r;

    make_test_data_dir(dir, sizeof(dir), "escape");
    r = AuditService_init(&service, dir);
    assert(r.success == 1);

    /* detail 中包含 '|'，应被转义，保证总管道符数仍为 7 */
    r = AuditService_record(
        &service,
        AUDIT_EVENT_CREATE,
        "DOC0001", "DOCTOR",
        "PRESCRIPTION", "PRE0042",
        "OK",
        "a|b|c");
    assert(r.success == 1);

    snprintf(log_path, sizeof(log_path), "%saudit.log", dir);
    read_file(log_path, content, sizeof(content));

    /* 未转义的 '|' 只应该是 7 个分隔符。转义后的 '\|' 中管道符虽然计入，
       但我们检查字段分隔点周围，用更直接的检查：反斜杠转义已出现。 */
    assert(strstr(content, "a\\|b\\|c") != 0);

    /* '|' 总数 = 7 分隔符 + 2 转义管道 = 9 */
    assert(count_char(content, '|') == 9);
}

/**
 * @brief 测试 data_dir 以 '/' 结尾与不以 '/' 结尾都能正确拼接
 */
static void test_init_handles_trailing_slash(void) {
    AuditService service_with;
    AuditService service_without;
    char dir_with[256];
    char dir_without[256];
    size_t len = 0;

    make_test_data_dir(dir_with, sizeof(dir_with), "slash_with");
    assert(AuditService_init(&service_with, dir_with).success == 1);
    assert(strstr(service_with.log_path, "audit.log") != 0);

    /* 构造不带尾部斜杠的版本 */
    make_test_data_dir(dir_without, sizeof(dir_without), "slash_without");
    len = strlen(dir_without);
    if (len > 0 && dir_without[len - 1] == '/') {
        dir_without[len - 1] = '\0';
    }
    assert(AuditService_init(&service_without, dir_without).success == 1);
    assert(strstr(service_without.log_path, "audit.log") != 0);
}

int main(void) {
    test_init_and_record_login_success();
    test_record_with_null_actor();
    test_record_escapes_pipe_in_detail();
    test_init_handles_trailing_slash();
    return 0;
}
