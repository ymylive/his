#include "ui/Workbench.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "domain/Department.h"
#include "domain/Doctor.h"
#include "raygui.h"
#include "ui/DesktopAdapters.h"
#include "ui/DesktopApp.h"

typedef struct AdminMaintenanceState {
    char department_id[HIS_DOMAIN_ID_CAPACITY];
    char department_name[HIS_DOMAIN_NAME_CAPACITY];
    char department_location[HIS_DOMAIN_LOCATION_CAPACITY];
    char department_description[HIS_DOMAIN_TEXT_CAPACITY];
    char doctor_id[HIS_DOMAIN_ID_CAPACITY];
    char doctor_name[HIS_DOMAIN_NAME_CAPACITY];
    char doctor_title[HIS_DOMAIN_NAME_CAPACITY];
    char doctor_department_id[HIS_DOMAIN_ID_CAPACITY];
    char doctor_schedule[HIS_DOMAIN_TEXT_CAPACITY];
    char doctor_query_id[HIS_DOMAIN_ID_CAPACITY];
    char output[WORKBENCH_OUTPUT_CAPACITY];
} AdminMaintenanceState;

static AdminMaintenanceState g_admin_maintenance;

static void show_result(DesktopApp *app, Result result) {
    DesktopAppState_show_message(
        &app->state,
        result.success ? DESKTOP_MESSAGE_SUCCESS : DESKTOP_MESSAGE_ERROR,
        result.message
    );
}

static void draw_text_input(
    Rectangle bounds,
    char *text,
    int text_size,
    int editable,
    int *active_field,
    int field_id
) {
    bool edit_mode = false;

    if (editable == 0 || active_field == 0) {
        GuiTextBox(bounds, text, text_size, false);
        return;
    }

    edit_mode = (*active_field == field_id);
    if (GuiTextBox(bounds, text, text_size, edit_mode)) {
        *active_field = edit_mode ? 0 : field_id;
    }
}

static void draw_output_panel(
    const DesktopApp *app,
    Rectangle rect,
    const char *title,
    const char *content,
    const char *empty_text
) {
    WorkbenchTextPanelLayout layout = Workbench_compute_text_panel_layout(rect, 14.0f, 22.0f, 14.0f);

    DrawRectangleRounded(rect, 0.12f, 8, app->theme.panel);
    DrawRectangleRoundedLinesEx(rect, 0.12f, 8, 1.0f, Fade(app->theme.border, 0.85f));
    DrawText(title, (int)layout.title_bounds.x, (int)layout.title_bounds.y, 20, app->theme.text_primary);
    DrawText(
        (content != 0 && content[0] != '\0') ? content : empty_text,
        (int)layout.content_bounds.x,
        (int)layout.content_bounds.y,
        17,
        app->theme.text_secondary
    );
}

static void admin_draw_home(DesktopApp *app, Rectangle panel) {
    const WorkbenchDef *wb = Workbench_get(USER_ROLE_ADMIN);
    const char *labels[4] = { "患者总数", "挂号总数", "当前住院", "低库存药品" };
    char v0[16];
    char v1[16];
    char v2[16];
    char v3[16];
    const char *values[4];
    WorkbenchButtonGroupLayout actions;
    Result result;
    int index = 0;

    if (app->state.dashboard.loaded == 0) {
        result = DesktopAdapters_load_dashboard(&app->application, &app->state.dashboard);
        show_result(app, result);
        app->state.dashboard.loaded = result.success ? 1 : -1;
    }

    snprintf(v0, sizeof(v0), "%d", app->state.dashboard.patient_count);
    snprintf(v1, sizeof(v1), "%d", app->state.dashboard.registration_count);
    snprintf(v2, sizeof(v2), "%d", app->state.dashboard.inpatient_count);
    snprintf(v3, sizeof(v3), "%d", app->state.dashboard.low_stock_count);
    values[0] = v0;
    values[1] = v1;
    values[2] = v2;
    values[3] = v3;

    Workbench_draw_home_cards(app, panel, labels, values, wb->accent);
    Workbench_draw_section_header(app, (int)panel.x, (int)panel.y + 104, "跨域管理入口");
    actions = Workbench_compute_button_group_layout(
        (Rectangle){ panel.x, panel.y + 144.0f, panel.width, 40.0f },
        4,
        150.0f,
        40.0f,
        14.0f
    );
    for (index = 0; index < actions.count; index++) {
        int clicked = 0;
        const char *label = index == 0 ? "患者总览" :
                            index == 1 ? "医生科室" :
                            index == 2 ? "病房药品" :
                                         "系统信息";
        Workbench_draw_quick_action_btn(app, actions.buttons[index], label, wb->accent, &clicked);
        if (clicked) {
            app->state.workbench_page = index + 1;
        }
    }
    draw_output_panel(
        app,
        (Rectangle){ panel.x, panel.y + 214.0f, panel.width, panel.height - 214.0f },
        "运营说明",
        "管理员工作台已接通患者主数据浏览、科室/医生维护、病房药品总览以及系统路径查看。",
        "暂无说明"
    );
}

