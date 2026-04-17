/**
 * @file MenuApplication.c
 * @brief 菜单应用程序实现 - HIS 系统核心业务协调层
 *
 * 本文件实现了 HIS 系统应用层的全部功能，包括：
 * 1. 系统初始化：验证路径、初始化所有业务服务
 * 2. 用户认证：登录/登出、患者会话绑定
 * 3. 患者管理：增删改查
 * 4. 科室与医生管理：增加/更新/查询/按科室列出
 * 5. 挂号管理：创建/取消/查询挂号记录
 * 6. 诊疗记录：创建就诊记录、检查记录、历史查询
 * 7. 住院管理：入院/出院/转床/出院检查
 * 8. 药房管理：添加药品/入库/发药/库存查询/低库存预警
 * 9. 交互式表单：患者/医生/科室/药品信息采集
 * 10. 模糊搜索选择器：支持关键字过滤和编号直选
 * 11. 操作分派：将菜单操作映射到具体业务处理流程
 */

#include "ui/MenuApplication.h"
#include "ui/MenuActionHandlers.h"

#include <ctype.h>
#include "ui/TuiStyle.h"
#include <limits.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <io.h>
#define MenuApp_isatty(fd) _isatty(fd)
#else
#include <unistd.h>
#define MenuApp_isatty(fd) isatty(fd)
#endif

#include "common/IdGenerator.h"
#include "common/InputHelper.h"
#include "service/DepartmentService.h"

/**
 * @brief 将用户角色枚举映射为审计日志中的角色标签
 */
static const char *MenuApplication_audit_role_label(UserRole role) {
    switch (role) {
        case USER_ROLE_ADMIN:             return "ADMIN";
        case USER_ROLE_DOCTOR:            return "DOCTOR";
        case USER_ROLE_PATIENT:           return "PATIENT";
        case USER_ROLE_INPATIENT_MANAGER: return "INPATIENT_MANAGER";
        case USER_ROLE_PHARMACY:          return "PHARMACY";
        default:                          return "UNKNOWN";
    }
}

/**
 * @brief 获取当前已认证用户ID；未登录时返回 NULL
 */
static const char *MenuApplication_audit_actor_id(const MenuApplication *application) {
    if (application == 0 || application->has_authenticated_user == 0) {
        return 0;
    }
    return application->authenticated_user.user_id;
}

/**
 * @brief 获取当前已认证用户角色标签；未登录时返回 NULL
 */
static const char *MenuApplication_audit_actor_role(const MenuApplication *application) {
    if (application == 0 || application->has_authenticated_user == 0) {
        return 0;
    }
    return MenuApplication_audit_role_label(application->authenticated_user.role);
}

/* ANSI cursor control sequences for interactive UI */
#define TUI_CURSOR_UP_FMT    "\033[%dA\r"   /* Move cursor up N lines + carriage return */
#define TUI_CURSOR_UP_ONLY   "\033[%dA"     /* Move cursor up N lines */
#define TUI_CLEAR_LINE       "\033[K"        /* Clear to end of line */
#define TUI_CLEAR_LINE_NL    "\033[K\n"      /* Clear to end of line + newline */

/* ═══════════════════════════════════════════════════════════════
 *  Form Panel System - interactive multi-field form display
 *  Types are defined in MenuActionHandlers.h
 * ═══════════════════════════════════════════════════════════════ */

/**
 * @brief Render the form panel to output.
 *
 * Draws the box border, title, all fields with status indicators.
 * For the current field, the cursor is left at the input position
 * (no trailing newline). Returns the total number of complete lines
 * rendered (excluding the partial current-field line).
 */
/**
 * @brief 表单渲染：绘制完整表单面板（导航模式，无输入光标）
 *
 * 显示所有字段的当前状态，选中字段用 ▶ 标识。
 * 所有行都以换行结尾，返回总行数。
 *
 * @param output        输出流
 * @param panel         表单面板
 * @param editing_field 当前正在编辑的字段索引（-1=非编辑状态）
 * @return 渲染的总行数
 */
static int MenuApplication_form_render_full(
    FILE *output,
    const FormPanel *panel,
    int editing_field
) {
    int i;
    int lines = 0;
    int box_width = 50;

    /* Top border */
    fprintf(output, "  " TUI_OC_DIM);
    for (i = 0; i < box_width; i++) fprintf(output, TUI_DH);
    fprintf(output, TUI_RESET TUI_CLEAR_LINE_NL);
    lines++;

    /* Title */
    fprintf(output, "  " TUI_BOLD "  %s" TUI_RESET TUI_CLEAR_LINE_NL, panel->title);
    lines++;

    /* Separator */
    fprintf(output, "  " TUI_OC_DIM);
    for (i = 0; i < box_width; i++) fprintf(output, TUI_H);
    fprintf(output, TUI_RESET TUI_CLEAR_LINE_NL);
    lines++;

    /* Fields */
    for (i = 0; i < panel->field_count; i++) {
        const FormField *f = &panel->fields[i];
        int is_selected = (i == panel->current_field);
        const char *arrow = is_selected
            ? TUI_BOLD_CYAN TUI_ARROW " " TUI_RESET
            : "  ";

        if (i == editing_field) {
            /* 编辑模式：显示标签后留空等待输入，不加换行 */
            if (f->hint[0] != '\0') {
                fprintf(output, "  %s" TUI_DIM "%-12s" TUI_RESET " " TUI_OC_MUTED "(%s)" TUI_RESET " ",
                        arrow, f->label, f->hint);
            } else {
                fprintf(output, "  %s" TUI_DIM "%-12s" TUI_RESET " ", arrow, f->label);
            }
            /* 不加换行 — 光标停在此处等待用户输入 */
        } else if (f->is_filled) {
            fprintf(output, "  %s" TUI_DIM "%-12s" TUI_RESET " " TUI_BOLD_GREEN "%s" TUI_RESET TUI_CLEAR_LINE_NL,
                    arrow, f->label, f->value);
            lines++;
        } else {
            /* 未填写 */
            if (f->is_required) {
                fprintf(output, "  %s" TUI_DIM "%-12s" TUI_RESET " " TUI_OC_MUTED "(\xe5\xbe\x85\xe5\xa1\xab\xe5\x86\x99)" TUI_RESET TUI_CLEAR_LINE_NL,
                        arrow, f->label);
            } else if (f->default_value[0] != '\0') {
                fprintf(output, "  %s" TUI_DIM "%-12s" TUI_RESET " " TUI_OC_DIM "(\xe5\x8f\xaf\xe7\x95\x99\xe7\xa9\xba, \xe9\xbb\x98\xe8\xae\xa4: %s)" TUI_RESET TUI_CLEAR_LINE_NL,
                        arrow, f->label, f->default_value);
            } else {
                fprintf(output, "  %s" TUI_DIM "%-12s" TUI_RESET " " TUI_OC_DIM "(\xe5\x8f\xaf\xe7\x95\x99\xe7\xa9\xba)" TUI_RESET TUI_CLEAR_LINE_NL,
                        arrow, f->label);
            }
            lines++;
        }
    }

    /* 确认提交按钮行 */
    if (editing_field < 0) {
        int is_submit = (panel->current_field == panel->field_count);
        const char *submit_arrow = is_submit
            ? TUI_BOLD_CYAN TUI_ARROW " " TUI_RESET
            : "  ";
        fprintf(output, "  " TUI_OC_DIM);
        for (i = 0; i < box_width; i++) fprintf(output, TUI_H);
        fprintf(output, TUI_RESET TUI_CLEAR_LINE_NL);
        lines++;

        fprintf(output, "  %s" TUI_BOLD_GREEN "[ \xe2\x9c\x93 \xe7\xa1\xae\xe8\xae\xa4\xe6\x8f\x90\xe4\xba\xa4 ]" TUI_RESET TUI_CLEAR_LINE_NL,
                submit_arrow);
        lines++;
    }

    /* Bottom border */
    fprintf(output, "  " TUI_OC_DIM);
    for (i = 0; i < box_width; i++) fprintf(output, TUI_H);
    fprintf(output, TUI_RESET TUI_CLEAR_LINE_NL);
    lines++;

    /* Help text */
    if (editing_field >= 0) {
        fprintf(output, "  " TUI_OC_DIM "\xe8\xbe\x93\xe5\x85\xa5\xe5\x86\x85\xe5\xae\xb9\xe5\x90\x8e\xe5\x9b\x9e\xe8\xbd\xa6\xe7\xa1\xae\xe8\xae\xa4 | \xe7\x9b\xb4\xe6\x8e\xa5\xe5\x9b\x9e\xe8\xbd\xa6\xe4\xbd\xbf\xe7\x94\xa8\xe9\xbb\x98\xe8\xae\xa4\xe5\x80\xbc | ESC\xe5\x8f\x96\xe6\xb6\x88" TUI_RESET TUI_CLEAR_LINE_NL);
    } else {
        fprintf(output, "  " TUI_OC_DIM "\xe2\x86\x91\xe2\x86\x93\xe9\x80\x89\xe6\x8b\xa9\xe5\xad\x97\xe6\xae\xb5 | Enter\xe7\xbc\x96\xe8\xbe\x91/\xe6\x8f\x90\xe4\xba\xa4 | ESC\xe5\x8f\x96\xe6\xb6\x88" TUI_RESET TUI_CLEAR_LINE_NL);
    }
    lines++;

    return lines;
}

/**
 * @brief 表单交互式运行（TTY模式）
 *
 * 交互模型：
 * 1. 导航模式：↑↓方向键选择字段，Enter进入编辑，ESC取消表单
 * 2. 编辑模式：输入文本，Enter确认并返回导航模式
 * 3. 移动到底部"确认提交"并按Enter提交表单
 */
static Result MenuApplication_form_run(
    MenuApplicationPromptContext *context,
    FormPanel *panel
) {
    int total_items;  /* field_count + 1 (submit button) */
    int prev_lines = 0;

    total_items = panel->field_count + 1;
    panel->current_field = 0;

    for (;;) {
        InputEvent event;

        /* 回退光标并重绘 */
        if (prev_lines > 0) {
            fprintf(context->output, TUI_CURSOR_UP_FMT, prev_lines);
        }
        prev_lines = MenuApplication_form_render_full(context->output, panel, -1);
        fflush(context->output);

        /* 导航模式：读取按键 */
        event = InputHelper_read_key(context->input);

        switch (event.key) {
            case INPUT_KEY_UP:
                if (panel->current_field > 0) {
                    panel->current_field--;
                }
                break;

            case INPUT_KEY_DOWN:
                if (panel->current_field < total_items - 1) {
                    panel->current_field++;
                }
                break;

            case INPUT_KEY_ESC:
                /* 清除表单显示 */
                fprintf(context->output, "\n");
                fflush(context->output);
                return Result_make_failure(INPUT_HELPER_ESC_MESSAGE);

            case INPUT_KEY_ENTER:
                if (panel->current_field == panel->field_count) {
                    /* 确认提交：检查必填字段 */
                    int i;
                    int missing = 0;
                    for (i = 0; i < panel->field_count; i++) {
                        if (panel->fields[i].is_required && !panel->fields[i].is_filled) {
                            /* 跳转到第一个未填必填字段 */
                            panel->current_field = i;
                            missing = 1;
                            break;
                        }
                    }
                    if (missing) {
                        break; /* 回到循环重绘，高亮缺失字段 */
                    }
                    /* 所有必填字段已填，对未填可选字段填入默认值 */
                    for (i = 0; i < panel->field_count; i++) {
                        if (!panel->fields[i].is_filled) {
                            if (panel->fields[i].default_value[0] != '\0') {
                                strncpy(panel->fields[i].value, panel->fields[i].default_value,
                                        sizeof(panel->fields[i].value) - 1);
                            } else {
                                strncpy(panel->fields[i].value, "\xe6\x97\xa0",
                                        sizeof(panel->fields[i].value) - 1);
                            }
                            panel->fields[i].value[sizeof(panel->fields[i].value) - 1] = '\0';
                            panel->fields[i].is_filled = 1;
                        }
                    }
                    /* 最终渲染 */
                    if (prev_lines > 0) {
                        fprintf(context->output, TUI_CURSOR_UP_FMT, prev_lines);
                    }
                    panel->current_field = -1;
                    prev_lines = MenuApplication_form_render_full(context->output, panel, -1);
                    fprintf(context->output, "\n");
                    fflush(context->output);
                    return Result_make_success("form completed");
                } else {
                    /* 进入编辑模式 */
                    FormField *field = &panel->fields[panel->current_field];
                    char input_buffer[FORM_VALUE_CAPACITY];
                    int read_result;

                    /* 重绘表单，当前字段显示为编辑状态 */
                    if (prev_lines > 0) {
                        fprintf(context->output, TUI_CURSOR_UP_FMT, prev_lines);
                    }
                    prev_lines = MenuApplication_form_render_full(
                        context->output, panel, panel->current_field);
                    fflush(context->output);

                    /* 读取用户输入 */
                    memset(input_buffer, 0, sizeof(input_buffer));
                    read_result = InputHelper_read_line(
                        context->input, input_buffer, sizeof(input_buffer));

                    if (read_result == 0) {
                        return Result_make_failure("input ended");
                    }
                    if (read_result == -2) {
                        /* ESC在编辑模式 = 取消本次编辑，回到导航 */
                        prev_lines++; /* 编辑行的换行 */
                        break;
                    }

                    /* 编辑行产生了一个换行，计入行数 */
                    prev_lines++;

                    /* Trim */
                    {
                        char *start = input_buffer;
                        char *end;
                        while (*start && isspace((unsigned char)*start)) start++;
                        if (start != input_buffer && *start) {
                            memmove(input_buffer, start, strlen(start) + 1);
                        } else if (start != input_buffer) {
                            input_buffer[0] = '\0';
                        }
                        end = input_buffer + strlen(input_buffer);
                        while (end > input_buffer && isspace((unsigned char)*(end - 1))) end--;
                        *end = '\0';
                    }

                    /* 处理输入 */
                    if (input_buffer[0] == '\0') {
                        if (field->default_value[0] != '\0') {
                            strncpy(field->value, field->default_value,
                                    sizeof(field->value) - 1);
                            field->value[sizeof(field->value) - 1] = '\0';
                            field->is_filled = 1;
                        } else if (!field->is_required) {
                            strncpy(field->value, "\xe6\x97\xa0",
                                    sizeof(field->value) - 1);
                            field->value[sizeof(field->value) - 1] = '\0';
                            field->is_filled = 1;
                        }
                        /* 必填字段留空：不标记已填，用户可重新编辑 */
                    } else {
                        strncpy(field->value, input_buffer,
                                sizeof(field->value) - 1);
                        field->value[sizeof(field->value) - 1] = '\0';
                        field->is_filled = 1;
                    }

                    /* 自动移动到下一个字段 */
                    if (panel->current_field < total_items - 1) {
                        panel->current_field++;
                    }
                }
                break;

            default:
                break;
        }
    }
}

/**
 * @brief Run the form as simple sequential prompts (non-TTY fallback).
 *
 * This preserves the original line-by-line behavior for piped input
 * (tests, scripts), without any cursor control or fancy rendering.
 */
static Result MenuApplication_form_run_simple(
    MenuApplicationPromptContext *context,
    FormPanel *panel
) {
    int i;
    char input_buffer[FORM_VALUE_CAPACITY];

    for (i = 0; i < panel->field_count; i++) {
        FormField *field = &panel->fields[i];
        int read_result;

        /* Print prompt */
        if (field->hint[0] != '\0') {
            fprintf(context->output, "%s(%s): ", field->label, field->hint);
        } else {
            fprintf(context->output, "%s ", field->label);
        }
        fflush(context->output);

        /* Read input */
        memset(input_buffer, 0, sizeof(input_buffer));
        read_result = InputHelper_read_line(context->input, input_buffer, sizeof(input_buffer));

        if (read_result == 0) {
            return Result_make_failure("input ended");
        }
        if (read_result == -2) {
            return Result_make_failure(INPUT_HELPER_ESC_MESSAGE);
        }

        /* Trim input */
        {
            char *start = input_buffer;
            char *end;
            while (*start != '\0' && isspace((unsigned char)*start)) start++;
            if (start != input_buffer && start[0] != '\0') {
                memmove(input_buffer, start, strlen(start) + 1);
            } else if (start != input_buffer) {
                input_buffer[0] = '\0';
            }
            end = input_buffer + strlen(input_buffer);
            while (end > input_buffer && isspace((unsigned char)*(end - 1))) end--;
            *end = '\0';
        }

        /* Process input */
        if (input_buffer[0] == '\0') {
            if (field->default_value[0] != '\0') {
                strncpy(field->value, field->default_value, sizeof(field->value) - 1);
                field->value[sizeof(field->value) - 1] = '\0';
                field->is_filled = 1;
            } else if (field->is_required) {
                i--; /* retry this field */
                continue;
            } else {
                strncpy(field->value, "\xe6\x97\xa0", sizeof(field->value) - 1);
                field->value[sizeof(field->value) - 1] = '\0';
                field->is_filled = 1;
            }
        } else {
            strncpy(field->value, input_buffer, sizeof(field->value) - 1);
            field->value[sizeof(field->value) - 1] = '\0';
            field->is_filled = 1;
        }
    }

    return Result_make_success("form completed");
}

/**
 * @brief Run a form panel, auto-detecting TTY vs pipe input.
 *
 * In TTY mode, displays the full interactive form panel with
 * ANSI cursor control. In non-TTY mode (pipes, tests), falls
 * back to simple sequential prompts.
 */
Result MenuApplication_form_dispatch(
    MenuApplicationPromptContext *context,
    FormPanel *panel
) {
    int input_fd = fileno(context->input);
    if (input_fd >= 0 && MenuApp_isatty(input_fd)) {
        return MenuApplication_form_run(context, panel);
    }
    return MenuApplication_form_run_simple(context, panel);
}

/**
 * @brief 检查字符串是否为空白（NULL、空字符串或仅含空白字符）
 * @param text 待检查的字符串
 * @return 1 表示空白，0 表示非空白
 */
int MenuApplication_is_blank(const char *text) {
    if (text == 0) {
        return 1;
    }

    while (*text != '\0') {
        if (!isspace((unsigned char)*text)) {
            return 0;
        }

        text++;
    }

    return 1;
}

static int MenuApplication_is_digit_char(char character) {
    return character >= '0' && character <= '9';
}

static int MenuApplication_parse_two_digit_number(const char *text) {
    return (text[0] - '0') * 10 + (text[1] - '0');
}

static int MenuApplication_parse_four_digit_number(const char *text) {
    return (text[0] - '0') * 1000 +
           (text[1] - '0') * 100 +
           (text[2] - '0') * 10 +
           (text[3] - '0');
}

static int MenuApplication_is_leap_year(int year) {
    if (year % 400 == 0) {
        return 1;
    }
    if (year % 100 == 0) {
        return 0;
    }
    return year % 4 == 0 ? 1 : 0;
}

static int MenuApplication_days_in_month(int year, int month) {
    static const int days_per_month[] = {
        31, 28, 31, 30, 31, 30,
        31, 31, 30, 31, 30, 31
    };

    if (month < 1 || month > 12) {
        return 0;
    }

    if (month == 2 && MenuApplication_is_leap_year(year)) {
        return 29;
    }

    return days_per_month[month - 1];
}

static int MenuApplication_is_valid_calendar_date(
    int year,
    int month,
    int day,
    int hour,
    int minute
) {
    int max_day = 0;

    if (year <= 0) {
        return 0;
    }

    if (hour < 0 || hour > 23 || minute < 0 || minute > 59) {
        return 0;
    }

    max_day = MenuApplication_days_in_month(year, month);
    if (max_day == 0 || day < 1 || day > max_day) {
        return 0;
    }

    return 1;
}

static int MenuApplication_localtime_safe(const time_t *value, struct tm *out_tm) {
    if (value == 0 || out_tm == 0) {
        return 0;
    }

#ifdef _WIN32
    {
        struct tm *local_tm = localtime(value);
        if (local_tm == 0) {
            return 0;
        }
        *out_tm = *local_tm;
        return 1;
    }
#else
    return localtime_r(value, out_tm) != 0 ? 1 : 0;
#endif
}

static int MenuApplication_parse_registration_time(
    const char *registered_at,
    time_t *out_time
) {
    struct tm input_tm;
    struct tm normalized_tm;
    time_t parsed_time = (time_t)0;
    int year = 0;
    int month = 0;
    int day = 0;
    int hour = 0;
    int minute = 0;
    size_t index = 0;
    size_t length = 0;

    if (registered_at == 0 || out_time == 0) {
        return 0;
    }

    length = strlen(registered_at);
    if (length != 16) {
        return 0;
    }

    for (index = 0; index < length; index++) {
        if (index == 4 || index == 7) {
            if (registered_at[index] != '-') {
                return 0;
            }
            continue;
        }

        if (index == 10) {
            if (registered_at[index] != ' ' && registered_at[index] != 'T') {
                return 0;
            }
            continue;
        }

        if (index == 13) {
            if (registered_at[index] != ':') {
                return 0;
            }
            continue;
        }

        if (!MenuApplication_is_digit_char(registered_at[index])) {
            return 0;
        }
    }

    year = MenuApplication_parse_four_digit_number(registered_at);
    month = MenuApplication_parse_two_digit_number(registered_at + 5);
    day = MenuApplication_parse_two_digit_number(registered_at + 8);
    hour = MenuApplication_parse_two_digit_number(registered_at + 11);
    minute = MenuApplication_parse_two_digit_number(registered_at + 14);

    if (!MenuApplication_is_valid_calendar_date(year, month, day, hour, minute)) {
        return 0;
    }

    memset(&input_tm, 0, sizeof(input_tm));
    input_tm.tm_year = year - 1900;
    input_tm.tm_mon = month - 1;
    input_tm.tm_mday = day;
    input_tm.tm_hour = hour;
    input_tm.tm_min = minute;
    input_tm.tm_sec = 0;
    input_tm.tm_isdst = -1;

    parsed_time = mktime(&input_tm);
    if (parsed_time == (time_t)-1) {
        return 0;
    }

    if (!MenuApplication_localtime_safe(&parsed_time, &normalized_tm)) {
        return 0;
    }

    if (normalized_tm.tm_year != input_tm.tm_year ||
        normalized_tm.tm_mon != input_tm.tm_mon ||
        normalized_tm.tm_mday != input_tm.tm_mday ||
        normalized_tm.tm_hour != input_tm.tm_hour ||
        normalized_tm.tm_min != input_tm.tm_min) {
        return 0;
    }

    *out_time = parsed_time;
    return 1;
}

static Result MenuApplication_validate_self_registration_time(const char *registered_at) {
    time_t registered_time = (time_t)0;
    time_t current_time = (time_t)0;
    double seconds_until_visit = 0.0;
    const double max_window_seconds = 7.0 * 24.0 * 60.0 * 60.0;

    if (!MenuApplication_parse_registration_time(registered_at, &registered_time)) {
        return Result_make_failure(
            "registration time invalid; expected YYYY-MM-DD HH:MM within a real calendar date"
        );
    }

    current_time = time(0);
    if (current_time == (time_t)-1) {
        return Result_make_failure("current time unavailable");
    }

    seconds_until_visit = difftime(registered_time, current_time);
    if (seconds_until_visit < 0.0 || seconds_until_visit > max_window_seconds) {
        return Result_make_failure("registration time must be in the next 7 days");
    }

    return Result_make_success("self registration time valid");
}

/**
 * @brief 验证必填文本字段不为空
 * @param text       待验证的文本
 * @param field_name 字段名称（用于错误消息）
 * @return Result    验证通过或失败
 */
static Result MenuApplication_validate_required_text(
    const char *text,
    const char *field_name
) {
    char message[RESULT_MESSAGE_CAPACITY];

    if (MenuApplication_is_blank(text)) {
        snprintf(message, sizeof(message), "%s missing", field_name);
        return Result_make_failure(message);
    }

    return Result_make_success("text valid");
}

/**
 * @brief 自动生成患者编号
 *
 * 根据当前患者总数 + 1 生成顺序编号，格式为 "PAT0001" 等。
 */
static Result MenuApplication_generate_patient_id(
    MenuApplication *application,
    char *buffer,
    size_t capacity
) {
    LinkedList patients;
    Result result;
    int sequence = 0;

    if (application == 0 || buffer == 0 || capacity == 0) {
        return Result_make_failure("patient id generation arguments missing");
    }

    LinkedList_init(&patients);
    result = PatientRepository_load_all(&application->patient_service.repository, &patients);
    if (result.success == 0) {
        return result;
    }

    sequence = (int)LinkedList_count(&patients) + 1;
    PatientRepository_clear_loaded_list(&patients);
    if (!IdGenerator_format(buffer, capacity, "PAT", sequence, 4)) {
        return Result_make_failure("failed to generate patient id");
    }

    return Result_make_success("patient id generated");
}

/** @brief 自动生成科室编号（格式 "DEP0001" 等） */
static Result MenuApplication_generate_department_id(
    MenuApplication *application,
    char *buffer,
    size_t capacity
) {
    LinkedList departments;
    Result result;
    int sequence = 0;

    if (application == 0 || buffer == 0 || capacity == 0) {
        return Result_make_failure("department id generation arguments missing");
    }

    LinkedList_init(&departments);
    result = DepartmentRepository_load_all(
        &application->doctor_service.department_repository,
        &departments
    );
    if (result.success == 0) {
        return result;
    }

    sequence = (int)LinkedList_count(&departments) + 1;
    DepartmentRepository_clear_list(&departments);
    if (!IdGenerator_format(buffer, capacity, "DEP", sequence, 4)) {
        return Result_make_failure("failed to generate department id");
    }

    return Result_make_success("department id generated");
}

/** @brief 自动生成医生工号（格式 "DOC0001" 等） */
static Result MenuApplication_generate_doctor_id(
    MenuApplication *application,
    char *buffer,
    size_t capacity
) {
    LinkedList doctors;
    Result result;
    int sequence = 0;

    if (application == 0 || buffer == 0 || capacity == 0) {
        return Result_make_failure("doctor id generation arguments missing");
    }

    LinkedList_init(&doctors);
    result = DoctorRepository_load_all(&application->doctor_service.doctor_repository, &doctors);
    if (result.success == 0) {
        return result;
    }

    sequence = (int)LinkedList_count(&doctors) + 1;
    DoctorRepository_clear_list(&doctors);
    if (!IdGenerator_format(buffer, capacity, "DOC", sequence, 4)) {
        return Result_make_failure("failed to generate doctor id");
    }

    return Result_make_success("doctor id generated");
}

/** @brief 自动生成药品编号（格式 "MED0001" 等） */
static Result MenuApplication_generate_medicine_id(
    MenuApplication *application,
    char *buffer,
    size_t capacity
) {
    LinkedList medicines;
    Result result;
    int sequence = 0;

    if (application == 0 || buffer == 0 || capacity == 0) {
        return Result_make_failure("medicine id generation arguments missing");
    }

    LinkedList_init(&medicines);
    result = MedicineRepository_load_all(&application->pharmacy_service.medicine_repository, &medicines);
    if (result.success == 0) {
        return result;
    }

    sequence = (int)LinkedList_count(&medicines) + 1;
    MedicineRepository_clear_list(&medicines);
    if (!IdGenerator_format(buffer, capacity, "MED", sequence, 4)) {
        return Result_make_failure("failed to generate medicine id");
    }

    return Result_make_success("medicine id generated");
}

/** @brief 安全复制字符串到目标缓冲区 */
void MenuApplication_copy_text(
    char *destination,
    size_t capacity,
    const char *source
) {
    if (destination == 0 || capacity == 0) {
        return;
    }

    if (source == 0) {
        destination[0] = '\0';
        return;
    }

    strncpy(destination, source, capacity - 1);
    destination[capacity - 1] = '\0';
}

/** @brief 清除患者会话状态 */
static void MenuApplication_clear_patient_session_state(MenuApplication *application) {
    if (application == 0) {
        return;
    }

    application->has_bound_patient_session = 0;
    memset(application->bound_patient_id, 0, sizeof(application->bound_patient_id));
}

/** @brief 清除已认证用户状态 */
static void MenuApplication_clear_authenticated_user_state(MenuApplication *application) {
    if (application == 0) {
        return;
    }

    application->has_authenticated_user = 0;
    memset(&application->authenticated_user, 0, sizeof(application->authenticated_user));
    application->last_activity = 0;
}

void MenuApplication_touch_activity(MenuApplication *application) {
    if (application == 0) {
        return;
    }
    application->last_activity = time(NULL);
}

int MenuApplication_is_session_idle(const MenuApplication *application, time_t now_seconds) {
    if (application == 0 || application->has_authenticated_user == 0) {
        return 0;
    }
    if (application->last_activity == 0) {
        return 0;
    }
    if (now_seconds <= application->last_activity) {
        return 0;
    }
    return (now_seconds - application->last_activity) > HIS_SESSION_IDLE_TIMEOUT_SECONDS;
}

/** @brief 检查是否已绑定患者会话（患者角色操作的前置条件） */
Result MenuApplication_require_patient_session(MenuApplication *application) {
    if (application == 0) {
        return Result_make_failure("menu application missing");
    }

    if (application->has_bound_patient_session == 0 ||
        MenuApplication_is_blank(application->bound_patient_id)) {
        return Result_make_failure("patient session not bound");
    }

    return Result_make_success("patient session ready");
}

/** @brief 验证请求的患者编号是否与当前绑定的会话匹配（数据隔离） */
static Result MenuApplication_authorize_patient_session(
    MenuApplication *application,
    const char *requested_patient_id
) {
    Result result = MenuApplication_require_patient_session(application);
    if (result.success == 0) {
        return result;
    }

    result = MenuApplication_validate_required_text(requested_patient_id, "patient id");
    if (result.success == 0) {
        return result;
    }

    if (strcmp(application->bound_patient_id, requested_patient_id) != 0) {
        return Result_make_failure("patient session unauthorized");
    }

    return Result_make_success("patient session authorized");
}

/** @brief 格式化文本写入缓冲区（类似 snprintf 的封装） */
static Result MenuApplication_write_text(
    char *buffer,
    size_t capacity,
    const char *format,
    ...
) {
    va_list arguments;
    int written = 0;

    if (buffer == 0 || capacity == 0 || format == 0) {
        return Result_make_failure("output buffer invalid");
    }

    va_start(arguments, format);
    written = vsnprintf(buffer, capacity, format, arguments);
    va_end(arguments);

    if (written < 0 || (size_t)written >= capacity) {
        return Result_make_failure("output buffer too small");
    }

    return Result_make_success("output ready");
}

/** @brief 追加格式化文本到缓冲区已有内容之后 */
static Result MenuApplication_append_text(
    char *buffer,
    size_t capacity,
    size_t *used,
    const char *format,
    ...
) {
    va_list arguments;
    int written = 0;

    if (buffer == 0 || capacity == 0 || used == 0 || format == 0 || *used >= capacity) {
        return Result_make_failure("output append invalid");
    }

    va_start(arguments, format);
    written = vsnprintf(buffer + *used, capacity - *used, format, arguments);
    va_end(arguments);

    if (written < 0 || (size_t)written >= capacity - *used) {
        return Result_make_failure("output buffer too small");
    }

    *used += (size_t)written;
    return Result_make_success("output appended");
}

/** @brief 将失败信息写入缓冲区并返回失败结果 */
static Result MenuApplication_write_failure(
    const char *message,
    char *buffer,
    size_t capacity
) {
    if (buffer != 0 && capacity > 0 && message != 0) {
        (void)MenuApplication_write_text(buffer, capacity, TUI_CROSS " 操作失败: %s", message);
    }

    if (message == 0) {
        return Result_make_failure("operation failed");
    }

    return Result_make_failure(message);
}

/** @brief 根据操作结果显示成功或错误动画 */
void MenuApplication_print_result(FILE *out, const char *text, int success) {
    if (out == 0 || text == 0 || text[0] == '\0') {
        return;
    }
    if (success) {
        tui_animate_success(out, text);
    } else {
        tui_animate_error(out, text);
    }
}

/** @brief 将性别枚举转换为中文标签 */
static const char *MenuApplication_gender_label(PatientGender gender) {
    switch (gender) {
        case PATIENT_GENDER_MALE:
            return "男";
        case PATIENT_GENDER_FEMALE:
            return "女";
        default:
            return "未知";
    }
}

/** @brief 将挂号状态枚举转换为中文标签 */
static const char *MenuApplication_registration_status_label(RegistrationStatus status) {
    switch (status) {
        case REG_STATUS_PENDING:
            return "待诊";
        case REG_STATUS_DIAGNOSED:
            return "已接诊";
        case REG_STATUS_CANCELLED:
            return "已取消";
        default:
            return "未知";
    }
}

