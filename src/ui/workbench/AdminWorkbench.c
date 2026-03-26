#include "ui/Workbench.h"
#include "ui/DesktopApp.h"
#include "ui/DesktopAdapters.h"
#include "raygui.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

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

static void draw_empty_state(const DesktopApp *app, Rectangle rect,
                             const char *title, const char *desc) {
    DrawRectangleRounded(rect, 0.12f, 8, Fade(app->theme.panel_alt, 0.88f));
    DrawRectangleRoundedLinesEx(rect, 0.12f, 8, 1.0f, Fade(app->theme.border, 0.85f));
    DrawText(title, (int)rect.x + 18, (int)rect.y + 16, 20, app->theme.text_primary);
    DrawText(desc, (int)rect.x + 18, (int)rect.y + 46, 18, app->theme.text_secondary);
}

static void draw_output_panel(const DesktopApp *app, Rectangle rect,
                              const char *title, const char *content,
                              const char *empty_text) {
    DrawRectangleRounded(rect, 0.12f, 8, app->theme.panel);
    DrawRectangleRoundedLinesEx(rect, 0.12f, 8, 1.0f, Fade(app->theme.border, 0.85f));
    DrawText(title, (int)rect.x + 14, (int)rect.y + 12, 20, app->theme.text_primary);
    if (content == 0 || content[0] == '\0') {
        draw_empty_state(app,
            (Rectangle){ rect.x + 12, rect.y + 46, rect.width - 24, 84 },
            "暂无输出", empty_text);
        return;
    }
    DrawText(content, (int)rect.x + 14, (int)rect.y + 48, 17, app->theme.text_secondary);
}

static void release_focus(int *active_field, const Rectangle *fields, int count) {
    Vector2 mouse = GetMousePosition();
    if (active_field == 0 || fields == 0 || count <= 0) return;
    if (*active_field <= 0 || *active_field > count) return;
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) &&
        !CheckCollisionPointRec(mouse, fields[*active_field - 1])) {
        *active_field = 0;
    }
}

/* ── Page 0: Home overview ── */

static void admin_draw_home(DesktopApp *app, Rectangle panel) {
    const WorkbenchDef *wb = Workbench_get(app->state.current_user.role);
    const char *labels[4] = { "患者总数", "医生总数", "当前住院", "低库存药品" };
    char v0[16], v1[16], v2[16], v3[16];
    const char *values[4];
    float section_y = 0;
    int clicked = 0;
    Result result;
    const LinkedListNode *cur = 0;
    int i = 0;

    if (app->state.dashboard.loaded == 0) {
        result = DesktopAdapters_load_dashboard(&app->application, &app->state.dashboard);
        show_result(app, result);
        app->state.dashboard.loaded = result.success ? 1 : -1;
    }

    snprintf(v0, sizeof(v0), "%d", app->state.dashboard.patient_count);
    snprintf(v1, sizeof(v1), "%d", app->state.dashboard.registration_count);
    snprintf(v2, sizeof(v2), "%d", app->state.dashboard.inpatient_count);
    snprintf(v3, sizeof(v3), "%d", app->state.dashboard.low_stock_count);
    values[0] = v0; values[1] = v1; values[2] = v2; values[3] = v3;

    Workbench_draw_home_cards(app, panel, labels, values, wb->accent);

    section_y = panel.y + 98;

    /* Quick actions */
    Workbench_draw_section_header(app, (int)panel.x, (int)section_y, "快捷操作");
    section_y += 36;

    Workbench_draw_quick_action_btn(app,
        (Rectangle){ panel.x, section_y, 140, 36 },
        "刷新数据", wb->accent, &clicked);
    if (clicked) { app->state.dashboard.loaded = 0; }

    Workbench_draw_quick_action_btn(app,
        (Rectangle){ panel.x + 158, section_y, 140, 36 },
        "患者管理", wb->accent, &clicked);
    if (clicked) { app->state.workbench_page = 1; }

    Workbench_draw_quick_action_btn(app,
        (Rectangle){ panel.x + 316, section_y, 140, 36 },
        "系统信息", wb->accent, &clicked);
    if (clicked) { app->state.workbench_page = 4; }

    /* Recent registrations */
    section_y += 54;
    Workbench_draw_section_header(app, (int)panel.x, (int)section_y, "最近挂号");
    section_y += 36;
    DrawRectangleRounded(
        (Rectangle){ panel.x, section_y, panel.width, 180 },
        0.12f, 8, app->theme.panel);

    cur = app->state.dashboard.recent_registrations.head;
    i = 0;
    while (cur != 0 && i < 5) {
        const Registration *r = (const Registration *)cur->data;
        DrawText(
            TextFormat("%s | %s | %s", r->registration_id, r->patient_id, r->registered_at),
            (int)panel.x + 16, (int)(section_y + 14 + i * 28), 17, app->theme.text_secondary);
        cur = cur->next;
        i++;
    }
    if (i == 0) {
        draw_empty_state(app,
            (Rectangle){ panel.x + 10, section_y + 10, panel.width - 20, 80 },
            "暂无挂号记录", "点击刷新数据重新获取。");
    }
}

