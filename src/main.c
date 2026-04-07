#include <stdio.h>
#include <string.h>

#include "ui/DemoData.h"
#include "ui/MenuApplication.h"
#include "ui/MenuController.h"
#include "ui/TuiStyle.h"

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

static int read_line(char *buffer, size_t capacity) {
    size_t length = 0;

    if (buffer == 0 || capacity == 0) {
        return 0;
    }

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
    MenuApplicationPaths paths = {
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
        tui_print_prompt(stdout, "请选择角色编号: ");
        result = Result_make_success("line ready");
        {
            int read_result = read_line(input, sizeof(input));
            if (read_result == 0) {
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

            snprintf(prompt_buf, sizeof(prompt_buf), "请输入[%s]用户编号: ", MenuController_role_label(role));
            tui_print_prompt(stdout, prompt_buf);
            {
                int read_result = read_line(input, sizeof(input));
                if (read_result == 0) {
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

            tui_print_prompt(stdout, "请输入密码: ");
            {
                int read_result = read_line(password, sizeof(password));
                if (read_result == 0) {
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
            tui_print_prompt(stdout, "请选择操作: ");
            {
                int read_result = read_line(input, sizeof(input));
                if (read_result == 0) {
                    tui_clear_screen();
                    tui_animate_goodbye(stdout);
                    tui_show_cursor(stdout);
                    return 0;
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
            tui_print_info(stdout, "按回车键继续...");
            read_line(input, sizeof(input));
        }

        MenuApplication_logout(&application);
    }
}
