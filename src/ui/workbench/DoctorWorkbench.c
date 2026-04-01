#include "ui/Workbench.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "repository/RegistrationRepository.h"
#include "raygui.h"
#include "ui/DesktopAdapters.h"
#include "ui/DesktopApp.h"

static MenuApplicationVisitHandoff g_doctor_handoff;
static WorkbenchSearchSelectState g_doctor_pending_select;
static WorkbenchSearchSelectState g_doctor_patient_select;
static WorkbenchSearchSelectState g_doctor_registration_select;
static int g_doctor_initialized = 0;

static void doctor_refresh_pending_registration_select(DesktopApp *app, DesktopDoctorPageState *state, WorkbenchSearchSelectState *picker) {
    LinkedList registrations;
    const LinkedListNode *current = 0;
    int selected_index = -1;

    if (!WorkbenchSearchSelectState_needs_refresh(picker)) return;

    WorkbenchSearchSelectState_reset(picker);
    LinkedList_init(&registrations);
    if (RegistrationRepository_load_all(&app->application.registration_repository, &registrations).success != 1) {
        return;
    }

    current = registrations.head;
    while (current != 0) {
        const Registration *registration = (const Registration *)current->data;
        char label[WORKBENCH_SELECT_LABEL_CAPACITY];

        if (registration->status != REG_STATUS_PENDING ||
            strcmp(registration->doctor_id, state->doctor_id) != 0) {
            current = current->next;
            continue;
        }

        snprintf(label, sizeof(label), "%s | 患者=%s | 科室=%s | %s", registration->registration_id, registration->patient_id, registration->department_id, registration->registered_at);
        if (Workbench_text_contains_ignore_case(label, picker->query)) {
            if (strcmp(state->registration_id, registration->registration_id) == 0) {
                selected_index = picker->option_count;
            }
            WorkbenchSearchSelectState_add_option(picker, label);
        }

        current = current->next;
    }

    WorkbenchSearchSelectState_finalize(picker);
    picker->active_index = selected_index;
    RegistrationRepository_clear_list(&registrations);
}

static void doctor_refresh_patient_select(DesktopApp *app, DesktopDoctorPageState *state) {
    LinkedList patients;
    const LinkedListNode *current = 0;
    int selected_index = -1;
    Result result;

    if (!WorkbenchSearchSelectState_needs_refresh(&g_doctor_patient_select)) return;

    WorkbenchSearchSelectState_reset(&g_doctor_patient_select);
    LinkedList_init(&patients);
    result = DesktopAdapters_search_patients(&app->application, DESKTOP_PATIENT_SEARCH_BY_NAME, "", &patients);
    if (result.success != 1) {
        return;
    }

    current = patients.head;
    while (current != 0) {
        const Patient *patient = (const Patient *)current->data;
        char label[WORKBENCH_SELECT_LABEL_CAPACITY];
        snprintf(label, sizeof(label), "%s | %s | %d岁", patient->patient_id, patient->name, patient->age);
        if (Workbench_text_contains_ignore_case(label, g_doctor_patient_select.query)) {
            if (strcmp(state->patient_id, patient->patient_id) == 0) {
                selected_index = g_doctor_patient_select.option_count;
            }
            WorkbenchSearchSelectState_add_option(&g_doctor_patient_select, label);
        }
        current = current->next;
    }

    WorkbenchSearchSelectState_finalize(&g_doctor_patient_select);
    g_doctor_patient_select.active_index = selected_index;
    LinkedList_clear(&patients, free);
}

static void doctor_draw_checkbox(Rectangle bounds, const char *text, int *value) {
    bool checked = value != 0 && *value != 0;

    GuiCheckBox(bounds, text, &checked);
    if (value != 0) {
        *value = checked ? 1 : 0;
    }
}

