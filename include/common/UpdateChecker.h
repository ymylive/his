/**
 * @file UpdateChecker.h
 * @brief 更新检查模块 - 通过 GitHub API 检查并安装新版本
 *
 * 本模块提供从 GitHub Releases 检查是否有新版本、
 * 向用户展示更新提示、以及自动下载安装更新的功能。
 * 支持 Windows、macOS 和 Linux 平台。
 */

#ifndef HIS_COMMON_UPDATE_CHECKER_H
#define HIS_COMMON_UPDATE_CHECKER_H

#include <stdio.h>

/** 当前软件版本号 */
#define HIS_VERSION "4.0.0"

/**
 * @brief 更新信息结构体
 *
 * 存储版本检查的结果，包括是否有更新、版本号和下载链接。
 */
typedef struct UpdateInfo {
    int has_update;              /**< 是否有可用更新：1 表示有，0 表示无 */
    char latest_version[32];     /**< 远程最新版本号字符串 */
    char current_version[32];    /**< 当前运行的版本号字符串 */
    char download_url[256];      /**< GitHub 发布页面 URL */
    char asset_url[512];         /**< 当前平台对应的 zip 包直接下载 URL */
} UpdateInfo;

/**
 * @brief 检查 GitHub 上是否有新版本
 *
 * 通过 curl 调用 GitHub API 获取最新发布信息，
 * 解析 JSON 响应并与当前版本号进行比较。
 *
 * @param info 输出参数，用于存储检查结果
 * @return 检查成功返回 1，失败（网络错误等）返回 0
 */
int UpdateChecker_check(UpdateInfo *info);

/**
 * @brief 向用户展示更新提示对话框
 *
 * 在终端中以 TUI 样式展示版本信息和操作选项，
 * 等待用户选择后执行相应操作（下载安装、打开浏览器或跳过）。
 *
 * @param out  输出流（通常为 stdout），用于显示提示信息
 * @param in   输入流（通常为 stdin），用于读取用户选择
 * @param info 更新信息结构体（由 UpdateChecker_check 填充）
 * @return 0 = 用户跳过更新
 *         1 = 用户选择在浏览器中打开下载页
 *         2 = 更新已安装，调用者应退出程序以完成重启
 */
int UpdateChecker_prompt(FILE *out, FILE *in, const UpdateInfo *info);

#endif
