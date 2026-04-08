/**
 * @file main.c
 * @brief HIS 医院信息系统 - 程序入口
 *
 * 本文件是 HIS 系统的主入口，负责：
 * 1. 系统初始化（数据加载、Windows 控制台配置）
 * 2. 启动动画展示
 * 3. 主菜单循环（角色选择 -> 登录 -> 角色菜单 -> 操作执行）
 * 4. 用户输入处理（支持 ESC 返回、EOF 退出）
 */

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#endif

#include "common/InputHelper.h"
#include "common/UpdateChecker.h"
#include "ui/DemoData.h"
#include "ui/MenuApplication.h"
#include "ui/MenuController.h"
#include "ui/TuiStyle.h"

#ifdef _WIN32
/**
 * @brief 初始化 Windows 控制台
 *
 * 设置控制台代码页为 UTF-8，启用 ANSI 转义序列处理，
 * 确保中文字符和彩色输出在 Windows 终端中正确显示。
 */
static void init_windows_console(void) {
    HANDLE hOut = INVALID_HANDLE_VALUE;
    HANDLE hIn  = INVALID_HANDLE_VALUE;
    DWORD  mode = 0;

    /* Set console code page to UTF-8 for correct CJK display */
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    /* Enable ANSI escape sequence processing on stdout */
    hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE && GetConsoleMode(hOut, &mode)) {
        mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING | ENABLE_PROCESSED_OUTPUT;
        SetConsoleMode(hOut, mode);
    }

    /* Enable VT input on stdin */
    hIn = GetStdHandle(STD_INPUT_HANDLE);
    if (hIn != INVALID_HANDLE_VALUE && GetConsoleMode(hIn, &mode)) {
        mode |= ENABLE_VIRTUAL_TERMINAL_INPUT;
        SetConsoleMode(hIn, mode);
    }
}
#endif

/**
 * @brief 将菜单角色枚举转换为用户角色枚举
 * @param role 菜单角色
 * @return 对应的用户角色（无法映射时返回 USER_ROLE_UNKNOWN）
 */
static UserRole role_to_user_role(MenuRole role) {
    switch (role) {
        case MENU_ROLE_ADMIN:
            return USER_ROLE_ADMIN;
        case MENU_ROLE_DOCTOR:
            return USER_ROLE_DOCTOR;
        case MENU_ROLE_PATIENT:
            return USER_ROLE_PATIENT;
        case MENU_ROLE_INPATIENT_MANAGER:
            return USER_ROLE_INPATIENT_MANAGER;
        case MENU_ROLE_PHARMACY:
            return USER_ROLE_PHARMACY;
        default:
            return USER_ROLE_UNKNOWN;
    }
}

/**
 * @brief 将菜单角色枚举转换为 TUI 主题枚举
 * @param role 菜单角色
 * @return 对应的 TUI 角色主题
 */
static TuiRoleTheme role_to_theme(MenuRole role) {
    switch (role) {
        case MENU_ROLE_ADMIN:
            return TUI_THEME_ADMIN;
        case MENU_ROLE_DOCTOR:
            return TUI_THEME_DOCTOR;
        case MENU_ROLE_PATIENT:
            return TUI_THEME_PATIENT;
        case MENU_ROLE_INPATIENT_MANAGER:
            return TUI_THEME_INPATIENT;
        case MENU_ROLE_PHARMACY:
            return TUI_THEME_PHARMACY;
        default:
            return TUI_THEME_DEFAULT;
    }
}

/**
 * @brief 程序主入口
 *
 * 执行流程：
 * 1. 初始化控制台和应用程序
 * 2. 显示启动动画（矩阵雨 -> Logo -> 标题闪烁 -> Banner）
 * 3. 检查更新
 * 4. 进入主循环：角色选择 -> 登录验证 -> 角色菜单 -> 操作执行
 * 5. 用户可通过 ESC 逐级返回，输入 0 或 EOF 退出系统
 */
