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
    WorkbenchButtonGroupLayout actions;
    int index = 0;

    Workbench_draw_home_cards(app, panel, labels, values, wb->accent);

    Workbench_draw_section_header(app, (int)panel.x, (int)panel.y + 110, "快捷操作");
    actions = Workbench_compute_button_group_layout(
        (Rectangle){ panel.x, panel.y + 148.0f, panel.width, 40.0f },
        4,
        140.0f,
        40.0f,
        14.0f
    );
    for (index = 0; index < actions.count; index++) {
        int clicked = 0;
        const char *label = index == 0 ? "病房总览" :
                            index == 1 ? "床位状态" :
                            index == 2 ? "转床调度" :
                                         "出院检查";
        Workbench_draw_quick_action_btn(app, actions.buttons[index], label, wb->accent, &clicked);
        if (clicked) {
            app->state.workbench_page = index + 1;
        }
    }

    draw_output_panel(
        app,
        (Rectangle){ panel.x, panel.y + 214.0f, panel.width, panel.height - 214.0f },
        "工作台说明",
        "病区管理工作台提供病房总览、床位状态查询、转床调度和出院检查功能。",
        "暂无说明"
    );
}

/* ── Page 1: Ward List ── */

static void draw_ward_list(DesktopApp *app, Rectangle panel) {
    DesktopInpatientPageState *st = &app->state.inpatient_page;
    WorkbenchListDetailLayout split = Workbench_compute_list_detail_layout(
        panel, 0.48f, 18.0f, 280.0f
    );
    Result result;
    const WorkbenchDef *wb = Workbench_get(USER_ROLE_WARD_MANAGER);

    /* Left panel: Ward list and controls */
    DrawRectangleRounded(split.list_bounds, 0.12f, 8, app->theme.panel);
    DrawRectangleRoundedLinesEx(split.list_bounds, 0.12f, 8, 1.0f, Fade(app->theme.border, 0.85f));
    DrawText("病房总览", (int)split.list_bounds.x + 16, (int)split.list_bounds.y + 14, 24, app->theme.text_primary);

    if (GuiButton((Rectangle){ split.list_bounds.x + 16, split.list_bounds.y + 56, 160, 36 }, "刷新病房列表")) {
        result = DesktopAdapters_list_wards(
            &app->application, st->output, sizeof(st->output)
        );
        show_result(app, result);
    }

    if (st->output[0] != '\0') {
        DrawText(st->output, (int)split.list_bounds.x + 16, (int)split.list_bounds.y + 108, 17, app->theme.text_secondary);
    } else {
        DrawText("点击「刷新病房列表」查看所有病房。", (int)split.list_bounds.x + 16, (int)split.list_bounds.y + 108, 17, app->theme.text_secondary);
    }

    /* Right panel: Bed grid visualization example */
    DrawRectangleRounded(split.detail_bounds, 0.12f, 8, app->theme.panel);
    DrawRectangleRoundedLinesEx(split.detail_bounds, 0.12f, 8, 1.0f, Fade(app->theme.border, 0.85f));
    DrawText("床位可视化示例", (int)split.detail_bounds.x + 16, (int)split.detail_bounds.y + 14, 20, app->theme.text_primary);

    /* Example bed grid - showing a sample ward with 12 beds, 7 occupied */
    {
        int example_occupied[7] = { 0, 2, 3, 5, 7, 9, 10 };
        Rectangle grid_area = {
            split.detail_bounds.x + 16,
            split.detail_bounds.y + 56,
            split.detail_bounds.width - 32,
            split.detail_bounds.height - 72
        };
        Workbench_draw_bed_grid(app, grid_area, "示例病区", 12, example_occupied, 7);
    }
}

/* ── Page 2: Bed Status ── */

