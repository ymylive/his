#include "ui/Workbench.h"
#include "ui/DesktopApp.h"
#include "ui/DesktopAdapters.h"
#include "raygui.h"
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "domain/Medicine.h"
#include "domain/VisitRecord.h"
#include "repository/DepartmentRepository.h"
#include "repository/MedicineRepository.h"
#include "repository/VisitRecordRepository.h"

/* ── Local helpers ── */

static WorkbenchSearchSelectState g_pharmacy_department_select;
static WorkbenchSearchSelectState g_pharmacy_medicine_select;
static WorkbenchSearchSelectState g_pharmacy_patient_select;
static WorkbenchSearchSelectState g_pharmacy_visit_select;

static int parse_int_text(const char *text, int *out_value) {
    char *end = 0;
    long value = 0;
    if (text == 0 || out_value == 0 || text[0] == '\0') return 0;
    value = strtol(text, &end, 10);
    if (end == text || end == 0 || *end != '\0') return 0;
    *out_value = (int)value;
    return 1;
}

static void auto_bind_pharmacist(DesktopApp *app) {
    DesktopPharmacyPageState *st = &app->state.pharmacy_page;
    if (app->state.current_user.role == USER_ROLE_PHARMACY && st->pharmacist_id[0] == '\0') {
        strncpy(st->pharmacist_id, app->state.current_user.user_id, sizeof(st->pharmacist_id) - 1);
        st->pharmacist_id[sizeof(st->pharmacist_id) - 1] = '\0';
    }
}

static void pharmacy_refresh_department_select(DesktopApp *app, DesktopPharmacyPageState *st) {
    LinkedList departments;
    const LinkedListNode *current = 0;
    int selected_index = -1;

    if (!WorkbenchSearchSelectState_needs_refresh(&g_pharmacy_department_select)) return;

    WorkbenchSearchSelectState_reset(&g_pharmacy_department_select);
    LinkedList_init(&departments);
    if (DepartmentRepository_load_all(&app->application.doctor_service.department_repository, &departments).success != 1) {
        return;
    }

    current = departments.head;
    while (current != 0) {
        const Department *department = (const Department *)current->data;
        char label[WORKBENCH_SELECT_LABEL_CAPACITY];
        snprintf(label, sizeof(label), "%s | %s | %s", department->department_id, department->name, department->location);
        if (Workbench_text_contains_ignore_case(label, g_pharmacy_department_select.query)) {
            if (strcmp(st->department_id, department->department_id) == 0) {
                selected_index = g_pharmacy_department_select.option_count;
            }
            WorkbenchSearchSelectState_add_option(&g_pharmacy_department_select, label);
        }
        current = current->next;
    }

    WorkbenchSearchSelectState_finalize(&g_pharmacy_department_select);
    g_pharmacy_department_select.active_index = selected_index;
    DepartmentRepository_clear_list(&departments);
}

static void pharmacy_refresh_medicine_select(DesktopApp *app, DesktopPharmacyPageState *st, const char *selected_id) {
    LinkedList medicines;
    const LinkedListNode *current = 0;
    int selected_index = -1;

    if (!WorkbenchSearchSelectState_needs_refresh(&g_pharmacy_medicine_select)) return;

    WorkbenchSearchSelectState_reset(&g_pharmacy_medicine_select);
    LinkedList_init(&medicines);
    if (MedicineRepository_load_all(&app->application.pharmacy_service.medicine_repository, &medicines).success != 1) {
        return;
    }

    current = medicines.head;
    while (current != 0) {
        const Medicine *medicine = (const Medicine *)current->data;
        char label[WORKBENCH_SELECT_LABEL_CAPACITY];
        snprintf(label, sizeof(label), "%s | %s | 库存=%d", medicine->medicine_id, medicine->name, medicine->stock);
        if (Workbench_text_contains_ignore_case(label, g_pharmacy_medicine_select.query)) {
            if (selected_id != 0 && strcmp(selected_id, medicine->medicine_id) == 0) {
                selected_index = g_pharmacy_medicine_select.option_count;
            }
            WorkbenchSearchSelectState_add_option(&g_pharmacy_medicine_select, label);
        }
        current = current->next;
    }

    WorkbenchSearchSelectState_finalize(&g_pharmacy_medicine_select);
    g_pharmacy_medicine_select.active_index = selected_index;
    MedicineRepository_clear_list(&medicines);
}

