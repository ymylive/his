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
    WorkbenchListDetailLayout split = Workbench_compute_list_detail_layout(panel, 0.52f, 18.0f, 280.0f);
    float lx = split.list_bounds.x;
    float ly = split.list_bounds.y;
    Result result;

    DrawRectangleRounded(split.list_bounds, 0.12f, 8, app->theme.panel);
    DrawRectangleRoundedLinesEx(split.list_bounds, 0.12f, 8, 1.0f, Fade(app->theme.border, 0.85f));
    DrawText("入院登记", (int)lx + 16, (int)ly + 14, 24, app->theme.text_primary);

    Workbench_draw_section_header(app, (int)lx + 16, (int)ly + 52, "患者信息");

    Workbench_draw_form_label(app, (int)lx + 16, (int)ly + 86, "患者编号", 1);
    draw_text_input((Rectangle){ lx + 16, ly + 108, 200, 34 }, st->patient_id, sizeof(st->patient_id), 1, &st->active_field, 1);

    Workbench_draw_section_header(app, (int)lx + 16, (int)ly + 158, "床位分配");

    Workbench_draw_form_label(app, (int)lx + 16, (int)ly + 192, "病区编号", 1);
    draw_text_input((Rectangle){ lx + 16, ly + 214, 200, 34 }, st->ward_id, sizeof(st->ward_id), 1, &st->active_field, 2);

    Workbench_draw_form_label(app, (int)lx + 232, (int)ly + 192, "床位编号", 1);
    draw_text_input((Rectangle){ lx + 232, ly + 214, 200, 34 }, st->bed_id, sizeof(st->bed_id), 1, &st->active_field, 3);

    Workbench_draw_section_header(app, (int)lx + 16, (int)ly + 264, "入院详情");

    Workbench_draw_form_label(app, (int)lx + 16, (int)ly + 298, "入院时间", 1);
    draw_text_input((Rectangle){ lx + 16, ly + 320, 200, 34 }, st->admitted_at, sizeof(st->admitted_at), 1, &st->active_field, 4);

    Workbench_draw_form_label(app, (int)lx + 16, (int)ly + 364, "入院摘要", 0);
    draw_text_input((Rectangle){ lx + 16, ly + 386, split.list_bounds.width - 32, 34 }, st->summary, sizeof(st->summary), 1, &st->active_field, 5);

    if (GuiButton((Rectangle){ lx + 16, ly + 440, 160, 36 }, "办理入院")) {
        result = DesktopAdapters_admit_patient(
            &app->application,
            st->patient_id, st->ward_id, st->bed_id,
            st->admitted_at, st->summary,
            st->output, sizeof(st->output)
        );
        show_result(app, result);
    }

    draw_output_panel(app, split.detail_bounds, "入院结果", st->output,
                      "填写左侧表单并点击「办理入院」。");
}

/* ── Page 2: Discharge ── */

static void draw_discharge(DesktopApp *app, Rectangle panel) {
    DesktopInpatientPageState *st = &app->state.inpatient_page;
    WorkbenchListDetailLayout split = Workbench_compute_list_detail_layout(panel, 0.52f, 18.0f, 280.0f);
    float lx = split.list_bounds.x;
    float ly = split.list_bounds.y;
    Result result;

    DrawRectangleRounded(split.list_bounds, 0.12f, 8, app->theme.panel);
    DrawRectangleRoundedLinesEx(split.list_bounds, 0.12f, 8, 1.0f, Fade(app->theme.border, 0.85f));
    DrawText("出院办理", (int)lx + 16, (int)ly + 14, 24, app->theme.text_primary);

    Workbench_draw_section_header(app, (int)lx + 16, (int)ly + 52, "出院流程");

    DrawText("步骤 1: 输入住院编号", (int)lx + 16, (int)ly + 86, 17, app->theme.text_secondary);
    DrawText("步骤 2: 填写出院时间", (int)lx + 16, (int)ly + 106, 17, app->theme.text_secondary);
    DrawText("步骤 3: 填写出院摘要", (int)lx + 16, (int)ly + 126, 17, app->theme.text_secondary);
    DrawText("步骤 4: 确认办理出院", (int)lx + 16, (int)ly + 146, 17, app->theme.text_secondary);

    Workbench_draw_section_header(app, (int)lx + 16, (int)ly + 182, "出院信息");

    Workbench_draw_form_label(app, (int)lx + 16, (int)ly + 216, "住院编号", 1);
    draw_text_input((Rectangle){ lx + 16, ly + 238, 200, 34 }, st->admission_id, sizeof(st->admission_id), 1, &st->active_field, 1);

    Workbench_draw_form_label(app, (int)lx + 16, (int)ly + 282, "出院时间", 1);
    draw_text_input((Rectangle){ lx + 16, ly + 304, 200, 34 }, st->discharged_at, sizeof(st->discharged_at), 1, &st->active_field, 2);

    Workbench_draw_form_label(app, (int)lx + 16, (int)ly + 348, "出院摘要", 0);
    draw_text_input((Rectangle){ lx + 16, ly + 370, split.list_bounds.width - 32, 34 }, st->summary, sizeof(st->summary), 1, &st->active_field, 3);

    if (GuiButton((Rectangle){ lx + 16, ly + 424, 160, 36 }, "办理出院")) {
        result = DesktopAdapters_discharge_patient(
            &app->application,
            st->admission_id, st->discharged_at, st->summary,
            st->output, sizeof(st->output)
        );
        show_result(app, result);
    }

    draw_output_panel(app, split.detail_bounds, "出院结果", st->output,
                      "填写左侧表单并点击「办理出院」。");
}

