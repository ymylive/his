/**
 * @file UpdateChecker.c
 * @brief 更新检查模块的实现 - 通过 GitHub API 检查、下载并安装新版本
 *
 * 本模块实现了完整的自动更新流程：
 * 1. 通过 curl 调用 GitHub Releases API 获取最新版本信息
 * 2. 解析 JSON 响应提取版本号和下载链接
 * 3. 比较版本号判断是否有可用更新
 * 4. 支持自动下载 zip 包并安装到当前目录
 * 5. 支持 Windows、macOS 和 Linux 三个平台
 */

#include "common/UpdateChecker.h"
#include "ui/TuiStyle.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#include <shellapi.h>
#define popen  _popen
#define pclose _pclose
#else
#include <unistd.h>
#include <sys/wait.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif
#endif

/** GitHub Releases API 地址 */
#define GITHUB_API_URL "https://api.github.com/repos/ymylive/his/releases/latest"
/** GitHub 仓库标识 */
#define GITHUB_REPO    "ymylive/his"
/** API 响应缓冲区最大大小（字节） */
#define MAX_RESPONSE   16384
/** URL 最大允许长度 */
#define MAX_URL_LENGTH 2048
/** 文件路径最大允许长度 */
#define MAX_PATH_LENGTH 512

/* ── 安全验证函数 ───────────────────────────────────────── */

/**
 * @brief 检查字符串是否包含 shell 元字符
 *
 * 拒绝可被用于命令注入的危险字符，包括：
 * ; | & $ ` ( ) { } < > ! # ' \n \r \\ (反斜杠)
 *
 * @param str    要检查的字符串
 * @param errbuf 错误信息输出缓冲区（可为 NULL）
 * @param errsz  错误缓冲区大小
 * @return 安全返回 1，包含危险字符返回 0
 */
static int check_no_shell_metachar(const char *str, char *errbuf, size_t errsz) {
    /* 禁止的 shell 元字符集合 */
    static const char forbidden[] = ";|&$`(){}[]<>!#'\"\n\r\\";
    size_t i;

    if (str == NULL) {
        if (errbuf) snprintf(errbuf, errsz, "输入字符串为空指针");
        return 0;
    }

    for (i = 0; str[i] != '\0'; i++) {
        if (strchr(forbidden, str[i]) != NULL) {
            if (errbuf) {
                snprintf(errbuf, errsz,
                    "检测到危险字符 0x%02X 在位置 %zu",
                    (unsigned char)str[i], i);
            }
            return 0;
        }
    }
    return 1;
}

/**
 * @brief 验证 URL 是否安全可用于 shell 命令
 *
 * 执行以下安全检查：
 * 1. 非空且长度不超过 MAX_URL_LENGTH
 * 2. 必须以 "https://" 开头（禁止 http、file、ftp 等协议）
 * 3. 不得包含任何 shell 元字符（防止命令注入）
 *
 * @param url    要验证的 URL 字符串
 * @param errbuf 错误信息输出缓冲区（可为 NULL）
 * @param errsz  错误缓冲区大小
 * @return URL 安全返回 1，不安全返回 0
 */
static int validate_url_for_shell(const char *url, char *errbuf, size_t errsz) {
    size_t len;

    if (url == NULL || url[0] == '\0') {
        if (errbuf) snprintf(errbuf, errsz, "URL 为空");
        return 0;
    }

    len = strlen(url);
    if (len > MAX_URL_LENGTH) {
        if (errbuf) snprintf(errbuf, errsz, "URL 过长（%zu > %d）", len, MAX_URL_LENGTH);
        return 0;
    }

    /* 强制要求 HTTPS 协议 */
    if (strncmp(url, "https://", 8) != 0) {
        if (errbuf) snprintf(errbuf, errsz, "URL 必须以 https:// 开头");
        return 0;
    }

    /* 检查 URL 中不含 shell 元字符。
     * 注意：合法的 HTTPS URL 仅由字母、数字和 /:.-_~?=&%+@ 组成，
     * 这里采用黑名单方式拒绝已知的危险字符。 */
    if (!check_no_shell_metachar(url, errbuf, errsz)) {
        return 0;
    }

    return 1;
}

