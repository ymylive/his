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

static void auto_bind_patient_id(DesktopApp *app, char *pid, size_t cap) {
    if (app->state.current_user.role == USER_ROLE_PATIENT && pid[0] == '\0') {
        strncpy(pid, app->state.current_user.user_id, cap - 1);
        pid[cap - 1] = '\0';
    }
}

/* ── Page 0: 服务首页 ── */

static void draw_home(DesktopApp *app, Rectangle panel) {
    const WorkbenchDef *wb = Workbench_get(USER_ROLE_PATIENT);
    const char *labels[4] = { "下一次挂号", "最近看诊", "检查状态", "住院状态" };
    const char *values[4] = { "--", "--", "--", "--" };
    int clicked = 0;
    float y = panel.y + 110;

    auto_bind_patient_id(app, app->state.doctor_page.patient_id,
                         sizeof(app->state.doctor_page.patient_id));
    auto_bind_patient_id(app, app->state.dispense_page.patient_id,
                         sizeof(app->state.dispense_page.patient_id));

    Workbench_draw_home_cards(app, panel, labels, values, wb->accent);

    Workbench_draw_section_header(app, (int)panel.x, (int)panel.y + 100,
                                  "快捷操作");

    Workbench_draw_quick_action_btn(
        app, (Rectangle){ panel.x, y + 10, 140, 38 },
        "我的挂号", wb->accent, &clicked);
    if (clicked) app->state.workbench_page = 1;

    Workbench_draw_quick_action_btn(
        app, (Rectangle){ panel.x + 158, y + 10, 140, 38 },
        "我的看诊", wb->accent, &clicked);
    if (clicked) app->state.workbench_page = 2;

    Workbench_draw_quick_action_btn(
        app, (Rectangle){ panel.x + 316, y + 10, 140, 38 },
        "我的住院", wb->accent, &clicked);
    if (clicked) app->state.workbench_page = 3;

    Workbench_draw_quick_action_btn(
        app, (Rectangle){ panel.x + 474, y + 10, 140, 38 },
        "我的发药", wb->accent, &clicked);
    if (clicked) app->state.workbench_page = 4;

    Workbench_draw_section_header(app, (int)panel.x, (int)panel.y + 180,
                                  "个人信息");
    Workbench_draw_info_row(app, (int)panel.x, (int)panel.y + 216,
                            "患者编号",
                            app->state.current_user.user_id);
}

/* ── Page 1: 我的挂号 ── */

static void draw_my_registrations(DesktopApp *app, Rectangle panel) {
    DesktopDoctorPageState *state = &app->state.doctor_page;
    Result result;

    auto_bind_patient_id(app, state->patient_id,
                         sizeof(state->patient_id));

    DrawText("我的挂号", (int)panel.x, (int)panel.y + 4, 24,
             app->theme.text_primary);
    Workbench_draw_info_row(app, (int)panel.x, (int)panel.y + 38,
                            "患者编号", state->patient_id);

    if (GuiButton((Rectangle){ panel.x + panel.width - 120, panel.y + 32,
                               110, 30 }, "刷新")) {
        result = DesktopAdapters_query_registrations_by_patient(
            &app->application, state->patient_id,
            state->output, sizeof(state->output));
        show_result(app, result);
    }

    /* Auto-load on first visit */
    if (state->output[0] == '\0' && state->patient_id[0] != '\0') {
        result = DesktopAdapters_query_registrations_by_patient(
            &app->application, state->patient_id,
            state->output, sizeof(state->output));
        if (!result.success) show_result(app, result);
    }

    draw_output_panel(
        app,
        (Rectangle){ panel.x, panel.y + 76, panel.width, panel.height - 86 },
        "挂号记录",
        state->output,
        "暂无挂号记录。"
    );
}

/* ── Page 2: 我的看诊 ── */

static void draw_my_visits(DesktopApp *app, Rectangle panel) {
    DesktopDoctorPageState *state = &app->state.doctor_page;
    Result result;

    auto_bind_patient_id(app, state->patient_id,
                         sizeof(state->patient_id));

    DrawText("我的看诊", (int)panel.x, (int)panel.y + 4, 24,
             app->theme.text_primary);
    Workbench_draw_info_row(app, (int)panel.x, (int)panel.y + 38,
                            "患者编号", state->patient_id);

    if (GuiButton((Rectangle){ panel.x + panel.width - 120, panel.y + 32,
                               110, 30 }, "刷新")) {
        result = DesktopAdapters_query_patient_history(
            &app->application, state->patient_id,
            state->output, sizeof(state->output));
        show_result(app, result);
    }

    if (state->output[0] == '\0' && state->patient_id[0] != '\0') {
        result = DesktopAdapters_query_patient_history(
            &app->application, state->patient_id,
            state->output, sizeof(state->output));
        if (!result.success) show_result(app, result);
    }

    draw_output_panel(
        app,
        (Rectangle){ panel.x, panel.y + 76, panel.width, panel.height - 86 },
        "看诊记录",
        state->output,
        "暂无看诊记录。"
    );
}

