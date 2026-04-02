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

static WorkbenchSearchSelectState g_inpatient_patient_select;
static WorkbenchSearchSelectState g_inpatient_ward_select;
static WorkbenchSearchSelectState g_inpatient_bed_select;
static WorkbenchSearchSelectState g_inpatient_admission_select;

static void inpatient_refresh_patient_select(DesktopApp *app, DesktopInpatientPageState *st) {
    LinkedList patients;
    const LinkedListNode *current = 0;
    int selected_index = -1;
    Result result;

    if (!WorkbenchSearchSelectState_needs_refresh(&g_inpatient_patient_select)) return;

    WorkbenchSearchSelectState_reset(&g_inpatient_patient_select);
    LinkedList_init(&patients);
    result = DesktopAdapters_search_patients(&app->application, DESKTOP_PATIENT_SEARCH_BY_NAME, "", &patients);
    if (result.success != 1) {
        return;
    }

    current = patients.head;
    while (current != 0) {
        const Patient *patient = (const Patient *)current->data;
        char label[WORKBENCH_SELECT_LABEL_CAPACITY];

        snprintf(label, sizeof(label), "%s | %s | %d岁 | %s", patient->patient_id, patient->name, patient->age, patient->contact);
        if (Workbench_text_contains_ignore_case(label, g_inpatient_patient_select.query)) {
            if (strcmp(st->patient_id, patient->patient_id) == 0 || strcmp(st->query_patient_id, patient->patient_id) == 0) {
                selected_index = g_inpatient_patient_select.option_count;
            }
            WorkbenchSearchSelectState_add_option(&g_inpatient_patient_select, label);
        }
        current = current->next;
    }

    WorkbenchSearchSelectState_finalize(&g_inpatient_patient_select);
    g_inpatient_patient_select.active_index = selected_index;
    LinkedList_clear(&patients, free);
}

static void inpatient_refresh_ward_select(DesktopApp *app, DesktopInpatientPageState *st) {
    LinkedList wards;
    const LinkedListNode *current = 0;
    int selected_index = -1;

    if (!WorkbenchSearchSelectState_needs_refresh(&g_inpatient_ward_select)) return;

    WorkbenchSearchSelectState_reset(&g_inpatient_ward_select);
    LinkedList_init(&wards);
    if (WardRepository_load_all(&app->application.inpatient_service.ward_repository, &wards).success != 1) {
        return;
    }

    current = wards.head;
    while (current != 0) {
        const Ward *ward = (const Ward *)current->data;
        char label[WORKBENCH_SELECT_LABEL_CAPACITY];

        snprintf(label, sizeof(label), "%s | %s | %s | %d/%d", ward->ward_id, ward->name, ward->location, ward->occupied_beds, ward->capacity);
        if (Workbench_text_contains_ignore_case(label, g_inpatient_ward_select.query)) {
            if (strcmp(st->ward_id, ward->ward_id) == 0) {
                selected_index = g_inpatient_ward_select.option_count;
            }
            WorkbenchSearchSelectState_add_option(&g_inpatient_ward_select, label);
        }
        current = current->next;
    }

    WorkbenchSearchSelectState_finalize(&g_inpatient_ward_select);
    g_inpatient_ward_select.active_index = selected_index;
    WardRepository_clear_loaded_list(&wards);
}

static void inpatient_refresh_bed_select(DesktopApp *app, DesktopInpatientPageState *st, int available_only) {
    LinkedList beds;
    const LinkedListNode *current = 0;
    int selected_index = -1;
    WorkbenchSearchSelectState *picker = &g_inpatient_bed_select;

    if (!WorkbenchSearchSelectState_needs_refresh(picker)) return;

    WorkbenchSearchSelectState_reset(picker);
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
        if (Workbench_text_contains_ignore_case(label, picker->query)) {
            if (strcmp(st->bed_id, bed->bed_id) == 0 || strcmp(st->query_bed_id, bed->bed_id) == 0) {
                selected_index = picker->option_count;
            }
            WorkbenchSearchSelectState_add_option(picker, label);
        }
        current = current->next;
    }

    WorkbenchSearchSelectState_finalize(picker);
    picker->active_index = selected_index;
    BedRepository_clear_loaded_list(&beds);
}