/* ── Page 1: Patient master data ── */

static void admin_draw_patients(DesktopApp *app, Rectangle panel) {
    DesktopPatientPageState *ps = &app->state.patient_page;
    Rectangle search_box = { panel.x, panel.y, panel.width - 150, 36 };
    Rectangle search_btn = { panel.x + panel.width - 132, panel.y, 132, 36 };
    Rectangle list_rect = { panel.x, panel.y + 52, panel.width * 0.48f, panel.height - 52 };
    Rectangle detail_rect = { panel.x + panel.width * 0.52f, panel.y + 52, panel.width * 0.48f, panel.height - 52 };
    Rectangle input_fields[] = { search_box };
    Result result;
    const LinkedListNode *cur = 0;
    int idx = 0;

    release_focus(&ps->active_field, input_fields, 1);

    ps->search_mode = (DesktopPatientSearchMode)GuiToggleGroup(
        (Rectangle){ panel.x, panel.y + 40, 220, 28 }, "按编号;按姓名",
        (int *)&ps->search_mode);

    draw_text_input(search_box, ps->query, sizeof(ps->query), 1, &ps->active_field, 1);

    if (GuiButton(search_btn, "查询")) {
        LinkedList_clear(&ps->results, free);
        LinkedList_init(&ps->results);
        ps->selected_index = -1;
        ps->has_selected_patient = 0;
        result = DesktopAdapters_search_patients(
            &app->application, ps->search_mode, ps->query, &ps->results);
        show_result(app, result);
        if (result.success && ps->results.head != 0) {
            ps->selected_patient = *(const Patient *)ps->results.head->data;
            ps->selected_index = 0;
            ps->has_selected_patient = 1;
        }
    }

    DrawRectangleRounded(list_rect, 0.12f, 8, app->theme.panel);
    DrawRectangleRounded(detail_rect, 0.12f, 8, app->theme.panel);
    DrawRectangleRoundedLinesEx(list_rect, 0.12f, 8, 1.0f, Fade(app->theme.border, 0.85f));
    DrawRectangleRoundedLinesEx(detail_rect, 0.12f, 8, 1.0f, Fade(app->theme.border, 0.85f));
    DrawText("患者列表", (int)list_rect.x + 14, (int)list_rect.y + 12, 20, app->theme.text_primary);
    DrawText("详情面板", (int)detail_rect.x + 14, (int)detail_rect.y + 12, 20, app->theme.text_primary);

    cur = ps->results.head;
    while (cur != 0 && idx < 10) {
        const Patient *p = (const Patient *)cur->data;
        Rectangle row = { list_rect.x + 12, list_rect.y + 48 + idx * 40, list_rect.width - 24, 32 };
        if (ps->selected_index == idx) {
            DrawRectangleRounded(row, 0.16f, 8, Fade(app->theme.nav_active, 0.15f));
        }
        if (GuiButton(row, TextFormat("%s | %s", p->patient_id, p->name))) {
            ps->selected_index = idx;
            ps->selected_patient = *p;
            ps->has_selected_patient = 1;
        }
        cur = cur->next;
        idx++;
    }
    if (idx == 0) {
        draw_empty_state(app,
            (Rectangle){ list_rect.x + 12, list_rect.y + 48, list_rect.width - 24, 84 },
            "暂无患者结果", "请调整查询条件后重试。");
    }
    if (ps->has_selected_patient) {
        const Patient *p = &ps->selected_patient;
        Workbench_draw_info_row(app, (int)detail_rect.x + 16, (int)detail_rect.y + 54, "编号", p->patient_id);
        Workbench_draw_info_row(app, (int)detail_rect.x + 16, (int)detail_rect.y + 82, "姓名", p->name);
        Workbench_draw_info_row(app, (int)detail_rect.x + 16, (int)detail_rect.y + 110, "年龄", TextFormat("%d", p->age));
        Workbench_draw_info_row(app, (int)detail_rect.x + 16, (int)detail_rect.y + 138, "联系方式", p->contact);
        Workbench_draw_info_row(app, (int)detail_rect.x + 16, (int)detail_rect.y + 166, "过敏史", p->allergy);
        Workbench_draw_info_row(app, (int)detail_rect.x + 16, (int)detail_rect.y + 194, "病史", p->medical_history);
        Workbench_draw_info_row(app, (int)detail_rect.x + 16, (int)detail_rect.y + 222, "住院状态", p->is_inpatient ? "是" : "否");
    } else {
        draw_empty_state(app,
            (Rectangle){ detail_rect.x + 12, detail_rect.y + 48, detail_rect.width - 24, 84 },
            "等待选择患者", "点击左侧患者行后可查看详情信息。");
    }
}