/* ── Page 3: 我的住院 ── */

static void draw_my_admission(DesktopApp *app, Rectangle panel) {
    DesktopDoctorPageState *state = &app->state.doctor_page;
    Result result;

    auto_bind_patient_id(app, state->patient_id,
                         sizeof(state->patient_id));

    DrawText("我的住院", (int)panel.x, (int)panel.y + 4, 24,
             app->theme.text_primary);
    Workbench_draw_info_row(app, (int)panel.x, (int)panel.y + 38,
                            "患者编号", state->patient_id);

    if (GuiButton((Rectangle){ panel.x + panel.width - 120, panel.y + 32,
                               110, 30 }, "刷新")) {
        result = DesktopAdapters_query_active_admission_by_patient(
            &app->application, state->patient_id,
            state->output, sizeof(state->output));
        show_result(app, result);
    }

    if (state->output[0] == '\0' && state->patient_id[0] != '\0') {
        result = DesktopAdapters_query_active_admission_by_patient(
            &app->application, state->patient_id,
            state->output, sizeof(state->output));
        if (!result.success) show_result(app, result);
    }

    draw_output_panel(
        app,
        (Rectangle){ panel.x, panel.y + 76, panel.width, panel.height - 86 },
        "住院信息",
        state->output,
        "暂无住院记录。"
    );
}

/* ── Page 4: 我的发药 ── */

static void draw_my_dispense(DesktopApp *app, Rectangle panel) {
    DesktopDispensePageState *ds = &app->state.dispense_page;
    const LinkedListNode *current = 0;
    int index = 0;
    int should_query = 0;
    Result result;

    auto_bind_patient_id(app, ds->patient_id, sizeof(ds->patient_id));

    DrawText("我的发药", (int)panel.x, (int)panel.y + 4, 24,
             app->theme.text_primary);
    Workbench_draw_info_row(app, (int)panel.x, (int)panel.y + 38,
                            "患者编号", ds->patient_id);

    DrawRectangleRounded(
        (Rectangle){ panel.x, panel.y + 72, panel.width, panel.height - 82 },
        0.12f, 8, app->theme.panel);
    DrawRectangleRoundedLinesEx(
        (Rectangle){ panel.x, panel.y + 72, panel.width, panel.height - 82 },
        0.12f, 8, 1.0f, Fade(app->theme.border, 0.85f));

    should_query = GuiButton(
        (Rectangle){ panel.x + panel.width - 120, panel.y + 32, 110, 30 },
        "刷新");
    if (ds->loaded == 0) should_query = 1;

    if (should_query && ds->patient_id[0] != '\0') {
        LinkedList_clear(&ds->results, free);
        LinkedList_init(&ds->results);
        result = DesktopAdapters_load_dispense_history(
            &app->application, ds->patient_id, &ds->results);
        show_result(app, result);
        ds->loaded = result.success ? 1 : -1;
    }

    current = ds->results.head;
    while (current != 0 && index < 10) {
        const DispenseRecord *rec = (const DispenseRecord *)current->data;
        DrawText(
            TextFormat("%s | %s | %s | 数量=%d | %s",
                       rec->dispense_id, rec->prescription_id,
                       rec->medicine_id, rec->quantity, rec->dispensed_at),
            (int)panel.x + 16,
            (int)panel.y + 94 + index * 28,
            18, app->theme.text_secondary);
        current = current->next;
        index++;
    }

    if (index == 0) {
        DrawText("暂无发药记录。",
                 (int)panel.x + 16, (int)panel.y + 94, 17,
                 app->theme.text_secondary);
    }
}

/* ── Entry point ── */

void PatientWorkbench_draw(DesktopApp *app, Rectangle panel, int page) {
    switch (page) {
        case 0: draw_home(app, panel); break;
        case 1: draw_my_registrations(app, panel); break;
        case 2: draw_my_visits(app, panel); break;
        case 3: draw_my_admission(app, panel); break;
        case 4: draw_my_dispense(app, panel); break;
        default: draw_home(app, panel); break;
    }
}