/** @brief 将挂号类型枚举转换为中文标签 */
static const char *MenuApplication_registration_type_label(int type) {
    switch (type) {
        case 0: return "普通号";
        case 1: return "专家号";
        case 2: return "急诊号";
        default: return "未知";
    }
}

/** @brief 将床位状态枚举转换为中文标签 */
static const char *MenuApplication_bed_status_label(BedStatus status) {
    switch (status) {
        case BED_STATUS_AVAILABLE:
            return "空闲";
        case BED_STATUS_OCCUPIED:
            return "占用";
        case BED_STATUS_MAINTENANCE:
            return "维护";
        default:
            return "未知";
    }
}

/** @brief 将病房状态枚举转换为中文标签 */
static const char *MenuApplication_ward_status_label(WardStatus status) {
    switch (status) {
        case WARD_STATUS_ACTIVE:
            return "启用";
        case WARD_STATUS_CLOSED:
            return "停用";
        default:
            return "未知";
    }
}

/** @brief 将病房类型枚举转换为中文标签 */
static const char *MenuApplication_ward_type_label(WardType ward_type) {
    switch (ward_type) {
        case WARD_TYPE_STANDARD:
            return "普通";
        case WARD_TYPE_DOUBLE:
            return "双人";
        case WARD_TYPE_VIP:
            return "VIP";
        default:
            return "未知";
    }
}

/** @brief 将住院状态枚举转换为中文标签 */
static const char *MenuApplication_admission_status_label(AdmissionStatus status) {
    switch (status) {
        case ADMISSION_STATUS_ACTIVE:
            return "住院中";
        case ADMISSION_STATUS_DISCHARGED:
            return "已出院";
        default:
            return "未知";
    }
}

/** @brief 将就诊记录数据填充到交接结构体中 */
static void MenuApplication_fill_visit_handoff(
    const VisitRecord *record,
    MenuApplicationVisitHandoff *out_handoff
) {
    if (record == 0 || out_handoff == 0) {
        return;
    }

    memset(out_handoff, 0, sizeof(*out_handoff));
    MenuApplication_copy_text(out_handoff->visit_id, sizeof(out_handoff->visit_id), record->visit_id);
    MenuApplication_copy_text(
        out_handoff->registration_id,
        sizeof(out_handoff->registration_id),
        record->registration_id
    );
    MenuApplication_copy_text(
        out_handoff->patient_id,
        sizeof(out_handoff->patient_id),
        record->patient_id
    );
    MenuApplication_copy_text(
        out_handoff->doctor_id,
        sizeof(out_handoff->doctor_id),
        record->doctor_id
    );
    MenuApplication_copy_text(
        out_handoff->department_id,
        sizeof(out_handoff->department_id),
        record->department_id
    );
    MenuApplication_copy_text(
        out_handoff->diagnosis,
        sizeof(out_handoff->diagnosis),
        record->diagnosis
    );
    MenuApplication_copy_text(
        out_handoff->advice,
        sizeof(out_handoff->advice),
        record->advice
    );
    out_handoff->need_exam = record->need_exam;
    out_handoff->need_admission = record->need_admission;
    out_handoff->need_medicine = record->need_medicine;
    MenuApplication_copy_text(
        out_handoff->visit_time,
        sizeof(out_handoff->visit_time),
        record->visit_time
    );
}

Result MenuApplication_init(MenuApplication *application, const MenuApplicationPaths *paths) {
    Result result;

    if (application == 0 || paths == 0) {
        return Result_make_failure("menu application arguments missing");
    }

    result = MenuApplication_validate_required_text(paths->user_path, "user path");
    if (result.success == 0) {
        return result;
    }
    result = MenuApplication_validate_required_text(paths->patient_path, "patient path");
    if (result.success == 0) {
        return result;
    }
    result = MenuApplication_validate_required_text(paths->department_path, "department path");
    if (result.success == 0) {
        return result;
    }
    result = MenuApplication_validate_required_text(paths->doctor_path, "doctor path");
    if (result.success == 0) {
        return result;
    }
    result = MenuApplication_validate_required_text(paths->registration_path, "registration path");
    if (result.success == 0) {
        return result;
    }
    result = MenuApplication_validate_required_text(paths->visit_path, "visit path");
    if (result.success == 0) {
        return result;
    }
    result = MenuApplication_validate_required_text(paths->examination_path, "examination path");
    if (result.success == 0) {
        return result;
    }
    result = MenuApplication_validate_required_text(paths->ward_path, "ward path");
    if (result.success == 0) {
        return result;
    }
    result = MenuApplication_validate_required_text(paths->bed_path, "bed path");
    if (result.success == 0) {
        return result;
    }
    result = MenuApplication_validate_required_text(paths->admission_path, "admission path");
    if (result.success == 0) {
        return result;
    }
    result = MenuApplication_validate_required_text(paths->medicine_path, "medicine path");
    if (result.success == 0) {
        return result;
    }
    result = MenuApplication_validate_required_text(
        paths->dispense_record_path,
        "dispense record path"
    );
    if (result.success == 0) {
        return result;
    }
    result = MenuApplication_validate_required_text(paths->prescription_path, "prescription path");
    if (result.success == 0) {
        return result;
    }

    result = AuthService_init(
        &application->auth_service,
        paths->user_path,
        paths->patient_path
    );
    if (result.success == 0) {
        return result;
    }

    result = PatientService_init(&application->patient_service, paths->patient_path);
    if (result.success == 0) {
        return result;
    }

    result = DoctorService_init(
        &application->doctor_service,
        paths->doctor_path,
        paths->department_path
    );
    if (result.success == 0) {
        return result;
    }

    result = RegistrationRepository_init(
        &application->registration_repository,
        paths->registration_path
    );
    if (result.success == 0) {
        return result;
    }

    result = RegistrationService_init(
        &application->registration_service,
        &application->registration_repository,
        &application->patient_service.repository,
        &application->doctor_service.doctor_repository,
        &application->doctor_service.department_repository
    );
    if (result.success == 0) {
        return result;
    }

    /* 初始化持久化序列号服务，替代挂号创建路径上的 O(N) 写放大。
     * sequences.txt 放在 registration_path 同目录下，保持与 data/ 共存。
     * 首次运行时会扫描 registrations.txt 得到迁移种子 (max REG 序号)。 */
    {
        char sequences_path_buffer[HIS_FILE_PATH_CAPACITY];
        const char *sequences_path = paths->sequences_path;
        SequenceTypeSpec spec;

        if (sequences_path == 0 || sequences_path[0] == '\0') {
            /* 未显式提供路径：在 registration_path 后追加 ".sequences" 扩展名。
             * 避免多测试共享同一 data/ 父目录时互相污染序列号状态（每个测试的
             * registration_path 唯一，因此派生出的 sequences 路径也唯一）。 */
            int written = snprintf(
                sequences_path_buffer,
                sizeof(sequences_path_buffer),
                "%s.sequences",
                paths->registration_path != 0 ? paths->registration_path : "sequences"
            );
            if (written < 0 || (size_t)written >= sizeof(sequences_path_buffer)) {
                return Result_make_failure("sequences path too long");
            }
            sequences_path = sequences_path_buffer;
        }

        /* 本轮仅注册 REGISTRATION 类型；后续扩展到其他 Service 时在此追加 spec。 */
        memset(&spec, 0, sizeof(spec));
        snprintf(spec.type_name, sizeof(spec.type_name), "REGISTRATION");
        snprintf(spec.id_prefix, sizeof(spec.id_prefix), "REG");
        if (paths->registration_path != 0) {
            snprintf(
                spec.data_path,
                sizeof(spec.data_path),
                "%s",
                paths->registration_path
            );
        }

        result = SequenceService_init(
            &application->sequence_service,
            sequences_path,
            &spec,
            1
        );
        if (result.success == 0) {
            return result;
        }

        RegistrationService_set_sequence_service(
            &application->registration_service,
            &application->sequence_service
        );
    }

    result = MedicalRecordService_init(
        &application->medical_record_service,
        paths->registration_path,
        paths->visit_path,
        paths->examination_path,
        paths->admission_path
    );
    if (result.success == 0) {
        return result;
    }

    result = InpatientService_init(
        &application->inpatient_service,
        paths->patient_path,
        paths->ward_path,
        paths->bed_path,
        paths->admission_path
    );
    if (result.success == 0) {
        return result;
    }

    result = BedService_init(
        &application->bed_service,
        paths->ward_path,
        paths->bed_path,
        paths->admission_path
    );
    if (result.success == 0) {
        return result;
    }

    result = PharmacyService_init(
        &application->pharmacy_service,
        paths->medicine_path,
        paths->dispense_record_path
    );
    if (result.success == 0) {
        return result;
    }

    result = PrescriptionService_init(
        &application->prescription_service,
        paths->prescription_path
    );
    if (result.success == 0) {
        return result;
    }

    if (paths->inpatient_order_path != 0 && paths->inpatient_order_path[0] != '\0') {
        result = InpatientOrderService_init(
            &application->inpatient_order_service,
            paths->inpatient_order_path
        );
        if (result.success == 0) {
            return result;
        }
    }

    if (paths->nursing_record_path != 0 && paths->nursing_record_path[0] != '\0') {
        result = NursingRecordService_init(
            &application->nursing_record_service,
            paths->nursing_record_path
        );
        if (result.success == 0) {
            return result;
        }
    }

    if (paths->round_record_path != 0 && paths->round_record_path[0] != '\0') {
        result = RoundRecordService_init(
            &application->round_record_service,
            paths->round_record_path
        );
        if (result.success == 0) {
            return result;
        }
    }

    /* 初始化审计日志服务：data_dir 缺省时退化为当前目录下的 audit.log */
    result = AuditService_init(
        &application->audit_service,
        paths->data_dir != 0 ? paths->data_dir : ""
    );
    if (result.success == 0) {
        return result;
    }

    MenuApplication_clear_authenticated_user_state(application);
    MenuApplication_clear_patient_session_state(application);

    return Result_make_success("application initialized");
}

Result MenuApplication_login(
    MenuApplication *application,
    const char *user_id,
    const char *password,
    UserRole required_role
) {
    User user;
    Result result;

    if (application == 0) {
        return Result_make_failure("menu application missing");
    }

    MenuApplication_clear_authenticated_user_state(application);
    MenuApplication_clear_patient_session_state(application);

    result = AuthService_authenticate(
        &application->auth_service,
        user_id,
        password,
        required_role,
        &user
    );
    if (result.success == 0) {
        /* 登录失败审计：角色尚未确认，仅记录尝试的用户ID */
        (void)AuditService_record(
            &application->audit_service,
            AUDIT_EVENT_LOGIN_FAILURE,
            user_id,
            0,
            0,
            0,
            "FAIL",
            result.message
        );
        return result;
    }

    application->authenticated_user = user;
    application->has_authenticated_user = 1;
    application->last_activity = time(NULL);

    if (user.role == USER_ROLE_PATIENT) {
        result = MenuApplication_bind_patient_session(application, user.patient_id);
        if (result.success == 0) {
            MenuApplication_clear_authenticated_user_state(application);
            return result;
        }
    }

    (void)AuditService_record(
        &application->audit_service,
        AUDIT_EVENT_LOGIN_SUCCESS,
        user.user_id,
        MenuApplication_audit_role_label(user.role),
        0,
        0,
        "OK",
        0
    );

    return Result_make_success("menu application login ready");
}

void MenuApplication_logout(MenuApplication *application) {
    if (application != 0 && application->has_authenticated_user) {
        (void)AuditService_record(
            &application->audit_service,
            AUDIT_EVENT_LOGOUT,
            application->authenticated_user.user_id,
            MenuApplication_audit_role_label(application->authenticated_user.role),
            0,
            0,
            "OK",
            0
        );
    }
    MenuApplication_clear_authenticated_user_state(application);
    MenuApplication_clear_patient_session_state(application);
}

Result MenuApplication_bind_patient_session(MenuApplication *application, const char *patient_id) {
    Patient patient;
    Result result;

    if (application == 0) {
        return Result_make_failure("menu application missing");
    }

    result = MenuApplication_validate_required_text(patient_id, "patient id");
    if (result.success == 0) {
        return result;
    }

    MenuApplication_clear_patient_session_state(application);
    result = PatientService_find_patient_by_id(
        &application->patient_service,
        patient_id,
        &patient
    );
    if (result.success == 0) {
        return result;
    }

    strncpy(
        application->bound_patient_id,
        patient.patient_id,
        sizeof(application->bound_patient_id) - 1
    );
    application->bound_patient_id[sizeof(application->bound_patient_id) - 1] = '\0';
    application->has_bound_patient_session = 1;
    return Result_make_success("patient session bound");
}

void MenuApplication_reset_patient_session(MenuApplication *application) {
    MenuApplication_clear_patient_session_state(application);
}

Result MenuApplication_add_patient(
    MenuApplication *application,
    const Patient *patient,
    char *buffer,
    size_t capacity
) {
    Patient stored_patient;
    Result result;

    if (application == 0 || patient == 0) {
        return Result_make_failure("patient add arguments missing");
    }

    stored_patient = *patient;
    if (MenuApplication_is_blank(stored_patient.patient_id)) {
        result = MenuApplication_generate_patient_id(
            application,
            stored_patient.patient_id,
            sizeof(stored_patient.patient_id)
        );
        if (result.success == 0) {
            return MenuApplication_write_failure(result.message, buffer, capacity);
        }
    }

    result = PatientService_create_patient(&application->patient_service, &stored_patient);

    if (result.success == 0) {
        return MenuApplication_write_failure(result.message, buffer, capacity);
    }

    return MenuApplication_write_text(
        buffer,
        capacity,
        "患者已添加: %s | %s | 性别=%s | 年龄=%d | 联系方式=%s | 住院=%s",
        stored_patient.patient_id,
        stored_patient.name,
        MenuApplication_gender_label(stored_patient.gender),
        stored_patient.age,
        stored_patient.contact,
        stored_patient.is_inpatient ? "是" : "否"
    );
}

Result MenuApplication_update_patient(
    MenuApplication *application,
    const Patient *patient,
    char *buffer,
    size_t capacity
) {
    Result result = PatientService_update_patient(&application->patient_service, patient);

    if (result.success == 0) {
        return MenuApplication_write_failure(result.message, buffer, capacity);
    }

    return MenuApplication_write_text(
        buffer,
        capacity,
        "患者已更新: %s | %s | 性别=%s | 年龄=%d | 联系方式=%s | 住院=%s",
        patient->patient_id,
        patient->name,
        MenuApplication_gender_label(patient->gender),
        patient->age,
        patient->contact,
        patient->is_inpatient ? "是" : "否"
    );
}

Result MenuApplication_query_patient(
    MenuApplication *application,
    const char *patient_id,
    char *buffer,
    size_t capacity
) {
    Patient patient;
    Result result = PatientService_find_patient_by_id(
        &application->patient_service,
        patient_id,
        &patient
    );

    if (result.success == 0) {
        return MenuApplication_write_failure(result.message, buffer, capacity);
    }

    return MenuApplication_write_text(
        buffer,
        capacity,
        "患者信息: %s | %s | 性别=%s | 年龄=%d | 联系方式=%s | 身份证=%s | 住院=%s",
        patient.patient_id,
        patient.name,
        MenuApplication_gender_label(patient.gender),
        patient.age,
        patient.contact,
        patient.id_card,
        patient.is_inpatient ? "是" : "否"
    );
}

Result MenuApplication_create_registration(
    MenuApplication *application,
    const char *patient_id,
    const char *doctor_id,
    const char *department_id,
    const char *registered_at,
    RegistrationType registration_type,
    double registration_fee,
    char *buffer,
    size_t capacity
) {
    Registration registration;
    Result result = RegistrationService_create(
        &application->registration_service,
        patient_id,
        doctor_id,
        department_id,
        registered_at,
        registration_type,
        registration_fee,
        &registration
    );

    if (result.success == 0) {
        return MenuApplication_write_failure(result.message, buffer, capacity);
    }

    (void)AuditService_record(
        &application->audit_service,
        AUDIT_EVENT_CREATE,
        MenuApplication_audit_actor_id(application),
        MenuApplication_audit_actor_role(application),
        "REGISTRATION",
        registration.registration_id,
        "OK",
        registration.patient_id
    );

    return MenuApplication_write_text(
        buffer,
        capacity,
        "挂号已创建: %s | 患者=%s | 医生=%s | 科室=%s | 时间=%s | 状态=%s | 类型=%s | 费用=%.2f元",
        registration.registration_id,
        registration.patient_id,
        registration.doctor_id,
        registration.department_id,
        registration.registered_at,
        MenuApplication_registration_status_label(registration.status),
        MenuApplication_registration_type_label(registration.registration_type),
        registration.registration_fee
    );
}

Result MenuApplication_create_self_registration(
    MenuApplication *application,
    const char *doctor_id,
    const char *department_id,
    const char *registered_at,
    RegistrationType registration_type,
    double registration_fee,
    Registration *out_registration
) {
    Result result = MenuApplication_require_patient_session(application);

    if (out_registration == 0) {
        return Result_make_failure("self registration output missing");
    }

    if (result.success == 0) {
        return result;
    }

    result = MenuApplication_validate_self_registration_time(registered_at);
    if (result.success == 0) {
        return result;
    }

    result = RegistrationService_create(
        &application->registration_service,
        application->bound_patient_id,
        doctor_id,
        department_id,
        registered_at,
        registration_type,
        registration_fee,
        out_registration
    );
    if (result.success != 0) {
        (void)AuditService_record(
            &application->audit_service,
            AUDIT_EVENT_CREATE,
            MenuApplication_audit_actor_id(application),
            MenuApplication_audit_actor_role(application),
            "REGISTRATION",
            out_registration->registration_id,
            "OK",
            out_registration->patient_id
        );
    }
    return result;
}

Result MenuApplication_query_registration(
    MenuApplication *application,
    const char *registration_id,
    char *buffer,
    size_t capacity
) {
    Registration registration;
    Result result = RegistrationService_find_by_registration_id(
        &application->registration_service,
        registration_id,
        &registration
    );

    if (result.success == 0) {
        return MenuApplication_write_failure(result.message, buffer, capacity);
    }

    return MenuApplication_write_text(
        buffer,
        capacity,
        "挂号信息: %s | 患者=%s | 医生=%s | 科室=%s | 状态=%s | 挂号时间=%s | 类型=%s | 费用=%.2f元",
        registration.registration_id,
        registration.patient_id,
        registration.doctor_id,
        registration.department_id,
        MenuApplication_registration_status_label(registration.status),
        registration.registered_at,
        MenuApplication_registration_type_label(registration.registration_type),
        registration.registration_fee
    );
}

Result MenuApplication_cancel_registration(
    MenuApplication *application,
    const char *registration_id,
    const char *cancelled_at,
    char *buffer,
    size_t capacity
) {
    Registration registration;
    Result result = RegistrationService_cancel(
        &application->registration_service,
        registration_id,
        cancelled_at,
        &registration
    );

    if (result.success == 0) {
        return MenuApplication_write_failure(result.message, buffer, capacity);
    }

    return MenuApplication_write_text(
        buffer,
        capacity,
        "挂号已取消: %s | 患者=%s | 时间=%s",
        registration.registration_id,
        registration.patient_id,
        registration.cancelled_at
    );
}

Result MenuApplication_query_registrations_by_patient(
    MenuApplication *application,
    const char *patient_id,
    char *buffer,
    size_t capacity
) {
    RegistrationRepositoryFilter filter;
    LinkedList registrations;
    const LinkedListNode *current = 0;
    size_t used = 0;
    Result result;

    LinkedList_init(&registrations);
    RegistrationRepositoryFilter_init(&filter);
    result = RegistrationService_find_by_patient_id(
        &application->registration_service,
        patient_id,
        &filter,
        &registrations
    );
    if (result.success == 0) {
        return MenuApplication_write_failure(result.message, buffer, capacity);
    }

    result = MenuApplication_write_text(
        buffer,
        capacity,
        "患者挂号记录: %s | count=%zu",
        patient_id,
        LinkedList_count(&registrations)
    );
    if (result.success == 0) {
        RegistrationRepository_clear_list(&registrations);
        return result;
    }

    used = strlen(buffer);
    current = registrations.head;
    while (current != 0) {
        const Registration *registration = (const Registration *)current->data;

        result = MenuApplication_append_text(
            buffer,
            capacity,
            &used,
            "\n%s | 患者=%s | 医生=%s | 科室=%s | 状态=%s | 类型=%s | 费用=%.2f元",
            registration->registration_id,
            registration->patient_id,
            registration->doctor_id,
            registration->department_id,
            MenuApplication_registration_status_label(registration->status),
            MenuApplication_registration_type_label(registration->registration_type),
            registration->registration_fee
        );
        if (result.success == 0) {
            RegistrationRepository_clear_list(&registrations);
            return result;
        }

        current = current->next;
    }

    RegistrationRepository_clear_list(&registrations);
    return Result_make_success("patient registrations ready");
}

Result MenuApplication_query_pending_registrations_by_doctor(
    MenuApplication *application,
    const char *doctor_id,
    char *buffer,
    size_t capacity
) {
    RegistrationRepositoryFilter filter;
    LinkedList registrations;
    const LinkedListNode *current = 0;
    size_t used = 0;
    Result result;

    LinkedList_init(&registrations);
    RegistrationRepositoryFilter_init(&filter);
    filter.use_status = 1;
    filter.status = REG_STATUS_PENDING;

    result = RegistrationService_find_by_doctor_id(
        &application->registration_service,
        doctor_id,
        &filter,
        &registrations
    );
    if (result.success == 0) {
        return MenuApplication_write_failure(result.message, buffer, capacity);
    }

    result = MenuApplication_write_text(
        buffer,
        capacity,
        "待诊列表: %s | count=%zu",
        doctor_id,
        LinkedList_count(&registrations)
    );
    if (result.success == 0) {
        RegistrationRepository_clear_list(&registrations);
        return result;
    }

    used = strlen(buffer);
    current = registrations.head;
    while (current != 0) {
        const Registration *registration = (const Registration *)current->data;

        result = MenuApplication_append_text(
            buffer,
            capacity,
            &used,
            "\n%s | 患者=%s | 科室=%s | 时间=%s | 类型=%s | 费用=%.2f元",
            registration->registration_id,
            registration->patient_id,
            registration->department_id,
            registration->registered_at,
            MenuApplication_registration_type_label(registration->registration_type),
            registration->registration_fee
        );
        if (result.success == 0) {
            RegistrationRepository_clear_list(&registrations);
            return result;
        }

        current = current->next;
    }

    RegistrationRepository_clear_list(&registrations);
    return Result_make_success("doctor pending registrations ready");
}

Result MenuApplication_create_visit_record(
    MenuApplication *application,
    const char *registration_id,
    const char *chief_complaint,
    const char *diagnosis,
    const char *advice,
    int need_exam,
    int need_admission,
    int need_medicine,
    const char *visit_time,
    char *buffer,
    size_t capacity
) {
    MenuApplicationVisitHandoff handoff;
    Result result = MenuApplication_create_visit_record_handoff(
        application,
        registration_id,
        chief_complaint,
        diagnosis,
        advice,
        need_exam,
        need_admission,
        need_medicine,
        visit_time,
        &handoff
    );

    if (result.success == 0) {
        return MenuApplication_write_failure(result.message, buffer, capacity);
    }

    return MenuApplication_write_text(
        buffer,
        capacity,
        "看诊记录已创建: %s | 挂号=%s | 患者=%s | 诊断=%s",
        handoff.visit_id,
        handoff.registration_id,
        handoff.patient_id,
        handoff.diagnosis
    );
}

Result MenuApplication_create_visit_record_handoff(
    MenuApplication *application,
    const char *registration_id,
    const char *chief_complaint,
    const char *diagnosis,
    const char *advice,
    int need_exam,
    int need_admission,
    int need_medicine,
    const char *visit_time,
    MenuApplicationVisitHandoff *out_handoff
) {
    VisitRecord record;
    VisitRecordParams params;
    Result result;

    if (application == 0 || out_handoff == 0) {
        return Result_make_failure("visit handoff arguments missing");
    }

    memset(&params, 0, sizeof(params));
    params.registration_id = registration_id;
    params.chief_complaint = chief_complaint;
    params.diagnosis = diagnosis;
    params.advice = advice;
    params.need_exam = need_exam;
    params.need_admission = need_admission;
    params.need_medicine = need_medicine;
    params.visit_time = visit_time;

    result = MedicalRecordService_create_visit_record(
        &application->medical_record_service,
        &params,
        &record
    );
    if (result.success == 0) {
        return result;
    }

    (void)AuditService_record(
        &application->audit_service,
        AUDIT_EVENT_CREATE,
        MenuApplication_audit_actor_id(application),
        MenuApplication_audit_actor_role(application),
        "VISIT",
        record.visit_id,
        "OK",
        record.patient_id
    );

    MenuApplication_fill_visit_handoff(&record, out_handoff);
    return Result_make_success("visit handoff ready");
}

Result MenuApplication_query_patient_history(
    MenuApplication *application,
    const char *patient_id,
    char *buffer,
    size_t capacity
) {
    MedicalRecordHistory history;
    Result result = MedicalRecordService_find_patient_history(
        &application->medical_record_service,
        patient_id,
        &history
    );

    if (result.success == 0) {
        return MenuApplication_write_failure(result.message, buffer, capacity);
    }

    result = MenuApplication_write_text(
        buffer,
        capacity,
        "患者历史: %s | registrations=%zu | visits=%zu | examinations=%zu | admissions=%zu",
        patient_id,
        LinkedList_count(&history.registrations),
        LinkedList_count(&history.visits),
        LinkedList_count(&history.examinations),
        LinkedList_count(&history.admissions)
    );
    MedicalRecordHistory_clear(&history);
    return result;
}

Result MenuApplication_create_examination_record(
    MenuApplication *application,
    const char *visit_id,
    const char *exam_item,
    const char *exam_type,
    double exam_fee,
    const char *requested_at,
    char *buffer,
    size_t capacity
) {
    ExaminationRecord record;
    Result result = MedicalRecordService_create_examination_record(
        &application->medical_record_service,
        visit_id,
        exam_item,
        exam_type,
        exam_fee,
        requested_at,
        &record
    );

    if (result.success == 0) {
        return MenuApplication_write_failure(result.message, buffer, capacity);
    }

    (void)AuditService_record(
        &application->audit_service,
        AUDIT_EVENT_CREATE,
        MenuApplication_audit_actor_id(application),
        MenuApplication_audit_actor_role(application),
        "EXAMINATION",
        record.examination_id,
        "OK",
        record.exam_item
    );

    return MenuApplication_write_text(
        buffer,
        capacity,
        "检查记录已创建: %s | 就诊=%s | 项目=%s | 类型=%s | 费用=%.2f元",
        record.examination_id,
        record.visit_id,
        record.exam_item,
        record.exam_type,
        record.exam_fee
    );
}

Result MenuApplication_complete_examination_record(
    MenuApplication *application,
    const char *examination_id,
    const char *result_text,
    const char *completed_at,
    char *buffer,
    size_t capacity
) {
    ExaminationRecord record;
    Result result = MedicalRecordService_update_examination_record(
        &application->medical_record_service,
        examination_id,
        EXAM_STATUS_COMPLETED,
        result_text,
        completed_at,
        &record
    );

    if (result.success == 0) {
        return MenuApplication_write_failure(result.message, buffer, capacity);
    }

    return MenuApplication_write_text(
        buffer,
        capacity,
        "检查结果已回写: %s | 结果=%s | 完成时间=%s",
        record.examination_id,
        record.result,
        record.completed_at
    );
}

Result MenuApplication_list_wards(
    MenuApplication *application,
    char *buffer,
    size_t capacity
) {
    LinkedList wards;
    const LinkedListNode *current = 0;
    size_t used = 0;
    Result result = BedService_list_wards(&application->bed_service, &wards);

    if (result.success == 0) {
        return MenuApplication_write_failure(result.message, buffer, capacity);
    }

    result = MenuApplication_write_text(
        buffer,
        capacity,
        "病房列表(%zu)",
        LinkedList_count(&wards)
    );
    if (result.success == 0) {
        BedService_clear_wards(&wards);
        return result;
    }

    used = strlen(buffer);
    current = wards.head;
    while (current != 0) {
        const Ward *ward = (const Ward *)current->data;

        result = MenuApplication_append_text(
            buffer,
            capacity,
            &used,
            "\n%s | %s | 科室=%s | 位置=%s | 床位=%d/%d | 类型=%s | 日费=%.2f | 状态=%s",
            ward->ward_id,
            ward->name,
            ward->department_id,
            ward->location,
            ward->occupied_beds,
            ward->capacity,
            MenuApplication_ward_type_label(ward->ward_type),
            ward->daily_fee,
            MenuApplication_ward_status_label(ward->status)
        );
        if (result.success == 0) {
            BedService_clear_wards(&wards);
            return result;
        }

        current = current->next;
    }

    BedService_clear_wards(&wards);
    return Result_make_success("ward list ready");
}

Result MenuApplication_list_beds_by_ward(
    MenuApplication *application,
    const char *ward_id,
    char *buffer,
    size_t capacity
) {
    LinkedList beds;
    const LinkedListNode *current = 0;
    size_t used = 0;
    Result result = BedService_list_beds_by_ward(
        &application->bed_service,
        ward_id,
        &beds
    );

    if (result.success == 0) {
        return MenuApplication_write_failure(result.message, buffer, capacity);
    }

    result = MenuApplication_write_text(
        buffer,
        capacity,
        "床位列表(%s/%zu)",
        ward_id,
        LinkedList_count(&beds)
    );
    if (result.success == 0) {
        BedService_clear_beds(&beds);
        return result;
    }

    used = strlen(buffer);
    current = beds.head;
    while (current != 0) {
        const Bed *bed = (const Bed *)current->data;

        result = MenuApplication_append_text(
            buffer,
            capacity,
            &used,
            "\n%s | 房间=%s | 床号=%s | 状态=%s",
            bed->bed_id,
            bed->room_no,
            bed->bed_no,
            MenuApplication_bed_status_label(bed->status)
        );
        if (result.success == 0) {
            BedService_clear_beds(&beds);
            return result;
        }

        current = current->next;
    }

    BedService_clear_beds(&beds);
    return Result_make_success("bed list ready");
}

Result MenuApplication_admit_patient(
    MenuApplication *application,
    const char *patient_id,
    const char *ward_id,
    const char *bed_id,
    const char *admitted_at,
    const char *summary,
    char *buffer,
    size_t capacity
) {
    Admission admission;
    Result result = InpatientService_admit_patient(
        &application->inpatient_service,
        patient_id,
        ward_id,
        bed_id,
        admitted_at,
        summary,
        &admission
    );

    if (result.success == 0) {
        return MenuApplication_write_failure(result.message, buffer, capacity);
    }

    (void)AuditService_record(
        &application->audit_service,
        AUDIT_EVENT_CREATE,
        MenuApplication_audit_actor_id(application),
        MenuApplication_audit_actor_role(application),
        "ADMISSION",
        admission.admission_id,
        "OK",
        admission.patient_id
    );

    return MenuApplication_write_text(
        buffer,
        capacity,
        "住院办理成功: %s | 患者=%s | 病房=%s | 床位=%s | 时间=%s",
        admission.admission_id,
        admission.patient_id,
        admission.ward_id,
        admission.bed_id,
        admission.admitted_at
    );
}

Result MenuApplication_discharge_patient(
    MenuApplication *application,
    const char *admission_id,
    const char *discharged_at,
    const char *summary,
    char *buffer,
    size_t capacity
) {
    Admission admission;
    Result result = InpatientService_discharge_patient(
        &application->inpatient_service,
        admission_id,
        discharged_at,
        summary,
        &admission
    );

    if (result.success == 0) {
        return MenuApplication_write_failure(result.message, buffer, capacity);
    }

    (void)AuditService_record(
        &application->audit_service,
        AUDIT_EVENT_DISCHARGE,
        MenuApplication_audit_actor_id(application),
        MenuApplication_audit_actor_role(application),
        "ADMISSION",
        admission.admission_id,
        "OK",
        admission.patient_id
    );

    return MenuApplication_write_text(
        buffer,
        capacity,
        "出院办理成功: %s | 患者=%s | 状态=%s | 时间=%s",
        admission.admission_id,
        admission.patient_id,
        MenuApplication_admission_status_label(admission.status),
        admission.discharged_at
    );
}

