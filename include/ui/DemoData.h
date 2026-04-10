/**
 * @file DemoData.h
 * @brief 演示数据模块 - 提供系统演示数据的重置和种子文件路径解析功能
 *
 * 本模块负责将预设的演示数据（种子文件）复制到运行时数据目录，
 * 从而恢复系统到初始演示状态。适用于演示展示和测试场景。
 */

#ifndef HIS_UI_DEMO_DATA_H
#define HIS_UI_DEMO_DATA_H

#include <stddef.h>

#include "common/Result.h"
#include "ui/MenuApplication.h"

/**
 * @brief 重置所有演示数据
 *
 * 将 demo_seed 目录下的种子文件逐一复制到运行时数据路径，
 * 恢复系统所有数据到初始演示状态。
 *
 * @param paths    包含所有数据文件路径的结构体指针
 * @param buffer   输出缓冲区，用于存放结果消息（如 "演示数据已重置"）
 * @param capacity 缓冲区容量
 * @return Result  成功时返回 "demo data reset complete"，失败时包含错误信息
 */
Result DemoData_reset(const MenuApplicationPaths *paths, char *buffer, size_t capacity);

/**
 * @brief 根据运行时文件路径推导对应的种子文件路径
 *
 * 将运行时路径（如 "data/users.txt"）转换为种子路径（如 "data/demo_seed/users.txt"），
 * 即在父目录下插入 "demo_seed" 子目录。
 *
 * @param runtime_path      运行时数据文件路径
 * @param seed_path         输出缓冲区，用于存放推导出的种子文件路径
 * @param seed_path_capacity 种子路径缓冲区容量
 * @return Result            成功时返回 "demo data seed path ready"，失败时包含错误信息
 */
Result DemoData_resolve_seed_path(
    const char *runtime_path,
    char *seed_path,
    size_t seed_path_capacity
);

#endif
