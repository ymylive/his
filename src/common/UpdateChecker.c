#include "common/UpdateChecker.h"
#include "ui/TuiStyle.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#define popen  _popen
#define pclose _pclose
#else
#include <unistd.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif
#endif

#define GITHUB_API_URL "https://api.github.com/repos/ymylive/his/releases/latest"
#define GITHUB_REPO    "ymylive/his"
#define MAX_RESPONSE   16384

/* ── JSON helpers ─────────────────────────────────────────── */

static int json_extract_string(
    const char *json, const char *key,
    char *buf, size_t buf_size
) {
    char pattern[64];
    const char *start = 0;
    const char *end = 0;
    size_t len = 0;

    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    start = strstr(json, pattern);
    if (start == 0) return 0;

    start += strlen(pattern);
    while (*start == ' ' || *start == ':' || *start == '\t') start++;
    if (*start != '"') return 0;
    start++;

    end = start;
    while (*end != '\0' && *end != '"') {
        if (*end == '\\') { end++; }
        if (*end != '\0') end++;
    }

    len = (size_t)(end - start);
    if (len >= buf_size) len = buf_size - 1;
    memcpy(buf, start, len);
    buf[len] = '\0';
    return 1;
}

/*
 * Find a download URL in the JSON whose "name" field contains `keyword`.
 * Searches the "assets" array for "browser_download_url".
 */