Result MenuApplication_transfer_bed(
    MenuApplication *application,
    const char *admission_id,
    const char *target_bed_id,
    const char *transferred_at,
    char *buffer,
    size_t capacity
) {
    Admission admission;
    Result result = InpatientService_transfer_bed(
        &application->inpatient_service,
        admission_id,
        target_bed_id,
        transferred_at,
        &admission
    );

    if (result.success == 0) {
        return MenuApplication_write_failure(result.message, buffer, capacity);
    }

    (void)AuditService_record(
        &application->audit_service,
        AUDIT_EVENT_UPDATE,
        MenuApplication_audit_actor_id(application),
        MenuApplication_audit_actor_role(application),
        "ADMISSION",
        admission.admission_id,
        "OK",
        admission.bed_id
    );

    return MenuApplication_write_text(
        buffer,
        capacity,
        "转床成功: %s | 病房=%s | 床位=%s | 时间=%s",
        admission.admission_id,
        admission.ward_id,
        admission.bed_id,
        transferred_at
    );
}

Result MenuApplication_query_active_admission_by_patient(
    MenuApplication *application,
    const char *patient_id,
    char *buffer,
    size_t capacity
) {
    Admission admission;
    Result result = InpatientService_find_active_by_patient_id(
        &application->inpatient_service,
        patient_id,
        &admission
    );

    if (result.success == 0) {
        return MenuApplication_write_failure(result.message, buffer, capacity);
    }

    return MenuApplication_write_text(
        buffer,
        capacity,
        "当前住院: %s | 患者=%s | 病房=%s | 床位=%s | 状态=%s",
        admission.admission_id,
        admission.patient_id,
        admission.ward_id,
        admission.bed_id,
        MenuApplication_admission_status_label(admission.status)
    );
}

Result MenuApplication_query_current_patient_by_bed(
    MenuApplication *application,
    const char *bed_id,
    char *buffer,
    size_t capacity
) {
    char patient_id[HIS_DOMAIN_ID_CAPACITY];
    Result result = BedService_find_current_patient_by_bed_id(
        &application->bed_service,
        bed_id,
        patient_id,
        sizeof(patient_id)
    );

    if (result.success == 0) {
        return MenuApplication_write_failure(result.message, buffer, capacity);
    }

    return MenuApplication_write_text(
        buffer,
        capacity,
        "当前入住患者: 床位=%s | 患者=%s",
        bed_id,
        patient_id
    );
}

Result MenuApplication_add_medicine(
    MenuApplication *application,
    const Medicine *medicine,
    char *buffer,
    size_t capacity
) {
    Medicine stored_medicine;
    Result result;

    if (application == 0 || medicine == 0) {
        return Result_make_failure("medicine add arguments missing");
    }

    stored_medicine = *medicine;
    if (MenuApplication_is_blank(stored_medicine.medicine_id)) {
        result = MenuApplication_generate_medicine_id(
            application,
            stored_medicine.medicine_id,
            sizeof(stored_medicine.medicine_id)
        );
        if (result.success == 0) {
            return MenuApplication_write_failure(result.message, buffer, capacity);
        }
    }

    result = PharmacyService_add_medicine(&application->pharmacy_service, &stored_medicine);

    if (result.success == 0) {
        return MenuApplication_write_failure(result.message, buffer, capacity);
    }

    return MenuApplication_write_text(
        buffer,
        capacity,
        "药品已添加: %s | %s | 单价=%.2f | 库存=%d",
        stored_medicine.medicine_id,
        stored_medicine.name,
        stored_medicine.price,
        stored_medicine.stock
    );
}

Result MenuApplication_restock_medicine(
    MenuApplication *application,
    const char *medicine_id,
    int quantity,
    char *buffer,
    size_t capacity
) {
    int stock = 0;
    Result result = PharmacyService_restock_medicine(
        &application->pharmacy_service,
        medicine_id,
        quantity
    );

    if (result.success == 0) {
        return MenuApplication_write_failure(result.message, buffer, capacity);
    }

    result = PharmacyService_get_stock(&application->pharmacy_service, medicine_id, &stock);
    if (result.success == 0) {
        return MenuApplication_write_failure(result.message, buffer, capacity);
    }

    return MenuApplication_write_text(
        buffer,
        capacity,
        "药品已入库: %s | 本次=%d | 当前库存=%d",
        medicine_id,
        quantity,
        stock
    );
}

Result MenuApplication_dispense_medicine(
    MenuApplication *application,
    const char *patient_id,
    const char *prescription_id,
    const char *medicine_id,
    int quantity,
    const char *pharmacist_id,
    const char *dispensed_at,
    char *buffer,
    size_t capacity
) {
    DispenseRecord record;
    int stock = 0;
    char dispense_detail[128];
    Result result = PharmacyService_dispense_medicine_for_patient(
        &application->pharmacy_service,
        patient_id,
        prescription_id,
        medicine_id,
        quantity,
        pharmacist_id,
        dispensed_at,
        &record
    );

    if (result.success == 0) {
        return MenuApplication_write_failure(result.message, buffer, capacity);
    }

    snprintf(
        dispense_detail,
        sizeof(dispense_detail),
        "patient=%s medicine=%s qty=%d",
        record.patient_id,
        record.medicine_id,
        record.quantity
    );
    (void)AuditService_record(
        &application->audit_service,
        AUDIT_EVENT_DISPENSE,
        MenuApplication_audit_actor_id(application),
        MenuApplication_audit_actor_role(application),
        "PRESCRIPTION",
        record.prescription_id,
        "OK",
        dispense_detail
    );

    result = PharmacyService_get_stock(&application->pharmacy_service, medicine_id, &stock);
    if (result.success == 0) {
        return MenuApplication_write_failure(result.message, buffer, capacity);
    }

    return MenuApplication_write_text(
        buffer,
        capacity,
        "发药完成: 患者=%s | 处方=%s | 药品=%s | 数量=%d | 当前库存=%d",
        record.patient_id,
        record.prescription_id,
        record.medicine_id,
        record.quantity,
        stock
    );
}

Result MenuApplication_create_prescription(
    MenuApplication *application,
    const Prescription *prescription,
    char *buffer,
    size_t capacity
) {
    VisitRecord visit;
    Medicine medicine;
    Result result;

    if (application == 0 || prescription == 0) {
        return MenuApplication_write_failure("arguments missing", buffer, capacity);
    }

    /* 校验就诊记录是否存在 */
    result = VisitRecordRepository_find_by_visit_id(
        &application->medical_record_service.visit_repository,
        prescription->visit_id,
        &visit
    );
    if (result.success == 0) {
        return MenuApplication_write_failure("visit not found", buffer, capacity);
    }

    /* 校验药品是否存在 */
    result = MedicineRepository_find_by_medicine_id(
        &application->pharmacy_service.medicine_repository,
        prescription->medicine_id,
        &medicine
    );
    if (result.success == 0) {
        return MenuApplication_write_failure("medicine not found", buffer, capacity);
    }

    /* 检查药品库存 */
    if (medicine.stock < prescription->quantity) {
        return MenuApplication_write_failure("medicine stock insufficient", buffer, capacity);
    }

    /* 创建处方 */
    result = PrescriptionService_create_prescription(
        &application->prescription_service,
        prescription
    );
    if (result.success == 0) {
        return MenuApplication_write_failure(result.message, buffer, capacity);
    }

    return MenuApplication_write_text(
        buffer,
        capacity,
        "处方创建成功: 处方ID=%s | 就诊ID=%s | 药品ID=%s | 数量=%d | 用法=%s",
        prescription->prescription_id,
        prescription->visit_id,
        prescription->medicine_id,
        prescription->quantity,
        prescription->usage
    );
}

Result MenuApplication_query_prescriptions_by_visit(
    MenuApplication *application,
    const char *visit_id,
    char *buffer,
    size_t capacity
) {
    LinkedList prescriptions;
    const LinkedListNode *current = 0;
    size_t used = 0;
    Result result;

    if (application == 0) {
        return MenuApplication_write_failure("arguments missing", buffer, capacity);
    }

    LinkedList_init(&prescriptions);
    result = PrescriptionService_find_by_visit_id(
        &application->prescription_service,
        visit_id,
        &prescriptions
    );
    if (result.success == 0) {
        return MenuApplication_write_failure(result.message, buffer, capacity);
    }

    result = MenuApplication_write_text(
        buffer,
        capacity,
        "就诊处方列表(%zu)",
        LinkedList_count(&prescriptions)
    );
    if (result.success == 0) {
        PrescriptionService_clear_results(&prescriptions);
        return result;
    }

    used = strlen(buffer);
    current = prescriptions.head;
    while (current != 0) {
        const Prescription *p = (const Prescription *)current->data;

        result = MenuApplication_append_text(
            buffer,
            capacity,
            &used,
            "\n%s | 药品=%s | 数量=%d | 用法=%s",
            p->prescription_id,
            p->medicine_id,
            p->quantity,
            p->usage
        );
        if (result.success == 0) {
            break;
        }

        current = current->next;
    }

    PrescriptionService_clear_results(&prescriptions);
    return Result_make_success("prescriptions listed");
}

Result MenuApplication_query_medicine_stock(
    MenuApplication *application,
    const char *medicine_id,
    char *buffer,
    size_t capacity
) {
    Medicine medicine;
    Result result = MedicineRepository_find_by_medicine_id(
        &application->pharmacy_service.medicine_repository,
        medicine_id,
        &medicine
    );

    if (result.success == 0) {
        return MenuApplication_write_failure(result.message, buffer, capacity);
    }

    if (medicine.alias[0] != '\0') {
        return MenuApplication_write_text(
            buffer,
            capacity,
            "药品库存: %s | %s (别名: %s) | %d",
            medicine.medicine_id,
            medicine.name,
            medicine.alias,
            medicine.stock
        );
    }

    return MenuApplication_write_text(
        buffer,
        capacity,
        "药品库存: %s | %s | %d",
        medicine.medicine_id,
        medicine.name,
        medicine.stock
    );
}

Result MenuApplication_find_low_stock_medicines(
    MenuApplication *application,
    char *buffer,
    size_t capacity
) {
    LinkedList medicines;
    const LinkedListNode *current = 0;
    size_t used = 0;
    Result result = PharmacyService_find_low_stock_medicines(
        &application->pharmacy_service,
        &medicines
    );

    if (result.success == 0) {
        return MenuApplication_write_failure(result.message, buffer, capacity);
    }

    result = MenuApplication_write_text(
        buffer,
        capacity,
        "低库存药品(%zu)",
        LinkedList_count(&medicines)
    );
    if (result.success == 0) {
        PharmacyService_clear_medicine_results(&medicines);
        return result;
    }

    used = strlen(buffer);
    current = medicines.head;
    while (current != 0) {
        const Medicine *medicine = (const Medicine *)current->data;

        if (medicine->alias[0] != '\0') {
            result = MenuApplication_append_text(
                buffer,
                capacity,
                &used,
                "\n%s | %s (别名: %s) | 库存=%d | 阈值=%d",
                medicine->medicine_id,
                medicine->name,
                medicine->alias,
                medicine->stock,
                medicine->low_stock_threshold
            );
        } else {
            result = MenuApplication_append_text(
                buffer,
                capacity,
                &used,
                "\n%s | %s | 库存=%d | 阈值=%d",
                medicine->medicine_id,
                medicine->name,
                medicine->stock,
                medicine->low_stock_threshold
            );
        }
        if (result.success == 0) {
            PharmacyService_clear_medicine_results(&medicines);
            return result;
        }

        current = current->next;
    }

    PharmacyService_clear_medicine_results(&medicines);
    return Result_make_success("low stock ready");
}

static Result MenuApplication_query_dispense_history_by_prescription_id(
    MenuApplication *application,
    const char *prescription_id,
    char *buffer,
    size_t capacity
) {
    LinkedList records;
    const LinkedListNode *current = 0;
    size_t used = 0;
    Result result;

    if (application == 0) {
        return Result_make_failure("menu dispense history application missing");
    }

    LinkedList_init(&records);
    result = PharmacyService_find_dispense_records_by_prescription_id(
        &application->pharmacy_service,
        prescription_id,
        &records
    );
    if (result.success == 0) {
        return MenuApplication_write_failure(result.message, buffer, capacity);
    }

    result = MenuApplication_write_text(
        buffer,
        capacity,
        "发药记录(%zu): 处方=%s",
        LinkedList_count(&records),
        prescription_id
    );
    if (result.success == 0) {
        PharmacyService_clear_dispense_record_results(&records);
        return result;
    }

    used = strlen(buffer);
    current = records.head;
    while (current != 0) {
        const DispenseRecord *record = (const DispenseRecord *)current->data;

        result = MenuApplication_append_text(
            buffer,
            capacity,
            &used,
            "\n%s | %s | 数量=%d | 发药人=%s | %s",
            record->dispense_id,
            record->medicine_id,
            record->quantity,
            record->pharmacist_id,
            record->dispensed_at
        );
        if (result.success == 0) {
            PharmacyService_clear_dispense_record_results(&records);
            return result;
        }

        current = current->next;
    }

    PharmacyService_clear_dispense_record_results(&records);
    return Result_make_success("dispense history ready");
}

Result MenuApplication_query_dispense_history_by_patient_id(
    MenuApplication *application,
    const char *patient_id,
    char *buffer,
    size_t capacity
) {
    LinkedList records;
    const LinkedListNode *current = 0;
    size_t used = 0;
    Result result;

    if (application == 0) {
        return Result_make_failure("menu dispense history application missing");
    }

    LinkedList_init(&records);
    result = PharmacyService_find_dispense_records_by_patient_id(
        &application->pharmacy_service,
        patient_id,
        &records
    );
    if (result.success == 0) {
        return MenuApplication_write_failure(result.message, buffer, capacity);
    }

    result = MenuApplication_write_text(
        buffer,
        capacity,
        "发药记录(%zu): 患者=%s",
        LinkedList_count(&records),
        patient_id
    );
    if (result.success == 0) {
        PharmacyService_clear_dispense_record_results(&records);
        return result;
    }

    used = strlen(buffer);
    current = records.head;
    while (current != 0) {
        const DispenseRecord *record = (const DispenseRecord *)current->data;

        result = MenuApplication_append_text(
            buffer,
            capacity,
            &used,
            "\n%s | 处方=%s | 药品=%s | 数量=%d | 发药人=%s | %s",
            record->dispense_id,
            record->prescription_id,
            record->medicine_id,
            record->quantity,
            record->pharmacist_id,
            record->dispensed_at
        );
        if (result.success == 0) {
            PharmacyService_clear_dispense_record_results(&records);
            return result;
        }

        current = current->next;
    }

    PharmacyService_clear_dispense_record_results(&records);
    return Result_make_success("dispense patient history ready");
}

/**
 * @brief 交互式读取一行文本输入
 *
 * 显示提示文本，等待用户输入一行。支持 ESC 取消和 EOF 检测。
 */
Result MenuApplication_prompt_line(
    MenuApplicationPromptContext *context,
    const char *prompt,
    char *buffer,
    size_t capacity
) {
    int read_result = 0;

    if (context == 0 || context->input == 0 || context->output == 0) {
        return Result_make_failure("prompt context invalid");
    }

    if (buffer == 0 || capacity == 0) {
        return Result_make_failure("prompt buffer invalid");
    }

    tui_print_prompt(context->output, prompt);
    fflush(context->output);

    read_result = InputHelper_read_line(context->input, buffer, capacity);

    if (read_result == -2) {
        return Result_make_failure(INPUT_HELPER_ESC_MESSAGE);
    }
    if (read_result == 0) {
        return Result_make_failure("input ended");
    }
    if (read_result < 0) {
        return Result_make_failure("input too long");
    }

    return Result_make_success("line read");
}

/** @brief 交互式读取一个整数值 */
Result MenuApplication_prompt_int(
    MenuApplicationPromptContext *context,
    const char *prompt,
    int *out_value
) {
    char buffer[64];
    char *end_pointer = 0;
    long value = 0;
    Result result = MenuApplication_prompt_line(context, prompt, buffer, sizeof(buffer));

    if (result.success == 0) {
        return result;
    }

    value = strtol(buffer, &end_pointer, 10);
    if (end_pointer == buffer || end_pointer == 0 || value < INT_MIN || value > INT_MAX) {
        return Result_make_failure("numeric input invalid");
    }

    while (*end_pointer != '\0') {
        if (!isspace((unsigned char)*end_pointer)) {
            return Result_make_failure("numeric input invalid");
        }

        end_pointer++;
    }

    *out_value = (int)value;
    return Result_make_success("number parsed");
}

/** @brief 忽略大小写比较两个字符是否相等 */
static int MenuApplication_char_equal_ignore_case(char left, char right) {
    return tolower((unsigned char)left) == tolower((unsigned char)right);
}

/** @brief 忽略大小写检查 text 中是否包含 query 子串 */
static int MenuApplication_text_contains_ignore_case(const char *text, const char *query) {
    size_t text_index = 0;
    size_t query_length = 0;
    size_t query_index = 0;

    if (query == 0 || query[0] == '\0') {
        return 1;
    }
    if (text == 0) {
        return 0;
    }

    query_length = strlen(query);
    while (text[text_index] != '\0') {
        for (query_index = 0; query_index < query_length; query_index++) {
            if (text[text_index + query_index] == '\0') {
                return 0;
            }
            if (!MenuApplication_char_equal_ignore_case(text[text_index + query_index], query[query_index])) {
                break;
            }
        }
        if (query_index == query_length) {
            return 1;
        }
        text_index++;
    }

    return 0;
}

/** @brief 在选项数组中精确匹配编号或标签 */
static int MenuApplication_find_exact_selection(
    const MenuApplicationSelectionOption *options,
    int option_count,
    const char *query
) {
    int index = 0;

    if (options == 0 || option_count <= 0 || query == 0 || query[0] == '\0') {
        return -1;
    }

    for (index = 0; index < option_count; index++) {
        if (strcmp(options[index].id, query) == 0 || strcmp(options[index].label, query) == 0) {
            return index;
        }
    }

    return -1;
}

/**
 * @brief 非交互式模糊搜索选择器（用于文件/管道输入）
 *
 * 支持两种选择方式：
 * 1. 直接输入编号或完整标签精确匹配
 * 2. 输入关键字模糊过滤，从候选列表中选择序号
 */
static Result MenuApplication_prompt_select_option_line(
    MenuApplicationPromptContext *context,
    const char *title,
    const MenuApplicationSelectionOption *options,
    int option_count,
    char *out_id,
    size_t out_id_capacity
) {
    char query[128];
    int filtered_indices[MENU_APPLICATION_SELECT_OPTION_MAX];
    int filtered_count = 0;
    int selected_number = 0;
    int selected_index = -1;
    int option_index = 0;
    Result result;

    result = MenuApplication_prompt_line(
        context,
        title,
        query,
        sizeof(query)
    );
    if (result.success == 0) {
        return result;
    }

    selected_index = MenuApplication_find_exact_selection(options, option_count, query);
    if (selected_index >= 0) {
        MenuApplication_copy_text(out_id, out_id_capacity, options[selected_index].id);
        fprintf(context->output, TUI_BOLD_GREEN " %s 已选择: " TUI_RESET "%s\n", TUI_CHECK, options[selected_index].label);
        return Result_make_success("selection chosen");
    }

    for (option_index = 0; option_index < option_count; option_index++) {
        if (MenuApplication_text_contains_ignore_case(options[option_index].label, query) ||
            MenuApplication_text_contains_ignore_case(options[option_index].id, query)) {
            filtered_indices[filtered_count] = option_index;
            filtered_count++;
        }
    }

    if (filtered_count == 0) {
        return Result_make_failure("selection candidates not found");
    }

    fprintf(context->output, TUI_BOLD_CYAN "\n候选列表:\n" TUI_RESET);
    for (option_index = 0; option_index < filtered_count; option_index++) {
        fprintf(
            context->output,
            "  " TUI_BOLD_YELLOW "%d" TUI_RESET ". %s\n",
            option_index + 1,
            options[filtered_indices[option_index]].label
        );
    }

    result = MenuApplication_prompt_int(context, "请选择序号: ", &selected_number);
    if (result.success == 0) {
        return result;
    }
    if (selected_number <= 0 || selected_number > filtered_count) {
        return Result_make_failure("selection index invalid");
    }

    selected_index = filtered_indices[selected_number - 1];
    MenuApplication_copy_text(out_id, out_id_capacity, options[selected_index].id);
    fprintf(context->output, TUI_BOLD_GREEN " %s 已选择: " TUI_RESET "%s\n", TUI_CHECK, options[selected_index].label);
    return Result_make_success("selection chosen");
}

/** @brief 最大可见行数 */
#define MENU_APP_SELECT_MAX_VISIBLE 15

/**
 * @brief 交互式实时过滤搜索选择器（终端模式）
 *
 * 使用 InputHelper_read_key 逐字符输入，实时过滤选项列表。
 * 支持方向键选择、左右键翻页、退格删除、回车确认、ESC 取消。
 */
static Result MenuApplication_prompt_select_option_interactive(
    MenuApplicationPromptContext *context,
    const char *title,
    const MenuApplicationSelectionOption *options,
    int option_count,
    char *out_id,
    size_t out_id_capacity
) {
    char query[128];
    int query_len = 0;
    int filtered_indices[MENU_APPLICATION_SELECT_OPTION_MAX];
    int filtered_count = 0;
    int selected = 0;      /* cursor position relative to visible window */
    int scroll_offset = 0; /* index of first visible item in filtered list */
    int prev_lines = 0;
    int option_index = 0;
    InputEvent ev;

    query[0] = '\0';

    /* Initial filter: show all */
    for (option_index = 0; option_index < option_count; option_index++) {
        filtered_indices[filtered_count++] = option_index;
    }

    for (;;) {
        int max_visible = MENU_APP_SELECT_MAX_VISIBLE;
        int remaining = filtered_count - scroll_offset;
        int visible = remaining < max_visible ? remaining : max_visible;
        int total_lines = 0;
        int i = 0;

        /* Move cursor up to overwrite previous output */
        if (prev_lines > 0) {
            fprintf(context->output, TUI_CURSOR_UP_FMT, prev_lines);
        }

        /* Line 1: title */
        fprintf(context->output, TUI_OC_MUTED "%s" TUI_RESET TUI_CLEAR_LINE_NL, title);
        total_lines++;

        /* Line 2: search input */
        fprintf(context->output,
                TUI_OC_ACCENT " > " TUI_RESET TUI_OC_TEXT "%s" TUI_RESET TUI_CLEAR_LINE_NL,
                query);
        total_lines++;

        /* Filtered results with scroll window */
        for (i = 0; i < visible; i++) {
            int idx = filtered_indices[scroll_offset + i];
            if (i == selected) {
                fprintf(context->output,
                        TUI_OC_BG_SELECT TUI_OC_ACCENT " > %s" TUI_RESET TUI_CLEAR_LINE_NL,
                        options[idx].label);
            } else {
                fprintf(context->output,
                        "   " TUI_OC_MUTED "%s" TUI_RESET TUI_CLEAR_LINE_NL,
                        options[idx].label);
            }
            total_lines++;
        }

        /* Page indicator */
        if (filtered_count > max_visible) {
            int page_end = scroll_offset + visible;
            fprintf(context->output,
                    TUI_OC_DIM "   [\xe7\xac\xac %d-%d \xe6\x9d\xa1 / \xe5\x85\xb1 %d \xe6\x9d\xa1]" TUI_RESET TUI_CLEAR_LINE_NL,
                    scroll_offset + 1, page_end, filtered_count);
            total_lines++;
        }

        /* Hint line */
        fprintf(context->output,
                TUI_OC_DIM "   \xe2\x86\x91\xe2\x86\x93 \xe5\xaf\xbc\xe8\x88\xaa  \xe2\x86\x90\xe2\x86\x92 \xe7\xbf\xbb\xe9\xa1\xb5  Enter \xe9\x80\x89\xe6\x8b\xa9  ESC \xe8\xbf\x94\xe5\x9b\x9e" TUI_RESET TUI_CLEAR_LINE_NL);
        total_lines++;

        /* Clear any leftover lines from previous render */
        if (prev_lines > total_lines) {
            for (i = 0; i < prev_lines - total_lines; i++) {
                fprintf(context->output, TUI_CLEAR_LINE_NL);
            }
            fprintf(context->output, TUI_CURSOR_UP_ONLY, prev_lines - total_lines);
        }

        prev_lines = total_lines;
        fflush(context->output);

        /* Read a key */
        ev = InputHelper_read_key(context->input);

        switch (ev.key) {
            case INPUT_KEY_ESC:
            case INPUT_KEY_CTRL_Q:
            case INPUT_KEY_CTRL_C:
                return Result_make_failure(INPUT_HELPER_ESC_MESSAGE);

            case INPUT_KEY_ENTER:
                if (filtered_count > 0 && selected < visible) {
                    int idx = filtered_indices[scroll_offset + selected];
                    MenuApplication_copy_text(out_id, out_id_capacity, options[idx].id);
                    fprintf(context->output,
                            TUI_BOLD_GREEN " %s \xe5\xb7\xb2\xe9\x80\x89\xe6\x8b\xa9: " TUI_RESET "%s\n",
                            TUI_CHECK, options[idx].label);
                    return Result_make_success("selection chosen");
                }
                break;

            case INPUT_KEY_UP:
                if (selected > 0) {
                    selected--;
                } else if (scroll_offset > 0) {
                    scroll_offset--;
                }
                break;

            case INPUT_KEY_DOWN:
                if (selected < visible - 1) {
                    selected++;
                } else if (scroll_offset + visible < filtered_count) {
                    scroll_offset++;
                }
                break;

            case INPUT_KEY_LEFT:
                /* Page Up */
                scroll_offset -= max_visible;
                if (scroll_offset < 0) scroll_offset = 0;
                selected = 0;
                break;

            case INPUT_KEY_RIGHT:
                /* Page Down */
                if (scroll_offset + max_visible < filtered_count) {
                    scroll_offset += max_visible;
                    selected = 0;
                }
                break;

            case INPUT_KEY_BACKSPACE:
                if (query_len > 0) {
                    query_len--;
                    query[query_len] = '\0';
                    filtered_count = 0;
                    selected = 0;
                    scroll_offset = 0;
                    for (option_index = 0; option_index < option_count; option_index++) {
                        if (query_len == 0 ||
                            MenuApplication_text_contains_ignore_case(options[option_index].label, query) ||
                            MenuApplication_text_contains_ignore_case(options[option_index].id, query)) {
                            filtered_indices[filtered_count++] = option_index;
                        }
                    }
                }
                break;

            case INPUT_KEY_CHAR:
                if (query_len < (int)sizeof(query) - 1) {
                    query[query_len++] = ev.ch;
                    query[query_len] = '\0';
                    filtered_count = 0;
                    selected = 0;
                    scroll_offset = 0;
                    for (option_index = 0; option_index < option_count; option_index++) {
                        if (MenuApplication_text_contains_ignore_case(options[option_index].label, query) ||
                            MenuApplication_text_contains_ignore_case(options[option_index].id, query)) {
                            filtered_indices[filtered_count++] = option_index;
                        }
                    }
                }
                break;

            case INPUT_KEY_NONE:
                return Result_make_failure("input ended");

            default:
                break;
        }
    }
}

/** @brief 交互式浏览模式最大可见行数 */
#define MENU_APP_BROWSE_MAX_VISIBLE 15

/**
 * @brief 交互式表格搜索浏览器（终端模式）
 *
 * 在表格上方显示搜索框，用户输入关键字实时过滤表格行。
 * 支持方向键导航、左右键翻页、Enter 查看详情、ESC 返回。
 */
static Result MenuApplication_interactive_browse_tty(
    MenuApplicationPromptContext *context,
    const char *title,
    const MenuApplicationSelectionOption *options,
    int option_count,
    char *out_id,
    size_t out_id_capacity
) {
    char query[128];
    int query_len = 0;
    int filtered_indices[MENU_APPLICATION_SELECT_OPTION_MAX];
    int filtered_count = 0;
    int selected = 0;      /* cursor position relative to visible window */
    int scroll_offset = 0; /* index of first visible item in filtered list */
    int prev_lines = 0;
    int option_index = 0;
    InputEvent ev;

    query[0] = '\0';

    /* Initial filter: show all */
    for (option_index = 0; option_index < option_count; option_index++) {
        filtered_indices[filtered_count++] = option_index;
    }

    for (;;) {
        int max_visible = MENU_APP_BROWSE_MAX_VISIBLE;
        int remaining = filtered_count - scroll_offset;
        int visible = remaining < max_visible ? remaining : max_visible;
        int total_lines = 0;
        int i = 0;

        /* Move cursor up to overwrite previous output */
        if (prev_lines > 0) {
            fprintf(context->output, TUI_CURSOR_UP_FMT, prev_lines);
        }

        /* Line 1: title */
        fprintf(context->output, TUI_OC_MUTED "%s" TUI_RESET TUI_CLEAR_LINE_NL, title);
        total_lines++;

        /* Line 2: search input */
        fprintf(context->output,
                TUI_OC_ACCENT " \xf0\x9f\x94\x8d \xe6\x90\x9c\xe7\xb4\xa2: " TUI_RESET TUI_OC_TEXT "%s" TUI_RESET TUI_CLEAR_LINE_NL,
                query);
        total_lines++;

        /* Line 3: result count / page indicator */
        if (filtered_count > max_visible) {
            int page_end = scroll_offset + visible;
            if (query_len > 0) {
                fprintf(context->output,
                        TUI_OC_DIM "   [\xe7\xac\xac %d-%d \xe6\x9d\xa1 / \xe5\x85\xb1 %d \xe6\x9d\xa1\xe7\xbb\x93\xe6\x9e\x9c, \xe5\xb7\xb2\xe8\xbf\x87\xe6\xbb\xa4 %d \xe6\x9d\xa1]" TUI_RESET TUI_CLEAR_LINE_NL,
                        scroll_offset + 1, page_end, filtered_count, option_count - filtered_count);
            } else {
                fprintf(context->output,
                        TUI_OC_DIM "   [\xe7\xac\xac %d-%d \xe6\x9d\xa1 / \xe5\x85\xb1 %d \xe6\x9d\xa1\xe8\xae\xb0\xe5\xbd\x95]" TUI_RESET TUI_CLEAR_LINE_NL,
                        scroll_offset + 1, page_end, filtered_count);
            }
        } else {
            if (query_len > 0) {
                fprintf(context->output,
                        TUI_OC_DIM "   \xe5\x85\xb1 %d \xe6\x9d\xa1\xe7\xbb\x93\xe6\x9e\x9c (\xe5\xb7\xb2\xe8\xbf\x87\xe6\xbb\xa4 %d \xe6\x9d\xa1)" TUI_RESET TUI_CLEAR_LINE_NL,
                        filtered_count, option_count - filtered_count);
            } else {
                fprintf(context->output,
                        TUI_OC_DIM "   \xe5\x85\xb1 %d \xe6\x9d\xa1\xe8\xae\xb0\xe5\xbd\x95" TUI_RESET TUI_CLEAR_LINE_NL,
                        option_count);
            }
        }
        total_lines++;

        /* Filtered results with scroll window */
        for (i = 0; i < visible; i++) {
            int idx = filtered_indices[scroll_offset + i];
            if (i == selected) {
                fprintf(context->output,
                        TUI_OC_BG_SELECT TUI_OC_ACCENT " > %s" TUI_RESET TUI_CLEAR_LINE_NL,
                        options[idx].label);
            } else {
                fprintf(context->output,
                        "   " TUI_OC_MUTED "%s" TUI_RESET TUI_CLEAR_LINE_NL,
                        options[idx].label);
            }
            total_lines++;
        }

        /* Hint line */
        fprintf(context->output,
                TUI_OC_DIM "   \xe2\x86\x91\xe2\x86\x93 \xe5\xaf\xbc\xe8\x88\xaa  \xe2\x86\x90\xe2\x86\x92 \xe7\xbf\xbb\xe9\xa1\xb5  Enter \xe6\x9f\xa5\xe7\x9c\x8b  ESC \xe8\xbf\x94\xe5\x9b\x9e" TUI_RESET TUI_CLEAR_LINE_NL);
        total_lines++;

        /* Clear any leftover lines from previous render */
        if (prev_lines > total_lines) {
            for (i = 0; i < prev_lines - total_lines; i++) {
                fprintf(context->output, TUI_CLEAR_LINE_NL);
            }
            fprintf(context->output, TUI_CURSOR_UP_ONLY, prev_lines - total_lines);
        }

        prev_lines = total_lines;
        fflush(context->output);

        /* Read a key */
        ev = InputHelper_read_key(context->input);

        switch (ev.key) {
            case INPUT_KEY_ESC:
            case INPUT_KEY_CTRL_Q:
            case INPUT_KEY_CTRL_C:
                return Result_make_failure(INPUT_HELPER_ESC_MESSAGE);

            case INPUT_KEY_ENTER:
                if (filtered_count > 0 && selected < visible) {
                    int idx = filtered_indices[scroll_offset + selected];
                    if (out_id != 0 && out_id_capacity > 0) {
                        MenuApplication_copy_text(out_id, out_id_capacity, options[idx].id);
                    }
                    fprintf(context->output,
                            TUI_BOLD_GREEN " %s \xe5\xb7\xb2\xe9\x80\x89\xe6\x8b\xa9: " TUI_RESET "%s\n",
                            TUI_CHECK, options[idx].label);
                    return Result_make_success("browse selection chosen");
                }
                break;

            case INPUT_KEY_UP:
                if (selected > 0) {
                    selected--;
                } else if (scroll_offset > 0) {
                    scroll_offset--;
                }
                break;

            case INPUT_KEY_DOWN:
                if (selected < visible - 1) {
                    selected++;
                } else if (scroll_offset + visible < filtered_count) {
                    scroll_offset++;
                }
                break;

            case INPUT_KEY_LEFT:
                /* Page Up */
                scroll_offset -= max_visible;
                if (scroll_offset < 0) scroll_offset = 0;
                selected = 0;
                break;

            case INPUT_KEY_RIGHT:
                /* Page Down */
                if (scroll_offset + max_visible < filtered_count) {
                    scroll_offset += max_visible;
                    selected = 0;
                }
                break;

            case INPUT_KEY_BACKSPACE:
                if (query_len > 0) {
                    query_len--;
                    query[query_len] = '\0';
                    filtered_count = 0;
                    selected = 0;
                    scroll_offset = 0;
                    for (option_index = 0; option_index < option_count; option_index++) {
                        if (query_len == 0 ||
                            MenuApplication_text_contains_ignore_case(options[option_index].label, query) ||
                            MenuApplication_text_contains_ignore_case(options[option_index].id, query)) {
                            filtered_indices[filtered_count++] = option_index;
                        }
                    }
                }
                break;

            case INPUT_KEY_CHAR:
                if (query_len < (int)sizeof(query) - 1) {
                    query[query_len++] = ev.ch;
                    query[query_len] = '\0';
                    filtered_count = 0;
                    selected = 0;
                    scroll_offset = 0;
                    for (option_index = 0; option_index < option_count; option_index++) {
                        if (MenuApplication_text_contains_ignore_case(options[option_index].label, query) ||
                            MenuApplication_text_contains_ignore_case(options[option_index].id, query)) {
                            filtered_indices[filtered_count++] = option_index;
                        }
                    }
                }
                break;

            case INPUT_KEY_NONE:
                return Result_make_failure("input ended");

            default:
                break;
        }
    }
}

