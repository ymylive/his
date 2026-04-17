/**
 * @file AuthServiceTestHooks.h
 * @brief AuthService 的测试专用钩子接口
 *
 * 仅供单元测试使用。通过注入自定义时钟函数，可以在测试中验证
 * 依赖时间的逻辑（如登录锁定到期），而不必真正等待时间流逝。
 */

#ifndef HIS_SERVICE_AUTH_SERVICE_TEST_HOOKS_H
#define HIS_SERVICE_AUTH_SERVICE_TEST_HOOKS_H

/**
 * @brief 注入自定义的 "当前时间" 函数（仅供测试使用）
 *
 * 默认情况下 AuthService 调用 time(NULL) 获取时钟。测试可以通过
 * 此函数注入可控时钟源，验证锁定到期逻辑；传入 NULL 恢复默认。
 *
 * @param now_fn 返回当前时间（Unix epoch 秒）的函数指针
 */
void AuthService_set_clock_for_testing(long (*now_fn)(void));

#endif