/* ── Page 3: Query ── */

static void draw_query(DesktopApp *app, Rectangle panel) {
    DesktopInpatientPageState *st = &app->state.inpatient_page;
    WorkbenchListDetailLayout split = Workbench_compute_list_detail_layout(panel, 0.52f, 18.0f, 280.0f);
    float lx = split.list_bounds.x;
    float ly = split.list_bounds.y;
    Result result;
    int search_clicked_patient = 0;
    int search_clicked_bed = 0;

    DrawRectangleRounded(split.list_bounds, 0.12f, 8, app->theme.panel);
    DrawRectangleRoundedLinesEx(split.list_bounds, 0.12f, 8, 1.0f, Fade(app->theme.border, 0.85f));
    DrawText("住院查询", (int)lx + 16, (int)ly + 14, 24, app->theme.text_primary);

    Workbench_draw_section_header(app, (int)lx + 16, (int)ly + 52, "按患者查询");

    Workbench_draw_form_label(app, (int)lx + 16, (int)ly + 86, "患者编号", 1);
    Workbench_draw_search_box(
        app,
        (Rectangle){ lx + 16, ly + 108, split.list_bounds.width - 32, 34 },
        st->query_patient_id, sizeof(st->query_patient_id),
        &st->active_field, 1,
        &search_clicked_patient
    );

    if (search_clicked_patient) {
        result = DesktopAdapters_query_active_admission_by_patient(
            &app->application, st->query_patient_id,
            st->output, sizeof(st->output)
        );
        show_result(app, result);
    }

    Workbench_draw_section_header(app, (int)lx + 16, (int)ly + 168, "按床位查询");

    Workbench_draw_form_label(app, (int)lx + 16, (int)ly + 202, "床位编号", 1);
    Workbench_draw_search_box(
        app,
        (Rectangle){ lx + 16, ly + 224, split.list_bounds.width - 32, 34 },
        st->query_bed_id, sizeof(st->query_bed_id),
        &st->active_field, 2,
        &search_clicked_bed
    );

    if (search_clicked_bed) {
        result = DesktopAdapters_query_current_patient_by_bed(
            &app->application, st->query_bed_id,
            st->output, sizeof(st->output)
        );
        show_result(app, result);
    }

    Workbench_draw_section_header(app, (int)lx + 16, (int)ly + 284, "筛选选项");

    DrawText("住院状态:", (int)lx + 16, (int)ly + 318, 17, app->theme.text_secondary);
    Rectangle badge_active = { lx + 110, ly + 314, 70, 26 };
    Rectangle badge_discharged = { lx + 190, ly + 314, 70, 26 };
    Workbench_draw_status_badge(app, badge_active, "住院中", (Color){ 34, 197, 94, 255 });
    Workbench_draw_status_badge(app, badge_discharged, "已出院", (Color){ 156, 163, 175, 255 });

    DrawText("查询范围:", (int)lx + 16, (int)ly + 354, 17, app->theme.text_secondary);
    DrawText("• 当前住院记录", (int)lx + 16, (int)ly + 378, 16, app->theme.text_secondary);
    DrawText("• 床位占用情况", (int)lx + 16, (int)ly + 398, 16, app->theme.text_secondary);
    DrawText("• 病区分布统计", (int)lx + 16, (int)ly + 418, 16, app->theme.text_secondary);

    draw_output_panel(app, split.detail_bounds, "查询结果", st->output,
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
