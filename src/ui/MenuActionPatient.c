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
#include <time.h>
#include "ui/TuiStyle.h"
#include "domain/Registration.h"

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

        case MENU_ACTION_PATIENT_SELF_REGISTRATION:
            result = MenuApplication_require_patient_session(app);
            if (result.success == 0) {
                return result;
            }
            {
                char department_id[HIS_DOMAIN_ID_CAPACITY];
                char doctor_id[HIS_DOMAIN_ID_CAPACITY];
                char registered_at[HIS_DOMAIN_TIME_CAPACITY];
                Registration registration;
                
                // 选择科室
                result = MenuApplication_prompt_select_department(
                    app,
                    &context,
                    "请选择挂号科室: ",
                    department_id,
                    sizeof(department_id)
                );
                if (result.success == 0) {
                    return result;
                }
                
                // 选择医生（基于科室过滤）
                result = MenuApplication_prompt_select_doctor(
                    app,
                    &context,
                    "请选择挂号医生: ",
                    department_id,
                    doctor_id,
                    sizeof(doctor_id)
                );
                if (result.success == 0) {
                    return result;
                }
                
                // 获取当前时间作为挂号时间
                time_t now = time(NULL);
                struct tm *local_time = localtime(&now);
                strftime(registered_at, sizeof(registered_at), "%Y-%m-%d %H:%M:%S", local_time);
                
                // 创建自主挂号
                result = MenuApplication_create_self_registration(
                    app,
                    doctor_id,
                    department_id,
                    registered_at,
                    &registration
                );
                if (result.success == 0) {
                    fprintf(output, TUI_CROSS " 挂号失败: %s\n", result.message);
                    return result;
                }
                
                // 显示挂号成功信息
                fprintf(output, TUI_CHECK " 挂号成功！\n");
                fprintf(output, "挂号编号: %s\n", registration.registration_id);
                fprintf(output, "患者: %s\n", registration.patient_id);
                fprintf(output, "医生: %s\n", registration.doctor_id);
                fprintf(output, "科室: %s\n", registration.department_id);
                fprintf(output, "挂号时间: %s\n", registration.registered_at);
                fprintf(output, "状态: %s\n", registration.status == REG_STATUS_PENDING ? "待诊" : "未知");
                
                return Result_make_success("self registration completed");
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

        default:
            return Result_make_failure("unknown patient action");
    }
}