static int json_find_asset_url(
    const char *json, const char *keyword,
    char *url_buf, size_t url_size
) {
    const char *cursor = json;
    const char *name_pos = 0;
    const char *url_pos = 0;

    while ((name_pos = strstr(cursor, "\"name\"")) != 0) {
        char name[256];
        const char *ns = name_pos + 6; /* skip "name" */
        const char *ne = 0;
        size_t nlen = 0;

        while (*ns == ' ' || *ns == ':' || *ns == '\t') ns++;
        if (*ns != '"') { cursor = ns; continue; }
        ns++;
        ne = ns;
        while (*ne != '\0' && *ne != '"') ne++;
        nlen = (size_t)(ne - ns);
        if (nlen >= sizeof(name)) nlen = sizeof(name) - 1;
        memcpy(name, ns, nlen);
        name[nlen] = '\0';

        if (strstr(name, keyword) != 0 && strstr(name, ".zip") != 0) {
            /* Found matching asset — look for browser_download_url nearby */
            url_pos = strstr(name_pos, "\"browser_download_url\"");
            if (url_pos != 0) {
                const char *us = url_pos + 22;
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

/* ── Version comparison ──────────────────────────────────── */

static int version_compare(const char *a, const char *b) {
    int a1 = 0, a2 = 0, a3 = 0;
    int b1 = 0, b2 = 0, b3 = 0;

    sscanf(a, "%d.%d.%d", &a1, &a2, &a3);
    sscanf(b, "%d.%d.%d", &b1, &b2, &b3);

    if (a1 != b1) return a1 - b1;
    if (a2 != b2) return a2 - b2;
    return a3 - b3;
}

/* ── Platform detection ──────────────────────────────────── */

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

/* ── Stored raw JSON for asset lookup ─────────────────────── */

static char g_api_response[MAX_RESPONSE];

/* ── Public API ──────────────────────────────────────────── */

int UpdateChecker_check(UpdateInfo *info) {
    FILE *pipe = 0;
    size_t total = 0;
    size_t bytes_read = 0;
    char tag_name[32];
    const char *ver = 0;

    if (info == 0) return 0;

    memset(info, 0, sizeof(UpdateInfo));
    strncpy(info->current_version, HIS_VERSION, sizeof(info->current_version) - 1);

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

    while ((bytes_read = fread(g_api_response + total, 1,
                               sizeof(g_api_response) - total - 1, pipe)) > 0) {
        total += bytes_read;
        if (total >= sizeof(g_api_response) - 1) break;
    }
    g_api_response[total] = '\0';
    pclose(pipe);

    if (total == 0) return 0;

    /* Extract tag_name (e.g. "v3.1.0") */
    if (!json_extract_string(g_api_response, "tag_name", tag_name, sizeof(tag_name))) {
        return 0;
    }

    /* Strip leading 'v' */
    ver = tag_name;
    if (ver[0] == 'v' || ver[0] == 'V') ver++;

    strncpy(info->latest_version, ver, sizeof(info->latest_version) - 1);

    /* Extract release page URL */
    json_extract_string(g_api_response, "html_url",
                        info->download_url, sizeof(info->download_url));

    /* Find platform-specific asset URL */
    json_find_asset_url(g_api_response, get_platform_keyword(),
                        info->asset_url, sizeof(info->asset_url));

    /* Compare versions */
    if (version_compare(info->latest_version, info->current_version) > 0) {
        info->has_update = 1;
    }

    return 1;
}

/* ── Download and install ────────────────────────────────── */

static void print_box_line(FILE *out, const char *left, const char *right, int width) {
    int used = (int)strlen(left);
    int i = 0;
    fprintf(out, TUI_BOLD_YELLOW "  %s" TUI_RESET, left);
    for (i = used; i < width; i++) fprintf(out, " ");
    fprintf(out, TUI_BOLD_YELLOW "%s" TUI_RESET "\n", right);
}

static int do_download_and_install(FILE *out, const UpdateInfo *info) {
    char cmd[1024];
    int ret = 0;

#ifdef _WIN32
    /* Windows: download zip → extract → copy over current dir */
    const char *tmp_zip = "%TEMP%\\his_update.zip";
    const char *tmp_dir = "%TEMP%\\his_update";

    fprintf(out, "\n");
    tui_print_info(out, "正在下载更新包...");
    fflush(out);

    snprintf(cmd, sizeof(cmd),
        "curl -L -s --progress-bar -o \"%s\" \"%s\"",
        tmp_zip, info->asset_url);
    ret = system(cmd);
    if (ret != 0) {
        tui_print_error(out, "下载失败，请检查网络连接。");
        return 0;
    }
    tui_print_success(out, "下载完成");

    tui_print_info(out, "正在解压更新包...");
    fflush(out);

    /* Clean old temp dir and extract */
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

    tui_print_info(out, "正在安装更新...");
    fflush(out);

    /*
     * The zip contains a top-level folder like lightweight-his-portable-vX.Y.Z-win64.
     * We use xcopy to copy everything from the inner folder to the current directory,
     * overwriting existing files. The /E copies subdirectories, /Y suppresses prompts.
     * We schedule a batch script to do the copy after this process exits
     * to avoid file-in-use issues with the running executable.
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
        last_slash = strrchr(exe_dir, '\\');
        if (last_slash) *(last_slash + 1) = '\0';

        snprintf(bat_path, sizeof(bat_path), "%s\\his_update\\his_updater.bat", getenv("TEMP") ? getenv("TEMP") : ".");
        bat = fopen(bat_path, "w");
        if (bat == 0) {
            tui_print_error(out, "无法创建更新脚本。");
            return 0;
        }

        fprintf(bat, "@echo off\r\n");
        fprintf(bat, "chcp 65001 >NUL 2>&1\r\n");
        fprintf(bat, "echo 正在等待 HIS 退出...\r\n");
        fprintf(bat, "timeout /t 2 /nobreak >NUL\r\n");
        fprintf(bat, "echo 正在安装更新...\r\n");
        /* Copy all files from the extracted folder (including subfolder) to exe_dir */
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

        snprintf(cmd, sizeof(cmd), "start \"HIS Updater\" /MIN cmd /c \"%s\"", bat_path);
        system(cmd);
    }

    return 1;

#else
    /* macOS / Linux: download zip → extract → copy over install dir */
    char tmp_zip[256];
    char tmp_dir[256];
    char exe_dir[512];
    ssize_t rlen = 0;
    char *last_slash = 0;

    snprintf(tmp_zip, sizeof(tmp_zip), "/tmp/his_update.zip");
    snprintf(tmp_dir, sizeof(tmp_dir), "/tmp/his_update");

    /* Detect current executable directory */
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

    fprintf(out, "\n");
    tui_print_info(out, "正在下载更新包...");
    fflush(out);

    snprintf(cmd, sizeof(cmd),
        "curl -L -s --progress-bar -o \"%s\" \"%s\" 2>&1",
        tmp_zip, info->asset_url);
    ret = system(cmd);
    if (ret != 0) {
        tui_print_error(out, "下载失败，请检查网络连接。");
        return 0;
    }
    tui_print_success(out, "下载完成");

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

    tui_print_info(out, "正在安装更新...");
    fflush(out);

    /* Copy extracted files to install directory */
    snprintf(cmd, sizeof(cmd),
        "for d in \"%s\"/*/; do cp -Rf \"$d\"* \"%s\" 2>/dev/null; done",
        tmp_dir, exe_dir);
    ret = system(cmd);

    /* Fix permissions on macOS */
#ifdef __APPLE__
    snprintf(cmd, sizeof(cmd),
        "xattr -cr \"%s\" 2>/dev/null; chmod +x \"%shis\" 2>/dev/null; "
        "chmod +x \"%shis_desktop\" 2>/dev/null",
        exe_dir, exe_dir, exe_dir);
    system(cmd);
#else
    snprintf(cmd, sizeof(cmd),
        "chmod +x \"%shis\" 2>/dev/null; chmod +x \"%shis_desktop\" 2>/dev/null",
        exe_dir, exe_dir);
    system(cmd);
#endif

    /* Cleanup */
    snprintf(cmd, sizeof(cmd),
        "rm -rf \"%s\" \"%s\"", tmp_dir, tmp_zip);
    system(cmd);

    tui_print_success(out, "更新安装完成！");
    tui_print_info(out, "请重新启动 HIS 以使用新版本。");

    return 1;
#endif
}

/* ── User prompt ─────────────────────────────────────────── */

int UpdateChecker_prompt(FILE *out, FILE *in, const UpdateInfo *info) {
    char answer[16];
    int has_direct_download = 0;

    if (out == 0 || in == 0 || info == 0 || info->has_update == 0) {
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
        int i = 0;
        for (i = used; i < 42; i++) fprintf(out, " ");
    }
    fprintf(out, TUI_BOLD_YELLOW "║\n" TUI_RESET);

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

    if (fgets(answer, sizeof(answer), in) == 0) {
        return 0;
    }

    if (has_direct_download && answer[0] == '1') {
        /* Direct download and install */
        if (do_download_and_install(out, info)) {
#ifdef _WIN32
            return 2; /* signal caller to exit for updater bat */
#else
            return 2; /* signal caller to exit and restart */
#endif
        }
        return 0;
    }

    if ((has_direct_download && answer[0] == '2') ||
        (!has_direct_download && answer[0] == '1')) {
        /* Open in browser */
#ifdef __APPLE__
        {
            char cmd[320];
            snprintf(cmd, sizeof(cmd), "open \"%s\" 2>/dev/null", info->download_url);
            system(cmd);
        }
#elif defined(_WIN32)
        {
            char cmd[320];
            snprintf(cmd, sizeof(cmd), "start \"\" \"%s\"", info->download_url);
            system(cmd);
        }
#else
        {
            char cmd[320];
            snprintf(cmd, sizeof(cmd), "xdg-open \"%s\" 2>/dev/null", info->download_url);
            system(cmd);
        }
#endif
        tui_print_success(out, "已在浏览器中打开下载页面");
        return 1;
    }

    tui_print_info(out, "已跳过更新，继续启动系统...");
    return 0;
}
