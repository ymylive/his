/**
 * @file MenuActionInpatient.c
 * @brief Inpatient domain action handlers for the HIS menu system
 *
 * Handles: MENU_ACTION_INPATIENT_QUERY_BED, MENU_ACTION_INPATIENT_LIST_WARDS,
 *          MENU_ACTION_INPATIENT_LIST_BEDS, MENU_ACTION_INPATIENT_ADMIT,
 *          MENU_ACTION_INPATIENT_DISCHARGE, MENU_ACTION_INPATIENT_QUERY_RECORD,
 *          MENU_ACTION_INPATIENT_TRANSFER_BED, MENU_ACTION_INPATIENT_DISCHARGE_CHECK
 */

#include "ui/MenuActionHandlers.h"

#include <string.h>
#include "ui/TuiStyle.h"

Result MenuAction_handle_inpatient(MenuApplication *app, MenuAction action, FILE *input, FILE *output) {
    MenuApplicationPromptContext context;
    char output_buffer[2048];
    char first_id[HIS_DOMAIN_ID_CAPACITY];
    char second_id[HIS_DOMAIN_ID_CAPACITY];
    char third_id[HIS_DOMAIN_TEXT_CAPACITY];
    char time_value[HIS_DOMAIN_TIME_CAPACITY];
    char long_text[HIS_DOMAIN_LARGE_TEXT_CAPACITY];
    Result result;

    context.input = input;
    context.output = output;
    memset(output_buffer, 0, sizeof(output_buffer));
    memset(first_id, 0, sizeof(first_id));
    memset(second_id, 0, sizeof(second_id));
    memset(third_id, 0, sizeof(third_id));
    memset(time_value, 0, sizeof(time_value));
    memset(long_text, 0, sizeof(long_text));

    switch (action) {
        case MENU_ACTION_INPATIENT_QUERY_BED:
        case MENU_ACTION_INPATIENT_LIST_WARDS:
        case MENU_ACTION_INPATIENT_LIST_BEDS:
            /* All three actions share the same ward->bed browse flow */
        {
            char selected_ward_id[HIS_DOMAIN_ID_CAPACITY] = {0};
            result = MenuApplication_browse_ward_table(app, &context, selected_ward_id, sizeof(selected_ward_id));
            if (result.success && selected_ward_id[0] != '\0') {
                MenuApplication_browse_bed_table(app, &context, selected_ward_id, 0, 0);
            }
            return Result_make_success("browse complete");
        }

        case MENU_ACTION_INPATIENT_ADMIT:
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
            result = MenuApplication_prompt_select_ward(
                app,
                &context,
                "病区搜索关键字/编号(回车列出全部): ",
                second_id,
                sizeof(second_id)
            );
            if (result.success == 0) {
                return result;
            }
            result = MenuApplication_prompt_select_bed(
                app,
                &context,
                "床位搜索关键字/编号(回车列出全部): ",
                second_id,
                1,
                0,
                third_id,
                sizeof(third_id)
            );
            if (result.success == 0) {
                return result;
            }
            {
                FormPanel admit_panel;
                memset(&admit_panel, 0, sizeof(admit_panel));
                snprintf(admit_panel.title, sizeof(admit_panel.title), TUI_MEDICAL " \xe5\x85\xa5\xe9\x99\xa2\xe7\x99\xbb\xe8\xae\xb0");

                snprintf(admit_panel.fields[0].label, FORM_LABEL_CAPACITY, "\xe5\x85\xa5\xe9\x99\xa2\xe6\x97\xb6\xe9\x97\xb4:");
                admit_panel.fields[0].is_required = 1;

                snprintf(admit_panel.fields[1].label, FORM_LABEL_CAPACITY, "\xe4\xbd\x8f\xe9\x99\xa2\xe6\x91\x98\xe8\xa6\x81:");
                snprintf(admit_panel.fields[1].default_value, FORM_VALUE_CAPACITY, "observation");

                admit_panel.field_count = 2;

                result = MenuApplication_form_dispatch(&context, &admit_panel);
                if (result.success == 0) {
                    return result;
                }

                strncpy(time_value, admit_panel.fields[0].value, sizeof(time_value) - 1);
                strncpy(long_text, admit_panel.fields[1].value, sizeof(long_text) - 1);
            }
            tui_spinner_run(output, "正在办理入院手续...", 600);
            result = MenuApplication_admit_patient(
                app,
                first_id,
                second_id,
                third_id,
                time_value,
                long_text,
                output_buffer,
                sizeof(output_buffer)
            );
            MenuApplication_print_result(output, output_buffer, result.success);
            return result;

        case MENU_ACTION_INPATIENT_DISCHARGE:
            result = MenuApplication_prompt_select_admission(
                app,
                &context,
                "住院记录搜索关键字/编号(回车列出全部): ",
                "",
                1,
                first_id,
                sizeof(first_id)
            );
            if (result.success == 0) {
                return result;
            }
            result = MenuApplication_prompt_line(
                &context,
                "出院时间: ",
                time_value,
                sizeof(time_value)
            );
            if (result.success == 0) {
                return result;
            }
            result = MenuApplication_prompt_line(
                &context,
                "出院小结: ",
                long_text,
                sizeof(long_text)
            );
            if (result.success == 0) {
                return result;
            }

            /* 显示出院结算单 */
            {
                char settlement_buffer[4096];
                char confirm[HIS_DOMAIN_ID_CAPACITY];
                memset(settlement_buffer, 0, sizeof(settlement_buffer));
                result = MenuApplication_discharge_settlement(
                    app,
                    first_id,
                    settlement_buffer,
                    sizeof(settlement_buffer)
                );
                if (result.success) {
                    MenuApplication_print_result(output, settlement_buffer, 1);
                }
                result = MenuApplication_prompt_line(
                    &context,
                    "确认出院并结算? (Enter确认, ESC取消): ",
                    confirm,
                    sizeof(confirm)
                );
                if (result.success == 0) {
                    return result;
                }
            }

            tui_spinner_run(output, "正在办理出院手续...", 600);
            result = MenuApplication_discharge_patient(
                app,
                first_id,
                time_value,
                long_text,
                output_buffer,
                sizeof(output_buffer)
            );
            MenuApplication_print_result(output, output_buffer, result.success);
            return result;

        case MENU_ACTION_INPATIENT_QUERY_RECORD:
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
            result = MenuApplication_query_active_admission_by_patient(
                app,
                first_id,
                output_buffer,
                sizeof(output_buffer)
            );
            MenuApplication_print_result(output, output_buffer, result.success);
            return result;

        case MENU_ACTION_INPATIENT_TRANSFER_BED:
            result = MenuApplication_prompt_select_admission(
                app,
                &context,
                "在院住院记录搜索关键字/编号(回车列出全部): ",
                "",
                1,
                first_id,
                sizeof(first_id)
            );
            if (result.success == 0) {
                return result;
            }
            result = MenuApplication_prompt_select_bed(
                app,
                &context,
                "目标床位搜索关键字/编号(回车列出全部): ",
                "",
                1,
                0,
                second_id,
                sizeof(second_id)
            );
            if (result.success == 0) {
                return result;
            }
            result = MenuApplication_prompt_line(
                &context,
                "转床时间: ",
                time_value,
                sizeof(time_value)
            );
            if (result.success == 0) {
                return result;
            }
            result = MenuApplication_transfer_bed(
                app,
                first_id,
                second_id,
                time_value,
                output_buffer,
                sizeof(output_buffer)
            );
            MenuApplication_print_result(output, output_buffer, result.success);
            return result;

        case MENU_ACTION_INPATIENT_DISCHARGE_CHECK:
            result = MenuApplication_prompt_select_admission(
                app,
                &context,
                "在院住院记录搜索关键字/编号(回车列出全部): ",
                "",
                1,
                first_id,
                sizeof(first_id)
            );
            if (result.success == 0) {
                return result;
            }
            result = MenuApplication_discharge_check(
                app,
                first_id,
                output_buffer,
                sizeof(output_buffer)
            );
            MenuApplication_print_result(output, output_buffer, result.success);
            return result;

        case MENU_ACTION_INPATIENT_CREATE_ORDER:
            tui_print_section(output, TUI_STAR, "开具住院医嘱");
            result = MenuApplication_prompt_select_admission(
                app,
                &context,
                "住院记录搜索关键字/编号(回车列出全部): ",
                "",
                1,
                first_id,
                sizeof(first_id)
            );
            if (result.success == 0) {
                return result;
            }
            {
                char order_type_choice[16] = {0};
                const char *order_type_labels[] = {
                    "用药", "检查", "护理", "饮食", "输液"
                };
                int type_index = 0;

                fprintf(output,
                    "\n  医嘱类型:\n"
                    "    1. 用药\n"
                    "    2. 检查\n"
                    "    3. 护理\n"
                    "    4. 饮食\n"
                    "    5. 输液\n"
                );
                result = MenuApplication_prompt_line(
                    &context,
                    "请选择医嘱类型(1-5): ",
                    order_type_choice,
                    sizeof(order_type_choice)
                );
                if (result.success == 0) {
                    return result;
                }
                type_index = order_type_choice[0] - '1';
                if (type_index < 0 || type_index > 4) {
                    MenuApplication_print_result(output, "无效的医嘱类型选择", 0);
                    return Result_make_failure("invalid order type");
                }
                strncpy(third_id, order_type_labels[type_index], sizeof(third_id) - 1);
            }
            {
                FormPanel order_panel;
                memset(&order_panel, 0, sizeof(order_panel));
                snprintf(order_panel.title, sizeof(order_panel.title), TUI_MEDICAL " \xe5\xbc\x80\xe5\x85\xb7\xe5\x8c\xbb\xe5\x98\xb1");

                snprintf(order_panel.fields[0].label, FORM_LABEL_CAPACITY, "\xe5\x8c\xbb\xe5\x98\xb1\xe5\x86\x85\xe5\xae\xb9:");
                order_panel.fields[0].is_required = 1;

                snprintf(order_panel.fields[1].label, FORM_LABEL_CAPACITY, "\xe5\xbc\x80\xe5\x85\xb7\xe6\x97\xb6\xe9\x97\xb4:");
                order_panel.fields[1].is_required = 1;

                order_panel.field_count = 2;

                result = MenuApplication_form_dispatch(&context, &order_panel);
                if (result.success == 0) {
                    return result;
                }

                strncpy(long_text, order_panel.fields[0].value, sizeof(long_text) - 1);
                strncpy(time_value, order_panel.fields[1].value, sizeof(time_value) - 1);
            }
            tui_spinner_run(output, "正在创建医嘱...", 400);
            result = MenuApplication_create_inpatient_order(
                app,
                first_id,
                third_id,
                long_text,
                time_value,
                output_buffer,
                sizeof(output_buffer)
            );
            MenuApplication_print_result(output, output_buffer, result.success);
            return result;

        case MENU_ACTION_INPATIENT_QUERY_ORDERS:
            tui_print_section(output, TUI_MEDICAL, "查看住院医嘱");
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
            tui_spinner_run(output, "正在查询医嘱...", 300);
            result = MenuApplication_query_orders_by_admission(
                app,
                first_id,
                output_buffer,
                sizeof(output_buffer)
            );
            MenuApplication_print_result(output, output_buffer, result.success);
            return result;

        case MENU_ACTION_INPATIENT_EXECUTE_ORDER:
            tui_print_section(output, TUI_MEDICAL, "执行住院医嘱");
            {
                FormPanel exec_panel;
                memset(&exec_panel, 0, sizeof(exec_panel));
                snprintf(exec_panel.title, sizeof(exec_panel.title), TUI_MEDICAL " \xe6\x89\xa7\xe8\xa1\x8c\xe5\x8c\xbb\xe5\x98\xb1");

                snprintf(exec_panel.fields[0].label, FORM_LABEL_CAPACITY, "\xe5\x8c\xbb\xe5\x98\xb1\xe7\xbc\x96\xe5\x8f\xb7:");
                exec_panel.fields[0].is_required = 1;

                snprintf(exec_panel.fields[1].label, FORM_LABEL_CAPACITY, "\xe6\x89\xa7\xe8\xa1\x8c\xe6\x97\xb6\xe9\x97\xb4:");
                exec_panel.fields[1].is_required = 1;

                exec_panel.field_count = 2;

                result = MenuApplication_form_dispatch(&context, &exec_panel);
                if (result.success == 0) {
                    return result;
                }

                strncpy(first_id, exec_panel.fields[0].value, sizeof(first_id) - 1);
                strncpy(time_value, exec_panel.fields[1].value, sizeof(time_value) - 1);
            }
            tui_spinner_run(output, "正在执行医嘱...", 400);
            result = MenuApplication_execute_inpatient_order(
                app,
                first_id,
                time_value,
                output_buffer,
                sizeof(output_buffer)
            );
            MenuApplication_print_result(output, output_buffer, result.success);
            return result;

        case MENU_ACTION_INPATIENT_CANCEL_ORDER:
            tui_print_section(output, TUI_MEDICAL, "取消住院医嘱");
            {
                FormPanel cancel_panel;
                memset(&cancel_panel, 0, sizeof(cancel_panel));
                snprintf(cancel_panel.title, sizeof(cancel_panel.title), TUI_MEDICAL " \xe5\x8f\x96\xe6\xb6\x88\xe5\x8c\xbb\xe5\x98\xb1");

                snprintf(cancel_panel.fields[0].label, FORM_LABEL_CAPACITY, "\xe5\x8c\xbb\xe5\x98\xb1\xe7\xbc\x96\xe5\x8f\xb7:");
                cancel_panel.fields[0].is_required = 1;

                snprintf(cancel_panel.fields[1].label, FORM_LABEL_CAPACITY, "\xe5\x8f\x96\xe6\xb6\x88\xe6\x97\xb6\xe9\x97\xb4:");
                cancel_panel.fields[1].is_required = 1;

                cancel_panel.field_count = 2;

                result = MenuApplication_form_dispatch(&context, &cancel_panel);
                if (result.success == 0) {
                    return result;
                }

                strncpy(first_id, cancel_panel.fields[0].value, sizeof(first_id) - 1);
                strncpy(time_value, cancel_panel.fields[1].value, sizeof(time_value) - 1);
            }
            tui_spinner_run(output, "正在取消医嘱...", 400);
            result = MenuApplication_cancel_inpatient_order(
                app,
                first_id,
                time_value,
                output_buffer,
                sizeof(output_buffer)
            );
            MenuApplication_print_result(output, output_buffer, result.success);
            return result;

        case MENU_ACTION_INPATIENT_NURSING_CREATE:
            tui_print_section(output, TUI_MEDICAL, "记录护理");
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
                char type_choice[16] = {0};
                const char *type_labels[] = {
                    "体温", "血压", "用药", "护理观察", "输液"
                };
                int type_index = 0;

                fprintf(output,
                    "\n  护理类型:\n"
                    "    1. 体温\n"
                    "    2. 血压\n"
                    "    3. 用药\n"
                    "    4. 护理观察\n"
                    "    5. 输液\n"
                );
                result = MenuApplication_prompt_line(
                    &context,
                    "请选择护理类型(1-5): ",
                    type_choice,
                    sizeof(type_choice)
                );
                if (result.success == 0) {
                    return result;
                }
                type_index = type_choice[0] - '1';
                if (type_index < 0 || type_index > 4) {
                    MenuApplication_print_result(output, "无效的护理类型选择", 0);
                    return Result_make_failure("invalid nursing type");
                }
                strncpy(third_id, type_labels[type_index], sizeof(third_id) - 1);
            }
            {
                FormPanel nurse_panel;
                memset(&nurse_panel, 0, sizeof(nurse_panel));
                snprintf(nurse_panel.title, sizeof(nurse_panel.title), TUI_MEDICAL " \xe6\x8a\xa4\xe7\x90\x86\xe8\xae\xb0\xe5\xbd\x95");

                snprintf(nurse_panel.fields[0].label, FORM_LABEL_CAPACITY, "\xe6\x8a\xa4\xe5\xa3\xab\xe5\xa7\x93\xe5\x90\x8d:");
                nurse_panel.fields[0].is_required = 1;

                snprintf(nurse_panel.fields[1].label, FORM_LABEL_CAPACITY, "\xe6\x8a\xa4\xe7\x90\x86\xe5\x86\x85\xe5\xae\xb9:");
                nurse_panel.fields[1].is_required = 1;

                snprintf(nurse_panel.fields[2].label, FORM_LABEL_CAPACITY, "\xe8\xae\xb0\xe5\xbd\x95\xe6\x97\xb6\xe9\x97\xb4:");
                nurse_panel.fields[2].is_required = 1;

                nurse_panel.field_count = 3;

                result = MenuApplication_form_dispatch(&context, &nurse_panel);
                if (result.success == 0) {
                    return result;
                }

                strncpy(second_id, nurse_panel.fields[0].value, sizeof(second_id) - 1);
                strncpy(long_text, nurse_panel.fields[1].value, sizeof(long_text) - 1);
                strncpy(time_value, nurse_panel.fields[2].value, sizeof(time_value) - 1);
            }
            tui_spinner_run(output, "正在记录护理...", 400);
            result = MenuApplication_create_nursing_record(
                app,
                first_id,
                third_id,
                second_id,
                long_text,
                time_value,
                output_buffer,
                sizeof(output_buffer)
            );
            MenuApplication_print_result(output, output_buffer, result.success);
            return result;

        case MENU_ACTION_INPATIENT_NURSING_QUERY:
            tui_print_section(output, TUI_MEDICAL, "查看护理记录");
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
            tui_spinner_run(output, "正在查询护理记录...", 300);
            result = MenuApplication_query_nursing_records(
                app,
                first_id,
                output_buffer,
                sizeof(output_buffer)
            );
            MenuApplication_print_result(output, output_buffer, result.success);
            return result;

        default:
            return Result_make_failure("unknown inpatient action");
    }
}