static void auto_bind_doctor_id(DesktopApp *app, DesktopDoctorPageState *state) {
    if (app->state.current_user.role == USER_ROLE_DOCTOR && state->doctor_id[0] == '\0') {
        strncpy(state->doctor_id, app->state.current_user.user_id, sizeof(state->doctor_id) - 1);
    }
}

static void doctor_draw_home(DesktopApp *app, Rectangle panel) {
    DesktopDoctorPageState *state = &app->state.doctor_page;
    const WorkbenchDef *wb = Workbench_get(USER_ROLE_DOCTOR);
    const char *labels[4] = { "当前医生", "当前挂号", "当前就诊", "交接提示" };
    const char *values[4];
    WorkbenchButtonGroupLayout actions;
    const char *hint = "--";
    int index = 0;

    auto_bind_doctor_id(app, state);
    if (g_doctor_handoff.visit_id[0] != '\0') {
        if (g_doctor_handoff.need_admission) {
            hint = "转住院";
        } else if (g_doctor_handoff.need_medicine) {
            hint = "转药房";
        } else if (g_doctor_handoff.need_exam) {
            hint = "转检查";
        } else {
            hint = "已接诊";
        }
    }
    values[0] = state->doctor_id[0] != '\0' ? state->doctor_id : "--";
    values[1] = state->registration_id[0] != '\0' ? state->registration_id : "--";
    values[2] = g_doctor_handoff.visit_id[0] != '\0' ? g_doctor_handoff.visit_id : "--";
    values[3] = hint;

    Workbench_draw_home_cards(app, panel, labels, values, wb->accent);
    Workbench_draw_section_header(app, (int)panel.x, (int)panel.y + 104, "医生主任务");
    actions = Workbench_compute_button_group_layout(
        (Rectangle){ panel.x, panel.y + 144.0f, panel.width, 40.0f },
        4,
        150.0f,
        40.0f,
        14.0f
    );
    for (index = 0; index < actions.count; index++) {
        int clicked = 0;
        const char *label = index == 0 ? "待诊列表" :
                            index == 1 ? "患者历史" :
                            index == 2 ? "接诊录入" :
                                         "检查管理";
        Workbench_draw_quick_action_btn(app, actions.buttons[index], label, wb->accent, &clicked);
        if (clicked) {
            app->state.workbench_page = index + 1;
        }
    }
    Workbench_draw_output_panel(
        app,
        (Rectangle){ panel.x, panel.y + 214.0f, panel.width, panel.height - 214.0f },
        "最近交接信息",
        state->output,
        "接诊成功后会在这里展示 visit_id 以及是否需要检查、住院、药房后续处理。"
    );
}

static void doctor_draw_pending(DesktopApp *app, Rectangle panel) {
    DesktopDoctorPageState *state = &app->state.doctor_page;
    Result result;
    Rectangle search_rect;
    Rectangle list_rect;

    auto_bind_doctor_id(app, state);
    doctor_refresh_pending_registration_select(app, state, &g_doctor_pending_select);
    search_rect = (Rectangle){ panel.x, panel.y + 58, panel.width, 34 };
    list_rect = (Rectangle){ panel.x, panel.y + 108, panel.width, panel.height - 108 };

    Workbench_draw_section_header(app, (int)panel.x, (int)panel.y, "待诊患者列表");

    Workbench_draw_form_label(app, (int)panel.x, (int)panel.y + 36, "当前医生", 1);
    Workbench_draw_status_badge(app, (Rectangle){ panel.x + 110, panel.y + 32, 180, 28 }, state->doctor_id, Workbench_get(USER_ROLE_DOCTOR)->accent);
    Workbench_draw_search_select(app, search_rect, list_rect, &g_doctor_pending_select, &state->active_field, 31);
    if (g_doctor_pending_select.active_index >= 0 && g_doctor_pending_select.active_index < g_doctor_pending_select.option_count) {
        sscanf(g_doctor_pending_select.labels[g_doctor_pending_select.active_index], "%15s", state->registration_id);
        result = DesktopAdapters_query_registration(&app->application, state->registration_id, state->output, sizeof(state->output));
        Workbench_show_result(app, result);
    }
}

