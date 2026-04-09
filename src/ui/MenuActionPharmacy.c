/**
 * @file MenuActionPharmacy.c
 * @brief Pharmacy domain action handlers for the HIS menu system
 *
 * Handles: MENU_ACTION_PHARMACY_ADD_MEDICINE, MENU_ACTION_PHARMACY_RESTOCK,
 *          MENU_ACTION_PHARMACY_DISPENSE, MENU_ACTION_PHARMACY_QUERY_STOCK,
 *          MENU_ACTION_PHARMACY_LOW_STOCK
 */

#include "ui/MenuActionHandlers.h"

#include <string.h>
#include "ui/TuiStyle.h"

Result MenuAction_handle_pharmacy(MenuApplication *app, MenuAction action, FILE *input, FILE *output) {
    MenuApplicationPromptContext context;
    char output_buffer[2048];
    char first_id[HIS_DOMAIN_ID_CAPACITY];
    char second_id[HIS_DOMAIN_ID_CAPACITY];
    char third_id[HIS_DOMAIN_TEXT_CAPACITY];
    char text_value[HIS_DOMAIN_TEXT_CAPACITY];
    char time_value[HIS_DOMAIN_TIME_CAPACITY];
    Medicine medicine;
    int flag_one = 0;
    Result result;

    context.input = input;
    context.output = output;
    memset(output_buffer, 0, sizeof(output_buffer));
    memset(first_id, 0, sizeof(first_id));
    memset(second_id, 0, sizeof(second_id));
    memset(third_id, 0, sizeof(third_id));
    memset(text_value, 0, sizeof(text_value));
    memset(time_value, 0, sizeof(time_value));

    switch (action) {
        case MENU_ACTION_PHARMACY_ADD_MEDICINE:
            result = MenuApplication_prompt_medicine_form(&context, &medicine, 0);
            if (result.success == 0) {
                return result;
            }
            result = MenuApplication_add_medicine(
                app,
                &medicine,
                output_buffer,
                sizeof(output_buffer)
            );
            MenuApplication_print_result(output, output_buffer, result.success);
            return result;

        case MENU_ACTION_PHARMACY_RESTOCK:
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
            result = MenuApplication_prompt_int(&context, "入库数量: ", &flag_one);
            if (result.success == 0) {
                return result;
            }
            result = MenuApplication_restock_medicine(
                app,
                first_id,
                flag_one,
                output_buffer,
                sizeof(output_buffer)
            );
            MenuApplication_print_result(output, output_buffer, result.success);
            if (result.success) {
                tui_animate_progress(output, "入库进度", 20, 25);
            }
            return result;

        case MENU_ACTION_PHARMACY_DISPENSE:
            result = MenuApplication_prompt_select_patient(
                app,
                &context,
                "患者搜索关键字/编号(回车列出全部): ",
                text_value,
                sizeof(text_value)
            );
            if (result.success == 0) {
                return result;
            }
            result = MenuApplication_prompt_select_visit(
                app,
                &context,
                "处方/就诊搜索关键字/编号(回车列出全部): ",
                text_value,
                "",
                first_id,
                sizeof(first_id)
            );
            if (result.success == 0) {
                return result;
            }
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
            result = MenuApplication_prompt_int(&context, "发药数量: ", &flag_one);
            if (result.success == 0) {
                return result;
            }
            if (app->has_authenticated_user != 0 && app->authenticated_user.role == USER_ROLE_PHARMACY) {
                MenuApplication_copy_text(third_id, sizeof(third_id), app->authenticated_user.user_id);
            } else {
                result = MenuApplication_prompt_line(
                    &context,
                    "药师工号: ",
                    third_id,
                    sizeof(third_id)
                );
                if (result.success == 0) {
                    return result;
                }
            }
            result = MenuApplication_prompt_line(
                &context,
                "发药时间: ",
                time_value,
                sizeof(time_value)
            );
            if (result.success == 0) {
                return result;
            }
            tui_spinner_run(output, "正在处理发药...", 500);
            result = MenuApplication_dispense_medicine(
                app,
                text_value,
                first_id,
                second_id,
                flag_one,
                third_id,
                time_value,
                output_buffer,
                sizeof(output_buffer)
            );
            MenuApplication_print_result(output, output_buffer, result.success);
            return result;

        case MENU_ACTION_PHARMACY_QUERY_STOCK:
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

        case MENU_ACTION_PHARMACY_LOW_STOCK:
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

        default:
            return Result_make_failure("unknown pharmacy action");
    }
}
