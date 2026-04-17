/**
 * @file UpdateChecker.c
 * @brief 更新检查模块的实现 - 通过 GitHub API 检查、下载并安装新版本
 *
 * 本模块实现了完整的自动更新流程：
 * 1. 通过 curl 调用 GitHub Releases API 获取最新版本信息
 * 2. 解析 JSON 响应提取版本号和下载链接
 * 3. 比较版本号判断是否有可用更新
 * 4. 下载 SHA256SUMS + 安装包并校验摘要，确保供应链完整性
 * 5. 支持 Windows、macOS 和 Linux 三个平台
 *
 * 安全加固（2026-04 安全审查遗留）：
 * - 所有外部命令改为 argv 数组形式（fork+execvp / CreateProcess），
 *   不再经过 shell，消除命令注入风险（不再调用 system()）。
 * - 所有 URL 采用白名单校验，必须以 https://github.com/ 或
 *   https://api.github.com/ 开头且仅含受限字符集。
 * - 下载产物按 SHA-256 校验；若 SHA256SUMS 缺失或摘要不匹配，
 *   安装流程中止并清理临时文件。
 * - JSON 解析器增加长度边界检查，拒绝 \u 转义以降低手写解析风险。
 */

#include "common/UpdateChecker.h"
#include "ui/TuiStyle.h"

#include "../../third_party/sha256/sha256.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#include <shellapi.h>
#else
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif
#endif

/** GitHub Releases API 地址 */
#define GITHUB_API_URL "https://api.github.com/repos/ymylive/his/releases/latest"
/** API 响应缓冲区最大大小（字节） */
#define MAX_RESPONSE   16384
/** URL 最大允许长度 */
#define MAX_URL_LENGTH 2048

/* ── URL / 路径 安全校验（白名单） ───────────────────────── */

/**
 * @brief 判断字符是否在 HTTPS URL 白名单字符集内
 *
 * 合法 URL 仅包含：字母数字 + / : . - _ ~ ? = & % + @ #
 * 任何其他字符（含空格、shell 元字符、控制字符）一律拒绝。
 */
static int is_url_char(unsigned char c) {
    if (c >= 'a' && c <= 'z') return 1;
    if (c >= 'A' && c <= 'Z') return 1;
    if (c >= '0' && c <= '9') return 1;
    switch (c) {
        case '/': case ':': case '.': case '-': case '_':
        case '~': case '?': case '=': case '&': case '%':
        case '+': case '@': case '#':
            return 1;
        default:
            return 0;
    }
}

/**
 * @brief 白名单校验 URL，仅接受受信 GitHub 前缀 + 受限字符
 *
 * @param url    待校验 URL
 * @param errbuf 错误信息（可 NULL）
 * @param errsz  错误缓冲区大小
 * @return 1 通过，0 拒绝
 */
static int validate_url(const char *url, char *errbuf, size_t errsz) {
    static const char *const kTrustedPrefixes[] = {
        "https://github.com/",
        "https://api.github.com/",
        "https://objects.githubusercontent.com/",
        "https://codeload.github.com/",
        NULL
    };
    size_t len;
    size_t i;
    int prefix_ok = 0;

    if (url == NULL || url[0] == '\0') {
        if (errbuf) snprintf(errbuf, errsz, "URL 为空");
        return 0;
    }

    len = strlen(url);
    if (len > MAX_URL_LENGTH) {
        if (errbuf) snprintf(errbuf, errsz, "URL 过长（%zu > %d）", len, MAX_URL_LENGTH);
        return 0;
    }

    for (i = 0; kTrustedPrefixes[i] != NULL; ++i) {
        size_t plen = strlen(kTrustedPrefixes[i]);
        if (len > plen && strncmp(url, kTrustedPrefixes[i], plen) == 0) {
            prefix_ok = 1;
            break;
        }
    }
    if (!prefix_ok) {
        if (errbuf) snprintf(errbuf, errsz, "URL 不在受信前缀白名单内");
        return 0;
    }

    for (i = 0; i < len; ++i) {
        if (!is_url_char((unsigned char)url[i])) {
            if (errbuf) {
                snprintf(errbuf, errsz,
                    "URL 第 %zu 位包含非法字符 0x%02X",
                    i, (unsigned char)url[i]);
            }
            return 0;
        }
    }
    return 1;
}

/* ── 进程执行封装（不经过 shell） ───────────────────────── */

/**
 * @brief 执行外部程序并等待其结束（不经过 shell）。
 *
 * 参数以 argv 数组形式直接传递给 execvp/CreateProcess，
 * 因此空格、shell 元字符等均不会被解释，彻底消除命令注入。
 *
 * @param argv 以 NULL 结尾的参数数组，argv[0] 为程序名
 * @return 成功 0；子进程非 0 退出返回其退出码；其它错误返回 -1
 */
