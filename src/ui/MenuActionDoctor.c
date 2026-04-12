/**
 * @file MenuActionDoctor.c
 * @brief Doctor domain action handlers for the HIS menu system
 *
 * Handles: MENU_ACTION_DOCTOR_PENDING_LIST, MENU_ACTION_DOCTOR_QUERY_PATIENT_HISTORY,
 *          MENU_ACTION_DOCTOR_VISIT_RECORD, MENU_ACTION_DOCTOR_PRESCRIPTION_STOCK,
 *          MENU_ACTION_DOCTOR_EXAM_RECORD
 */

#include "ui/MenuActionHandlers.h"

#include <stdlib.h>
#include <string.h>
#include "ui/TuiStyle.h"
#include "domain/Prescription.h"

Result MenuAction_handle_doctor(MenuApplication *app, MenuAction action, FILE *input, FILE *output) {
    MenuApplicationPromptContext context;
    char output_buffer[2048];
    char first_id[HIS_DOMAIN_ID_CAPACITY];
    char second_id[HIS_DOMAIN_ID_CAPACITY];
    char third_id[HIS_DOMAIN_TEXT_CAPACITY];
    char text_value[HIS_DOMAIN_TEXT_CAPACITY];
    char time_value[HIS_DOMAIN_TIME_CAPACITY];
    char long_text[HIS_DOMAIN_LARGE_TEXT_CAPACITY];
    char fee_buffer[64];
    double exam_fee = 0;
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
            {
                FormPanel vr_panel;
                memset(&vr_panel, 0, sizeof(vr_panel));
                snprintf(vr_panel.title, sizeof(vr_panel.title), TUI_MEDICAL " \xe8\xaf\x8a\xe7\x96\x97\xe8\xae\xb0\xe5\xbd\x95");

                snprintf(vr_panel.fields[0].label, FORM_LABEL_CAPACITY, "\xe4\xb8\xbb\xe8\xaf\x89:");
                vr_panel.fields[0].is_required = 1;

                snprintf(vr_panel.fields[1].label, FORM_LABEL_CAPACITY, "\xe8\xaf\x8a\xe6\x96\xad\xe7\xbb\x93\xe6\x9e\x9c:");
                vr_panel.fields[1].is_required = 1;

                snprintf(vr_panel.fields[2].label, FORM_LABEL_CAPACITY, "\xe5\x8c\xbb\xe7\x94\x9f\xe5\xbb\xba\xe8\xae\xae:");
                vr_panel.fields[2].is_required = 1;

                snprintf(vr_panel.fields[3].label, FORM_LABEL_CAPACITY, "\xe9\x9c\x80\xe8\xa6\x81\xe6\xa3\x80\xe6\x9f\xa5:");
                snprintf(vr_panel.fields[3].hint, FORM_HINT_CAPACITY, "0\xe5\x90\xa6/1\xe6\x98\xaf");
                snprintf(vr_panel.fields[3].default_value, FORM_VALUE_CAPACITY, "0");

                snprintf(vr_panel.fields[4].label, FORM_LABEL_CAPACITY, "\xe9\x9c\x80\xe8\xa6\x81\xe4\xbd\x8f\xe9\x99\xa2:");
                snprintf(vr_panel.fields[4].hint, FORM_HINT_CAPACITY, "0\xe5\x90\xa6/1\xe6\x98\xaf");
                snprintf(vr_panel.fields[4].default_value, FORM_VALUE_CAPACITY, "0");

                snprintf(vr_panel.fields[5].label, FORM_LABEL_CAPACITY, "\xe9\x9c\x80\xe8\xa6\x81\xe7\x94\xa8\xe8\x8d\xaf:");
                snprintf(vr_panel.fields[5].hint, FORM_HINT_CAPACITY, "0\xe5\x90\xa6/1\xe6\x98\xaf");
                snprintf(vr_panel.fields[5].default_value, FORM_VALUE_CAPACITY, "0");

                snprintf(vr_panel.fields[6].label, FORM_LABEL_CAPACITY, "\xe5\xb0\xb1\xe8\xaf\x8a\xe6\x97\xb6\xe9\x97\xb4:");
                vr_panel.fields[6].is_required = 1;

                vr_panel.field_count = 7;

                result = MenuApplication_form_dispatch(&context, &vr_panel);
                if (result.success == 0) {
                    return result;
                }

                strncpy(text_value, vr_panel.fields[0].value, sizeof(text_value) - 1);
                strncpy(third_id, vr_panel.fields[1].value, sizeof(third_id) - 1);
                strncpy(long_text, vr_panel.fields[2].value, sizeof(long_text) - 1);
                flag_one = atoi(vr_panel.fields[3].value);
                flag_two = atoi(vr_panel.fields[4].value);
                flag_three = atoi(vr_panel.fields[5].value);
                strncpy(time_value, vr_panel.fields[6].value, sizeof(time_value) - 1);
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
            result = MenuApplication_prompt_line(
                &context,
                "\n  " TUI_BOLD_YELLOW "[1]" TUI_RESET " 查询药品库存\n"
                "  " TUI_BOLD_YELLOW "[2]" TUI_RESET " 开具处方\n"
                "  " TUI_BOLD_YELLOW "[3]" TUI_RESET " 查看处方\n"
                "\n请选择操作编号: ",
                first_id,
                sizeof(first_id)
            );
            if (result.success == 0) {
                return result;
            }
            if (strcmp(first_id, "1") == 0) {
                result = MenuApplication_prompt_select_medicine(
                    app,
                    &context,
                    "药品搜索关键字/编号(回车列出全部): ",
                    "",
                    second_id,
                    sizeof(second_id)
                );
                if (result.success == 0) {
                    return result;
                }
                result = MenuApplication_query_medicine_stock(
                    app,
                    second_id,
                    output_buffer,
                    sizeof(output_buffer)
                );
            } else if (strcmp(first_id, "2") == 0) {
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
                result = MenuApplication_prompt_select_medicine(
                    app,
                    &context,
                    "药品搜索关键字/编号(回车列出全部): ",
                    "",
                    third_id,
                    sizeof(third_id)
                );
                if (result.success == 0) {
                    return result;
                }
                {
                    FormPanel rx_panel;
                    memset(&rx_panel, 0, sizeof(rx_panel));
                    snprintf(rx_panel.title, sizeof(rx_panel.title), TUI_MEDICAL " \xe5\xbc\x80\xe5\x85\xb7\xe5\xa4\x84\xe6\x96\xb9");

                    snprintf(rx_panel.fields[0].label, FORM_LABEL_CAPACITY, "\xe5\xbc\x80\xe8\x8d\xaf\xe6\x95\xb0\xe9\x87\x8f:");
                    rx_panel.fields[0].is_required = 1;

                    snprintf(rx_panel.fields[1].label, FORM_LABEL_CAPACITY, "\xe7\x94\xa8\xe6\xb3\x95\xe7\x94\xa8\xe9\x87\x8f:");
                    snprintf(rx_panel.fields[1].hint, FORM_HINT_CAPACITY, "\xe5\xa6\x82\xe6\xaf\x8f\xe6\x97\xa5" "3\xe6\xac\xa1\xe6\xaf\x8f\xe6\xac\xa1" "1\xe7\x89\x87");
                    rx_panel.fields[1].is_required = 1;

                    rx_panel.field_count = 2;

                    result = MenuApplication_form_dispatch(&context, &rx_panel);
                    if (result.success == 0) {
                        return result;
                    }

                    flag_one = atoi(rx_panel.fields[0].value);
                    strncpy(text_value, rx_panel.fields[1].value, sizeof(text_value) - 1);
                }
                {
                    Prescription prescription;
                    memset(&prescription, 0, sizeof(prescription));
                    MenuApplication_copy_text(prescription.visit_id, sizeof(prescription.visit_id), second_id);
                    MenuApplication_copy_text(prescription.medicine_id, sizeof(prescription.medicine_id), third_id);
                    prescription.quantity = flag_one;
                    MenuApplication_copy_text(prescription.usage, sizeof(prescription.usage), text_value);
                    tui_spinner_run(output, "正在保存处方...", 500);
                    result = MenuApplication_create_prescription(
                        app,
                        &prescription,
                        output_buffer,
                        sizeof(output_buffer)
                    );
                }
            } else if (strcmp(first_id, "3") == 0) {
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
                result = MenuApplication_query_prescriptions_by_visit(
                    app,
                    second_id,
                    output_buffer,
                    sizeof(output_buffer)
                );
            } else {
                return Result_make_failure("invalid prescription action");
            }
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
                {
                    FormPanel exam_panel;
                    memset(&exam_panel, 0, sizeof(exam_panel));
                    snprintf(exam_panel.title, sizeof(exam_panel.title), TUI_MEDICAL " \xe6\xa3\x80\xe6\x9f\xa5\xe7\x94\xb3\xe8\xaf\xb7");

                    snprintf(exam_panel.fields[0].label, FORM_LABEL_CAPACITY, "\xe6\xa3\x80\xe6\x9f\xa5\xe9\xa1\xb9\xe7\x9b\xae:");
                    exam_panel.fields[0].is_required = 1;

                    snprintf(exam_panel.fields[1].label, FORM_LABEL_CAPACITY, "\xe6\xa3\x80\xe6\x9f\xa5\xe7\xb1\xbb\xe5\x9e\x8b:");
                    snprintf(exam_panel.fields[1].hint, FORM_HINT_CAPACITY, "\xe5\xa6\x82\xe5\x8c\x96\xe9\xaa\x8c/\xe5\xbd\xb1\xe5\x83\x8f/\xe8\xb6\x85\xe5\xa3\xb0");
                    exam_panel.fields[1].is_required = 1;

                    snprintf(exam_panel.fields[2].label, FORM_LABEL_CAPACITY, "\xe6\xa3\x80\xe6\x9f\xa5\xe8\xb4\xb9\xe7\x94\xa8:");
                    snprintf(exam_panel.fields[2].hint, FORM_HINT_CAPACITY, "\xe5\x85\x83");
                    exam_panel.fields[2].is_required = 1;

                    snprintf(exam_panel.fields[3].label, FORM_LABEL_CAPACITY, "\xe7\x94\xb3\xe8\xaf\xb7\xe6\x97\xb6\xe9\x97\xb4:");
                    exam_panel.fields[3].is_required = 1;

                    exam_panel.field_count = 4;

                    result = MenuApplication_form_dispatch(&context, &exam_panel);
                    if (result.success == 0) {
                        return result;
                    }

                    strncpy(text_value, exam_panel.fields[0].value, sizeof(text_value) - 1);
                    strncpy(third_id, exam_panel.fields[1].value, sizeof(third_id) - 1);
                    strncpy(fee_buffer, exam_panel.fields[2].value, sizeof(fee_buffer) - 1);
                    strncpy(time_value, exam_panel.fields[3].value, sizeof(time_value) - 1);
                }
                {
                    char *end_ptr = 0;
                    exam_fee = strtod(fee_buffer, &end_ptr);
                    if (end_ptr == fee_buffer || end_ptr == 0) {
                        return Result_make_failure("exam fee invalid");
                    }
                    while (*end_ptr != '\0') {
                        if (*end_ptr != ' ' && *end_ptr != '\t') {
                            return Result_make_failure("exam fee invalid");
                        }
                        end_ptr++;
                    }
                    if (exam_fee < 0) {
                        return Result_make_failure("exam fee must be non-negative");
                    }
                }
                result = MenuApplication_create_examination_record(
                    app,
                    second_id,
                    text_value,
                    third_id,
                    exam_fee,
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

        case MENU_ACTION_DOCTOR_ROUND_CREATE:
            result = MenuApplication_prompt_select_admission(
                app,
                &context,
                "住院记录搜索关键字/编号(回车列出全部): ",
                "",
                0,
                first_id,
                sizeof(first_id)
            );
            if (result.success == 0) {
                return result;
            }
            {
                FormPanel round_panel;
                char plan_text[HIS_DOMAIN_LARGE_TEXT_CAPACITY];
                memset(&round_panel, 0, sizeof(round_panel));
                memset(plan_text, 0, sizeof(plan_text));
                snprintf(round_panel.title, sizeof(round_panel.title), TUI_MEDICAL " \xe6\x9f\xa5\xe6\x88\xbf\xe8\xae\xb0\xe5\xbd\x95");

                snprintf(round_panel.fields[0].label, FORM_LABEL_CAPACITY, "\xe6\x9f\xa5\xe6\x88\xbf\xe5\x8f\x91\xe7\x8e\xb0:");
                round_panel.fields[0].is_required = 1;

                snprintf(round_panel.fields[1].label, FORM_LABEL_CAPACITY, "\xe6\xb2\xbb\xe7\x96\x97\xe8\xae\xa1\xe5\x88\x92:");
                round_panel.fields[1].is_required = 1;

                snprintf(round_panel.fields[2].label, FORM_LABEL_CAPACITY, "\xe6\x9f\xa5\xe6\x88\xbf\xe6\x97\xb6\xe9\x97\xb4:");
                round_panel.fields[2].is_required = 1;

                round_panel.field_count = 3;

                result = MenuApplication_form_dispatch(&context, &round_panel);
                if (result.success == 0) {
                    return result;
                }

                strncpy(long_text, round_panel.fields[0].value, sizeof(long_text) - 1);
                strncpy(plan_text, round_panel.fields[1].value, sizeof(plan_text) - 1);
                strncpy(time_value, round_panel.fields[2].value, sizeof(time_value) - 1);

                tui_spinner_run(output, "正在保存查房记录...", 500);
                result = MenuApplication_create_round_record(
                    app,
                    first_id,
                    app->has_authenticated_user != 0 ? app->authenticated_user.user_id : "",
                    long_text,
                    plan_text,
                    time_value,
                    output_buffer,
                    sizeof(output_buffer)
                );
            }
            MenuApplication_print_result(output, output_buffer, result.success);
            return result;

        case MENU_ACTION_DOCTOR_ROUND_QUERY:
            result = MenuApplication_prompt_select_admission(
                app,
                &context,
                "住院记录搜索关键字/编号(回车列出全部): ",
                "",
                0,
                first_id,
                sizeof(first_id)
            );
            if (result.success == 0) {
                return result;
            }
            result = MenuApplication_query_round_records(
                app,
                first_id,
                output_buffer,
                sizeof(output_buffer)
            );
            MenuApplication_print_result(output, output_buffer, result.success);
            return result;

        default:
            return Result_make_failure("unknown doctor action");
    }
}