/* ── Page 2: Doctors & departments ── */

static void admin_draw_doctors(DesktopApp *app, Rectangle panel) {
    const WorkbenchDef *wb = Workbench_get(app->state.current_user.role);

    DrawRectangleRounded(panel, 0.12f, 8, app->theme.panel);
    DrawText("医生与科室", (int)panel.x + 16, (int)panel.y + 16, 24, app->theme.text_primary);

    Workbench_draw_section_header(app, (int)panel.x + 16, (int)panel.y + 60, "科室信息");
    Workbench_draw_info_row(app, (int)panel.x + 16, (int)panel.y + 96, "内科", "DEPT001");
    Workbench_draw_info_row(app, (int)panel.x + 16, (int)panel.y + 124, "外科", "DEPT002");
    Workbench_draw_info_row(app, (int)panel.x + 16, (int)panel.y + 152, "儿科", "DEPT003");

    Workbench_draw_section_header(app, (int)panel.x + 16, (int)panel.y + 200, "说明");
    DrawText("医生与科室数据由系统管理员维护，当前为只读展示。",
        (int)panel.x + 16, (int)panel.y + 236, 18, app->theme.text_secondary);

    (void)wb;
}

/* ── Page 3: Wards & medicines ── */

static void admin_draw_wards(DesktopApp *app, Rectangle panel) {
    static char output[8192] = {0};
    Result result;
    int clicked = 0;
    const WorkbenchDef *wb = Workbench_get(app->state.current_user.role);
    Rectangle left = { panel.x, panel.y, panel.width * 0.48f, panel.height };
    Rectangle right = { panel.x + panel.width * 0.52f, panel.y, panel.width * 0.48f, panel.height };

    DrawRectangleRounded(left, 0.12f, 8, app->theme.panel);
    DrawText("病房与药品总览", (int)left.x + 16, (int)left.y + 16, 24, app->theme.text_primary);

    Workbench_draw_quick_action_btn(app,
        (Rectangle){ left.x + 16, left.y + 60, 160, 36 },
        "查看病房列表", wb->accent, &clicked);
    if (clicked) {
        result = DesktopAdapters_list_wards(&app->application, output, sizeof(output));
        show_result(app, result);
    }

    Workbench_draw_quick_action_btn(app,
        (Rectangle){ left.x + 16, left.y + 112, 160, 36 },
        "低库存药品", wb->accent, &clicked);
    if (clicked) {
        result = DesktopAdapters_find_low_stock_medicines(&app->application, output, sizeof(output));
        show_result(app, result);
    }

    draw_output_panel(app, right, "查询结果", output,
        "点击左侧按钮查看病房或药品信息。");
}

/* ── Page 4: System info ── */

static void admin_draw_system(const DesktopApp *app, Rectangle panel) {
    DrawRectangleRounded(panel, 0.12f, 8, app->theme.panel);
    DrawText("系统信息", (int)panel.x + 16, (int)panel.y + 16, 24, app->theme.text_primary);

    Workbench_draw_info_row(app, (int)panel.x + 16, (int)panel.y + 60, "当前用户", app->state.current_user.user_id);
    Workbench_draw_info_row(app, (int)panel.x + 16, (int)panel.y + 88, "当前角色",
        DesktopApp_user_role_label(app->state.current_user.role));

    if (app->paths != 0) {
        Workbench_draw_section_header(app, (int)panel.x + 16, (int)panel.y + 130, "数据文件路径");
        Workbench_draw_info_row(app, (int)panel.x + 16, (int)panel.y + 166, "users.txt", app->paths->user_path);
        Workbench_draw_info_row(app, (int)panel.x + 16, (int)panel.y + 194, "patients.txt", app->paths->patient_path);
        Workbench_draw_info_row(app, (int)panel.x + 16, (int)panel.y + 222, "registrations", app->paths->registration_path);
        Workbench_draw_info_row(app, (int)panel.x + 16, (int)panel.y + 250, "dispense_rec", app->paths->dispense_record_path);
    }
}

/* ── Main entry point ── */

void AdminWorkbench_draw(DesktopApp *app, Rectangle panel, int page) {
    if (app == 0) return;

    switch (page) {
        case 0: admin_draw_home(app, panel); break;
        case 1: admin_draw_patients(app, panel); break;
        case 2: admin_draw_doctors(app, panel); break;
        case 3: admin_draw_wards(app, panel); break;
        case 4: admin_draw_system(app, panel); break;
        default: admin_draw_home(app, panel); break;
    }
}
