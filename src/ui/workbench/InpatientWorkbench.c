#include "ui/Workbench.h"
#include "ui/DesktopApp.h"
#include "ui/DesktopAdapters.h"
#include "raygui.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

/* ── Local helpers ── */

static void show_result(DesktopApp *app, Result result) {
    DesktopAppState_show_message(
        &app->state,
        result.success ? DESKTOP_MESSAGE_SUCCESS : DESKTOP_MESSAGE_ERROR,
        result.message
    );
}

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

static void draw_output_panel(const DesktopApp *app, Rectangle rect,
                              const char *title, const char *content,
                              const char *empty_text) {
    DrawRectangleRounded(rect, 0.12f, 8, app->theme.panel);
    DrawRectangleRoundedLinesEx(rect, 0.12f, 8, 1.0f, Fade(app->theme.border, 0.85f));
    DrawText(title, (int)rect.x + 14, (int)rect.y + 12, 20, app->theme.text_primary);
    if (content == 0 || content[0] == '\0') {
        DrawText(empty_text, (int)rect.x + 14, (int)rect.y + 48, 17, app->theme.text_secondary);
        return;
    }
    DrawText(content, (int)rect.x + 14, (int)rect.y + 48, 17, app->theme.text_secondary);
}

/* ── Page 0: Home ── */

static void draw_home(DesktopApp *app, Rectangle panel) {
    const WorkbenchDef *wb = Workbench_get(USER_ROLE_INPATIENT_REGISTRAR);
    const char *labels[4] = { "今日入院", "今日出院", "待办理入院", "待办理出院" };
    const char *values[4] = { "--", "--", "--", "--" };
    WorkbenchButtonGroupLayout actions;
    int index = 0;

    Workbench_draw_home_cards(app, panel, labels, values, wb->accent);

    Workbench_draw_section_header(app, (int)panel.x, (int)panel.y + 110, "快捷操作");
    actions = Workbench_compute_button_group_layout(
        (Rectangle){ panel.x, panel.y + 148.0f, panel.width, 40.0f },
        3,
        160.0f,
        40.0f,
        14.0f
    );
    for (index = 0; index < actions.count; index++) {
        int clicked = 0;
        const char *label = index == 0 ? "入院登记" : index == 1 ? "出院办理" : "住院查询";
        Workbench_draw_quick_action_btn(app, actions.buttons[index], label, wb->accent, &clicked);
        if (clicked) {
            app->state.workbench_page = index + 1;
        }
    }
}

/* ── Page 1: Admit ── */

static void draw_admit(DesktopApp *app, Rectangle panel) {
    DesktopInpatientPageState *st = &app->state.inpatient_page;
    Rectangle left = { panel.x, panel.y, panel.width * 0.52f, panel.height };
    Rectangle right = { panel.x + panel.width * 0.56f, panel.y, panel.width * 0.44f, panel.height };
    float lx = left.x + 16;
    float ly = left.y;
    Result result;

    DrawRectangleRounded(left, 0.12f, 8, app->theme.panel);
    DrawRectangleRoundedLinesEx(left, 0.12f, 8, 1.0f, Fade(app->theme.border, 0.85f));
    DrawText("入院登记", (int)lx, (int)ly + 14, 24, app->theme.text_primary);

    GuiLabel((Rectangle){ lx, ly + 52, 120, 20 }, "患者编号");
    draw_text_input((Rectangle){ lx, ly + 74, 200, 28 }, st->patient_id, sizeof(st->patient_id), 1, &st->active_field, 1);

    GuiLabel((Rectangle){ lx, ly + 112, 120, 20 }, "病区编号");
    draw_text_input((Rectangle){ lx, ly + 134, 200, 28 }, st->ward_id, sizeof(st->ward_id), 1, &st->active_field, 2);

    GuiLabel((Rectangle){ lx, ly + 172, 120, 20 }, "床位编号");
    draw_text_input((Rectangle){ lx, ly + 194, 200, 28 }, st->bed_id, sizeof(st->bed_id), 1, &st->active_field, 3);

    GuiLabel((Rectangle){ lx, ly + 232, 120, 20 }, "入院时间");
    draw_text_input((Rectangle){ lx, ly + 254, 200, 28 }, st->admitted_at, sizeof(st->admitted_at), 1, &st->active_field, 4);

    GuiLabel((Rectangle){ lx, ly + 292, 120, 20 }, "入院摘要");
    draw_text_input((Rectangle){ lx, ly + 314, 340, 28 }, st->summary, sizeof(st->summary), 1, &st->active_field, 5);

    if (GuiButton((Rectangle){ lx, ly + 360, 160, 34 }, "办理入院")) {
        result = DesktopAdapters_admit_patient(
            &app->application,
            st->patient_id, st->ward_id, st->bed_id,
            st->admitted_at, st->summary,
            st->output, sizeof(st->output)
        );
        show_result(app, result);
    }

    draw_output_panel(app, right, "入院结果", st->output,
                      "填写左侧表单并点击「办理入院」。");
}