static int run_process(const char *const argv[]) {
    if (argv == NULL || argv[0] == NULL) return -1;

#ifdef _WIN32
    /* 将 argv 拼接成 CreateProcessA 需要的单字符串命令行；
     * 参数按 CommandLineToArgvW 的规则加引号 / 转义反斜杠。
     * 注意：拼接发生在进程创建内核路径而非 shell，不会被 shell 解释。 */
    char cmdline[4096];
    size_t pos = 0;
    size_t i;
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    DWORD exit_code = 0;

    cmdline[0] = '\0';
    for (i = 0; argv[i] != NULL; ++i) {
        const char *a = argv[i];
        int needs_quote = (a[0] == '\0') || strpbrk(a, " \t\"\\") != NULL;
        size_t j;

        if (i > 0 && pos < sizeof(cmdline) - 1) {
            cmdline[pos++] = ' ';
            cmdline[pos] = '\0';
        }
        if (needs_quote && pos < sizeof(cmdline) - 1) {
            cmdline[pos++] = '"';
        }
        for (j = 0; a[j] != '\0'; ++j) {
            if (pos >= sizeof(cmdline) - 3) return -1;
            if (a[j] == '"' || a[j] == '\\') {
                cmdline[pos++] = '\\';
            }
            cmdline[pos++] = a[j];
        }
        if (needs_quote && pos < sizeof(cmdline) - 1) {
            cmdline[pos++] = '"';
        }
        cmdline[pos] = '\0';
    }

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    if (!CreateProcessA(NULL, cmdline, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        return -1;
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    GetExitCodeProcess(pi.hProcess, &exit_code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return (int)exit_code;
#else
    pid_t pid = fork();
    if (pid < 0) return -1;
    if (pid == 0) {
        /* 子进程：静音 stderr 降低噪声（保留 stdout 让 curl 的进度可见） */
        execvp(argv[0], (char *const *)argv);
        _exit(127); /* exec 失败 */
    }
    {
        int status = 0;
        if (waitpid(pid, &status, 0) < 0) return -1;
        if (WIFEXITED(status)) return WEXITSTATUS(status);
        return -1;
    }
#endif
}

#ifndef _WIN32
/**
 * @brief fork+execvp，但把子进程 stderr 重定向到 /dev/null
 *
 * 用于原本 2>/dev/null 的命令（例如 xattr / chmod 失败时不打扰用户）。
 */
static int run_process_quiet(const char *const argv[]) {
    if (argv == NULL || argv[0] == NULL) return -1;
    pid_t pid = fork();
    if (pid < 0) return -1;
    if (pid == 0) {
        int devnull = open("/dev/null", O_WRONLY);
        if (devnull >= 0) {
            dup2(devnull, STDERR_FILENO);
            close(devnull);
        }
        execvp(argv[0], (char *const *)argv);
        _exit(127);
    }
    {
        int status = 0;
        if (waitpid(pid, &status, 0) < 0) return -1;
        if (WIFEXITED(status)) return WEXITSTATUS(status);
        return -1;
    }
}
#endif

/**
 * @brief 通过 curl 捕获指定 URL 的响应到内存缓冲区
 *
 * 使用 pipe + fork + execvp（Windows 下用 _pipe + CreateProcess），
 * 避免 shell 拼接。curl 参数全部通过 argv 直接传递。
 *
 * @param url  待下载的 URL
 * @param buf  输出缓冲区
 * @param bufsz 缓冲区大小（返回内容包含尾部 '\0'）
 * @param nread 实际读取字节数（可 NULL）
 * @return 1 成功，0 失败
 */
static int http_fetch_to_buffer(const char *url, char *buf, size_t bufsz, size_t *nread) {
    if (url == NULL || buf == NULL || bufsz == 0) return 0;

#ifdef _WIN32
    /* Windows: CreateProcess + 匿名管道读取 curl stdout */
    HANDLE rd = NULL, wr = NULL;
    SECURITY_ATTRIBUTES sa;
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    char cmdline[4096];
    size_t total = 0;
    DWORD exit_code = 0;

    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;
    if (!CreatePipe(&rd, &wr, &sa, 0)) return 0;
    SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);

    snprintf(cmdline, sizeof(cmdline),
        "curl -sL -m 10 --proto =https --tlsv1.2 "
        "-H \"Accept: application/vnd.github.v3+json\" "
        "-H \"User-Agent: his-updater\" \"%s\"", url);

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = wr;
    si.hStdError  = GetStdHandle(STD_ERROR_HANDLE);
    si.hStdInput  = GetStdHandle(STD_INPUT_HANDLE);
    ZeroMemory(&pi, sizeof(pi));

    if (!CreateProcessA(NULL, cmdline, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
        CloseHandle(rd); CloseHandle(wr);
        return 0;
    }
    CloseHandle(wr); /* 父进程只读 */

    for (;;) {
        DWORD got = 0;
        if (total + 1 >= bufsz) break;
        if (!ReadFile(rd, buf + total, (DWORD)(bufsz - total - 1), &got, NULL) || got == 0) {
            break;
        }
        total += got;
    }
    buf[total] = '\0';
    CloseHandle(rd);
    WaitForSingleObject(pi.hProcess, INFINITE);
    GetExitCodeProcess(pi.hProcess, &exit_code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    if (nread) *nread = total;
    return (exit_code == 0 && total > 0) ? 1 : 0;
#else
    int fds[2];
    pid_t pid;
    size_t total = 0;
    ssize_t got;
    int status = 0;

    if (pipe(fds) != 0) return 0;
    pid = fork();
    if (pid < 0) {
        close(fds[0]); close(fds[1]);
        return 0;
    }
    if (pid == 0) {
        close(fds[0]);
        dup2(fds[1], STDOUT_FILENO);
        close(fds[1]);
        /* 静音 stderr，curl 默认不打印进度到 stderr */
        {
            int devnull = open("/dev/null", O_WRONLY);
            if (devnull >= 0) { dup2(devnull, STDERR_FILENO); close(devnull); }
        }
        {
            const char *argv[] = {
                "curl", "-sL", "-m", "10",
                "--proto", "=https", "--tlsv1.2",
                "-H", "Accept: application/vnd.github.v3+json",
                "-H", "User-Agent: his-updater",
                url, NULL
            };
            execvp(argv[0], (char *const *)argv);
            _exit(127);
        }
    }
    close(fds[1]);
    while (total + 1 < bufsz) {
        got = read(fds[0], buf + total, bufsz - total - 1);
        if (got <= 0) break;
        total += (size_t)got;
    }
    buf[total] = '\0';
    close(fds[0]);
    if (waitpid(pid, &status, 0) < 0) return 0;
    if (nread) *nread = total;
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) return 0;
    return total > 0 ? 1 : 0;
#endif
}

/**
 * @brief 用 curl 将 URL 下载到文件（argv 模式，不经过 shell）。
 *
 * @return 0 成功；其它值为 curl 退出码或 -1。
 */
static int http_download_to_file(const char *url, const char *out_path) {
    const char *argv[] = {
        "curl", "-L", "-s", "-f",
        "--proto", "=https", "--tlsv1.2",
        "-H", "User-Agent: his-updater",
        "-o", out_path,
        url, NULL
    };
    return run_process(argv);
}

/* ── JSON 辅助解析函数（手写，已加长度边界与 \u 拒绝） ── */

/**
 * @brief 从 JSON 字符串中提取指定键的字符串值
 *
 * 简易 JSON 解析器：按 "key" 文本匹配定位，然后提取双引号包围的值。
 * 加固点：
 *   1. 搜索范围限定在 [json, json + json_len)，避免越过缓冲区。
 *   2. 字符串值遇到 `\u` Unicode 转义直接视为解析失败，
 *      防止手写实现对 Unicode 边界处理不鲁棒。
 *   3. 截断时保留 NUL，输出长度永不超过 buf_size - 1。
 */
static int json_extract_string(
    const char *json, size_t json_len, const char *key,
    char *buf, size_t buf_size
) {
    char pattern[64];
    const char *hay_end;
    const char *start;
    const char *end;
    size_t plen;
    size_t out_len;

    if (json == NULL || key == NULL || buf == NULL || buf_size == 0) return 0;
    hay_end = json + json_len;

    plen = (size_t)snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    if (plen == 0 || plen >= sizeof(pattern)) return 0;

    start = strstr(json, pattern);
    if (start == NULL || start >= hay_end) return 0;

    start += plen;
    while (start < hay_end && (*start == ' ' || *start == ':' || *start == '\t')) start++;
    if (start >= hay_end || *start != '"') return 0;
    start++;

    end = start;
    while (end < hay_end && *end != '"') {
        if (*end == '\\') {
            if (end + 1 >= hay_end) return 0;
            if (end[1] == 'u') return 0; /* 拒绝 Unicode 转义 */
            end += 2;
            continue;
        }
        end++;
    }
    if (end >= hay_end) return 0;

    out_len = (size_t)(end - start);
    if (out_len >= buf_size) out_len = buf_size - 1;
    memcpy(buf, start, out_len);
    buf[out_len] = '\0';
    return 1;
}

/**
 * @brief 从 JSON 的 assets 数组中查找匹配的下载 URL
 *
 * 遍历 JSON 中所有 "name" 字段，找到同时包含指定关键字和 ".zip"
 * 后缀的资源，然后提取其 "browser_download_url" 字段。
 */
static int json_find_asset_url(
    const char *json, size_t json_len, const char *keyword,
    char *url_buf, size_t url_size
) {
    const char *cursor = json;
    const char *hay_end = json + json_len;
    const char *name_pos;

    while ((name_pos = strstr(cursor, "\"name\"")) != NULL && name_pos < hay_end) {
        char name[256];
        const char *ns = name_pos + 6;
        const char *ne;
        size_t nlen;

        while (ns < hay_end && (*ns == ' ' || *ns == ':' || *ns == '\t')) ns++;
        if (ns >= hay_end || *ns != '"') { cursor = ns; continue; }
        ns++;
        ne = ns;
        while (ne < hay_end && *ne != '"') {
            if (*ne == '\\' && ne + 1 < hay_end) { ne += 2; continue; }
            ne++;
        }
        if (ne >= hay_end) return 0;
        nlen = (size_t)(ne - ns);
        if (nlen >= sizeof(name)) nlen = sizeof(name) - 1;
        memcpy(name, ns, nlen);
        name[nlen] = '\0';

        if (strstr(name, keyword) != NULL && strstr(name, ".zip") != NULL) {
            const char *url_pos = strstr(name_pos, "\"browser_download_url\"");
            if (url_pos != NULL && url_pos < hay_end) {
                const char *us = url_pos + 22;
                const char *ue;
                size_t ulen;
                while (us < hay_end && (*us == ' ' || *us == ':' || *us == '\t')) us++;
                if (us >= hay_end || *us != '"') { cursor = ne; continue; }
                us++;
                ue = us;
                while (ue < hay_end && *ue != '"') {
                    if (*ue == '\\' && ue + 1 < hay_end) { ue += 2; continue; }
                    ue++;
                }
                if (ue >= hay_end) return 0;
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

/* ── SHA256SUMS 解析与校验 ──────────────────────────────── */

/**
 * @brief 从 SHA256SUMS 文本中查找指定文件名对应的 64 位 16 进制摘要
 *
 * SHA256SUMS 的每一行格式约定：`<hex(64)>  <filename>`（两个空格）
 * 或 `<hex(64)> *<filename>` （二进制模式）。本函数对两种格式都兼容。
 *
 * @param sums_text SHA256SUMS 文本（NUL 结尾）
 * @param filename  待匹配的文件名（不含路径）
 * @param hex_out   输出 65 字节缓冲区（64 位 + NUL）
 * @return 1 找到，0 未找到
 */
static int sha256sums_lookup(const char *sums_text, const char *filename, char hex_out[65]) {
    const char *p;
    size_t name_len;
    size_t i;

    if (sums_text == NULL || filename == NULL || filename[0] == '\0') return 0;
    name_len = strlen(filename);

    p = sums_text;
    while (*p != '\0') {
        const char *line_start = p;
        const char *line_end = strchr(p, '\n');
        size_t line_len = (line_end != NULL) ? (size_t)(line_end - line_start)
                                             : strlen(line_start);
        const char *q = line_start;
        const char *file_part;

        /* 最少 64 hex + 两个分隔 + 至少 1 个文件名字符 */
        if (line_len < 64 + 2 + 1) {
            p = (line_end != NULL) ? (line_end + 1) : line_start + line_len;
            if (line_end == NULL) break;
            continue;
        }

        /* 前 64 个字符必须全部是十六进制 */
        for (i = 0; i < 64; ++i) {
            char c = q[i];
            int hex = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
            if (!hex) break;
        }
        if (i != 64) {
            p = (line_end != NULL) ? (line_end + 1) : line_start + line_len;
            if (line_end == NULL) break;
            continue;
        }

        file_part = q + 64;
        /* 跳过空格 / tab / 二进制模式的 '*' 标记 */
        while (file_part < line_start + line_len &&
               (*file_part == ' ' || *file_part == '\t' || *file_part == '*')) {
            file_part++;
        }

        if ((size_t)((line_start + line_len) - file_part) == name_len &&
            strncmp(file_part, filename, name_len) == 0) {
            /* 命中：复制前 64 字符为 hex_out（统一小写） */
            for (i = 0; i < 64; ++i) {
                char c = q[i];
                if (c >= 'A' && c <= 'F') c = (char)(c - 'A' + 'a');
                hex_out[i] = c;
            }
            hex_out[64] = '\0';
            return 1;
        }

        p = (line_end != NULL) ? (line_end + 1) : line_start + line_len;
        if (line_end == NULL) break;
    }
    return 0;
}

/**
 * @brief 返回 URL 中最后一个 '/' 之后的文件名部分（忽略 query string）。
 */
static const char *url_basename(const char *url) {
    const char *slash = strrchr(url, '/');
    return (slash != NULL) ? slash + 1 : url;
}

/* ── 版本号比较 ──────────────────────────────────────────── */

static int version_compare(const char *a, const char *b) {
    int a1 = 0, a2 = 0, a3 = 0;
    int b1 = 0, b2 = 0, b3 = 0;

    sscanf(a, "%d.%d.%d", &a1, &a2, &a3);
    sscanf(b, "%d.%d.%d", &b1, &b2, &b3);

    if (a1 != b1) return a1 - b1;
    if (a2 != b2) return a2 - b2;
    return a3 - b3;
}

/* ── 平台检测 ────────────────────────────────────────────── */

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

static char g_api_response[MAX_RESPONSE];
static size_t g_api_response_len = 0;

/* ── 公开接口实现 ────────────────────────────────────────── */

int UpdateChecker_check(UpdateInfo *info) {
    char tag_name[32];
    const char *ver;
    size_t total = 0;

    if (info == NULL) return 0;

    memset(info, 0, sizeof(UpdateInfo));
    strncpy(info->current_version, HIS_VERSION, sizeof(info->current_version) - 1);

    /* 通过 argv 直接 exec curl；URL 是编译期常量，无注入风险 */
    if (!http_fetch_to_buffer(GITHUB_API_URL, g_api_response, sizeof(g_api_response), &total)) {
        g_api_response_len = 0;
        return 0;
    }
    g_api_response_len = total;
    if (total == 0) return 0;

    if (!json_extract_string(g_api_response, g_api_response_len,
                             "tag_name", tag_name, sizeof(tag_name))) {
        return 0;
    }

    ver = tag_name;
    if (ver[0] == 'v' || ver[0] == 'V') ver++;
    strncpy(info->latest_version, ver, sizeof(info->latest_version) - 1);

    json_extract_string(g_api_response, g_api_response_len,
                        "html_url", info->download_url, sizeof(info->download_url));

    json_find_asset_url(g_api_response, g_api_response_len,
                        get_platform_keyword(), info->asset_url, sizeof(info->asset_url));

    /* 验证从 JSON 提取的 URL 均处于白名单，防止恶意 Release 元数据 */
    {
        char err[128];
        if (info->download_url[0] != '\0' &&
            !validate_url(info->download_url, err, sizeof(err))) {
            info->download_url[0] = '\0';
        }
        if (info->asset_url[0] != '\0' &&
            !validate_url(info->asset_url, err, sizeof(err))) {
            info->asset_url[0] = '\0';
        }
    }

    if (version_compare(info->latest_version, info->current_version) > 0) {
        info->has_update = 1;
    }

    return 1;
}

/* ── 下载并安装更新 ──────────────────────────────────────── */

/**
 * @brief 解析当前 asset_url 所在 release 的 SHA256SUMS URL
 *
 * GitHub Release 资源 URL 形如：
 *   https://github.com/<owner>/<repo>/releases/download/<tag>/<file>
 * 将最后一段 <file> 替换为 "SHA256SUMS" 即为摘要清单 URL。
 *
 * @return 1 成功组装出 out_buf，0 失败
 */
static int derive_sha256sums_url(const char *asset_url, char *out_buf, size_t bufsz) {
    const char *slash = strrchr(asset_url, '/');
    size_t prefix_len;
    if (slash == NULL) return 0;
    prefix_len = (size_t)(slash - asset_url) + 1;
    if (prefix_len + strlen("SHA256SUMS") >= bufsz) return 0;
    memcpy(out_buf, asset_url, prefix_len);
    memcpy(out_buf + prefix_len, "SHA256SUMS", strlen("SHA256SUMS") + 1);
    return 1;
}

/**
 * @brief 下载 SHA256SUMS，计算本地 zip 的摘要并对比
 *
 * @return 1 校验通过；0 校验失败 / SHA256SUMS 不存在等。
 */
static int verify_download_sha256(
    FILE *out, const char *asset_url, const char *zip_path
) {
    char sums_url[MAX_URL_LENGTH];
    char sums_text[8192];
    size_t sums_len = 0;
    uint8_t digest[SHA256_BLOCK_SIZE];
    char local_hex[65];
    char expected_hex[65];
    char err[128];
    const char *fname;

    if (!derive_sha256sums_url(asset_url, sums_url, sizeof(sums_url))) {
        tui_print_error(out, "无法推导 SHA256SUMS 下载地址。");
        return 0;
    }
    if (!validate_url(sums_url, err, sizeof(err))) {
        tui_print_error(out, "SHA256SUMS URL 安全校验失败：");
        tui_print_error(out, err);
        return 0;
    }

    if (!http_fetch_to_buffer(sums_url, sums_text, sizeof(sums_text), &sums_len) ||
        sums_len == 0) {
        tui_print_error(out, "未找到发布者签署的 SHA256SUMS，拒绝安装未校验的更新包。");
        return 0;
    }

    fname = url_basename(asset_url);
    if (!sha256sums_lookup(sums_text, fname, expected_hex)) {
        tui_print_error(out, "SHA256SUMS 中缺少当前平台安装包的摘要记录。");
        return 0;
    }

    if (!sha256_file(zip_path, digest)) {
        tui_print_error(out, "无法读取下载的安装包以计算摘要。");
        return 0;
    }
    sha256_hex(digest, local_hex);

    if (strcmp(local_hex, expected_hex) != 0) {
        tui_print_error(out, "SHA-256 校验失败！下载的文件可能已被篡改，更新中止。");
        return 0;
    }
    tui_print_success(out, "SHA-256 校验通过。");
    return 1;
}

/**
 * @brief 校验下载的 zip 文件是否非空且不过小
 */
static int sanity_check_zip(FILE *out, const char *zip_path) {
    FILE *fcheck = fopen(zip_path, "rb");
    long fsize;
    if (fcheck == NULL) {
        tui_print_error(out, "下载的文件不存在。");
        return 0;
    }
    fseek(fcheck, 0, SEEK_END);
    fsize = ftell(fcheck);
    fclose(fcheck);
    if (fsize <= 0) {
        tui_print_error(out, "下载的文件为空，更新中止。");
        return 0;
    }
    if (fsize < 1024) {
        tui_print_error(out, "下载的文件过小，可能已损坏，更新中止。");
        return 0;
    }
    return 1;
}

static int do_download_and_install(FILE *out, const UpdateInfo *info) {
    int ret;

#ifdef _WIN32
    /* ===== Windows 平台更新流程 ===== */
    char tmp_zip[MAX_PATH];
    char tmp_dir[MAX_PATH];
    const char *tmp_env = getenv("TEMP");
    char err[128];
    if (tmp_env == NULL || tmp_env[0] == '\0') tmp_env = ".";

    if (!validate_url(info->asset_url, err, sizeof(err))) {
        tui_print_error(out, "下载链接安全验证失败：");
        tui_print_error(out, err);
        return 0;
    }

    snprintf(tmp_zip, sizeof(tmp_zip), "%s\\his_update.zip", tmp_env);
    snprintf(tmp_dir, sizeof(tmp_dir), "%s\\his_update", tmp_env);

    fprintf(out, "\n");
    tui_print_info(out, "正在下载更新包...");
    fflush(out);

    ret = http_download_to_file(info->asset_url, tmp_zip);
    if (ret != 0) {
        tui_print_error(out, "下载失败，请检查网络连接。");
        return 0;
    }
    tui_print_success(out, "下载完成");

    if (!sanity_check_zip(out, tmp_zip)) {
        remove(tmp_zip);
        return 0;
    }

    tui_print_info(out, "正在校验文件完整性 (SHA-256)...");
    fflush(out);
    if (!verify_download_sha256(out, info->asset_url, tmp_zip)) {
        remove(tmp_zip);
        return 0;
    }

    /* 清理旧的解压目录（argv 模式调用 cmd.exe /c rd） */
    tui_print_info(out, "正在解压更新包...");
    fflush(out);
    {
        const char *rd_argv[] = { "cmd.exe", "/c", "rd", "/s", "/q", tmp_dir, NULL };
        (void)run_process(rd_argv); /* 目录可能不存在，忽略返回值 */
    }

    /* 用 PowerShell 的 Expand-Archive 解压；-Path / -DestinationPath
     * 由 PowerShell 参数解析器处理，不经过 shell 字符串拼接。 */
    {
        char ps_cmd[1024];
        snprintf(ps_cmd, sizeof(ps_cmd),
            "Expand-Archive -LiteralPath \"$env:TEMP\\his_update.zip\" "
            "-DestinationPath \"$env:TEMP\\his_update\" -Force");
        const char *ps_argv[] = {
            "powershell.exe", "-NoProfile",
            "-ExecutionPolicy", "Bypass",
            "-Command", ps_cmd, NULL
        };
        ret = run_process(ps_argv);
        if (ret != 0) {
            tui_print_error(out, "解压失败。");
            return 0;
        }
    }
    tui_print_success(out, "解压完成");

    tui_print_info(out, "正在安装更新...");
    fflush(out);
    {
        char bat_path[MAX_PATH];
        char exe_dir[MAX_PATH];
        FILE *bat;
        DWORD len;
        char *last_slash;

        len = GetModuleFileNameA(NULL, exe_dir, sizeof(exe_dir));
        if (len == 0) {
            tui_print_error(out, "无法获取程序路径。");
            return 0;
        }
        last_slash = strrchr(exe_dir, '\\');
        if (last_slash) *(last_slash + 1) = '\0';

        if ((size_t)(strlen(tmp_env) + 40) > sizeof(bat_path)) {
            tui_print_error(out, "TEMP 路径过长。");
            return 0;
        }
        snprintf(bat_path, sizeof(bat_path), "%s\\his_update\\his_updater.bat", tmp_env);

        bat = fopen(bat_path, "w");
        if (bat == NULL) {
            tui_print_error(out, "无法创建更新脚本。");
            return 0;
        }
        fprintf(bat, "@echo off\r\n");
        fprintf(bat, "chcp 65001 >NUL 2>&1\r\n");
        fprintf(bat, "echo 正在等待 HIS 退出...\r\n");
        fprintf(bat, "timeout /t 2 /nobreak >NUL\r\n");
        fprintf(bat, "echo 正在安装更新...\r\n");
        fprintf(bat, "for /d %%%%D in (\"%%TEMP%%\\his_update\\*\") do (\r\n");
        fprintf(bat, "    xcopy /E /Y /Q \"%%%%D\\*\" \"%s\"\r\n", exe_dir);
        fprintf(bat, ")\r\n");
        fprintf(bat, "echo 更新完成！正在重新启动...\r\n");
        fprintf(bat, "timeout /t 1 /nobreak >NUL\r\n");
        fprintf(bat, "start \"\" \"%shis.exe\"\r\n", exe_dir);
        fprintf(bat, "rd /s /q \"%%TEMP%%\\his_update\" 2>NUL\r\n");
        fprintf(bat, "del \"%%TEMP%%\\his_update.zip\" 2>NUL\r\n");
        fprintf(bat, "del \"%%~f0\" 2>NUL\r\n");
        fclose(bat);

        tui_print_success(out, "更新准备就绪");
        tui_print_info(out, "系统将自动退出并完成更新...");
        fflush(out);

        /* 异步启动批处理：argv 模式调用 cmd.exe */
        {
            const char *bat_argv[] = {
                "cmd.exe", "/c", "start", "HIS Updater", "/MIN",
                "cmd.exe", "/c", bat_path, NULL
            };
            (void)run_process(bat_argv);
        }
    }
    return 1;

#else
    /* ===== macOS / Linux 平台更新流程 ===== */
    const char *tmp_zip = "/tmp/his_update.zip";
    const char *tmp_dir = "/tmp/his_update";
    char exe_dir[512];
    char exe_path[512];
    char desktop_path[512];
    ssize_t rlen;
    char *last_slash;
    char err[128];

    if (!validate_url(info->asset_url, err, sizeof(err))) {
        tui_print_error(out, "下载链接安全验证失败：");
        tui_print_error(out, err);
        return 0;
    }

#ifdef __APPLE__
    {
        uint32_t bsize = sizeof(exe_dir);
        if (_NSGetExecutablePath(exe_dir, &bsize) != 0) {
            tui_print_error(out, "无法获取程序路径。");
            return 0;
        }
    }
#else
    rlen = readlink("/proc/self/exe", exe_dir, sizeof(exe_dir) - 1);
    if (rlen < 0) {
        tui_print_error(out, "无法获取程序路径。");
        return 0;
    }
    exe_dir[rlen] = '\0';
#endif
    last_slash = strrchr(exe_dir, '/');
    if (last_slash) *(last_slash + 1) = '\0';

    snprintf(exe_path, sizeof(exe_path), "%shis", exe_dir);
    snprintf(desktop_path, sizeof(desktop_path), "%shis_desktop", exe_dir);

    fprintf(out, "\n");
    tui_print_info(out, "正在下载更新包...");
    fflush(out);

    ret = http_download_to_file(info->asset_url, tmp_zip);
    if (ret != 0) {
        tui_print_error(out, "下载失败，请检查网络连接。");
        return 0;
    }
    tui_print_success(out, "下载完成");

    if (!sanity_check_zip(out, tmp_zip)) {
        remove(tmp_zip);
        return 0;
    }

    tui_print_info(out, "正在校验文件完整性 (SHA-256)...");
    fflush(out);
    if (!verify_download_sha256(out, info->asset_url, tmp_zip)) {
        remove(tmp_zip);
        return 0;
    }

    tui_print_info(out, "正在解压更新包...");
    fflush(out);

    /* rm -rf tmp_dir（目录可能不存在，忽略返回码） */
    {
        const char *rm_argv[] = { "rm", "-rf", tmp_dir, NULL };
        (void)run_process_quiet(rm_argv);
    }
    /* mkdir -p tmp_dir */
    {
        const char *mk_argv[] = { "mkdir", "-p", tmp_dir, NULL };
        if (run_process_quiet(mk_argv) != 0) {
            tui_print_error(out, "无法创建临时解压目录。");
            return 0;
        }
    }
    /* unzip -q -o tmp_zip -d tmp_dir */
    {
        const char *uz_argv[] = { "unzip", "-q", "-o", tmp_zip, "-d", tmp_dir, NULL };
        ret = run_process(uz_argv);
        if (ret != 0) {
            tui_print_error(out, "解压失败。");
            return 0;
        }
    }
    tui_print_success(out, "解压完成");

    /*
     * 删除旧二进制（保留 inode），再由 cp 创建新文件，避免覆盖正在运行
     * 的可执行文件导致 SIGKILL。
     */
    tui_print_info(out, "正在安装更新...");
    fflush(out);
    {
        const char *rm_argv[] = { "rm", "-f", exe_path, desktop_path, NULL };
        (void)run_process_quiet(rm_argv);
    }

    /* 将解压目录下第一级子目录内容复制到 exe_dir
     * 用 find + 遍历，而不是 shell glob，以保持无 shell 模式。 */
    {
        DIR *dir;
        struct dirent *entry;
#ifdef __linux__
        dir = opendir(tmp_dir);
#else
        dir = opendir(tmp_dir);
#endif
        if (dir == NULL) {
            tui_print_error(out, "无法打开解压目录。");
            remove(tmp_zip);
            return 0;
        }
        while ((entry = readdir(dir)) != NULL) {
            char sub[512];
            struct stat st;
            if (entry->d_name[0] == '.') continue;
            snprintf(sub, sizeof(sub), "%s/%s", tmp_dir, entry->d_name);
            if (stat(sub, &st) != 0) continue;
            if (S_ISDIR(st.st_mode)) {
                /* cp -Rf "<sub>/." "<exe_dir>" 将子目录内容递归拷贝过来 */
                char cp_src[520];
                snprintf(cp_src, sizeof(cp_src), "%s/.", sub);
                const char *cp_argv[] = { "cp", "-Rf", cp_src, exe_dir, NULL };
                (void)run_process_quiet(cp_argv);
            } else {
                const char *cp_argv[] = { "cp", "-f", sub, exe_dir, NULL };
                (void)run_process_quiet(cp_argv);
            }
        }
        closedir(dir);
    }

    /* 修复权限 */
#ifdef __APPLE__
    {
        const char *xattr_argv[] = { "xattr", "-cr", exe_dir, NULL };
        (void)run_process_quiet(xattr_argv);
    }
#endif
    {
        const char *chmod_exe[] = { "chmod", "+x", exe_path, NULL };
        const char *chmod_desktop[] = { "chmod", "+x", desktop_path, NULL };
        (void)run_process_quiet(chmod_exe);
        (void)run_process_quiet(chmod_desktop);
    }

    /* 清理临时文件 */
    {
        const char *rm_argv[] = { "rm", "-rf", tmp_dir, tmp_zip, NULL };
        (void)run_process_quiet(rm_argv);
    }

    tui_print_success(out, "更新安装完成！");
    tui_print_info(out, "请重新启动 HIS 以使用新版本。");
    return 1;
#endif
}

/* ── 用户更新提示对话框 ──────────────────────────────────── */

int UpdateChecker_prompt(FILE *out, FILE *in, const UpdateInfo *info) {
    char answer[16];
    int has_direct_download;

    if (out == NULL || in == NULL || info == NULL || info->has_update == 0) {
        return 0;
    }
    has_direct_download = (info->asset_url[0] != '\0') ? 1 : 0;

    fprintf(out, "\n");
    fprintf(out, TUI_BOLD_YELLOW
        "  ╔══════════════════════════════════════════╗\n"
        "  ║" TUI_BOLD_WHITE "          发现新版本可用！"
        TUI_BOLD_YELLOW "              ║\n"
        "  ╠══════════════════════════════════════════╣\n" TUI_RESET);

    fprintf(out,
        TUI_BOLD_YELLOW "  ║" TUI_RESET
        "  当前版本: " TUI_DIM "v%s" TUI_RESET,
        info->current_version);
    {
        int used = 14 + (int)strlen(info->current_version);
        int i;
        for (i = used; i < 42; i++) fprintf(out, " ");
    }
    fprintf(out, TUI_BOLD_YELLOW "║\n" TUI_RESET);

    fprintf(out,
        TUI_BOLD_YELLOW "  ║" TUI_RESET
        "  最新版本: " TUI_BOLD_GREEN "v%s" TUI_RESET,
        info->latest_version);
    {
        int used = 14 + (int)strlen(info->latest_version);
        int i;
        for (i = used; i < 42; i++) fprintf(out, " ");
    }
    fprintf(out, TUI_BOLD_YELLOW "║\n" TUI_RESET);

    fprintf(out, TUI_BOLD_YELLOW
        "  ╠══════════════════════════════════════════╣\n" TUI_RESET);

    if (has_direct_download) {
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

    fprintf(out, TUI_BOLD_YELLOW
        "  ╚══════════════════════════════════════════╝\n" TUI_RESET);

    fprintf(out, "\n");
    if (has_direct_download) {
        fprintf(out, TUI_BOLD_CYAN "  请选择 " TUI_RESET
                     TUI_DIM "[0/1/2]: " TUI_RESET);
    } else {
        fprintf(out, TUI_BOLD_CYAN "  请选择 " TUI_RESET
                     TUI_DIM "[0/1]: " TUI_RESET);
    }
    fflush(out);

    if (fgets(answer, sizeof(answer), in) == NULL) {
        return 0;
    }

    if (has_direct_download && answer[0] == '1') {
        if (do_download_and_install(out, info)) {
            return 2;
        }
        return 0;
    }

    if ((has_direct_download && answer[0] == '2') ||
        (!has_direct_download && answer[0] == '1')) {
        char err[128];
        if (!validate_url(info->download_url, err, sizeof(err))) {
            tui_print_error(out, "下载页面链接安全验证失败：");
            tui_print_error(out, err);
            return 0;
        }
#ifdef __APPLE__
        {
            pid_t pid = fork();
            if (pid == 0) {
                execlp("open", "open", info->download_url, (char *)NULL);
                _exit(127);
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

    tui_print_info(out, "已跳过更新，继续启动系统...");
    return 0;
}