static void pharmacy_refresh_patient_select(DesktopApp *app, DesktopPharmacyPageState *st) {
    LinkedList patients;
    const LinkedListNode *current = 0;
    int selected_index = -1;
    Result result;

    if (!WorkbenchSearchSelectState_needs_refresh(&g_pharmacy_patient_select)) return;

    WorkbenchSearchSelectState_reset(&g_pharmacy_patient_select);
    LinkedList_init(&patients);
    result = DesktopAdapters_search_patients(&app->application, DESKTOP_PATIENT_SEARCH_BY_NAME, "", &patients);
    if (result.success != 1) {
        return;
    }

    current = patients.head;
    while (current != 0) {
        const Patient *patient = (const Patient *)current->data;
        char label[WORKBENCH_SELECT_LABEL_CAPACITY];
        snprintf(label, sizeof(label), "%s | %s | %d岁", patient->patient_id, patient->name, patient->age);
        if (Workbench_text_contains_ignore_case(label, g_pharmacy_patient_select.query)) {
            if (strcmp(st->patient_id, patient->patient_id) == 0) {
                selected_index = g_pharmacy_patient_select.option_count;
            }
            WorkbenchSearchSelectState_add_option(&g_pharmacy_patient_select, label);
        }
        current = current->next;
    }

    WorkbenchSearchSelectState_finalize(&g_pharmacy_patient_select);
    g_pharmacy_patient_select.active_index = selected_index;
    LinkedList_clear(&patients, free);
}

static void pharmacy_refresh_visit_select(DesktopApp *app, DesktopPharmacyPageState *st) {
    LinkedList visits;
    const LinkedListNode *current = 0;
    int selected_index = -1;

    if (!WorkbenchSearchSelectState_needs_refresh(&g_pharmacy_visit_select)) return;

    WorkbenchSearchSelectState_reset(&g_pharmacy_visit_select);
    LinkedList_init(&visits);
    if (VisitRecordRepository_load_all(&app->application.medical_record_service.visit_repository, &visits).success != 1) {
        return;
    }

    current = visits.head;
    while (current != 0) {
        const VisitRecord *v = (const VisitRecord *)current->data;
        char label[WORKBENCH_SELECT_LABEL_CAPACITY];
        if (v->need_medicine) {
            snprintf(label, sizeof(label), "%s | 患者=%s | %s | %s", v->visit_id, v->patient_id, v->diagnosis, v->visit_time);
            if (Workbench_text_contains_ignore_case(label, g_pharmacy_visit_select.query)) {
                if (strcmp(st->prescription_id, v->visit_id) == 0) {
                    selected_index = g_pharmacy_visit_select.option_count;
                }
                WorkbenchSearchSelectState_add_option(&g_pharmacy_visit_select, label);
            }
        }
        current = current->next;
    }

    WorkbenchSearchSelectState_finalize(&g_pharmacy_visit_select);
    g_pharmacy_visit_select.active_index = selected_index;
    VisitRecordRepository_clear_list(&visits);
}

/* ── Page 0: Home ── */

static void draw_home(DesktopApp *app, Rectangle panel) {
    const WorkbenchDef *wb = Workbench_get(USER_ROLE_PHARMACY);
    const char *labels[4] = { "药品总数", "低库存数", "今日入库", "今日发药" };
    const char *values[4] = { "--", "--", "--", "--" };
    WorkbenchButtonGroupLayout first_row;
    WorkbenchButtonGroupLayout second_row;
    int index = 0;

    auto_bind_pharmacist(app);

    Workbench_draw_home_cards(app, panel, labels, values, wb->accent);

    Workbench_draw_section_header(app, (int)panel.x, (int)panel.y + 110, "快捷操作");
    first_row = Workbench_compute_button_group_layout(
        (Rectangle){ panel.x, panel.y + 148.0f, panel.width, 40.0f },
        4,
        130.0f,
        40.0f,
        14.0f
    );
    for (index = 0; index < first_row.count; index++) {
        int clicked = 0;
        const char *label = index == 0 ? "药品建档" :
                            index == 1 ? "入库补货" :
                            index == 2 ? "发药处理" :
                                         "库存查询";
        Workbench_draw_quick_action_btn(app, first_row.buttons[index], label, wb->accent, &clicked);
        if (clicked) {
            app->state.workbench_page = index + 1;
        }
    }
    second_row = Workbench_compute_button_group_layout(
        (Rectangle){ panel.x, panel.y + 202.0f, panel.width * 0.38f, 40.0f },
        1,
        160.0f,
        40.0f,
        0.0f
    );
    for (index = 0; index < second_row.count; index++) {
        int clicked = 0;
        Workbench_draw_quick_action_btn(app, second_row.buttons[index], "低库存预警", wb->accent, &clicked);
        if (clicked) {
            app->state.workbench_page = 5;
        }
    }
}

