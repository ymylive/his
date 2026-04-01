#include "ui/Workbench.h"
#include "ui/DesktopApp.h"
#include "ui/DesktopAdapters.h"
#include "raygui.h"
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "repository/AdmissionRepository.h"
#include "repository/BedRepository.h"
#include "repository/WardRepository.h"

/* ── Local helpers ── */

static WorkbenchSearchSelectState g_ward_select;
static WorkbenchSearchSelectState g_ward_bed_select;
static WorkbenchSearchSelectState g_ward_admission_select;

static void ward_refresh_ward_select(DesktopApp *app, DesktopInpatientPageState *st) {
    LinkedList wards;
    const LinkedListNode *current = 0;
    int selected_index = -1;

    if (!WorkbenchSearchSelectState_needs_refresh(&g_ward_select)) return;

    WorkbenchSearchSelectState_reset(&g_ward_select);
    LinkedList_init(&wards);
    if (WardRepository_load_all(&app->application.inpatient_service.ward_repository, &wards).success != 1) {
        return;
    }

    current = wards.head;
    while (current != 0) {
        const Ward *ward = (const Ward *)current->data;
        char label[WORKBENCH_SELECT_LABEL_CAPACITY];
        snprintf(label, sizeof(label), "%s | %s | %s", ward->ward_id, ward->name, ward->location);
        if (Workbench_text_contains_ignore_case(label, g_ward_select.query)) {
            if (strcmp(st->ward_id, ward->ward_id) == 0) {
                selected_index = g_ward_select.option_count;
            }
            WorkbenchSearchSelectState_add_option(&g_ward_select, label);
        }
        current = current->next;
    }

    WorkbenchSearchSelectState_finalize(&g_ward_select);
    g_ward_select.active_index = selected_index;
    WardRepository_clear_loaded_list(&wards);
}

static void ward_refresh_bed_select(DesktopApp *app, DesktopInpatientPageState *st, int available_only) {
    LinkedList beds;
    const LinkedListNode *current = 0;
    int selected_index = -1;

    if (!WorkbenchSearchSelectState_needs_refresh(&g_ward_bed_select)) return;

    WorkbenchSearchSelectState_reset(&g_ward_bed_select);
    LinkedList_init(&beds);
    if (BedRepository_load_all(&app->application.inpatient_service.bed_repository, &beds).success != 1) {
        return;
    }

    current = beds.head;
    while (current != 0) {
        const Bed *bed = (const Bed *)current->data;
        char label[WORKBENCH_SELECT_LABEL_CAPACITY];

        if (st->ward_id[0] != '\0' && strcmp(st->ward_id, bed->ward_id) != 0) {
            current = current->next;
            continue;
        }
        if (available_only != 0 && bed->status != BED_STATUS_AVAILABLE) {
            current = current->next;
            continue;
        }

        snprintf(label, sizeof(label), "%s | 病区=%s | 房间=%s | 床位=%s", bed->bed_id, bed->ward_id, bed->room_no, bed->bed_no);
        if (Workbench_text_contains_ignore_case(label, g_ward_bed_select.query)) {
            if (strcmp(st->bed_id, bed->bed_id) == 0 || strcmp(st->query_bed_id, bed->bed_id) == 0) {
                selected_index = g_ward_bed_select.option_count;
            }
            WorkbenchSearchSelectState_add_option(&g_ward_bed_select, label);
        }
        current = current->next;
    }

    WorkbenchSearchSelectState_finalize(&g_ward_bed_select);
    g_ward_bed_select.active_index = selected_index;
    BedRepository_clear_loaded_list(&beds);
}

