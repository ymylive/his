#include "ui/Workbench.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "raygui.h"
#include "ui/DesktopAdapters.h"
#include "ui/DesktopApp.h"

typedef struct PatientRegistrationViewState {
    char departments[WORKBENCH_OUTPUT_CAPACITY];
    char doctors[WORKBENCH_OUTPUT_CAPACITY];
    char registrations[WORKBENCH_OUTPUT_CAPACITY];
} PatientRegistrationViewState;

static PatientRegistrationViewState g_patient_registration_view;

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

static void show_result(DesktopApp *app, Result result) {
    DesktopAppState_show_message(
        &app->state,
        result.success ? DESKTOP_MESSAGE_SUCCESS : DESKTOP_MESSAGE_ERROR,
        result.message
    );
}

static void auto_bind_patient_id(DesktopApp *app) {
    if (app->state.current_user.role != USER_ROLE_PATIENT) {
        return;
    }

    if (app->state.registration_page.patient_id[0] == '\0') {
        strncpy(
            app->state.registration_page.patient_id,
            app->state.current_user.user_id,
            sizeof(app->state.registration_page.patient_id) - 1
        );
    }
    if (app->state.doctor_page.patient_id[0] == '\0') {
        strncpy(
            app->state.doctor_page.patient_id,
            app->state.current_user.user_id,
            sizeof(app->state.doctor_page.patient_id) - 1
        );
    }
    if (app->state.dispense_page.patient_id[0] == '\0') {
        strncpy(
            app->state.dispense_page.patient_id,
            app->state.current_user.user_id,
            sizeof(app->state.dispense_page.patient_id) - 1
        );
    }
    if (app->state.inpatient_page.query_patient_id[0] == '\0') {
        strncpy(
            app->state.inpatient_page.query_patient_id,
            app->state.current_user.user_id,
            sizeof(app->state.inpatient_page.query_patient_id) - 1
        );
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

static void patient_draw_home(DesktopApp *app, Rectangle panel) {
    const WorkbenchDef *wb = Workbench_get(USER_ROLE_PATIENT);
    const char *labels[4] = { "当前身份", "最近挂号", "住院状态", "发药记录" };
    const char *values[4];
    WorkbenchButtonGroupLayout actions;
    char role_value[32];
    const char *last_registration = "--";
    const char *admission_status = "未住院";
    const char *dispense_status = app->state.dispense_page.loaded > 0 ? "已同步" : "待同步";
    int index = 0;

    auto_bind_patient_id(app);
    snprintf(role_value, sizeof(role_value), "%s", "患者");
    if (app->state.registration_page.last_registration_id[0] != '\0') {
        last_registration = app->state.registration_page.last_registration_id;
    }
    if (app->state.inpatient_page.output[0] != '\0') {
        admission_status = "有住院信息";
    }
    values[0] = role_value;
    values[1] = last_registration;
    values[2] = admission_status;
    values[3] = dispense_status;

    Workbench_draw_home_cards(app, panel, labels, values, wb->accent);
    Workbench_draw_section_header(app, (int)panel.x, (int)panel.y + 104, "患者自助流程");
    actions = Workbench_compute_button_group_layout(
        (Rectangle){ panel.x, panel.y + 144.0f, panel.width, 40.0f },
        4,
        150.0f,
        40.0f,
        14.0f
    );
    for (index = 0; index < actions.count; index++) {
        int clicked = 0;
        const char *label = index == 0 ? "自助挂号" :
                            index == 1 ? "我的病历" :
                            index == 2 ? "住院状态" :
                                         "用药说明";
        Workbench_draw_quick_action_btn(app, actions.buttons[index], label, wb->accent, &clicked);
        if (clicked) {
            app->state.workbench_page = index + 1;
        }
    }

    draw_output_panel(
        app,
        (Rectangle){ panel.x, panel.y + 214.0f, panel.width, panel.height - 214.0f },
        "当前说明",
        "患者工作台已接通自助挂号、病历/检查查询、住院状态查看、发药记录与药品说明查询。",
        "暂无说明"
    );
}

static void patient_draw_registration(DesktopApp *app, Rectangle panel) {
    DesktopRegistrationPageState *state = &app->state.registration_page;
    WorkbenchListDetailLayout split = Workbench_compute_list_detail_layout(panel, 0.46f, 18.0f, 280.0f);
    WorkbenchButtonGroupLayout actions;
    Result result;
    int index = 0;

    auto_bind_patient_id(app);

    DrawRectangleRounded(split.list_bounds, 0.12f, 8, app->theme.panel);
    DrawRectangleRoundedLinesEx(split.list_bounds, 0.12f, 8, 1.0f, Fade(app->theme.border, 0.85f));
    DrawText("自助挂号", (int)split.list_bounds.x + 16, (int)split.list_bounds.y + 14, 24, app->theme.text_primary);
    Workbench_draw_info_row(
        app,
        (int)split.list_bounds.x + 16,
        (int)split.list_bounds.y + 52,
        "患者编号",
        state->patient_id
    );

    GuiLabel((Rectangle){ split.list_bounds.x + 16, split.list_bounds.y + 96, 120, 20 }, "科室编号");
    draw_text_input(
        (Rectangle){ split.list_bounds.x + 16, split.list_bounds.y + 120, split.list_bounds.width - 32, 34 },
        state->department_id,
        sizeof(state->department_id),
        1,
        &state->active_field,
        1
    );
    GuiLabel((Rectangle){ split.list_bounds.x + 16, split.list_bounds.y + 168, 120, 20 }, "医生编号");
    draw_text_input(
        (Rectangle){ split.list_bounds.x + 16, split.list_bounds.y + 192, split.list_bounds.width - 32, 34 },
        state->doctor_id,
        sizeof(state->doctor_id),
        1,
        &state->active_field,
        2
    );
    GuiLabel((Rectangle){ split.list_bounds.x + 16, split.list_bounds.y + 240, 120, 20 }, "挂号时间");
    draw_text_input(
        (Rectangle){ split.list_bounds.x + 16, split.list_bounds.y + 264, split.list_bounds.width - 32, 34 },
        state->registered_at,
        sizeof(state->registered_at),
        1,
        &state->active_field,
        3
    );

    actions = Workbench_compute_button_group_layout(
        (Rectangle){ split.list_bounds.x + 16, split.list_bounds.y + 320.0f, split.list_bounds.width - 32, 36.0f },
        3,
        110.0f,
        36.0f,
        12.0f
    );
    for (index = 0; index < actions.count; index++) {
        int clicked = 0;
        const char *label = index == 0 ? "科室列表" : index == 1 ? "医生列表" : "提交挂号";
        Workbench_draw_quick_action_btn(app, actions.buttons[index], label, app->theme.nav_active, &clicked);
        if (!clicked) {
            continue;
        }
        if (index == 0) {
            result = DesktopAdapters_list_departments(
                &app->application,
                g_patient_registration_view.departments,
                sizeof(g_patient_registration_view.departments)
            );
            show_result(app, result);
        } else if (index == 1) {
            result = DesktopAdapters_list_doctors_by_department(
                &app->application,
                state->department_id,
                g_patient_registration_view.doctors,
                sizeof(g_patient_registration_view.doctors)
            );
            show_result(app, result);
        } else {
            Registration registration;
            memset(&registration, 0, sizeof(registration));
            result = DesktopAdapters_submit_self_registration(
                &app->application,
                state->doctor_id,
                state->department_id,
                state->registered_at,
                &registration
            );
            show_result(app, result);
            if (result.success) {
                strncpy(
                    state->last_registration_id,
                    registration.registration_id,
                    sizeof(state->last_registration_id) - 1
                );
                DesktopAdapters_query_registrations_by_patient(
                    &app->application,
                    state->patient_id,
                    g_patient_registration_view.registrations,
                    sizeof(g_patient_registration_view.registrations)
                );
                DesktopApp_mark_dirty(&app->state, DESKTOP_PAGE_DASHBOARD);
            }
        }
    }

    DrawText(
        TextFormat("最近成功挂号: %s", state->last_registration_id[0] != '\0' ? state->last_registration_id : "--"),
        (int)split.list_bounds.x + 16,
        (int)split.list_bounds.y + 380,
        18,
        app->theme.text_secondary
    );

    DrawRectangleRounded(split.detail_bounds, 0.12f, 8, app->theme.panel);
    DrawRectangleRoundedLinesEx(split.detail_bounds, 0.12f, 8, 1.0f, Fade(app->theme.border, 0.85f));
    draw_output_panel(
        app,
        (Rectangle){
            split.detail_bounds.x + 12,
            split.detail_bounds.y + 12,
            split.detail_bounds.width - 24,
            (split.detail_bounds.height - 42) / 3.0f
        },
        "科室列表",
        g_patient_registration_view.departments,
        "点击“科室列表”加载可选科室。"
    );
    draw_output_panel(
        app,
        (Rectangle){
            split.detail_bounds.x + 12,
            split.detail_bounds.y + 18 + (split.detail_bounds.height - 42) / 3.0f,
            split.detail_bounds.width - 24,
            (split.detail_bounds.height - 42) / 3.0f
        },
        "医生列表",
        g_patient_registration_view.doctors,
        "输入科室编号后点击“医生列表”。"
    );
    draw_output_panel(
        app,
        (Rectangle){
            split.detail_bounds.x + 12,
            split.detail_bounds.y + 24 + (split.detail_bounds.height - 42) / 3.0f * 2.0f,
            split.detail_bounds.width - 24,
            (split.detail_bounds.height - 42) / 3.0f
        },
        "我的挂号",
        g_patient_registration_view.registrations,
        "提交挂号后会在这里刷新个人挂号记录。"
    );
}

static void patient_draw_visits(DesktopApp *app, Rectangle panel) {
    DesktopDoctorPageState *state = &app->state.doctor_page;
    Result result;

    auto_bind_patient_id(app);
    if (GuiButton((Rectangle){ panel.x + panel.width - 118, panel.y, 118, 34 }, "刷新病历")) {
        result = DesktopAdapters_query_patient_history(
            &app->application,
            state->patient_id,
            state->output,
            sizeof(state->output)
        );
        show_result(app, result);
    }
    draw_output_panel(
        app,
        (Rectangle){ panel.x, panel.y + 48, panel.width, panel.height - 48 },
        "我的病历与检查",
        state->output,
        "点击“刷新病历”加载挂号、看诊、检查和住院摘要。"
    );
}

static void patient_draw_admission(DesktopApp *app, Rectangle panel) {
    DesktopInpatientPageState *state = &app->state.inpatient_page;
    Result result;

    auto_bind_patient_id(app);
    if (GuiButton((Rectangle){ panel.x + panel.width - 118, panel.y, 118, 34 }, "刷新住院")) {
        result = DesktopAdapters_query_active_admission_by_patient(
            &app->application,
            state->query_patient_id,
            state->output,
            sizeof(state->output)
        );
        show_result(app, result);
    }
    draw_output_panel(
        app,
        (Rectangle){ panel.x, panel.y + 48, panel.width, panel.height - 48 },
        "住院状态",
        state->output,
        "当前无在院记录时会在这里显示查询失败提示，可用于答辩演示。"
    );
}

static void patient_draw_dispense(DesktopApp *app, Rectangle panel) {
    DesktopDispensePageState *history_state = &app->state.dispense_page;
    DesktopPharmacyPageState *medicine_state = &app->state.pharmacy_page;
    WorkbenchListDetailLayout split = Workbench_compute_list_detail_layout(panel, 0.5f, 18.0f, 260.0f);
    const LinkedListNode *current = 0;
    int row = 0;
    Result result;

    auto_bind_patient_id(app);

    if (GuiButton((Rectangle){ split.list_bounds.x, split.list_bounds.y, 118, 34 }, "刷新发药")) {
        LinkedList_clear(&history_state->results, free);
        LinkedList_init(&history_state->results);
        result = DesktopAdapters_load_dispense_history(
            &app->application,
            history_state->patient_id,
            &history_state->results
        );
        history_state->loaded = result.success ? 1 : -1;
        show_result(app, result);
    }
    DrawRectangleRounded(
        (Rectangle){ split.list_bounds.x, split.list_bounds.y + 48, split.list_bounds.width, split.list_bounds.height - 48 },
        0.12f,
        8,
        app->theme.panel
    );
    DrawRectangleRoundedLinesEx(
        (Rectangle){ split.list_bounds.x, split.list_bounds.y + 48, split.list_bounds.width, split.list_bounds.height - 48 },
        0.12f,
        8,
        1.0f,
        Fade(app->theme.border, 0.85f)
    );
    DrawText("我的发药记录", (int)split.list_bounds.x + 16, (int)split.list_bounds.y + 60, 20, app->theme.text_primary);
    current = history_state->results.head;
    while (current != 0 && row < 8) {
        const DispenseRecord *record = (const DispenseRecord *)current->data;
        DrawText(
            TextFormat("%s | %s | %s x%d", record->dispense_id, record->medicine_id, record->dispensed_at, record->quantity),
            (int)split.list_bounds.x + 16,
            (int)split.list_bounds.y + 96 + row * 28,
            17,
            app->theme.text_secondary
        );
        current = current->next;
        row++;
    }
    if (row == 0) {
        DrawText("暂无发药记录。", (int)split.list_bounds.x + 16, (int)split.list_bounds.y + 96, 17, app->theme.text_secondary);
    }

    DrawRectangleRounded(split.detail_bounds, 0.12f, 8, app->theme.panel);
    DrawRectangleRoundedLinesEx(split.detail_bounds, 0.12f, 8, 1.0f, Fade(app->theme.border, 0.85f));
    DrawText("药品说明查询", (int)split.detail_bounds.x + 16, (int)split.detail_bounds.y + 14, 20, app->theme.text_primary);
    GuiLabel((Rectangle){ split.detail_bounds.x + 16, split.detail_bounds.y + 48, 120, 20 }, "药品编号");
    draw_text_input(
        (Rectangle){ split.detail_bounds.x + 16, split.detail_bounds.y + 72, split.detail_bounds.width - 152, 34 },
        medicine_state->stock_query_medicine_id,
        sizeof(medicine_state->stock_query_medicine_id),
        1,
        &medicine_state->active_field,
        1
    );
    if (GuiButton((Rectangle){ split.detail_bounds.x + split.detail_bounds.width - 118, split.detail_bounds.y + 72, 102, 34 }, "查询说明")) {
        result = DesktopAdapters_query_medicine_detail(
            &app->application,
            medicine_state->stock_query_medicine_id,
            1,
            medicine_state->output,
            sizeof(medicine_state->output)
        );
        show_result(app, result);
    }
    draw_output_panel(
        app,
        (Rectangle){ split.detail_bounds.x + 12, split.detail_bounds.y + 124, split.detail_bounds.width - 24, split.detail_bounds.height - 136 },
        "药品说明",
        medicine_state->output,
        "输入药品编号后点击“查询说明”。"
    );
}

void PatientWorkbench_draw(DesktopApp *app, Rectangle panel, int page) {
    switch (page) {
        case 0:
            patient_draw_home(app, panel);
            break;
        case 1:
            patient_draw_registration(app, panel);
            break;
        case 2:
            patient_draw_visits(app, panel);
            break;
        case 3:
            patient_draw_admission(app, panel);
            break;
        case 4:
            patient_draw_dispense(app, panel);
            break;
        default:
            patient_draw_home(app, panel);
            break;
    }
}