/**
 * @brief 交互式表格搜索浏览器
 *
 * 在表格上方显示搜索框，用户输入关键字实时过滤表格行。
 * 支持方向键导航、Enter 查看详情、ESC 返回。
 *
 * @param context     输入输出上下文
 * @param title       表格标题
 * @param options     选项数组（id + label 用于搜索匹配）
 * @param option_count 选项数量
 * @param out_id      选中项的 ID 输出缓冲区（可为 NULL 表示仅浏览模式）
 * @param out_id_capacity 输出缓冲区容量
 * @return Result     success = 选中了一项, failure = ESC取消或仅浏览
 */
Result MenuApplication_interactive_browse(
    MenuApplicationPromptContext *context,
    const char *title,
    const MenuApplicationSelectionOption *options,
    int option_count,
    char *out_id,
    size_t out_id_capacity
) {
    int input_fd = -1;

    if (context == 0 || options == 0 || option_count <= 0) {
        return Result_make_failure("browse arguments invalid");
    }

    /* Detect whether input is a terminal */
    input_fd = fileno(context->input);
    if (input_fd >= 0 && MenuApp_isatty(input_fd)) {
        return MenuApplication_interactive_browse_tty(
            context, title, options, option_count, out_id, out_id_capacity);
    }

    /* Non-TTY fallback: always print all options first (for test visibility) */
    {
        int i = 0;
        fprintf(context->output, "%s\n", title);
        for (i = 0; i < option_count; i++) {
            fprintf(context->output, "  %s\n", options[i].label);
        }
    }

    /* If caller wants a selection, attempt line-based selection */
    if (out_id != 0 && out_id_capacity > 0) {
        return MenuApplication_prompt_select_option_line(
            context, title, options, option_count, out_id, out_id_capacity);
    }

    /* Browse-only mode: just printed everything above */
    return Result_make_failure("browse mode non-interactive");
}

/**
 * @brief 交互式模糊搜索选择器
 *
 * 在终端模式下使用实时过滤（逐字符输入 + 方向键导航），
 * 在非终端模式下（管道/文件）回退到行读取方式以保持测试兼容。
 */
static Result MenuApplication_prompt_select_option(
    MenuApplicationPromptContext *context,
    const char *title,
    const MenuApplicationSelectionOption *options,
    int option_count,
    char *out_id,
    size_t out_id_capacity
) {
    int input_fd = -1;

    if (context == 0 || options == 0 || option_count <= 0 || out_id == 0 || out_id_capacity == 0) {
        return Result_make_failure("selection arguments invalid");
    }

    /* Detect whether input is a terminal */
    input_fd = fileno(context->input);
    if (input_fd >= 0 && MenuApp_isatty(input_fd)) {
        return MenuApplication_prompt_select_option_interactive(
            context, title, options, option_count, out_id, out_id_capacity);
    }

    /* Fallback: line-based selection for pipes/files (tests) */
    return MenuApplication_prompt_select_option_line(
        context, title, options, option_count, out_id, out_id_capacity);
}

/** @brief 交互式选择患者（加载全部患者，通过模糊搜索选择） */
Result MenuApplication_prompt_select_patient(
    MenuApplication *application,
    MenuApplicationPromptContext *context,
    const char *prompt,
    char *out_patient_id,
    size_t out_patient_id_capacity
) {
    LinkedList patients;
    const LinkedListNode *current = 0;
    MenuApplicationSelectionOption options[MENU_APPLICATION_SELECT_OPTION_MAX];
    int option_count = 0;
    Result result;

    LinkedList_init(&patients);
    result = PatientRepository_load_all(&application->patient_service.repository, &patients);
    if (result.success == 0) {
        return result;
    }

    current = patients.head;
    while (current != 0 && option_count < MENU_APPLICATION_SELECT_OPTION_MAX) {
        const Patient *patient = (const Patient *)current->data;
        MenuApplication_copy_text(options[option_count].id, sizeof(options[option_count].id), patient->patient_id);
        snprintf(
            options[option_count].label,
            sizeof(options[option_count].label),
            "%s | %s | %d岁 | 联系=%s",
            patient->patient_id,
            patient->name,
            patient->age,
            patient->contact
        );
        option_count++;
        current = current->next;
    }

    result = MenuApplication_prompt_select_option(
        context,
        prompt,
        options,
        option_count,
        out_patient_id,
        out_patient_id_capacity
    );
    PatientRepository_clear_loaded_list(&patients);
    return result;
}

/** @brief 交互式选择科室 */
Result MenuApplication_prompt_select_department(
    MenuApplication *application,
    MenuApplicationPromptContext *context,
    const char *prompt,
    char *out_department_id,
    size_t out_department_id_capacity
) {
    LinkedList departments;
    const LinkedListNode *current = 0;
    MenuApplicationSelectionOption options[MENU_APPLICATION_SELECT_OPTION_MAX];
    int option_count = 0;
    Result result;

    LinkedList_init(&departments);
    result = DepartmentRepository_load_all(&application->doctor_service.department_repository, &departments);
    if (result.success == 0) {
        return result;
    }

    current = departments.head;
    while (current != 0 && option_count < MENU_APPLICATION_SELECT_OPTION_MAX) {
        const Department *department = (const Department *)current->data;
        MenuApplication_copy_text(options[option_count].id, sizeof(options[option_count].id), department->department_id);
        snprintf(
            options[option_count].label,
            sizeof(options[option_count].label),
            "%s | %s | %s",
            department->department_id,
            department->name,
            department->location
        );
        option_count++;
        current = current->next;
    }

    result = MenuApplication_prompt_select_option(
        context,
        prompt,
        options,
        option_count,
        out_department_id,
        out_department_id_capacity
    );
    DepartmentRepository_clear_list(&departments);
    return result;
}

/** @brief 交互式选择医生（可按科室过滤） */
Result MenuApplication_prompt_select_doctor(
    MenuApplication *application,
    MenuApplicationPromptContext *context,
    const char *prompt,
    const char *department_id_filter,
    char *out_doctor_id,
    size_t out_doctor_id_capacity
) {
    LinkedList doctors;
    const LinkedListNode *current = 0;
    MenuApplicationSelectionOption options[MENU_APPLICATION_SELECT_OPTION_MAX];
    int option_count = 0;
    Result result;

    LinkedList_init(&doctors);
    if (department_id_filter != 0 && department_id_filter[0] != '\0') {
        result = DoctorService_list_by_department(
            &application->doctor_service,
            department_id_filter,
            &doctors
        );
    } else {
        result = DoctorRepository_load_all(&application->doctor_service.doctor_repository, &doctors);
    }
    if (result.success == 0) {
        return result;
    }

    current = doctors.head;
    while (current != 0 && option_count < MENU_APPLICATION_SELECT_OPTION_MAX) {
        const Doctor *doctor = (const Doctor *)current->data;
        MenuApplication_copy_text(options[option_count].id, sizeof(options[option_count].id), doctor->doctor_id);
        snprintf(
            options[option_count].label,
            sizeof(options[option_count].label),
            "%s | %s | %s | %s",
            doctor->doctor_id,
            doctor->name,
            doctor->title,
            doctor->schedule
        );
        option_count++;
        current = current->next;
    }

    result = MenuApplication_prompt_select_option(
        context,
        prompt,
        options,
        option_count,
        out_doctor_id,
        out_doctor_id_capacity
    );
    DoctorService_clear_list(&doctors);
    return result;
}

/* ─── 可挂号时段选择 ───────────────────────────────────────────────── */

#define MENU_APP_SLOT_CAPACITY 5          /* 每个时段的号数上限 */
#define MENU_APP_SLOT_LOOKAHEAD_DAYS 7    /* 向后展开的天数 */

/** @brief 将 3 字母英文星期缩写映射为 Mon=0..Sun=6 */
static int MenuApplication_day_abbrev_index(const char *abbrev) {
    if (abbrev == 0) return -1;
    if (strncmp(abbrev, "Mon", 3) == 0) return 0;
    if (strncmp(abbrev, "Tue", 3) == 0) return 1;
    if (strncmp(abbrev, "Wed", 3) == 0) return 2;
    if (strncmp(abbrev, "Thu", 3) == 0) return 3;
    if (strncmp(abbrev, "Fri", 3) == 0) return 4;
    if (strncmp(abbrev, "Sat", 3) == 0) return 5;
    if (strncmp(abbrev, "Sun", 3) == 0) return 6;
    return -1;
}

/**
 * @brief 解析医生排班字符串
 *
 * 支持形如 "Mon-Fri AM" / "Tue-Sat" / "Mon-Sun" / "Wed-Sun PM" 的排班。
 * 未识别的排班默认视为"全周全时段"。
 */
static void MenuApplication_parse_schedule(
    const char *schedule,
    int *out_start_day,
    int *out_end_day,
    int *out_has_am,
    int *out_has_pm
) {
    const char *p = 0;
    int start = 0;
    int end = 6;
    int has_am = 1;
    int has_pm = 1;
    int first = -1;
    int second = -1;

    *out_start_day = 0;
    *out_end_day = 6;
    *out_has_am = 1;
    *out_has_pm = 1;
    if (schedule == 0 || schedule[0] == '\0') {
        return;
    }

    for (p = schedule; *p != '\0'; p++) {
        int d = MenuApplication_day_abbrev_index(p);
        if (d >= 0) {
            first = d;
            p += 3;
            break;
        }
    }
    if (first < 0) {
        return;
    }
    if (*p == '-') {
        int d = MenuApplication_day_abbrev_index(p + 1);
        if (d >= 0) {
            second = d;
        }
    }
    start = first;
    end = second >= 0 ? second : first;

    if (strstr(schedule, "AM") != 0 && strstr(schedule, "PM") == 0) {
        has_am = 1;
        has_pm = 0;
    } else if (strstr(schedule, "PM") != 0 && strstr(schedule, "AM") == 0) {
        has_am = 0;
        has_pm = 1;
    } else {
        has_am = 1;
        has_pm = 1;
    }

    *out_start_day = start;
    *out_end_day = end;
    *out_has_am = has_am;
    *out_has_pm = has_pm;
}

/** @brief 判断 day_mon0（Mon=0..Sun=6）是否落在 [start,end] 区间（支持跨周） */
static int MenuApplication_day_in_range(int day_mon0, int start, int end) {
    if (start <= end) {
        return day_mon0 >= start && day_mon0 <= end;
    }
    return day_mon0 >= start || day_mon0 <= end;
}

/** @brief 交互式选择可挂号时段 */
Result MenuApplication_prompt_select_registration_slot(
    MenuApplication *application,
    MenuApplicationPromptContext *context,
    const char *prompt,
    const char *doctor_id,
    char *out_slot_time,
    size_t out_slot_time_capacity
) {
    static const char *am_times[] = {"09:00", "09:30", "10:00", "10:30", "11:00"};
    static const char *pm_times[] = {"14:00", "14:30", "15:00", "15:30", "16:00"};
    static const char *week_labels[] = {
        "\xe5\x91\xa8\xe4\xb8\x80", /* 周一 */
        "\xe5\x91\xa8\xe4\xba\x8c", /* 周二 */
        "\xe5\x91\xa8\xe4\xb8\x89", /* 周三 */
        "\xe5\x91\xa8\xe5\x9b\x9b", /* 周四 */
        "\xe5\x91\xa8\xe4\xba\x94", /* 周五 */
        "\xe5\x91\xa8\xe5\x85\xad", /* 周六 */
        "\xe5\x91\xa8\xe6\x97\xa5"  /* 周日 */
    };
    const int times_per_period = 5;
    Doctor doctor;
    MenuApplicationSelectionOption options[MENU_APPLICATION_SELECT_OPTION_MAX];
    char slot_times[MENU_APPLICATION_SELECT_OPTION_MAX][HIS_DOMAIN_TIME_CAPACITY];
    int option_count = 0;
    LinkedList registrations;
    int start_day = 0;
    int end_day = 6;
    int has_am = 1;
    int has_pm = 1;
    time_t now_t = (time_t)0;
    struct tm now_tm;
    int d = 0;
    int period = 0;
    int t_index = 0;
    char selected_id[HIS_DOMAIN_ID_CAPACITY];
    Result result;

    if (application == 0 || context == 0 || doctor_id == 0 ||
        out_slot_time == 0 || out_slot_time_capacity == 0) {
        return Result_make_failure("slot selection arguments invalid");
    }

    memset(&doctor, 0, sizeof(doctor));
    memset(&now_tm, 0, sizeof(now_tm));
    LinkedList_init(&registrations);

    result = DoctorRepository_find_by_doctor_id(
        &application->doctor_service.doctor_repository,
        doctor_id,
        &doctor
    );
    if (result.success == 0) {
        return Result_make_failure("doctor not found");
    }

    MenuApplication_parse_schedule(doctor.schedule, &start_day, &end_day, &has_am, &has_pm);

    now_t = time(0);
    if (now_t == (time_t)-1 || !MenuApplication_localtime_safe(&now_t, &now_tm)) {
        return Result_make_failure("current time unavailable");
    }

    /* 一次性加载该医生的全部挂号记录，逐时段统计未取消号数 */
    result = RegistrationService_find_by_doctor_id(
        &application->registration_service,
        doctor_id,
        0,
        &registrations
    );
    if (result.success == 0) {
        LinkedList_init(&registrations);
    }

    for (d = 0; d < MENU_APP_SLOT_LOOKAHEAD_DAYS &&
                option_count < MENU_APPLICATION_SELECT_OPTION_MAX; d++) {
        struct tm day_tm;
        time_t day_t = (time_t)0;
        int day_mon0 = 0;

        day_tm = now_tm;
        day_tm.tm_mday = now_tm.tm_mday + d;
        day_tm.tm_hour = 0;
        day_tm.tm_min = 0;
        day_tm.tm_sec = 0;
        day_tm.tm_isdst = -1;
        day_t = mktime(&day_tm);
        if (day_t == (time_t)-1) {
            continue;
        }
        day_mon0 = (day_tm.tm_wday + 6) % 7;
        if (!MenuApplication_day_in_range(day_mon0, start_day, end_day)) {
            continue;
        }

        for (period = 0; period < 2 &&
                         option_count < MENU_APPLICATION_SELECT_OPTION_MAX; period++) {
            const char **times_array = period == 0 ? am_times : pm_times;
            const char *period_label = period == 0
                ? "\xe4\xb8\x8a\xe5\x8d\x88"  /* 上午 */
                : "\xe4\xb8\x8b\xe5\x8d\x88"; /* 下午 */
            if ((period == 0 && !has_am) || (period == 1 && !has_pm)) {
                continue;
            }
            for (t_index = 0; t_index < times_per_period &&
                              option_count < MENU_APPLICATION_SELECT_OPTION_MAX; t_index++) {
                char slot_time[HIS_DOMAIN_TIME_CAPACITY];
                const LinkedListNode *current = 0;
                int used_count = 0;
                int remaining = 0;
                time_t slot_t = (time_t)0;

                snprintf(slot_time, sizeof(slot_time), "%04d-%02d-%02d %s",
                         day_tm.tm_year + 1900,
                         day_tm.tm_mon + 1,
                         day_tm.tm_mday,
                         times_array[t_index]);

                /* 当天已过时的时段不显示 */
                if (d == 0 && MenuApplication_parse_registration_time(slot_time, &slot_t)) {
                    if (difftime(slot_t, now_t) < 0.0) {
                        continue;
                    }
                }

                current = registrations.head;
                while (current != 0) {
                    const Registration *r = (const Registration *)current->data;
                    if (r != 0 &&
                        r->status != REG_STATUS_CANCELLED &&
                        strcmp(r->registered_at, slot_time) == 0) {
                        used_count++;
                    }
                    current = current->next;
                }
                remaining = MENU_APP_SLOT_CAPACITY - used_count;
                if (remaining <= 0) {
                    continue;
                }

                MenuApplication_copy_text(
                    slot_times[option_count],
                    sizeof(slot_times[option_count]),
                    slot_time
                );
                snprintf(options[option_count].id,
                         sizeof(options[option_count].id),
                         "S%d", option_count);
                snprintf(options[option_count].label,
                         sizeof(options[option_count].label),
                         "%s (%s %s) | \xe4\xbd\x99 %d/%d",
                         slot_time,
                         week_labels[day_mon0],
                         period_label,
                         remaining,
                         MENU_APP_SLOT_CAPACITY);
                option_count++;
            }
        }
    }

    RegistrationRepository_clear_list(&registrations);

    if (option_count == 0) {
        return Result_make_failure("no available registration slots in next 7 days");
    }

    result = MenuApplication_prompt_select_option(
        context,
        prompt,
        options,
        option_count,
        selected_id,
        sizeof(selected_id)
    );
    if (result.success == 0) {
        return result;
    }

    if (selected_id[0] == 'S') {
        int index = atoi(selected_id + 1);
        if (index >= 0 && index < option_count) {
            MenuApplication_copy_text(out_slot_time, out_slot_time_capacity, slot_times[index]);
            return Result_make_success("slot selected");
        }
    }
    return Result_make_failure("invalid slot selection");
}

/** @brief 交互式选择挂号记录（可按患者、医生、状态过滤） */
Result MenuApplication_prompt_select_registration(
    MenuApplication *application,
    MenuApplicationPromptContext *context,
    const char *prompt,
    const char *patient_id_filter,
    const char *doctor_id_filter,
    int status_filter,
    int apply_status_filter,
    char *out_registration_id,
    size_t out_registration_id_capacity
) {
    LinkedList registrations;
    const LinkedListNode *current = 0;
    MenuApplicationSelectionOption options[MENU_APPLICATION_SELECT_OPTION_MAX];
    int option_count = 0;
    Result result;

    LinkedList_init(&registrations);
    result = RegistrationRepository_load_all(&application->registration_repository, &registrations);
    if (result.success == 0) {
        return result;
    }

    current = registrations.head;
    while (current != 0 && option_count < MENU_APPLICATION_SELECT_OPTION_MAX) {
        const Registration *registration = (const Registration *)current->data;
        if (patient_id_filter != 0 && patient_id_filter[0] != '\0' &&
            strcmp(registration->patient_id, patient_id_filter) != 0) {
            current = current->next;
            continue;
        }
        if (doctor_id_filter != 0 && doctor_id_filter[0] != '\0' &&
            strcmp(registration->doctor_id, doctor_id_filter) != 0) {
            current = current->next;
            continue;
        }
        if (apply_status_filter != 0 && registration->status != status_filter) {
            current = current->next;
            continue;
        }

        MenuApplication_copy_text(
            options[option_count].id,
            sizeof(options[option_count].id),
            registration->registration_id
        );
        snprintf(
            options[option_count].label,
            sizeof(options[option_count].label),
            "%s | 患者=%s | 医生=%s | 科室=%s | 时间=%s | 状态=%s | 类型=%s | 费用=%.2f元",
            registration->registration_id,
            registration->patient_id,
            registration->doctor_id,
            registration->department_id,
            registration->registered_at,
            MenuApplication_registration_status_label(registration->status),
            MenuApplication_registration_type_label(registration->registration_type),
            registration->registration_fee
        );
        option_count++;
        current = current->next;
    }

    result = MenuApplication_prompt_select_option(
        context,
        prompt,
        options,
        option_count,
        out_registration_id,
        out_registration_id_capacity
    );
    RegistrationRepository_clear_list(&registrations);
    return result;
}

/** @brief 交互式选择就诊记录（可按患者、医生过滤） */
Result MenuApplication_prompt_select_visit(
    MenuApplication *application,
    MenuApplicationPromptContext *context,
    const char *prompt,
    const char *patient_id_filter,
    const char *doctor_id_filter,
    char *out_visit_id,
    size_t out_visit_id_capacity
) {
    LinkedList visits;
    const LinkedListNode *current = 0;
    MenuApplicationSelectionOption options[MENU_APPLICATION_SELECT_OPTION_MAX];
    int option_count = 0;
    Result result;

    LinkedList_init(&visits);
    result = VisitRecordRepository_load_all(
        &application->medical_record_service.visit_repository,
        &visits
    );
    if (result.success == 0) {
        return result;
    }

    current = visits.head;
    while (current != 0 && option_count < MENU_APPLICATION_SELECT_OPTION_MAX) {
        const VisitRecord *visit = (const VisitRecord *)current->data;
        if (patient_id_filter != 0 && patient_id_filter[0] != '\0' &&
            strcmp(visit->patient_id, patient_id_filter) != 0) {
            current = current->next;
            continue;
        }
        if (doctor_id_filter != 0 && doctor_id_filter[0] != '\0' &&
            strcmp(visit->doctor_id, doctor_id_filter) != 0) {
            current = current->next;
            continue;
        }

        MenuApplication_copy_text(options[option_count].id, sizeof(options[option_count].id), visit->visit_id);
        snprintf(
            options[option_count].label,
            sizeof(options[option_count].label),
            "%s | 挂号=%s | 患者=%s | 诊断=%s | 时间=%s",
            visit->visit_id,
            visit->registration_id,
            visit->patient_id,
            visit->diagnosis,
            visit->visit_time
        );
        option_count++;
        current = current->next;
    }

    result = MenuApplication_prompt_select_option(
        context,
        prompt,
        options,
        option_count,
        out_visit_id,
        out_visit_id_capacity
    );
    VisitRecordRepository_clear_list(&visits);
    return result;
}

/** @brief 交互式选择检查记录（可按医生过滤、仅显示待完成） */
Result MenuApplication_prompt_select_examination(
    MenuApplication *application,
    MenuApplicationPromptContext *context,
    const char *prompt,
    const char *doctor_id_filter,
    int pending_only,
    char *out_examination_id,
    size_t out_examination_id_capacity
) {
    LinkedList records;
    const LinkedListNode *current = 0;
    MenuApplicationSelectionOption options[MENU_APPLICATION_SELECT_OPTION_MAX];
    int option_count = 0;
    Result result;

    LinkedList_init(&records);
    result = ExaminationRecordRepository_load_all(
        &application->medical_record_service.examination_repository,
        &records
    );
    if (result.success == 0) {
        return result;
    }

    current = records.head;
    while (current != 0 && option_count < MENU_APPLICATION_SELECT_OPTION_MAX) {
        const ExaminationRecord *record = (const ExaminationRecord *)current->data;
        if (doctor_id_filter != 0 && doctor_id_filter[0] != '\0' &&
            strcmp(record->doctor_id, doctor_id_filter) != 0) {
            current = current->next;
            continue;
        }
        if (pending_only != 0 && record->status != EXAM_STATUS_PENDING) {
            current = current->next;
            continue;
        }

        MenuApplication_copy_text(
            options[option_count].id,
            sizeof(options[option_count].id),
            record->examination_id
        );
        snprintf(
            options[option_count].label,
            sizeof(options[option_count].label),
            "%s | 就诊=%s | 患者=%s | 项目=%s | 费用=%.2f元 | 状态=%s",
            record->examination_id,
            record->visit_id,
            record->patient_id,
            record->exam_item,
            record->exam_fee,
            record->status == EXAM_STATUS_COMPLETED ? "已完成" : "待完成"
        );
        option_count++;
        current = current->next;
    }

    result = MenuApplication_prompt_select_option(
        context,
        prompt,
        options,
        option_count,
        out_examination_id,
        out_examination_id_capacity
    );
    ExaminationRecordRepository_clear_list(&records);
    return result;
}

/** @brief 交互式选择病房 */
Result MenuApplication_prompt_select_ward(
    MenuApplication *application,
    MenuApplicationPromptContext *context,
    const char *prompt,
    char *out_ward_id,
    size_t out_ward_id_capacity
) {
    LinkedList wards;
    const LinkedListNode *current = 0;
    MenuApplicationSelectionOption options[MENU_APPLICATION_SELECT_OPTION_MAX];
    int option_count = 0;
    Result result;

    LinkedList_init(&wards);
    result = WardRepository_load_all(&application->inpatient_service.ward_repository, &wards);
    if (result.success == 0) {
        return result;
    }

    current = wards.head;
    while (current != 0 && option_count < MENU_APPLICATION_SELECT_OPTION_MAX) {
        const Ward *ward = (const Ward *)current->data;
        MenuApplication_copy_text(options[option_count].id, sizeof(options[option_count].id), ward->ward_id);
        snprintf(
            options[option_count].label,
            sizeof(options[option_count].label),
            "%s | %s | %s | 已占用=%d/%d",
            ward->ward_id,
            ward->name,
            ward->location,
            ward->occupied_beds,
            ward->capacity
        );
        option_count++;
        current = current->next;
    }

    result = MenuApplication_prompt_select_option(
        context,
        prompt,
        options,
        option_count,
        out_ward_id,
        out_ward_id_capacity
    );
    WardRepository_clear_loaded_list(&wards);
    return result;
}

/** @brief 交互式选择床位（可按病房过滤、仅空闲/仅占用） */
Result MenuApplication_prompt_select_bed(
    MenuApplication *application,
    MenuApplicationPromptContext *context,
    const char *prompt,
    const char *ward_id_filter,
    int available_only,
    int occupied_only,
    char *out_bed_id,
    size_t out_bed_id_capacity
) {
    LinkedList beds;
    const LinkedListNode *current = 0;
    MenuApplicationSelectionOption options[MENU_APPLICATION_SELECT_OPTION_MAX];
    int option_count = 0;
    Result result;

    LinkedList_init(&beds);
    result = BedRepository_load_all(&application->inpatient_service.bed_repository, &beds);
    if (result.success == 0) {
        return result;
    }

    current = beds.head;
    while (current != 0 && option_count < MENU_APPLICATION_SELECT_OPTION_MAX) {
        const Bed *bed = (const Bed *)current->data;
        if (ward_id_filter != 0 && ward_id_filter[0] != '\0' &&
            strcmp(bed->ward_id, ward_id_filter) != 0) {
            current = current->next;
            continue;
        }
        if (available_only != 0 && bed->status != BED_STATUS_AVAILABLE) {
            current = current->next;
            continue;
        }
        if (occupied_only != 0 && bed->status != BED_STATUS_OCCUPIED) {
            current = current->next;
            continue;
        }

        MenuApplication_copy_text(options[option_count].id, sizeof(options[option_count].id), bed->bed_id);
        snprintf(
            options[option_count].label,
            sizeof(options[option_count].label),
            "%s | 病区=%s | 房间=%s | 床位=%s | 状态=%s",
            bed->bed_id,
            bed->ward_id,
            bed->room_no,
            bed->bed_no,
            MenuApplication_bed_status_label(bed->status)
        );
        option_count++;
        current = current->next;
    }

    result = MenuApplication_prompt_select_option(
        context,
        prompt,
        options,
        option_count,
        out_bed_id,
        out_bed_id_capacity
    );
    BedRepository_clear_loaded_list(&beds);
    return result;
}

/** @brief 交互式选择住院记录（可按患者过滤、仅在院） */
Result MenuApplication_prompt_select_admission(
    MenuApplication *application,
    MenuApplicationPromptContext *context,
    const char *prompt,
    const char *patient_id_filter,
    int active_only,
    char *out_admission_id,
    size_t out_admission_id_capacity
) {
    LinkedList admissions;
    const LinkedListNode *current = 0;
    MenuApplicationSelectionOption options[MENU_APPLICATION_SELECT_OPTION_MAX];
    int option_count = 0;
    Result result;

    LinkedList_init(&admissions);
    result = AdmissionRepository_load_all(&application->inpatient_service.admission_repository, &admissions);
    if (result.success == 0) {
        return result;
    }

    current = admissions.head;
    while (current != 0 && option_count < MENU_APPLICATION_SELECT_OPTION_MAX) {
        const Admission *admission = (const Admission *)current->data;
        if (patient_id_filter != 0 && patient_id_filter[0] != '\0' &&
            strcmp(admission->patient_id, patient_id_filter) != 0) {
            current = current->next;
            continue;
        }
        if (active_only != 0 && admission->status != ADMISSION_STATUS_ACTIVE) {
            current = current->next;
            continue;
        }

        MenuApplication_copy_text(
            options[option_count].id,
            sizeof(options[option_count].id),
            admission->admission_id
        );
        snprintf(
            options[option_count].label,
            sizeof(options[option_count].label),
            "%s | 患者=%s | 病区=%s | 床位=%s | 状态=%s",
            admission->admission_id,
            admission->patient_id,
            admission->ward_id,
            admission->bed_id,
            MenuApplication_admission_status_label(admission->status)
        );
        option_count++;
        current = current->next;
    }

    result = MenuApplication_prompt_select_option(
        context,
        prompt,
        options,
        option_count,
        out_admission_id,
        out_admission_id_capacity
    );
    AdmissionRepository_clear_loaded_list(&admissions);
    return result;
}

/** @brief 交互式选择药品（可按科室过滤） */
Result MenuApplication_prompt_select_medicine(
    MenuApplication *application,
    MenuApplicationPromptContext *context,
    const char *prompt,
    const char *department_id_filter,
    char *out_medicine_id,
    size_t out_medicine_id_capacity
) {
    LinkedList medicines;
    const LinkedListNode *current = 0;
    MenuApplicationSelectionOption options[MENU_APPLICATION_SELECT_OPTION_MAX];
    int option_count = 0;
    Result result;

    LinkedList_init(&medicines);
    result = MedicineRepository_load_all(&application->pharmacy_service.medicine_repository, &medicines);
    if (result.success == 0) {
        return result;
    }

    current = medicines.head;
    while (current != 0 && option_count < MENU_APPLICATION_SELECT_OPTION_MAX) {
        const Medicine *medicine = (const Medicine *)current->data;
        if (department_id_filter != 0 && department_id_filter[0] != '\0' &&
            strcmp(medicine->department_id, department_id_filter) != 0) {
            current = current->next;
            continue;
        }

        MenuApplication_copy_text(options[option_count].id, sizeof(options[option_count].id), medicine->medicine_id);
        {
            char alias_part[128] = "";
            char category_part[128] = "";
            if (medicine->alias[0] != '\0') {
                snprintf(alias_part, sizeof(alias_part), " (别名: %s)", medicine->alias);
            }
            if (medicine->category[0] != '\0') {
                snprintf(category_part, sizeof(category_part), " [%s]", medicine->category);
            }
            snprintf(
                options[option_count].label,
                sizeof(options[option_count].label),
                "%s | %s%s%s | 库存=%d | 阈值=%d",
                medicine->medicine_id,
                medicine->name,
                alias_part,
                category_part,
                medicine->stock,
                medicine->low_stock_threshold
            );
        }
        option_count++;
        current = current->next;
    }

    result = MenuApplication_prompt_select_option(
        context,
        prompt,
        options,
        option_count,
        out_medicine_id,
        out_medicine_id_capacity
    );
    MedicineRepository_clear_list(&medicines);
    return result;
}