/* ── Page 1: Add Medicine ── */

static void draw_add_medicine(DesktopApp *app, Rectangle panel) {
    DesktopPharmacyPageState *st = &app->state.pharmacy_page;
    Rectangle left = { panel.x, panel.y, panel.width * 0.52f, panel.height };
    Rectangle right = { panel.x + panel.width * 0.56f, panel.y, panel.width * 0.44f, panel.height };
    float lx = left.x + 16;
    float ly = left.y;
    Medicine medicine;
    int stock = 0;
    int threshold = 0;
    Result result;
    Rectangle department_search = { right.x + 14, right.y + 56, right.width - 28, 34 };
    Rectangle department_list = { right.x + 14, right.y + 98, right.width - 28, right.height - 114 };

    pharmacy_refresh_department_select(app, st);

    DrawRectangleRounded(left, 0.12f, 8, app->theme.panel);
    DrawRectangleRoundedLinesEx(left, 0.12f, 8, 1.0f, Fade(app->theme.border, 0.85f));
    DrawText("药品建档", (int)lx, (int)ly + 14, 24, app->theme.text_primary);

    Workbench_draw_form_label(app, (int)lx, (int)ly + 52, "药品编号", 0);
    DrawText("新增时自动生成", (int)lx, (int)ly + 78, 16, app->theme.text_secondary);

    Workbench_draw_form_label(app, (int)lx + 220, (int)ly + 52, "药品名称", 1);
    Workbench_draw_text_input((Rectangle){ lx + 220, ly + 74, 200, 34 }, st->medicine_name, sizeof(st->medicine_name), 1, &st->active_field, 1);

    Workbench_draw_form_label(app, (int)lx, (int)ly + 118, "单价", 1);
    Workbench_draw_text_input((Rectangle){ lx, ly + 140, 120, 34 }, st->price_text, sizeof(st->price_text), 1, &st->active_field, 2);

    Workbench_draw_form_label(app, (int)lx + 140, (int)ly + 118, "库存", 1);
    Workbench_draw_text_input((Rectangle){ lx + 140, ly + 140, 100, 34 }, st->stock_text, sizeof(st->stock_text), 1, &st->active_field, 3);

    Workbench_draw_form_label(app, (int)lx + 260, (int)ly + 118, "低库存阈值", 1);
    Workbench_draw_text_input((Rectangle){ lx + 260, ly + 140, 100, 34 }, st->low_stock_text, sizeof(st->low_stock_text), 1, &st->active_field, 4);

    Workbench_draw_form_label(app, (int)lx, (int)ly + 184, "已选科室", 1);
    Workbench_draw_status_badge(app, (Rectangle){ lx + 120, ly + 180, left.width - 152, 28 }, st->department_id[0] != '\0' ? st->department_id : "请在右侧搜索并选择科室", Workbench_get(USER_ROLE_PHARMACY)->accent);

    if (GuiButton((Rectangle){ lx, ly + 256, 160, 36 }, "添加药品")) {
        if (parse_int_text(st->stock_text, &stock) == 0 ||
            parse_int_text(st->low_stock_text, &threshold) == 0) {
            DesktopAppState_show_message(&app->state, DESKTOP_MESSAGE_ERROR, "库存或阈值格式无效");
        } else {
            memset(&medicine, 0, sizeof(medicine));
            strncpy(medicine.name, st->medicine_name, sizeof(medicine.name) - 1);
            strncpy(medicine.department_id, st->department_id, sizeof(medicine.department_id) - 1);
            medicine.price = atof(st->price_text);
            medicine.stock = stock;
            medicine.low_stock_threshold = threshold;
            result = DesktopAdapters_add_medicine(&app->application, &medicine, st->output, sizeof(st->output));
            Workbench_show_result(app, result);
            if (result.success) {
                WorkbenchSearchSelectState_mark_dirty(&g_pharmacy_medicine_select);
            }
        }
    }

    DrawText("科室搜索与选择", (int)right.x + 14, (int)right.y + 16, 20, app->theme.text_primary);
    Workbench_draw_search_select(app, department_search, department_list, &g_pharmacy_department_select, &st->active_field, 61);
    if (g_pharmacy_department_select.active_index >= 0 && g_pharmacy_department_select.active_index < g_pharmacy_department_select.option_count) {
        sscanf(g_pharmacy_department_select.labels[g_pharmacy_department_select.active_index], "%15s", st->department_id);
    }
}