static void inpatient_refresh_admission_select(DesktopApp *app, DesktopInpatientPageState *st, int active_only) {
    LinkedList admissions;
    const LinkedListNode *current = 0;
    int selected_index = -1;

    if (!WorkbenchSearchSelectState_needs_refresh(&g_inpatient_admission_select)) return;

    WorkbenchSearchSelectState_reset(&g_inpatient_admission_select);
    LinkedList_init(&admissions);
    if (AdmissionRepository_load_all(&app->application.inpatient_service.admission_repository, &admissions).success != 1) {
        return;
    }

    current = admissions.head;
    while (current != 0) {
        const Admission *admission = (const Admission *)current->data;
        char label[WORKBENCH_SELECT_LABEL_CAPACITY];

        if (active_only != 0 && admission->status != ADMISSION_STATUS_ACTIVE) {
            current = current->next;
            continue;
        }

        snprintf(label, sizeof(label), "%s | 患者=%s | 病区=%s | 床位=%s", admission->admission_id, admission->patient_id, admission->ward_id, admission->bed_id);
        if (Workbench_text_contains_ignore_case(label, g_inpatient_admission_select.query)) {
            if (strcmp(st->admission_id, admission->admission_id) == 0) {
                selected_index = g_inpatient_admission_select.option_count;
            }
            WorkbenchSearchSelectState_add_option(&g_inpatient_admission_select, label);
        }
        current = current->next;
    }

    WorkbenchSearchSelectState_finalize(&g_inpatient_admission_select);
    g_inpatient_admission_select.active_index = selected_index;
    AdmissionRepository_clear_loaded_list(&admissions);
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
    Rectangle patient_search = { split.detail_bounds.x + 16, split.detail_bounds.y + 50, split.detail_bounds.width - 32, 34 };
    Rectangle patient_list = { split.detail_bounds.x + 16, split.detail_bounds.y + 92, split.detail_bounds.width - 32, split.detail_bounds.height * 0.22f };
    Rectangle ward_search = { split.detail_bounds.x + 16, split.detail_bounds.y + split.detail_bounds.height * 0.34f, split.detail_bounds.width - 32, 34 };
    Rectangle ward_list = { split.detail_bounds.x + 16, split.detail_bounds.y + split.detail_bounds.height * 0.34f + 42, split.detail_bounds.width - 32, split.detail_bounds.height * 0.18f };
    Rectangle bed_search = { split.detail_bounds.x + 16, split.detail_bounds.y + split.detail_bounds.height * 0.58f, split.detail_bounds.width - 32, 34 };
    Rectangle bed_list = { split.detail_bounds.x + 16, split.detail_bounds.y + split.detail_bounds.height * 0.58f + 42, split.detail_bounds.width - 32, split.detail_bounds.height * 0.18f };

    inpatient_refresh_patient_select(app, st);
    inpatient_refresh_ward_select(app, st);
    inpatient_refresh_bed_select(app, st, 1);

    DrawRectangleRounded(split.list_bounds, 0.12f, 8, app->theme.panel);
    DrawRectangleRoundedLinesEx(split.list_bounds, 0.12f, 8, 1.0f, Fade(app->theme.border, 0.85f));
    DrawText("入院登记", (int)lx + 16, (int)ly + 14, 24, app->theme.text_primary);

    Workbench_draw_section_header(app, (int)lx + 16, (int)ly + 52, "患者信息");

    Workbench_draw_form_label(app, (int)lx + 16, (int)ly + 86, "已选患者", 1);
    Workbench_draw_status_badge(app, (Rectangle){ lx + 120, ly + 82, split.list_bounds.width - 136, 28 }, st->patient_id[0] != '\0' ? st->patient_id : "请在右侧搜索并选择患者", Workbench_get(USER_ROLE_INPATIENT_REGISTRAR)->accent);

    Workbench_draw_section_header(app, (int)lx + 16, (int)ly + 158, "床位分配");

    Workbench_draw_form_label(app, (int)lx + 16, (int)ly + 192, "已选病区", 1);
    Workbench_draw_status_badge(app, (Rectangle){ lx + 120, ly + 188, split.list_bounds.width - 136, 28 }, st->ward_id[0] != '\0' ? st->ward_id : "请在右侧搜索并选择病区", Workbench_get(USER_ROLE_INPATIENT_REGISTRAR)->accent);

    Workbench_draw_form_label(app, (int)lx + 16, (int)ly + 228, "已选床位", 1);
    Workbench_draw_status_badge(app, (Rectangle){ lx + 120, ly + 224, split.list_bounds.width - 136, 28 }, st->bed_id[0] != '\0' ? st->bed_id : "请在右侧搜索并选择床位", Workbench_get(USER_ROLE_INPATIENT_REGISTRAR)->accent);

    Workbench_draw_section_header(app, (int)lx + 16, (int)ly + 264, "入院详情");

    Workbench_draw_form_label(app, (int)lx + 16, (int)ly + 298, "入院时间", 1);
    {
        static WorkbenchDatePickerState inpatient_admit_dp;
        static int inpatient_admit_dp_init = 0;
        if (!inpatient_admit_dp_init) { WorkbenchDatePickerState_init(&inpatient_admit_dp); inpatient_admit_dp_init = 1; }
        Workbench_draw_date_picker(app, (Rectangle){ lx + 16, ly + 320, 200, 34 }, &inpatient_admit_dp, st->admitted_at, sizeof(st->admitted_at), &st->active_field, 4);
    }

    Workbench_draw_form_label(app, (int)lx + 16, (int)ly + 364, "入院摘要", 0);
    Workbench_draw_text_input((Rectangle){ lx + 16, ly + 386, split.list_bounds.width - 32, 34 }, st->summary, sizeof(st->summary), 1, &st->active_field, 5);

    if (GuiButton((Rectangle){ lx + 16, ly + 440, 160, 36 }, "办理入院")) {
        if (!Workbench_validate_time_format(st->admitted_at)) {
            DesktopAppState_show_message(&app->state, DESKTOP_MESSAGE_ERROR, "入院时间格式无效，请使用 YYYY-MM-DD HH:MM");
        } else {
        result = DesktopAdapters_admit_patient(
            &app->application,
            st->patient_id, st->ward_id, st->bed_id,
            st->admitted_at, st->summary,
            st->output, sizeof(st->output)
        );
        Workbench_show_result(app, result);
        if (result.success) {
            WorkbenchSearchSelectState_mark_dirty(&g_inpatient_patient_select);
            WorkbenchSearchSelectState_mark_dirty(&g_inpatient_ward_select);
            WorkbenchSearchSelectState_mark_dirty(&g_inpatient_bed_select);
            WorkbenchSearchSelectState_mark_dirty(&g_inpatient_admission_select);
        }
        }
    }

    DrawText("患者搜索与选择", (int)split.detail_bounds.x + 16, (int)split.detail_bounds.y + 16, 20, app->theme.text_primary);
    Workbench_draw_search_select(app, patient_search, patient_list, &g_inpatient_patient_select, &st->active_field, 41);
    if (g_inpatient_patient_select.active_index >= 0 && g_inpatient_patient_select.active_index < g_inpatient_patient_select.option_count) {
        sscanf(g_inpatient_patient_select.labels[g_inpatient_patient_select.active_index], "%15s", st->patient_id);
    }
    DrawText("病区搜索与选择", (int)split.detail_bounds.x + 16, (int)(split.detail_bounds.y + split.detail_bounds.height * 0.34f - 28), 20, app->theme.text_primary);
    Workbench_draw_search_select(app, ward_search, ward_list, &g_inpatient_ward_select, &st->active_field, 42);
    if (g_inpatient_ward_select.active_index >= 0 && g_inpatient_ward_select.active_index < g_inpatient_ward_select.option_count) {
        sscanf(g_inpatient_ward_select.labels[g_inpatient_ward_select.active_index], "%15s", st->ward_id);
        st->bed_id[0] = '\0';
    }
    DrawText("床位搜索与选择", (int)split.detail_bounds.x + 16, (int)(split.detail_bounds.y + split.detail_bounds.height * 0.58f - 28), 20, app->theme.text_primary);
    Workbench_draw_search_select(app, bed_search, bed_list, &g_inpatient_bed_select, &st->active_field, 43);
    if (g_inpatient_bed_select.active_index >= 0 && g_inpatient_bed_select.active_index < g_inpatient_bed_select.option_count) {
        sscanf(g_inpatient_bed_select.labels[g_inpatient_bed_select.active_index], "%15s", st->bed_id);
    }
    Workbench_draw_output_panel(app, (Rectangle){ split.detail_bounds.x + 12, split.detail_bounds.y + split.detail_bounds.height * 0.82f, split.detail_bounds.width - 24, split.detail_bounds.height * 0.16f - 12 }, "入院结果", st->output,
                      "填写左侧表单并点击「办理入院」。");
}

