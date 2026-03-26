#include "ui/Workbench.h"
#include "ui/DesktopApp.h"
#include "ui/DesktopAdapters.h"
#include "raygui.h"
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* ── Local helpers ── */

static void draw_text_input(Rectangle bounds, char *text, int text_size,
                            int editable, int *active_field, int field_id) {
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

static void draw_output_panel(const DesktopApp *app, Rectangle rect,
                              const char *title, const char *content,
                              const char *empty_text) {
    DrawRectangleRounded(rect, 0.12f, 8, app->theme.panel);
    DrawRectangleRoundedLinesEx(rect, 0.12f, 8, 1.0f,
                                Fade(app->theme.border, 0.85f));
    DrawText(title, (int)rect.x + 14, (int)rect.y + 12, 20,
             app->theme.text_primary);
    if (content == 0 || content[0] == '\0') {
        DrawText(empty_text, (int)rect.x + 14, (int)rect.y + 48, 17,
                 app->theme.text_secondary);
        return;
    }
    DrawText(content, (int)rect.x + 14, (int)rect.y + 48, 17,
             app->theme.text_secondary);
}

static void release_focus_on_click(int *active_field,
                                   const Rectangle *fields, int count) {
    Vector2 mouse = GetMousePosition();
    if (active_field == 0 || fields == 0 || count <= 0) return;
    if (*active_field <= 0 || *active_field > count) return;
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) &&
        !CheckCollisionPointRec(mouse, fields[*active_field - 1])) {
        *active_field = 0;
    }
}

static void auto_bind_doctor_id(DesktopApp *app, DesktopDoctorPageState *s) {
    if (app->state.current_user.role == USER_ROLE_DOCTOR &&
        s->doctor_id[0] == '\0') {
        strncpy(s->doctor_id, app->state.current_user.user_id,
                sizeof(s->doctor_id) - 1);
        s->doctor_id[sizeof(s->doctor_id) - 1] = '\0';
    }
}

/* ── Page 0: 接诊首页 ── */

static void draw_home(DesktopApp *app, Rectangle panel) {
    DesktopDoctorPageState *state = &app->state.doctor_page;
    const WorkbenchDef *wb = Workbench_get(USER_ROLE_DOCTOR);
    const char *labels[4] = { "待诊人数", "今日已接诊", "待回写检查", "药品查询入口" };
    const char *values[4] = { "--", "--", "--", ">" };
    int clicked = 0;
    float y = panel.y + 110;

    auto_bind_doctor_id(app, state);

    Workbench_draw_home_cards(app, panel, labels, values, wb->accent);

    /* Quick actions */
    Workbench_draw_section_header(app, (int)panel.x, (int)panel.y + 100, "快捷操作");

    Workbench_draw_quick_action_btn(
        app, (Rectangle){ panel.x, y + 10, 160, 38 },
        "下一位患者", wb->accent, &clicked);
    if (clicked) app->state.workbench_page = 1;

    Workbench_draw_quick_action_btn(
        app, (Rectangle){ panel.x + 178, y + 10, 160, 38 },
        "查询历史", wb->accent, &clicked);
    if (clicked) app->state.workbench_page = 2;

    Workbench_draw_quick_action_btn(
        app, (Rectangle){ panel.x + 356, y + 10, 160, 38 },
        "写诊疗", wb->accent, &clicked);
    if (clicked) app->state.workbench_page = 3;

    /* Recent records section */
    Workbench_draw_section_header(app, (int)panel.x, (int)panel.y + 180, "最近记录");
    Workbench_draw_info_row(app, (int)panel.x, (int)panel.y + 216,
                            "医生编号", state->doctor_id);
    draw_output_panel(
        app,
        (Rectangle){ panel.x, panel.y + 250, panel.width, panel.height - 260 },
        "最近操作输出",
        state->output,
        "暂无记录，请通过快捷操作开始工作。"
    );
}

/* ── Page 1: 待诊列表 ── */

static void draw_pending_list(DesktopApp *app, Rectangle panel) {
    DesktopDoctorPageState *state = &app->state.doctor_page;
    Rectangle left = { panel.x, panel.y, panel.width * 0.52f, panel.height };
    Rectangle right = { panel.x + panel.width * 0.56f, panel.y,
                        panel.width * 0.44f, panel.height };
    Rectangle doctor_box = { left.x + 16, left.y + 60, 220, 30 };
    Rectangle input_fields[] = { doctor_box };
    Result result;

    auto_bind_doctor_id(app, state);
    release_focus_on_click(&state->active_field, input_fields, 1);

    DrawRectangleRounded(left, 0.12f, 8, app->theme.panel);
    DrawRectangleRoundedLinesEx(left, 0.12f, 8, 1.0f,
                                Fade(app->theme.border, 0.85f));
    DrawText("待诊列表", (int)left.x + 16, (int)left.y + 16, 24,
             app->theme.text_primary);

    GuiLabel((Rectangle){ left.x + 16, left.y + 42, 120, 20 }, "医生编号");
    draw_text_input(doctor_box, state->doctor_id, sizeof(state->doctor_id),
                    app->state.current_user.role != USER_ROLE_DOCTOR,
                    &state->active_field, 1);

    if (GuiButton((Rectangle){ left.x + 246, left.y + 60, 110, 30 },
                  "查询待诊")) {
        result = DesktopAdapters_query_pending_registrations_by_doctor(
            &app->application, state->doctor_id,
            state->output, sizeof(state->output));
        show_result(app, result);
    }

    draw_output_panel(app, right, "待诊患者", state->output,
                      "点击查询待诊查看当前待诊列表。");
}