static void admin_draw_patients(DesktopApp *app, Rectangle panel) {
    DesktopPatientPageState *state = &app->state.patient_page;
    WorkbenchListDetailLayout split = Workbench_compute_list_detail_layout(
        (Rectangle){ panel.x, panel.y + 56.0f, panel.width, panel.height - 56.0f },
        0.46f,
        18.0f,
        260.0f
    );
    const LinkedListNode *current = 0;
    Result result;
    int row = 0;

    {
        int active_mode = (int)state->search_mode;
        GuiToggleGroup((Rectangle){ panel.x, panel.y, 220, 30 }, "按编号;按姓名", &active_mode);
        state->search_mode = (DesktopPatientSearchMode)active_mode;
    }
    draw_text_input((Rectangle){ panel.x + 236, panel.y, panel.width - 360, 34 }, state->query, sizeof(state->query), 1, &state->active_field, 1);
    if (GuiButton((Rectangle){ panel.x + panel.width - 110, panel.y, 110, 34 }, "查询")) {
        LinkedList_clear(&state->results, free);
        LinkedList_init(&state->results);
        result = DesktopAdapters_search_patients(&app->application, state->search_mode, state->query, &state->results);
        show_result(app, result);
        state->selected_index = -1;
        state->has_selected_patient = 0;
    }

    DrawRectangleRounded(split.list_bounds, 0.12f, 8, app->theme.panel);
    DrawRectangleRoundedLinesEx(split.list_bounds, 0.12f, 8, 1.0f, Fade(app->theme.border, 0.85f));
    DrawRectangleRounded(split.detail_bounds, 0.12f, 8, app->theme.panel);
    DrawRectangleRoundedLinesEx(split.detail_bounds, 0.12f, 8, 1.0f, Fade(app->theme.border, 0.85f));
    DrawText("患者列表", (int)split.list_bounds.x + 16, (int)split.list_bounds.y + 14, 20, app->theme.text_primary);
    DrawText("患者详情", (int)split.detail_bounds.x + 16, (int)split.detail_bounds.y + 14, 20, app->theme.text_primary);

    current = state->results.head;
    while (current != 0 && row < 10) {
        const Patient *patient = (const Patient *)current->data;
        Rectangle row_bounds = {
            split.list_bounds.x + 14,
            split.list_bounds.y + 48 + row * 38.0f,
            split.list_bounds.width - 28,
            30
        };
        if (GuiButton(row_bounds, TextFormat("%s | %s", patient->patient_id, patient->name))) {
            state->selected_patient = *patient;
            state->has_selected_patient = 1;
        }
        current = current->next;
        row++;
    }
    if (state->has_selected_patient) {
        Workbench_draw_info_row(app, (int)split.detail_bounds.x + 16, (int)split.detail_bounds.y + 56, "编号", state->selected_patient.patient_id);
        Workbench_draw_info_row(app, (int)split.detail_bounds.x + 16, (int)split.detail_bounds.y + 84, "姓名", state->selected_patient.name);
        Workbench_draw_info_row(app, (int)split.detail_bounds.x + 16, (int)split.detail_bounds.y + 112, "病史", state->selected_patient.medical_history);
        Workbench_draw_info_row(app, (int)split.detail_bounds.x + 16, (int)split.detail_bounds.y + 140, "住院状态", state->selected_patient.is_inpatient ? "是" : "否");
        if (GuiButton((Rectangle){ split.detail_bounds.x + 16, split.detail_bounds.y + 188, 120, 36 }, "删除患者")) {
            result = DesktopAdapters_delete_patient(&app->application, state->selected_patient.patient_id, g_admin_maintenance.output, sizeof(g_admin_maintenance.output));
            show_result(app, result);
        }
    } else {
        DrawText("管理员可查看并删除基础患者档案。", (int)split.detail_bounds.x + 16, (int)split.detail_bounds.y + 56, 17, app->theme.text_secondary);
    }
}