static void ward_refresh_admission_select(DesktopApp *app, DesktopInpatientPageState *st) {
    LinkedList admissions;
    const LinkedListNode *current = 0;
    int selected_index = -1;

    if (!WorkbenchSearchSelectState_needs_refresh(&g_ward_admission_select)) return;

    WorkbenchSearchSelectState_reset(&g_ward_admission_select);
    LinkedList_init(&admissions);
    if (AdmissionRepository_load_all(&app->application.inpatient_service.admission_repository, &admissions).success != 1) {
        return;
    }

    current = admissions.head;
    while (current != 0) {
        const Admission *admission = (const Admission *)current->data;
        char label[WORKBENCH_SELECT_LABEL_CAPACITY];
        if (admission->status != ADMISSION_STATUS_ACTIVE) {
            current = current->next;
            continue;
        }
        snprintf(label, sizeof(label), "%s | 患者=%s | 病区=%s | 床位=%s", admission->admission_id, admission->patient_id, admission->ward_id, admission->bed_id);
        if (Workbench_text_contains_ignore_case(label, g_ward_admission_select.query)) {
            if (strcmp(st->admission_id, admission->admission_id) == 0) {
                selected_index = g_ward_admission_select.option_count;
            }
            WorkbenchSearchSelectState_add_option(&g_ward_admission_select, label);
        }
        current = current->next;
    }

    WorkbenchSearchSelectState_finalize(&g_ward_admission_select);
    g_ward_admission_select.active_index = selected_index;
    AdmissionRepository_clear_loaded_list(&admissions);
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

    Workbench_draw_output_panel(
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
        Workbench_show_result(app, result);
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
    Rectangle ward_search = { lx, ly + 112, 240, 34 };
    Rectangle ward_list = { lx, ly + 156, 240, 120 };

    ward_refresh_ward_select(app, st);

    /* Left panel: Query form */
    DrawRectangleRounded(split.list_bounds, 0.12f, 8, app->theme.panel);
    DrawRectangleRoundedLinesEx(split.list_bounds, 0.12f, 8, 1.0f, Fade(app->theme.border, 0.85f));
    DrawText("床位状态查询", (int)lx, (int)ly + 14, 24, app->theme.text_primary);

    Workbench_draw_section_header(app, (int)lx, (int)ly + 56, "查询条件");

    Workbench_draw_form_label(app, (int)lx, (int)ly + 90, "病区搜索与选择", 1);
    Workbench_draw_search_select(app, ward_search, ward_list, &g_ward_select, &st->active_field, 51);
    if (g_ward_select.active_index >= 0 && g_ward_select.active_index < g_ward_select.option_count) {
        sscanf(g_ward_select.labels[g_ward_select.active_index], "%15s", st->ward_id);
        result = DesktopAdapters_list_beds_by_ward(
            &app->application, st->ward_id,
            st->output, sizeof(st->output)
        );
        Workbench_show_result(app, result);
    }

    /* Status legend */
    Workbench_draw_section_header(app, (int)lx, (int)ly + 300, "状态说明");
    {
        Rectangle available_badge = { lx, ly + 334, 100, 28 };
        Rectangle occupied_badge = { lx + 116, ly + 334, 100, 28 };
        Workbench_draw_status_badge(app, available_badge, "可用", (Color){ 34, 197, 94, 255 });
        Workbench_draw_status_badge(app, occupied_badge, "占用", (Color){ 239, 68, 68, 255 });
    }

    DrawText("绿色表示床位可用，红色表示已占用", (int)lx, (int)ly + 376, 16, app->theme.text_secondary);

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
    Rectangle admission_search = { lx, ly + 112, 240, 34 };
    Rectangle admission_list = { lx, ly + 156, 240, 96 };
    Rectangle bed_search = { lx, ly + 286, 240, 34 };
    Rectangle bed_list = { lx, ly + 330, 240, 96 };

    ward_refresh_admission_select(app, st);
    ward_refresh_bed_select(app, st, 1);

    /* Left panel: Transfer form */
    DrawRectangleRounded(split.list_bounds, 0.12f, 8, app->theme.panel);
    DrawRectangleRoundedLinesEx(split.list_bounds, 0.12f, 8, 1.0f, Fade(app->theme.border, 0.85f));
    DrawText("转床调度", (int)lx, (int)ly + 14, 24, app->theme.text_primary);

    Workbench_draw_section_header(app, (int)lx, (int)ly + 56, "转床信息");

    Workbench_draw_form_label(app, (int)lx, (int)ly + 90, "住院记录搜索与选择", 1);
    Workbench_draw_search_select(app, admission_search, admission_list, &g_ward_admission_select, &st->active_field, 52);
    if (g_ward_admission_select.active_index >= 0 && g_ward_admission_select.active_index < g_ward_admission_select.option_count) {
        sscanf(g_ward_admission_select.labels[g_ward_admission_select.active_index], "%15s", st->admission_id);
    }

    Workbench_draw_form_label(app, (int)lx, (int)ly + 264, "目标床位搜索与选择", 1);
    Workbench_draw_search_select(app, bed_search, bed_list, &g_ward_bed_select, &st->active_field, 53);
    if (g_ward_bed_select.active_index >= 0 && g_ward_bed_select.active_index < g_ward_bed_select.option_count) {
        sscanf(g_ward_bed_select.labels[g_ward_bed_select.active_index], "%15s", st->bed_id);
    }

    Workbench_draw_form_label(app, (int)lx, (int)ly + 438, "转床时间", 0);
    Workbench_draw_text_input((Rectangle){ lx, ly + 460, 240, 34 }, transferred_at, sizeof(transferred_at), 1, &st->active_field, 3);

    if (GuiButton((Rectangle){ lx, ly + 510, 160, 36 }, "执行转床")) {
        result = DesktopAdapters_transfer_bed(
            &app->application,
            st->admission_id, st->bed_id, transferred_at,
            st->output, sizeof(st->output)
        );
        Workbench_show_result(app, result);
        if (result.success) {
            WorkbenchSearchSelectState_mark_dirty(&g_ward_select);
            WorkbenchSearchSelectState_mark_dirty(&g_ward_admission_select);
            WorkbenchSearchSelectState_mark_dirty(&g_ward_bed_select);
        }
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
    Rectangle admission_search = { lx, ly + 112, 240, 34 };
    Rectangle admission_list = { lx, ly + 156, 240, 120 };

    ward_refresh_admission_select(app, st);

    /* Left panel: Check form */
    DrawRectangleRounded(split.list_bounds, 0.12f, 8, app->theme.panel);
    DrawRectangleRoundedLinesEx(split.list_bounds, 0.12f, 8, 1.0f, Fade(app->theme.border, 0.85f));
    DrawText("出院检查", (int)lx, (int)ly + 14, 24, app->theme.text_primary);

    Workbench_draw_section_header(app, (int)lx, (int)ly + 56, "患者信息");

    Workbench_draw_form_label(app, (int)lx, (int)ly + 90, "住院记录搜索与选择", 1);
    Workbench_draw_search_select(app, admission_search, admission_list, &g_ward_admission_select, &st->active_field, 54);
    if (g_ward_admission_select.active_index >= 0 && g_ward_admission_select.active_index < g_ward_admission_select.option_count) {
        sscanf(g_ward_admission_select.labels[g_ward_admission_select.active_index], "%15s", st->admission_id);
    }

    if (GuiButton((Rectangle){ lx, ly + 290, 160, 36 }, "执行检查")) {
        result = DesktopAdapters_discharge_check(
            &app->application, st->admission_id,
            st->output, sizeof(st->output)
        );
        Workbench_show_result(app, result);
        if (result.success) {
            WorkbenchSearchSelectState_mark_dirty(&g_ward_admission_select);
        }
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
    static int initialized = 0;
    if (!initialized) {
        WorkbenchSearchSelectState_mark_dirty(&g_ward_select);
        WorkbenchSearchSelectState_mark_dirty(&g_ward_bed_select);
        WorkbenchSearchSelectState_mark_dirty(&g_ward_admission_select);
        initialized = 1;
    }
    switch (page) {
        case 0: draw_home(app, panel); break;
        case 1: draw_ward_list(app, panel); break;
        case 2: draw_bed_status(app, panel); break;
        case 3: draw_transfer(app, panel); break;
        case 4: draw_discharge_check(app, panel); break;
        default: draw_home(app, panel); break;
    }
}