/* ── Page 2: Restock ── */

static void draw_restock(DesktopApp *app, Rectangle panel) {
    DesktopPharmacyPageState *st = &app->state.pharmacy_page;
    Rectangle left = { panel.x, panel.y, panel.width * 0.52f, panel.height };
    Rectangle right = { panel.x + panel.width * 0.56f, panel.y, panel.width * 0.44f, panel.height };
    float lx = left.x + 16;
    float ly = left.y;
    int restock_qty = 0;
    Result result;
    Rectangle medicine_search = { right.x + 14, right.y + 56, right.width - 28, 34 };
    Rectangle medicine_list = { right.x + 14, right.y + 98, right.width - 28, right.height - 114 };

    pharmacy_refresh_medicine_select(app, st, st->medicine_id);

    DrawRectangleRounded(left, 0.12f, 8, app->theme.panel);
    DrawRectangleRoundedLinesEx(left, 0.12f, 8, 1.0f, Fade(app->theme.border, 0.85f));
    DrawText("入库补货", (int)lx, (int)ly + 14, 24, app->theme.text_primary);

    Workbench_draw_form_label(app, (int)lx, (int)ly + 52, "已选药品", 1);
    Workbench_draw_status_badge(app, (Rectangle){ lx + 120, ly + 48, left.width - 152, 28 }, st->medicine_id[0] != '\0' ? st->medicine_id : "请在右侧搜索并选择药品", Workbench_get(USER_ROLE_PHARMACY)->accent);

    Workbench_draw_form_label(app, (int)lx, (int)ly + 118, "入库数量", 1);
    Workbench_draw_text_input((Rectangle){ lx, ly + 140, 140, 34 }, st->restock_quantity_text, sizeof(st->restock_quantity_text), 1, &st->active_field, 1);

    if (GuiButton((Rectangle){ lx, ly + 190, 160, 36 }, "执行入库")) {
        if (parse_int_text(st->restock_quantity_text, &restock_qty) == 0) {
            DesktopAppState_show_message(&app->state, DESKTOP_MESSAGE_ERROR, "入库数量格式无效");
        } else {
            result = DesktopAdapters_restock_medicine(
                &app->application, st->medicine_id, restock_qty,
                st->output, sizeof(st->output)
            );
            Workbench_show_result(app, result);
            if (result.success) {
                WorkbenchSearchSelectState_mark_dirty(&g_pharmacy_medicine_select);
            }
        }
    }

    DrawText("药品搜索与选择", (int)right.x + 14, (int)right.y + 16, 20, app->theme.text_primary);
    Workbench_draw_search_select(app, medicine_search, medicine_list, &g_pharmacy_medicine_select, &st->active_field, 62);
    if (g_pharmacy_medicine_select.active_index >= 0 && g_pharmacy_medicine_select.active_index < g_pharmacy_medicine_select.option_count) {
        sscanf(g_pharmacy_medicine_select.labels[g_pharmacy_medicine_select.active_index], "%15s", st->medicine_id);
        sscanf(g_pharmacy_medicine_select.labels[g_pharmacy_medicine_select.active_index], "%15s", st->stock_query_medicine_id);
    }
}

/* ── Page 3: Dispense ── */