static void admin_draw_doctors(DesktopApp *app, Rectangle panel) {
    WorkbenchListDetailLayout split = Workbench_compute_list_detail_layout(panel, 0.52f, 18.0f, 280.0f);
    Department department;
    Doctor doctor;
    Result result;

    DrawRectangleRounded(split.list_bounds, 0.12f, 8, app->theme.panel);
    DrawRectangleRoundedLinesEx(split.list_bounds, 0.12f, 8, 1.0f, Fade(app->theme.border, 0.85f));
    DrawText("科室与医生维护", (int)split.list_bounds.x + 16, (int)split.list_bounds.y + 14, 24, app->theme.text_primary);

    GuiLabel((Rectangle){ split.list_bounds.x + 16, split.list_bounds.y + 54, 120, 20 }, "科室编号");
    draw_text_input((Rectangle){ split.list_bounds.x + 16, split.list_bounds.y + 78, 180, 34 }, g_admin_maintenance.department_id, sizeof(g_admin_maintenance.department_id), 1, &app->state.patient_page.active_field, 2);
    GuiLabel((Rectangle){ split.list_bounds.x + 208, split.list_bounds.y + 54, 120, 20 }, "科室名称");
    draw_text_input((Rectangle){ split.list_bounds.x + 208, split.list_bounds.y + 78, split.list_bounds.width - 224, 34 }, g_admin_maintenance.department_name, sizeof(g_admin_maintenance.department_name), 1, &app->state.patient_page.active_field, 3);
    draw_text_input((Rectangle){ split.list_bounds.x + 16, split.list_bounds.y + 126, 180, 34 }, g_admin_maintenance.department_location, sizeof(g_admin_maintenance.department_location), 1, &app->state.patient_page.active_field, 4);
    draw_text_input((Rectangle){ split.list_bounds.x + 208, split.list_bounds.y + 126, split.list_bounds.width - 224, 34 }, g_admin_maintenance.department_description, sizeof(g_admin_maintenance.department_description), 1, &app->state.patient_page.active_field, 5);

    if (GuiButton((Rectangle){ split.list_bounds.x + 16, split.list_bounds.y + 176, 140, 36 }, "新增科室")) {
        memset(&department, 0, sizeof(department));
        strncpy(department.department_id, g_admin_maintenance.department_id, sizeof(department.department_id) - 1);
        strncpy(department.name, g_admin_maintenance.department_name, sizeof(department.name) - 1);
        strncpy(department.location, g_admin_maintenance.department_location, sizeof(department.location) - 1);
        strncpy(department.description, g_admin_maintenance.department_description, sizeof(department.description) - 1);
        result = DesktopAdapters_add_department(&app->application, &department, g_admin_maintenance.output, sizeof(g_admin_maintenance.output));
        show_result(app, result);
    }
    if (GuiButton((Rectangle){ split.list_bounds.x + 172, split.list_bounds.y + 176, 140, 36 }, "更新科室")) {
        memset(&department, 0, sizeof(department));
        strncpy(department.department_id, g_admin_maintenance.department_id, sizeof(department.department_id) - 1);
        strncpy(department.name, g_admin_maintenance.department_name, sizeof(department.name) - 1);
        strncpy(department.location, g_admin_maintenance.department_location, sizeof(department.location) - 1);
        strncpy(department.description, g_admin_maintenance.department_description, sizeof(department.description) - 1);
        result = DesktopAdapters_update_department(&app->application, &department, g_admin_maintenance.output, sizeof(g_admin_maintenance.output));
        show_result(app, result);
    }
    if (GuiButton((Rectangle){ split.list_bounds.x + 328, split.list_bounds.y + 176, 120, 36 }, "列出科室")) {
        result = DesktopAdapters_list_departments(&app->application, g_admin_maintenance.output, sizeof(g_admin_maintenance.output));
        show_result(app, result);
    }

    GuiLabel((Rectangle){ split.list_bounds.x + 16, split.list_bounds.y + 240, 120, 20 }, "医生编号");
    draw_text_input((Rectangle){ split.list_bounds.x + 16, split.list_bounds.y + 264, 180, 34 }, g_admin_maintenance.doctor_id, sizeof(g_admin_maintenance.doctor_id), 1, &app->state.patient_page.active_field, 6);
    GuiLabel((Rectangle){ split.list_bounds.x + 208, split.list_bounds.y + 240, 120, 20 }, "医生姓名");
    draw_text_input((Rectangle){ split.list_bounds.x + 208, split.list_bounds.y + 264, split.list_bounds.width - 224, 34 }, g_admin_maintenance.doctor_name, sizeof(g_admin_maintenance.doctor_name), 1, &app->state.patient_page.active_field, 7);
    draw_text_input((Rectangle){ split.list_bounds.x + 16, split.list_bounds.y + 312, 180, 34 }, g_admin_maintenance.doctor_title, sizeof(g_admin_maintenance.doctor_title), 1, &app->state.patient_page.active_field, 8);
    draw_text_input((Rectangle){ split.list_bounds.x + 208, split.list_bounds.y + 312, 180, 34 }, g_admin_maintenance.doctor_department_id, sizeof(g_admin_maintenance.doctor_department_id), 1, &app->state.patient_page.active_field, 9);
    draw_text_input((Rectangle){ split.list_bounds.x + 16, split.list_bounds.y + 360, split.list_bounds.width - 32, 34 }, g_admin_maintenance.doctor_schedule, sizeof(g_admin_maintenance.doctor_schedule), 1, &app->state.patient_page.active_field, 10);

    if (GuiButton((Rectangle){ split.list_bounds.x + 16, split.list_bounds.y + 410, 140, 36 }, "新增医生")) {
        memset(&doctor, 0, sizeof(doctor));
        strncpy(doctor.doctor_id, g_admin_maintenance.doctor_id, sizeof(doctor.doctor_id) - 1);
        strncpy(doctor.name, g_admin_maintenance.doctor_name, sizeof(doctor.name) - 1);
        strncpy(doctor.title, g_admin_maintenance.doctor_title, sizeof(doctor.title) - 1);
        strncpy(doctor.department_id, g_admin_maintenance.doctor_department_id, sizeof(doctor.department_id) - 1);
        strncpy(doctor.schedule, g_admin_maintenance.doctor_schedule, sizeof(doctor.schedule) - 1);
        doctor.status = DOCTOR_STATUS_ACTIVE;
        result = DesktopAdapters_add_doctor(&app->application, &doctor, g_admin_maintenance.output, sizeof(g_admin_maintenance.output));
        show_result(app, result);
    }
    if (GuiButton((Rectangle){ split.list_bounds.x + 172, split.list_bounds.y + 410, 140, 36 }, "按科室列医生")) {
        result = DesktopAdapters_list_doctors_by_department(&app->application, g_admin_maintenance.doctor_department_id, g_admin_maintenance.output, sizeof(g_admin_maintenance.output));
        show_result(app, result);
    }
    draw_text_input((Rectangle){ split.list_bounds.x + 16, split.list_bounds.y + 466, 180, 34 }, g_admin_maintenance.doctor_query_id, sizeof(g_admin_maintenance.doctor_query_id), 1, &app->state.patient_page.active_field, 11);
    if (GuiButton((Rectangle){ split.list_bounds.x + 208, split.list_bounds.y + 466, 120, 34 }, "查询医生")) {
        result = DesktopAdapters_query_doctor(&app->application, g_admin_maintenance.doctor_query_id, g_admin_maintenance.output, sizeof(g_admin_maintenance.output));
        show_result(app, result);
    }

    draw_output_panel(
        app,
        split.detail_bounds,
        "维护输出",
        g_admin_maintenance.output,
        "管理员可直接维护科室和医生基础资料。"
    );
}

