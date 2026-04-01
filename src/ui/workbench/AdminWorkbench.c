#include "ui/Workbench.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "domain/Department.h"
#include "domain/Doctor.h"
#include "repository/DepartmentRepository.h"
#include "repository/DoctorRepository.h"
#include "service/DoctorService.h"
#include "raygui.h"
#include "ui/AdminWorkbenchSupport.h"
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
static WorkbenchSearchSelectState g_admin_department_select;
static WorkbenchSearchSelectState g_admin_doctor_select;
static WorkbenchSearchSelectState g_admin_doctor_query_select;
static int g_admin_active_field = 0;
static char g_admin_patient_output[WORKBENCH_OUTPUT_CAPACITY];
static int g_admin_initialized = 0;

static void admin_refresh_patient_results(DesktopApp *app, DesktopPatientPageState *state) {
    if (app == 0 || state == 0) {
        return;
    }

    LinkedList_clear(&state->results, free);
    LinkedList_init(&state->results);
    if (DesktopAdapters_search_patients(&app->application, state->search_mode, state->query, &state->results).success != 1) {
        return;
    }
}

static void admin_refresh_department_select(DesktopApp *app) {
    LinkedList departments;
    const LinkedListNode *current = 0;
    int selected_index = -1;

    if (app == 0) {
        return;
    }

    if (!WorkbenchSearchSelectState_needs_refresh(&g_admin_department_select)) return;

    WorkbenchSearchSelectState_reset(&g_admin_department_select);
    LinkedList_init(&departments);
    if (DepartmentRepository_load_all(&app->application.doctor_service.department_repository, &departments).success != 1) {
        return;
    }

    current = departments.head;
    while (current != 0) {
        const Department *department = (const Department *)current->data;
        char label[WORKBENCH_SELECT_LABEL_CAPACITY];

        snprintf(label, sizeof(label), "%s | %s | %s", department->department_id, department->name, department->location);
        if (Workbench_text_contains_ignore_case(label, g_admin_department_select.query)) {
            if (strcmp(g_admin_maintenance.department_id, department->department_id) == 0) {
                selected_index = g_admin_department_select.option_count;
            }
            WorkbenchSearchSelectState_add_option(&g_admin_department_select, label);
        }
        current = current->next;
    }

    WorkbenchSearchSelectState_finalize(&g_admin_department_select);
    g_admin_department_select.active_index = selected_index;
    DepartmentRepository_clear_list(&departments);
}

static void admin_populate_doctor_select(
    DesktopApp *app,
    WorkbenchSearchSelectState *state,
    const char *current_id
) {
    LinkedList doctors;
    const LinkedListNode *current = 0;
    int selected_index = -1;

    if (app == 0 || state == 0) {
        return;
    }

    if (!WorkbenchSearchSelectState_needs_refresh(state)) return;

    WorkbenchSearchSelectState_reset(state);
    LinkedList_init(&doctors);
    if (DoctorRepository_load_all(&app->application.doctor_service.doctor_repository, &doctors).success != 1) {
        return;
    }

    current = doctors.head;
    while (current != 0) {
        const Doctor *doctor = (const Doctor *)current->data;
        char label[WORKBENCH_SELECT_LABEL_CAPACITY];

        snprintf(label, sizeof(label), "%s | %s | %s | %s", doctor->doctor_id, doctor->name, doctor->title, doctor->department_id);
        if (Workbench_text_contains_ignore_case(label, state->query)) {
            if (current_id != 0 && strcmp(current_id, doctor->doctor_id) == 0) {
                selected_index = state->option_count;
            }
            WorkbenchSearchSelectState_add_option(state, label);
        }

        current = current->next;
    }

    WorkbenchSearchSelectState_finalize(state);
    state->active_index = selected_index;
    DoctorRepository_clear_list(&doctors);
}

static void admin_refresh_doctor_select(DesktopApp *app) {
    admin_populate_doctor_select(app, &g_admin_doctor_select, g_admin_maintenance.doctor_id);
}