static void draw_dispense(DesktopApp *app, Rectangle panel) {
    DesktopPharmacyPageState *st = &app->state.pharmacy_page;
    Rectangle left = { panel.x, panel.y, panel.width * 0.52f, panel.height };
    Rectangle right = { panel.x + panel.width * 0.56f, panel.y, panel.width * 0.44f, panel.height };
    float lx = left.x + 16;
    float ly = left.y;
    int is_pharmacy = (app->state.current_user.role == USER_ROLE_PHARMACY);
    int quantity = 0;
    Result result;
    float section_height = right.height * 0.30f;
    float gap = right.height * 0.035f;
    float sec0_y = right.y;
    float sec1_y = sec0_y + 40 + section_height + gap;
    float sec2_y = sec1_y + 40 + section_height + gap;
    Rectangle patient_search = { right.x + 14, sec0_y + 32, right.width - 28, 34 };
    Rectangle patient_list = { right.x + 14, sec0_y + 72, right.width - 28, section_height - 32 };
    Rectangle visit_search = { right.x + 14, sec1_y + 32, right.width - 28, 34 };
    Rectangle visit_list = { right.x + 14, sec1_y + 72, right.width - 28, section_height - 32 };
    Rectangle medicine_search = { right.x + 14, sec2_y + 32, right.width - 28, 34 };
    Rectangle medicine_list = { right.x + 14, sec2_y + 72, right.width - 28, section_height - 32 };

    auto_bind_pharmacist(app);
    pharmacy_refresh_patient_select(app, st);
    pharmacy_refresh_visit_select(app, st);
    pharmacy_refresh_medicine_select(app, st, st->medicine_id);

    DrawRectangleRounded(left, 0.12f, 8, app->theme.panel);
    DrawRectangleRoundedLinesEx(left, 0.12f, 8, 1.0f, Fade(app->theme.border, 0.85f));
    DrawText("发药处理", (int)lx, (int)ly + 14, 24, app->theme.text_primary);

    Workbench_draw_form_label(app, (int)lx, (int)ly + 52, "已选患者", 1);
    Workbench_draw_status_badge(app, (Rectangle){ lx + 120, ly + 48, left.width - 152, 28 }, st->patient_id[0] != '\0' ? st->patient_id : "请在右侧搜索并选择患者", Workbench_get(USER_ROLE_PHARMACY)->accent);

    Workbench_draw_form_label(app, (int)lx, (int)ly + 86, "处方编号", 1);
    Workbench_draw_status_badge(app, (Rectangle){ lx + 120, ly + 82, left.width - 152, 28 }, st->prescription_id[0] != '\0' ? st->prescription_id : "请在右侧搜索并选择处方", Workbench_get(USER_ROLE_PHARMACY)->accent);

    Workbench_draw_form_label(app, (int)lx, (int)ly + 118, "已选药品", 1);
    Workbench_draw_status_badge(app, (Rectangle){ lx + 120, ly + 114, left.width - 152, 28 }, st->medicine_id[0] != '\0' ? st->medicine_id : "请在右侧搜索并选择药品", Workbench_get(USER_ROLE_PHARMACY)->accent);

    Workbench_draw_form_label(app, (int)lx + 220, (int)ly + 118, "发药数量", 1);
    Workbench_draw_text_input((Rectangle){ lx + 220, ly + 140, 100, 34 }, st->dispense_quantity_text, sizeof(st->dispense_quantity_text), 1, &st->active_field, 1);

    Workbench_draw_form_label(app, (int)lx, (int)ly + 184, "药剂师编号", 1);
    Workbench_draw_text_input((Rectangle){ lx, ly + 206, 200, 34 }, st->pharmacist_id, sizeof(st->pharmacist_id), is_pharmacy ? 0 : 1, &st->active_field, 2);

    Workbench_draw_form_label(app, (int)lx + 220, (int)ly + 184, "发药时间", 0);
    {
        static WorkbenchDatePickerState pharmacy_dispensed_dp;
        static int pharmacy_dispensed_dp_init = 0;
        if (!pharmacy_dispensed_dp_init) { WorkbenchDatePickerState_init(&pharmacy_dispensed_dp); pharmacy_dispensed_dp_init = 1; }
        Workbench_draw_date_picker(app, (Rectangle){ lx + 220, ly + 206, 200, 34 }, &pharmacy_dispensed_dp, st->dispensed_at, sizeof(st->dispensed_at), &st->active_field, 3);
    }

    if (GuiButton((Rectangle){ lx, ly + 256, 160, 36 }, "执行发药")) {
        if (parse_int_text(st->dispense_quantity_text, &quantity) == 0) {
            DesktopAppState_show_message(&app->state, DESKTOP_MESSAGE_ERROR, "发药数量格式无效");
        } else if (!Workbench_validate_time_format(st->dispensed_at)) {
            DesktopAppState_show_message(&app->state, DESKTOP_MESSAGE_ERROR, "发药时间格式无效，请使用 YYYY-MM-DD HH:MM");
        } else {
            result = DesktopAdapters_dispense_medicine(
                &app->application,
                st->patient_id, st->prescription_id, st->medicine_id,
                quantity, st->pharmacist_id, st->dispensed_at,
                st->output, sizeof(st->output)
            );
            Workbench_show_result(app, result);
            if (result.success) {
                app->state.dashboard.loaded = 0;
                WorkbenchSearchSelectState_mark_dirty(&g_pharmacy_patient_select);
                WorkbenchSearchSelectState_mark_dirty(&g_pharmacy_visit_select);
                WorkbenchSearchSelectState_mark_dirty(&g_pharmacy_medicine_select);
            }
        }
    }

    DrawText("患者搜索与选择", (int)right.x + 14, (int)sec0_y + 8, 20, app->theme.text_primary);
    Workbench_draw_search_select(app, patient_search, patient_list, &g_pharmacy_patient_select, &st->active_field, 63);
    if (g_pharmacy_patient_select.active_index >= 0 && g_pharmacy_patient_select.active_index < g_pharmacy_patient_select.option_count) {
        sscanf(g_pharmacy_patient_select.labels[g_pharmacy_patient_select.active_index], "%15s", st->patient_id);
    }

    DrawText("处方搜索与选择", (int)right.x + 14, (int)sec1_y + 8, 20, app->theme.text_primary);
    Workbench_draw_search_select(app, visit_search, visit_list, &g_pharmacy_visit_select, &st->active_field, 65);
    if (g_pharmacy_visit_select.active_index >= 0 && g_pharmacy_visit_select.active_index < g_pharmacy_visit_select.option_count) {
        sscanf(g_pharmacy_visit_select.labels[g_pharmacy_visit_select.active_index], "%15s", st->prescription_id);
    }

    DrawText("药品搜索与选择", (int)right.x + 14, (int)sec2_y + 8, 20, app->theme.text_primary);
    Workbench_draw_search_select(app, medicine_search, medicine_list, &g_pharmacy_medicine_select, &st->active_field, 66);
    if (g_pharmacy_medicine_select.active_index >= 0 && g_pharmacy_medicine_select.active_index < g_pharmacy_medicine_select.option_count) {
        sscanf(g_pharmacy_medicine_select.labels[g_pharmacy_medicine_select.active_index], "%15s", st->medicine_id);
    }
}

