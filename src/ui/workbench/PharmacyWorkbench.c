#include "ui/Workbench.h"
#include "ui/DesktopApp.h"
#include "ui/DesktopAdapters.h"
#include "raygui.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "domain/Medicine.h"

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

    DrawRectangleRounded(left, 0.12f, 8, app->theme.panel);
    DrawRectangleRoundedLinesEx(left, 0.12f, 8, 1.0f, Fade(app->theme.border, 0.85f));
    DrawText("药品建档", (int)lx, (int)ly + 14, 24, app->theme.text_primary);

    GuiLabel((Rectangle){ lx, ly + 52, 120, 20 }, "药品编号");
    draw_text_input((Rectangle){ lx, ly + 74, 200, 28 }, st->medicine_id, sizeof(st->medicine_id), 1, &st->active_field, 1);

    GuiLabel((Rectangle){ lx + 220, ly + 52, 120, 20 }, "药品名称");
    draw_text_input((Rectangle){ lx + 220, ly + 74, 200, 28 }, st->medicine_name, sizeof(st->medicine_name), 1, &st->active_field, 2);

    GuiLabel((Rectangle){ lx, ly + 112, 100, 20 }, "单价");
    draw_text_input((Rectangle){ lx, ly + 134, 120, 28 }, st->price_text, sizeof(st->price_text), 1, &st->active_field, 3);

    GuiLabel((Rectangle){ lx + 140, ly + 112, 100, 20 }, "库存");
    draw_text_input((Rectangle){ lx + 140, ly + 134, 100, 28 }, st->stock_text, sizeof(st->stock_text), 1, &st->active_field, 4);

    GuiLabel((Rectangle){ lx + 260, ly + 112, 100, 20 }, "低库存阈值");
    draw_text_input((Rectangle){ lx + 260, ly + 134, 100, 28 }, st->low_stock_text, sizeof(st->low_stock_text), 1, &st->active_field, 5);

    GuiLabel((Rectangle){ lx, ly + 172, 120, 20 }, "科室编号");
    draw_text_input((Rectangle){ lx, ly + 194, 200, 28 }, st->department_id, sizeof(st->department_id), 1, &st->active_field, 6);

    if (GuiButton((Rectangle){ lx, ly + 240, 160, 34 }, "添加药品")) {
        if (parse_int_text(st->stock_text, &stock) == 0 ||
            parse_int_text(st->low_stock_text, &threshold) == 0) {
            DesktopAppState_show_message(&app->state, DESKTOP_MESSAGE_ERROR, "库存或阈值格式无效");
        } else {
            memset(&medicine, 0, sizeof(medicine));
            strncpy(medicine.medicine_id, st->medicine_id, sizeof(medicine.medicine_id) - 1);
            strncpy(medicine.name, st->medicine_name, sizeof(medicine.name) - 1);
            strncpy(medicine.department_id, st->department_id, sizeof(medicine.department_id) - 1);
            medicine.price = atof(st->price_text);
            medicine.stock = stock;
            medicine.low_stock_threshold = threshold;
            result = DesktopAdapters_add_medicine(&app->application, &medicine, st->output, sizeof(st->output));
            show_result(app, result);
        }
    }

    draw_output_panel(app, right, "建档结果", st->output,
                      "填写左侧表单并点击「添加药品」。");
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

    DrawRectangleRounded(left, 0.12f, 8, app->theme.panel);
    DrawRectangleRoundedLinesEx(left, 0.12f, 8, 1.0f, Fade(app->theme.border, 0.85f));
    DrawText("入库补货", (int)lx, (int)ly + 14, 24, app->theme.text_primary);

    GuiLabel((Rectangle){ lx, ly + 52, 120, 20 }, "药品编号");
    draw_text_input((Rectangle){ lx, ly + 74, 200, 28 }, st->medicine_id, sizeof(st->medicine_id), 1, &st->active_field, 1);

    GuiLabel((Rectangle){ lx, ly + 112, 120, 20 }, "入库数量");
    draw_text_input((Rectangle){ lx, ly + 134, 140, 28 }, st->restock_quantity_text, sizeof(st->restock_quantity_text), 1, &st->active_field, 2);

    if (GuiButton((Rectangle){ lx, ly + 180, 160, 34 }, "执行入库")) {
        if (parse_int_text(st->restock_quantity_text, &restock_qty) == 0) {
            DesktopAppState_show_message(&app->state, DESKTOP_MESSAGE_ERROR, "入库数量格式无效");
        } else {
            result = DesktopAdapters_restock_medicine(
                &app->application, st->medicine_id, restock_qty,
                st->output, sizeof(st->output)
            );
            show_result(app, result);
        }
    }

    draw_output_panel(app, right, "入库结果", st->output,
                      "输入药品编号和数量，点击「执行入库」。");
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

    auto_bind_pharmacist(app);

    DrawRectangleRounded(left, 0.12f, 8, app->theme.panel);
    DrawRectangleRoundedLinesEx(left, 0.12f, 8, 1.0f, Fade(app->theme.border, 0.85f));
    DrawText("发药处理", (int)lx, (int)ly + 14, 24, app->theme.text_primary);

    GuiLabel((Rectangle){ lx, ly + 52, 120, 20 }, "患者编号");
    draw_text_input((Rectangle){ lx, ly + 74, 200, 28 }, st->patient_id, sizeof(st->patient_id), 1, &st->active_field, 1);

    GuiLabel((Rectangle){ lx + 220, ly + 52, 120, 20 }, "处方编号");
    draw_text_input((Rectangle){ lx + 220, ly + 74, 200, 28 }, st->prescription_id, sizeof(st->prescription_id), 1, &st->active_field, 2);

    GuiLabel((Rectangle){ lx, ly + 112, 120, 20 }, "药品编号");
    draw_text_input((Rectangle){ lx, ly + 134, 200, 28 }, st->medicine_id, sizeof(st->medicine_id), 1, &st->active_field, 3);

    GuiLabel((Rectangle){ lx + 220, ly + 112, 120, 20 }, "发药数量");
    draw_text_input((Rectangle){ lx + 220, ly + 134, 100, 28 }, st->dispense_quantity_text, sizeof(st->dispense_quantity_text), 1, &st->active_field, 4);

    GuiLabel((Rectangle){ lx, ly + 172, 120, 20 }, "药剂师编号");
    draw_text_input((Rectangle){ lx, ly + 194, 200, 28 }, st->pharmacist_id, sizeof(st->pharmacist_id), is_pharmacy ? 0 : 1, &st->active_field, 5);

    GuiLabel((Rectangle){ lx + 220, ly + 172, 120, 20 }, "发药时间");
    draw_text_input((Rectangle){ lx + 220, ly + 194, 200, 28 }, st->dispensed_at, sizeof(st->dispensed_at), 1, &st->active_field, 6);

    if (GuiButton((Rectangle){ lx, ly + 240, 160, 34 }, "执行发药")) {
        if (parse_int_text(st->dispense_quantity_text, &quantity) == 0) {
            DesktopAppState_show_message(&app->state, DESKTOP_MESSAGE_ERROR, "发药数量格式无效");
        } else {
            result = DesktopAdapters_dispense_medicine(
                &app->application,
                st->patient_id, st->prescription_id, st->medicine_id,
                quantity, st->pharmacist_id, st->dispensed_at,
                st->output, sizeof(st->output)
            );
            show_result(app, result);
            if (result.success) {
                app->state.dashboard.loaded = 0;
            }
        }
    }

    draw_output_panel(app, right, "发药结果", st->output,
                      "填写左侧表单并点击「执行发药」。");
}

