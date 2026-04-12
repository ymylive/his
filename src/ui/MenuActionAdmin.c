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
#include "service/PatientService.h"
#include "ui/TuiStyle.h"

Result MenuAction_handle_admin(MenuApplication *app, MenuAction action, FILE *input, FILE *output) {
    MenuApplicationPromptContext context;
    char output_buffer[4096];
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
                "\n  " TUI_BOLD_YELLOW "[1]" TUI_RESET " 添加患者\n"
                "  " TUI_BOLD_YELLOW "[2]" TUI_RESET " 修改患者信息\n"
                "  " TUI_BOLD_YELLOW "[3]" TUI_RESET " 删除患者\n"
                "  " TUI_BOLD_YELLOW "[4]" TUI_RESET " 查询患者\n"
                "\n请选择操作编号: ",
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
                fprintf(output, "\n");
                tui_print_section(output, TUI_HEART, "确认添加以下患者");
                fprintf(output, "  姓名: %s\n", patient.name);
                fprintf(output, "  性别: %s\n", patient.gender == 1 ? "男" : patient.gender == 2 ? "女" : "未知");
                fprintf(output, "  年龄: %d\n", patient.age);
                fprintf(output, "  联系方式: %s\n", patient.contact);
                fprintf(output, "  身份证: %s\n", patient.id_card);
                fprintf(output, "\n");
                {
                    char confirm[16] = {0};
                    result = MenuApplication_prompt_line(&context, "确认添加? (Enter确认, ESC取消): ", confirm, sizeof(confirm));
                    if (result.success == 0) {
                        return result;
                    }
                }
                result = MenuApplication_add_patient(
                    app,
                    &patient,
                    output_buffer,
                    sizeof(output_buffer)
                );
            } else if (strcmp(first_id, "2") == 0) {
                /* 先选择要修改的患者，加载现有数据 */
                result = MenuApplication_prompt_select_patient(
                    app,
                    &context,
                    "\xe6\x82\xa3\xe8\x80\x85\xe6\x90\x9c\xe7\xb4\xa2\xe5\x85\xb3\xe9\x94\xae\xe5\xad\x97/\xe7\xbc\x96\xe5\x8f\xb7(\xe5\x9b\x9e\xe8\xbd\xa6\xe5\x88\x97\xe5\x87\xba\xe5\x85\xa8\xe9\x83\xa8): ",
                    second_id,
                    sizeof(second_id)
                );
                if (result.success == 0) {
                    return result;
                }
                /* 加载现有患者数据到patient结构体 */
                {
                    Patient existing;
                    memset(&existing, 0, sizeof(existing));
                    result = MenuApplication_query_patient(
                        app,
                        second_id,
                        output_buffer,
                        sizeof(output_buffer)
                    );
                    /* 通过PatientService加载完整数据 */
                    result = PatientService_find_patient_by_id(
                        &app->patient_service,
                        second_id,
                        &existing
                    );
                    if (result.success == 0) {
                        MenuApplication_print_result(output, "\xe6\x82\xa3\xe8\x80\x85\xe4\xb8\x8d\xe5\xad\x98\xe5\x9c\xa8", 0);
                        return result;
                    }
                    /* 复制到patient，表单会检测name非空进入编辑模式 */
                    patient = existing;
                }
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
                /* Add confirmation prompt */
                {
                    char confirm[16] = {0};
                    fprintf(output, "\n");
                    tui_print_warning(output, "此操作不可撤销！");
                    result = MenuApplication_prompt_line(
                        &context,
                        "确认删除? (输入 yes 确认, 其他取消): ",
                        confirm, sizeof(confirm)
                    );
                    if (result.success == 0 || strcmp(confirm, "yes") != 0) {
                        tui_print_info(output, "已取消删除操作");
                        return Result_make_success("delete cancelled");
                    }
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
                "\n  " TUI_BOLD_YELLOW "[1]" TUI_RESET " 新增科室\n"
                "  " TUI_BOLD_YELLOW "[2]" TUI_RESET " 修改科室\n"
                "  " TUI_BOLD_YELLOW "[3]" TUI_RESET " 添加医生\n"
                "  " TUI_BOLD_YELLOW "[4]" TUI_RESET " 查询医生\n"
                "  " TUI_BOLD_YELLOW "[5]" TUI_RESET " 按科室查看医生\n"
                "\n请选择操作编号: ",
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
                fprintf(output, "\n");
                tui_print_section(output, TUI_MEDICAL, "确认添加以下医生");
                fprintf(output, "  姓名: %s\n", doctor.name);
                fprintf(output, "  职称: %s\n", doctor.title);
                fprintf(output, "  科室ID: %s\n", doctor.department_id);
                fprintf(output, "  排班: %s\n", doctor.schedule);
                fprintf(output, "  状态: %s\n", doctor.status == DOCTOR_STATUS_ACTIVE ? "在岗" : "停诊");
                fprintf(output, "\n");
                {
                    char confirm[16] = {0};
                    result = MenuApplication_prompt_line(&context, "确认添加? (Enter确认, ESC取消): ", confirm, sizeof(confirm));
                    if (result.success == 0) {
                        return result;
                    }
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
                "\n  " TUI_BOLD_YELLOW "[1]" TUI_RESET " 药品详情\n"
                "  " TUI_BOLD_YELLOW "[2]" TUI_RESET " 低库存提醒\n"
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

        case MENU_ACTION_ADMIN_STATS_REVENUE:
            tui_spinner_run(output, "正在统计科室收入...", 500);
            result = MenuApplication_stats_department_revenue(app, output_buffer, sizeof(output_buffer));
            MenuApplication_print_result(output, output_buffer, result.success);
            return result;

        case MENU_ACTION_ADMIN_STATS_WORKLOAD:
            tui_spinner_run(output, "正在统计医生工作量...", 500);
            result = MenuApplication_stats_doctor_workload(app, output_buffer, sizeof(output_buffer));
            MenuApplication_print_result(output, output_buffer, result.success);
            return result;

        case MENU_ACTION_ADMIN_STATS_BED_UTIL:
            tui_spinner_run(output, "正在统计床位利用率...", 500);
            result = MenuApplication_stats_bed_utilization(app, output_buffer, sizeof(output_buffer));
            MenuApplication_print_result(output, output_buffer, result.success);
            return result;

        case MENU_ACTION_ADMIN_STATS_MEDICINE:
            tui_spinner_run(output, "正在统计药品消耗...", 500);
            result = MenuApplication_stats_medicine_consumption(app, output_buffer, sizeof(output_buffer));
            MenuApplication_print_result(output, output_buffer, result.success);
            return result;

        case MENU_ACTION_ADMIN_STATS_PATIENT_FLOW:
            tui_spinner_run(output, "正在统计患者流量...", 500);
            result = MenuApplication_stats_patient_flow(app, output_buffer, sizeof(output_buffer));
            MenuApplication_print_result(output, output_buffer, result.success);
            return result;

        case MENU_ACTION_ADMIN_STATS_DEPT_PERFORMANCE:
            tui_spinner_run(output, "正在统计科室绩效...", 500);
            result = MenuApplication_stats_dept_performance(app, output_buffer, sizeof(output_buffer));
            MenuApplication_print_result(output, output_buffer, result.success);
            return result;

        case MENU_ACTION_ADMIN_STATS_ADMISSION_TURNOVER:
            tui_spinner_run(output, "正在统计住院周转率...", 500);
            result = MenuApplication_stats_admission_turnover(app, output_buffer, sizeof(output_buffer));
            MenuApplication_print_result(output, output_buffer, result.success);
            return result;

        default:
            return Result_make_failure("unknown admin action");
    }
}
