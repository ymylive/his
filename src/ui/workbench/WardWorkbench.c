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
    const WorkbenchDef *wb = Workbench_get(USER_ROLE_WARD_MANAGER);
    const char *labels[4] = { "病房总数", "已占用床位", "可用床位", "待出院检查" };
    const char *values[4] = { "--", "--", "--", "--" };
    int clicked = 0;

    Workbench_draw_home_cards(app, panel, labels, values, wb->accent);

    Workbench_draw_section_header(app, (int)panel.x, (int)panel.y + 110, "快捷操作");

    Workbench_draw_quick_action_btn(
        app,
        (Rectangle){ panel.x, panel.y + 148, 140, 40 },
        "病房总览", wb->accent, &clicked
    );
    if (clicked) { app->state.workbench_page = 1; }

    Workbench_draw_quick_action_btn(
        app,
        (Rectangle){ panel.x + 158, panel.y + 148, 140, 40 },
        "床位状态", wb->accent, &clicked
    );
    if (clicked) { app->state.workbench_page = 2; }

    Workbench_draw_quick_action_btn(
        app,
        (Rectangle){ panel.x + 316, panel.y + 148, 140, 40 },
        "转床调度", wb->accent, &clicked
    );
    if (clicked) { app->state.workbench_page = 3; }

    Workbench_draw_quick_action_btn(
        app,
        (Rectangle){ panel.x + 474, panel.y + 148, 140, 40 },
        "出院检查", wb->accent, &clicked
    );
    if (clicked) { app->state.workbench_page = 4; }
}

/* ── Page 1: Ward List ── */

static void draw_ward_list(DesktopApp *app, Rectangle panel) {
    DesktopInpatientPageState *st = &app->state.inpatient_page;
    Result result;

    DrawRectangleRounded(panel, 0.12f, 8, app->theme.panel);
    DrawRectangleRoundedLinesEx(panel, 0.12f, 8, 1.0f, Fade(app->theme.border, 0.85f));
    DrawText("病房总览", (int)panel.x + 16, (int)panel.y + 14, 24, app->theme.text_primary);

    if (GuiButton((Rectangle){ panel.x + 16, panel.y + 52, 160, 34 }, "刷新病房列表")) {
        result = DesktopAdapters_list_wards(
            &app->application, st->output, sizeof(st->output)
        );
        show_result(app, result);
    }

    if (st->output[0] != '\0') {
        DrawText(st->output, (int)panel.x + 16, (int)panel.y + 100, 17, app->theme.text_secondary);
    } else {
        DrawText("点击「刷新病房列表」查看所有病房。", (int)panel.x + 16, (int)panel.y + 100, 17, app->theme.text_secondary);
    }
}

/* ── Page 2: Bed Status ── */

static void draw_bed_status(DesktopApp *app, Rectangle panel) {
    DesktopInpatientPageState *st = &app->state.inpatient_page;
    Rectangle left = { panel.x, panel.y, panel.width * 0.52f, panel.height };
    Rectangle right = { panel.x + panel.width * 0.56f, panel.y, panel.width * 0.44f, panel.height };
    float lx = left.x + 16;
    float ly = left.y;
    Result result;

    DrawRectangleRounded(left, 0.12f, 8, app->theme.panel);
    DrawRectangleRoundedLinesEx(left, 0.12f, 8, 1.0f, Fade(app->theme.border, 0.85f));
    DrawText("床位状态", (int)lx, (int)ly + 14, 24, app->theme.text_primary);

    GuiLabel((Rectangle){ lx, ly + 52, 120, 20 }, "病区编号");
    draw_text_input((Rectangle){ lx, ly + 74, 200, 28 }, st->ward_id, sizeof(st->ward_id), 1, &st->active_field, 1);

    if (GuiButton((Rectangle){ lx + 216, ly + 74, 140, 30 }, "查看床位")) {
        result = DesktopAdapters_list_beds_by_ward(
            &app->application, st->ward_id,
            st->output, sizeof(st->output)
        );
        show_result(app, result);
    }

    draw_output_panel(app, right, "床位列表", st->output,
                      "输入病区编号并点击「查看床位」。");
}

