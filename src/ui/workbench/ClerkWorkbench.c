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

static void release_focus(int *active_field, const Rectangle *fields, int count) {
    Vector2 mouse = GetMousePosition();
    if (active_field == 0 || fields == 0 || count <= 0) return;
    if (*active_field <= 0 || *active_field > count) return;
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) &&
        !CheckCollisionPointRec(mouse, fields[*active_field - 1])) {
        *active_field = 0;
    }
}

/* ── Page 0: Reception home ── */

static void clerk_draw_home(DesktopApp *app, Rectangle panel) {
    const WorkbenchDef *wb = Workbench_get(app->state.current_user.role);
    const char *labels[4] = { "今日挂号", "待接待", "可取消", "新建患者" };
    char v0[16], v1[16], v2[16], v3[16];
    const char *values[4];
    float section_y = 0;
    int clicked = 0;
    Result result;

    if (app->state.dashboard.loaded == 0) {
        result = DesktopAdapters_load_dashboard(&app->application, &app->state.dashboard);
        show_result(app, result);
        app->state.dashboard.loaded = result.success ? 1 : -1;
    }

    snprintf(v0, sizeof(v0), "%d", app->state.dashboard.registration_count);
    snprintf(v1, sizeof(v1), "%d", app->state.dashboard.patient_count);
    snprintf(v2, sizeof(v2), "--");
    snprintf(v3, sizeof(v3), "--");
    values[0] = v0; values[1] = v1; values[2] = v2; values[3] = v3;

    Workbench_draw_home_cards(app, panel, labels, values, wb->accent);

    section_y = panel.y + 98;
    Workbench_draw_section_header(app, (int)panel.x, (int)section_y, "快捷操作");
    section_y += 36;

    Workbench_draw_quick_action_btn(app,
        (Rectangle){ panel.x, section_y, 140, 36 },
        "刷新数据", wb->accent, &clicked);
    if (clicked) { app->state.dashboard.loaded = 0; }

    Workbench_draw_quick_action_btn(app,
        (Rectangle){ panel.x + 158, section_y, 140, 36 },
        "患者查询", wb->accent, &clicked);
    if (clicked) { app->state.workbench_page = 2; }

    Workbench_draw_quick_action_btn(app,
        (Rectangle){ panel.x + 316, section_y, 140, 36 },
        "挂号办理", wb->accent, &clicked);
    if (clicked) { app->state.workbench_page = 3; }

    Workbench_draw_quick_action_btn(app,
        (Rectangle){ panel.x + 474, section_y, 140, 36 },
        "挂号查询", wb->accent, &clicked);
    if (clicked) { app->state.workbench_page = 4; }

    /* Recent registrations summary */
    section_y += 54;
    Workbench_draw_section_header(app, (int)panel.x, (int)section_y, "最近挂号");
    section_y += 36;

    {
        const LinkedListNode *cur = app->state.dashboard.recent_registrations.head;
        int i = 0;
        DrawRectangleRounded(
            (Rectangle){ panel.x, section_y, panel.width, 180 },
            0.12f, 8, app->theme.panel);
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
}

/* ── Page 1: Patient creation (placeholder) ── */

static void clerk_draw_patient_create(DesktopApp *app, Rectangle panel) {
    DrawRectangleRounded(panel, 0.12f, 8, app->theme.panel);
    DrawText("患者建档", (int)panel.x + 16, (int)panel.y + 16, 24, app->theme.text_primary);

    draw_empty_state(app,
        (Rectangle){ panel.x + 16, panel.y + 60, panel.width - 32, 100 },
        "功能开发中",
        "患者建档功能尚未接入后端适配器，敬请期待。");
}

/* ── Page 2: Patient search ── */

static void clerk_draw_patient_search(DesktopApp *app, Rectangle panel) {
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
    } else {
        draw_empty_state(app,
            (Rectangle){ detail_rect.x + 12, detail_rect.y + 48, detail_rect.width - 24, 84 },
            "等待选择患者", "点击左侧患者行后可查看详情信息。");
    }
}

/* ── Page 3: Registration form ── */

static void clerk_draw_registration(DesktopApp *app, Rectangle panel) {
    DesktopRegistrationPageState *rp = &app->state.registration_page;
    Rectangle patient_box = { panel.x + 16, panel.y + 82, 260, 34 };
    Rectangle doctor_box = { panel.x + 16, panel.y + 152, 260, 34 };
    Rectangle dept_box = { panel.x + 16, panel.y + 222, 260, 34 };
    Rectangle time_box = { panel.x + 16, panel.y + 292, 260, 34 };
    Rectangle input_fields[] = { patient_box, doctor_box, dept_box, time_box };
    Result result;
    Registration registration;

    DrawRectangleRounded(panel, 0.12f, 8, app->theme.panel);
    DrawText("挂号办理", (int)panel.x + 16, (int)panel.y + 16, 24, app->theme.text_primary);

    release_focus(&rp->active_field, input_fields, 4);

    GuiLabel((Rectangle){ panel.x + 16, panel.y + 58, 120, 20 }, "患者编号");
    draw_text_input(patient_box, rp->patient_id, sizeof(rp->patient_id), 1, &rp->active_field, 1);

    GuiLabel((Rectangle){ panel.x + 16, panel.y + 128, 120, 20 }, "医生编号");
    draw_text_input(doctor_box, rp->doctor_id, sizeof(rp->doctor_id), 1, &rp->active_field, 2);

    GuiLabel((Rectangle){ panel.x + 16, panel.y + 198, 120, 20 }, "科室编号");
    draw_text_input(dept_box, rp->department_id, sizeof(rp->department_id), 1, &rp->active_field, 3);

    GuiLabel((Rectangle){ panel.x + 16, panel.y + 268, 120, 20 }, "挂号时间");
    draw_text_input(time_box, rp->registered_at, sizeof(rp->registered_at), 1, &rp->active_field, 4);

    if (GuiButton((Rectangle){ panel.x + 16, panel.y + 342, 160, 38 }, "提交挂号")) {
        memset(&registration, 0, sizeof(registration));
        result = DesktopAdapters_submit_registration(
            &app->application,
            rp->patient_id, rp->doctor_id,
            rp->department_id, rp->registered_at,
            &registration);
        show_result(app, result);
        if (result.success) {
            strncpy(rp->last_registration_id, registration.registration_id,
                sizeof(rp->last_registration_id) - 1);
            rp->last_registration_id[sizeof(rp->last_registration_id) - 1] = '\0';
            app->state.dashboard.loaded = 0;
        }
    }

    DrawText(
        TextFormat("最近成功挂号编号: %s",
            rp->last_registration_id[0] != '\0' ? rp->last_registration_id : "--"),
        (int)panel.x + 320, (int)panel.y + 90, 20, app->theme.text_secondary);
}