/**
 * @brief 验证文件路径是否安全可用于 shell 命令
 *
 * 执行以下安全检查：
 * 1. 非空且长度不超过 MAX_PATH_LENGTH
 * 2. 不得包含 shell 元字符（防止命令注入）
 * 3. 仅允许字母、数字、/ \ . _ - 和空格（白名单方式）
 *
 * @param path   要验证的路径字符串
 * @param errbuf 错误信息输出缓冲区（可为 NULL）
 * @param errsz  错误缓冲区大小
 * @return 路径安全返回 1，不安全返回 0
 */
static int validate_path_for_shell(const char *path, char *errbuf, size_t errsz) {
    size_t len;
    size_t i;

    if (path == NULL || path[0] == '\0') {
        if (errbuf) snprintf(errbuf, errsz, "路径为空");
        return 0;
    }

    len = strlen(path);
    if (len > MAX_PATH_LENGTH) {
        if (errbuf) snprintf(errbuf, errsz, "路径过长（%zu > %d）", len, MAX_PATH_LENGTH);
        return 0;
    }

    /* 路径白名单：仅允许安全字符 */
    for (i = 0; i < len; i++) {
        char c = path[i];
        int safe = 0;
        if (c >= 'a' && c <= 'z') safe = 1;
        else if (c >= 'A' && c <= 'Z') safe = 1;
        else if (c >= '0' && c <= '9') safe = 1;
        else if (c == '/' || c == '\\' || c == '.' || c == '_' || c == '-' || c == ' ' || c == ':') safe = 1;
        if (!safe) {
            if (errbuf) {
                snprintf(errbuf, errsz,
                    "路径包含不允许的字符 0x%02X 在位置 %zu",
                    (unsigned char)c, i);
            }
            return 0;
        }
    }

    return 1;
}

/* ── JSON 辅助解析函数 ───────────────────────────────────── */

/**
 * @brief 从 JSON 字符串中提取指定键的字符串值
 *
 * 简易 JSON 解析器，通过字符串匹配定位键名，然后提取对应的字符串值。
 * 不依赖第三方 JSON 库，适用于结构简单的 GitHub API 响应。
 *
 * @param json     JSON 字符串
 * @param key      要提取的键名
 * @param buf      输出缓冲区
 * @param buf_size 输出缓冲区大小
 * @return 成功提取返回 1，未找到返回 0
 */
static int json_extract_string(
    const char *json, const char *key,
    char *buf, size_t buf_size
) {
    char pattern[64];
    const char *start = 0;
    const char *end = 0;
    size_t len = 0;

    /* 构造搜索模式 "key" */
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    start = strstr(json, pattern);
    if (start == 0) return 0;

    /* 跳过键名及冒号、空白等分隔字符 */
    start += strlen(pattern);
    while (*start == ' ' || *start == ':' || *start == '\t') start++;
    /* 期望值以双引号开头 */
    if (*start != '"') return 0;
    start++; /* 跳过开头双引号 */

    /* 找到值的结束位置（匹配闭合双引号，处理转义字符） */
    end = start;
    while (*end != '\0' && *end != '"') {
        if (*end == '\\') { end++; } /* 跳过转义字符 */
        if (*end != '\0') end++;
    }

    /* 将提取的值复制到输出缓冲区 */
    len = (size_t)(end - start);
    if (len >= buf_size) len = buf_size - 1;
    memcpy(buf, start, len);
    buf[len] = '\0';
    return 1;
}

/**
 * @brief 从 JSON 的 assets 数组中查找匹配的下载 URL
 *
 * 遍历 JSON 中所有 "name" 字段，找到同时包含指定关键字和 ".zip"
 * 后缀的资源，然后提取其 "browser_download_url" 字段。
 *
 * @param json    JSON 字符串
 * @param keyword 平台关键字（如 "win64"、"macos-arm64" 等）
 * @param url_buf 输出 URL 缓冲区
 * @param url_size 输出缓冲区大小
 * @return 找到匹配的资源返回 1，未找到返回 0
 */