static void admin_refresh_doctor_query_select(DesktopApp *app) {
    admin_populate_doctor_select(app, &g_admin_doctor_query_select, g_admin_maintenance.doctor_query_id);
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
        Workbench_show_result(app, result);
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
                            index == 1 ? "医生与科室" :
                            index == 2 ? "病房药品" :
                                         "系统信息";
        Workbench_draw_quick_action_btn(app, actions.buttons[index], label, wb->accent, &clicked);
        if (clicked) {
            app->state.workbench_page = index + 1;
        }
    }
    Workbench_draw_output_panel(
        app,
        (Rectangle){ panel.x, panel.y + 214.0f, panel.width, panel.height - 214.0f },
        "运营说明",
        "管理员工作台已接通患者主数据浏览、科室/医生维护、病房药品总览以及系统信息查看。",
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
    int search_clicked = 0;

    {
        int active_mode = (int)state->search_mode;
        GuiToggleGroup((Rectangle){ panel.x, panel.y, 220, 30 }, "按编号;按姓名", &active_mode);
        state->search_mode = (DesktopPatientSearchMode)active_mode;
    }

    Workbench_draw_search_box(
        app,
        (Rectangle){ panel.x + 236, panel.y, panel.width - 236, 34 },
        state->query, sizeof(state->query),
        &state->active_field, 1,
        &search_clicked
    );

    if (search_clicked) {
        LinkedList_clear(&state->results, free);
        LinkedList_init(&state->results);
        result = DesktopAdapters_search_patients(&app->application, state->search_mode, state->query, &state->results);
        Workbench_show_result(app, result);
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
        int is_selected = state->has_selected_patient &&
                         strcmp(state->selected_patient.patient_id, patient->patient_id) == 0;

        if (is_selected) {
            DrawRectangleRounded(row_bounds, 0.15f, 8, Fade(app->theme.border, 0.2f));
        }

        if (GuiButton(row_bounds, TextFormat("%s | %s", patient->patient_id, patient->name))) {
            state->selected_patient = *patient;
            state->has_selected_patient = 1;
        }
        current = current->next;
        row++;
    }

    if (state->has_selected_patient) {
        float detail_y = split.detail_bounds.y + 56;
        Workbench_draw_info_row(app, (int)split.detail_bounds.x + 16, (int)detail_y, "编号", state->selected_patient.patient_id);
        detail_y += 32;
        Workbench_draw_info_row(app, (int)split.detail_bounds.x + 16, (int)detail_y, "姓名", state->selected_patient.name);
        detail_y += 32;
        Workbench_draw_info_row(app, (int)split.detail_bounds.x + 16, (int)detail_y, "病史", state->selected_patient.medical_history);
        detail_y += 32;

        Rectangle status_badge = { split.detail_bounds.x + 126, detail_y, 80, 26 };
        Color status_color = state->selected_patient.is_inpatient ?
                            (Color){ 245, 158, 11, 255 } : (Color){ 34, 197, 94, 255 };
        Workbench_draw_status_badge(app, status_badge,
                                    state->selected_patient.is_inpatient ? "住院" : "门诊",
                                    status_color);
        DrawText("住院状态", (int)split.detail_bounds.x + 16, (int)detail_y + 4, 18, app->theme.text_secondary);

        detail_y += 48;
        if (GuiButton((Rectangle){ split.detail_bounds.x + 16, detail_y, 120, 36 }, "删除患者")) {
            result = DesktopAdapters_delete_patient(
                &app->application,
                state->selected_patient.patient_id,
                g_admin_patient_output,
                sizeof(g_admin_patient_output)
            );
            Workbench_show_result(app, result);
            if (result.success != 0) {
                DesktopApp_mark_dirty(&app->state, DESKTOP_PAGE_DASHBOARD);
                state->has_selected_patient = 0;
                state->selected_index = -1;
                memset(&state->selected_patient, 0, sizeof(state->selected_patient));
                admin_refresh_patient_results(app, state);
            }
        }

        detail_y += 52;
        Workbench_draw_output_panel(
            app,
            (Rectangle){
                split.detail_bounds.x,
                detail_y,
                split.detail_bounds.width,
                split.detail_bounds.y + split.detail_bounds.height - detail_y
            },
            "患者操作输出",
            g_admin_patient_output,
            "删除结果会显示在这里。"
        );
    } else {
        DrawText("管理员可查看并删除基础患者档案。", (int)split.detail_bounds.x + 16, (int)split.detail_bounds.y + 56, 17, app->theme.text_secondary);
    }
}

