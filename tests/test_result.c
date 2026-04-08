/**
 * @file test_result.c
 * @brief 通用结果类型（Result）的单元测试文件
 *
 * 本文件测试 Result 结构体的基本功能：
 * - 创建成功结果并验证 success 标志和消息
 * - 创建失败结果并验证 success 标志和消息
 */

#include <assert.h>
#include <string.h>

#include "common/Result.h"

/**
 * @brief 测试主函数
 *
 * 验证 Result_make_success 和 Result_make_failure 的行为：
 * - 成功结果的 success 应为 1，消息为 "ok"
 * - 失败结果的 success 应为 0，消息为 "fail"
 */
int main(void) {
    /* 创建成功结果 */
    Result ok = Result_make_success("ok");
    /* 创建失败结果 */
    Result fail = Result_make_failure("fail");

    /* 验证成功结果 */
    assert(ok.success == 1);                    /* success 标志为 1 */
    assert(strcmp(ok.message, "ok") == 0);       /* 消息为 "ok" */

    /* 验证失败结果 */
    assert(fail.success == 0);                   /* success 标志为 0 */
    assert(strcmp(fail.message, "fail") == 0);   /* 消息为 "fail" */

    return 0;
}
