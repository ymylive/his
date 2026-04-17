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
#include "repository/RepositoryUtils.h"
#include "ui/TuiStyle.h"

Result MenuAction_handle_patient(MenuApplication *app, MenuAction action, FILE *input, FILE *output) {
    MenuApplicationPromptContext context;
    char output_buffer[4096];
    char first_id[HIS_DOMAIN_ID_CAPACITY];
    char second_id[HIS_DOMAIN_ID_CAPACITY];
    char time_value[HIS_DOMAIN_TIME_CAPACITY];
    Registration registration;
    Result result;

    context.input = input;
    context.output = output;
    memset(output_buffer, 0, sizeof(output_buffer));
    memset(first_id, 0, sizeof(first_id));
    memset(second_id, 0, sizeof(second_id));
    memset(time_value, 0, sizeof(time_value));

    switch (action) {
        case MENU_ACTION_PATIENT_BASIC_INFO:
            result = MenuApplication_require_patient_session(app);
            if (result.success == 0) {
                return result;
            }
            tui_spinner_run(output, "正在查询患者信息...", 400);
            MenuApplication_print_patient_card(app, output, app->bound_patient_id);
            return Result_make_success("patient info displayed");

        case MENU_ACTION_PATIENT_SELF_REGISTER:
            result = MenuApplication_require_patient_session(app);
            if (result.success == 0) {
                return result;
            }
            /* 选择科室 */
            result = MenuApplication_prompt_select_department(
                app,
                &context,
                "选择挂号科室(输入关键字搜索): ",
                first_id,
                sizeof(first_id)
            );
            if (result.success == 0) {
                return result;
            }
            /* 选择该科室的医生 */
            result = MenuApplication_prompt_select_doctor(
                app,
                &context,
                "选择医生(输入关键字搜索): ",
                first_id,
                second_id,
                sizeof(second_id)
            );
            if (result.success == 0) {
                return result;
            }
            /* 选择可挂号时段（由医生排班 + 剩余号数自动展开） */
            result = MenuApplication_prompt_select_registration_slot(
                app,
                &context,
                "选择可挂号时段: ",
                second_id,
                time_value,
                sizeof(time_value)
            );
            if (result.success == 0) {
                tui_print_error(output, result.message);
                return result;
            }
            /* 选择挂号类型 */
            {
                char type_choice[16];
                RegistrationType reg_type = REG_TYPE_STANDARD;
                double reg_fee = 5.00;

                tui_print_info(output, "请选择挂号类型:");
                tui_print_info(output, "  [1] 普通号 (5.00元)");
                tui_print_info(output, "  [2] 专家号 (20.00元)");
                tui_print_info(output, "  [3] 急诊号 (10.00元)");
                result = MenuApplication_prompt_line(
                    &context,
                    "请输入选项(1-3): ",
                    type_choice,
                    sizeof(type_choice)
                );
                if (result.success == 0) {
                    return result;
                }
                if (type_choice[0] == '2') {
                    reg_type = REG_TYPE_SPECIALIST;
                    reg_fee = 20.00;
                } else if (type_choice[0] == '3') {
                    reg_type = REG_TYPE_EMERGENCY;
                    reg_fee = 10.00;
                } else if (type_choice[0] != '1') {
                    tui_print_error(output, "无效的挂号类型选项，默认使用普通号");
                }

                /* 创建挂号 */
                tui_spinner_run(output, "正在创建挂号...", 500);
                result = MenuApplication_create_self_registration(
                    app,
                    second_id,
                    first_id,
                    time_value,
                    reg_type,
                    reg_fee,
                    &registration
                );
                if (result.success == 0) {
                    MenuApplication_print_result(output, result.message, 0);
                    return result;
                }
                snprintf(output_buffer, sizeof(output_buffer),
                    "挂号成功! 挂号编号: %s | 科室: %s | 医生: %s | 时间: %s | 类型: %s | 费用: %.2f元 | 状态: 待诊",
                    registration.registration_id,
                    registration.department_id,
                    registration.doctor_id,
                    registration.registered_at,
                    reg_type == REG_TYPE_SPECIALIST ? "专家号" :
                    reg_type == REG_TYPE_EMERGENCY ? "急诊号" : "普通号",
                    reg_fee);
                MenuApplication_print_result(output, output_buffer, 1);
                return result;
            }

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

        case MENU_ACTION_PATIENT_QUERY_FEES:
            result = MenuApplication_require_patient_session(app);
            if (result.success == 0) {
                return result;
            }
            tui_spinner_run(output, "正在查询费用...", 400);
            result = MenuApplication_query_patient_fees(app, app->bound_patient_id, output_buffer, sizeof(output_buffer));
            MenuApplication_print_result(output, output_buffer, result.success);
            return result;

        default:
            return Result_make_failure("unknown patient action");
    }
}
