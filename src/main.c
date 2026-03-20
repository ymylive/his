#include <stdio.h>
#include <string.h>

#include "ui/MenuController.h"

static int read_line(char *buffer, size_t capacity) {
    size_t length = 0;

    if (buffer == 0 || capacity == 0) {
        return 0;
    }

    if (fgets(buffer, (int)capacity, stdin) == 0) {
        return 0;
    }

    length = strlen(buffer);
    if (length > 0 && buffer[length - 1] == '\n') {
        buffer[length - 1] = '\0';
    }

    return 1;
}

int main(void) {
    char menu_text[2048];
    char feedback[256];
    char input[128];

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
        if (!read_line(input, sizeof(input))) {
            puts("输入流结束，系统退出。");
            return 0;
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

        for (;;) {
            MenuAction action = MENU_ACTION_INVALID;

            result = MenuController_render_role_menu(role, menu_text, sizeof(menu_text));
            if (result.success == 0) {
                fputs("角色菜单渲染失败。\n", stderr);
                return 1;
            }

            puts(menu_text);
            printf("[%s] 请选择操作编号: ", MenuController_role_label(role));
            if (!read_line(input, sizeof(input))) {
                puts("输入流结束，系统退出。");
                return 0;
            }

            result = MenuController_parse_role_selection(role, input, &action);
            if (result.success == 0) {
                puts("输入无效，请重新选择。");
                continue;
            }

            if (MenuController_is_back_action(action)) {
                break;
            }

            result = MenuController_format_action_feedback(
                action,
                feedback,
                sizeof(feedback)
            );
            if (result.success == 0) {
                fputs("操作提示生成失败。\n", stderr);
                return 1;
            }

            puts(feedback);
        }
    }
}