/** @brief 交互式采集患者信息表单（姓名、性别、年龄、联系方式等） */
Result MenuApplication_prompt_patient_form(
    MenuApplicationPromptContext *context,
    Patient *out_patient,
    int require_patient_id
) {
    FormPanel panel;
    int idx = 0;
    int is_edit;
    Result result;

    if (out_patient == 0) {
        return Result_make_failure("patient form missing");
    }

    /* 判断是新建还是编辑：如果 out_patient->name 非空则为编辑模式 */
    is_edit = (out_patient->name[0] != '\0') ? 1 : 0;

    memset(&panel, 0, sizeof(panel));
    if (!is_edit) {
        memset(out_patient, 0, sizeof(*out_patient));
        snprintf(panel.title, sizeof(panel.title), TUI_MEDICAL " \xe6\x82\xa3\xe8\x80\x85\xe4\xbf\xa1\xe6\x81\xaf\xe5\xbd\x95\xe5\x85\xa5");
    } else {
        snprintf(panel.title, sizeof(panel.title), TUI_MEDICAL " \xe4\xbf\xae\xe6\x94\xb9\xe6\x82\xa3\xe8\x80\x85\xe4\xbf\xa1\xe6\x81\xaf");
    }

    if (require_patient_id != 0) {
        snprintf(panel.fields[idx].label, FORM_LABEL_CAPACITY, "\xe6\x82\xa3\xe8\x80\x85\xe7\xbc\x96\xe5\x8f\xb7:");
        panel.fields[idx].is_required = 1;
        if (is_edit && out_patient->patient_id[0]) {
            strncpy(panel.fields[idx].value, out_patient->patient_id, FORM_VALUE_CAPACITY - 1);
            panel.fields[idx].is_filled = 1;
        }
        idx++;
    }

    snprintf(panel.fields[idx].label, FORM_LABEL_CAPACITY, "\xe5\xa7\x93\xe5\x90\x8d:");
    panel.fields[idx].is_required = 1;
    if (is_edit && out_patient->name[0]) {
        strncpy(panel.fields[idx].value, out_patient->name, FORM_VALUE_CAPACITY - 1);
        panel.fields[idx].is_filled = 1;
    }
    idx++;

    snprintf(panel.fields[idx].label, FORM_LABEL_CAPACITY, "\xe6\x80\xa7\xe5\x88\xab:");
    snprintf(panel.fields[idx].hint, FORM_HINT_CAPACITY, "0\xe6\x9c\xaa\xe7\x9f\xa5/1\xe7\x94\xb7/2\xe5\xa5\xb3");
    snprintf(panel.fields[idx].default_value, FORM_VALUE_CAPACITY, "0");
    if (is_edit) {
        snprintf(panel.fields[idx].value, FORM_VALUE_CAPACITY, "%d", (int)out_patient->gender);
        panel.fields[idx].is_filled = 1;
    }
    idx++;

    snprintf(panel.fields[idx].label, FORM_LABEL_CAPACITY, "\xe5\xb9\xb4\xe9\xbe\x84:");
    panel.fields[idx].is_required = 1;
    if (is_edit) {
        snprintf(panel.fields[idx].value, FORM_VALUE_CAPACITY, "%d", out_patient->age);
        panel.fields[idx].is_filled = 1;
    }
    idx++;

    snprintf(panel.fields[idx].label, FORM_LABEL_CAPACITY, "\xe8\x81\x94\xe7\xb3\xbb\xe6\x96\xb9\xe5\xbc\x8f:");
    panel.fields[idx].is_required = 1;
    if (is_edit && out_patient->contact[0]) {
        strncpy(panel.fields[idx].value, out_patient->contact, FORM_VALUE_CAPACITY - 1);
        panel.fields[idx].is_filled = 1;
    }
    idx++;

    snprintf(panel.fields[idx].label, FORM_LABEL_CAPACITY, "\xe8\xba\xab\xe4\xbb\xbd\xe8\xaf\x81\xe5\x8f\xb7:");
    panel.fields[idx].is_required = 1;
    if (is_edit && out_patient->id_card[0]) {
        strncpy(panel.fields[idx].value, out_patient->id_card, FORM_VALUE_CAPACITY - 1);
        panel.fields[idx].is_filled = 1;
    }
    idx++;

    snprintf(panel.fields[idx].label, FORM_LABEL_CAPACITY, "\xe8\xbf\x87\xe6\x95\x8f\xe5\x8f\xb2:");
    snprintf(panel.fields[idx].default_value, FORM_VALUE_CAPACITY, "\xe6\x97\xa0");
    if (is_edit && out_patient->allergy[0]) {
        strncpy(panel.fields[idx].value, out_patient->allergy, FORM_VALUE_CAPACITY - 1);
        panel.fields[idx].is_filled = 1;
    }
    idx++;

    snprintf(panel.fields[idx].label, FORM_LABEL_CAPACITY, "\xe7\x97\x85\xe5\x8f\xb2:");
    snprintf(panel.fields[idx].default_value, FORM_VALUE_CAPACITY, "\xe6\x97\xa0");
    if (is_edit && out_patient->medical_history[0]) {
        strncpy(panel.fields[idx].value, out_patient->medical_history, FORM_VALUE_CAPACITY - 1);
        panel.fields[idx].is_filled = 1;
    }
    idx++;

    snprintf(panel.fields[idx].label, FORM_LABEL_CAPACITY, "\xe6\x98\xaf\xe5\x90\xa6\xe4\xbd\x8f\xe9\x99\xa2:");
    snprintf(panel.fields[idx].hint, FORM_HINT_CAPACITY, "0\xe5\x90\xa6/1\xe6\x98\xaf");
    snprintf(panel.fields[idx].default_value, FORM_VALUE_CAPACITY, "0");
    if (is_edit) {
        snprintf(panel.fields[idx].value, FORM_VALUE_CAPACITY, "%d", out_patient->is_inpatient);
        panel.fields[idx].is_filled = 1;
    }
    idx++;

    snprintf(panel.fields[idx].label, FORM_LABEL_CAPACITY, "\xe5\xa4\x87\xe6\xb3\xa8:");
    snprintf(panel.fields[idx].default_value, FORM_VALUE_CAPACITY, "\xe6\x97\xa0");
    if (is_edit && out_patient->remarks[0]) {
        strncpy(panel.fields[idx].value, out_patient->remarks, FORM_VALUE_CAPACITY - 1);
        panel.fields[idx].is_filled = 1;
    }
    idx++;

    panel.field_count = idx;

    result = MenuApplication_form_dispatch(context, &panel);
    if (result.success == 0) {
        return result;
    }

    /* Parse form values into Patient struct */
    idx = 0;
    if (require_patient_id != 0) {
        strncpy(out_patient->patient_id, panel.fields[idx].value, sizeof(out_patient->patient_id) - 1);
        idx++;
    }
    strncpy(out_patient->name, panel.fields[idx].value, sizeof(out_patient->name) - 1);
    idx++;
    out_patient->gender = (PatientGender)atoi(panel.fields[idx].value);
    idx++;
    out_patient->age = atoi(panel.fields[idx].value);
    idx++;
    strncpy(out_patient->contact, panel.fields[idx].value, sizeof(out_patient->contact) - 1);
    idx++;
    strncpy(out_patient->id_card, panel.fields[idx].value, sizeof(out_patient->id_card) - 1);
    idx++;
    strncpy(out_patient->allergy, panel.fields[idx].value, sizeof(out_patient->allergy) - 1);
    idx++;
    strncpy(out_patient->medical_history, panel.fields[idx].value, sizeof(out_patient->medical_history) - 1);
    idx++;
    out_patient->is_inpatient = atoi(panel.fields[idx].value);
    idx++;
    strncpy(out_patient->remarks, panel.fields[idx].value, sizeof(out_patient->remarks) - 1);

    return Result_make_success("patient form ready");
}

/** @brief 交互式采集药品信息表单（名称、单价、库存、阈值等） */
Result MenuApplication_prompt_medicine_form(
    MenuApplicationPromptContext *context,
    Medicine *out_medicine,
    int require_medicine_id
) {
    FormPanel panel;
    int idx = 0;
    char *end_pointer = 0;
    Result result;

    if (out_medicine == 0) {
        return Result_make_failure("medicine form missing");
    }

    memset(&panel, 0, sizeof(panel));
    memset(out_medicine, 0, sizeof(*out_medicine));
    snprintf(panel.title, sizeof(panel.title), TUI_FLASK " \xe8\x8d\xaf\xe5\x93\x81\xe4\xbf\xa1\xe6\x81\xaf\xe5\xbd\x95\xe5\x85\xa5");

    if (require_medicine_id != 0) {
        snprintf(panel.fields[idx].label, FORM_LABEL_CAPACITY, "\xe8\x8d\xaf\xe5\x93\x81\xe7\xbc\x96\xe5\x8f\xb7:");
        panel.fields[idx].is_required = 1;
        idx++;
    }

    snprintf(panel.fields[idx].label, FORM_LABEL_CAPACITY, "\xe8\x8d\xaf\xe5\x93\x81\xe5\x90\x8d\xe7\xa7\xb0:");
    panel.fields[idx].is_required = 1;
    idx++;

    snprintf(panel.fields[idx].label, FORM_LABEL_CAPACITY, "\xe8\x8d\xaf\xe5\x93\x81\xe5\x88\xab\xe5\x90\x8d:");
    snprintf(panel.fields[idx].default_value, FORM_VALUE_CAPACITY, "\xe6\x97\xa0");
    idx++;

    snprintf(panel.fields[idx].label, FORM_LABEL_CAPACITY, "\xe8\x8d\xaf\xe5\x93\x81\xe5\x88\x86\xe7\xb1\xbb:");
    snprintf(panel.fields[idx].default_value, FORM_VALUE_CAPACITY, "\xe6\x97\xa0");
    idx++;

    snprintf(panel.fields[idx].label, FORM_LABEL_CAPACITY, "\xe5\x8d\x95\xe4\xbb\xb7:");
    snprintf(panel.fields[idx].hint, FORM_HINT_CAPACITY, "\xe5\x85\x83");
    panel.fields[idx].is_required = 1;
    idx++;

    snprintf(panel.fields[idx].label, FORM_LABEL_CAPACITY, "\xe5\xba\x93\xe5\xad\x98:");
    panel.fields[idx].is_required = 1;
    idx++;

    snprintf(panel.fields[idx].label, FORM_LABEL_CAPACITY, "\xe7\xa7\x91\xe5\xae\xa4\xe7\xbc\x96\xe5\x8f\xb7:");
    panel.fields[idx].is_required = 1;
    idx++;

    snprintf(panel.fields[idx].label, FORM_LABEL_CAPACITY, "\xe5\xba\x93\xe5\xad\x98\xe9\xa2\x84\xe8\xad\xa6:");
    panel.fields[idx].is_required = 1;
    idx++;

    panel.field_count = idx;

    result = MenuApplication_form_dispatch(context, &panel);
    if (result.success == 0) {
        return result;
    }

    /* Parse form values into Medicine struct */
    idx = 0;
    if (require_medicine_id != 0) {
        strncpy(out_medicine->medicine_id, panel.fields[idx].value, sizeof(out_medicine->medicine_id) - 1);
        idx++;
    }
    strncpy(out_medicine->name, panel.fields[idx].value, sizeof(out_medicine->name) - 1);
    idx++;
    strncpy(out_medicine->alias, panel.fields[idx].value, sizeof(out_medicine->alias) - 1);
    idx++;
    strncpy(out_medicine->category, panel.fields[idx].value, sizeof(out_medicine->category) - 1);
    idx++;
    out_medicine->price = strtod(panel.fields[idx].value, &end_pointer);
    if (end_pointer == panel.fields[idx].value || end_pointer == 0) {
        return Result_make_failure("medicine price invalid");
    }
    while (*end_pointer != '\0') {
        if (!isspace((unsigned char)*end_pointer)) {
            return Result_make_failure("medicine price invalid");
        }
        end_pointer++;
    }
    idx++;
    out_medicine->stock = atoi(panel.fields[idx].value);
    idx++;
    strncpy(out_medicine->department_id, panel.fields[idx].value, sizeof(out_medicine->department_id) - 1);
    idx++;
    out_medicine->low_stock_threshold = atoi(panel.fields[idx].value);

    return Result_make_success("medicine form ready");
}

/** @brief 交互式采集科室信息表单（名称、位置、描述） */
Result MenuApplication_prompt_department_form(
    MenuApplicationPromptContext *context,
    Department *out_department,
    int require_department_id
) {
    FormPanel panel;
    int idx = 0;
    Result result;

    if (out_department == 0) {
        return Result_make_failure("department form missing");
    }

    memset(&panel, 0, sizeof(panel));
    memset(out_department, 0, sizeof(*out_department));
    snprintf(panel.title, sizeof(panel.title), TUI_GEAR " \xe7\xa7\x91\xe5\xae\xa4\xe4\xbf\xa1\xe6\x81\xaf\xe5\xbd\x95\xe5\x85\xa5");

    if (require_department_id != 0) {
        snprintf(panel.fields[idx].label, FORM_LABEL_CAPACITY, "\xe7\xa7\x91\xe5\xae\xa4\xe7\xbc\x96\xe5\x8f\xb7:");
        panel.fields[idx].is_required = 1;
        idx++;
    }

    snprintf(panel.fields[idx].label, FORM_LABEL_CAPACITY, "\xe7\xa7\x91\xe5\xae\xa4\xe5\x90\x8d\xe7\xa7\xb0:");
    panel.fields[idx].is_required = 1;
    idx++;

    snprintf(panel.fields[idx].label, FORM_LABEL_CAPACITY, "\xe7\xa7\x91\xe5\xae\xa4\xe4\xbd\x8d\xe7\xbd\xae:");
    panel.fields[idx].is_required = 1;
    idx++;

    snprintf(panel.fields[idx].label, FORM_LABEL_CAPACITY, "\xe7\xa7\x91\xe5\xae\xa4\xe6\x8f\x8f\xe8\xbf\xb0:");
    snprintf(panel.fields[idx].default_value, FORM_VALUE_CAPACITY, "\xe6\x97\xa0");
    idx++;

    panel.field_count = idx;

    result = MenuApplication_form_dispatch(context, &panel);
    if (result.success == 0) {
        return result;
    }

    /* Parse form values into Department struct */
    idx = 0;
    if (require_department_id != 0) {
        strncpy(out_department->department_id, panel.fields[idx].value, sizeof(out_department->department_id) - 1);
        idx++;
    }
    strncpy(out_department->name, panel.fields[idx].value, sizeof(out_department->name) - 1);
    idx++;
    strncpy(out_department->location, panel.fields[idx].value, sizeof(out_department->location) - 1);
    idx++;
    strncpy(out_department->description, panel.fields[idx].value, sizeof(out_department->description) - 1);

    return Result_make_success("department form ready");
}

/** @brief 交互式采集医生信息表单（姓名、职称、科室、排班等） */
Result MenuApplication_prompt_doctor_form(
    MenuApplicationPromptContext *context,
    Doctor *out_doctor,
    int require_doctor_id
) {
    FormPanel panel;
    int idx = 0;
    Result result;

    if (out_doctor == 0) {
        return Result_make_failure("doctor form missing");
    }

    memset(&panel, 0, sizeof(panel));
    memset(out_doctor, 0, sizeof(*out_doctor));
    snprintf(panel.title, sizeof(panel.title), TUI_MEDICAL " \xe5\x8c\xbb\xe7\x94\x9f\xe4\xbf\xa1\xe6\x81\xaf\xe5\xbd\x95\xe5\x85\xa5");

    if (require_doctor_id != 0) {
        snprintf(panel.fields[idx].label, FORM_LABEL_CAPACITY, "\xe5\x8c\xbb\xe7\x94\x9f\xe5\xb7\xa5\xe5\x8f\xb7:");
        panel.fields[idx].is_required = 1;
        idx++;
    }

    snprintf(panel.fields[idx].label, FORM_LABEL_CAPACITY, "\xe5\xa7\x93\xe5\x90\x8d:");
    panel.fields[idx].is_required = 1;
    idx++;

    snprintf(panel.fields[idx].label, FORM_LABEL_CAPACITY, "\xe8\x81\x8c\xe7\xa7\xb0:");
    panel.fields[idx].is_required = 1;
    idx++;

    snprintf(panel.fields[idx].label, FORM_LABEL_CAPACITY, "\xe7\xa7\x91\xe5\xae\xa4\xe7\xbc\x96\xe5\x8f\xb7:");
    panel.fields[idx].is_required = 1;
    idx++;

    snprintf(panel.fields[idx].label, FORM_LABEL_CAPACITY, "\xe5\x87\xba\xe8\xaf\x8a\xe5\xae\x89\xe6\x8e\x92:");
    panel.fields[idx].is_required = 1;
    idx++;

    snprintf(panel.fields[idx].label, FORM_LABEL_CAPACITY, "\xe7\x8a\xb6\xe6\x80\x81:");
    snprintf(panel.fields[idx].hint, FORM_HINT_CAPACITY, "0\xe5\x81\x9c\xe7\x94\xa8/1\xe5\x90\xaf\xe7\x94\xa8");
    snprintf(panel.fields[idx].default_value, FORM_VALUE_CAPACITY, "1");
    idx++;

    panel.field_count = idx;

    result = MenuApplication_form_dispatch(context, &panel);
    if (result.success == 0) {
        return result;
    }

    /* Parse form values into Doctor struct */
    idx = 0;
    if (require_doctor_id != 0) {
        strncpy(out_doctor->doctor_id, panel.fields[idx].value, sizeof(out_doctor->doctor_id) - 1);
        idx++;
    }
    strncpy(out_doctor->name, panel.fields[idx].value, sizeof(out_doctor->name) - 1);
    idx++;
    strncpy(out_doctor->title, panel.fields[idx].value, sizeof(out_doctor->title) - 1);
    idx++;
    strncpy(out_doctor->department_id, panel.fields[idx].value, sizeof(out_doctor->department_id) - 1);
    idx++;
    strncpy(out_doctor->schedule, panel.fields[idx].value, sizeof(out_doctor->schedule) - 1);
    idx++;
    out_doctor->status = (DoctorStatus)atoi(panel.fields[idx].value);

    return Result_make_success("doctor form ready");
}

Result MenuApplication_delete_patient(
    MenuApplication *application,
    const char *patient_id,
    char *buffer,
    size_t capacity
) {
    Result result = PatientService_delete_patient(&application->patient_service, patient_id);

    if (result.success == 0) {
        return MenuApplication_write_failure(result.message, buffer, capacity);
    }

    return MenuApplication_write_text(buffer, capacity, "患者已删除: %s", patient_id);
}

Result MenuApplication_list_departments(
    MenuApplication *application,
    char *buffer,
    size_t capacity
) {
    LinkedList departments;
    const LinkedListNode *current = 0;
    size_t used = 0;
    Result result;

    if (application == 0) {
        return Result_make_failure("menu application missing");
    }

    result = DepartmentRepository_load_all(
        &application->doctor_service.department_repository,
        &departments
    );
    if (result.success == 0) {
        return MenuApplication_write_failure(result.message, buffer, capacity);
    }

    result = MenuApplication_write_text(
        buffer,
        capacity,
        "科室列表(%zu)",
        LinkedList_count(&departments)
    );
    if (result.success == 0) {
        DepartmentRepository_clear_list(&departments);
        return result;
    }

    used = strlen(buffer);
    current = departments.head;
    while (current != 0) {
        const Department *department = (const Department *)current->data;

        result = MenuApplication_append_text(
            buffer,
            capacity,
            &used,
            "\n%s | %s | 位置=%s | 描述=%s",
            department->department_id,
            department->name,
            department->location,
            department->description
        );
        if (result.success == 0) {
            DepartmentRepository_clear_list(&departments);
            return result;
        }

        current = current->next;
    }

    DepartmentRepository_clear_list(&departments);
    return Result_make_success("department list ready");
}

Result MenuApplication_add_department(
    MenuApplication *application,
    const Department *department,
    char *buffer,
    size_t capacity
) {
    DepartmentService department_service;
    Department stored_department;
    Result result;

    if (application == 0 || department == 0) {
        return Result_make_failure("department add arguments missing");
    }

    stored_department = *department;
    if (MenuApplication_is_blank(stored_department.department_id)) {
        result = MenuApplication_generate_department_id(
            application,
            stored_department.department_id,
            sizeof(stored_department.department_id)
        );
        if (result.success == 0) {
            return MenuApplication_write_failure(result.message, buffer, capacity);
        }
    }

    department_service.repository = application->doctor_service.department_repository;
    result = DepartmentService_add(&department_service, &stored_department);
    if (result.success == 0) {
        return MenuApplication_write_failure(result.message, buffer, capacity);
    }

    return MenuApplication_write_text(
        buffer,
        capacity,
        "科室已添加: %s | %s",
        stored_department.department_id,
        stored_department.name
    );
}

Result MenuApplication_update_department(
    MenuApplication *application,
    const Department *department,
    char *buffer,
    size_t capacity
) {
    DepartmentService department_service;
    Result result;

    if (application == 0 || department == 0) {
        return Result_make_failure("department update arguments missing");
    }

    department_service.repository = application->doctor_service.department_repository;
    result = DepartmentService_update(&department_service, department);
    if (result.success == 0) {
        return MenuApplication_write_failure(result.message, buffer, capacity);
    }

    return MenuApplication_write_text(
        buffer,
        capacity,
        "科室已更新: %s | %s",
        department->department_id,
        department->name
    );
}

Result MenuApplication_add_doctor(
    MenuApplication *application,
    const Doctor *doctor,
    char *buffer,
    size_t capacity
) {
    Doctor stored_doctor;
    Result result;

    if (application == 0 || doctor == 0) {
        return Result_make_failure("doctor add arguments missing");
    }

    stored_doctor = *doctor;
    if (MenuApplication_is_blank(stored_doctor.doctor_id)) {
        result = MenuApplication_generate_doctor_id(
            application,
            stored_doctor.doctor_id,
            sizeof(stored_doctor.doctor_id)
        );
        if (result.success == 0) {
            return MenuApplication_write_failure(result.message, buffer, capacity);
        }
    }

    result = DoctorService_add(&application->doctor_service, &stored_doctor);

    if (result.success == 0) {
        return MenuApplication_write_failure(result.message, buffer, capacity);
    }

    return MenuApplication_write_text(
        buffer,
        capacity,
        "医生已添加: %s | %s | 科室=%s",
        stored_doctor.doctor_id,
        stored_doctor.name,
        stored_doctor.department_id
    );
}

Result MenuApplication_query_doctor(
    MenuApplication *application,
    const char *doctor_id,
    char *buffer,
    size_t capacity
) {
    Doctor doctor;
    Result result = DoctorService_get_by_id(&application->doctor_service, doctor_id, &doctor);

    if (result.success == 0) {
        return MenuApplication_write_failure(result.message, buffer, capacity);
    }

    return MenuApplication_write_text(
        buffer,
        capacity,
        "医生信息: %s | %s | 职称=%s | 科室=%s | 排班=%s",
        doctor.doctor_id,
        doctor.name,
        doctor.title,
        doctor.department_id,
        doctor.schedule
    );
}

Result MenuApplication_list_doctors_by_department(
    MenuApplication *application,
    const char *department_id,
    char *buffer,
    size_t capacity
) {
    LinkedList doctors;
    const LinkedListNode *current = 0;
    size_t used = 0;
    Result result = DoctorService_list_by_department(
        &application->doctor_service,
        department_id,
        &doctors
    );

    if (result.success == 0) {
        return MenuApplication_write_failure(result.message, buffer, capacity);
    }

    result = MenuApplication_write_text(
        buffer,
        capacity,
        "科室医生列表: %s | count=%zu",
        department_id,
        LinkedList_count(&doctors)
    );
    if (result.success == 0) {
        DoctorService_clear_list(&doctors);
        return result;
    }

    used = strlen(buffer);
    current = doctors.head;
    while (current != 0) {
        const Doctor *doctor = (const Doctor *)current->data;

        result = MenuApplication_append_text(
            buffer,
            capacity,
            &used,
            "\n%s | %s | 职称=%s | 排班=%s",
            doctor->doctor_id,
            doctor->name,
            doctor->title,
            doctor->schedule
        );
        if (result.success == 0) {
            DoctorService_clear_list(&doctors);
            return result;
        }

        current = current->next;
    }

    DoctorService_clear_list(&doctors);
    return Result_make_success("department doctor list ready");
}

Result MenuApplication_query_records_by_time_range(
    MenuApplication *application,
    const char *time_from,
    const char *time_to,
    char *buffer,
    size_t capacity
) {
    MedicalRecordHistory history;
    Result result = MedicalRecordService_find_records_by_time_range(
        &application->medical_record_service,
        time_from,
        time_to,
        &history
    );

    if (result.success == 0) {
        return MenuApplication_write_failure(result.message, buffer, capacity);
    }

    result = MenuApplication_write_text(
        buffer,
        capacity,
        "时间范围记录: %s ~ %s | registrations=%zu | visits=%zu | examinations=%zu | admissions=%zu",
        time_from,
        time_to,
        LinkedList_count(&history.registrations),
        LinkedList_count(&history.visits),
        LinkedList_count(&history.examinations),
        LinkedList_count(&history.admissions)
    );
    MedicalRecordHistory_clear(&history);
    return result;
}

Result MenuApplication_query_medicine_detail(
    MenuApplication *application,
    const char *medicine_id,
    char *buffer,
    size_t capacity,
    int include_instruction_note
) {
    Medicine medicine;
    Result result = MedicineRepository_find_by_medicine_id(
        &application->pharmacy_service.medicine_repository,
        medicine_id,
        &medicine
    );

    if (result.success == 0) {
        return MenuApplication_write_failure(result.message, buffer, capacity);
    }

    {
        char alias_part[128] = "";
        char category_part[128] = "";
        if (medicine.alias[0] != '\0') {
            snprintf(alias_part, sizeof(alias_part), " (别名: %s)", medicine.alias);
        }
        if (medicine.category[0] != '\0') {
            snprintf(category_part, sizeof(category_part), " [%s]", medicine.category);
        }

        if (include_instruction_note != 0) {
            return MenuApplication_write_text(
                buffer,
                capacity,
                "药品信息: %s | %s%s%s | 单价=%.2f | 科室=%s | 当前系统未维护用法说明",
                medicine.medicine_id,
                medicine.name,
                alias_part,
                category_part,
                medicine.price,
                medicine.department_id
            );
        }

        return MenuApplication_write_text(
            buffer,
            capacity,
            "药品信息: %s | %s%s%s | 单价=%.2f | 库存=%d | 科室=%s",
            medicine.medicine_id,
            medicine.name,
            alias_part,
            category_part,
            medicine.price,
            medicine.stock,
            medicine.department_id
        );
    }
}

Result MenuApplication_discharge_check(
    MenuApplication *application,
    const char *admission_id,
    char *buffer,
    size_t capacity
) {
    Admission admission;
    Result result = InpatientService_find_by_id(
        &application->inpatient_service,
        admission_id,
        &admission
    );

    if (result.success == 0) {
        return MenuApplication_write_failure(result.message, buffer, capacity);
    }

    if (admission.status != ADMISSION_STATUS_ACTIVE) {
        return MenuApplication_write_failure("admission not active", buffer, capacity);
    }

    return MenuApplication_write_text(
        buffer,
        capacity,
        "出院前检查: %s | 患者=%s | 病房=%s | 床位=%s | 可办理出院",
        admission.admission_id,
        admission.patient_id,
        admission.ward_id,
        admission.bed_id
    );
}

/** @brief 以表格形式打印病房列表 */
void MenuApplication_print_ward_table(MenuApplication *application, FILE *out) {
    LinkedList wards;
    const LinkedListNode *current = 0;
    TuiTable table;

    LinkedList_init(&wards);
    if (BedService_list_wards(&application->bed_service, &wards).success == 0) {
        return;
    }

    tui_print_section(out, TUI_CIRCLE, "\xe7\x97\x85\xe6\x88\xbf\xe5\x88\x97\xe8\xa1\xa8");
    TuiTable_init(&table, 7);
    TuiTable_set_header(&table, 0, "\xe7\xbc\x96\xe5\x8f\xb7", 8);
    TuiTable_set_header(&table, 1, "\xe5\x90\x8d\xe7\xa7\xb0", 12);
    TuiTable_set_header(&table, 2, "\xe4\xbd\x8d\xe7\xbd\xae", 10);
    TuiTable_set_header(&table, 3, "\xe5\x8d\xa0\xe7\x94\xa8/\xe5\xae\xb9\xe9\x87\x8f", 10);
    TuiTable_set_header(&table, 4, "\xe7\xb1\xbb\xe5\x9e\x8b", 6);
    TuiTable_set_header(&table, 5, "\xe6\x97\xa5\xe8\xb4\xb9(\xe5\x85\x83)", 10);
    TuiTable_set_header(&table, 6, "\xe7\x8a\xb6\xe6\x80\x81", 6);
    TuiTable_print_top(&table, out);
    TuiTable_print_header_row(&table, out);
    TuiTable_print_separator(&table, out);

    current = wards.head;
    while (current != 0) {
        const Ward *ward = (const Ward *)current->data;
        char occ[32];
        char fee_str[32];
        const char *vals[7];
        const char *colors[7] = {TUI_CYAN, 0, TUI_DIM, 0, 0, 0, 0};
        snprintf(occ, sizeof(occ), "%d/%d", ward->occupied_beds, ward->capacity);
        snprintf(fee_str, sizeof(fee_str), "%.2f", ward->daily_fee);
        vals[0] = ward->ward_id;
        vals[1] = ward->name;
        vals[2] = ward->location;
        vals[3] = occ;
        vals[4] = MenuApplication_ward_type_label(ward->ward_type);
        vals[5] = fee_str;
        vals[6] = ward->status == WARD_STATUS_ACTIVE
            ? "\xe2\x9c\x93 \xe5\x90\xaf\xe7\x94\xa8"
            : "\xe2\x9c\x97 \xe5\x81\x9c\xe7\x94\xa8";
        colors[6] = ward->status == WARD_STATUS_ACTIVE ? TUI_GREEN : TUI_RED;
        TuiTable_print_row_colored(&table, out, vals, colors, 7);
        current = current->next;
    }
    TuiTable_print_bottom(&table, out);
    BedService_clear_wards(&wards);
}

/** @brief 以表格形式打印指定病房的床位列表 */
void MenuApplication_print_bed_table(MenuApplication *application, FILE *out, const char *ward_id) {
    LinkedList beds;
    const LinkedListNode *current = 0;
    TuiTable table;

    LinkedList_init(&beds);
    if (BedService_list_beds_by_ward(&application->bed_service, ward_id, &beds).success == 0) {
        return;
    }

    tui_print_section(out, TUI_DIAMOND, "\xe5\xba\x8a\xe4\xbd\x8d\xe7\x8a\xb6\xe6\x80\x81");
    TuiTable_init(&table, 4);
    TuiTable_set_header(&table, 0, "\xe5\xba\x8a\xe4\xbd\x8d\xe7\xbc\x96\xe5\x8f\xb7", 10);
    TuiTable_set_header(&table, 1, "\xe6\x88\xbf\xe9\x97\xb4\xe5\x8f\xb7", 8);
    TuiTable_set_header(&table, 2, "\xe5\xba\x8a\xe5\x8f\xb7", 6);
    TuiTable_set_header(&table, 3, "\xe7\x8a\xb6\xe6\x80\x81", 8);
    TuiTable_print_top(&table, out);
    TuiTable_print_header_row(&table, out);
    TuiTable_print_separator(&table, out);

    current = beds.head;
    while (current != 0) {
        const Bed *bed = (const Bed *)current->data;
        const char *vals[4];
        const char *colors[4] = {TUI_CYAN, 0, 0, 0};
        vals[0] = bed->bed_id;
        vals[1] = bed->room_no;
        vals[2] = bed->bed_no;
        if (bed->status == BED_STATUS_AVAILABLE) {
            vals[3] = "\xe2\x97\x8b \xe7\xa9\xba\xe9\x97\xb2";
            colors[3] = TUI_GREEN;
        } else if (bed->status == BED_STATUS_OCCUPIED) {
            vals[3] = "\xe2\x97\x89 \xe5\x8d\xa0\xe7\x94\xa8";
            colors[3] = TUI_RED;
        } else {
            vals[3] = "\xe2\x9a\x99 \xe7\xbb\xb4\xe6\x8a\xa4";
            colors[3] = TUI_YELLOW;
        }
        TuiTable_print_row_colored(&table, out, vals, colors, 4);
        current = current->next;
    }
    TuiTable_print_bottom(&table, out);
    BedService_clear_beds(&beds);
}