static int json_find_asset_url(
    const char *json, const char *keyword,
    char *url_buf, size_t url_size
) {
    const char *cursor = json;
    const char *name_pos = 0;
    const char *url_pos = 0;

    /* 遍历所有 "name" 字段 */
    while ((name_pos = strstr(cursor, "\"name\"")) != 0) {
        char name[256];
        const char *ns = name_pos + 6; /* 跳过 "name" */
        const char *ne = 0;
        size_t nlen = 0;

        /* 跳过分隔符，提取 name 的值 */
        while (*ns == ' ' || *ns == ':' || *ns == '\t') ns++;
        if (*ns != '"') { cursor = ns; continue; }
        ns++;
        ne = ns;
        while (*ne != '\0' && *ne != '"') ne++;
        nlen = (size_t)(ne - ns);
        if (nlen >= sizeof(name)) nlen = sizeof(name) - 1;
        memcpy(name, ns, nlen);
        name[nlen] = '\0';

        /* 检查文件名是否同时包含平台关键字和 .zip 后缀 */
        if (strstr(name, keyword) != 0 && strstr(name, ".zip") != 0) {
            /* 找到匹配的资源文件，在附近查找 browser_download_url */
            url_pos = strstr(name_pos, "\"browser_download_url\"");
            if (url_pos != 0) {
                const char *us = url_pos + 22; /* 跳过 "browser_download_url" */
                const char *ue = 0;
                size_t ulen = 0;
                while (*us == ' ' || *us == ':' || *us == '\t') us++;
                if (*us != '"') { cursor = ne; continue; }
                us++;
                ue = us;
                while (*ue != '\0' && *ue != '"') ue++;
                ulen = (size_t)(ue - us);
                if (ulen >= url_size) ulen = url_size - 1;
                memcpy(url_buf, us, ulen);
                url_buf[ulen] = '\0';
                return 1;
            }
        }
        cursor = ne;
    }
    return 0;
}

/* ── 版本号比较 ──────────────────────────────────────────── */

/**
 * @brief 比较两个语义化版本号字符串（major.minor.patch）
 *
 * @param a 版本号字符串 A
 * @param b 版本号字符串 B
 * @return a > b 返回正数，a < b 返回负数，相等返回 0
 */
static int version_compare(const char *a, const char *b) {
    int a1 = 0, a2 = 0, a3 = 0; /* A 的主版本、次版本、补丁版本 */
    int b1 = 0, b2 = 0, b3 = 0; /* B 的主版本、次版本、补丁版本 */

    sscanf(a, "%d.%d.%d", &a1, &a2, &a3);
    sscanf(b, "%d.%d.%d", &b1, &b2, &b3);

    /* 依次比较主版本、次版本、补丁版本 */
    if (a1 != b1) return a1 - b1;
    if (a2 != b2) return a2 - b2;
    return a3 - b3;
}

/* ── 平台检测 ────────────────────────────────────────────── */

/**
 * @brief 获取当前平台的关键字标识
 *
 * 根据编译期宏定义返回对应平台的关键字字符串，
 * 用于在 GitHub Release 资源中匹配对应平台的下载包。
 *
 * @return 平台关键字字符串（如 "win64"、"macos-arm64"、"linux" 等）
 */
static const char *get_platform_keyword(void) {
#if defined(_WIN32)
    #if defined(_WIN64) || defined(__x86_64__) || defined(__amd64__)
        return "win64";
    #else
        return "win32";
    #endif
#elif defined(__APPLE__)
    #if defined(__aarch64__) || defined(__arm64__)
        return "macos-arm64";
    #else
        return "macos-x86_64";
    #endif
#elif defined(__linux__)
    return "linux";
#else
    return "portable";
#endif
}

/* ── 全局变量：缓存 API 响应的原始 JSON ─────────────────── */

/** 用于存储 GitHub API 返回的原始 JSON 响应 */
static char g_api_response[MAX_RESPONSE];

/* ── 公开接口实现 ────────────────────────────────────────── */

/**
 * @brief 检查 GitHub 上是否有新版本
 *
 * 通过 curl 调用 GitHub API 获取最新发布信息，解析 JSON
 * 响应提取版本号、下载页面 URL 和平台对应的安装包 URL，
 * 最后与当前版本号比较判断是否需要更新。
 *
 * @param info 输出参数，用于存储检查结果
 * @return 检查成功返回 1，失败返回 0
 */