/* ── Page 2: 患者历史 ── */

static void draw_patient_history(DesktopApp *app, Rectangle panel) {
    DesktopDoctorPageState *state = &app->state.doctor_page;
    Rectangle left = { panel.x, panel.y, panel.width * 0.52f, panel.height };
    Rectangle right = { panel.x + panel.width * 0.56f, panel.y,
                        panel.width * 0.44f, panel.height };
    Rectangle patient_box = { left.x + 16, left.y + 60, 220, 30 };
    Rectangle input_fields[] = { patient_box };
    Result result;

    release_focus_on_click(&state->active_field, input_fields, 1);

    DrawRectangleRounded(left, 0.12f, 8, app->theme.panel);
    DrawRectangleRoundedLinesEx(left, 0.12f, 8, 1.0f,
                                Fade(app->theme.border, 0.85f));
    DrawText("患者历史", (int)left.x + 16, (int)left.y + 16, 24,
             app->theme.text_primary);

    GuiLabel((Rectangle){ left.x + 16, left.y + 42, 120, 20 }, "患者编号");
    draw_text_input(patient_box, state->patient_id,
                    sizeof(state->patient_id), 1, &state->active_field, 1);

    if (GuiButton((Rectangle){ left.x + 246, left.y + 60, 110, 30 },
                  "查询历史")) {
        result = DesktopAdapters_query_patient_history(
            &app->application, state->patient_id,
            state->output, sizeof(state->output));
        show_result(app, result);
    }

    draw_output_panel(app, right, "患者看诊历史", state->output,
                      "输入患者编号后点击查询历史。");
}

/* ── Page 3: 看诊记录 ── */

static void draw_visit_record(DesktopApp *app, Rectangle panel) {
    DesktopDoctorPageState *state = &app->state.doctor_page;
    Rectangle left = { panel.x, panel.y, panel.width * 0.52f, panel.height };
    Rectangle right = { panel.x + panel.width * 0.56f, panel.y,
                        panel.width * 0.44f, panel.height };
    Rectangle reg_box     = { left.x + 16, left.y + 60, 220, 28 };
    Rectangle chief_box   = { left.x + 16, left.y + 118, 340, 28 };
    Rectangle diag_box    = { left.x + 16, left.y + 176, 340, 28 };
    Rectangle advice_box  = { left.x + 16, left.y + 234, 340, 28 };
    Rectangle vtime_box   = { left.x + 16, left.y + 340, 200, 28 };
    Rectangle input_fields[] = { reg_box, chief_box, diag_box, advice_box, vtime_box };
    bool need_exam = state->need_exam != 0;
    bool need_admission = state->need_admission != 0;
    bool need_medicine = state->need_medicine != 0;
    Result result;

    release_focus_on_click(&state->active_field, input_fields, 5);

    DrawRectangleRounded(left, 0.12f, 8, app->theme.panel);
    DrawRectangleRoundedLinesEx(left, 0.12f, 8, 1.0f,
                                Fade(app->theme.border, 0.85f));
    DrawText("看诊记录", (int)left.x + 16, (int)left.y + 16, 24,
             app->theme.text_primary);

    GuiLabel((Rectangle){ left.x + 16, left.y + 42, 120, 18 }, "挂号编号");
    draw_text_input(reg_box, state->registration_id,
                    sizeof(state->registration_id), 1,
                    &state->active_field, 1);

    GuiLabel((Rectangle){ left.x + 16, left.y + 100, 100, 18 }, "主诉");
    draw_text_input(chief_box, state->chief_complaint,
                    sizeof(state->chief_complaint), 1,
                    &state->active_field, 2);

    GuiLabel((Rectangle){ left.x + 16, left.y + 158, 100, 18 }, "诊断");
    draw_text_input(diag_box, state->diagnosis,
                    sizeof(state->diagnosis), 1,
                    &state->active_field, 3);

    GuiLabel((Rectangle){ left.x + 16, left.y + 216, 100, 18 }, "建议");
    draw_text_input(advice_box, state->advice,
                    sizeof(state->advice), 1,
                    &state->active_field, 4);

    GuiCheckBox((Rectangle){ left.x + 16, left.y + 278, 20, 20 },
                "需要检查", &need_exam);
    GuiCheckBox((Rectangle){ left.x + 130, left.y + 278, 20, 20 },
                "需要住院", &need_admission);
    GuiCheckBox((Rectangle){ left.x + 244, left.y + 278, 20, 20 },
                "需要用药", &need_medicine);
    state->need_exam = need_exam ? 1 : 0;
    state->need_admission = need_admission ? 1 : 0;
    state->need_medicine = need_medicine ? 1 : 0;

    GuiLabel((Rectangle){ left.x + 16, left.y + 322, 120, 18 }, "就诊时间");
    draw_text_input(vtime_box, state->visit_time,
                    sizeof(state->visit_time), 1,
                    &state->active_field, 5);

    if (GuiButton((Rectangle){ left.x + 228, left.y + 340, 128, 30 },
                  "提交诊疗")) {
        result = DesktopAdapters_create_visit_record(
            &app->application, state->registration_id,
            state->chief_complaint, state->diagnosis, state->advice,
            state->need_exam, state->need_admission, state->need_medicine,
            state->visit_time, state->output, sizeof(state->output));
        show_result(app, result);
    }

    draw_output_panel(app, right, "诊疗结果", state->output,
                      "填写诊疗信息后点击提交诊疗。");
}

