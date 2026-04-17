/**
 * @file AuthServiceTestHooks.h
 * @brief AuthService 的测试专用钩子接口
 *
 * 仅供单元测试使用。通过注入自定义时钟函数，可以在测试中验证
 * 依赖时间的逻辑（如登录锁定到期），而不必真正等待时间流逝。
 */

#ifndef HIS_SERVICE_AUTH_SERVICE_TEST_HOOKS_H
#define HIS_SERVICE_AUTH_SERVICE_TEST_HOOKS_H

#include <stddef.h>
#include <stdint.h>

/**
 * @brief 注入自定义的 "当前时间" 函数（仅供测试使用）
 *
 * 默认情况下 AuthService 调用 time(NULL) 获取时钟。测试可以通过
 * 此函数注入可控时钟源，验证锁定到期逻辑；传入 NULL 恢复默认。
 *
 * @param now_fn 返回当前时间（Unix epoch 秒）的函数指针
 */
void AuthService_set_clock_for_testing(long (*now_fn)(void));

/**
 * @brief 供单元测试校验 PBKDF2 实现正确性的直通入口
 *
 * 允许测试验证 RFC / NIST 公开的 PBKDF2-HMAC-SHA256 向量。
 * 生产代码路径不应调用此符号。
 */
void AuthService_pbkdf2_hmac_sha256_for_testing(
    const uint8_t *password, size_t pwd_len,
    const uint8_t *salt, size_t salt_len,
    uint32_t iterations,
    uint8_t *out, size_t out_len
);

#endif