static void admin_draw_doctors(DesktopApp *app, Rectangle panel) {
    WorkbenchListDetailLayout split = Workbench_compute_list_detail_layout(panel, 0.52f, 18.0f, 280.0f);
    Department department;
    Doctor doctor;
    Result result;
    float ly = split.list_bounds.y;
    float lx = split.list_bounds.x;

    DrawRectangleRounded(split.list_bounds, 0.12f, 8, app->theme.panel);
    DrawRectangleRoundedLinesEx(split.list_bounds, 0.12f, 8, 1.0f, Fade(app->theme.border, 0.85f));
    DrawText("科室与医生维护", (int)lx + 16, (int)ly + 14, 24, app->theme.text_primary);

    /* Department section */
    Workbench_draw_section_header(app, (int)lx + 16, (int)ly + 50, "科室管理");

    Workbench_draw_form_label(app, (int)lx + 16, (int)ly + 84, "科室编号", 1);
    Workbench_draw_text_input((Rectangle){ lx + 16, ly + 106, 180, 34 }, g_admin_maintenance.department_id, sizeof(g_admin_maintenance.department_id), 1, &g_admin_active_field, 2);

    Workbench_draw_form_label(app, (int)lx + 208, (int)ly + 84, "科室名称", 1);
    Workbench_draw_text_input((Rectangle){ lx + 208, ly + 106, split.list_bounds.width - 224, 34 }, g_admin_maintenance.department_name, sizeof(g_admin_maintenance.department_name), 1, &g_admin_active_field, 3);

    Workbench_draw_form_label(app, (int)lx + 16, (int)ly + 150, "位置", 0);
    Workbench_draw_text_input((Rectangle){ lx + 16, ly + 172, 180, 34 }, g_admin_maintenance.department_location, sizeof(g_admin_maintenance.department_location), 1, &g_admin_active_field, 4);

    Workbench_draw_form_label(app, (int)lx + 208, (int)ly + 150, "描述", 0);
    Workbench_draw_text_input((Rectangle){ lx + 208, ly + 172, split.list_bounds.width - 224, 34 }, g_admin_maintenance.department_description, sizeof(g_admin_maintenance.department_description), 1, &g_admin_active_field, 5);

    if (GuiButton((Rectangle){ lx + 16, ly + 222, 140, 36 }, "新增科室")) {
        memset(&department, 0, sizeof(department));
        strncpy(department.department_id, g_admin_maintenance.department_id, sizeof(department.department_id) - 1);
        strncpy(department.name, g_admin_maintenance.department_name, sizeof(department.name) - 1);
        strncpy(department.location, g_admin_maintenance.department_location, sizeof(department.location) - 1);
        strncpy(department.description, g_admin_maintenance.department_description, sizeof(department.description) - 1);
        result = DesktopAdapters_add_department(&app->application, &department, g_admin_maintenance.output, sizeof(g_admin_maintenance.output));
        Workbench_show_result(app, result);
        if (result.success != 0) {
            DesktopApp_mark_dirty(&app->state, DESKTOP_PAGE_DASHBOARD);
            WorkbenchSearchSelectState_mark_dirty(&g_admin_department_select);
        }
    }
    if (GuiButton((Rectangle){ lx + 172, ly + 222, 140, 36 }, "更新科室")) {
        memset(&department, 0, sizeof(department));
        strncpy(department.department_id, g_admin_maintenance.department_id, sizeof(department.department_id) - 1);
        strncpy(department.name, g_admin_maintenance.department_name, sizeof(department.name) - 1);
        strncpy(department.location, g_admin_maintenance.department_location, sizeof(department.location) - 1);
        strncpy(department.description, g_admin_maintenance.department_description, sizeof(department.description) - 1);
        result = DesktopAdapters_update_department(&app->application, &department, g_admin_maintenance.output, sizeof(g_admin_maintenance.output));
        Workbench_show_result(app, result);
        if (result.success != 0) {
            DesktopApp_mark_dirty(&app->state, DESKTOP_PAGE_DASHBOARD);
            WorkbenchSearchSelectState_mark_dirty(&g_admin_department_select);
        }
    }
    if (GuiButton((Rectangle){ lx + 328, ly + 222, 120, 36 }, "列出科室")) {
        result = DesktopAdapters_list_departments(&app->application, g_admin_maintenance.output, sizeof(g_admin_maintenance.output));
        Workbench_show_result(app, result);
    }

    /* Doctor section */
    Workbench_draw_section_header(app, (int)lx + 16, (int)ly + 280, "医生管理");

    Workbench_draw_form_label(app, (int)lx + 16, (int)ly + 314, "医生编号", 1);
    Workbench_draw_text_input((Rectangle){ lx + 16, ly + 336, 180, 34 }, g_admin_maintenance.doctor_id, sizeof(g_admin_maintenance.doctor_id), 1, &g_admin_active_field, 6);

    Workbench_draw_form_label(app, (int)lx + 208, (int)ly + 314, "医生姓名", 1);
    Workbench_draw_text_input((Rectangle){ lx + 208, ly + 336, split.list_bounds.width - 224, 34 }, g_admin_maintenance.doctor_name, sizeof(g_admin_maintenance.doctor_name), 1, &g_admin_active_field, 7);

    Workbench_draw_form_label(app, (int)lx + 16, (int)ly + 380, "职称", 0);
    Workbench_draw_text_input((Rectangle){ lx + 16, ly + 402, 180, 34 }, g_admin_maintenance.doctor_title, sizeof(g_admin_maintenance.doctor_title), 1, &g_admin_active_field, 8);

    Workbench_draw_form_label(app, (int)lx + 208, (int)ly + 380, "所属科室编号", 1);
    Workbench_draw_text_input((Rectangle){ lx + 208, ly + 402, 180, 34 }, g_admin_maintenance.doctor_department_id, sizeof(g_admin_maintenance.doctor_department_id), 1, &g_admin_active_field, 9);

    Workbench_draw_form_label(app, (int)lx + 16, (int)ly + 446, "排班信息", 0);
    Workbench_draw_text_input((Rectangle){ lx + 16, ly + 468, split.list_bounds.width - 32, 34 }, g_admin_maintenance.doctor_schedule, sizeof(g_admin_maintenance.doctor_schedule), 1, &g_admin_active_field, 10);

    if (GuiButton((Rectangle){ lx + 16, ly + 518, 140, 36 }, "新增医生")) {
        memset(&doctor, 0, sizeof(doctor));
        strncpy(doctor.doctor_id, g_admin_maintenance.doctor_id, sizeof(doctor.doctor_id) - 1);
        strncpy(doctor.name, g_admin_maintenance.doctor_name, sizeof(doctor.name) - 1);
        strncpy(doctor.title, g_admin_maintenance.doctor_title, sizeof(doctor.title) - 1);
        strncpy(doctor.department_id, g_admin_maintenance.doctor_department_id, sizeof(doctor.department_id) - 1);
        strncpy(doctor.schedule, g_admin_maintenance.doctor_schedule, sizeof(doctor.schedule) - 1);
        doctor.status = DOCTOR_STATUS_ACTIVE;
        result = DesktopAdapters_add_doctor(&app->application, &doctor, g_admin_maintenance.output, sizeof(g_admin_maintenance.output));
        Workbench_show_result(app, result);
        if (result.success != 0) {
            DesktopApp_mark_dirty(&app->state, DESKTOP_PAGE_DASHBOARD);
            WorkbenchSearchSelectState_mark_dirty(&g_admin_doctor_select);
            WorkbenchSearchSelectState_mark_dirty(&g_admin_doctor_query_select);
        }
    }
    if (GuiButton((Rectangle){ lx + 172, ly + 518, 140, 36 }, "按科室查看")) {
        result = DesktopAdapters_list_doctors_by_department(&app->application, g_admin_maintenance.doctor_department_id, g_admin_maintenance.output, sizeof(g_admin_maintenance.output));
        Workbench_show_result(app, result);
    }

    Workbench_draw_form_label(app, (int)lx + 16, (int)ly + 570, "查询医生工号", 0);
    Workbench_draw_text_input((Rectangle){ lx + 16, ly + 592, 180, 34 }, g_admin_maintenance.doctor_query_id, sizeof(g_admin_maintenance.doctor_query_id), 1, &g_admin_active_field, 11);
    if (GuiButton((Rectangle){ lx + 208, ly + 592, 120, 34 }, "查询医生")) {
        result = DesktopAdapters_query_doctor(&app->application, g_admin_maintenance.doctor_query_id, g_admin_maintenance.output, sizeof(g_admin_maintenance.output));
        Workbench_show_result(app, result);
    }
    admin_refresh_department_select(app);
    admin_refresh_doctor_select(app);
    admin_refresh_doctor_query_select(app);

    const float detail_padding = 12.0f;
    const float list_height = 56.0f;
    const float between_gap = 12.0f;
    Rectangle detail = split.detail_bounds;
    float curr_y = detail.y + detail_padding;
    Rectangle dept_search_rect;
    Rectangle dept_list_rect;
    Rectangle doctor_search_rect;
    Rectangle doctor_list_rect;
    Rectangle output_rect;

    Workbench_draw_form_label(app, (int)(detail.x + detail_padding), (int)curr_y, "科室选择", 1);
    curr_y += 22.0f;
    dept_search_rect = (Rectangle){
        detail.x + detail_padding,
        curr_y,
        detail.width - detail_padding * 2,
        34.0f
    };
    curr_y += dept_search_rect.height + 6.0f;
    dept_list_rect = (Rectangle){
        detail.x + detail_padding,
        curr_y,
        detail.width - detail_padding * 2,
        list_height
    };
    curr_y += dept_list_rect.height + between_gap;

    Workbench_draw_form_label(app, (int)(detail.x + detail_padding), (int)curr_y, "医生选择", 1);
    curr_y += 22.0f;
    doctor_search_rect = (Rectangle){
        detail.x + detail_padding,
        curr_y,
        detail.width - detail_padding * 2,
        34.0f
    };
    curr_y += doctor_search_rect.height + 6.0f;
    doctor_list_rect = (Rectangle){
        detail.x + detail_padding,
        curr_y,
        detail.width - detail_padding * 2,
        list_height
    };
    curr_y += doctor_list_rect.height + between_gap;

    Workbench_draw_form_label(app, (int)(detail.x + detail_padding), (int)curr_y, "医生查询", 1);
    curr_y += 22.0f;
    Rectangle doctor_query_search_rect = (Rectangle){
        detail.x + detail_padding,
        curr_y,
        detail.width - detail_padding * 2,
        34.0f
    };
    curr_y += doctor_query_search_rect.height + 6.0f;
    Rectangle doctor_query_list_rect = (Rectangle){
        detail.x + detail_padding,
        curr_y,
        detail.width - detail_padding * 2,
        list_height
    };
    curr_y += doctor_query_list_rect.height + between_gap;

    output_rect = (Rectangle){
        detail.x,
        curr_y,
        detail.width,
        detail.y + detail.height - curr_y - detail_padding
    };
    if (output_rect.height < 110.0f) {
        output_rect.height = 110.0f;
        output_rect.y = detail.y + detail.height - detail_padding - output_rect.height;
    }

    Workbench_draw_search_select(app, dept_search_rect, dept_list_rect, &g_admin_department_select, &g_admin_active_field, 12);
    if (g_admin_department_select.active_index >= 0 && g_admin_department_select.active_index < g_admin_department_select.option_count) {
        char selected_id[HIS_DOMAIN_ID_CAPACITY];
        if (AdminWorkbench_parse_option_id(
                g_admin_department_select.labels[g_admin_department_select.active_index],
                selected_id,
                sizeof(selected_id)
            ) == 1 &&
            strcmp(g_admin_maintenance.department_id, selected_id) != 0) {
            Department department;
            if (DepartmentRepository_find_by_department_id(&app->application.doctor_service.department_repository, selected_id, &department).success == 1) {
                AdminWorkbench_apply_department_selection(
                    &department,
                    g_admin_maintenance.department_id,
                    sizeof(g_admin_maintenance.department_id),
                    g_admin_maintenance.department_name,
                    sizeof(g_admin_maintenance.department_name),
                    g_admin_maintenance.department_location,
                    sizeof(g_admin_maintenance.department_location),
                    g_admin_maintenance.department_description,
                    sizeof(g_admin_maintenance.department_description),
                    g_admin_maintenance.doctor_department_id,
                    sizeof(g_admin_maintenance.doctor_department_id)
                );
            }
        }
    }
    Workbench_draw_search_select(app, doctor_search_rect, doctor_list_rect, &g_admin_doctor_select, &g_admin_active_field, 13);
    if (g_admin_doctor_select.active_index >= 0 && g_admin_doctor_select.active_index < g_admin_doctor_select.option_count) {
        char selected_id[HIS_DOMAIN_ID_CAPACITY];
        if (AdminWorkbench_parse_option_id(
                g_admin_doctor_select.labels[g_admin_doctor_select.active_index],
                selected_id,
                sizeof(selected_id)
            ) == 1 &&
            strcmp(g_admin_maintenance.doctor_id, selected_id) != 0) {
            Doctor selected_doctor;
            if (DoctorRepository_find_by_doctor_id(&app->application.doctor_service.doctor_repository, selected_id, &selected_doctor).success == 1) {
                strncpy(g_admin_maintenance.doctor_id, selected_doctor.doctor_id, sizeof(g_admin_maintenance.doctor_id) - 1);
                strncpy(g_admin_maintenance.doctor_name, selected_doctor.name, sizeof(g_admin_maintenance.doctor_name) - 1);
                strncpy(g_admin_maintenance.doctor_title, selected_doctor.title, sizeof(g_admin_maintenance.doctor_title) - 1);
                strncpy(g_admin_maintenance.doctor_department_id, selected_doctor.department_id, sizeof(g_admin_maintenance.doctor_department_id) - 1);
                strncpy(g_admin_maintenance.doctor_schedule, selected_doctor.schedule, sizeof(g_admin_maintenance.doctor_schedule) - 1);
            }
        }
    }
    Workbench_draw_search_select(app, doctor_query_search_rect, doctor_query_list_rect, &g_admin_doctor_query_select, &g_admin_active_field, 14);
    if (g_admin_doctor_query_select.active_index >= 0 && g_admin_doctor_query_select.active_index < g_admin_doctor_query_select.option_count) {
        char selected_id[HIS_DOMAIN_ID_CAPACITY];
        if (AdminWorkbench_parse_option_id(
                g_admin_doctor_query_select.labels[g_admin_doctor_query_select.active_index],
                selected_id,
                sizeof(selected_id)
            ) == 1) {
            strncpy(g_admin_maintenance.doctor_query_id, selected_id, sizeof(g_admin_maintenance.doctor_query_id) - 1);
        }
    }

    Workbench_draw_output_panel(
        app,
        output_rect,
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
        Workbench_show_result(app, result);
    }
    if (GuiButton((Rectangle){ split.list_bounds.x + 130, split.list_bounds.y, 118, 34 }, "低库存")) {
        result = DesktopAdapters_find_low_stock_medicines(&app->application, medicine_output, sizeof(medicine_output));
        Workbench_show_result(app, result);
    }
    Workbench_draw_output_panel(app, (Rectangle){ split.list_bounds.x, split.list_bounds.y + 48, split.list_bounds.width, split.list_bounds.height - 48 }, "病房床位", ward_output, "点击病房总览查看当前病区状态。");
    Workbench_draw_output_panel(app, split.detail_bounds, "药品预警", medicine_output, "管理员可看到药房低库存摘要。");
}