/* ── Page 4: Registration query & cancel ── */

static void clerk_draw_reg_query(DesktopApp *app, Rectangle panel) {
    static char output[8192] = {0};
    static char query_reg_id[64] = {0};
    static char cancel_reg_id[64] = {0};
    static char cancel_time[64] = {0};
    static char query_patient_id[64] = {0};
    static int active_field = 0;
    Rectangle reg_id_box = { panel.x + 16, panel.y + 82, 220, 34 };
    Rectangle cancel_id_box = { panel.x + 16, panel.y + 182, 220, 34 };
    Rectangle cancel_time_box = { panel.x + 248, panel.y + 182, 200, 34 };
    Rectangle patient_id_box = { panel.x + 16, panel.y + 282, 220, 34 };
    Rectangle input_fields[] = { reg_id_box, cancel_id_box, cancel_time_box, patient_id_box };
    Rectangle left = { panel.x, panel.y, panel.width * 0.52f, panel.height };
    Rectangle right = { panel.x + panel.width * 0.56f, panel.y, panel.width * 0.44f, panel.height };
    Result result;

    release_focus(&active_field, input_fields, 4);

    DrawRectangleRounded(left, 0.12f, 8, app->theme.panel);
    DrawRectangleRoundedLinesEx(left, 0.12f, 8, 1.0f, Fade(app->theme.border, 0.85f));
    DrawText("挂号查询与取消", (int)left.x + 16, (int)left.y + 16, 24, app->theme.text_primary);

    /* Query by registration ID */
    Workbench_draw_section_header(app, (int)left.x + 16, (int)left.y + 56, "按挂号编号查询");
    draw_text_input(reg_id_box, query_reg_id, sizeof(query_reg_id), 1, &active_field, 1);
    if (GuiButton((Rectangle){ left.x + 248, left.y + 82, 120, 34 }, "查询")) {
        result = DesktopAdapters_query_registration(
            &app->application, query_reg_id, output, sizeof(output));
        show_result(app, result);
    }

    /* Cancel registration */
    Workbench_draw_section_header(app, (int)left.x + 16, (int)left.y + 136, "取消挂号");
    GuiLabel((Rectangle){ left.x + 16, left.y + 164, 120, 18 }, "挂号编号 / 取消时间");
    draw_text_input(cancel_id_box, cancel_reg_id, sizeof(cancel_reg_id), 1, &active_field, 2);
    draw_text_input(cancel_time_box, cancel_time, sizeof(cancel_time), 1, &active_field, 3);
    if (GuiButton((Rectangle){ left.x + 16, left.y + 224, 120, 34 }, "取消挂号")) {
        result = DesktopAdapters_cancel_registration(
            &app->application, cancel_reg_id, cancel_time, output, sizeof(output));
        show_result(app, result);
        if (result.success) { app->state.dashboard.loaded = 0; }
    }

    /* Query by patient ID */
    Workbench_draw_section_header(app, (int)left.x + 16, (int)left.y + 276, "按患者编号查询");
    draw_text_input(patient_id_box, query_patient_id, sizeof(query_patient_id), 1, &active_field, 4);
    if (GuiButton((Rectangle){ left.x + 248, left.y + 282, 120, 34 }, "查询")) {
        result = DesktopAdapters_query_registrations_by_patient(
            &app->application, query_patient_id, output, sizeof(output));
        show_result(app, result);
    }

    /* Output panel */
    DrawRectangleRounded(right, 0.12f, 8, app->theme.panel);
    DrawRectangleRoundedLinesEx(right, 0.12f, 8, 1.0f, Fade(app->theme.border, 0.85f));
    DrawText("查询结果", (int)right.x + 14, (int)right.y + 12, 20, app->theme.text_primary);
    if (output[0] != '\0') {
        DrawText(output, (int)right.x + 14, (int)right.y + 48, 17, app->theme.text_secondary);
    } else {
        draw_empty_state(app,
            (Rectangle){ right.x + 12, right.y + 46, right.width - 24, 84 },
            "暂无输出", "左侧提交查询或取消操作后，这里显示返回结果。");
    }
}

/* ── Main entry point ── */

void ClerkWorkbench_draw(DesktopApp *app, Rectangle panel, int page) {
    if (app == 0) return;

    switch (page) {
        case 0: clerk_draw_home(app, panel); break;
        case 1: clerk_draw_patient_create(app, panel); break;
        case 2: clerk_draw_patient_search(app, panel); break;
        case 3: clerk_draw_registration(app, panel); break;
        case 4: clerk_draw_reg_query(app, panel); break;
        default: clerk_draw_home(app, panel); break;
    }
}
