/**
 * @file MenuActionAdmin.c
 * @brief Admin domain action handlers for the HIS menu system
 *
 * Handles: MENU_ACTION_ADMIN_PATIENT_MANAGEMENT, MENU_ACTION_ADMIN_DOCTOR_DEPARTMENT,
 *          MENU_ACTION_ADMIN_MEDICAL_RECORDS, MENU_ACTION_ADMIN_WARD_BED_OVERVIEW,
 *          MENU_ACTION_ADMIN_MEDICINE_OVERVIEW
 */

#include "ui/MenuActionHandlers.h"

#include <string.h>
#include "ui/TuiStyle.h"

Result MenuAction_handle_admin(MenuApplication *app, MenuAction action, FILE *input, FILE *output) {
    MenuApplicationPromptContext context;
    char output_buffer[2048];
    char first_id[HIS_DOMAIN_ID_CAPACITY];
    char second_id[HIS_DOMAIN_ID_CAPACITY];
    char text_value[HIS_DOMAIN_TEXT_CAPACITY];
    char time_value[HIS_DOMAIN_TIME_CAPACITY];
    Patient patient;
    Doctor doctor;
    Department department;
    Result result;

    context.input = input;
    context.output = output;
    memset(output_buffer, 0, sizeof(output_buffer));
    memset(first_id, 0, sizeof(first_id));
    memset(second_id, 0, sizeof(second_id));
    memset(text_value, 0, sizeof(text_value));
    memset(time_value, 0, sizeof(time_value));

    switch (action) {
        case MENU_ACTION_ADMIN_PATIENT_MANAGEMENT:
            result = MenuApplication_prompt_line(
                &context,
                TUI_BOLD_YELLOW "1" TUI_RESET ".添加  " TUI_BOLD_YELLOW "2" TUI_RESET ".修改  " TUI_BOLD_YELLOW "3" TUI_RESET ".删除  " TUI_BOLD_YELLOW "4" TUI_RESET ".查询\n请选择: ",
                first_id,
                sizeof(first_id)
            );
            if (result.success == 0) {
                return result;
            }
            if (strcmp(first_id, "1") == 0) {
                result = MenuApplication_prompt_patient_form(&context, &patient, 0);
                if (result.success == 0) {
                    return result;
                }
                result = MenuApplication_add_patient(
                    app,
                    &patient,
                    output_buffer,
                    sizeof(output_buffer)
                );
            } else if (strcmp(first_id, "2") == 0) {
                result = MenuApplication_prompt_patient_form(&context, &patient, 1);
                if (result.success == 0) {
                    return result;
                }
                result = MenuApplication_update_patient(
                    app,
                    &patient,
                    output_buffer,
                    sizeof(output_buffer)
                );
            } else if (strcmp(first_id, "3") == 0) {
                result = MenuApplication_prompt_select_patient(
                    app,
                    &context,
                    "患者搜索关键字/编号(回车列出全部): ",
                    second_id,
                    sizeof(second_id)
                );
                if (result.success == 0) {
                    return result;
                }
                result = MenuApplication_delete_patient(
                    app,
                    second_id,
                    output_buffer,
                    sizeof(output_buffer)
                );
            } else if (strcmp(first_id, "4") == 0) {
                result = MenuApplication_prompt_select_patient(
                    app,
                    &context,
                    "患者搜索关键字/编号(回车列出全部): ",
                    second_id,
                    sizeof(second_id)
                );
                if (result.success == 0) {
                    return result;
                }
                result = MenuApplication_query_patient(
                    app,
                    second_id,
                    output_buffer,
                    sizeof(output_buffer)
                );
            } else {
                return Result_make_failure("invalid admin patient action");
            }
            MenuApplication_print_result(output, output_buffer, result.success);
            return result;

        case MENU_ACTION_ADMIN_DOCTOR_DEPARTMENT:
            result = MenuApplication_prompt_line(
                &context,
                TUI_BOLD_YELLOW "1" TUI_RESET ".新增科室  " TUI_BOLD_YELLOW "2" TUI_RESET ".修改科室  " TUI_BOLD_YELLOW "3" TUI_RESET ".添加医生  " TUI_BOLD_YELLOW "4" TUI_RESET ".查询医生  " TUI_BOLD_YELLOW "5" TUI_RESET ".按科室查看医生\n请选择: ",
                first_id,
                sizeof(first_id)
            );
            if (result.success == 0) {
                return result;
            }
            if (strcmp(first_id, "1") == 0 || strcmp(first_id, "2") == 0) {
                result = MenuApplication_prompt_department_form(&context, &department, strcmp(first_id, "2") == 0 ? 1 : 0);
                if (result.success == 0) {
                    return result;
                }
                result = strcmp(first_id, "1") == 0
                    ? MenuApplication_add_department(
                        app,
                        &department,
                        output_buffer,
                        sizeof(output_buffer)
                    )
                    : MenuApplication_update_department(
                        app,
                        &department,
                        output_buffer,
                        sizeof(output_buffer)
                    );
            } else if (strcmp(first_id, "3") == 0) {
                result = MenuApplication_prompt_doctor_form(&context, &doctor, 0);
                if (result.success == 0) {
                    return result;
                }
                result = MenuApplication_add_doctor(
                    app,
                    &doctor,
                    output_buffer,
                    sizeof(output_buffer)
                );
            } else if (strcmp(first_id, "4") == 0) {
                result = MenuApplication_prompt_select_doctor(
                    app,
                    &context,
                    "医生搜索关键字/工号(回车列出全部): ",
                    "",
                    second_id,
                    sizeof(second_id)
                );
                if (result.success == 0) {
                    return result;
                }
                result = MenuApplication_query_doctor(
                    app,
                    second_id,
                    output_buffer,
                    sizeof(output_buffer)
                );
            } else if (strcmp(first_id, "5") == 0) {
                result = MenuApplication_prompt_select_department(
                    app,
                    &context,
                    "科室搜索关键字/编号(回车列出全部): ",
                    second_id,
                    sizeof(second_id)
                );
                if (result.success == 0) {
                    return result;
                }
                result = MenuApplication_list_doctors_by_department(
                    app,
                    second_id,
                    output_buffer,
                    sizeof(output_buffer)
                );
            } else {
                return Result_make_failure("invalid admin doctor department action");
            }
            MenuApplication_print_result(output, output_buffer, result.success);
            return result;

        case MENU_ACTION_ADMIN_MEDICAL_RECORDS:
            result = MenuApplication_prompt_line(
                &context,
                "起始时间: ",
                time_value,
                sizeof(time_value)
            );
            if (result.success == 0) {
                return result;
            }
            result = MenuApplication_prompt_line(
                &context,
                "结束时间: ",
                text_value,
                sizeof(text_value)
            );
            if (result.success == 0) {
                return result;
            }
            result = MenuApplication_query_records_by_time_range(
                app,
                time_value,
                text_value,
                output_buffer,
                sizeof(output_buffer)
            );
            MenuApplication_print_result(output, output_buffer, result.success);
            return result;

        case MENU_ACTION_ADMIN_WARD_BED_OVERVIEW:
        {
            char selected_ward_id[HIS_DOMAIN_ID_CAPACITY] = {0};
            result = MenuApplication_browse_ward_table(app, &context, selected_ward_id, sizeof(selected_ward_id));
            if (result.success && selected_ward_id[0] != '\0') {
                /* Browse beds in view-only mode (NULL out_id) so non-TTY tests still see bed data */
                MenuApplication_browse_bed_table(app, &context, selected_ward_id, 0, 0);
            }
            return Result_make_success("browse complete");
        }

        case MENU_ACTION_ADMIN_MEDICINE_OVERVIEW:
            result = MenuApplication_prompt_line(
                &context,
                TUI_BOLD_YELLOW "1" TUI_RESET ".药品详情  " TUI_BOLD_YELLOW "2" TUI_RESET ".低库存提醒\n请选择: ",
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
                result = MenuApplication_query_medicine_detail(
                    app,
                    second_id,
                    output_buffer,
                    sizeof(output_buffer),
                    0
                );
            } else if (strcmp(first_id, "2") == 0) {
            {
                char selected_medicine_id[HIS_DOMAIN_ID_CAPACITY] = {0};
                result = MenuApplication_browse_low_stock_table(app, &context, selected_medicine_id, sizeof(selected_medicine_id));
                if (result.success && selected_medicine_id[0] != '\0') {
                    result = MenuApplication_query_medicine_detail(
                        app,
                        selected_medicine_id,
                        output_buffer,
                        sizeof(output_buffer),
                        0
                    );
                    MenuApplication_print_result(output, output_buffer, result.success);
                }
                return Result_make_success("browse complete");
            }
            } else {
                return Result_make_failure("invalid admin medicine action");
            }
            MenuApplication_print_result(output, output_buffer, result.success);
            return result;

        default:
            return Result_make_failure("unknown admin action");
    }
}