int UpdateChecker_check(UpdateInfo *info) {
    FILE *pipe = 0;
    size_t total = 0;
    size_t bytes_read = 0;
    char tag_name[32];
    const char *ver = 0;

    if (info == 0) return 0;

    /* 初始化更新信息结构体 */
    memset(info, 0, sizeof(UpdateInfo));
    strncpy(info->current_version, HIS_VERSION, sizeof(info->current_version) - 1);

    /* 通过 popen 执行 curl 命令获取 GitHub API 响应 */
#ifdef _WIN32
    pipe = popen(
        "curl -s -m 5 -H \"Accept: application/vnd.github.v3+json\" "
        "\"" GITHUB_API_URL "\" 2>NUL",
        "r"
    );
#else
    pipe = popen(
        "curl -s -m 5 -H \"Accept: application/vnd.github.v3+json\" "
        "\"" GITHUB_API_URL "\" 2>/dev/null",
        "r"
    );
#endif
    if (pipe == 0) return 0;

    /* 读取 API 响应内容到全局缓冲区 */
    while ((bytes_read = fread(g_api_response + total, 1,
                               sizeof(g_api_response) - total - 1, pipe)) > 0) {
        total += bytes_read;
        if (total >= sizeof(g_api_response) - 1) break;
    }
    g_api_response[total] = '\0';
    pclose(pipe);

    if (total == 0) return 0; /* 无响应数据，检查失败 */

    /* 从 JSON 中提取 tag_name（如 "v3.1.0"） */
    if (!json_extract_string(g_api_response, "tag_name", tag_name, sizeof(tag_name))) {
        return 0;
    }

    /* 去除版本号前导的 'v' 或 'V' */
    ver = tag_name;
    if (ver[0] == 'v' || ver[0] == 'V') ver++;

    strncpy(info->latest_version, ver, sizeof(info->latest_version) - 1);

    /* 提取 GitHub 发布页面 URL */
    json_extract_string(g_api_response, "html_url",
                        info->download_url, sizeof(info->download_url));

    /* 查找当前平台对应的安装包直接下载 URL */
    json_find_asset_url(g_api_response, get_platform_keyword(),
                        info->asset_url, sizeof(info->asset_url));

    /* 比较版本号，判断是否有新版本 */
    if (version_compare(info->latest_version, info->current_version) > 0) {
        info->has_update = 1;
    }

    return 1;
}

/* ── 下载并安装更新 ──────────────────────────────────────── */

/**
 * @brief 打印更新对话框的一行（带左右边框）
 *
 * 内部辅助函数，用于绘制 TUI 样式的对话框行。
 *
 * @param out   输出流
 * @param left  行左侧内容
 * @param right 行右侧边框字符
 * @param width 行的总宽度（用于空格填充）
 */
static void print_box_line(FILE *out, const char *left, const char *right, int width) {
    int used = (int)strlen(left);
    int i = 0;
    fprintf(out, TUI_BOLD_YELLOW "  %s" TUI_RESET, left);
    /* 用空格填充至指定宽度 */
    for (i = used; i < width; i++) fprintf(out, " ");
    fprintf(out, TUI_BOLD_YELLOW "%s" TUI_RESET "\n", right);
}

/**
 * @brief 执行下载和安装更新的核心逻辑
 *
 * 根据平台分别实现：
 * - Windows: 下载 zip -> 解压 -> 创建批处理脚本 -> 退出后自动覆盖安装
 * - macOS/Linux: 下载 zip -> 解压 -> 直接覆盖安装 -> 修复权限
 *
 * @param out  输出流，用于显示进度信息
 * @param info 更新信息结构体（包含下载 URL）
 * @return 成功返回 1，失败返回 0
 */
