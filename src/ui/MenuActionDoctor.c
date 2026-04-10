/**
 * @file MenuActionDoctor.c
 * @brief Doctor domain action handlers for the HIS menu system
 *
 * Handles: MENU_ACTION_DOCTOR_PENDING_LIST, MENU_ACTION_DOCTOR_QUERY_PATIENT_HISTORY,
 *          MENU_ACTION_DOCTOR_VISIT_RECORD, MENU_ACTION_DOCTOR_PRESCRIPTION_STOCK,
 *          MENU_ACTION_DOCTOR_EXAM_RECORD
 */

#include "ui/MenuActionHandlers.h"

#include <string.h>
#include "ui/TuiStyle.h"

Result MenuAction_handle_doctor(MenuApplication *app, MenuAction action, FILE *input, FILE *output) {
    MenuApplicationPromptContext context;
    char output_buffer[2048];
    char first_id[HIS_DOMAIN_ID_CAPACITY];
    char second_id[HIS_DOMAIN_ID_CAPACITY];
    char third_id[HIS_DOMAIN_TEXT_CAPACITY];
    char text_value[HIS_DOMAIN_TEXT_CAPACITY];
    char time_value[HIS_DOMAIN_TIME_CAPACITY];
    char long_text[HIS_DOMAIN_LARGE_TEXT_CAPACITY];
    int flag_one = 0;
    int flag_two = 0;
    int flag_three = 0;
    Result result;

    context.input = input;
    context.output = output;
    memset(output_buffer, 0, sizeof(output_buffer));
    memset(first_id, 0, sizeof(first_id));
    memset(second_id, 0, sizeof(second_id));
    memset(third_id, 0, sizeof(third_id));
    memset(text_value, 0, sizeof(text_value));
    memset(time_value, 0, sizeof(time_value));
    memset(long_text, 0, sizeof(long_text));

    switch (action) {
        case MENU_ACTION_DOCTOR_PENDING_LIST:
            if (app->has_authenticated_user != 0 && app->authenticated_user.role == USER_ROLE_DOCTOR) {
                MenuApplication_copy_text(first_id, sizeof(first_id), app->authenticated_user.user_id);
            } else {
                result = MenuApplication_prompt_select_doctor(
                    app,
                    &context,
                    "\xe5\x8c\xbb\xe7\x94\x9f\xe6\x90\x9c\xe7\xb4\xa2\xe5\x85\xb3\xe9\x94\xae\xe5\xad\x97/\xe5\xb7\xa5\xe5\x8f\xb7(\xe5\x9b\x9e\xe8\xbd\xa6\xe5\x88\x97\xe5\x87\xba\xe5\x85\xa8\xe9\x83\xa8): ",
                    "",
                    first_id,
                    sizeof(first_id)
                );
                if (result.success == 0) {
                    return result;
                }
            }
            {
                char selected_registration_id[HIS_DOMAIN_ID_CAPACITY] = {0};
                result = MenuApplication_browse_pending_table(app, &context, first_id, selected_registration_id, sizeof(selected_registration_id));
                if (result.success && selected_registration_id[0] != '\0') {
                    result = MenuApplication_query_registration(
                        app,
                        selected_registration_id,
                        output_buffer,
                        sizeof(output_buffer)
                    );
                    MenuApplication_print_result(output, output_buffer, result.success);
                }
                return Result_make_success("browse complete");
            }

        case MENU_ACTION_DOCTOR_QUERY_PATIENT_HISTORY:
            result = MenuApplication_prompt_select_patient(
                app,
                &context,
                "患者搜索关键字/编号(回车列出全部): ",
                first_id,
                sizeof(first_id)
            );
            if (result.success == 0) {
                return result;
            }
            result = MenuApplication_query_patient_history(
                app,
                first_id,
                output_buffer,
                sizeof(output_buffer)
            );
            MenuApplication_print_result(output, output_buffer, result.success);
            return result;

        case MENU_ACTION_DOCTOR_VISIT_RECORD:
            result = MenuApplication_prompt_select_registration(
                app,
                &context,
                "待接诊挂号搜索关键字/编号(回车列出全部): ",
                "",
                app->has_authenticated_user != 0 ? app->authenticated_user.user_id : "",
                REG_STATUS_PENDING,
                1,
                first_id,
                sizeof(first_id)
            );
            if (result.success == 0) {
                return result;
            }
            result = MenuApplication_prompt_line(
                &context,
                "主诉: ",
                text_value,
                sizeof(text_value)
            );
            if (result.success == 0) {
                return result;
            }
            result = MenuApplication_prompt_line(
                &context,
                "诊断结果: ",
                third_id,
                sizeof(third_id)
            );
            if (result.success == 0) {
                return result;
            }
            result = MenuApplication_prompt_line(
                &context,
                "医生建议: ",
                long_text,
                sizeof(long_text)
            );
            if (result.success == 0) {
                return result;
            }
            result = MenuApplication_prompt_int(&context, "是否需要检查(0/1): ", &flag_one);
            if (result.success == 0) {
                return result;
            }
            result = MenuApplication_prompt_int(&context, "是否需要住院(0/1): ", &flag_two);
            if (result.success == 0) {
                return result;
            }
            result = MenuApplication_prompt_int(&context, "是否需要用药(0/1): ", &flag_three);
            if (result.success == 0) {
                return result;
            }
            result = MenuApplication_prompt_line(
                &context,
                "就诊时间: ",
                time_value,
                sizeof(time_value)
            );
            if (result.success == 0) {
                return result;
            }
            tui_spinner_run(output, "正在保存诊疗记录...", 500);
            result = MenuApplication_create_visit_record(
                app,
                first_id,
                text_value,
                third_id,
                long_text,
                flag_one,
                flag_two,
                flag_three,
                time_value,
                output_buffer,
                sizeof(output_buffer)
            );
            MenuApplication_print_result(output, output_buffer, result.success);
            return result;

        case MENU_ACTION_DOCTOR_PRESCRIPTION_STOCK:
            result = MenuApplication_prompt_select_medicine(
                app,
                &context,
                "药品搜索关键字/编号(回车列出全部): ",
                "",
                first_id,
                sizeof(first_id)
            );
            if (result.success == 0) {
                return result;
            }
            result = MenuApplication_query_medicine_stock(
                app,
                first_id,
                output_buffer,
                sizeof(output_buffer)
            );
            MenuApplication_print_result(output, output_buffer, result.success);
            return result;

        case MENU_ACTION_DOCTOR_EXAM_RECORD:
            result = MenuApplication_prompt_line(
                &context,
                "\n  " TUI_BOLD_YELLOW "[1]" TUI_RESET " 新增检查\n"
                "  " TUI_BOLD_YELLOW "[2]" TUI_RESET " 回写结果\n"
                "\n请选择操作编号: ",
                first_id,
                sizeof(first_id)
            );
            if (result.success == 0) {
                return result;
            }
            if (strcmp(first_id, "1") == 0) {
                result = MenuApplication_prompt_select_visit(
                    app,
                    &context,
                    "就诊搜索关键字/编号(回车列出全部): ",
                    "",
                    app->has_authenticated_user != 0 ? app->authenticated_user.user_id : "",
                    second_id,
                    sizeof(second_id)
                );
                if (result.success == 0) {
                    return result;
                }
                result = MenuApplication_prompt_line(
                    &context,
                    "检查项目: ",
                    text_value,
                    sizeof(text_value)
                );
                if (result.success == 0) {
                    return result;
                }
                result = MenuApplication_prompt_line(
                    &context,
                    "检查类型: ",
                    third_id,
                    sizeof(third_id)
                );
                if (result.success == 0) {
                    return result;
                }
                result = MenuApplication_prompt_line(
                    &context,
                    "申请时间: ",
                    time_value,
                    sizeof(time_value)
                );
                if (result.success == 0) {
                    return result;
                }
                result = MenuApplication_create_examination_record(
                    app,
                    second_id,
                    text_value,
                    third_id,
                    time_value,
                    output_buffer,
                    sizeof(output_buffer)
                );
            } else if (strcmp(first_id, "2") == 0) {
                result = MenuApplication_prompt_select_examination(
                    app,
                    &context,
                    "待回写检查搜索关键字/编号(回车列出全部): ",
                    app->has_authenticated_user != 0 ? app->authenticated_user.user_id : "",
                    1,
                    second_id,
                    sizeof(second_id)
                );
                if (result.success == 0) {
                    return result;
                }
                result = MenuApplication_prompt_line(
                    &context,
                    "检查结果: ",
                    long_text,
                    sizeof(long_text)
                );
                if (result.success == 0) {
                    return result;
                }
                result = MenuApplication_prompt_line(
                    &context,
                    "完成时间: ",
                    time_value,
                    sizeof(time_value)
                );
                if (result.success == 0) {
                    return result;
                }
                result = MenuApplication_complete_examination_record(
                    app,
                    second_id,
                    long_text,
                    time_value,
                    output_buffer,
                    sizeof(output_buffer)
                );
            } else {
                return Result_make_failure("invalid exam action");
            }
            MenuApplication_print_result(output, output_buffer, result.success);
            return result;

        default:
            return Result_make_failure("unknown doctor action");
    }
}
