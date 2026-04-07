#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <conio.h>
#include <windows.h>
#else
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>
#endif

#include "common/UpdateChecker.h"
#include "ui/DemoData.h"
#include "ui/MenuApplication.h"
#include "ui/MenuController.h"
#include "ui/TuiStyle.h"

#ifdef _WIN32
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

static void discard_rest_of_line(FILE *input) {
    int character = 0;

    while ((character = fgetc(input)) != EOF) {
        if (character == '\n') {
            return;
        }
    }
}

static int peek_escape_key(void) {
#ifdef _WIN32
    if (_kbhit()) {
        int ch = _getch();
        if (ch == 27) return 1;
        _ungetch(ch);
    }
    return 0;
#else
    struct termios old_settings;
    struct termios new_settings;
    int ch = 0;
    fd_set fds;
    struct timeval tv;

    if (!isatty(STDIN_FILENO)) {
        return 0;
    }

    tcgetattr(STDIN_FILENO, &old_settings);
    new_settings = old_settings;
    new_settings.c_lflag &= ~(ICANON | ECHO);
    new_settings.c_cc[VMIN] = 0;
    new_settings.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &new_settings);

    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    tv.tv_sec = 0;
    tv.tv_usec = 0;

    if (select(STDIN_FILENO + 1, &fds, 0, 0, &tv) > 0) {
        ch = getchar();
        if (ch == 27) {
            /* consume any trailing escape sequence bytes */
            tv.tv_sec = 0;
            tv.tv_usec = 50000;
            while (select(STDIN_FILENO + 1, &fds, 0, 0, &tv) > 0) {
                getchar();
                FD_ZERO(&fds);
                FD_SET(STDIN_FILENO, &fds);
                tv.tv_sec = 0;
                tv.tv_usec = 10000;
            }
            tcsetattr(STDIN_FILENO, TCSANOW, &old_settings);
            return 1;
        }
        if (ch != EOF) {
            ungetc(ch, stdin);
        }
    }

    tcsetattr(STDIN_FILENO, TCSANOW, &old_settings);
    return 0;
#endif
}

static int read_line(char *buffer, size_t capacity) {
    size_t length = 0;
#ifndef _WIN32
    struct termios old_settings;
    struct termios new_settings;
    int ch = 0;
#endif

    if (buffer == 0 || capacity == 0) {
        return 0;
    }

#ifdef _WIN32
    {
        int ch = _getch();
        if (ch == 27) {
            buffer[0] = '\0';
            return -2;
        }
        _ungetch(ch);
    }
#else
    if (isatty(STDIN_FILENO)) {
        tcgetattr(STDIN_FILENO, &old_settings);
        new_settings = old_settings;
        new_settings.c_lflag &= ~(ICANON | ECHO);
        new_settings.c_cc[VMIN] = 1;
        new_settings.c_cc[VTIME] = 0;
        tcsetattr(STDIN_FILENO, TCSANOW, &new_settings);

        ch = getchar();
        tcsetattr(STDIN_FILENO, TCSANOW, &old_settings);

        if (ch == 27) {
            /* drain any escape sequence bytes */
            struct timeval tv;
            fd_set fds;
            new_settings = old_settings;
            new_settings.c_lflag &= ~(ICANON | ECHO);
            new_settings.c_cc[VMIN] = 0;
            new_settings.c_cc[VTIME] = 0;
            tcsetattr(STDIN_FILENO, TCSANOW, &new_settings);
            tv.tv_sec = 0;
            tv.tv_usec = 50000;
            FD_ZERO(&fds);
            FD_SET(STDIN_FILENO, &fds);
            while (select(STDIN_FILENO + 1, &fds, 0, 0, &tv) > 0) {
                getchar();
                FD_ZERO(&fds);
                FD_SET(STDIN_FILENO, &fds);
                tv.tv_sec = 0;
                tv.tv_usec = 10000;
            }
            tcsetattr(STDIN_FILENO, TCSANOW, &old_settings);
            buffer[0] = '\0';
            return -2;
        }
        if (ch == EOF) {
            buffer[0] = '\0';
            return 0;
        }
        ungetc(ch, stdin);
    }
#endif

    if (fgets(buffer, (int)capacity, stdin) == 0) {
        return 0;
    }

    length = strlen(buffer);
    if (length > 0 && buffer[length - 1] != '\n' && !feof(stdin)) {
        discard_rest_of_line(stdin);
        buffer[0] = '\0';
        return -1;
    }

    if (length > 0 && buffer[length - 1] == '\n') {
        buffer[length - 1] = '\0';
    }

    return 1;
}