/* ── Page 2: Discharge ── */

static void draw_discharge(DesktopApp *app, Rectangle panel) {
    DesktopInpatientPageState *st = &app->state.inpatient_page;
    WorkbenchListDetailLayout split = Workbench_compute_list_detail_layout(panel, 0.52f, 18.0f, 280.0f);
    float lx = split.list_bounds.x;
    float ly = split.list_bounds.y;
    Result result;
    Rectangle admission_search = { split.detail_bounds.x + 16, split.detail_bounds.y + 50, split.detail_bounds.width - 32, 34 };
    Rectangle admission_list = { split.detail_bounds.x + 16, split.detail_bounds.y + 92, split.detail_bounds.width - 32, split.detail_bounds.height - 108 };

    inpatient_refresh_admission_select(app, st, 1);

    DrawRectangleRounded(split.list_bounds, 0.12f, 8, app->theme.panel);
    DrawRectangleRoundedLinesEx(split.list_bounds, 0.12f, 8, 1.0f, Fade(app->theme.border, 0.85f));
    DrawText("出院办理", (int)lx + 16, (int)ly + 14, 24, app->theme.text_primary);

    Workbench_draw_section_header(app, (int)lx + 16, (int)ly + 52, "出院流程");

    DrawText("步骤 1: 选择住院记录", (int)lx + 16, (int)ly + 86, 17, app->theme.text_secondary);
    DrawText("步骤 2: 填写出院时间", (int)lx + 16, (int)ly + 106, 17, app->theme.text_secondary);
    DrawText("步骤 3: 填写出院摘要", (int)lx + 16, (int)ly + 126, 17, app->theme.text_secondary);
    DrawText("步骤 4: 确认办理出院", (int)lx + 16, (int)ly + 146, 17, app->theme.text_secondary);

    Workbench_draw_section_header(app, (int)lx + 16, (int)ly + 182, "出院信息");

    Workbench_draw_form_label(app, (int)lx + 16, (int)ly + 216, "已选住院记录", 1);
    Workbench_draw_status_badge(app, (Rectangle){ lx + 120, ly + 212, split.list_bounds.width - 136, 28 }, st->admission_id[0] != '\0' ? st->admission_id : "请在右侧搜索并选择住院记录", Workbench_get(USER_ROLE_INPATIENT_REGISTRAR)->accent);

    Workbench_draw_form_label(app, (int)lx + 16, (int)ly + 282, "出院时间", 1);
    {
        static WorkbenchDatePickerState inpatient_discharge_dp;
        static int inpatient_discharge_dp_init = 0;
        if (!inpatient_discharge_dp_init) { WorkbenchDatePickerState_init(&inpatient_discharge_dp); inpatient_discharge_dp_init = 1; }
        Workbench_draw_date_picker(app, (Rectangle){ lx + 16, ly + 304, 200, 34 }, &inpatient_discharge_dp, st->discharged_at, sizeof(st->discharged_at), &st->active_field, 2);
    }

    Workbench_draw_form_label(app, (int)lx + 16, (int)ly + 348, "出院摘要", 0);
    Workbench_draw_text_input((Rectangle){ lx + 16, ly + 370, split.list_bounds.width - 32, 34 }, st->summary, sizeof(st->summary), 1, &st->active_field, 3);

    if (GuiButton((Rectangle){ lx + 16, ly + 424, 160, 36 }, "办理出院")) {
        if (!Workbench_validate_time_format(st->discharged_at)) {
            DesktopAppState_show_message(&app->state, DESKTOP_MESSAGE_ERROR, "出院时间格式无效，请使用 YYYY-MM-DD HH:MM");
        } else {
        result = DesktopAdapters_discharge_patient(
            &app->application,
            st->admission_id, st->discharged_at, st->summary,
            st->output, sizeof(st->output)
        );
        Workbench_show_result(app, result);
        if (result.success) {
            WorkbenchSearchSelectState_mark_dirty(&g_inpatient_admission_select);
            WorkbenchSearchSelectState_mark_dirty(&g_inpatient_bed_select);
            WorkbenchSearchSelectState_mark_dirty(&g_inpatient_ward_select);
        }
        }
    }

    DrawText("住院记录搜索与选择", (int)split.detail_bounds.x + 16, (int)split.detail_bounds.y + 16, 20, app->theme.text_primary);
    Workbench_draw_search_select(app, admission_search, admission_list, &g_inpatient_admission_select, &st->active_field, 44);
    if (g_inpatient_admission_select.active_index >= 0 && g_inpatient_admission_select.active_index < g_inpatient_admission_select.option_count) {
        sscanf(g_inpatient_admission_select.labels[g_inpatient_admission_select.active_index], "%15s", st->admission_id);
    }
}