static void admin_draw_wards(DesktopApp *app, Rectangle panel) {
    static char ward_output[WORKBENCH_OUTPUT_CAPACITY];
    static char medicine_output[WORKBENCH_OUTPUT_CAPACITY];
    WorkbenchListDetailLayout split = Workbench_compute_list_detail_layout(panel, 0.48f, 18.0f, 280.0f);
    Result result;

    if (GuiButton((Rectangle){ split.list_bounds.x, split.list_bounds.y, 118, 34 }, "病房总览")) {
        result = DesktopAdapters_list_wards(&app->application, ward_output, sizeof(ward_output));
        show_result(app, result);
    }
    if (GuiButton((Rectangle){ split.list_bounds.x + 130, split.list_bounds.y, 118, 34 }, "低库存")) {
        result = DesktopAdapters_find_low_stock_medicines(&app->application, medicine_output, sizeof(medicine_output));
        show_result(app, result);
    }
    draw_output_panel(app, (Rectangle){ split.list_bounds.x, split.list_bounds.y + 48, split.list_bounds.width, split.list_bounds.height - 48 }, "病房床位", ward_output, "点击病房总览查看当前病区状态。");
    draw_output_panel(app, split.detail_bounds, "药品预警", medicine_output, "管理员可看到药房低库存摘要。");
}

