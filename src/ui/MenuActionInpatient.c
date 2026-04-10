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
            result = MenuApplication_prompt_line(
                &context,
                "入院时间: ",
                time_value,
                sizeof(time_value)
            );
            if (result.success == 0) {
                return result;
            }
            result = MenuApplication_prompt_line(
                &context,
                "住院摘要: ",
                long_text,
                sizeof(long_text)
            );
            if (result.success == 0) {
                return result;
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

        default:
            return Result_make_failure("unknown inpatient action");
    }
}