/* ── Page 4: Stock Query ── */

static void draw_stock_query(DesktopApp *app, Rectangle panel) {
    DesktopPharmacyPageState *st = &app->state.pharmacy_page;
    Rectangle left = { panel.x, panel.y, panel.width * 0.52f, panel.height };
    Rectangle right = { panel.x + panel.width * 0.56f, panel.y, panel.width * 0.44f, panel.height };
    float lx = left.x + 16;
    float ly = left.y;
    Result result;
    Rectangle search_rect = { lx, ly + 74, left.width - 32, 34 };
    Rectangle list_rect = { lx, ly + 118, left.width - 32, left.height - 134 };

    pharmacy_refresh_medicine_select(app, st, st->stock_query_medicine_id);

    DrawRectangleRounded(left, 0.12f, 8, app->theme.panel);
    DrawRectangleRoundedLinesEx(left, 0.12f, 8, 1.0f, Fade(app->theme.border, 0.85f));
    DrawText("库存查询", (int)lx, (int)ly + 14, 24, app->theme.text_primary);

    Workbench_draw_form_label(app, (int)lx, (int)ly + 52, "药品搜索与选择", 1);
    Workbench_draw_search_select(app, search_rect, list_rect, &g_pharmacy_medicine_select, &st->active_field, 65);
    if (g_pharmacy_medicine_select.active_index >= 0 && g_pharmacy_medicine_select.active_index < g_pharmacy_medicine_select.option_count) {
        sscanf(g_pharmacy_medicine_select.labels[g_pharmacy_medicine_select.active_index], "%15s", st->stock_query_medicine_id);
        result = DesktopAdapters_query_medicine_stock(
            &app->application, st->stock_query_medicine_id,
            st->output, sizeof(st->output)
        );
        Workbench_show_result(app, result);
    }

    DrawRectangleRounded(right, 0.12f, 8, app->theme.panel);
    DrawRectangleRoundedLinesEx(right, 0.12f, 8, 1.0f, Fade(app->theme.border, 0.85f));
    DrawText("库存信息", (int)right.x + 14, (int)right.y + 12, 20, app->theme.text_primary);

    if (st->output[0] != '\0') {
        float detail_y = right.y + 56;
        DrawText(st->output, (int)right.x + 14, (int)detail_y, 17, app->theme.text_secondary);

        /* Parse stock info and display visual indicators if possible */
        /* Example: if output contains stock numbers, show progress bar */
        char *stock_str = strstr(st->output, "库存:");
        if (stock_str != 0) {
            int current_stock = 0;
            int threshold = 0;
            if (sscanf(stock_str, "库存:%d", &current_stock) == 1) {
                char *threshold_str = strstr(st->output, "阈值:");
                if (threshold_str != 0 && sscanf(threshold_str, "阈值:%d", &threshold) == 1 && threshold > 0) {
                    float progress = (float)current_stock / (float)(threshold * 3);
                    if (progress > 1.0f) progress = 1.0f;

                    Color bar_color;
                    const char *status_text;
                    Color status_color;

                    if (current_stock <= threshold) {
                        bar_color = (Color){ 239, 68, 68, 255 };
                        status_text = "缺货";
                        status_color = (Color){ 239, 68, 68, 255 };
                    } else if (current_stock <= threshold * 2) {
                        bar_color = (Color){ 245, 158, 11, 255 };
                        status_text = "低库存";
                        status_color = (Color){ 245, 158, 11, 255 };
                    } else {
                        bar_color = (Color){ 34, 197, 94, 255 };
                        status_text = "充足";
                        status_color = (Color){ 34, 197, 94, 255 };
                    }

                    detail_y += 120;
                    DrawText("库存状态", (int)right.x + 14, (int)detail_y, 18, app->theme.text_secondary);
                    detail_y += 28;

                    Rectangle status_badge = { right.x + 14, detail_y, 80, 26 };
                    Workbench_draw_status_badge(app, status_badge, status_text, status_color);

                    detail_y += 40;
                    DrawText("库存水平", (int)right.x + 14, (int)detail_y, 18, app->theme.text_secondary);
                    detail_y += 28;

                    Rectangle progress_rect = { right.x + 14, detail_y, right.width - 28, 24 };
                    Workbench_draw_progress_bar(app, progress_rect, progress, bar_color);
                }
            }
        }
    } else {
        DrawText("输入药品编号并点击搜索按钮。", (int)right.x + 14, (int)right.y + 56, 17, app->theme.text_secondary);
    }
}