static void admin_draw_system(DesktopApp *app, Rectangle panel) {
    Result result;
    char reset_message[RESULT_MESSAGE_CAPACITY];

    DrawRectangleRounded(panel, 0.12f, 8, app->theme.panel);
    DrawRectangleRoundedLinesEx(panel, 0.12f, 8, 1.0f, Fade(app->theme.border, 0.85f));
    DrawText("系统信息", (int)panel.x + 16, (int)panel.y + 14, 24, app->theme.text_primary);
    Workbench_draw_info_row(app, (int)panel.x + 16, (int)panel.y + 58, "当前用户", app->state.current_user.user_id);
    Workbench_draw_info_row(app, (int)panel.x + 16, (int)panel.y + 86, "当前角色", DesktopApp_user_role_label(app->state.current_user.role));
    if (GuiButton((Rectangle){ panel.x + panel.width - 180, panel.y + 12, 160, 34 }, "重置演示数据")) {
        result = DesktopAdapters_reset_demo_data(
            &app->application,
            app->paths,
            reset_message,
            sizeof(reset_message)
        );
        Workbench_show_result(app, result);
        if (result.success != 0) {
            DesktopAppState_logout(&app->state);
        }
    }
    if (app->paths != 0) {
        Workbench_draw_info_row(app, (int)panel.x + 16, (int)panel.y + 138, "patients", app->paths->patient_path);
        Workbench_draw_info_row(app, (int)panel.x + 16, (int)panel.y + 166, "doctors", app->paths->doctor_path);
        Workbench_draw_info_row(app, (int)panel.x + 16, (int)panel.y + 194, "registrations", app->paths->registration_path);
        Workbench_draw_info_row(app, (int)panel.x + 16, (int)panel.y + 222, "medicines", app->paths->medicine_path);
    }
}

void AdminWorkbench_draw(DesktopApp *app, Rectangle panel, int page) {
    if (!g_admin_initialized) {
        WorkbenchSearchSelectState_mark_dirty(&g_admin_department_select);
        WorkbenchSearchSelectState_mark_dirty(&g_admin_doctor_select);
        WorkbenchSearchSelectState_mark_dirty(&g_admin_doctor_query_select);
        g_admin_initialized = 1;
    }
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
