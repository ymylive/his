#include <stdio.h>
#include <string.h>

#include "ui/DemoData.h"
#include "ui/MenuApplication.h"
#include "ui/MenuController.h"

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

    puts("轻量级 HIS 控制台已启动。");

    for (;;) {
        MenuRole role = MENU_ROLE_INVALID;
        Result result = MenuController_render_main_menu(menu_text, sizeof(menu_text));

        if (result.success == 0) {
            fputs("主菜单渲染失败。\n", stderr);
            return 1;
        }

        puts(menu_text);
        printf("请选择角色编号: ");
        result = Result_make_success("line ready");
        {
            int read_result = read_line(input, sizeof(input));
            if (read_result == 0) {
                puts("输入流结束，系统退出。");
                return 0;
            }
            if (read_result < 0) {
                puts("输入过长，请重新输入。");
                continue;
            }
        }

        result = MenuController_parse_main_selection(input, &role);
        if (result.success == 0) {
            puts("输入无效，请重新输入菜单编号。");
            continue;
        }

        if (MenuController_is_exit_role(role)) {
            puts("系统已退出。");
            return 0;
        }

        if (role == MENU_ROLE_RESET_DEMO) {
            char reset_message[RESULT_MESSAGE_CAPACITY];
            result = DemoData_reset(&paths, reset_message, sizeof(reset_message));
            if (result.success == 0) {
                printf("重置失败: %s\n", reset_message[0] != '\0' ? reset_message : result.message);
            } else {
                printf("%s\n", reset_message);
            }
            continue;
        }

        {
            UserRole required_role = role_to_user_role(role);
            if (required_role == USER_ROLE_UNKNOWN) {
                puts("角色未配置登录映射。");
                continue;
            }

            printf("请输入[%s]用户编号: ", MenuController_role_label(role));
            {
                int read_result = read_line(input, sizeof(input));
                if (read_result == 0) {
                    puts("输入流结束，系统退出。");
                    return 0;
                }
                if (read_result < 0) {
                    puts("输入过长，请重新输入。");
                    continue;
                }
            }

            printf("请输入密码: ");
            {
                int read_result = read_line(password, sizeof(password));
                if (read_result == 0) {
                    puts("输入流结束，系统退出。");
                    return 0;
                }
                if (read_result < 0) {
                    puts("输入过长，请重新输入。");
                    continue;
                }
            }

            result = MenuApplication_login(&application, input, password, required_role);
            memset(password, 0, sizeof(password));
            if (result.success == 0) {
                printf("登录失败: %s\n", result.message);
                continue;
            }
        }

        for (;;) {
            MenuAction action = MENU_ACTION_INVALID;

            result = MenuController_render_role_menu(role, menu_text, sizeof(menu_text));
            if (result.success == 0) {
                fputs("角色菜单渲染失败。\n", stderr);
                return 1;
            }

            puts(menu_text);
            printf("[%s] 请选择操作编号: ", MenuController_role_label(role));
            {
                int read_result = read_line(input, sizeof(input));
                if (read_result == 0) {
                    puts("输入流结束，系统退出。");
                    return 0;
                }
                if (read_result < 0) {
                    puts("输入过长，请重新输入。");
                    continue;
                }
            }

            result = MenuController_parse_role_selection(role, input, &action);
            if (result.success == 0) {
                puts("输入无效，请重新选择。");
                continue;
            }

            if (MenuController_is_back_action(action)) {
                break;
            }

            result = MenuApplication_execute_action(&application, action, stdin, stdout);
            if (result.success == 0 &&
                strcmp(result.message, "action not implemented yet") != 0) {
                printf("操作未完成: %s\n", result.message);
            }
        }

        MenuApplication_logout(&application);
    }
}