/* ── Page 4: 检查管理 ── */

static void draw_examination(DesktopApp *app, Rectangle panel) {
    DesktopDoctorPageState *state = &app->state.doctor_page;
    Rectangle left = { panel.x, panel.y, panel.width * 0.52f, panel.height };
    Rectangle right = { panel.x + panel.width * 0.56f, panel.y,
                        panel.width * 0.44f, panel.height };
    /* Create examination fields */
    Rectangle vid_box   = { left.x + 16, left.y + 60, 160, 28 };
    Rectangle item_box  = { left.x + 186, left.y + 60, 170, 28 };
    Rectangle type_box  = { left.x + 16, left.y + 94, 160, 28 };
    Rectangle req_box   = { left.x + 186, left.y + 94, 170, 28 };
    /* Complete examination fields */
    Rectangle eid_box   = { left.x + 16, left.y + 210, 160, 28 };
    Rectangle res_box   = { left.x + 186, left.y + 210, 170, 28 };
    Rectangle comp_box  = { left.x + 16, left.y + 244, 200, 28 };
    Rectangle input_fields[] = {
        vid_box, item_box, type_box, req_box,
        eid_box, res_box, comp_box
    };
    Result result;

    release_focus_on_click(&state->active_field, input_fields, 7);

    DrawRectangleRounded(left, 0.12f, 8, app->theme.panel);
    DrawRectangleRoundedLinesEx(left, 0.12f, 8, 1.0f,
                                Fade(app->theme.border, 0.85f));
    DrawText("检查管理", (int)left.x + 16, (int)left.y + 16, 24,
             app->theme.text_primary);

    /* Create section */
    DrawText("新增检查", (int)left.x + 16, (int)left.y + 42, 18,
             app->theme.text_secondary);
    draw_text_input(vid_box, state->visit_id,
                    sizeof(state->visit_id), 1, &state->active_field, 1);
    draw_text_input(item_box, state->exam_item,
                    sizeof(state->exam_item), 1, &state->active_field, 2);
    draw_text_input(type_box, state->exam_type,
                    sizeof(state->exam_type), 1, &state->active_field, 3);
    draw_text_input(req_box, state->requested_at,
                    sizeof(state->requested_at), 1, &state->active_field, 4);

    if (GuiButton((Rectangle){ left.x + 16, left.y + 130, 160, 30 },
                  "新增检查")) {
        result = DesktopAdapters_create_examination_record(
            &app->application, state->visit_id, state->exam_item,
            state->exam_type, state->requested_at,
            state->output, sizeof(state->output));
        show_result(app, result);
    }

    /* Complete section */
    DrawText("回写检查结果", (int)left.x + 16, (int)left.y + 186, 18,
             app->theme.text_secondary);
    draw_text_input(eid_box, state->examination_id,
                    sizeof(state->examination_id), 1,
                    &state->active_field, 5);
    draw_text_input(res_box, state->exam_result,
                    sizeof(state->exam_result), 1,
                    &state->active_field, 6);
    draw_text_input(comp_box, state->completed_at,
                    sizeof(state->completed_at), 1,
                    &state->active_field, 7);

    if (GuiButton((Rectangle){ left.x + 228, left.y + 244, 128, 30 },
                  "回写结果")) {
        result = DesktopAdapters_complete_examination_record(
            &app->application, state->examination_id,
            state->exam_result, state->completed_at,
            state->output, sizeof(state->output));
        show_result(app, result);
    }

    draw_output_panel(app, right, "检查操作输出", state->output,
                      "新增检查或回写结果后，这里显示返回信息。");
}

/* ── Entry point ── */

void DoctorWorkbench_draw(DesktopApp *app, Rectangle panel, int page) {
    switch (page) {
        case 0: draw_home(app, panel); break;
        case 1: draw_pending_list(app, panel); break;
        case 2: draw_patient_history(app, panel); break;
        case 3: draw_visit_record(app, panel); break;
        case 4: draw_examination(app, panel); break;
        default: draw_home(app, panel); break;
    }
}