static void admin_draw_system(const DesktopApp *app, Rectangle panel) {
    DrawRectangleRounded(panel, 0.12f, 8, app->theme.panel);
    DrawRectangleRoundedLinesEx(panel, 0.12f, 8, 1.0f, Fade(app->theme.border, 0.85f));
    DrawText("系统信息", (int)panel.x + 16, (int)panel.y + 14, 24, app->theme.text_primary);
    Workbench_draw_info_row(app, (int)panel.x + 16, (int)panel.y + 58, "当前用户", app->state.current_user.user_id);
    Workbench_draw_info_row(app, (int)panel.x + 16, (int)panel.y + 86, "当前角色", DesktopApp_user_role_label(app->state.current_user.role));
    if (app->paths != 0) {
        Workbench_draw_info_row(app, (int)panel.x + 16, (int)panel.y + 138, "patients", app->paths->patient_path);
        Workbench_draw_info_row(app, (int)panel.x + 16, (int)panel.y + 166, "doctors", app->paths->doctor_path);
        Workbench_draw_info_row(app, (int)panel.x + 16, (int)panel.y + 194, "registrations", app->paths->registration_path);
        Workbench_draw_info_row(app, (int)panel.x + 16, (int)panel.y + 222, "medicines", app->paths->medicine_path);
    }
}

void AdminWorkbench_draw(DesktopApp *app, Rectangle panel, int page) {
    switch (page) {
        case 0:
            admin_draw_home(app, panel);
            break;
        case 1:
            admin_draw_patients(app, panel);
            break;
        case 2:
            admin_draw_doctors(app, panel);
            break;
        case 3:
            admin_draw_wards(app, panel);
            break;
        case 4:
            admin_draw_system(app, panel);
            break;
        default:
            admin_draw_home(app, panel);
            break;
    }
}