/* ── Page 3: Query ── */

static void draw_query(DesktopApp *app, Rectangle panel) {
    DesktopInpatientPageState *st = &app->state.inpatient_page;
    WorkbenchListDetailLayout split = Workbench_compute_list_detail_layout(panel, 0.52f, 18.0f, 280.0f);
    float lx = split.list_bounds.x;
    float ly = split.list_bounds.y;
    Result result;
    Rectangle patient_search = { lx + 16, ly + 108, split.list_bounds.width - 32, 34 };
    Rectangle patient_list = { lx + 16, ly + 150, split.list_bounds.width - 32, 96 };
    Rectangle bed_search = { lx + 16, ly + 224, split.list_bounds.width - 32, 34 };
    Rectangle bed_list = { lx + 16, ly + 266, split.list_bounds.width - 32, 96 };

    inpatient_refresh_patient_select(app, st);
    inpatient_refresh_bed_select(app, st, 0);

    DrawRectangleRounded(split.list_bounds, 0.12f, 8, app->theme.panel);
    DrawRectangleRoundedLinesEx(split.list_bounds, 0.12f, 8, 1.0f, Fade(app->theme.border, 0.85f));
    DrawText("住院查询", (int)lx + 16, (int)ly + 14, 24, app->theme.text_primary);

    Workbench_draw_section_header(app, (int)lx + 16, (int)ly + 52, "按患者查询");

    Workbench_draw_form_label(app, (int)lx + 16, (int)ly + 86, "患者搜索与选择", 1);
    Workbench_draw_search_select(app, patient_search, patient_list, &g_inpatient_patient_select, &st->active_field, 45);
    if (g_inpatient_patient_select.active_index >= 0 && g_inpatient_patient_select.active_index < g_inpatient_patient_select.option_count) {
        sscanf(g_inpatient_patient_select.labels[g_inpatient_patient_select.active_index], "%15s", st->query_patient_id);
        result = DesktopAdapters_query_active_admission_by_patient(
            &app->application, st->query_patient_id,
            st->output, sizeof(st->output)
        );
        Workbench_show_result(app, result);
    }

    Workbench_draw_section_header(app, (int)lx + 16, (int)ly + 168, "按床位查询");

    Workbench_draw_form_label(app, (int)lx + 16, (int)ly + 202, "床位搜索与选择", 1);
    Workbench_draw_search_select(app, bed_search, bed_list, &g_inpatient_bed_select, &st->active_field, 46);
    if (g_inpatient_bed_select.active_index >= 0 && g_inpatient_bed_select.active_index < g_inpatient_bed_select.option_count) {
        sscanf(g_inpatient_bed_select.labels[g_inpatient_bed_select.active_index], "%15s", st->query_bed_id);
        result = DesktopAdapters_query_current_patient_by_bed(
            &app->application, st->query_bed_id,
            st->output, sizeof(st->output)
        );
        Workbench_show_result(app, result);
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

    Workbench_draw_output_panel(app, split.detail_bounds, "查询结果", st->output,
                      "在左侧输入患者编号或床位编号进行查询。");
}

/* ── Entry point ── */

static int g_inpatient_initialized = 0;

void InpatientWorkbench_draw(DesktopApp *app, Rectangle panel, int page) {
    if (!g_inpatient_initialized) {
        WorkbenchSearchSelectState_mark_dirty(&g_inpatient_patient_select);
        WorkbenchSearchSelectState_mark_dirty(&g_inpatient_ward_select);
        WorkbenchSearchSelectState_mark_dirty(&g_inpatient_bed_select);
        WorkbenchSearchSelectState_mark_dirty(&g_inpatient_admission_select);
        g_inpatient_initialized = 1;
    }
    switch (page) {
        case 0: draw_home(app, panel); break;
        case 1: draw_admit(app, panel); break;
        case 2: draw_discharge(app, panel); break;
        case 3: draw_query(app, panel); break;
        default: draw_home(app, panel); break;
    }
}