static int do_download_and_install(FILE *out, const UpdateInfo *info) {
    char cmd[1024];
    int ret = 0;

#ifdef _WIN32
    /* ===== Windows 平台更新流程 ===== */
    const char *tmp_zip = "%TEMP%\\his_update.zip";
    const char *tmp_dir = "%TEMP%\\his_update";

    /* 安全验证：在执行任何 shell 命令前，验证所有外部输入 */
    {
        char security_err[256];
        if (!validate_url_for_shell(info->asset_url, security_err, sizeof(security_err))) {
            tui_print_error(out, "下载链接安全验证失败：");
            tui_print_error(out, security_err);
            return 0;
        }
    }

    fprintf(out, "\n");
    tui_print_info(out, "正在下载更新包...");
    fflush(out);

    /* 第一步：使用 curl 下载 zip 安装包 */
    snprintf(cmd, sizeof(cmd),
        "curl -L -s --progress-bar -o \"%s\" \"%s\"",
        tmp_zip, info->asset_url);
    ret = system(cmd);
    if (ret != 0) {
        tui_print_error(out, "下载失败，请检查网络连接。");
        return 0;
    }
    tui_print_success(out, "下载完成");

    /* 第二步：使用 PowerShell 解压 zip 文件 */
    tui_print_info(out, "正在解压更新包...");
    fflush(out);

    snprintf(cmd, sizeof(cmd),
        "if exist \"%s\" rd /s /q \"%s\" && "
        "powershell -NoProfile -Command \""
        "Expand-Archive -Path '%s' -DestinationPath '%s' -Force"
        "\"",
        tmp_dir, tmp_dir, tmp_zip, tmp_dir);
    ret = system(cmd);
    if (ret != 0) {
        tui_print_error(out, "解压失败。");
        return 0;
    }
    tui_print_success(out, "解压完成");

    /* 第三步：创建更新批处理脚本 */
    tui_print_info(out, "正在安装更新...");
    fflush(out);

    /*
     * Windows 下无法直接覆盖正在运行的可执行文件，
     * 因此创建一个批处理脚本，在当前进程退出后自动执行覆盖安装。
     * 脚本流程：等待 HIS 退出 -> 复制文件 -> 重新启动 -> 清理临时文件
     */
    {
        char bat_path[512];
        FILE *bat = 0;
        char exe_dir[512];
        DWORD len = GetModuleFileNameA(NULL, exe_dir, sizeof(exe_dir));
        char *last_slash = 0;

        if (len == 0) {
            tui_print_error(out, "无法获取程序路径。");
            return 0;
        }
        /* 从完整路径中截取目录部分 */
        last_slash = strrchr(exe_dir, '\\');
        if (last_slash) *(last_slash + 1) = '\0';

        /* 拼接批处理脚本路径 */
        {
            const char *tmp_env = getenv("TEMP");
            if (tmp_env == 0) tmp_env = ".";
            if (strlen(tmp_env) + 40 > sizeof(bat_path)) {
                tui_print_error(out, "TEMP路径过长。");
                return 0;
            }
            snprintf(bat_path, sizeof(bat_path), "%s\\his_update\\his_updater.bat", tmp_env);
        }
        bat = fopen(bat_path, "w");
        if (bat == 0) {
            tui_print_error(out, "无法创建更新脚本。");
            return 0;
        }

        /* 写入批处理脚本内容 */
        fprintf(bat, "@echo off\r\n");
        fprintf(bat, "chcp 65001 >NUL 2>&1\r\n");         /* 设置 UTF-8 编码 */
        fprintf(bat, "echo 正在等待 HIS 退出...\r\n");
        fprintf(bat, "timeout /t 2 /nobreak >NUL\r\n");    /* 等待 2 秒让旧进程退出 */
        fprintf(bat, "echo 正在安装更新...\r\n");
        /* 遍历解压目录的子文件夹，将内容复制到程序目录 */
        fprintf(bat, "for /d %%%%D in (\"%%TEMP%%\\his_update\\*\") do (\r\n");
        fprintf(bat, "    xcopy /E /Y /Q \"%%%%D\\*\" \"%s\"\r\n", exe_dir);
        fprintf(bat, ")\r\n");
        fprintf(bat, "echo 更新完成！正在重新启动...\r\n");
        fprintf(bat, "timeout /t 1 /nobreak >NUL\r\n");
        fprintf(bat, "start \"\" \"%shis.exe\"\r\n", exe_dir); /* 启动新版本 */
        fprintf(bat, "rd /s /q \"%%TEMP%%\\his_update\" 2>NUL\r\n"); /* 清理解压目录 */
        fprintf(bat, "del \"%%TEMP%%\\his_update.zip\" 2>NUL\r\n");  /* 清理 zip 文件 */
        fprintf(bat, "del \"%%~f0\" 2>NUL\r\n");           /* 删除批处理脚本自身 */
        fclose(bat);

        tui_print_success(out, "更新准备就绪");
        tui_print_info(out, "系统将自动退出并完成更新...");
        fflush(out);

        /* 以最小化窗口方式启动更新脚本 */
        {
            char security_err[256];
            if (!validate_path_for_shell(bat_path, security_err, sizeof(security_err))) {
                tui_print_error(out, "更新脚本路径安全验证失败：");
                tui_print_error(out, security_err);
                return 0;
            }
        }
        snprintf(cmd, sizeof(cmd), "start \"HIS Updater\" /MIN cmd /c \"%s\"", bat_path);
        system(cmd);
    }

    return 1;

#else
    /* ===== macOS / Linux 平台更新流程 ===== */
    char tmp_zip[256];
    char tmp_dir[256];
    char exe_dir[512];
    ssize_t rlen = 0;
    char *last_slash = 0;

    snprintf(tmp_zip, sizeof(tmp_zip), "/tmp/his_update.zip");
    snprintf(tmp_dir, sizeof(tmp_dir), "/tmp/his_update");

    /* 检测当前可执行文件所在目录 */
#ifdef __APPLE__
    {
        /* macOS 使用 _NSGetExecutablePath 获取可执行文件路径 */
        uint32_t bsize = sizeof(exe_dir);
        if (_NSGetExecutablePath(exe_dir, &bsize) != 0) {
            tui_print_error(out, "无法获取程序路径。");
            return 0;
        }
    }
#else
    /* Linux 通过 /proc/self/exe 软链接获取可执行文件路径 */
    rlen = readlink("/proc/self/exe", exe_dir, sizeof(exe_dir) - 1);
    if (rlen < 0) {
        tui_print_error(out, "无法获取程序路径。");
        return 0;
    }
    exe_dir[rlen] = '\0';
#endif
    /* 从完整路径中截取目录部分 */
    last_slash = strrchr(exe_dir, '/');
    if (last_slash) *(last_slash + 1) = '\0';

    /* 安全验证：在执行任何 shell 命令前，验证所有外部输入 */
    {
        char security_err[256];
        if (!validate_url_for_shell(info->asset_url, security_err, sizeof(security_err))) {
            tui_print_error(out, "下载链接安全验证失败：");
            tui_print_error(out, security_err);
            return 0;
        }
        if (!validate_path_for_shell(exe_dir, security_err, sizeof(security_err))) {
            tui_print_error(out, "程序路径安全验证失败：");
            tui_print_error(out, security_err);
            return 0;
        }
    }

    fprintf(out, "\n");
    tui_print_info(out, "正在下载更新包...");
    fflush(out);

    /* 第一步：使用 curl 下载 zip 安装包（-L 跟随重定向） */
    snprintf(cmd, sizeof(cmd),
        "curl -L -s --progress-bar -o \"%s\" \"%s\" 2>&1",
        tmp_zip, info->asset_url);
    ret = system(cmd);
    if (ret != 0) {
        tui_print_error(out, "下载失败，请检查网络连接。");
        return 0;
    }
    tui_print_success(out, "下载完成");

    /* 第二步：清理旧的临时目录并解压 zip 文件 */
    tui_print_info(out, "正在解压更新包...");
    fflush(out);

    snprintf(cmd, sizeof(cmd),
        "rm -rf \"%s\" && mkdir -p \"%s\" && unzip -q -o \"%s\" -d \"%s\"",
        tmp_dir, tmp_dir, tmp_zip, tmp_dir);
    ret = system(cmd);
    if (ret != 0) {
        tui_print_error(out, "解压失败。");
        return 0;
    }
    tui_print_success(out, "解压完成");

    /* 第三步：先删除旧的可执行文件，再复制新文件到安装目录
     *
     * 关键：不能直接 cp 覆盖正在运行的二进制文件！
     * 在 macOS/Linux 上，cp 会就地覆写文件内容，导致内核缓存的代码页失效，
     * 进程被 SIGKILL (zsh: killed)。正确做法是先 rm 旧文件（Unix 会保留
     * inode 直到进程退出），再 cp 新文件（创建新 inode），这样当前进程
     * 继续使用旧 inode 运行，新版本等重启后生效。
     */
    tui_print_info(out, "正在安装更新...");
    fflush(out);

    /* 先删除旧的可执行文件（rm 对运行中的二进制是安全的） */
    snprintf(cmd, sizeof(cmd),
        "rm -f \"%shis\" \"%shis_desktop\" 2>/dev/null",
        exe_dir, exe_dir);
    system(cmd);

    /* 复制新文件到安装目录 */
    snprintf(cmd, sizeof(cmd),
        "for d in \"%s\"/*/; do cp -Rf \"$d\"* \"%s\" 2>/dev/null; done",
        tmp_dir, exe_dir);
    ret = system(cmd);

    /* 第四步：修复文件权限 */
#ifdef __APPLE__
    /* macOS 需要清除扩展属性（quarantine）并添加执行权限 */
    snprintf(cmd, sizeof(cmd),
        "xattr -cr \"%s\" 2>/dev/null; chmod +x \"%shis\" 2>/dev/null; "
        "chmod +x \"%shis_desktop\" 2>/dev/null",
        exe_dir, exe_dir, exe_dir);
    system(cmd);
#else
    /* Linux 仅需添加执行权限 */
    snprintf(cmd, sizeof(cmd),
        "chmod +x \"%shis\" 2>/dev/null; chmod +x \"%shis_desktop\" 2>/dev/null",
        exe_dir, exe_dir);
    system(cmd);
#endif

    /* 第五步：清理临时文件 */
    snprintf(cmd, sizeof(cmd),
        "rm -rf \"%s\" \"%s\"", tmp_dir, tmp_zip);
    system(cmd);

    tui_print_success(out, "更新安装完成！");
    tui_print_info(out, "请重新启动 HIS 以使用新版本。");

    return 1;
#endif
}