int main(void) {
    char menu_text[8192];   /* 菜单渲染缓冲区（双线渐变边框需要更多空间） */
    char input[128];        /* 用户输入缓冲区 */
    char password[128];     /* 密码输入缓冲区 */
    char prompt_buf[256];   /* 提示信息格式化缓冲区 */
    char user_id[128];      /* 登录用户编号缓存 */
    MenuApplication application;
    MenuApplicationPaths paths;

#ifdef _WIN32
    init_windows_console();
#endif

    /* 配置所有数据文件的路径 */
    paths = (MenuApplicationPaths){
        "data/users.txt",
        "data/patients.txt",
        "data/departments.txt",
        "data/doctors.txt",
        "data/registrations.txt",
        "data/visits.txt",
        "data/examinations.txt",
        "data/wards.txt",
        "data/beds.txt",
        "data/admissions.txt",
        "data/medicines.txt",
        "data/dispense_records.txt"
    };
    /* 初始化应用程序（加载所有业务服务） */
    Result init_result = MenuApplication_init(&application, &paths);

    if (init_result.success == 0) {
        fprintf(stderr, "系统初始化失败: %s\n", init_result.message);
        return 1;
    }

    /* 启动动画序列：光带扫描 -> 矩阵雨 -> Logo呼吸脉冲 -> Banner */
    tui_clear_screen();
    tui_animate_scanline(stdout, 48, 5);
    tui_animate_matrix_rain(stdout, 48, 8, 500);
    tui_animate_logo_pulse(stdout, 1);
    tui_animate_banner(stdout, "轻量级医院信息系统 (HIS)");

    /* ── 启动时检查更新 ── */
    {
        UpdateInfo update_info;
        tui_spinner_run(stdout, "正在检查更新...", 300);
        if (UpdateChecker_check(&update_info) && update_info.has_update) {
            int update_result = UpdateChecker_prompt(stdout, stdin, &update_info);
            if (update_result == 2) {
                /* 更新已安装，退出以便新版本启动 */
                tui_delay(1500);
                tui_show_cursor(stdout);
                return 0;
            }
            tui_delay(500);
        }
    }

    /* ═══════════════ 主循环：角色选择 -> 登录 -> 操作 ═══════════════ */
    for (;;) {
        MenuRole role = MENU_ROLE_INVALID;
        TuiRoleTheme theme = TUI_THEME_DEFAULT;
        /* 渲染主菜单（角色选择菜单）到缓冲区 */
        Result result = MenuController_render_main_menu(menu_text, sizeof(menu_text));

        if (result.success == 0) {
            fputs("主菜单渲染失败。\n", stderr);
            return 1;
        }

        tui_clear_screen();
        tui_animate_logo(stdout);
        tui_print_gradient_hline(stdout, 46);
        tui_print_section(stdout, TUI_MEDICAL, "\xe8\xaf\xb7\xe9\x80\x89\xe6\x8b\xa9\xe6\x82\xa8\xe7\x9a\x84\xe8\xa7\x92\xe8\x89\xb2" /* 请选择您的角色 */);
        tui_animate_lines(stdout, menu_text, 30);
        tui_print_prompt(stdout, "请选择角色编号 (ESC返回): ");
        result = Result_make_success("line ready");
        {
            int read_result = InputHelper_read_line(stdin, input, sizeof(input));
            if (read_result == 0 || read_result == -2) {
                tui_clear_screen();
                tui_animate_goodbye(stdout);
                tui_show_cursor(stdout);
                return 0;
            }
            if (read_result < 0) {
                tui_print_warning(stdout, "输入过长，请重新输入。");
                tui_delay(1000);
                continue;
            }
        }

        result = MenuController_parse_main_selection(input, &role);
        if (result.success == 0) {
            tui_print_warning(stdout, "输入无效，请重新输入菜单编号。");
            tui_delay(1000);
            continue;
        }

        if (MenuController_is_exit_role(role)) {
            tui_clear_screen();
            tui_animate_goodbye(stdout);
            tui_show_cursor(stdout);
            return 0;
        }

        /* 处理演示数据重置（特殊角色，不需要登录） */
        if (role == MENU_ROLE_RESET_DEMO) {
            char reset_message[RESULT_MESSAGE_CAPACITY];
            tui_print_section(stdout, TUI_SPARKLE, "重置演示数据");
            tui_spinner_run(stdout, "正在重置数据...", 600);
            result = DemoData_reset(&paths, reset_message, sizeof(reset_message));
            if (result.success == 0) {
                tui_animate_error(stdout, reset_message[0] != '\0' ? reset_message : result.message);
            } else {
                tui_animate_success(stdout, reset_message);
            }
            continue;
        }

        /* 获取角色对应的主题配色 */
        theme = role_to_theme(role);

        /* ── 登录流程 ── */
        {
            UserRole required_role = role_to_user_role(role);
            int login_ok = 0;  /* 登录是否成功标志 */

            if (required_role == USER_ROLE_UNKNOWN) {
                tui_print_warning(stdout, "角色未配置登录映射。");
                tui_delay(1000);
                continue;
            }

            /* 登录循环：用户编号处按 ESC 返回角色选择，
               密码处按 ESC 返回用户编号输入 */
            for (;;) {
                int read_result = 0;

                snprintf(prompt_buf, sizeof(prompt_buf),
                    "请输入[%s]用户编号 (ESC返回): ",
                    MenuController_role_label(role));
                tui_print_prompt(stdout, prompt_buf);
                read_result = InputHelper_read_line(stdin, input, sizeof(input));
                if (read_result == 0) {
                    tui_clear_screen();
                    tui_animate_goodbye(stdout);
                    tui_show_cursor(stdout);
                    return 0;
                }
                if (read_result == -2) {
                    break; /* back to role selection */
                }
                if (read_result < 0) {
                    tui_print_warning(stdout, "输入过长，请重新输入。");
                    tui_delay(800);
                    continue; /* retry user-ID */
                }

                tui_print_prompt(stdout, "请输入密码 (ESC返回): ");
                read_result = InputHelper_read_line(stdin, password, sizeof(password));
                if (read_result == 0) {
                    tui_clear_screen();
                    tui_animate_goodbye(stdout);
                    tui_show_cursor(stdout);
                    return 0;
                }
                if (read_result == -2) {
                    continue; /* back to user-ID prompt */
                }
                if (read_result < 0) {
                    tui_print_warning(stdout, "输入过长，请重新输入。");
                    tui_delay(800);
                    continue; /* retry from user-ID */
                }

                /* 尝试登录验证 */
                result = MenuApplication_login(&application, input, password, required_role);
                /* 安全清除密码：使用 volatile 防止编译器优化掉清零操作 */
                { volatile char *p = password; size_t n = sizeof(password); while (n--) *p++ = 0; }
                if (result.success == 0) {
                    tui_animate_error(stdout, result.message);
                    tui_delay(500);
                    continue; /* retry from user-ID */
                }

                login_ok = 1;
                break;
            }

            if (!login_ok) {
                continue; /* back to role selection */
            }
        }

        /* 保存已登录的用户编号 */
        strncpy(user_id, input, sizeof(user_id) - 1);
        user_id[sizeof(user_id) - 1] = '\0';

        /* 登录成功：光带扫描 -> 欢迎框 -> 心电图 */
        tui_clear_screen();
        tui_animate_scanline(stdout, 48, 4);
        tui_animate_welcome(stdout, theme, user_id);
        tui_animate_heartbeat(stdout, 44, 1);
        tui_print_info(stdout, "按回车键进入系统 (ESC返回)...");
        {
            int enter_result = InputHelper_read_line(stdin, input, sizeof(input));
            if (enter_result == -2) {
                MenuApplication_logout(&application);
                continue;
            }
            if (enter_result == 0) {
                tui_clear_screen();
                tui_animate_goodbye(stdout);
                tui_show_cursor(stdout);
                return 0;
            }
        }

        /* ── 角色菜单循环：显示操作列表，执行用户选择的操作 ── */
        for (;;) {
            MenuAction action = MENU_ACTION_INVALID;

            /* 渲染当前角色的操作菜单 */
            result = MenuController_render_role_menu(role, menu_text, sizeof(menu_text));
            if (result.success == 0) {
                fputs("角色菜单渲染失败。\n", stderr);
                return 1;
            }

            tui_clear_screen();
            tui_print_status_bar_themed(stdout, theme, user_id);
            tui_print_gradient_hline(stdout, 46);
            tui_animate_lines(stdout, menu_text, 25);
            tui_print_prompt_themed(stdout, "请选择操作 (ESC返回): ", theme);
            {
                int read_result = InputHelper_read_line(stdin, input, sizeof(input));
                if (read_result == 0) {
                    tui_clear_screen();
                    tui_animate_goodbye(stdout);
                    tui_show_cursor(stdout);
                    return 0;
                }
                if (read_result == -2) {
                    break;
                }
                if (read_result < 0) {
                    tui_print_warning(stdout, "输入过长，请重新输入。");
                    tui_delay(800);
                    continue;
                }
            }

            result = MenuController_parse_role_selection(role, input, &action);
            if (result.success == 0) {
                tui_print_warning(stdout, "输入无效，请重新输入菜单编号。");
                tui_delay(800);
                continue;
            }

            if (MenuController_is_back_action(action)) {
                break;
            }

            /* 执行选中的操作（包括交互式输入和业务逻辑处理） */
            result = MenuApplication_execute_action(&application, action, stdin, stdout);
            /* 仅在真正的错误时显示错误消息（跳过"未实现"和"ESC取消"） */
            if (result.success == 0 &&
                strcmp(result.message, "action not implemented yet") != 0 &&
                !InputHelper_is_esc_cancel(result.message)) {
                tui_print_error(stdout, result.message);
            }

            tui_delay(300);
            printf("\n");
            tui_print_info(stdout, "按回车键继续 (ESC返回上级)...");
            {
                int cont_result = InputHelper_read_line(stdin, input, sizeof(input));
                if (cont_result == -2) {
                    break;
                }
                if (cont_result == 0) {
                    tui_clear_screen();
                    tui_animate_goodbye(stdout);
                    tui_show_cursor(stdout);
                    return 0;
                }
            }
        }

        /* 退出角色菜单循环后，执行登出操作 */
        MenuApplication_logout(&application);
    }
}