void MenuApplication_print_low_stock_table(MenuApplication *application, FILE *out) {
    LinkedList medicines;
    const LinkedListNode *current = 0;
    TuiTable table;

    LinkedList_init(&medicines);
    if (PharmacyService_find_low_stock_medicines(&application->pharmacy_service, &medicines).success == 0) {
        return;
    }

    tui_print_section(out, TUI_LIGHTNING, "\xe5\xba\x93\xe5\xad\x98\xe4\xb8\x8d\xe8\xb6\xb3\xe8\x8d\xaf\xe5\x93\x81");
    TuiTable_init(&table, 4);
    TuiTable_set_header(&table, 0, "\xe7\xbc\x96\xe5\x8f\xb7", 8);
    TuiTable_set_header(&table, 1, "\xe5\x90\x8d\xe7\xa7\xb0", 14);
    TuiTable_set_header(&table, 2, "\xe5\xba\x93\xe5\xad\x98", 6);
    TuiTable_set_header(&table, 3, "\xe9\x98\x88\xe5\x80\xbc", 6);
    TuiTable_set_colors(&table, TUI_BOLD_RED, TUI_BOLD_WHITE);
    TuiTable_print_top(&table, out);
    TuiTable_print_header_row(&table, out);
    TuiTable_print_separator(&table, out);

    current = medicines.head;
    while (current != 0) {
        const Medicine *m = (const Medicine *)current->data;
        char stock_s[16];
        char thresh_s[16];
        const char *vals[4];
        const char *colors[4] = {TUI_CYAN, 0, TUI_BOLD_RED, TUI_DIM};
        snprintf(stock_s, sizeof(stock_s), "%d", m->stock);
        snprintf(thresh_s, sizeof(thresh_s), "%d", m->low_stock_threshold);
        vals[0] = m->medicine_id;
        vals[1] = m->name;
        vals[2] = stock_s;
        vals[3] = thresh_s;
        TuiTable_print_row_colored(&table, out, vals, colors, 4);
        current = current->next;
    }
    TuiTable_print_bottom(&table, out);
    PharmacyService_clear_medicine_results(&medicines);
}

void MenuApplication_print_patient_card(MenuApplication *application, FILE *out, const char *patient_id) {
    Patient patient;
    char age_s[16];

    if (PatientService_find_patient_by_id(&application->patient_service, patient_id, &patient).success == 0) {
        return;
    }

    tui_print_section(out, TUI_HEART, "\xe6\x82\xa3\xe8\x80\x85\xe4\xbf\xa1\xe6\x81\xaf\xe5\x8d\xa1");
    tui_print_kv_colored(out, "\xe7\xbc\x96\xe5\x8f\xb7", patient.patient_id, TUI_BOLD_CYAN);
    tui_print_kv_colored(out, "\xe5\xa7\x93\xe5\x90\x8d", patient.name, TUI_BOLD_WHITE);
    tui_print_kv_colored(out, "\xe6\x80\xa7\xe5\x88\xab",
        patient.gender == PATIENT_GENDER_MALE ? "\xe7\x94\xb7" :
        patient.gender == PATIENT_GENDER_FEMALE ? "\xe5\xa5\xb3" : "\xe6\x9c\xaa\xe7\x9f\xa5",
        TUI_BOLD_YELLOW);
    snprintf(age_s, sizeof(age_s), "%d \xe5\xb2\x81", patient.age);
    tui_print_kv_colored(out, "\xe5\xb9\xb4\xe9\xbe\x84", age_s, TUI_RESET);
    tui_print_kv_colored(out, "\xe8\x81\x94\xe7\xb3\xbb\xe6\x96\xb9\xe5\xbc\x8f", patient.contact, TUI_RESET);
    tui_print_kv_colored(out, "\xe8\xba\xab\xe4\xbb\xbd\xe8\xaf\x81", patient.id_card, TUI_DIM);
    tui_print_kv_colored(out, "\xe4\xbd\x8f\xe9\x99\xa2\xe7\x8a\xb6\xe6\x80\x81",
        patient.is_inpatient
            ? "\xe2\x97\x89 \xe4\xbd\x8f\xe9\x99\xa2\xe4\xb8\xad"
            : "\xe2\x97\x8b \xe9\x97\xa8\xe8\xaf\x8a",
        patient.is_inpatient ? TUI_BOLD_RED : TUI_BOLD_GREEN);
}

void MenuApplication_print_pending_table(MenuApplication *application, FILE *out, const char *doctor_id) {
    RegistrationRepositoryFilter filter;
    LinkedList registrations;
    const LinkedListNode *current = 0;
    TuiTable table;
    char count_msg[64];

    LinkedList_init(&registrations);
    RegistrationRepositoryFilter_init(&filter);
    filter.use_status = 1;
    filter.status = REG_STATUS_PENDING;

    if (RegistrationService_find_by_doctor_id(
            &application->registration_service, doctor_id, &filter, &registrations).success == 0) {
        return;
    }

    tui_print_section(out, TUI_LOZENGE, "\xe5\xbe\x85\xe8\xaf\x8a\xe5\x88\x97\xe8\xa1\xa8");
    TuiTable_init(&table, 4);
    TuiTable_set_header(&table, 0, "\xe6\x8c\x82\xe5\x8f\xb7\xe7\xbc\x96\xe5\x8f\xb7", 10);
    TuiTable_set_header(&table, 1, "\xe6\x82\xa3\xe8\x80\x85\xe7\xbc\x96\xe5\x8f\xb7", 10);
    TuiTable_set_header(&table, 2, "\xe7\xa7\x91\xe5\xae\xa4\xe7\xbc\x96\xe5\x8f\xb7", 10);
    TuiTable_set_header(&table, 3, "\xe6\x8c\x82\xe5\x8f\xb7\xe6\x97\xb6\xe9\x97\xb4", 20);
    TuiTable_set_colors(&table, TUI_BOLD_GREEN, TUI_BOLD_WHITE);
    TuiTable_print_top(&table, out);
    TuiTable_print_header_row(&table, out);
    TuiTable_print_separator(&table, out);

    current = registrations.head;
    while (current != 0) {
        const Registration *r = (const Registration *)current->data;
        const char *vals[4] = {r->registration_id, r->patient_id, r->department_id, r->registered_at};
        const char *colors[4] = {TUI_CYAN, TUI_BOLD_YELLOW, TUI_DIM, TUI_DIM};
        TuiTable_print_row_colored(&table, out, vals, colors, 4);
        current = current->next;
    }
    TuiTable_print_bottom(&table, out);

    snprintf(count_msg, sizeof(count_msg),
        "\xe5\x85\xb1 %zu \xe4\xbd\x8d\xe6\x82\xa3\xe8\x80\x85\xe7\xad\x89\xe5\xbe\x85\xe5\xb0\xb1\xe8\xaf\x8a",
        LinkedList_count(&registrations));
    tui_print_info(out, count_msg);
    RegistrationRepository_clear_list(&registrations);
}

/* ─── Interactive browse functions ─────────────────────────────────── */

/** @brief 交互式病房浏览（搜索/过滤/选择） */
Result MenuApplication_browse_ward_table(
    MenuApplication *application,
    MenuApplicationPromptContext *context,
    char *out_ward_id,
    size_t out_id_capacity
) {
    LinkedList wards;
    const LinkedListNode *current = 0;
    MenuApplicationSelectionOption options[MENU_APPLICATION_SELECT_OPTION_MAX];
    int option_count = 0;
    Result result;

    LinkedList_init(&wards);
    result = BedService_list_wards(&application->bed_service, &wards);
    if (result.success == 0) {
        return result;
    }

    current = wards.head;
    while (current != 0 && option_count < MENU_APPLICATION_SELECT_OPTION_MAX) {
        const Ward *ward = (const Ward *)current->data;
        MenuApplication_copy_text(options[option_count].id, sizeof(options[option_count].id), ward->ward_id);
        snprintf(
            options[option_count].label,
            sizeof(options[option_count].label),
            "%s | %s | %s | %d/%d | %s | %.2f\xe5\x85\x83/\xe6\x97\xa5 | %s",
            ward->ward_id,
            ward->name,
            ward->location,
            ward->occupied_beds,
            ward->capacity,
            MenuApplication_ward_type_label(ward->ward_type),
            ward->daily_fee,
            ward->status == WARD_STATUS_ACTIVE ? "\xe2\x9c\x93 \xe5\x90\xaf\xe7\x94\xa8" : "\xe2\x9c\x97 \xe5\x81\x9c\xe7\x94\xa8"
        );
        option_count++;
        current = current->next;
    }

    result = MenuApplication_interactive_browse(
        context,
        "\xe7\x97\x85\xe6\x88\xbf\xe5\x88\x97\xe8\xa1\xa8 - \xe8\xbe\x93\xe5\x85\xa5\xe5\x85\xb3\xe9\x94\xae\xe5\xad\x97\xe6\x90\x9c\xe7\xb4\xa2",
        options,
        option_count,
        out_ward_id,
        out_id_capacity
    );
    BedService_clear_wards(&wards);
    return result;
}

/** @brief 交互式床位浏览（搜索/过滤/选择） */
Result MenuApplication_browse_bed_table(
    MenuApplication *application,
    MenuApplicationPromptContext *context,
    const char *ward_id,
    char *out_bed_id,
    size_t out_id_capacity
) {
    LinkedList beds;
    const LinkedListNode *current = 0;
    MenuApplicationSelectionOption options[MENU_APPLICATION_SELECT_OPTION_MAX];
    int option_count = 0;
    Result result;

    LinkedList_init(&beds);
    result = BedService_list_beds_by_ward(&application->bed_service, ward_id, &beds);
    if (result.success == 0) {
        return result;
    }

    current = beds.head;
    while (current != 0 && option_count < MENU_APPLICATION_SELECT_OPTION_MAX) {
        const Bed *bed = (const Bed *)current->data;
        const char *status_label = "";
        if (bed->status == BED_STATUS_AVAILABLE) {
            status_label = "\xe2\x97\x8b \xe7\xa9\xba\xe9\x97\xb2";
        } else if (bed->status == BED_STATUS_OCCUPIED) {
            status_label = "\xe2\x97\x89 \xe5\x8d\xa0\xe7\x94\xa8";
        } else {
            status_label = "\xe2\x9a\x99 \xe7\xbb\xb4\xe6\x8a\xa4";
        }
        MenuApplication_copy_text(options[option_count].id, sizeof(options[option_count].id), bed->bed_id);
        snprintf(
            options[option_count].label,
            sizeof(options[option_count].label),
            "%s | \xe6\x88\xbf%s | \xe5\xba\x8a%s | %s",
            bed->bed_id,
            bed->room_no,
            bed->bed_no,
            status_label
        );
        option_count++;
        current = current->next;
    }

    result = MenuApplication_interactive_browse(
        context,
        "\xe5\xba\x8a\xe4\xbd\x8d\xe5\x88\x97\xe8\xa1\xa8 - \xe8\xbe\x93\xe5\x85\xa5\xe5\x85\xb3\xe9\x94\xae\xe5\xad\x97\xe6\x90\x9c\xe7\xb4\xa2",
        options,
        option_count,
        out_bed_id,
        out_id_capacity
    );
    BedService_clear_beds(&beds);
    return result;
}

/** @brief 交互式低库存药品浏览（搜索/过滤/选择） */
Result MenuApplication_browse_low_stock_table(
    MenuApplication *application,
    MenuApplicationPromptContext *context,
    char *out_medicine_id,
    size_t out_id_capacity
) {
    LinkedList medicines;
    const LinkedListNode *current = 0;
    MenuApplicationSelectionOption options[MENU_APPLICATION_SELECT_OPTION_MAX];
    int option_count = 0;
    Result result;

    LinkedList_init(&medicines);
    result = PharmacyService_find_low_stock_medicines(&application->pharmacy_service, &medicines);
    if (result.success == 0) {
        return result;
    }

    current = medicines.head;
    while (current != 0 && option_count < MENU_APPLICATION_SELECT_OPTION_MAX) {
        const Medicine *m = (const Medicine *)current->data;
        MenuApplication_copy_text(options[option_count].id, sizeof(options[option_count].id), m->medicine_id);
        if (m->alias[0] != '\0') {
            snprintf(
                options[option_count].label,
                sizeof(options[option_count].label),
                "%s | %s (\xe5\x88\xab\xe5\x90\x8d: %s) | \xe5\xba\x93\xe5\xad\x98=%d | \xe9\x98\x88\xe5\x80\xbc=%d",
                m->medicine_id,
                m->name,
                m->alias,
                m->stock,
                m->low_stock_threshold
            );
        } else {
            snprintf(
                options[option_count].label,
                sizeof(options[option_count].label),
                "%s | %s | \xe5\xba\x93\xe5\xad\x98=%d | \xe9\x98\x88\xe5\x80\xbc=%d",
                m->medicine_id,
                m->name,
                m->stock,
                m->low_stock_threshold
            );
        }
        option_count++;
        current = current->next;
    }

    result = MenuApplication_interactive_browse(
        context,
        "\xe5\xba\x93\xe5\xad\x98\xe4\xb8\x8d\xe8\xb6\xb3\xe8\x8d\xaf\xe5\x93\x81 - \xe8\xbe\x93\xe5\x85\xa5\xe5\x85\xb3\xe9\x94\xae\xe5\xad\x97\xe6\x90\x9c\xe7\xb4\xa2",
        options,
        option_count,
        out_medicine_id,
        out_id_capacity
    );
    PharmacyService_clear_medicine_results(&medicines);
    return result;
}

/** @brief 交互式待诊列表浏览（搜索/过滤/选择） */
Result MenuApplication_browse_pending_table(
    MenuApplication *application,
    MenuApplicationPromptContext *context,
    const char *doctor_id,
    char *out_registration_id,
    size_t out_id_capacity
) {
    RegistrationRepositoryFilter filter;
    LinkedList registrations;
    const LinkedListNode *current = 0;
    MenuApplicationSelectionOption options[MENU_APPLICATION_SELECT_OPTION_MAX];
    int option_count = 0;
    Result result;

    LinkedList_init(&registrations);
    RegistrationRepositoryFilter_init(&filter);
    filter.use_status = 1;
    filter.status = REG_STATUS_PENDING;

    result = RegistrationService_find_by_doctor_id(
        &application->registration_service, doctor_id, &filter, &registrations);
    if (result.success == 0) {
        return result;
    }

    current = registrations.head;
    while (current != 0 && option_count < MENU_APPLICATION_SELECT_OPTION_MAX) {
        const Registration *r = (const Registration *)current->data;
        MenuApplication_copy_text(options[option_count].id, sizeof(options[option_count].id), r->registration_id);
        snprintf(
            options[option_count].label,
            sizeof(options[option_count].label),
            "%s | \xe6\x82\xa3\xe8\x80\x85=%s | \xe7\xa7\x91\xe5\xae\xa4=%s | %s",
            r->registration_id,
            r->patient_id,
            r->department_id,
            r->registered_at
        );
        option_count++;
        current = current->next;
    }

    result = MenuApplication_interactive_browse(
        context,
        "\xe5\xbe\x85\xe8\xaf\x8a\xe5\x88\x97\xe8\xa1\xa8 - \xe8\xbe\x93\xe5\x85\xa5\xe5\x85\xb3\xe9\x94\xae\xe5\xad\x97\xe6\x90\x9c\xe7\xb4\xa2",
        options,
        option_count,
        out_registration_id,
        out_id_capacity
    );
    RegistrationRepository_clear_list(&registrations);
    return result;
}

/** @brief 交互式患者历史记录浏览 */
Result MenuApplication_browse_patient_history(
    MenuApplication *application,
    MenuApplicationPromptContext *context,
    const char *patient_id
) {
    MedicalRecordHistory history;
    LinkedList dispense_records;
    const LinkedListNode *current = 0;
    MenuApplicationSelectionOption options[MENU_APPLICATION_SELECT_OPTION_MAX];
    int option_count = 0;
    Result result;
    char selected_id[HIS_DOMAIN_ID_CAPACITY];
    char detail_buf[4096];

    if (application == 0 || context == 0 || patient_id == 0) {
        return Result_make_failure("browse patient history arguments missing");
    }

    /* Load medical record history */
    result = MedicalRecordService_find_patient_history(
        &application->medical_record_service,
        patient_id,
        &history
    );
    if (result.success == 0) {
        return result;
    }

    /* Load dispense records */
    LinkedList_init(&dispense_records);
    PharmacyService_find_dispense_records_by_patient_id(
        &application->pharmacy_service,
        patient_id,
        &dispense_records
    );

    /* Build options from registrations */
    current = history.registrations.head;
    while (current != 0 && option_count < MENU_APPLICATION_SELECT_OPTION_MAX) {
        const Registration *r = (const Registration *)current->data;
        MenuApplication_copy_text(options[option_count].id, sizeof(options[option_count].id), r->registration_id);
        snprintf(
            options[option_count].label,
            sizeof(options[option_count].label),
            "[\xe6\x8c\x82\xe5\x8f\xb7] %s | \xe5\x8c\xbb\xe7\x94\x9f=%s | \xe7\xa7\x91\xe5\xae\xa4=%s | %s | %s | %s | %.2f\xe5\x85\x83",
            r->registration_id,
            r->doctor_id,
            r->department_id,
            MenuApplication_registration_status_label(r->status),
            r->registered_at,
            MenuApplication_registration_type_label(r->registration_type),
            r->registration_fee
        );
        option_count++;
        current = current->next;
    }

    /* Build options from visits */
    current = history.visits.head;
    while (current != 0 && option_count < MENU_APPLICATION_SELECT_OPTION_MAX) {
        const VisitRecord *v = (const VisitRecord *)current->data;
        MenuApplication_copy_text(options[option_count].id, sizeof(options[option_count].id), v->visit_id);
        snprintf(
            options[option_count].label,
            sizeof(options[option_count].label),
            "[\xe5\xb0\xb1\xe8\xaf\x8a] %s | \xe8\xaf\x8a\xe6\x96\xad=%s | %s",
            v->visit_id,
            v->diagnosis,
            v->visit_time
        );
        option_count++;
        current = current->next;
    }

    /* Build options from examinations */
    current = history.examinations.head;
    while (current != 0 && option_count < MENU_APPLICATION_SELECT_OPTION_MAX) {
        const ExaminationRecord *e = (const ExaminationRecord *)current->data;
        MenuApplication_copy_text(options[option_count].id, sizeof(options[option_count].id), e->examination_id);
        snprintf(
            options[option_count].label,
            sizeof(options[option_count].label),
            "[\xe6\xa3\x80\xe6\x9f\xa5] %s | %s | %.2f\xe5\x85\x83 | %s | %s",
            e->examination_id,
            e->exam_item,
            e->exam_fee,
            e->status == EXAM_STATUS_COMPLETED ? "\xe5\xb7\xb2\xe5\xae\x8c\xe6\x88\x90" : "\xe5\xbe\x85\xe6\xa3\x80\xe6\x9f\xa5",
            e->requested_at
        );
        option_count++;
        current = current->next;
    }

    /* Build options from admissions */
    current = history.admissions.head;
    while (current != 0 && option_count < MENU_APPLICATION_SELECT_OPTION_MAX) {
        const Admission *a = (const Admission *)current->data;
        MenuApplication_copy_text(options[option_count].id, sizeof(options[option_count].id), a->admission_id);
        snprintf(
            options[option_count].label,
            sizeof(options[option_count].label),
            "[\xe4\xbd\x8f\xe9\x99\xa2] %s | \xe7\x97\x85\xe6\x88\xbf=%s | \xe5\xba\x8a\xe4\xbd\x8d=%s | %s | %s",
            a->admission_id,
            a->ward_id,
            a->bed_id,
            a->status == ADMISSION_STATUS_ACTIVE ? "\xe4\xbd\x8f\xe9\x99\xa2\xe4\xb8\xad" : "\xe5\xb7\xb2\xe5\x87\xba\xe9\x99\xa2",
            a->admitted_at
        );
        option_count++;
        current = current->next;
    }

    /* Build options from dispense records */
    current = dispense_records.head;
    while (current != 0 && option_count < MENU_APPLICATION_SELECT_OPTION_MAX) {
        const DispenseRecord *d = (const DispenseRecord *)current->data;
        MenuApplication_copy_text(options[option_count].id, sizeof(options[option_count].id), d->dispense_id);
        snprintf(
            options[option_count].label,
            sizeof(options[option_count].label),
            "[\xe5\x8f\x91\xe8\x8d\xaf] %s | \xe5\xa4\x84\xe6\x96\xb9=%s | \xe8\x8d\xaf\xe5\x93\x81=%s | \xe6\x95\xb0\xe9\x87\x8f=%d | %s",
            d->dispense_id,
            d->prescription_id,
            d->medicine_id,
            d->quantity,
            d->dispensed_at
        );
        option_count++;
        current = current->next;
    }

    if (option_count == 0) {
        MedicalRecordHistory_clear(&history);
        PharmacyService_clear_dispense_record_results(&dispense_records);
        return Result_make_failure("\xe8\xaf\xa5\xe6\x82\xa3\xe8\x80\x85\xe6\x9a\x82\xe6\x97\xa0\xe5\x8e\x86\xe5\x8f\xb2\xe8\xae\xb0\xe5\xbd\x95");
    }

    /* Show browse UI */
    snprintf(detail_buf, sizeof(detail_buf),
        "\xe6\x82\xa3\xe8\x80\x85\xe5\x8e\x86\xe5\x8f\xb2: %s (\xe6\x8c\x82\xe5\x8f\xb7=%zu \xe5\xb0\xb1\xe8\xaf\x8a=%zu \xe6\xa3\x80\xe6\x9f\xa5=%zu \xe4\xbd\x8f\xe9\x99\xa2=%zu \xe5\x8f\x91\xe8\x8d\xaf=%zu)",
        patient_id,
        LinkedList_count(&history.registrations),
        LinkedList_count(&history.visits),
        LinkedList_count(&history.examinations),
        LinkedList_count(&history.admissions),
        LinkedList_count(&dispense_records));

    result = MenuApplication_interactive_browse(
        context,
        detail_buf,
        options,
        option_count,
        selected_id,
        sizeof(selected_id)
    );

    MedicalRecordHistory_clear(&history);
    PharmacyService_clear_dispense_record_results(&dispense_records);
    return result;
}

/* ─── Statistics & Fee Inquiry Functions ──────────────────────────── */

/**
 * @brief 科室收入统计
 *
 * 加载全部发药记录，根据处方ID关联就诊记录找到科室，
 * 累加各科室的药品收入（价格 x 数量）。
 */
Result MenuApplication_stats_department_revenue(
    MenuApplication *app,
    char *buffer,
    size_t capacity
) {
    LinkedList dispense_records;
    LinkedList departments;
    const LinkedListNode *current = 0;
    size_t used = 0;
    Result result;

    /* 每个科室的统计数据 */
    #define STATS_MAX_DEPARTMENTS 64
    struct {
        char department_id[HIS_DOMAIN_ID_CAPACITY];
        char department_name[HIS_DOMAIN_NAME_CAPACITY];
        double total_revenue;
        int dispense_count;
    } dept_stats[STATS_MAX_DEPARTMENTS];
    int dept_count = 0;
    int i;

    if (app == 0 || buffer == 0 || capacity == 0) {
        return Result_make_failure("stats arguments missing");
    }

    /* 加载所有科室用于名称查找 */
    LinkedList_init(&departments);
    result = DepartmentRepository_load_all(
        &app->doctor_service.department_repository,
        &departments
    );
    if (result.success == 0) {
        return MenuApplication_write_failure("无法加载科室数据", buffer, capacity);
    }

    memset(dept_stats, 0, sizeof(dept_stats));

    /* 预填充科室列表 */
    current = departments.head;
    while (current != 0 && dept_count < STATS_MAX_DEPARTMENTS) {
        const Department *dept = (const Department *)current->data;
        strncpy(dept_stats[dept_count].department_id, dept->department_id, HIS_DOMAIN_ID_CAPACITY - 1);
        strncpy(dept_stats[dept_count].department_name, dept->name, HIS_DOMAIN_NAME_CAPACITY - 1);
        dept_stats[dept_count].total_revenue = 0.0;
        dept_stats[dept_count].dispense_count = 0;
        dept_count++;
        current = current->next;
    }
    DepartmentRepository_clear_list(&departments);

    /* 加载所有发药记录 */
    LinkedList_init(&dispense_records);
    result = DispenseRecordRepository_load_all(
        &app->pharmacy_service.dispense_record_repository,
        &dispense_records
    );
    if (result.success == 0) {
        return MenuApplication_write_failure("无法加载发药记录", buffer, capacity);
    }

    /* 遍历发药记录，计算每个科室的收入 */
    current = dispense_records.head;
    while (current != 0) {
        const DispenseRecord *dr = (const DispenseRecord *)current->data;
        Medicine medicine;
        VisitRecord visit;
        const char *target_dept_id = 0;

        /* 查找药品获取价格 */
        result = MedicineRepository_find_by_medicine_id(
            &app->pharmacy_service.medicine_repository,
            dr->medicine_id,
            &medicine
        );
        if (result.success == 0) {
            current = current->next;
            continue;
        }

        /* 通过 prescription_id（即 visit_id）找到就诊记录，获取科室 */
        result = VisitRecordRepository_find_by_visit_id(
            &app->medical_record_service.visit_repository,
            dr->prescription_id,
            &visit
        );
        if (result.success) {
            target_dept_id = visit.department_id;
        } else {
            /* 如果找不到就诊记录，跳过 */
            current = current->next;
            continue;
        }

        /* 累加到对应科室 */
        for (i = 0; i < dept_count; i++) {
            if (strcmp(dept_stats[i].department_id, target_dept_id) == 0) {
                dept_stats[i].total_revenue += medicine.price * dr->quantity;
                dept_stats[i].dispense_count++;
                break;
            }
        }

        current = current->next;
    }
    DispenseRecordRepository_clear_list(&dispense_records);

    /* 格式化输出 */
    result = MenuApplication_write_text(
        buffer, capacity,
        TUI_STAR " 科室收入统计\n"
        "%-12s | %-16s | %12s | %8s\n"
        "-------------|------------------|--------------|----------",
        "科室编号", "科室名称", "总收入(元)", "发药次数"
    );
    if (result.success == 0) return result;
    used = strlen(buffer);

    for (i = 0; i < dept_count; i++) {
        result = MenuApplication_append_text(
            buffer, capacity, &used,
            "\n%-12s | %-16s | %12.2f | %8d",
            dept_stats[i].department_id,
            dept_stats[i].department_name,
            dept_stats[i].total_revenue,
            dept_stats[i].dispense_count
        );
        if (result.success == 0) return result;
    }

    return Result_make_success("department revenue stats ready");
    #undef STATS_MAX_DEPARTMENTS
}

/**
 * @brief 医生工作量统计
 *
 * 加载所有就诊记录和挂号记录，统计每位医生的就诊数、挂号数和检查数。
 */
Result MenuApplication_stats_doctor_workload(
    MenuApplication *app,
    char *buffer,
    size_t capacity
) {
    LinkedList doctors;
    LinkedList visits;
    LinkedList registrations;
    LinkedList examinations;
    const LinkedListNode *current = 0;
    size_t used = 0;
    Result result;

    #define STATS_MAX_DOCTORS 128
    struct {
        char doctor_id[HIS_DOMAIN_ID_CAPACITY];
        char doctor_name[HIS_DOMAIN_NAME_CAPACITY];
        int visit_count;
        int registration_count;
        int exam_count;
    } doc_stats[STATS_MAX_DOCTORS];
    int doc_count = 0;
    int i;

    if (app == 0 || buffer == 0 || capacity == 0) {
        return Result_make_failure("stats arguments missing");
    }

    /* 加载所有医生 */
    LinkedList_init(&doctors);
    result = DoctorRepository_load_all(
        &app->doctor_service.doctor_repository,
        &doctors
    );
    if (result.success == 0) {
        return MenuApplication_write_failure("无法加载医生数据", buffer, capacity);
    }

    memset(doc_stats, 0, sizeof(doc_stats));

    /* 预填充医生列表 */
    current = doctors.head;
    while (current != 0 && doc_count < STATS_MAX_DOCTORS) {
        const Doctor *doc = (const Doctor *)current->data;
        strncpy(doc_stats[doc_count].doctor_id, doc->doctor_id, HIS_DOMAIN_ID_CAPACITY - 1);
        strncpy(doc_stats[doc_count].doctor_name, doc->name, HIS_DOMAIN_NAME_CAPACITY - 1);
        doc_count++;
        current = current->next;
    }
    DoctorRepository_clear_list(&doctors);

    /* 加载所有就诊记录，统计 visit_count */
    LinkedList_init(&visits);
    result = VisitRecordRepository_load_all(
        &app->medical_record_service.visit_repository,
        &visits
    );
    if (result.success) {
        current = visits.head;
        while (current != 0) {
            const VisitRecord *v = (const VisitRecord *)current->data;
            for (i = 0; i < doc_count; i++) {
                if (strcmp(doc_stats[i].doctor_id, v->doctor_id) == 0) {
                    doc_stats[i].visit_count++;
                    break;
                }
            }
            current = current->next;
        }
        VisitRecordRepository_clear_list(&visits);
    }

    /* 加载所有挂号记录，统计 registration_count */
    LinkedList_init(&registrations);
    result = RegistrationRepository_load_all(
        &app->registration_repository,
        &registrations
    );
    if (result.success) {
        current = registrations.head;
        while (current != 0) {
            const Registration *r = (const Registration *)current->data;
            for (i = 0; i < doc_count; i++) {
                if (strcmp(doc_stats[i].doctor_id, r->doctor_id) == 0) {
                    doc_stats[i].registration_count++;
                    break;
                }
            }
            current = current->next;
        }
        RegistrationRepository_clear_list(&registrations);
    }

    /* 加载所有检查记录，统计 exam_count */
    LinkedList_init(&examinations);
    result = ExaminationRecordRepository_load_all(
        &app->medical_record_service.examination_repository,
        &examinations
    );
    if (result.success) {
        current = examinations.head;
        while (current != 0) {
            const ExaminationRecord *e = (const ExaminationRecord *)current->data;
            for (i = 0; i < doc_count; i++) {
                if (strcmp(doc_stats[i].doctor_id, e->doctor_id) == 0) {
                    doc_stats[i].exam_count++;
                    break;
                }
            }
            current = current->next;
        }
        ExaminationRecordRepository_clear_list(&examinations);
    }

    /* 格式化输出 */
    result = MenuApplication_write_text(
        buffer, capacity,
        TUI_STAR " 医生工作量统计\n"
        "%-12s | %-12s | %8s | %8s | %8s\n"
        "-------------|--------------|----------|----------|----------",
        "医生工号", "医生姓名", "就诊数", "挂号数", "检查数"
    );
    if (result.success == 0) return result;
    used = strlen(buffer);

    for (i = 0; i < doc_count; i++) {
        result = MenuApplication_append_text(
            buffer, capacity, &used,
            "\n%-12s | %-12s | %8d | %8d | %8d",
            doc_stats[i].doctor_id,
            doc_stats[i].doctor_name,
            doc_stats[i].visit_count,
            doc_stats[i].registration_count,
            doc_stats[i].exam_count
        );
        if (result.success == 0) return result;
    }

    return Result_make_success("doctor workload stats ready");
    #undef STATS_MAX_DOCTORS
}

/**
 * @brief 床位利用率统计
 *
 * 加载所有病房和床位，统计每个病房的总床位、已占用数和利用率。
 */