/* ── Page 4: Stock Query ── */

static void draw_stock_query(DesktopApp *app, Rectangle panel) {
    DesktopPharmacyPageState *st = &app->state.pharmacy_page;
    Rectangle left = { panel.x, panel.y, panel.width * 0.52f, panel.height };
    Rectangle right = { panel.x + panel.width * 0.56f, panel.y, panel.width * 0.44f, panel.height };
    float lx = left.x + 16;
    float ly = left.y;
    Result result;

    DrawRectangleRounded(left, 0.12f, 8, app->theme.panel);
    DrawRectangleRoundedLinesEx(left, 0.12f, 8, 1.0f, Fade(app->theme.border, 0.85f));
    DrawText("库存查询", (int)lx, (int)ly + 14, 24, app->theme.text_primary);

    GuiLabel((Rectangle){ lx, ly + 52, 120, 20 }, "药品编号");
    draw_text_input((Rectangle){ lx, ly + 74, 200, 28 }, st->stock_query_medicine_id, sizeof(st->stock_query_medicine_id), 1, &st->active_field, 1);

    if (GuiButton((Rectangle){ lx + 216, ly + 74, 140, 30 }, "查询库存")) {
        result = DesktopAdapters_query_medicine_stock(
            &app->application, st->stock_query_medicine_id,
            st->output, sizeof(st->output)
        );
        show_result(app, result);
    }

    draw_output_panel(app, right, "库存信息", st->output,
                      "输入药品编号并点击「查询库存」。");
}

/* ── Page 5: Low Stock Alert ── */

static void draw_low_stock(DesktopApp *app, Rectangle panel) {
    DesktopPharmacyPageState *st = &app->state.pharmacy_page;
    Result result;

    DrawRectangleRounded(panel, 0.12f, 8, app->theme.panel);
    DrawRectangleRoundedLinesEx(panel, 0.12f, 8, 1.0f, Fade(app->theme.border, 0.85f));
    DrawText("低库存预警", (int)panel.x + 16, (int)panel.y + 14, 24, app->theme.text_primary);

    if (GuiButton((Rectangle){ panel.x + 16, panel.y + 52, 180, 34 }, "检查低库存药品")) {
        result = DesktopAdapters_find_low_stock_medicines(
            &app->application, st->output, sizeof(st->output)
        );
        show_result(app, result);
    }

    if (st->output[0] != '\0') {
        DrawText(st->output, (int)panel.x + 16, (int)panel.y + 100, 17, app->theme.text_secondary);
    } else {
        DrawText("点击「检查低库存药品」查看预警列表。", (int)panel.x + 16, (int)panel.y + 100, 17, app->theme.text_secondary);
    }
}

/* ── Entry point ── */

void PharmacyWorkbench_draw(DesktopApp *app, Rectangle panel, int page) {
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