static void draw_bed_status(DesktopApp *app, Rectangle panel) {
    DesktopInpatientPageState *st = &app->state.inpatient_page;
    WorkbenchListDetailLayout split = Workbench_compute_list_detail_layout(
        panel, 0.48f, 18.0f, 280.0f
    );
    float lx = split.list_bounds.x + 16;
    float ly = split.list_bounds.y;
    Result result;

    /* Left panel: Query form */
    DrawRectangleRounded(split.list_bounds, 0.12f, 8, app->theme.panel);
    DrawRectangleRoundedLinesEx(split.list_bounds, 0.12f, 8, 1.0f, Fade(app->theme.border, 0.85f));
    DrawText("床位状态查询", (int)lx, (int)ly + 14, 24, app->theme.text_primary);

    Workbench_draw_section_header(app, (int)lx, (int)ly + 56, "查询条件");

    Workbench_draw_form_label(app, (int)lx, (int)ly + 90, "病区编号", 1);
    draw_text_input((Rectangle){ lx, ly + 112, 240, 34 }, st->ward_id, sizeof(st->ward_id), 1, &st->active_field, 1);

    if (GuiButton((Rectangle){ lx, ly + 162, 160, 36 }, "查看床位")) {
        result = DesktopAdapters_list_beds_by_ward(
            &app->application, st->ward_id,
            st->output, sizeof(st->output)
        );
        show_result(app, result);
    }

    /* Status legend */
    Workbench_draw_section_header(app, (int)lx, (int)ly + 220, "状态说明");
    {
        Rectangle available_badge = { lx, ly + 254, 100, 28 };
        Rectangle occupied_badge = { lx + 116, ly + 254, 100, 28 };
        Workbench_draw_status_badge(app, available_badge, "可用", (Color){ 34, 197, 94, 255 });
        Workbench_draw_status_badge(app, occupied_badge, "占用", (Color){ 239, 68, 68, 255 });
    }

    DrawText("绿色表示床位可用，红色表示已占用", (int)lx, (int)ly + 296, 16, app->theme.text_secondary);

    /* Right panel: Results */
    DrawRectangleRounded(split.detail_bounds, 0.12f, 8, app->theme.panel);
    DrawRectangleRoundedLinesEx(split.detail_bounds, 0.12f, 8, 1.0f, Fade(app->theme.border, 0.85f));
    DrawText("床位列表", (int)split.detail_bounds.x + 16, (int)split.detail_bounds.y + 14, 20, app->theme.text_primary);

    if (st->output[0] != '\0') {
        DrawText(st->output, (int)split.detail_bounds.x + 16, (int)split.detail_bounds.y + 56, 17, app->theme.text_secondary);
    } else {
        DrawText("输入病区编号并点击「查看床位」。", (int)split.detail_bounds.x + 16, (int)split.detail_bounds.y + 56, 17, app->theme.text_secondary);
    }
}

/* ── Page 3: Transfer ── */

static void draw_transfer(DesktopApp *app, Rectangle panel) {
    DesktopInpatientPageState *st = &app->state.inpatient_page;
    WorkbenchListDetailLayout split = Workbench_compute_list_detail_layout(
        panel, 0.48f, 18.0f, 280.0f
    );
    float lx = split.list_bounds.x + 16;
    float ly = split.list_bounds.y;
    static char transferred_at[64] = "";
    Result result;

    /* Left panel: Transfer form */
    DrawRectangleRounded(split.list_bounds, 0.12f, 8, app->theme.panel);
    DrawRectangleRoundedLinesEx(split.list_bounds, 0.12f, 8, 1.0f, Fade(app->theme.border, 0.85f));
    DrawText("转床调度", (int)lx, (int)ly + 14, 24, app->theme.text_primary);

    Workbench_draw_section_header(app, (int)lx, (int)ly + 56, "转床信息");

    Workbench_draw_form_label(app, (int)lx, (int)ly + 90, "住院编号", 1);
    draw_text_input((Rectangle){ lx, ly + 112, 240, 34 }, st->admission_id, sizeof(st->admission_id), 1, &st->active_field, 1);

    Workbench_draw_form_label(app, (int)lx, (int)ly + 156, "目标床位", 1);
    draw_text_input((Rectangle){ lx, ly + 178, 240, 34 }, st->bed_id, sizeof(st->bed_id), 1, &st->active_field, 2);

    Workbench_draw_form_label(app, (int)lx, (int)ly + 222, "转床时间", 0);
    draw_text_input((Rectangle){ lx, ly + 244, 240, 34 }, transferred_at, sizeof(transferred_at), 1, &st->active_field, 3);

    if (GuiButton((Rectangle){ lx, ly + 294, 160, 36 }, "执行转床")) {
        result = DesktopAdapters_transfer_bed(
            &app->application,
            st->admission_id, st->bed_id, transferred_at,
            st->output, sizeof(st->output)
        );
        show_result(app, result);
    }

    /* Right panel: Instructions and result */
    DrawRectangleRounded(split.detail_bounds, 0.12f, 8, app->theme.panel);
    DrawRectangleRoundedLinesEx(split.detail_bounds, 0.12f, 8, 1.0f, Fade(app->theme.border, 0.85f));
    DrawText("操作指引", (int)split.detail_bounds.x + 16, (int)split.detail_bounds.y + 14, 20, app->theme.text_primary);

    {
        float guide_y = split.detail_bounds.y + 56;
        DrawText("转床操作步骤：", (int)split.detail_bounds.x + 16, (int)guide_y, 18, app->theme.text_primary);
        guide_y += 32;
        DrawText("1. 输入患者的住院编号", (int)split.detail_bounds.x + 16, (int)guide_y, 17, app->theme.text_secondary);
        guide_y += 28;
        DrawText("2. 输入目标床位编号", (int)split.detail_bounds.x + 16, (int)guide_y, 17, app->theme.text_secondary);
        guide_y += 28;
        DrawText("3. 填写转床时间（可选）", (int)split.detail_bounds.x + 16, (int)guide_y, 17, app->theme.text_secondary);
        guide_y += 28;
        DrawText("4. 点击「执行转床」完成操作", (int)split.detail_bounds.x + 16, (int)guide_y, 17, app->theme.text_secondary);
        guide_y += 48;

        if (st->output[0] != '\0') {
            Workbench_draw_section_header(app, (int)split.detail_bounds.x + 16, (int)guide_y, "转床结果");
            guide_y += 34;
            DrawText(st->output, (int)split.detail_bounds.x + 16, (int)guide_y, 17, app->theme.text_secondary);
        }
    }
}