Result MenuApplication_stats_bed_utilization(
    MenuApplication *app,
    char *buffer,
    size_t capacity
) {
    LinkedList wards;
    LinkedList beds;
    const LinkedListNode *ward_node = 0;
    const LinkedListNode *bed_node = 0;
    size_t used = 0;
    Result result;

    if (app == 0 || buffer == 0 || capacity == 0) {
        return Result_make_failure("stats arguments missing");
    }

    /* 加载所有病房 */
    LinkedList_init(&wards);
    result = WardRepository_load_all(
        &app->bed_service.ward_repository,
        &wards
    );
    if (result.success == 0) {
        return MenuApplication_write_failure("无法加载病房数据", buffer, capacity);
    }

    /* 加载所有床位 */
    LinkedList_init(&beds);
    result = BedRepository_load_all(
        &app->bed_service.bed_repository,
        &beds
    );
    if (result.success == 0) {
        WardRepository_clear_loaded_list(&wards);
        return MenuApplication_write_failure("无法加载床位数据", buffer, capacity);
    }

    /* 格式化表头 */
    result = MenuApplication_write_text(
        buffer, capacity,
        TUI_STAR " 床位利用率统计\n"
        "%-12s | %-16s | %8s | %8s | %8s\n"
        "-------------|------------------|----------|----------|----------",
        "病房编号", "病房名称", "总床位", "已占用", "利用率"
    );
    if (result.success == 0) {
        WardRepository_clear_loaded_list(&wards);
        BedRepository_clear_loaded_list(&beds);
        return result;
    }
    used = strlen(buffer);

    /* 遍历每个病房，统计床位 */
    ward_node = wards.head;
    while (ward_node != 0) {
        const Ward *ward = (const Ward *)ward_node->data;
        int total_beds = 0;
        int occupied_beds = 0;
        double rate = 0.0;

        /* 遍历所有床位统计该病房的数据 */
        bed_node = beds.head;
        while (bed_node != 0) {
            const Bed *bed = (const Bed *)bed_node->data;
            if (strcmp(bed->ward_id, ward->ward_id) == 0) {
                total_beds++;
                if (bed->status == BED_STATUS_OCCUPIED) {
                    occupied_beds++;
                }
            }
            bed_node = bed_node->next;
        }

        if (total_beds > 0) {
            rate = (double)occupied_beds / (double)total_beds * 100.0;
        }

        result = MenuApplication_append_text(
            buffer, capacity, &used,
            "\n%-12s | %-16s | %8d | %8d | %7.1f%%",
            ward->ward_id,
            ward->name,
            total_beds,
            occupied_beds,
            rate
        );
        if (result.success == 0) break;

        ward_node = ward_node->next;
    }

    WardRepository_clear_loaded_list(&wards);
    BedRepository_clear_loaded_list(&beds);
    return Result_make_success("bed utilization stats ready");
}

/**
 * @brief 药品消耗排行
 *
 * 加载全部发药记录和药品信息，统计每种药品的发药总量和总收入，
 * 按收入降序排列，展示 TOP 排行和条形图。
 */
Result MenuApplication_stats_medicine_consumption(
    MenuApplication *app,
    char *buffer,
    size_t capacity
) {
    LinkedList dispense_records;
    LinkedList medicines;
    const LinkedListNode *current = 0;
    size_t used = 0;
    Result result;
    int i, j;

    #define STATS_MAX_MEDICINES 128
    struct {
        char medicine_id[HIS_DOMAIN_ID_CAPACITY];
        char medicine_name[HIS_DOMAIN_NAME_CAPACITY];
        int total_quantity;
        double total_revenue;
    } med_stats[STATS_MAX_MEDICINES];
    int med_count = 0;
    double max_revenue = 0.0;
    double grand_total = 0.0;

    if (app == 0 || buffer == 0 || capacity == 0) {
        return Result_make_failure("stats arguments missing");
    }

    /* 加载所有药品用于名称查找 */
    LinkedList_init(&medicines);
    result = MedicineRepository_load_all(
        &app->pharmacy_service.medicine_repository,
        &medicines
    );
    if (result.success == 0) {
        return MenuApplication_write_failure("无法加载药品数据", buffer, capacity);
    }

    memset(med_stats, 0, sizeof(med_stats));

    /* 预填充药品列表 */
    current = medicines.head;
    while (current != 0 && med_count < STATS_MAX_MEDICINES) {
        const Medicine *med = (const Medicine *)current->data;
        strncpy(med_stats[med_count].medicine_id, med->medicine_id, HIS_DOMAIN_ID_CAPACITY - 1);
        strncpy(med_stats[med_count].medicine_name, med->name, HIS_DOMAIN_NAME_CAPACITY - 1);
        med_stats[med_count].total_quantity = 0;
        med_stats[med_count].total_revenue = 0.0;
        med_count++;
        current = current->next;
    }
    MedicineRepository_clear_list(&medicines);

    /* 加载所有发药记录 */
    LinkedList_init(&dispense_records);
    result = DispenseRecordRepository_load_all(
        &app->pharmacy_service.dispense_record_repository,
        &dispense_records
    );
    if (result.success == 0) {
        return MenuApplication_write_failure("无法加载发药记录", buffer, capacity);
    }

    /* 遍历发药记录，累计每种药品的数量和收入 */
    current = dispense_records.head;
    while (current != 0) {
        const DispenseRecord *dr = (const DispenseRecord *)current->data;
        Medicine medicine;

        result = MedicineRepository_find_by_medicine_id(
            &app->pharmacy_service.medicine_repository,
            dr->medicine_id,
            &medicine
        );
        if (result.success) {
            for (i = 0; i < med_count; i++) {
                if (strcmp(med_stats[i].medicine_id, dr->medicine_id) == 0) {
                    med_stats[i].total_quantity += dr->quantity;
                    med_stats[i].total_revenue += medicine.price * dr->quantity;
                    break;
                }
            }
        }
        current = current->next;
    }
    DispenseRecordRepository_clear_list(&dispense_records);

    /* 按收入降序排序（简单冒泡） */
    for (i = 0; i < med_count - 1; i++) {
        for (j = 0; j < med_count - 1 - i; j++) {
            if (med_stats[j].total_revenue < med_stats[j + 1].total_revenue) {
                char tmp_id[HIS_DOMAIN_ID_CAPACITY];
                char tmp_name[HIS_DOMAIN_NAME_CAPACITY];
                int tmp_qty;
                double tmp_rev;

                strncpy(tmp_id, med_stats[j].medicine_id, HIS_DOMAIN_ID_CAPACITY);
                strncpy(tmp_name, med_stats[j].medicine_name, HIS_DOMAIN_NAME_CAPACITY);
                tmp_qty = med_stats[j].total_quantity;
                tmp_rev = med_stats[j].total_revenue;

                strncpy(med_stats[j].medicine_id, med_stats[j + 1].medicine_id, HIS_DOMAIN_ID_CAPACITY);
                strncpy(med_stats[j].medicine_name, med_stats[j + 1].medicine_name, HIS_DOMAIN_NAME_CAPACITY);
                med_stats[j].total_quantity = med_stats[j + 1].total_quantity;
                med_stats[j].total_revenue = med_stats[j + 1].total_revenue;

                strncpy(med_stats[j + 1].medicine_id, tmp_id, HIS_DOMAIN_ID_CAPACITY);
                strncpy(med_stats[j + 1].medicine_name, tmp_name, HIS_DOMAIN_NAME_CAPACITY);
                med_stats[j + 1].total_quantity = tmp_qty;
                med_stats[j + 1].total_revenue = tmp_rev;
            }
        }
    }

    /* 找到最大收入和总收入 */
    for (i = 0; i < med_count; i++) {
        grand_total += med_stats[i].total_revenue;
        if (med_stats[i].total_revenue > max_revenue) {
            max_revenue = med_stats[i].total_revenue;
        }
    }

    /* 格式化输出 */
    result = MenuApplication_write_text(
        buffer, capacity,
        TUI_STAR " 药品消耗排行\n"
        TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH
        TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH
        TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH "\n"
        "%3s " TUI_DV " %-16s " TUI_DV " %6s " TUI_DV " %10s " TUI_DV " %-20s\n"
        TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH
        TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH
        TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH,
        " # ", "药品名称", "数量", "总金额", "消耗占比"
    );
    if (result.success == 0) return result;
    used = strlen(buffer);

    for (i = 0; i < med_count && i < 10; i++) {
        char bar[64];
        int bar_len = 12;
        int filled = 0;
        int k;
        double pct = 0.0;

        if (grand_total > 0.0) {
            pct = med_stats[i].total_revenue / grand_total * 100.0;
        }
        if (max_revenue > 0.0) {
            filled = (int)(med_stats[i].total_revenue / max_revenue * bar_len);
        }
        if (filled > bar_len) filled = bar_len;

        bar[0] = '\0';
        for (k = 0; k < filled; k++) {
            strncat(bar, TUI_BLOCK_FULL, sizeof(bar) - strlen(bar) - 1);
        }
        for (k = filled; k < bar_len; k++) {
            strncat(bar, TUI_BLOCK_LIGHT, sizeof(bar) - strlen(bar) - 1);
        }

        result = MenuApplication_append_text(
            buffer, capacity, &used,
            "\n%3d " TUI_DV " %-16s " TUI_DV " %6d " TUI_DV " %10.2f " TUI_DV " %s %4.1f%%",
            i + 1,
            med_stats[i].medicine_name,
            med_stats[i].total_quantity,
            med_stats[i].total_revenue,
            bar,
            pct
        );
        if (result.success == 0) return result;
    }

    result = MenuApplication_append_text(
        buffer, capacity, &used,
        "\n" TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH
        TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH
        TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH
        "\n" TUI_BULLET " 药品总数: %d    总消耗金额: %.2f 元",
        med_count,
        grand_total
    );

    return Result_make_success("medicine consumption stats ready");
    #undef STATS_MAX_MEDICINES
}

/**
 * @brief 患者流量统计
 *
 * 加载所有挂号记录，按科室统计挂号数量，按状态统计分布，计算取消率。
 */
Result MenuApplication_stats_patient_flow(
    MenuApplication *app,
    char *buffer,
    size_t capacity
) {
    LinkedList registrations;
    LinkedList departments;
    const LinkedListNode *current = 0;
    size_t used = 0;
    Result result;
    int i;

    #define STATS_FLOW_MAX_DEPTS 64
    struct {
        char department_id[HIS_DOMAIN_ID_CAPACITY];
        char department_name[HIS_DOMAIN_NAME_CAPACITY];
        int pending_count;
        int diagnosed_count;
        int cancelled_count;
        int total_count;
    } dept_flow[STATS_FLOW_MAX_DEPTS];
    int dept_count = 0;
    int total_pending = 0;
    int total_diagnosed = 0;
    int total_cancelled = 0;
    int total_all = 0;

    if (app == 0 || buffer == 0 || capacity == 0) {
        return Result_make_failure("stats arguments missing");
    }

    /* 加载所有科室 */
    LinkedList_init(&departments);
    result = DepartmentRepository_load_all(
        &app->doctor_service.department_repository,
        &departments
    );
    if (result.success == 0) {
        return MenuApplication_write_failure("无法加载科室数据", buffer, capacity);
    }

    memset(dept_flow, 0, sizeof(dept_flow));

    current = departments.head;
    while (current != 0 && dept_count < STATS_FLOW_MAX_DEPTS) {
        const Department *dept = (const Department *)current->data;
        strncpy(dept_flow[dept_count].department_id, dept->department_id, HIS_DOMAIN_ID_CAPACITY - 1);
        strncpy(dept_flow[dept_count].department_name, dept->name, HIS_DOMAIN_NAME_CAPACITY - 1);
        dept_count++;
        current = current->next;
    }
    DepartmentRepository_clear_list(&departments);

    /* 加载所有挂号记录 */
    LinkedList_init(&registrations);
    result = RegistrationRepository_load_all(
        &app->registration_repository,
        &registrations
    );
    if (result.success == 0) {
        return MenuApplication_write_failure("无法加载挂号记录", buffer, capacity);
    }

    /* 统计 */
    current = registrations.head;
    while (current != 0) {
        const Registration *r = (const Registration *)current->data;
        for (i = 0; i < dept_count; i++) {
            if (strcmp(dept_flow[i].department_id, r->department_id) == 0) {
                dept_flow[i].total_count++;
                if (r->status == REG_STATUS_PENDING) {
                    dept_flow[i].pending_count++;
                } else if (r->status == REG_STATUS_DIAGNOSED) {
                    dept_flow[i].diagnosed_count++;
                } else if (r->status == REG_STATUS_CANCELLED) {
                    dept_flow[i].cancelled_count++;
                }
                break;
            }
        }
        current = current->next;
    }
    RegistrationRepository_clear_list(&registrations);

    /* 汇总 */
    for (i = 0; i < dept_count; i++) {
        total_pending += dept_flow[i].pending_count;
        total_diagnosed += dept_flow[i].diagnosed_count;
        total_cancelled += dept_flow[i].cancelled_count;
        total_all += dept_flow[i].total_count;
    }

    /* 格式化输出 */
    result = MenuApplication_write_text(
        buffer, capacity,
        TUI_STAR " 患者流量统计\n"
        TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH
        TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH
        TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH "\n"
        TUI_BULLET " 总挂号数: %d    待诊: %d    已就诊: %d    已取消: %d\n"
        TUI_BULLET " 取消率: %.1f%%\n"
        TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH
        TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH
        TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH "\n"
        "%-12s " TUI_DV " %-14s " TUI_DV " %6s " TUI_DV " %6s " TUI_DV " %6s " TUI_DV " %6s " TUI_DV " %8s\n"
        "-------------|----------------|--------|--------|--------|--------|---------",
        total_all, total_pending, total_diagnosed, total_cancelled,
        total_all > 0 ? (double)total_cancelled / total_all * 100.0 : 0.0,
        "科室编号", "科室名称", "总计", "待诊", "已诊", "取消", "取消率"
    );
    if (result.success == 0) return result;
    used = strlen(buffer);

    for (i = 0; i < dept_count; i++) {
        double cancel_rate = 0.0;
        if (dept_flow[i].total_count > 0) {
            cancel_rate = (double)dept_flow[i].cancelled_count / dept_flow[i].total_count * 100.0;
        }
        result = MenuApplication_append_text(
            buffer, capacity, &used,
            "\n%-12s " TUI_DV " %-14s " TUI_DV " %6d " TUI_DV " %6d " TUI_DV " %6d " TUI_DV " %6d " TUI_DV " %7.1f%%",
            dept_flow[i].department_id,
            dept_flow[i].department_name,
            dept_flow[i].total_count,
            dept_flow[i].pending_count,
            dept_flow[i].diagnosed_count,
            dept_flow[i].cancelled_count,
            cancel_rate
        );
        if (result.success == 0) return result;
    }

    return Result_make_success("patient flow stats ready");
    #undef STATS_FLOW_MAX_DEPTS
}

/**
 * @brief 科室综合绩效
 *
 * 综合收入、患者数、医生数，计算人均患者数，展示科室对比。
 */
Result MenuApplication_stats_dept_performance(
    MenuApplication *app,
    char *buffer,
    size_t capacity
) {
    LinkedList departments;
    LinkedList doctors;
    LinkedList registrations;
    LinkedList dispense_records;
    const LinkedListNode *current = 0;
    size_t used = 0;
    Result result;
    int i;

    #define STATS_PERF_MAX_DEPTS 64
    struct {
        char department_id[HIS_DOMAIN_ID_CAPACITY];
        char department_name[HIS_DOMAIN_NAME_CAPACITY];
        double revenue;
        int patient_count;
        int doctor_count;
    } perf[STATS_PERF_MAX_DEPTS];
    int dept_count = 0;

    if (app == 0 || buffer == 0 || capacity == 0) {
        return Result_make_failure("stats arguments missing");
    }

    /* 加载科室 */
    LinkedList_init(&departments);
    result = DepartmentRepository_load_all(
        &app->doctor_service.department_repository,
        &departments
    );
    if (result.success == 0) {
        return MenuApplication_write_failure("无法加载科室数据", buffer, capacity);
    }

    memset(perf, 0, sizeof(perf));

    current = departments.head;
    while (current != 0 && dept_count < STATS_PERF_MAX_DEPTS) {
        const Department *dept = (const Department *)current->data;
        strncpy(perf[dept_count].department_id, dept->department_id, HIS_DOMAIN_ID_CAPACITY - 1);
        strncpy(perf[dept_count].department_name, dept->name, HIS_DOMAIN_NAME_CAPACITY - 1);
        dept_count++;
        current = current->next;
    }
    DepartmentRepository_clear_list(&departments);

    /* 统计医生数 */
    LinkedList_init(&doctors);
    result = DoctorRepository_load_all(
        &app->doctor_service.doctor_repository,
        &doctors
    );
    if (result.success) {
        current = doctors.head;
        while (current != 0) {
            const Doctor *doc = (const Doctor *)current->data;
            for (i = 0; i < dept_count; i++) {
                if (strcmp(perf[i].department_id, doc->department_id) == 0) {
                    perf[i].doctor_count++;
                    break;
                }
            }
            current = current->next;
        }
        DoctorRepository_clear_list(&doctors);
    }

    /* 统计患者数（通过挂号记录） */
    LinkedList_init(&registrations);
    result = RegistrationRepository_load_all(
        &app->registration_repository,
        &registrations
    );
    if (result.success) {
        current = registrations.head;
        while (current != 0) {
            const Registration *r = (const Registration *)current->data;
            for (i = 0; i < dept_count; i++) {
                if (strcmp(perf[i].department_id, r->department_id) == 0) {
                    perf[i].patient_count++;
                    break;
                }
            }
            current = current->next;
        }
        RegistrationRepository_clear_list(&registrations);
    }

    /* 统计收入（通过发药记录） */
    LinkedList_init(&dispense_records);
    result = DispenseRecordRepository_load_all(
        &app->pharmacy_service.dispense_record_repository,
        &dispense_records
    );
    if (result.success) {
        current = dispense_records.head;
        while (current != 0) {
            const DispenseRecord *dr = (const DispenseRecord *)current->data;
            Medicine medicine;
            VisitRecord visit;

            result = MedicineRepository_find_by_medicine_id(
                &app->pharmacy_service.medicine_repository,
                dr->medicine_id,
                &medicine
            );
            if (result.success) {
                result = VisitRecordRepository_find_by_visit_id(
                    &app->medical_record_service.visit_repository,
                    dr->prescription_id,
                    &visit
                );
                if (result.success) {
                    for (i = 0; i < dept_count; i++) {
                        if (strcmp(perf[i].department_id, visit.department_id) == 0) {
                            perf[i].revenue += medicine.price * dr->quantity;
                            break;
                        }
                    }
                }
            }
            current = current->next;
        }
        DispenseRecordRepository_clear_list(&dispense_records);
    }

    /* 格式化输出 */
    result = MenuApplication_write_text(
        buffer, capacity,
        TUI_STAR " 科室综合绩效\n"
        TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH
        TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH
        TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH "\n"
        "%-12s " TUI_DV " %-14s " TUI_DV " %10s " TUI_DV " %6s " TUI_DV " %6s " TUI_DV " %8s\n"
        "-------------|----------------|------------|--------|--------|---------",
        "科室编号", "科室名称", "收入(元)", "患者数", "医生数", "人均患者"
    );
    if (result.success == 0) return result;
    used = strlen(buffer);

    for (i = 0; i < dept_count; i++) {
        double avg_patients = 0.0;
        if (perf[i].doctor_count > 0) {
            avg_patients = (double)perf[i].patient_count / perf[i].doctor_count;
        }
        result = MenuApplication_append_text(
            buffer, capacity, &used,
            "\n%-12s " TUI_DV " %-14s " TUI_DV " %10.2f " TUI_DV " %6d " TUI_DV " %6d " TUI_DV " %8.1f",
            perf[i].department_id,
            perf[i].department_name,
            perf[i].revenue,
            perf[i].patient_count,
            perf[i].doctor_count,
            avg_patients
        );
        if (result.success == 0) return result;
    }

    return Result_make_success("department performance stats ready");
    #undef STATS_PERF_MAX_DEPTS
}

/**
 * @brief 将日期转换为自 epoch 起的天数（用于天数差计算）
 *
 * 使用标准库 mktime 正确处理闰年、月长差异与跨年边界。
 * tm_hour 固定为 12 以规避夏令时切换日可能出现的 23/25 小时问题。
 * 调用方通过两次调用相减得到实际自然日差（例如住院天数）。
 * 参数非法或 mktime 失败时返回 0（保持既有调用方容错路径）。
 */
static int MenuApplication_date_to_days(int year, int month, int day) {
    struct tm tm_date;
    time_t t;
    memset(&tm_date, 0, sizeof(tm_date));
    tm_date.tm_year = year - 1900;
    tm_date.tm_mon  = month - 1;
    tm_date.tm_mday = day;
    tm_date.tm_hour = 12;  /* 避免 DST 边界问题 */
    tm_date.tm_isdst = -1;
    t = mktime(&tm_date);
    if (t == (time_t)-1) {
        return 0;
    }
    return (int)(t / 86400);
}

/**
 * @brief 住院周转率
 *
 * 加载所有入院记录，统计总入院数、在院/出院分布、平均住院天数、
 * 各病房入院数和占用率。
 */
Result MenuApplication_stats_admission_turnover(
    MenuApplication *app,
    char *buffer,
    size_t capacity
) {
    LinkedList admissions;
    LinkedList wards;
    LinkedList beds;
    const LinkedListNode *current = 0;
    const LinkedListNode *bed_node = 0;
    size_t used = 0;
    Result result;
    int i;

    int total_admissions = 0;
    int active_count = 0;
    int discharged_count = 0;
    int los_count = 0;
    double total_los = 0.0;

    #define STATS_TURNOVER_MAX_WARDS 32
    struct {
        char ward_id[HIS_DOMAIN_ID_CAPACITY];
        char ward_name[HIS_DOMAIN_NAME_CAPACITY];
        int admission_count;
        int active_admission_count;
        int total_beds;
        int occupied_beds;
    } ward_stats[STATS_TURNOVER_MAX_WARDS];
    int ward_count = 0;

    if (app == 0 || buffer == 0 || capacity == 0) {
        return Result_make_failure("stats arguments missing");
    }

    /* 加载病房 */
    LinkedList_init(&wards);
    result = WardRepository_load_all(
        &app->bed_service.ward_repository,
        &wards
    );
    if (result.success == 0) {
        return MenuApplication_write_failure("无法加载病房数据", buffer, capacity);
    }

    memset(ward_stats, 0, sizeof(ward_stats));

    current = wards.head;
    while (current != 0 && ward_count < STATS_TURNOVER_MAX_WARDS) {
        const Ward *w = (const Ward *)current->data;
        strncpy(ward_stats[ward_count].ward_id, w->ward_id, HIS_DOMAIN_ID_CAPACITY - 1);
        strncpy(ward_stats[ward_count].ward_name, w->name, HIS_DOMAIN_NAME_CAPACITY - 1);
        ward_count++;
        current = current->next;
    }
    WardRepository_clear_loaded_list(&wards);

    /* 加载床位统计每病房总床位和已占用 */
    LinkedList_init(&beds);
    result = BedRepository_load_all(
        &app->bed_service.bed_repository,
        &beds
    );
    if (result.success) {
        bed_node = beds.head;
        while (bed_node != 0) {
            const Bed *bed = (const Bed *)bed_node->data;
            for (i = 0; i < ward_count; i++) {
                if (strcmp(ward_stats[i].ward_id, bed->ward_id) == 0) {
                    ward_stats[i].total_beds++;
                    if (bed->status == BED_STATUS_OCCUPIED) {
                        ward_stats[i].occupied_beds++;
                    }
                    break;
                }
            }
            bed_node = bed_node->next;
        }
        BedRepository_clear_loaded_list(&beds);
    }

    /* 加载所有入院记录 */
    LinkedList_init(&admissions);
    result = AdmissionRepository_load_all(
        &app->inpatient_service.admission_repository,
        &admissions
    );
    if (result.success == 0) {
        return MenuApplication_write_failure("无法加载住院记录", buffer, capacity);
    }

    current = admissions.head;
    while (current != 0) {
        const Admission *a = (const Admission *)current->data;
        total_admissions++;

        if (a->status == ADMISSION_STATUS_ACTIVE) {
            active_count++;
        } else {
            discharged_count++;
            /* 估算住院天数 */
            if (a->admitted_at[0] != '\0' && a->discharged_at[0] != '\0') {
                if (strlen(a->admitted_at) >= 10 && strlen(a->discharged_at) >= 10) {
                    int ay = 0, am = 0, ad = 0, dy = 0, dm = 0, dd = 0;
                    if (sscanf(a->admitted_at, "%d-%d-%d", &ay, &am, &ad) == 3 &&
                        sscanf(a->discharged_at, "%d-%d-%d", &dy, &dm, &dd) == 3) {
                        int admit_day = MenuApplication_date_to_days(ay, am, ad);
                        int discharge_day = MenuApplication_date_to_days(dy, dm, dd);
                        if (discharge_day > admit_day) {
                            total_los += (discharge_day - admit_day);
                            los_count++;
                        } else {
                            total_los += 1.0;
                            los_count++;
                        }
                    }
                }
            }
        }

        /* 统计病房入院数 */
        for (i = 0; i < ward_count; i++) {
            if (strcmp(ward_stats[i].ward_id, a->ward_id) == 0) {
                ward_stats[i].admission_count++;
                if (a->status == ADMISSION_STATUS_ACTIVE) {
                    ward_stats[i].active_admission_count++;
                }
                break;
            }
        }

        current = current->next;
    }
    AdmissionRepository_clear_loaded_list(&admissions);

    /* 格式化输出 */
    {
        double avg_los = los_count > 0 ? total_los / los_count : 0.0;

        result = MenuApplication_write_text(
            buffer, capacity,
            TUI_MEDICAL " 住院周转率分析\n"
            TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH
            TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH
            TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH "\n"
            TUI_BULLET " 总入院数: %d    在院: %d    已出院: %d\n"
            TUI_BULLET " 平均住院天数: %.1f 天\n"
            TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH
            TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH
            TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH "\n"
            "%-12s " TUI_DV " %-14s " TUI_DV " %6s " TUI_DV " %6s " TUI_DV " %6s " TUI_DV " %6s " TUI_DV " %8s\n"
            "-------------|----------------|--------|--------|--------|--------|---------",
            total_admissions, active_count, discharged_count,
            avg_los,
            "病房编号", "病房名称", "入院数", "在院", "总床位", "占用", "占用率"
        );
    }
    if (result.success == 0) return result;
    used = strlen(buffer);

    for (i = 0; i < ward_count; i++) {
        double occ_rate = 0.0;
        if (ward_stats[i].total_beds > 0) {
            occ_rate = (double)ward_stats[i].occupied_beds / ward_stats[i].total_beds * 100.0;
        }

        result = MenuApplication_append_text(
            buffer, capacity, &used,
            "\n%-12s " TUI_DV " %-14s " TUI_DV " %6d " TUI_DV " %6d " TUI_DV " %6d " TUI_DV " %6d " TUI_DV " %7.1f%%",
            ward_stats[i].ward_id,
            ward_stats[i].ward_name,
            ward_stats[i].admission_count,
            ward_stats[i].active_admission_count,
            ward_stats[i].total_beds,
            ward_stats[i].occupied_beds,
            occ_rate
        );
        if (result.success == 0) return result;
    }

    return Result_make_success("admission turnover stats ready");
    #undef STATS_TURNOVER_MAX_WARDS
}

/**
 * @brief 患者费用查询
 *
 * 加载指定患者的发药记录和住院记录，计算药品费用和住院床位费，
 * 分类展示明细并汇总总费用。
 */
