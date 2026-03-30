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

    Workbench_draw_form_label(app, (int)lx, (int)ly + 52, "药品编号", 1);
    draw_text_input((Rectangle){ lx, ly + 74, 200, 34 }, st->medicine_id, sizeof(st->medicine_id), 1, &st->active_field, 1);

    Workbench_draw_form_label(app, (int)lx + 220, (int)ly + 52, "药品名称", 1);
    draw_text_input((Rectangle){ lx + 220, ly + 74, 200, 34 }, st->medicine_name, sizeof(st->medicine_name), 1, &st->active_field, 2);

    Workbench_draw_form_label(app, (int)lx, (int)ly + 118, "单价", 1);
    draw_text_input((Rectangle){ lx, ly + 140, 120, 34 }, st->price_text, sizeof(st->price_text), 1, &st->active_field, 3);

    Workbench_draw_form_label(app, (int)lx + 140, (int)ly + 118, "库存", 1);
    draw_text_input((Rectangle){ lx + 140, ly + 140, 100, 34 }, st->stock_text, sizeof(st->stock_text), 1, &st->active_field, 4);

    Workbench_draw_form_label(app, (int)lx + 260, (int)ly + 118, "低库存阈值", 1);
    draw_text_input((Rectangle){ lx + 260, ly + 140, 100, 34 }, st->low_stock_text, sizeof(st->low_stock_text), 1, &st->active_field, 5);

    Workbench_draw_form_label(app, (int)lx, (int)ly + 184, "科室编号", 1);
    draw_text_input((Rectangle){ lx, ly + 206, 200, 34 }, st->department_id, sizeof(st->department_id), 1, &st->active_field, 6);

    if (GuiButton((Rectangle){ lx, ly + 256, 160, 36 }, "添加药品")) {
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

    Workbench_draw_form_label(app, (int)lx, (int)ly + 52, "药品编号", 1);
    draw_text_input((Rectangle){ lx, ly + 74, 200, 34 }, st->medicine_id, sizeof(st->medicine_id), 1, &st->active_field, 1);

    Workbench_draw_form_label(app, (int)lx, (int)ly + 118, "入库数量", 1);
    draw_text_input((Rectangle){ lx, ly + 140, 140, 34 }, st->restock_quantity_text, sizeof(st->restock_quantity_text), 1, &st->active_field, 2);

    if (GuiButton((Rectangle){ lx, ly + 190, 160, 36 }, "执行入库")) {
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

    Workbench_draw_form_label(app, (int)lx, (int)ly + 52, "患者编号", 1);
    draw_text_input((Rectangle){ lx, ly + 74, 200, 34 }, st->patient_id, sizeof(st->patient_id), 1, &st->active_field, 1);

    Workbench_draw_form_label(app, (int)lx + 220, (int)ly + 52, "处方编号", 1);
    draw_text_input((Rectangle){ lx + 220, ly + 74, 200, 34 }, st->prescription_id, sizeof(st->prescription_id), 1, &st->active_field, 2);

    Workbench_draw_form_label(app, (int)lx, (int)ly + 118, "药品编号", 1);
    draw_text_input((Rectangle){ lx, ly + 140, 200, 34 }, st->medicine_id, sizeof(st->medicine_id), 1, &st->active_field, 3);

    Workbench_draw_form_label(app, (int)lx + 220, (int)ly + 118, "发药数量", 1);
    draw_text_input((Rectangle){ lx + 220, ly + 140, 100, 34 }, st->dispense_quantity_text, sizeof(st->dispense_quantity_text), 1, &st->active_field, 4);

    Workbench_draw_form_label(app, (int)lx, (int)ly + 184, "药剂师编号", 1);
    draw_text_input((Rectangle){ lx, ly + 206, 200, 34 }, st->pharmacist_id, sizeof(st->pharmacist_id), is_pharmacy ? 0 : 1, &st->active_field, 5);

    Workbench_draw_form_label(app, (int)lx + 220, (int)ly + 184, "发药时间", 0);
    draw_text_input((Rectangle){ lx + 220, ly + 206, 200, 34 }, st->dispensed_at, sizeof(st->dispensed_at), 1, &st->active_field, 6);

    if (GuiButton((Rectangle){ lx, ly + 256, 160, 36 }, "执行发药")) {
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
    int search_clicked = 0;
    Result result;

    DrawRectangleRounded(left, 0.12f, 8, app->theme.panel);
    DrawRectangleRoundedLinesEx(left, 0.12f, 8, 1.0f, Fade(app->theme.border, 0.85f));
    DrawText("库存查询", (int)lx, (int)ly + 14, 24, app->theme.text_primary);

    Workbench_draw_form_label(app, (int)lx, (int)ly + 52, "药品编号", 1);
    Workbench_draw_search_box(
        app,
        (Rectangle){ lx, ly + 74, left.width - 32, 34 },
        st->stock_query_medicine_id, sizeof(st->stock_query_medicine_id),
        &st->active_field, 1,
        &search_clicked
    );

    if (search_clicked) {
        result = DesktopAdapters_query_medicine_stock(
            &app->application, st->stock_query_medicine_id,
            st->output, sizeof(st->output)
        );
        show_result(app, result);
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
        show_result(app, result);
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