/* ── Page 3: Transfer ── */

static void draw_transfer(DesktopApp *app, Rectangle panel) {
    DesktopInpatientPageState *st = &app->state.inpatient_page;
    Rectangle left = { panel.x, panel.y, panel.width * 0.52f, panel.height };
    Rectangle right = { panel.x + panel.width * 0.56f, panel.y, panel.width * 0.44f, panel.height };
    float lx = left.x + 16;
    float ly = left.y;
    /* transfer needs: admission_id, target bed_id, transferred_at */
    static char transferred_at[64] = "";
    Result result;

    DrawRectangleRounded(left, 0.12f, 8, app->theme.panel);
    DrawRectangleRoundedLinesEx(left, 0.12f, 8, 1.0f, Fade(app->theme.border, 0.85f));
    DrawText("转床调度", (int)lx, (int)ly + 14, 24, app->theme.text_primary);

    GuiLabel((Rectangle){ lx, ly + 52, 120, 20 }, "住院编号");
    draw_text_input((Rectangle){ lx, ly + 74, 200, 28 }, st->admission_id, sizeof(st->admission_id), 1, &st->active_field, 1);

    GuiLabel((Rectangle){ lx, ly + 112, 120, 20 }, "目标床位");
    draw_text_input((Rectangle){ lx, ly + 134, 200, 28 }, st->bed_id, sizeof(st->bed_id), 1, &st->active_field, 2);

    GuiLabel((Rectangle){ lx, ly + 172, 120, 20 }, "转床时间");
    draw_text_input((Rectangle){ lx, ly + 194, 200, 28 }, transferred_at, sizeof(transferred_at), 1, &st->active_field, 3);

    if (GuiButton((Rectangle){ lx, ly + 240, 160, 34 }, "执行转床")) {
        result = DesktopAdapters_transfer_bed(
            &app->application,
            st->admission_id, st->bed_id, transferred_at,
            st->output, sizeof(st->output)
        );
        show_result(app, result);
    }

    draw_output_panel(app, right, "转床结果", st->output,
                      "填写左侧表单并点击「执行转床」。");
}

/* ── Page 4: Discharge Check ── */

static void draw_discharge_check(DesktopApp *app, Rectangle panel) {
    DesktopInpatientPageState *st = &app->state.inpatient_page;
    Rectangle left = { panel.x, panel.y, panel.width * 0.52f, panel.height };
    Rectangle right = { panel.x + panel.width * 0.56f, panel.y, panel.width * 0.44f, panel.height };
    float lx = left.x + 16;
    float ly = left.y;
    Result result;

    DrawRectangleRounded(left, 0.12f, 8, app->theme.panel);
    DrawRectangleRoundedLinesEx(left, 0.12f, 8, 1.0f, Fade(app->theme.border, 0.85f));
    DrawText("出院检查", (int)lx, (int)ly + 14, 24, app->theme.text_primary);

    GuiLabel((Rectangle){ lx, ly + 52, 120, 20 }, "住院编号");
    draw_text_input((Rectangle){ lx, ly + 74, 200, 28 }, st->admission_id, sizeof(st->admission_id), 1, &st->active_field, 1);

    if (GuiButton((Rectangle){ lx + 216, ly + 74, 140, 30 }, "执行检查")) {
        result = DesktopAdapters_discharge_check(
            &app->application, st->admission_id,
            st->output, sizeof(st->output)
        );
        show_result(app, result);
    }

    draw_output_panel(app, right, "检查结果", st->output,
                      "输入住院编号并点击「执行检查」。");
}

/* ── Entry point ── */

void WardWorkbench_draw(DesktopApp *app, Rectangle panel, int page) {
    switch (page) {
        case 0: draw_home(app, panel); break;
        case 1: draw_ward_list(app, panel); break;
        case 2: draw_bed_status(app, panel); break;
        case 3: draw_transfer(app, panel); break;
        case 4: draw_discharge_check(app, panel); break;
        default: draw_home(app, panel); break;
    }
}