static void doctor_draw_history(DesktopApp *app, Rectangle panel) {
    DesktopDoctorPageState *state = &app->state.doctor_page;
    Result result;
    Rectangle search_rect = { panel.x, panel.y + 58, panel.width, 34 };
    Rectangle list_rect = { panel.x, panel.y + 108, panel.width, panel.height - 108 };

    doctor_refresh_patient_select(app, state);

    Workbench_draw_section_header(app, (int)panel.x, (int)panel.y, "患者历史查询");
    Workbench_draw_search_select(app, search_rect, list_rect, &g_doctor_patient_select, &state->active_field, 32);
    if (g_doctor_patient_select.active_index >= 0 && g_doctor_patient_select.active_index < g_doctor_patient_select.option_count) {
        sscanf(g_doctor_patient_select.labels[g_doctor_patient_select.active_index], "%15s", state->patient_id);
        result = DesktopAdapters_query_patient_history(&app->application, state->patient_id, state->output, sizeof(state->output));
        Workbench_show_result(app, result);
    }
}

static void doctor_draw_visit_record(DesktopApp *app, Rectangle panel) {
    DesktopDoctorPageState *state = &app->state.doctor_page;
    WorkbenchListDetailLayout split = Workbench_compute_list_detail_layout(panel, 0.48f, 18.0f, 280.0f);
    Result result;
    float ly = split.list_bounds.y;
    float lx = split.list_bounds.x;

    DrawRectangleRounded(split.list_bounds, 0.12f, 8, app->theme.panel);
    DrawRectangleRoundedLinesEx(split.list_bounds, 0.12f, 8, 1.0f, Fade(app->theme.border, 0.85f));
    DrawText("接诊录入", (int)lx + 16, (int)ly + 14, 24, app->theme.text_primary);

    doctor_refresh_pending_registration_select(app, state, &g_doctor_registration_select);
    Workbench_draw_form_label(app, (int)lx + 16, (int)ly + 54, "已选挂号", 1);
    Workbench_draw_status_badge(
        app,
        (Rectangle){ lx + 120, ly + 50, split.list_bounds.width - 136, 28 },
        state->registration_id[0] != '\0' ? state->registration_id : "请先在右侧选择待诊挂号",
        Workbench_get(USER_ROLE_DOCTOR)->accent
    );

    Workbench_draw_form_label(app, (int)lx + 16, (int)ly + 120, "主诉", 1);
    Workbench_draw_text_input((Rectangle){ lx + 16, ly + 142, split.list_bounds.width - 32, 34 }, state->chief_complaint, sizeof(state->chief_complaint), 1, &state->active_field, 2);

    Workbench_draw_form_label(app, (int)lx + 16, (int)ly + 186, "诊断", 1);
    Workbench_draw_text_input((Rectangle){ lx + 16, ly + 208, split.list_bounds.width - 32, 34 }, state->diagnosis, sizeof(state->diagnosis), 1, &state->active_field, 3);

    Workbench_draw_form_label(app, (int)lx + 16, (int)ly + 252, "建议", 0);
    Workbench_draw_text_input((Rectangle){ lx + 16, ly + 274, split.list_bounds.width - 32, 34 }, state->advice, sizeof(state->advice), 1, &state->active_field, 4);

    Workbench_draw_form_label(app, (int)lx + 16, (int)ly + 318, "接诊时间", 1);
    Workbench_draw_text_input((Rectangle){ lx + 16, ly + 340, split.list_bounds.width - 32, 34 }, state->visit_time, sizeof(state->visit_time), 1, &state->active_field, 5);

    Workbench_draw_section_header(app, (int)lx + 16, (int)ly + 388, "后续处理");
    doctor_draw_checkbox((Rectangle){ lx + 16, ly + 422, 18, 18 }, "需要检查", &state->need_exam);
    doctor_draw_checkbox((Rectangle){ lx + 140, ly + 422, 18, 18 }, "需要住院", &state->need_admission);
    doctor_draw_checkbox((Rectangle){ lx + 264, ly + 422, 18, 18 }, "需要用药", &state->need_medicine);

    if (GuiButton((Rectangle){ lx + 16, ly + 464, 160, 40 }, "提交接诊")) {
        memset(&g_doctor_handoff, 0, sizeof(g_doctor_handoff));
        result = DesktopAdapters_create_visit_record_handoff(
            &app->application,
            state->registration_id,
            state->chief_complaint,
            state->diagnosis,
            state->advice,
            state->need_exam,
            state->need_admission,
            state->need_medicine,
            state->visit_time,
            &g_doctor_handoff
        );
        Workbench_show_result(app, result);
        if (result.success) {
            snprintf(
                state->output,
                sizeof(state->output),
                "visit_id=%s | patient=%s | diagnosis=%s | exam=%d | admission=%d | medicine=%d | 下游凭据统一使用 visit_id",
                g_doctor_handoff.visit_id,
                g_doctor_handoff.patient_id,
                g_doctor_handoff.diagnosis,
                g_doctor_handoff.need_exam,
                g_doctor_handoff.need_admission,
                g_doctor_handoff.need_medicine
            );
            strncpy(state->visit_id, g_doctor_handoff.visit_id, sizeof(state->visit_id) - 1);
            strncpy(state->patient_id, g_doctor_handoff.patient_id, sizeof(state->patient_id) - 1);
            DesktopApp_mark_dirty(&app->state, DESKTOP_PAGE_DASHBOARD);
            WorkbenchSearchSelectState_mark_dirty(&g_doctor_pending_select);
            WorkbenchSearchSelectState_mark_dirty(&g_doctor_registration_select);
        }
    }

    Workbench_draw_output_panel(
        app,
        split.detail_bounds,
        "待诊挂号与交接摘要",
        state->output,
        "医生接诊后会生成 visit_id，药房和住院后续都可以直接复用该凭据。"
    );
    Workbench_draw_search_select(
        app,
        (Rectangle){ split.detail_bounds.x + 14, split.detail_bounds.y + 50, split.detail_bounds.width - 28, 34 },
        (Rectangle){ split.detail_bounds.x + 14, split.detail_bounds.y + 92, split.detail_bounds.width - 28, split.detail_bounds.height * 0.38f },
        &g_doctor_registration_select,
        &state->active_field,
        33
    );
    if (g_doctor_registration_select.active_index >= 0 && g_doctor_registration_select.active_index < g_doctor_registration_select.option_count) {
        sscanf(g_doctor_registration_select.labels[g_doctor_registration_select.active_index], "%15s", state->registration_id);
    }
}