Result MenuApplication_query_patient_fees(
    MenuApplication *app,
    const char *patient_id,
    char *buffer,
    size_t capacity
) {
    LinkedList records;
    LinkedList admissions;
    LinkedList reg_list;
    LinkedList visit_list;
    LinkedList exam_list;
    const LinkedListNode *current = 0;
    size_t used = 0;
    double drug_total = 0.0;
    double bed_total = 0.0;
    double reg_total = 0.0;
    double exam_total = 0.0;
    double grand_total = 0.0;
    int drug_count = 0;
    int bed_count = 0;
    int reg_count = 0;
    int exam_count = 0;
    Result result;

    if (app == 0 || patient_id == 0 || buffer == 0 || capacity == 0) {
        return Result_make_failure("fee query arguments missing");
    }

    /* 加载该患者的发药记录 */
    LinkedList_init(&records);
    result = PharmacyService_find_dispense_records_by_patient_id(
        &app->pharmacy_service,
        patient_id,
        &records
    );
    if (result.success == 0) {
        return MenuApplication_write_failure("无法加载发药记录", buffer, capacity);
    }

    /* 加载所有住院记录 */
    LinkedList_init(&admissions);
    result = AdmissionRepository_load_all(
        &app->inpatient_service.admission_repository,
        &admissions
    );
    if (result.success == 0) {
        PharmacyService_clear_dispense_record_results(&records);
        return MenuApplication_write_failure("无法加载住院记录", buffer, capacity);
    }

    /* 加载该患者的挂号记录 */
    LinkedList_init(&reg_list);
    {
        RegistrationRepositoryFilter reg_filter;
        RegistrationRepositoryFilter_init(&reg_filter);
        result = RegistrationService_find_by_patient_id(
            &app->registration_service,
            patient_id,
            &reg_filter,
            &reg_list
        );
        if (result.success == 0) {
            PharmacyService_clear_dispense_record_results(&records);
            AdmissionRepository_clear_loaded_list(&admissions);
            return MenuApplication_write_failure("无法加载挂号记录", buffer, capacity);
        }
    }

    /* 加载所有就诊记录以便查找该患者的检查费用 */
    LinkedList_init(&visit_list);
    result = VisitRecordRepository_load_all(
        &app->medical_record_service.visit_repository,
        &visit_list
    );
    if (result.success == 0) {
        PharmacyService_clear_dispense_record_results(&records);
        AdmissionRepository_clear_loaded_list(&admissions);
        RegistrationRepository_clear_list(&reg_list);
        return MenuApplication_write_failure("无法加载就诊记录", buffer, capacity);
    }

    /* 检查是否有任何费用记录 */
    {
        int has_admissions = 0;
        int has_exams = 0;
        current = admissions.head;
        while (current != 0) {
            const Admission *adm = (const Admission *)current->data;
            if (strcmp(adm->patient_id, patient_id) == 0) {
                has_admissions = 1;
                break;
            }
            current = current->next;
        }

        /* 检查是否有检查记录 */
        current = visit_list.head;
        while (current != 0 && has_exams == 0) {
            const VisitRecord *visit = (const VisitRecord *)current->data;
            if (strcmp(visit->patient_id, patient_id) == 0) {
                LinkedList tmp_exams;
                LinkedList_init(&tmp_exams);
                if (ExaminationRecordRepository_find_by_visit_id(
                        &app->medical_record_service.examination_repository,
                        visit->visit_id, &tmp_exams).success) {
                    if (LinkedList_count(&tmp_exams) > 0) {
                        has_exams = 1;
                    }
                }
                ExaminationRecordRepository_clear_list(&tmp_exams);
            }
            current = current->next;
        }

        if (LinkedList_count(&records) == 0 && has_admissions == 0 &&
            LinkedList_count(&reg_list) == 0 && has_exams == 0) {
            PharmacyService_clear_dispense_record_results(&records);
            AdmissionRepository_clear_loaded_list(&admissions);
            RegistrationRepository_clear_list(&reg_list);
            VisitRecordRepository_clear_list(&visit_list);
            return MenuApplication_write_text(
                buffer, capacity,
                TUI_STAR " 费用查询 - 患者: %s\n暂无费用记录",
                patient_id
            );
        }
    }

    /* 总标题 */
    result = MenuApplication_write_text(
        buffer, capacity,
        TUI_STAR " 费用查询 - 患者: %s",
        patient_id
    );
    if (result.success == 0) {
        PharmacyService_clear_dispense_record_results(&records);
        AdmissionRepository_clear_loaded_list(&admissions);
        RegistrationRepository_clear_list(&reg_list);
        VisitRecordRepository_clear_list(&visit_list);
        return result;
    }
    used = strlen(buffer);

    /* ===== 一、挂号费用 ===== */
    result = MenuApplication_append_text(
        buffer, capacity, &used,
        "\n\n" TUI_BULLET " 一、挂号费用\n"
        "%-12s | %-6s | %-10s | %8s\n"
        "-------------|--------|------------|----------",
        "编号", "类型", "科室", "费用(元)"
    );
    if (result.success == 0) {
        PharmacyService_clear_dispense_record_results(&records);
        AdmissionRepository_clear_loaded_list(&admissions);
        RegistrationRepository_clear_list(&reg_list);
        VisitRecordRepository_clear_list(&visit_list);
        return result;
    }

    current = reg_list.head;
    while (current != 0) {
        const Registration *reg = (const Registration *)current->data;
        Department dept;
        const char *dept_name = reg->department_id;

        if (DepartmentRepository_find_by_department_id(
                &app->doctor_service.department_repository,
                reg->department_id, &dept).success) {
            dept_name = dept.name;
        }

        reg_total += reg->registration_fee;
        reg_count++;
        result = MenuApplication_append_text(
            buffer, capacity, &used,
            "\n%-12s | %-6s | %-10s | %8.2f",
            reg->registration_id,
            MenuApplication_registration_type_label(reg->registration_type),
            dept_name,
            reg->registration_fee
        );
        if (result.success == 0) break;
        current = current->next;
    }
    RegistrationRepository_clear_list(&reg_list);

    result = MenuApplication_append_text(
        buffer, capacity, &used,
        "\n挂号费小计: %.2f 元",
        reg_total
    );

    /* ===== 二、检查费用 ===== */
    result = MenuApplication_append_text(
        buffer, capacity, &used,
        "\n\n" TUI_BULLET " 二、检查费用\n"
        "%-12s | %-12s | %-6s | %8s\n"
        "-------------|--------------|--------|----------",
        "编号", "检查项目", "类型", "费用(元)"
    );
    if (result.success == 0) {
        PharmacyService_clear_dispense_record_results(&records);
        AdmissionRepository_clear_loaded_list(&admissions);
        VisitRecordRepository_clear_list(&visit_list);
        return result;
    }

    current = visit_list.head;
    while (current != 0) {
        const VisitRecord *visit = (const VisitRecord *)current->data;
        if (strcmp(visit->patient_id, patient_id) == 0) {
            LinkedList exams;
            LinkedList_init(&exams);
            if (ExaminationRecordRepository_find_by_visit_id(
                    &app->medical_record_service.examination_repository,
                    visit->visit_id, &exams).success) {
                const LinkedListNode *exam_node = exams.head;
                while (exam_node != 0) {
                    const ExaminationRecord *exam = (const ExaminationRecord *)exam_node->data;
                    exam_total += exam->exam_fee;
                    exam_count++;
                    result = MenuApplication_append_text(
                        buffer, capacity, &used,
                        "\n%-12s | %-12s | %-6s | %8.2f",
                        exam->examination_id,
                        exam->exam_item,
                        exam->exam_type,
                        exam->exam_fee
                    );
                    if (result.success == 0) break;
                    exam_node = exam_node->next;
                }
            }
            ExaminationRecordRepository_clear_list(&exams);
            if (result.success == 0) break;
        }
        current = current->next;
    }
    VisitRecordRepository_clear_list(&visit_list);

    result = MenuApplication_append_text(
        buffer, capacity, &used,
        "\n检查费小计: %.2f 元",
        exam_total
    );

    /* ===== 三、药品费用 ===== */
    result = MenuApplication_append_text(
        buffer, capacity, &used,
        "\n\n" TUI_BULLET " 三、药品费用\n"
        "%-12s | %-16s | %8s | %8s | %12s\n"
        "-------------|------------------|----------|----------|--------------",
        "发药编号", "药品名称", "单价", "数量", "小计(元)"
    );
    if (result.success == 0) {
        PharmacyService_clear_dispense_record_results(&records);
        AdmissionRepository_clear_loaded_list(&admissions);
        return result;
    }

    current = records.head;
    while (current != 0) {
        const DispenseRecord *dr = (const DispenseRecord *)current->data;
        Medicine medicine;
        double item_fee = 0.0;

        result = MedicineRepository_find_by_medicine_id(
            &app->pharmacy_service.medicine_repository,
            dr->medicine_id,
            &medicine
        );
        if (result.success) {
            item_fee = medicine.price * dr->quantity;
            drug_total += item_fee;
            drug_count++;

            result = MenuApplication_append_text(
                buffer, capacity, &used,
                "\n%-12s | %-16s | %8.2f | %8d | %12.2f",
                dr->dispense_id,
                medicine.name,
                medicine.price,
                dr->quantity,
                item_fee
            );
        } else {
            /* 找不到药品信息时用ID代替 */
            result = MenuApplication_append_text(
                buffer, capacity, &used,
                "\n%-12s | %-16s | %8s | %8d | %12s",
                dr->dispense_id,
                dr->medicine_id,
                "未知",
                dr->quantity,
                "未知"
            );
        }
        if (result.success == 0) break;

        current = current->next;
    }

    result = MenuApplication_append_text(
        buffer, capacity, &used,
        "\n药品费小计: %.2f 元",
        drug_total
    );

    /* ===== 四、住院床位费 ===== */
    result = MenuApplication_append_text(
        buffer, capacity, &used,
        "\n\n" TUI_BULLET " 四、住院床位费\n"
        "%-12s | %-12s | %6s | %8s | %6s | %12s\n"
        "-------------|--------------|--------|----------|--------|--------------",
        "住院编号", "病房名称", "类型", "日费", "天数", "小计(元)"
    );

    current = admissions.head;
    while (current != 0) {
        const Admission *adm = (const Admission *)current->data;
        if (strcmp(adm->patient_id, patient_id) == 0) {
            Ward ward;
            Result ward_result = WardRepository_find_by_id(
                &app->bed_service.ward_repository,
                adm->ward_id,
                &ward
            );
            if (ward_result.success) {
                int days = 1;
                double bed_fee = 0.0;

                /* 解析入院和出院日期计算天数 */
                {
                    int adm_year = 0, adm_month = 0, adm_day = 0;
                    int dis_year = 0, dis_month = 0, dis_day = 0;
                    int adm_days_total = 0;
                    int dis_days_total = 0;

                    if (sscanf(adm->admitted_at, "%d-%d-%d",
                               &adm_year, &adm_month, &adm_day) == 3) {
                        adm_days_total = MenuApplication_date_to_days(adm_year, adm_month, adm_day);

                        if (adm->status == ADMISSION_STATUS_DISCHARGED &&
                            adm->discharged_at[0] != '\0') {
                            if (sscanf(adm->discharged_at, "%d-%d-%d",
                                       &dis_year, &dis_month, &dis_day) == 3) {
                                dis_days_total = MenuApplication_date_to_days(dis_year, dis_month, dis_day);
                                days = dis_days_total - adm_days_total;
                            }
                        } else {
                            /* 在院患者：计算到当前日期 */
                            {
                                time_t now = time(NULL);
                                struct tm *tm_now = localtime(&now);
                                if (tm_now) {
                                    dis_days_total = MenuApplication_date_to_days(
                                        tm_now->tm_year + 1900,
                                        tm_now->tm_mon + 1,
                                        tm_now->tm_mday);
                                }
                            }
                            days = dis_days_total - adm_days_total;
                        }
                        if (days < 1) days = 1;
                    }
                }

                bed_fee = ward.daily_fee * days;
                bed_total += bed_fee;
                bed_count++;

                result = MenuApplication_append_text(
                    buffer, capacity, &used,
                    "\n%-12s | %-12s | %6s | %8.2f | %6d | %12.2f",
                    adm->admission_id,
                    ward.name,
                    MenuApplication_ward_type_label(ward.ward_type),
                    ward.daily_fee,
                    days,
                    bed_fee
                );
            } else {
                result = MenuApplication_append_text(
                    buffer, capacity, &used,
                    "\n%-12s | %-12s | %6s | %8s | %6s | %12s",
                    adm->admission_id,
                    adm->ward_id,
                    "未知",
                    "未知",
                    "未知",
                    "未知"
                );
            }
            if (result.success == 0) break;
        }
        current = current->next;
    }

    result = MenuApplication_append_text(
        buffer, capacity, &used,
        "\n住院费小计: %.2f 元",
        bed_total
    );

    /* ===== 总费用汇总 ===== */
    grand_total = reg_total + exam_total + drug_total + bed_total;
    result = MenuApplication_append_text(
        buffer, capacity, &used,
        "\n\n" TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH
        TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH
        TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH
        TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH
        TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH
        "\n费用总计: %.2f 元",
        grand_total
    );

    PharmacyService_clear_dispense_record_results(&records);
    AdmissionRepository_clear_loaded_list(&admissions);
    return Result_make_success("patient fees query ready");
}

/* ─── 出院结算单 ─────────────────────────────────────────────────────── */

Result MenuApplication_discharge_settlement(
    MenuApplication *app,
    const char *admission_id,
    char *buffer,
    size_t capacity
) {
    Admission admission;
    Ward ward;
    LinkedList dispense_list;
    LinkedList visit_list;
    LinkedList exam_list;
    const LinkedListNode *current = 0;
    size_t used = 0;
    double bed_total = 0.0;
    double drug_total = 0.0;
    double exam_total = 0.0;
    double grand_total = 0.0;
    int days = 1;
    Result result;

    if (app == 0 || admission_id == 0 || buffer == 0 || capacity == 0) {
        return Result_make_failure("discharge settlement arguments missing");
    }

    /* 查找住院记录 */
    result = AdmissionRepository_find_by_id(
        &app->inpatient_service.admission_repository,
        admission_id,
        &admission
    );
    if (result.success == 0) {
        return MenuApplication_write_failure("未找到住院记录", buffer, capacity);
    }

    /* 查找病房信息计算床位费 */
    result = WardRepository_find_by_id(
        &app->bed_service.ward_repository,
        admission.ward_id,
        &ward
    );
    if (result.success) {
        int adm_year = 0, adm_month = 0, adm_day = 0;
        int dis_year = 0, dis_month = 0, dis_day = 0;
        int adm_days_total = 0;
        int dis_days_total = 0;

        if (sscanf(admission.admitted_at, "%d-%d-%d",
                   &adm_year, &adm_month, &adm_day) == 3) {
            adm_days_total = MenuApplication_date_to_days(adm_year, adm_month, adm_day);

            if (admission.status == ADMISSION_STATUS_DISCHARGED &&
                admission.discharged_at[0] != '\0') {
                if (sscanf(admission.discharged_at, "%d-%d-%d",
                           &dis_year, &dis_month, &dis_day) == 3) {
                    dis_days_total = MenuApplication_date_to_days(dis_year, dis_month, dis_day);
                    days = dis_days_total - adm_days_total;
                }
            } else {
                /* 在院患者：计算到当前日期 */
                {
                    time_t now = time(NULL);
                    struct tm *tm_now = localtime(&now);
                    if (tm_now) {
                        dis_days_total = MenuApplication_date_to_days(
                            tm_now->tm_year + 1900,
                            tm_now->tm_mon + 1,
                            tm_now->tm_mday);
                    }
                }
                days = dis_days_total - adm_days_total;
            }
            if (days < 1) days = 1;
        }
        bed_total = ward.daily_fee * days;
    }

    /* 加载该患者的发药记录并筛选住院期间的 */
    LinkedList_init(&dispense_list);
    result = PharmacyService_find_dispense_records_by_patient_id(
        &app->pharmacy_service,
        admission.patient_id,
        &dispense_list
    );
    if (result.success) {
        current = dispense_list.head;
        while (current != 0) {
            const DispenseRecord *dr = (const DispenseRecord *)current->data;
            /* 检查发药时间是否在住院期间 */
            if (dr->dispensed_at[0] != '\0' &&
                strcmp(dr->dispensed_at, admission.admitted_at) >= 0) {
                /* 如果已出院，检查是否在出院前 */
                if (admission.discharged_at[0] == '\0' ||
                    strcmp(dr->dispensed_at, admission.discharged_at) <= 0) {
                    Medicine medicine;
                    if (MedicineRepository_find_by_medicine_id(
                            &app->pharmacy_service.medicine_repository,
                            dr->medicine_id, &medicine).success) {
                        drug_total += medicine.price * dr->quantity;
                    }
                }
            }
            current = current->next;
        }
    }

    /* 加载就诊记录并查找住院期间的检查费用 */
    LinkedList_init(&visit_list);
    result = VisitRecordRepository_load_all(
        &app->medical_record_service.visit_repository,
        &visit_list
    );
    if (result.success) {
        current = visit_list.head;
        while (current != 0) {
            const VisitRecord *visit = (const VisitRecord *)current->data;
            if (strcmp(visit->patient_id, admission.patient_id) == 0 &&
                visit->visit_time[0] != '\0' &&
                strcmp(visit->visit_time, admission.admitted_at) >= 0) {
                if (admission.discharged_at[0] == '\0' ||
                    strcmp(visit->visit_time, admission.discharged_at) <= 0) {
                    LinkedList_init(&exam_list);
                    if (ExaminationRecordRepository_find_by_visit_id(
                            &app->medical_record_service.examination_repository,
                            visit->visit_id, &exam_list).success) {
                        const LinkedListNode *exam_node = exam_list.head;
                        while (exam_node != 0) {
                            const ExaminationRecord *exam =
                                (const ExaminationRecord *)exam_node->data;
                            exam_total += exam->exam_fee;
                            exam_node = exam_node->next;
                        }
                    }
                    ExaminationRecordRepository_clear_list(&exam_list);
                }
            }
            current = current->next;
        }
    }

    grand_total = bed_total + drug_total + exam_total;

    /* 生成结算单 */
    result = MenuApplication_write_text(
        buffer, capacity,
        TUI_STAR " 出院结算单"
    );
    if (result.success == 0) {
        PharmacyService_clear_dispense_record_results(&dispense_list);
        VisitRecordRepository_clear_list(&visit_list);
        return result;
    }
    used = strlen(buffer);

    result = MenuApplication_append_text(
        buffer, capacity, &used,
        "\n  住院编号: %s"
        "\n  患者编号: %s"
        "\n  病房: %s"
        "\n  入院时间: %s"
        "\n  住院天数: %d 天",
        admission.admission_id,
        admission.patient_id,
        ward.name,
        admission.admitted_at,
        days
    );

    result = MenuApplication_append_text(
        buffer, capacity, &used,
        "\n\n" TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH
        TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH
        TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH
        TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH
        TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH
    );

    result = MenuApplication_append_text(
        buffer, capacity, &used,
        "\n" TUI_BULLET " 床位费: %d 天 x %.2f 元/天 = %.2f 元",
        days,
        ward.daily_fee,
        bed_total
    );

    result = MenuApplication_append_text(
        buffer, capacity, &used,
        "\n" TUI_BULLET " 药品费: %.2f 元",
        drug_total
    );

    result = MenuApplication_append_text(
        buffer, capacity, &used,
        "\n" TUI_BULLET " 检查费: %.2f 元",
        exam_total
    );

    result = MenuApplication_append_text(
        buffer, capacity, &used,
        "\n\n" TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH
        TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH
        TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH
        TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH
        TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH TUI_DH
        "\n费用总计: %.2f 元",
        grand_total
    );

    PharmacyService_clear_dispense_record_results(&dispense_list);
    VisitRecordRepository_clear_list(&visit_list);
    return Result_make_success("discharge settlement ready");
}

/* ─── 住院医嘱业务函数 ──────────────���───────────────��──────────────── */

Result MenuApplication_create_inpatient_order(
    MenuApplication *application,
    const char *admission_id,
    const char *order_type,
    const char *content,
    const char *ordered_at,
    char *buffer,
    size_t capacity
) {
    InpatientOrder order;
    Result result;

    if (application == 0 || buffer == 0 || capacity == 0) {
        return Result_make_failure("create order arguments missing");
    }

    memset(&order, 0, sizeof(order));
    strncpy(order.admission_id, admission_id ? admission_id : "", sizeof(order.admission_id) - 1);
    strncpy(order.order_type, order_type ? order_type : "", sizeof(order.order_type) - 1);
    strncpy(order.content, content ? content : "", sizeof(order.content) - 1);
    strncpy(order.ordered_at, ordered_at ? ordered_at : "", sizeof(order.ordered_at) - 1);

    result = InpatientOrderService_create_order(
        &application->inpatient_order_service,
        &order
    );

    if (result.success) {
        snprintf(buffer, capacity,
            TUI_STAR " 医嘱创建成功\n"
            "  医嘱编号: %s\n"
            "  入院编号: %s\n"
            "  医嘱类型: %s\n"
            "  医嘱内容: %s\n"
            "  开具时间: %s\n"
            "  状  态: 待执行",
            order.order_id,
            order.admission_id,
            order.order_type,
            order.content,
            order.ordered_at
        );
    } else {
        snprintf(buffer, capacity, "医嘱创建失败: %s", result.message);
    }

    return result;
}

Result MenuApplication_query_orders_by_admission(
    MenuApplication *application,
    const char *admission_id,
    char *buffer,
    size_t capacity
) {
    LinkedList orders;
    LinkedListNode *current = 0;
    int used = 0;
    int count = 0;
    Result result;

    if (application == 0 || buffer == 0 || capacity == 0) {
        return Result_make_failure("query orders arguments missing");
    }

    result = InpatientOrderService_find_by_admission_id(
        &application->inpatient_order_service,
        admission_id,
        &orders
    );

    if (result.success == 0) {
        snprintf(buffer, capacity, "查询医嘱失败: %s", result.message);
        return result;
    }

    used = snprintf(buffer, capacity,
        TUI_MEDICAL " 入院记录 [%s] 的医嘱列表\n",
        admission_id ? admission_id : ""
    );

    current = orders.head;
    while (current != 0 && (size_t)used < capacity - 1) {
        const InpatientOrder *order = (const InpatientOrder *)current->data;
        const char *status_text = "待执行";
        const char *status_color = TUI_YELLOW;

        if (order->status == INPATIENT_ORDER_STATUS_EXECUTED) {
            status_text = "已执行";
            status_color = TUI_GREEN;
        } else if (order->status == INPATIENT_ORDER_STATUS_CANCELLED) {
            status_text = "已取消";
            status_color = TUI_RED;
        }

        used += snprintf(buffer + used, capacity - (size_t)used,
            "\n  " TUI_STAR " 医嘱编号: %s\n"
            "    类型: %s | 内容: %s\n"
            "    开具时间: %s\n"
            "    状态: %s%s" TUI_RESET "\n",
            order->order_id,
            order->order_type,
            order->content,
            order->ordered_at,
            status_color,
            status_text
        );

        if (order->status == INPATIENT_ORDER_STATUS_EXECUTED && order->executed_at[0] != '\0') {
            used += snprintf(buffer + used, capacity - (size_t)used,
                "    执行时间: %s\n", order->executed_at);
        }
        if (order->status == INPATIENT_ORDER_STATUS_CANCELLED && order->cancelled_at[0] != '\0') {
            used += snprintf(buffer + used, capacity - (size_t)used,
                "    取消时间: %s\n", order->cancelled_at);
        }

        count++;
        current = current->next;
    }

    if (count == 0) {
        used += snprintf(buffer + used, capacity - (size_t)used,
            "\n  (暂无医嘱记录)\n");
    } else {
        used += snprintf(buffer + used, capacity - (size_t)used,
            "\n  共 %d 条医嘱\n", count);
    }

    InpatientOrderService_clear_results(&orders);
    return Result_make_success("orders query ready");
}

Result MenuApplication_execute_inpatient_order(
    MenuApplication *application,
    const char *order_id,
    const char *executed_at,
    char *buffer,
    size_t capacity
) {
    Result result;

    if (application == 0 || buffer == 0 || capacity == 0) {
        return Result_make_failure("execute order arguments missing");
    }

    result = InpatientOrderService_execute_order(
        &application->inpatient_order_service,
        order_id,
        executed_at
    );

    if (result.success) {
        snprintf(buffer, capacity,
            TUI_MEDICAL " 医嘱执行成功\n"
            "  医嘱编号: %s\n"
            "  执行时间: %s\n"
            "  状  态: " TUI_GREEN "已执行" TUI_RESET,
            order_id ? order_id : "",
            executed_at ? executed_at : ""
        );
    } else {
        snprintf(buffer, capacity, "医嘱执行失败: %s", result.message);
    }

    return result;
}

Result MenuApplication_cancel_inpatient_order(
    MenuApplication *application,
    const char *order_id,
    const char *cancelled_at,
    char *buffer,
    size_t capacity
) {
    Result result;

    if (application == 0 || buffer == 0 || capacity == 0) {
        return Result_make_failure("cancel order arguments missing");
    }

    result = InpatientOrderService_cancel_order(
        &application->inpatient_order_service,
        order_id,
        cancelled_at
    );

    if (result.success) {
        snprintf(buffer, capacity,
            TUI_MEDICAL " 医嘱取消成功\n"
            "  医嘱编号: %s\n"
            "  取消时间: %s\n"
            "  状  态: " TUI_RED "已取消" TUI_RESET,
            order_id ? order_id : "",
            cancelled_at ? cancelled_at : ""
        );
    } else {
        snprintf(buffer, capacity, "医嘱取消失败: %s", result.message);
    }

    return result;
}

Result MenuApplication_create_nursing_record(
    MenuApplication *application,
    const char *admission_id,
    const char *record_type,
    const char *nurse_name,
    const char *content,
    const char *recorded_at,
    char *buffer,
    size_t capacity
) {
    NursingRecord record;
    Result result;

    if (application == 0 || buffer == 0 || capacity == 0) {
        return Result_make_failure("nursing record arguments missing");
    }

    memset(&record, 0, sizeof(record));
    strncpy(record.admission_id, admission_id ? admission_id : "", sizeof(record.admission_id) - 1);
    strncpy(record.nurse_name, nurse_name ? nurse_name : "", sizeof(record.nurse_name) - 1);
    strncpy(record.record_type, record_type ? record_type : "", sizeof(record.record_type) - 1);
    strncpy(record.content, content ? content : "", sizeof(record.content) - 1);
    strncpy(record.recorded_at, recorded_at ? recorded_at : "", sizeof(record.recorded_at) - 1);

    result = NursingRecordService_create_record(
        &application->nursing_record_service,
        &record
    );

    if (result.success) {
        snprintf(buffer, capacity,
            TUI_MEDICAL " 护理记录已创建\n"
            "  记录编号: " TUI_BOLD_CYAN "%s" TUI_RESET "\n"
            "  住院编号: %s\n"
            "  护理类型: " TUI_BOLD_YELLOW "%s" TUI_RESET "\n"
            "  护士姓名: %s\n"
            "  记录内容: %s\n"
            "  记录时间: %s",
            record.nursing_id,
            record.admission_id,
            record.record_type,
            record.nurse_name,
            record.content,
            record.recorded_at
        );
    } else {
        snprintf(buffer, capacity, "护理记录创建失败: %s", result.message);
    }

    return result;
}

Result MenuApplication_query_nursing_records(
    MenuApplication *application,
    const char *admission_id,
    char *buffer,
    size_t capacity
) {
    LinkedList records;
    LinkedListNode *current = 0;
    int written = 0;
    int count = 0;
    Result result;

    if (application == 0 || buffer == 0 || capacity == 0) {
        return Result_make_failure("nursing query arguments missing");
    }

    LinkedList_init(&records);
    result = NursingRecordService_find_by_admission_id(
        &application->nursing_record_service,
        admission_id,
        &records
    );

    if (result.success == 0) {
        snprintf(buffer, capacity, "查询护理记录失败: %s", result.message);
        return result;
    }

    count = (int)LinkedList_count(&records);
    if (count == 0) {
        NursingRecordService_clear_results(&records);
        snprintf(buffer, capacity, "该住院记录暂无护理记录");
        return Result_make_success("no nursing records");
    }

    written = snprintf(buffer, capacity,
        TUI_MEDICAL " 住院编号 %s 的护理记录 (共%d条)\n"
        "  %-10s %-8s %-10s %-8s %s\n"
        "  ---------- -------- ---------- -------- --------",
        admission_id ? admission_id : "", count,
        "记录编号", "类型", "护士", "时间", "内容"
    );

    current = records.head;
    while (current != 0 && written >= 0 && (size_t)written < capacity) {
        const NursingRecord *rec = (const NursingRecord *)current->data;
        const char *type_color = TUI_RESET;

        if (strcmp(rec->record_type, "体温") == 0) {
            type_color = TUI_RED;
        } else if (strcmp(rec->record_type, "血压") == 0) {
            type_color = TUI_BOLD_CYAN;
        } else if (strcmp(rec->record_type, "用药") == 0) {
            type_color = TUI_BOLD_YELLOW;
        } else if (strcmp(rec->record_type, "护理观察") == 0) {
            type_color = TUI_GREEN;
        } else if (strcmp(rec->record_type, "输液") == 0) {
            type_color = TUI_MAGENTA;
        }

        written += snprintf(buffer + written, capacity - (size_t)written,
            "\n  %-10s %s%-8s" TUI_RESET " %-10s %-8s %s",
            rec->nursing_id,
            type_color, rec->record_type,
            rec->nurse_name,
            rec->recorded_at,
            rec->content
        );
        current = current->next;
    }

    NursingRecordService_clear_results(&records);
    return Result_make_success("nursing records listed");
}

/* ─── 查房记录业务函数 ──────────────────────────────────────────────── */

Result MenuApplication_create_round_record(
    MenuApplication *application,
    const char *admission_id,
    const char *doctor_id,
    const char *findings,
    const char *plan,
    const char *rounded_at,
    char *buffer,
    size_t capacity
) {
    RoundRecord record;
    Result result;

    if (application == 0 || buffer == 0 || capacity == 0) {
        return Result_make_failure("round record arguments missing");
    }

    memset(&record, 0, sizeof(record));
    strncpy(record.admission_id, admission_id ? admission_id : "", sizeof(record.admission_id) - 1);
    strncpy(record.doctor_id, doctor_id ? doctor_id : "", sizeof(record.doctor_id) - 1);
    strncpy(record.findings, findings ? findings : "", sizeof(record.findings) - 1);
    strncpy(record.plan, plan ? plan : "", sizeof(record.plan) - 1);
    strncpy(record.rounded_at, rounded_at ? rounded_at : "", sizeof(record.rounded_at) - 1);

    result = RoundRecordService_create_record(
        &application->round_record_service,
        &record
    );

    if (result.success) {
        snprintf(buffer, capacity,
            TUI_MEDICAL " 查房记录已创建\n"
            "  记录编号: %s\n"
            "  住院编号: %s\n"
            "  查房医生: %s\n"
            "  查房时间: %s",
            record.round_id,
            record.admission_id,
            record.doctor_id,
            record.rounded_at);
    } else {
        snprintf(buffer, capacity, "创建查房记录失败: %s", result.message);
    }

    return result;
}

Result MenuApplication_query_round_records(
    MenuApplication *application,
    const char *admission_id,
    char *buffer,
    size_t capacity
) {
    LinkedList records;
    LinkedListNode *current = 0;
    size_t used = 0;
    int count = 0;
    Result result;

    if (application == 0 || buffer == 0 || capacity == 0) {
        return Result_make_failure("round query arguments missing");
    }

    buffer[0] = '\0';

    result = RoundRecordService_find_by_admission_id(
        &application->round_record_service,
        admission_id,
        &records
    );

    if (!result.success) {
        snprintf(buffer, capacity, "查询查房记录失败: %s", result.message);
        return result;
    }

    if (LinkedList_count(&records) == 0) {
        snprintf(buffer, capacity, "该住院记录暂无查房记录");
        RoundRecordService_clear_results(&records);
        return Result_make_success("no round records");
    }

    used = (size_t)snprintf(buffer, capacity,
        TUI_MEDICAL " 住院编号 %s 的查房记录 (共 %d 条)\n",
        admission_id, (int)LinkedList_count(&records));

    current = records.head;
    while (current != 0 && used < capacity - 1) {
        const RoundRecord *r = (const RoundRecord *)current->data;
        int written = 0;
        count++;
        written = snprintf(buffer + used, capacity - used,
            "\n  [%d] %s\n"
            "      医生: %s | 时间: %s\n"
            "      发现: %s\n"
            "      计划: %s",
            count, r->round_id,
            r->doctor_id, r->rounded_at,
            r->findings,
            r->plan);
        if (written < 0 || (size_t)written >= capacity - used) break;
        used += (size_t)written;
        current = current->next;
    }

    RoundRecordService_clear_results(&records);
    return Result_make_success("round records listed");
}

Result MenuApplication_execute_action(
    MenuApplication *application,
    MenuAction action,
    FILE *input,
    FILE *output
) {
    if (application == 0 || input == 0 || output == 0) {
        return Result_make_failure("menu action arguments missing");
    }

    /* Role-action authorization check */
    {
        int action_val = (int)action;
        UserRole required = USER_ROLE_UNKNOWN;
        if ((action_val >= 101 && action_val <= 105) || (action_val >= 206 && action_val <= 212)) {
            required = USER_ROLE_ADMIN;
        } else if (action_val >= 301 && action_val <= 399) {
            required = USER_ROLE_DOCTOR;
        } else if (action_val >= 401 && action_val <= 499) {
            required = USER_ROLE_PATIENT;
        } else if (action_val >= 501 && action_val <= 599) {
            required = USER_ROLE_INPATIENT_MANAGER;
        } else if (action_val >= 701 && action_val <= 799) {
            required = USER_ROLE_PHARMACY;
        }
        if (required != USER_ROLE_UNKNOWN && application->has_authenticated_user) {
            if (application->authenticated_user.role != required) {
                return Result_make_failure("permission denied: role mismatch");
            }
        }
    }

    switch (action) {
        case MENU_ACTION_ADMIN_PATIENT_MANAGEMENT:
        case MENU_ACTION_ADMIN_DOCTOR_DEPARTMENT:
        case MENU_ACTION_ADMIN_MEDICAL_RECORDS:
        case MENU_ACTION_ADMIN_WARD_BED_OVERVIEW:
        case MENU_ACTION_ADMIN_MEDICINE_OVERVIEW:
        case MENU_ACTION_ADMIN_STATS_REVENUE:
        case MENU_ACTION_ADMIN_STATS_WORKLOAD:
        case MENU_ACTION_ADMIN_STATS_BED_UTIL:
        case MENU_ACTION_ADMIN_STATS_MEDICINE:
        case MENU_ACTION_ADMIN_STATS_PATIENT_FLOW:
        case MENU_ACTION_ADMIN_STATS_DEPT_PERFORMANCE:
        case MENU_ACTION_ADMIN_STATS_ADMISSION_TURNOVER:
            return MenuAction_handle_admin(application, action, input, output);

        case MENU_ACTION_DOCTOR_PENDING_LIST:
        case MENU_ACTION_DOCTOR_QUERY_PATIENT_HISTORY:
        case MENU_ACTION_DOCTOR_VISIT_RECORD:
        case MENU_ACTION_DOCTOR_PRESCRIPTION_STOCK:
        case MENU_ACTION_DOCTOR_EXAM_RECORD:
        case MENU_ACTION_DOCTOR_ROUND_CREATE:
        case MENU_ACTION_DOCTOR_ROUND_QUERY:
            return MenuAction_handle_doctor(application, action, input, output);

        case MENU_ACTION_PATIENT_BASIC_INFO:
        case MENU_ACTION_PATIENT_SELF_REGISTER:
        case MENU_ACTION_PATIENT_QUERY_REGISTRATION:
        case MENU_ACTION_PATIENT_QUERY_VISITS:
        case MENU_ACTION_PATIENT_QUERY_EXAMS:
        case MENU_ACTION_PATIENT_QUERY_ADMISSIONS:
        case MENU_ACTION_PATIENT_QUERY_DISPENSE:
        case MENU_ACTION_PATIENT_QUERY_MEDICINE_USAGE:
        case MENU_ACTION_PATIENT_QUERY_FEES:
            return MenuAction_handle_patient(application, action, input, output);

        case MENU_ACTION_INPATIENT_QUERY_BED:
        case MENU_ACTION_INPATIENT_LIST_WARDS:
        case MENU_ACTION_INPATIENT_LIST_BEDS:
        case MENU_ACTION_INPATIENT_ADMIT:
        case MENU_ACTION_INPATIENT_DISCHARGE:
        case MENU_ACTION_INPATIENT_QUERY_RECORD:
        case MENU_ACTION_INPATIENT_TRANSFER_BED:
        case MENU_ACTION_INPATIENT_DISCHARGE_CHECK:
        case MENU_ACTION_INPATIENT_CREATE_ORDER:
        case MENU_ACTION_INPATIENT_QUERY_ORDERS:
        case MENU_ACTION_INPATIENT_EXECUTE_ORDER:
        case MENU_ACTION_INPATIENT_CANCEL_ORDER:
        case MENU_ACTION_INPATIENT_NURSING_CREATE:
        case MENU_ACTION_INPATIENT_NURSING_QUERY:
            return MenuAction_handle_inpatient(application, action, input, output);

        case MENU_ACTION_PHARMACY_ADD_MEDICINE:
        case MENU_ACTION_PHARMACY_RESTOCK:
        case MENU_ACTION_PHARMACY_DISPENSE:
        case MENU_ACTION_PHARMACY_QUERY_STOCK:
        case MENU_ACTION_PHARMACY_LOW_STOCK:
            return MenuAction_handle_pharmacy(application, action, input, output);

        default:
            fprintf(output, "当前动作尚未落地: %s\n", MenuController_action_label(action));
            return Result_make_failure("action not implemented yet");
    }
}