/* ── Page 4: Discharge Check ── */

static void draw_discharge_check(DesktopApp *app, Rectangle panel) {
    DesktopInpatientPageState *st = &app->state.inpatient_page;
    WorkbenchListDetailLayout split = Workbench_compute_list_detail_layout(
        panel, 0.48f, 18.0f, 280.0f
    );
    float lx = split.list_bounds.x + 16;
    float ly = split.list_bounds.y;
    Result result;

    /* Left panel: Check form */
    DrawRectangleRounded(split.list_bounds, 0.12f, 8, app->theme.panel);
    DrawRectangleRoundedLinesEx(split.list_bounds, 0.12f, 8, 1.0f, Fade(app->theme.border, 0.85f));
    DrawText("出院检查", (int)lx, (int)ly + 14, 24, app->theme.text_primary);

    Workbench_draw_section_header(app, (int)lx, (int)ly + 56, "患者信息");

    Workbench_draw_form_label(app, (int)lx, (int)ly + 90, "住院编号", 1);
    draw_text_input((Rectangle){ lx, ly + 112, 240, 34 }, st->admission_id, sizeof(st->admission_id), 1, &st->active_field, 1);

    if (GuiButton((Rectangle){ lx, ly + 162, 160, 36 }, "执行检查")) {
        result = DesktopAdapters_discharge_check(
            &app->application, st->admission_id,
            st->output, sizeof(st->output)
        );
        show_result(app, result);
    }

    /* Right panel: Process guide and result */
    DrawRectangleRounded(split.detail_bounds, 0.12f, 8, app->theme.panel);
    DrawRectangleRoundedLinesEx(split.detail_bounds, 0.12f, 8, 1.0f, Fade(app->theme.border, 0.85f));
    DrawText("出院流程", (int)split.detail_bounds.x + 16, (int)split.detail_bounds.y + 14, 20, app->theme.text_primary);

    {
        float guide_y = split.detail_bounds.y + 56;
        DrawText("出院检查流程：", (int)split.detail_bounds.x + 16, (int)guide_y, 18, app->theme.text_primary);
        guide_y += 32;
        DrawText("1. 输入患者住院编号", (int)split.detail_bounds.x + 16, (int)guide_y, 17, app->theme.text_secondary);
        guide_y += 28;
        DrawText("2. 点击「执行检查」", (int)split.detail_bounds.x + 16, (int)guide_y, 17, app->theme.text_secondary);
        guide_y += 28;
        DrawText("3. 系统将验证以下项目：", (int)split.detail_bounds.x + 16, (int)guide_y, 17, app->theme.text_secondary);
        guide_y += 28;
        DrawText("   - 医疗费用是否结清", (int)split.detail_bounds.x + 28, (int)guide_y, 16, app->theme.text_secondary);
        guide_y += 24;
        DrawText("   - 医嘱是否全部完成", (int)split.detail_bounds.x + 28, (int)guide_y, 16, app->theme.text_secondary);
        guide_y += 24;
        DrawText("   - 床位是否可释放", (int)split.detail_bounds.x + 28, (int)guide_y, 16, app->theme.text_secondary);
        guide_y += 32;
        DrawText("4. 检查通过后可办理出院", (int)split.detail_bounds.x + 16, (int)guide_y, 17, app->theme.text_secondary);
        guide_y += 48;

        if (st->output[0] != '\0') {
            Workbench_draw_section_header(app, (int)split.detail_bounds.x + 16, (int)guide_y, "检查结果");
            guide_y += 34;
            DrawText(st->output, (int)split.detail_bounds.x + 16, (int)guide_y, 17, app->theme.text_secondary);
        }
    }
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