int main(void) {
    char menu_text[2048];
    char input[128];
    char password[128];
    char prompt_buf[256];
    char user_id[128];
    MenuApplication application;
    MenuApplicationPaths paths;

#ifdef _WIN32
    init_windows_console();
#endif

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
    Result init_result = MenuApplication_init(&application, &paths);

    if (init_result.success == 0) {
        fprintf(stderr, "系统初始化失败: %s\n", init_result.message);
        return 1;
    }

    tui_clear_screen();
    tui_animate_logo(stdout);
    tui_animate_banner(stdout, "轻量级医院信息系统 (HIS)");

    /* ── startup update check ── */
    {
        UpdateInfo update_info;
        tui_spinner_run(stdout, "正在检查更新...", 300);
        if (UpdateChecker_check(&update_info) && update_info.has_update) {
            int update_result = UpdateChecker_prompt(stdout, stdin, &update_info);
            if (update_result == 2) {
                /* Update installed — exit so new version can start */
                tui_delay(1500);
                tui_show_cursor(stdout);
                return 0;
            }
            tui_delay(500);
        }
    }

    for (;;) {
        MenuRole role = MENU_ROLE_INVALID;
        TuiRoleTheme theme = TUI_THEME_DEFAULT;
        Result result = MenuController_render_main_menu(menu_text, sizeof(menu_text));

        if (result.success == 0) {
            fputs("主菜单渲染失败。\n", stderr);
            return 1;
        }

        tui_clear_screen();
        tui_animate_logo(stdout);
        tui_animate_lines(stdout, menu_text, 30);
        tui_print_prompt(stdout, "请选择角色编号 (ESC返回): ");
        result = Result_make_success("line ready");
        {
            int read_result = read_line(input, sizeof(input));
            if (read_result == 0 || read_result == -2) {
                tui_clear_screen();
                tui_animate_goodbye(stdout);
                tui_show_cursor(stdout);
                return 0;
            }
            if (read_result < 0) {
                tui_print_warning(stdout, "输入过长，请重新输入。");
                tui_delay(800);
                tui_delay(500);
                continue;
            }
        }

        result = MenuController_parse_main_selection(input, &role);
        if (result.success == 0) {
            tui_print_warning(stdout, "输入无效，请重新输入菜单编号。");
            tui_delay(800);
            tui_delay(500);
            continue;
        }

        if (MenuController_is_exit_role(role)) {
            tui_clear_screen();
            tui_animate_goodbye(stdout);
            tui_show_cursor(stdout);
            return 0;
        }

        if (role == MENU_ROLE_RESET_DEMO) {
            char reset_message[RESULT_MESSAGE_CAPACITY];
            tui_print_section(stdout, TUI_SPARKLE, "重置演示数据");
            tui_spinner_run(stdout, "正在重置数据...", 800);
            result = DemoData_reset(&paths, reset_message, sizeof(reset_message));
            if (result.success == 0) {
                tui_animate_error(stdout, reset_message[0] != '\0' ? reset_message : result.message);
            } else {
                tui_animate_success(stdout, reset_message);
            }
            continue;
        }

        theme = role_to_theme(role);

        {
            UserRole required_role = role_to_user_role(role);
            if (required_role == USER_ROLE_UNKNOWN) {
                tui_print_warning(stdout, "角色未配置登录映射。");
                tui_delay(800);
                tui_delay(500);
                continue;
            }

            snprintf(prompt_buf, sizeof(prompt_buf), "请输入[%s]用户编号 (ESC返回): ", MenuController_role_label(role));
            tui_print_prompt(stdout, prompt_buf);
            {
                int read_result = read_line(input, sizeof(input));
                if (read_result == 0) {
                    tui_clear_screen();
                    tui_animate_goodbye(stdout);
                    tui_show_cursor(stdout);
                    return 0;
                }
                if (read_result == -2) {
                    continue;
                }
                if (read_result < 0) {
                    tui_print_warning(stdout, "输入过长，请重新输入。");
                    tui_delay(800);
                    tui_delay(500);
                    continue;
                }
            }

            tui_print_prompt(stdout, "请输入密码 (ESC返回): ");
            {
                int read_result = read_line(password, sizeof(password));
                if (read_result == 0) {
                    tui_clear_screen();
                    tui_animate_goodbye(stdout);
                    tui_show_cursor(stdout);
                    return 0;
                }
                if (read_result == -2) {
                    continue;
                }
                if (read_result < 0) {
                    tui_print_warning(stdout, "输入过长，请重新输入。");
                    tui_delay(800);
                    tui_delay(500);
                    continue;
                }
            }

            result = MenuApplication_login(&application, input, password, required_role);
            memset(password, 0, sizeof(password));
            if (result.success == 0) {
                tui_animate_error(stdout, result.message);
                tui_delay(500);
                continue;
            }
        }

        strncpy(user_id, input, sizeof(user_id) - 1);
        user_id[sizeof(user_id) - 1] = '\0';

        tui_clear_screen();
        tui_animate_welcome(stdout, theme, user_id);
        tui_print_info(stdout, "按回车键进入系统...");
        read_line(input, sizeof(input));
        tui_animate_transition(stdout);

        for (;;) {
            MenuAction action = MENU_ACTION_INVALID;

            result = MenuController_render_role_menu(role, menu_text, sizeof(menu_text));
            if (result.success == 0) {
                fputs("角色菜单渲染失败。\n", stderr);
                return 1;
            }

            tui_clear_screen();
            tui_print_status_bar_themed(stdout, theme, user_id);
            tui_animate_lines(stdout, menu_text, 25);
            tui_print_prompt(stdout, "请选择操作 (ESC返回): ");
            {
                int read_result = read_line(input, sizeof(input));
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

            result = MenuApplication_execute_action(&application, action, stdin, stdout);
            if (result.success == 0 &&
                strcmp(result.message, "action not implemented yet") != 0) {
                tui_print_error(stdout, result.message);
            }

            tui_delay(300);
            printf("\n");
            tui_print_info(stdout, "按回车键继续 (ESC返回上级)...");
            read_line(input, sizeof(input));
        }

        MenuApplication_logout(&application);
    }
}