static void doctor_draw_examination(DesktopApp *app, Rectangle panel) {
    DesktopDoctorPageState *state = &app->state.doctor_page;
    WorkbenchListDetailLayout split = Workbench_compute_list_detail_layout(panel, 0.48f, 18.0f, 280.0f);
    Result result;
    float ly = split.list_bounds.y;
    float lx = split.list_bounds.x;

    DrawRectangleRounded(split.list_bounds, 0.12f, 8, app->theme.panel);
    DrawRectangleRoundedLinesEx(split.list_bounds, 0.12f, 8, 1.0f, Fade(app->theme.border, 0.85f));
    DrawText("检查管理", (int)lx + 16, (int)ly + 14, 24, app->theme.text_primary);

    Workbench_draw_section_header(app, (int)lx + 16, (int)ly + 50, "新增检查");

    Workbench_draw_form_label(app, (int)lx + 16, (int)ly + 84, "就诊编号", 1);
    Workbench_draw_text_input((Rectangle){ lx + 16, ly + 106, split.list_bounds.width - 32, 34 }, state->visit_id, sizeof(state->visit_id), 1, &state->active_field, 6);

    Workbench_draw_form_label(app, (int)lx + 16, (int)ly + 150, "检查项目", 1);
    Workbench_draw_text_input((Rectangle){ lx + 16, ly + 172, split.list_bounds.width - 32, 34 }, state->exam_item, sizeof(state->exam_item), 1, &state->active_field, 7);

    Workbench_draw_form_label(app, (int)lx + 16, (int)ly + 216, "检查类型", 1);
    Workbench_draw_text_input((Rectangle){ lx + 16, ly + 238, split.list_bounds.width - 32, 34 }, state->exam_type, sizeof(state->exam_type), 1, &state->active_field, 8);

    Workbench_draw_form_label(app, (int)lx + 16, (int)ly + 282, "申请时间", 1);
    Workbench_draw_text_input((Rectangle){ lx + 16, ly + 304, split.list_bounds.width - 32, 34 }, state->requested_at, sizeof(state->requested_at), 1, &state->active_field, 9);

    if (GuiButton((Rectangle){ lx + 16, ly + 352, 160, 40 }, "新增检查")) {
        result = DesktopAdapters_create_examination_record(&app->application, state->visit_id, state->exam_item, state->exam_type, state->requested_at, state->output, sizeof(state->output));
        Workbench_show_result(app, result);
    }

    Workbench_draw_section_header(app, (int)lx + 16, (int)ly + 414, "回写检查结果");

    Workbench_draw_form_label(app, (int)lx + 16, (int)ly + 448, "检查编号", 1);
    Workbench_draw_text_input((Rectangle){ lx + 16, ly + 470, split.list_bounds.width - 32, 34 }, state->examination_id, sizeof(state->examination_id), 1, &state->active_field, 10);

    Workbench_draw_form_label(app, (int)lx + 16, (int)ly + 514, "检查结果", 1);
    Workbench_draw_text_input((Rectangle){ lx + 16, ly + 536, split.list_bounds.width - 32, 34 }, state->exam_result, sizeof(state->exam_result), 1, &state->active_field, 11);

    Workbench_draw_form_label(app, (int)lx + 16, (int)ly + 580, "完成时间", 1);
    Workbench_draw_text_input((Rectangle){ lx + 16, ly + 602, split.list_bounds.width - 32, 34 }, state->completed_at, sizeof(state->completed_at), 1, &state->active_field, 12);

    if (GuiButton((Rectangle){ lx + 16, ly + 650, 160, 40 }, "回写检查")) {
        result = DesktopAdapters_complete_examination_record(&app->application, state->examination_id, state->exam_result, state->completed_at, state->output, sizeof(state->output));
        Workbench_show_result(app, result);
    }

    Workbench_draw_output_panel(
        app,
        split.detail_bounds,
        "检查输出",
        state->output,
        "支持新增检查和回写检查结果，患者端可看到同一条链路的结果。"
    );
}

void DoctorWorkbench_draw(DesktopApp *app, Rectangle panel, int page) {
    if (!g_doctor_initialized) {
        WorkbenchSearchSelectState_mark_dirty(&g_doctor_pending_select);
        WorkbenchSearchSelectState_mark_dirty(&g_doctor_patient_select);
        WorkbenchSearchSelectState_mark_dirty(&g_doctor_registration_select);
        g_doctor_initialized = 1;
    }
    switch (page) {
        case 0:
            doctor_draw_home(app, panel);
            break;
        case 1:
            doctor_draw_pending(app, panel);
            break;
        case 2:
            doctor_draw_history(app, panel);
            break;
        case 3:
            doctor_draw_visit_record(app, panel);
            break;
        case 4:
            doctor_draw_examination(app, panel);
            break;
        default:
            doctor_draw_home(app, panel);
            break;
    }
}