/* ── Page 5: Low Stock Alert ── */

static void draw_low_stock(DesktopApp *app, Rectangle panel) {
    DesktopPharmacyPageState *st = &app->state.pharmacy_page;
    const WorkbenchDef *wb = Workbench_get(USER_ROLE_PHARMACY);
    Result result;
    float y_offset = panel.y + 14;

    DrawRectangleRounded(panel, 0.12f, 8, app->theme.panel);
    DrawRectangleRoundedLinesEx(panel, 0.12f, 8, 1.0f, Fade(app->theme.border, 0.85f));

    /* Header with warning badge */
    DrawText("低库存预警", (int)panel.x + 16, (int)y_offset, 24, app->theme.text_primary);
    Rectangle warning_badge = { panel.x + 180, y_offset + 2, 80, 26 };
    Workbench_draw_status_badge(app, warning_badge, "警告", (Color){ 239, 68, 68, 255 });

    y_offset += 48;

    if (GuiButton((Rectangle){ panel.x + 16, y_offset, 180, 36 }, "检查低库存药品")) {
        result = DesktopAdapters_find_low_stock_medicines(
            &app->application, st->output, sizeof(st->output)
        );
        Workbench_show_result(app, result);
    }

    y_offset += 56;

    if (st->output[0] != '\0') {
        /* Display alert section header */
        Workbench_draw_section_header(app, (int)panel.x + 16, (int)y_offset, "预警列表");
        y_offset += 40;

        /* Parse and display low stock items with visual indicators */
        char *line = st->output;
        char *next_line = 0;
        int item_count = 0;
        const int max_items = 8;

        while (line != 0 && item_count < max_items && y_offset + 80 < panel.y + panel.height) {
            next_line = strchr(line, '\n');
            if (next_line != 0) {
                *next_line = '\0';
            }

            /* Skip empty lines */
            if (line[0] != '\0' && strlen(line) > 3) {
                /* Draw item background */
                Rectangle item_rect = { panel.x + 16, y_offset, panel.width - 32, 70 };
                DrawRectangleRounded(item_rect, 0.1f, 8, Fade(app->theme.border, 0.15f));
                DrawRectangleRoundedLinesEx(item_rect, 0.1f, 8, 1.0f, Fade((Color){ 239, 68, 68, 255 }, 0.3f));

                /* Draw item text */
                DrawText(line, (int)item_rect.x + 12, (int)item_rect.y + 10, 17, app->theme.text_primary);

                /* Try to parse stock numbers for progress bar */
                int current_stock = 0;
                int threshold = 0;
                char *stock_str = strstr(line, "库存:");
                char *threshold_str = strstr(line, "阈值:");

                if (stock_str != 0 && threshold_str != 0) {
                    if (sscanf(stock_str, "库存:%d", &current_stock) == 1 &&
                        sscanf(threshold_str, "阈值:%d", &threshold) == 1 && threshold > 0) {

                        float progress = (float)current_stock / (float)threshold;
                        if (progress > 1.0f) progress = 1.0f;

                        Color bar_color;
                        const char *status_text;
                        Color status_color;

                        if (current_stock == 0) {
                            bar_color = (Color){ 239, 68, 68, 255 };
                            status_text = "缺货";
                            status_color = (Color){ 239, 68, 68, 255 };
                        } else if (progress <= 0.5f) {
                            bar_color = (Color){ 239, 68, 68, 255 };
                            status_text = "严重";
                            status_color = (Color){ 239, 68, 68, 255 };
                        } else {
                            bar_color = (Color){ 245, 158, 11, 255 };
                            status_text = "低库存";
                            status_color = (Color){ 245, 158, 11, 255 };
                        }

                        /* Status badge */
                        Rectangle badge_rect = { item_rect.x + item_rect.width - 90, item_rect.y + 10, 76, 24 };
                        Workbench_draw_status_badge(app, badge_rect, status_text, status_color);

                        /* Progress bar */
                        Rectangle progress_rect = { item_rect.x + 12, item_rect.y + 42, item_rect.width - 24, 18 };
                        Workbench_draw_progress_bar(app, progress_rect, progress, bar_color);
                    }
                }

                y_offset += 80;
                item_count++;
            }

            if (next_line != 0) {
                *next_line = '\n';
                line = next_line + 1;
            } else {
                line = 0;
            }
        }

        if (item_count == 0) {
            DrawText("未找到低库存药品。", (int)panel.x + 16, (int)y_offset, 17, app->theme.text_secondary);
        }
    } else {
        DrawText("点击「检查低库存药品」查看预警列表。", (int)panel.x + 16, (int)y_offset, 17, app->theme.text_secondary);
    }
}

/* ── Entry point ── */

void PharmacyWorkbench_draw(DesktopApp *app, Rectangle panel, int page) {
    static int initialized = 0;
    if (!initialized) {
        WorkbenchSearchSelectState_mark_dirty(&g_pharmacy_department_select);
        WorkbenchSearchSelectState_mark_dirty(&g_pharmacy_medicine_select);
        WorkbenchSearchSelectState_mark_dirty(&g_pharmacy_patient_select);
        WorkbenchSearchSelectState_mark_dirty(&g_pharmacy_visit_select);
        initialized = 1;
    }
    switch (page) {
        case 0: draw_home(app, panel); break;
        case 1: draw_add_medicine(app, panel); break;
        case 2: draw_restock(app, panel); break;
        case 3: draw_dispense(app, panel); break;
        case 4: draw_stock_query(app, panel); break;
        case 5: draw_low_stock(app, panel); break;
        default: draw_home(app, panel); break;
    }
}