/* ── Page 2: Discharge ── */

static void draw_discharge(DesktopApp *app, Rectangle panel) {
    DesktopInpatientPageState *st = &app->state.inpatient_page;
    Rectangle left = { panel.x, panel.y, panel.width * 0.52f, panel.height };
    Rectangle right = { panel.x + panel.width * 0.56f, panel.y, panel.width * 0.44f, panel.height };
    float lx = left.x + 16;
    float ly = left.y;
    Result result;

    DrawRectangleRounded(left, 0.12f, 8, app->theme.panel);
    DrawRectangleRoundedLinesEx(left, 0.12f, 8, 1.0f, Fade(app->theme.border, 0.85f));
    DrawText("出院办理", (int)lx, (int)ly + 14, 24, app->theme.text_primary);

    GuiLabel((Rectangle){ lx, ly + 52, 120, 20 }, "住院编号");
    draw_text_input((Rectangle){ lx, ly + 74, 200, 28 }, st->admission_id, sizeof(st->admission_id), 1, &st->active_field, 1);

    GuiLabel((Rectangle){ lx, ly + 112, 120, 20 }, "出院时间");
    draw_text_input((Rectangle){ lx, ly + 134, 200, 28 }, st->discharged_at, sizeof(st->discharged_at), 1, &st->active_field, 2);

    GuiLabel((Rectangle){ lx, ly + 172, 120, 20 }, "出院摘要");
    draw_text_input((Rectangle){ lx, ly + 194, 340, 28 }, st->summary, sizeof(st->summary), 1, &st->active_field, 3);

    if (GuiButton((Rectangle){ lx, ly + 240, 160, 34 }, "办理出院")) {
        result = DesktopAdapters_discharge_patient(
            &app->application,
            st->admission_id, st->discharged_at, st->summary,
            st->output, sizeof(st->output)
        );
        show_result(app, result);
    }

    draw_output_panel(app, right, "出院结果", st->output,
                      "填写左侧表单并点击「办理出院」。");
}

/* ── Page 3: Query ── */

static void draw_query(DesktopApp *app, Rectangle panel) {
    DesktopInpatientPageState *st = &app->state.inpatient_page;
    Rectangle left = { panel.x, panel.y, panel.width * 0.52f, panel.height };
    Rectangle right = { panel.x + panel.width * 0.56f, panel.y, panel.width * 0.44f, panel.height };
    float lx = left.x + 16;
    float ly = left.y;
    Result result;

    DrawRectangleRounded(left, 0.12f, 8, app->theme.panel);
    DrawRectangleRoundedLinesEx(left, 0.12f, 8, 1.0f, Fade(app->theme.border, 0.85f));
    DrawText("住院查询", (int)lx, (int)ly + 14, 24, app->theme.text_primary);

    Workbench_draw_section_header(app, (int)lx, (int)ly + 52, "按患者查询");
    GuiLabel((Rectangle){ lx, ly + 84, 120, 20 }, "患者编号");
    draw_text_input((Rectangle){ lx, ly + 106, 200, 28 }, st->query_patient_id, sizeof(st->query_patient_id), 1, &st->active_field, 1);
    if (GuiButton((Rectangle){ lx + 216, ly + 106, 140, 30 }, "查当前住院")) {
        result = DesktopAdapters_query_active_admission_by_patient(
            &app->application, st->query_patient_id,
            st->output, sizeof(st->output)
        );
        show_result(app, result);
    }

    Workbench_draw_section_header(app, (int)lx, (int)ly + 162, "按床位查询");
    GuiLabel((Rectangle){ lx, ly + 194, 120, 20 }, "床位编号");
    draw_text_input((Rectangle){ lx, ly + 216, 200, 28 }, st->query_bed_id, sizeof(st->query_bed_id), 1, &st->active_field, 2);
    if (GuiButton((Rectangle){ lx + 216, ly + 216, 140, 30 }, "查床位患者")) {
        result = DesktopAdapters_query_current_patient_by_bed(
            &app->application, st->query_bed_id,
            st->output, sizeof(st->output)
        );
        show_result(app, result);
    }

    draw_output_panel(app, right, "查询结果", st->output,
                      "在左侧输入患者编号或床位编号进行查询。");
}

/* ── Entry point ── */

void InpatientWorkbench_draw(DesktopApp *app, Rectangle panel, int page) {
    switch (page) {
        case 0: draw_home(app, panel); break;
        case 1: draw_admit(app, panel); break;
        case 2: draw_discharge(app, panel); break;
        case 3: draw_query(app, panel); break;
        default: draw_home(app, panel); break;
    }
}
