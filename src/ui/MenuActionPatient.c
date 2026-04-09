/**
 * @file MenuActionPatient.c
 * @brief Patient domain action handlers for the HIS menu system
 *
 * Handles: MENU_ACTION_PATIENT_BASIC_INFO, MENU_ACTION_PATIENT_QUERY_REGISTRATION,
 *          MENU_ACTION_PATIENT_QUERY_VISITS, MENU_ACTION_PATIENT_QUERY_EXAMS,
 *          MENU_ACTION_PATIENT_QUERY_ADMISSIONS, MENU_ACTION_PATIENT_QUERY_DISPENSE,
 *          MENU_ACTION_PATIENT_QUERY_MEDICINE_USAGE
 */

#include "ui/MenuActionHandlers.h"

#include <string.h>
#include "ui/TuiStyle.h"

Result MenuAction_handle_patient(MenuApplication *app, MenuAction action, FILE *input, FILE *output) {
    MenuApplicationPromptContext context;
    char output_buffer[2048];
    char first_id[HIS_DOMAIN_ID_CAPACITY];
    Result result;

    context.input = input;
    context.output = output;
    memset(output_buffer, 0, sizeof(output_buffer));
    memset(first_id, 0, sizeof(first_id));

    switch (action) {
        case MENU_ACTION_PATIENT_BASIC_INFO:
            result = MenuApplication_require_patient_session(app);
            if (result.success == 0) {
                return result;
            }
            tui_spinner_run(output, "正在查询患者信息...", 400);
            MenuApplication_print_patient_card(app, output, app->bound_patient_id);
            return Result_make_success("patient info displayed");

        case MENU_ACTION_PATIENT_QUERY_REGISTRATION:
            result = MenuApplication_require_patient_session(app);
            if (result.success == 0) {
                return result;
            }
            MenuApplication_browse_patient_history(app, &context, app->bound_patient_id);
            return Result_make_success("browse complete");

        case MENU_ACTION_PATIENT_QUERY_VISITS:
        case MENU_ACTION_PATIENT_QUERY_EXAMS:
        case MENU_ACTION_PATIENT_QUERY_ADMISSIONS:
            result = MenuApplication_require_patient_session(app);
            if (result.success == 0) {
                return result;
            }
            MenuApplication_browse_patient_history(app, &context, app->bound_patient_id);
            return Result_make_success("browse complete");

        case MENU_ACTION_PATIENT_QUERY_DISPENSE:
            result = MenuApplication_require_patient_session(app);
            if (result.success == 0) {
                return result;
            }
            MenuApplication_browse_patient_history(app, &context, app->bound_patient_id);
            return Result_make_success("browse complete");

        case MENU_ACTION_PATIENT_QUERY_MEDICINE_USAGE:
            result = MenuApplication_require_patient_session(app);
            if (result.success == 0) {
                return result;
            }
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
            result = MenuApplication_query_medicine_detail(
                app,
                first_id,
                output_buffer,
                sizeof(output_buffer),
                1
            );
            MenuApplication_print_result(output, output_buffer, result.success);
            return result;

        default:
            return Result_make_failure("unknown patient action");
    }
}
