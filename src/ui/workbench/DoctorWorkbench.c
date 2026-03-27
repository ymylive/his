#include "ui/Workbench.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "raygui.h"
#include "ui/DesktopAdapters.h"
#include "ui/DesktopApp.h"

static MenuApplicationVisitHandoff g_doctor_handoff;

static void doctor_draw_checkbox(Rectangle bounds, const char *text, int *value) {
    bool checked = value != 0 && *value != 0;

    GuiCheckBox(bounds, text, &checked);
    if (value != 0) {
        *value = checked ? 1 : 0;
    }
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

static void show_result(DesktopApp *app, Result result) {
    DesktopAppState_show_message(
        &app->state,
        result.success ? DESKTOP_MESSAGE_SUCCESS : DESKTOP_MESSAGE_ERROR,
        result.message
    );
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
    draw_output_panel(
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

    auto_bind_doctor_id(app, state);
    draw_text_input((Rectangle){ panel.x, panel.y, panel.width - 130, 34 }, state->doctor_id, sizeof(state->doctor_id), app->state.current_user.role != USER_ROLE_DOCTOR, &state->active_field, 1);
    if (GuiButton((Rectangle){ panel.x + panel.width - 118, panel.y, 118, 34 }, "刷新待诊")) {
        result = DesktopAdapters_query_pending_registrations_by_doctor(&app->application, state->doctor_id, state->output, sizeof(state->output));
        show_result(app, result);
    }
    draw_output_panel(
        app,
        (Rectangle){ panel.x, panel.y + 48, panel.width, panel.height - 48 },
        "待诊列表",
        state->output,
        "输入医生编号后刷新，可查看当前待诊挂号。"
    );
}

static void doctor_draw_history(DesktopApp *app, Rectangle panel) {
    DesktopDoctorPageState *state = &app->state.doctor_page;
    Result result;

    draw_text_input((Rectangle){ panel.x, panel.y, panel.width - 130, 34 }, state->patient_id, sizeof(state->patient_id), 1, &state->active_field, 1);
    if (GuiButton((Rectangle){ panel.x + panel.width - 118, panel.y, 118, 34 }, "查询历史")) {
        result = DesktopAdapters_query_patient_history(&app->application, state->patient_id, state->output, sizeof(state->output));
        show_result(app, result);
    }
    draw_output_panel(
        app,
        (Rectangle){ panel.x, panel.y + 48, panel.width, panel.height - 48 },
        "患者历史",
        state->output,
        "支持在工作台内查看挂号、看诊、检查、住院四类摘要。"
    );
}

static void doctor_draw_visit_record(DesktopApp *app, Rectangle panel) {
    DesktopDoctorPageState *state = &app->state.doctor_page;
    WorkbenchListDetailLayout split = Workbench_compute_list_detail_layout(panel, 0.48f, 18.0f, 280.0f);
    Result result;

    DrawRectangleRounded(split.list_bounds, 0.12f, 8, app->theme.panel);
    DrawRectangleRoundedLinesEx(split.list_bounds, 0.12f, 8, 1.0f, Fade(app->theme.border, 0.85f));
    DrawText("接诊录入", (int)split.list_bounds.x + 16, (int)split.list_bounds.y + 14, 24, app->theme.text_primary);

    GuiLabel((Rectangle){ split.list_bounds.x + 16, split.list_bounds.y + 54, 120, 20 }, "挂号编号");
    draw_text_input((Rectangle){ split.list_bounds.x + 16, split.list_bounds.y + 78, split.list_bounds.width - 32, 34 }, state->registration_id, sizeof(state->registration_id), 1, &state->active_field, 1);
    GuiLabel((Rectangle){ split.list_bounds.x + 16, split.list_bounds.y + 126, 120, 20 }, "主诉");
    draw_text_input((Rectangle){ split.list_bounds.x + 16, split.list_bounds.y + 150, split.list_bounds.width - 32, 34 }, state->chief_complaint, sizeof(state->chief_complaint), 1, &state->active_field, 2);
    GuiLabel((Rectangle){ split.list_bounds.x + 16, split.list_bounds.y + 198, 120, 20 }, "诊断");
    draw_text_input((Rectangle){ split.list_bounds.x + 16, split.list_bounds.y + 222, split.list_bounds.width - 32, 34 }, state->diagnosis, sizeof(state->diagnosis), 1, &state->active_field, 3);
    GuiLabel((Rectangle){ split.list_bounds.x + 16, split.list_bounds.y + 270, 120, 20 }, "建议");
    draw_text_input((Rectangle){ split.list_bounds.x + 16, split.list_bounds.y + 294, split.list_bounds.width - 32, 34 }, state->advice, sizeof(state->advice), 1, &state->active_field, 4);
    GuiLabel((Rectangle){ split.list_bounds.x + 16, split.list_bounds.y + 342, 120, 20 }, "接诊时间");
    draw_text_input((Rectangle){ split.list_bounds.x + 16, split.list_bounds.y + 366, split.list_bounds.width - 32, 34 }, state->visit_time, sizeof(state->visit_time), 1, &state->active_field, 5);

    doctor_draw_checkbox((Rectangle){ split.list_bounds.x + 16, split.list_bounds.y + 416, 18, 18 }, "需要检查", &state->need_exam);
    doctor_draw_checkbox((Rectangle){ split.list_bounds.x + 140, split.list_bounds.y + 416, 18, 18 }, "需要住院", &state->need_admission);
    doctor_draw_checkbox((Rectangle){ split.list_bounds.x + 264, split.list_bounds.y + 416, 18, 18 }, "需要用药", &state->need_medicine);

    if (GuiButton((Rectangle){ split.list_bounds.x + 16, split.list_bounds.y + 454, 140, 38 }, "提交接诊")) {
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
        show_result(app, result);
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
        }
    }

    draw_output_panel(
        app,
        split.detail_bounds,
        "交接摘要",
        state->output,
        "医生接诊后会生成 visit_id，药房和住院后续都可以直接复用该凭据。"
    );
}

static void doctor_draw_examination(DesktopApp *app, Rectangle panel) {
    DesktopDoctorPageState *state = &app->state.doctor_page;
    WorkbenchListDetailLayout split = Workbench_compute_list_detail_layout(panel, 0.48f, 18.0f, 280.0f);
    Result result;

    DrawRectangleRounded(split.list_bounds, 0.12f, 8, app->theme.panel);
    DrawRectangleRoundedLinesEx(split.list_bounds, 0.12f, 8, 1.0f, Fade(app->theme.border, 0.85f));
    DrawText("检查管理", (int)split.list_bounds.x + 16, (int)split.list_bounds.y + 14, 24, app->theme.text_primary);

    GuiLabel((Rectangle){ split.list_bounds.x + 16, split.list_bounds.y + 54, 120, 20 }, "就诊编号");
    draw_text_input((Rectangle){ split.list_bounds.x + 16, split.list_bounds.y + 78, split.list_bounds.width - 32, 34 }, state->visit_id, sizeof(state->visit_id), 1, &state->active_field, 6);
    GuiLabel((Rectangle){ split.list_bounds.x + 16, split.list_bounds.y + 126, 120, 20 }, "检查项目");
    draw_text_input((Rectangle){ split.list_bounds.x + 16, split.list_bounds.y + 150, split.list_bounds.width - 32, 34 }, state->exam_item, sizeof(state->exam_item), 1, &state->active_field, 7);
    GuiLabel((Rectangle){ split.list_bounds.x + 16, split.list_bounds.y + 198, 120, 20 }, "检查类型");
    draw_text_input((Rectangle){ split.list_bounds.x + 16, split.list_bounds.y + 222, split.list_bounds.width - 32, 34 }, state->exam_type, sizeof(state->exam_type), 1, &state->active_field, 8);
    GuiLabel((Rectangle){ split.list_bounds.x + 16, split.list_bounds.y + 270, 120, 20 }, "申请时间");
    draw_text_input((Rectangle){ split.list_bounds.x + 16, split.list_bounds.y + 294, split.list_bounds.width - 32, 34 }, state->requested_at, sizeof(state->requested_at), 1, &state->active_field, 9);
    if (GuiButton((Rectangle){ split.list_bounds.x + 16, split.list_bounds.y + 342, 140, 38 }, "新增检查")) {
        result = DesktopAdapters_create_examination_record(&app->application, state->visit_id, state->exam_item, state->exam_type, state->requested_at, state->output, sizeof(state->output));
        show_result(app, result);
    }

    GuiLabel((Rectangle){ split.list_bounds.x + 16, split.list_bounds.y + 404, 120, 20 }, "检查编号");
    draw_text_input((Rectangle){ split.list_bounds.x + 16, split.list_bounds.y + 428, split.list_bounds.width - 32, 34 }, state->examination_id, sizeof(state->examination_id), 1, &state->active_field, 10);
    GuiLabel((Rectangle){ split.list_bounds.x + 16, split.list_bounds.y + 476, 120, 20 }, "检查结果");
    draw_text_input((Rectangle){ split.list_bounds.x + 16, split.list_bounds.y + 500, split.list_bounds.width - 32, 34 }, state->exam_result, sizeof(state->exam_result), 1, &state->active_field, 11);
    GuiLabel((Rectangle){ split.list_bounds.x + 16, split.list_bounds.y + 548, 120, 20 }, "完成时间");
    draw_text_input((Rectangle){ split.list_bounds.x + 16, split.list_bounds.y + 572, split.list_bounds.width - 32, 34 }, state->completed_at, sizeof(state->completed_at), 1, &state->active_field, 12);
    if (GuiButton((Rectangle){ split.list_bounds.x + 16, split.list_bounds.y + 620, 140, 38 }, "回写检查")) {
        result = DesktopAdapters_complete_examination_record(&app->application, state->examination_id, state->exam_result, state->completed_at, state->output, sizeof(state->output));
        show_result(app, result);
    }

    draw_output_panel(
        app,
        split.detail_bounds,
        "检查输出",
        state->output,
        "支持新增检查和回写检查结果，患者端可看到同一条链路的结果。"
    );
}

void DoctorWorkbench_draw(DesktopApp *app, Rectangle panel, int page) {
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