/* ── 用户更新提示对话框 ──────────────────────────────────── */

/**
 * @brief 向用户展示更新提示对话框
 *
 * 在终端中绘制带边框的 TUI 风格对话框，展示当前版本和最新版本，
 * 并提供操作选项。根据是否有直接下载链接，显示不同的菜单选项。
 *
 * @param out  输出流
 * @param in   输入流
 * @param info 更新信息结构体
 * @return 0=跳过更新, 1=已打开浏览器, 2=已安装更新需重启
 */
int UpdateChecker_prompt(FILE *out, FILE *in, const UpdateInfo *info) {
    char answer[16];
    int has_direct_download = 0;

    /* 参数校验：输入输出流和更新信息不能为空，且必须有可用更新 */
    if (out == 0 || in == 0 || info == 0 || info->has_update == 0) {
        return 0;
    }

    /* 判断是否有当前平台的直接下载链接 */
    has_direct_download = (info->asset_url[0] != '\0') ? 1 : 0;

    /* 绘制更新提示对话框的标题区域 */
    fprintf(out, "\n");
    fprintf(out, TUI_BOLD_YELLOW
        "  ╔══════════════════════════════════════════╗\n"
        "  ║" TUI_BOLD_WHITE "          发现新版本可用！"
        TUI_BOLD_YELLOW "              ║\n"
        "  ╠══════════════════════════════════════════╣\n" TUI_RESET);

    /* 显示当前版本号（灰色/暗淡样式） */
    fprintf(out,
        TUI_BOLD_YELLOW "  ║" TUI_RESET
        "  当前版本: " TUI_DIM "v%s" TUI_RESET,
        info->current_version);
    {
        int used = 14 + (int)strlen(info->current_version);
        int i = 0;
        for (i = used; i < 42; i++) fprintf(out, " "); /* 填充空格对齐右边框 */
    }
    fprintf(out, TUI_BOLD_YELLOW "║\n" TUI_RESET);

    /* 显示最新版本号（绿色高亮样式） */
    fprintf(out,
        TUI_BOLD_YELLOW "  ║" TUI_RESET
        "  最新版本: " TUI_BOLD_GREEN "v%s" TUI_RESET,
        info->latest_version);
    {
        int used = 14 + (int)strlen(info->latest_version);
        int i = 0;
        for (i = used; i < 42; i++) fprintf(out, " ");
    }
    fprintf(out, TUI_BOLD_YELLOW "║\n" TUI_RESET);

    /* 绘制分隔线 */
    fprintf(out, TUI_BOLD_YELLOW
        "  ╠══════════════════════════════════════════╣\n" TUI_RESET);

    /* 根据是否有直接下载链接，显示不同的菜单选项 */
    if (has_direct_download) {
        /* 有直接下载链接时提供三个选项 */
        fprintf(out,
            TUI_BOLD_YELLOW "  ║" TUI_RESET
            "  " TUI_BOLD_WHITE "1." TUI_RESET " 立即下载并安装更新"
            "              "
            TUI_BOLD_YELLOW "║\n" TUI_RESET);
        fprintf(out,
            TUI_BOLD_YELLOW "  ║" TUI_RESET
            "  " TUI_BOLD_WHITE "2." TUI_RESET " 在浏览器中打开下载页"
            "              "
            TUI_BOLD_YELLOW "║\n" TUI_RESET);
        fprintf(out,
            TUI_BOLD_YELLOW "  ║" TUI_RESET
            "  " TUI_DIM    "0. 跳过更新" TUI_RESET
            "                              "
            TUI_BOLD_YELLOW "║\n" TUI_RESET);
    } else {
        /* 无直接下载链接时只提供两个选项 */
        fprintf(out,
            TUI_BOLD_YELLOW "  ║" TUI_RESET
            "  " TUI_BOLD_WHITE "1." TUI_RESET " 在浏览器中打开下载页"
            "              "
            TUI_BOLD_YELLOW "║\n" TUI_RESET);
        fprintf(out,
            TUI_BOLD_YELLOW "  ║" TUI_RESET
            "  " TUI_DIM    "0. 跳过更新" TUI_RESET
            "                              "
            TUI_BOLD_YELLOW "║\n" TUI_RESET);
    }

    /* 绘制底部边框 */
    fprintf(out, TUI_BOLD_YELLOW
        "  ╚══════════════════════════════════════════╝\n" TUI_RESET);

    /* 显示输入提示并等待用户选择 */
    fprintf(out, "\n");
    if (has_direct_download) {
        fprintf(out, TUI_BOLD_CYAN "  请选择 " TUI_RESET
                     TUI_DIM "[0/1/2]: " TUI_RESET);
    } else {
        fprintf(out, TUI_BOLD_CYAN "  请选择 " TUI_RESET
                     TUI_DIM "[0/1]: " TUI_RESET);
    }
    fflush(out);

    /* 读取用户输入 */
    if (fgets(answer, sizeof(answer), in) == 0) {
        return 0;
    }

    /* 处理用户选择：直接下载安装 */
    if (has_direct_download && answer[0] == '1') {
        if (do_download_and_install(out, info)) {
#ifdef _WIN32
            return 2; /* Windows：通知调用者退出，由批处理脚本完成后续安装 */
#else
            return 2; /* macOS/Linux：安装已完成，通知调用者退出以重启 */
#endif
        }
        return 0;
    }

    /* 处理用户选择：在浏览器中打开下载页面 */
    if ((has_direct_download && answer[0] == '2') ||
        (!has_direct_download && answer[0] == '1')) {
        /* 安全验证：验证 download_url 为合法 HTTPS 链接，防止打开恶意 URI */
        {
            char security_err[256];
            if (!validate_url_for_shell(info->download_url, security_err, sizeof(security_err))) {
                tui_print_error(out, "下载页面链接安全验证失败：");
                tui_print_error(out, security_err);
                return 0;
            }
        }
        /* 使用平台原生方式打开浏览器，避免通过 system() 造成命令注入风险 */
#ifdef __APPLE__
        {
            pid_t pid = fork();
            if (pid == 0) {
                execlp("open", "open", info->download_url, (char *)NULL);
                _exit(127); /* exec 失败时退出子进程 */
            }
        }
#elif defined(_WIN32)
        ShellExecuteA(NULL, "open", info->download_url, NULL, NULL, SW_SHOWNORMAL);
#else
        {
            pid_t pid = fork();
            if (pid == 0) {
                execlp("xdg-open", "xdg-open", info->download_url, (char *)NULL);
                _exit(127);
            }
        }
#endif
        tui_print_success(out, "已在浏览器中打开下载页面");
        return 1;
    }

    /* 用户选择跳过更新（输入 0 或其他值） */
    tui_print_info(out, "已跳过更新，继续启动系统...");
    return 0;
}
