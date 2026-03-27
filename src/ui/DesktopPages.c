#include "ui/DesktopPages.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>

#define RAYGUI_IMPLEMENTATION
#include "raygui.h"

#include "ui/DesktopAdapters.h"
#include "ui/Workbench.h"

static const char *DESKTOP_ROLE_OPTIONS =
    "患者;医生;系统管理员;挂号员;住院登记员;病区管理员;药房人员";

static float DesktopPages_maxf(float a, float b) {
    return a > b ? a : b;
}

static float DesktopPages_minf(float a, float b) {
    return a < b ? a : b;
}

static float DesktopPages_clampf(float value, float min_value, float max_value) {
    return DesktopPages_maxf(min_value, DesktopPages_minf(value, max_value));
}

static const char *DesktopPages_role_name_from_index(int index) {
    return DesktopApp_user_role_label(DesktopApp_role_from_index(index));
}

static int DesktopPages_wrap_role_index(int index) {
    if (index < 0) {
        return 6;
    }

    if (index > 6) {
        return 0;
    }

    return index;
}

static int DesktopPages_page_visible_for_role(UserRole role, DesktopPage page) {
    if (role == USER_ROLE_ADMIN) {
        return 1;
    }

    switch (page) {
        case DESKTOP_PAGE_DASHBOARD:
            return 1;
        case DESKTOP_PAGE_PATIENTS:
            return 1;
        case DESKTOP_PAGE_DOCTOR:
            return role == USER_ROLE_DOCTOR;
        case DESKTOP_PAGE_INPATIENT:
            return role == USER_ROLE_INPATIENT_REGISTRAR || role == USER_ROLE_WARD_MANAGER;
        case DESKTOP_PAGE_PHARMACY:
            return role == USER_ROLE_PHARMACY;
        case DESKTOP_PAGE_DISPENSE:
            return role == USER_ROLE_PATIENT || role == USER_ROLE_PHARMACY;
        case DESKTOP_PAGE_REGISTRATION:
            return role == USER_ROLE_REGISTRATION_CLERK;
        case DESKTOP_PAGE_SYSTEM:
            return 0;
        default:
            return 0;
    }
}

static void DesktopPages_draw_empty_state(
    const DesktopApp *app,
    Rectangle rect,
    const char *title,
    const char *description
) {
    DrawRectangleRounded(rect, 0.12f, 8, Fade(app->theme.panel_alt, 0.88f));
    DrawRectangleRoundedLinesEx(rect, 0.12f, 8, 1.0f, Fade(app->theme.border, 0.85f));
    DrawText(title, (int)rect.x + 18, (int)rect.y + 16, 20, app->theme.text_primary);
    DrawText(description, (int)rect.x + 18, (int)rect.y + 46, 18, app->theme.text_secondary);
}

static Rectangle DesktopPages_main_panel(const DesktopApp *app) {
    return (Rectangle){
        (float)(app->theme.sidebar_width + app->theme.margin),
        (float)(app->theme.topbar_height + app->theme.margin),
        (float)(GetScreenWidth() - app->theme.sidebar_width - app->theme.margin * 2),
        (float)(GetScreenHeight() - app->theme.topbar_height - app->theme.margin * 2)
    };
}

DesktopTopbarLayout DesktopPages_compute_topbar_layout(
    int screen_width,
    const DesktopTheme *theme,
    float title_width,
    float session_width,
    float time_width
) {
    DesktopTopbarLayout layout;
    float padding = 24.0f;
    float topbar_height = 84.0f;
    float gap = 18.0f;
    float title_x = 24.0f;
    float title_max_width = title_width > 0.0f ? title_width : 280.0f;
    float session_max_width = session_width > 0.0f ? session_width : 360.0f;
    float time_actual_width = time_width > 0.0f ? time_width : 148.0f;
    float logout_width = 88.0f;
    float logout_height = 34.0f;
    float title_min_width = 180.0f;
    float title_available = 0.0f;
    float session_x = 0.0f;
    float session_available = 0.0f;

    memset(&layout, 0, sizeof(layout));
    if (theme != 0) {
        padding = (float)(theme->margin > 0 ? theme->margin : 24);
        topbar_height = (float)(theme->topbar_height > 0 ? theme->topbar_height : 84);
        gap = (float)(theme->spacing > 0 ? theme->spacing : 18);
    }

    layout.gap = gap;
    layout.bar_bounds = (Rectangle){ 0.0f, 0.0f, (float)screen_width, topbar_height };
    layout.logout_bounds = (Rectangle){
        (float)screen_width - padding - logout_width,
        (topbar_height - logout_height) * 0.5f,
        logout_width,
        logout_height
    };
    layout.time_bounds = (Rectangle){
        layout.logout_bounds.x - gap - time_actual_width,
        (topbar_height - 18.0f) * 0.5f,
        time_actual_width,
        18.0f
    };

    title_available = layout.time_bounds.x - gap - title_x - 120.0f;
    if (title_available < title_min_width) {
        title_available = title_min_width;
    }
    title_max_width = DesktopPages_minf(title_max_width, title_available);
    layout.title_bounds = (Rectangle){
        title_x,
        (topbar_height - 28.0f) * 0.5f,
        title_max_width,
        28.0f
    };

    session_x = layout.title_bounds.x + layout.title_bounds.width + gap;
    session_available = layout.time_bounds.x - gap - session_x;
    if (session_available < 0.0f) {
        session_available = 0.0f;
    }
    layout.session_bounds = (Rectangle){
        session_x,
        (topbar_height - 18.0f) * 0.5f,
        DesktopPages_minf(session_max_width, session_available),
        18.0f
    };

    return layout;
}

DesktopLoginLayout DesktopPages_compute_login_layout(
    int screen_width,
    int screen_height,
    const DesktopTheme *theme
) {
    DesktopLoginLayout layout;
    float margin = 32.0f;
    float gap = 18.0f;
    float card_width = 620.0f;
    float padding = 36.0f;
    float field_height = 44.0f;
    float role_button_width = 42.0f;
    float card_x = 0.0f;
    float card_y = 0.0f;
    float user_y = 0.0f;
    float password_y = 0.0f;
    float role_y = 0.0f;
    float login_y = 0.0f;
    float card_height = 0.0f;
    float role_value_width = 0.0f;

    memset(&layout, 0, sizeof(layout));
    if (theme != 0) {
        margin = (float)(theme->margin > 0 ? theme->margin : 32);
        gap = (float)(theme->spacing > 0 ? theme->spacing : 18);
    }

    card_width = DesktopPages_clampf((float)screen_width - margin * 2.0f, 460.0f, 620.0f);
    padding = screen_width <= 1440 ? 32.0f : 36.0f;
    user_y = 132.0f;
    password_y = user_y + field_height + 38.0f;
    role_y = password_y + field_height + 38.0f;
    login_y = role_y + field_height + 32.0f;
    card_height = login_y + 48.0f + 36.0f;
    card_height = DesktopPages_clampf(card_height, 440.0f, (float)screen_height - margin * 2.0f);
    card_x = ((float)screen_width - card_width) * 0.5f;
    card_y = ((float)screen_height - card_height) * 0.5f;
    role_value_width = card_width - padding * 2.0f - role_button_width * 2.0f - gap * 2.0f;

    layout.gap = gap;
    layout.card_bounds = (Rectangle){ card_x, card_y, card_width, card_height };
    layout.user_box = (Rectangle){ card_x + padding, card_y + user_y, card_width - padding * 2.0f, field_height };
    layout.password_box = (Rectangle){ card_x + padding, card_y + password_y, card_width - padding * 2.0f, field_height };
    layout.role_prev_bounds = (Rectangle){ card_x + padding, card_y + role_y, role_button_width, field_height };
    layout.role_value_bounds = (Rectangle){
        layout.role_prev_bounds.x + layout.role_prev_bounds.width + gap,
        card_y + role_y,
        role_value_width,
        field_height
    };
    layout.role_next_bounds = (Rectangle){
        layout.role_value_bounds.x + layout.role_value_bounds.width + gap,
        card_y + role_y,
        role_button_width,
        field_height
    };
    layout.login_button_bounds = (Rectangle){
        card_x + padding,
        card_y + login_y,
        card_width - padding * 2.0f,
        48.0f
    };
    return layout;
}

static void DesktopPages_draw_card(
    const DesktopApp *app,
    Rectangle rect,
    const char *title,
    int value,
    Color accent
) {
    DrawRectangleRounded(rect, 0.16f, 8, app->theme.panel);
    DrawRectangleRoundedLinesEx(rect, 0.16f, 8, 1.0f, app->theme.border);
    DrawRectangle((int)rect.x, (int)rect.y, 6, (int)rect.height, accent);
    DrawText(title, (int)rect.x + 20, (int)rect.y + 16, 20, app->theme.text_secondary);
    DrawText(TextFormat("%d", value), (int)rect.x + 20, (int)rect.y + 50, 34, app->theme.text_primary);
}

static void DesktopPages_show_result_message(DesktopApp *app, const Result result) {
    DesktopMessageKind kind = result.success ? DESKTOP_MESSAGE_SUCCESS : DESKTOP_MESSAGE_ERROR;
    DesktopAppState_show_message(&app->state, kind, result.message);
}

static void DesktopPages_draw_message_bar(DesktopApp *app) {
    Rectangle rect;
    Color background;

    if (app->state.message.visible == 0) {
        return;
    }

    rect = (Rectangle){
        (float)(app->theme.sidebar_width + app->theme.margin),
        (float)(app->theme.topbar_height - 14),
        (float)(GetScreenWidth() - app->theme.sidebar_width - app->theme.margin * 2),
        36.0f
    };

    switch (app->state.message.kind) {
        case DESKTOP_MESSAGE_SUCCESS:
            background = app->theme.success;
            break;
        case DESKTOP_MESSAGE_WARNING:
            background = app->theme.warning;
            break;
        case DESKTOP_MESSAGE_ERROR:
            background = app->theme.error;
            break;
        default:
            background = app->theme.accent;
            break;
    }

    DrawRectangleRounded(rect, 0.2f, 8, Fade(background, 0.18f));
    DrawRectangleRoundedLinesEx(rect, 0.2f, 8, 1.0f, Fade(background, 0.35f));
    DrawText(app->state.message.text, (int)rect.x + 12, (int)rect.y + 9, 18, app->theme.text_primary);
}

static int DesktopPages_parse_int_text(const char *text, int *out_value) {
    char *end = 0;
    long value = 0;

    if (text == 0 || out_value == 0 || text[0] == '\0') {
        return 0;
    }

    value = strtol(text, &end, 10);
    if (end == text || end == 0 || *end != '\0') {
        return 0;
    }

    *out_value = (int)value;
    return 1;
}

static void DesktopPages_copy_message(char *target, size_t target_size, const char *text) {
    if (target == 0 || target_size == 0) {
        return;
    }

    if (text == 0) {
        target[0] = '\0';
        return;
    }

    strncpy(target, text, target_size - 1);
    target[target_size - 1] = '\0';
}

static void DesktopPages_draw_output_panel(
    const DesktopApp *app,
    Rectangle rect,
    const char *title,
    const char *content,
    const char *empty_text
) {
    WorkbenchTextPanelLayout layout = Workbench_compute_text_panel_layout(rect, 14.0f, 20.0f, 16.0f);

    DrawRectangleRounded(rect, 0.12f, 8, app->theme.panel);
    DrawRectangleRoundedLinesEx(rect, 0.12f, 8, 1.0f, Fade(app->theme.border, 0.85f));
    DrawText(title, (int)layout.title_bounds.x, (int)layout.title_bounds.y, 20, app->theme.text_primary);

    if (content == 0 || content[0] == '\0') {
        DesktopPages_draw_empty_state(
            app,
            (Rectangle){
                layout.content_bounds.x,
                layout.content_bounds.y,
                layout.content_bounds.width,
                DesktopPages_minf(layout.content_bounds.height, 84.0f)
            },
            "暂无输出",
            empty_text
        );
        return;
    }

    DrawText(
        content,
        (int)layout.content_bounds.x,
        (int)layout.content_bounds.y,
        17,
        app->theme.text_secondary
    );
}

static void DesktopPages_release_focus_on_click(
    int *active_field,
    const Rectangle *fields,
    int field_count
) {
    Vector2 mouse = GetMousePosition();

    if (active_field == 0 || fields == 0 || field_count <= 0) {
        return;
    }

    if (*active_field <= 0 || *active_field > field_count) {
        return;
    }

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) &&
        !CheckCollisionPointRec(mouse, fields[*active_field - 1])) {
        *active_field = 0;
    }
}

static void DesktopPages_draw_text_input(
    Rectangle bounds,
    char *text,
    int text_size,
    int editable,
    int *active_field,
    int field_id
) {
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

static void DesktopPages_draw_login(DesktopApp *app) {
    DesktopLoginLayout layout = DesktopPages_compute_login_layout(
        GetScreenWidth(),
        GetScreenHeight(),
        &app->theme
    );
    Rectangle card = layout.card_bounds;
    Rectangle user_box = layout.user_box;
    Rectangle password_box = layout.password_box;
    Rectangle role_box = layout.role_value_bounds;
    Rectangle role_prev = layout.role_prev_bounds;
    Rectangle role_next = layout.role_next_bounds;
    Rectangle login_button = layout.login_button_bounds;
    Rectangle input_fields[] = { layout.user_box, layout.password_box };
    User user;
    Result result;

    DrawRectangleGradientV(0, 0, GetScreenWidth(), GetScreenHeight(), (Color){ 244, 248, 255, 255 }, app->theme.background);
    DrawCircleGradient((int)(card.x - 30), (int)(card.y + 60), 140.0f, Fade(app->theme.nav_active, 0.12f), BLANK);
    DrawCircleGradient((int)(card.x + card.width + 20), (int)(card.y + card.height - 40), 180.0f, Fade(app->theme.accent, 0.10f), BLANK);
    DrawRectangleRounded(card, 0.2f, 10, Fade(app->theme.panel, 0.94f));
    DrawRectangleRoundedLinesEx(card, 0.2f, 10, 1.0f, Fade(app->theme.border, 0.9f));
    DrawRectangleRounded(
        (Rectangle){ card.x + 24, card.y + 24, 92, 8 },
        0.5f,
        8,
        Fade(app->theme.nav_active, 0.9f)
    );

    DrawText("轻量级 HIS 桌面控制页", (int)card.x + 36, (int)card.y + 44, 34, app->theme.text_primary);
    DrawText(
        "原生工作台 / 自动识别账号角色 / 现代医疗后台",
        (int)card.x + 36,
        (int)card.y + 88,
        20,
        app->theme.text_secondary
    );

    DesktopPages_release_focus_on_click(
        &app->state.login_form.active_field,
        input_fields,
        (int)(sizeof(input_fields) / sizeof(input_fields[0]))
    );

    GuiLabel((Rectangle){ user_box.x, user_box.y - 24, 120, 20 }, "用户编号");
    DesktopPages_draw_text_input(
        user_box,
        app->state.login_form.user_id,
        sizeof(app->state.login_form.user_id),
        1,
        &app->state.login_form.active_field,
        1
    );

    GuiLabel((Rectangle){ password_box.x, password_box.y - 24, 120, 20 }, "密码");
    DesktopPages_draw_text_input(
        password_box,
        app->state.login_form.password,
        sizeof(app->state.login_form.password),
        1,
        &app->state.login_form.active_field,
        2
    );

    GuiLabel((Rectangle){ role_box.x, role_box.y - 24, 120, 20 }, "角色");
    if (GuiButton(role_prev, "<")) {
        app->state.login_form.active_field = 0;
        app->state.login_form.role_index = DesktopPages_wrap_role_index(app->state.login_form.role_index - 1);
    }
    DrawRectangleRounded(role_box, 0.18f, 8, Fade(app->theme.panel_alt, 0.96f));
    DrawRectangleRoundedLinesEx(role_box, 0.18f, 8, 1.0f, Fade(app->theme.border, 0.9f));
    DrawText(
        DesktopPages_role_name_from_index(app->state.login_form.role_index),
        (int)role_box.x + 18,
        (int)role_box.y + 10,
        22,
        app->theme.text_primary
    );
    if (GuiButton(role_next, ">")) {
        app->state.login_form.active_field = 0;
        app->state.login_form.role_index = DesktopPages_wrap_role_index(app->state.login_form.role_index + 1);
    }

    if (GuiButton(login_button, "登录")) {
        result = DesktopAdapters_login(
            &app->application,
            app->state.login_form.user_id,
            app->state.login_form.password,
            DesktopApp_role_from_index(app->state.login_form.role_index),
            &user
        );
        DesktopPages_show_result_message(app, result);
        if (result.success != 0) {
            DesktopAppState_login_success(&app->state, &user);
        }
    }
}

static void DesktopPages_draw_topbar(DesktopApp *app) {
    const WorkbenchDef *wb = Workbench_get(app->state.current_user.role);
    const char *title = "Lightweight HIS Desktop";
    char session_text[128];
    char time_text[64];
    DesktopTopbarLayout layout;
    time_t now = time(0);
    struct tm *local = localtime(&now);

    snprintf(
        session_text,
        sizeof(session_text),
        "%s | %s",
        app->state.current_user.user_id,
        DesktopApp_user_role_label(app->state.current_user.role)
    );
    if (local != 0) {
        strftime(time_text, sizeof(time_text), "%Y-%m-%d %H:%M", local);
    } else {
        strcpy(time_text, "--:--");
    }

    layout = DesktopPages_compute_topbar_layout(
        GetScreenWidth(),
        &app->theme,
        (float)MeasureText(title, 28),
        (float)MeasureText(session_text, 18),
        (float)MeasureText(time_text, 18)
    );

    DrawRectangleRec(layout.bar_bounds, app->theme.panel_alt);
    /* Accent bar at top */
    DrawRectangle(0, 0, GetScreenWidth(), 3, wb != 0 ? wb->accent : app->theme.nav_active);
    DrawLine(0, app->theme.topbar_height - 1, GetScreenWidth(), app->theme.topbar_height - 1, app->theme.border);
    DrawText(title, (int)layout.title_bounds.x, (int)layout.title_bounds.y, 28, app->theme.text_primary);
    DrawText(
        session_text,
        (int)layout.session_bounds.x,
        (int)layout.session_bounds.y,
        18,
        app->theme.text_secondary
    );
    DrawText(time_text, (int)layout.time_bounds.x, (int)layout.time_bounds.y, 18, app->theme.text_secondary);

    if (GuiButton(layout.logout_bounds, "退出")) {
        MenuApplication_logout(&app->application);
        DesktopAppState_logout(&app->state);
        DesktopAppState_show_message(&app->state, DESKTOP_MESSAGE_INFO, "已退出登录");
    }
}

/* Old sidebar removed - now using Workbench_draw_sidebar */

static void DesktopPages_draw_dashboard(DesktopApp *app, Rectangle panel) {
    Rectangle cards[4];
    const float card_width = (panel.width - app->theme.spacing * 3) / 4.0f;
    const float section_top = panel.y + app->theme.card_height + 64;
    const float section_height = panel.height - app->theme.card_height - 76;
    int i = 0;
    const LinkedListNode *current = 0;
    Result result;
    int left_count = 0;
    int right_count = 0;

    if (app->state.dashboard.loaded == 0) {
        result = DesktopAdapters_load_dashboard(&app->application, &app->state.dashboard);
        DesktopPages_show_result_message(app, result);
        app->state.dashboard.loaded = result.success != 0 ? 1 : -1;
    }

    for (i = 0; i < 4; i++) {
        cards[i] = (Rectangle){
            panel.x + i * (card_width + app->theme.spacing),
            panel.y,
            card_width,
            (float)app->theme.card_height
        };
    }

    DesktopPages_draw_card(app, cards[0], "患者总数", app->state.dashboard.patient_count, app->theme.nav_active);
    DesktopPages_draw_card(app, cards[1], "挂号总数", app->state.dashboard.registration_count, app->theme.accent);
    DesktopPages_draw_card(app, cards[2], "当前住院", app->state.dashboard.inpatient_count, app->theme.success);
    DesktopPages_draw_card(app, cards[3], "低库存药品", app->state.dashboard.low_stock_count, app->theme.warning);

    if (GuiButton((Rectangle){ panel.x + panel.width - 110, panel.y + app->theme.card_height + 12, 96, 32 }, "刷新")) {
        app->state.dashboard.loaded = 0;
    }

    DrawText("最近挂号", (int)panel.x, (int)(panel.y + app->theme.card_height + 20), 22, app->theme.text_primary);
    DrawText("最近发药", (int)(panel.x + panel.width / 2 + 10), (int)(panel.y + app->theme.card_height + 20), 22, app->theme.text_primary);

    DrawRectangleRounded(
        (Rectangle){ panel.x, section_top, panel.width / 2 - 8, section_height },
        0.16f,
        8,
        app->theme.panel
    );
    DrawRectangleRounded(
        (Rectangle){ panel.x + panel.width / 2 + 8, section_top, panel.width / 2 - 8, section_height },
        0.16f,
        8,
        app->theme.panel
    );

    current = app->state.dashboard.recent_registrations.head;
    i = 0;
    while (current != 0 && i < 5) {
        const Registration *registration = (const Registration *)current->data;
        DrawText(
            TextFormat("%s | %s | %s", registration->registration_id, registration->patient_id, registration->registered_at),
            (int)panel.x + 16,
            (int)(section_top + 20 + i * 30),
            18,
            app->theme.text_secondary
        );
        current = current->next;
        i++;
    }
    left_count = i;

    current = app->state.dashboard.recent_dispensations.head;
    i = 0;
    while (current != 0 && i < 5) {
        const DispenseRecord *record = (const DispenseRecord *)current->data;
        DrawText(
            TextFormat("%s | %s | %s", record->dispense_id, record->patient_id, record->dispensed_at),
            (int)(panel.x + panel.width / 2 + 24),
            (int)(section_top + 20 + i * 30),
            18,
            app->theme.text_secondary
        );
        current = current->next;
        i++;
    }
    right_count = i;

    if (left_count == 0) {
        DesktopPages_draw_empty_state(
            app,
            (Rectangle){ panel.x + 10, section_top + 12, panel.width / 2 - 28, 92 },
            "暂无挂号记录",
            "可点击右上角“刷新”重新获取。"
        );
    }

    if (right_count == 0) {
        DesktopPages_draw_empty_state(
            app,
            (Rectangle){ panel.x + panel.width / 2 + 18, section_top + 12, panel.width / 2 - 28, 92 },
            "暂无发药记录",
            "当前时间段没有可展示的发药数据。"
        );
    }
}

static void DesktopPages_draw_patients(DesktopApp *app, Rectangle panel) {
    Rectangle search_box = { panel.x, panel.y, panel.width - 150, 36 };
    Rectangle search_button = { panel.x + panel.width - 132, panel.y, 132, 36 };
    Rectangle content_bounds;
    Rectangle list_rect;
    Rectangle detail_rect;
    Rectangle input_fields[] = { search_box };
    WorkbenchListDetailLayout split_layout;
    const int is_patient = app->state.current_user.role == USER_ROLE_PATIENT;
    Result result;
    const LinkedListNode *current = 0;
    int index = 0;

    if (is_patient) {
        strncpy(
            app->state.patient_page.query,
            app->state.current_user.user_id,
            sizeof(app->state.patient_page.query) - 1
        );
        app->state.patient_page.query[sizeof(app->state.patient_page.query) - 1] = '\0';
        app->state.patient_page.search_mode = DESKTOP_PATIENT_SEARCH_BY_ID;
    }

    DesktopPages_release_focus_on_click(
        &app->state.patient_page.active_field,
        input_fields,
        (int)(sizeof(input_fields) / sizeof(input_fields[0]))
    );

    GuiLabel((Rectangle){ panel.x, panel.y - 22, 180, 20 }, "患者查询");
    if (is_patient == 0) {
        app->state.patient_page.search_mode = (DesktopPatientSearchMode)GuiToggleGroup(
            (Rectangle){ panel.x, panel.y + 40, 220, 28 },
            "按编号;按姓名",
            (int *)&app->state.patient_page.search_mode
        );
    }
    DesktopPages_draw_text_input(
        search_box,
        app->state.patient_page.query,
        sizeof(app->state.patient_page.query),
        app->state.current_user.role != USER_ROLE_PATIENT,
        &app->state.patient_page.active_field,
        1
    );
    content_bounds = (Rectangle){
        panel.x,
        panel.y + (is_patient != 0 ? 52.0f : 84.0f),
        panel.width,
        panel.height - (is_patient != 0 ? 52.0f : 84.0f)
    };
    split_layout = Workbench_compute_list_detail_layout(content_bounds, 0.48f, 20.0f, 260.0f);
    list_rect = split_layout.list_bounds;
    detail_rect = split_layout.detail_bounds;

    if (GuiButton(search_button, "查询")) {
        LinkedList_clear(&app->state.patient_page.results, free);
        LinkedList_init(&app->state.patient_page.results);
        app->state.patient_page.selected_index = -1;
        app->state.patient_page.has_selected_patient = 0;

        result = DesktopAdapters_search_patients(
            &app->application,
            app->state.patient_page.search_mode,
            app->state.patient_page.query,
            &app->state.patient_page.results
        );
        DesktopPages_show_result_message(app, result);
        if (result.success != 0 && app->state.patient_page.results.head != 0) {
            const Patient *first = (const Patient *)app->state.patient_page.results.head->data;
            app->state.patient_page.selected_patient = *first;
            app->state.patient_page.selected_index = 0;
            app->state.patient_page.has_selected_patient = 1;
        }
    }

    DrawRectangleRounded(list_rect, 0.12f, 8, app->theme.panel);
    DrawRectangleRounded(detail_rect, 0.12f, 8, app->theme.panel);
    DrawRectangleRoundedLinesEx(list_rect, 0.12f, 8, 1.0f, Fade(app->theme.border, 0.85f));
    DrawRectangleRoundedLinesEx(detail_rect, 0.12f, 8, 1.0f, Fade(app->theme.border, 0.85f));
    DrawText("患者列表", (int)list_rect.x + 14, (int)list_rect.y + 12, 20, app->theme.text_primary);
    DrawText("详情面板", (int)detail_rect.x + 14, (int)detail_rect.y + 12, 20, app->theme.text_primary);

    current = app->state.patient_page.results.head;
    while (current != 0 && index < 10) {
        const Patient *patient = (const Patient *)current->data;
        Rectangle row = {
            list_rect.x + 12,
            list_rect.y + 48 + index * 40,
            list_rect.width - 24,
            32
        };

        if (app->state.patient_page.selected_index == index) {
            DrawRectangleRounded(row, 0.16f, 8, Fade(app->theme.nav_active, 0.15f));
        }

        if (GuiButton(row, TextFormat("%s | %s", patient->patient_id, patient->name))) {
            app->state.patient_page.selected_index = index;
            app->state.patient_page.selected_patient = *patient;
            app->state.patient_page.has_selected_patient = 1;
        }

        current = current->next;
        index++;
    }

    if (index == 0) {
        DesktopPages_draw_empty_state(
            app,
            (Rectangle){ list_rect.x + 12, list_rect.y + 48, list_rect.width - 24, 84 },
            "暂无患者结果",
            "请调整查询条件后重试。"
        );
    }

    if (app->state.patient_page.has_selected_patient != 0) {
        const Patient *patient = &app->state.patient_page.selected_patient;
        const char *detail_labels[] = {
            "编号", "姓名", "年龄", "联系方式", "过敏史", "病史", "住院状态"
        };
        const char *detail_values[] = {
            patient->patient_id,
            patient->name,
            TextFormat("%d", patient->age),
            patient->contact,
            patient->allergy,
            patient->medical_history,
            patient->is_inpatient ? "是" : "否"
        };
        int row = 0;

        for (row = 0; row < 7; row++) {
            WorkbenchInfoRowLayout info_layout = Workbench_compute_info_row_layout(
                (Rectangle){
                    detail_rect.x + 16.0f,
                    detail_rect.y + 54.0f + (float)row * 32.0f,
                    detail_rect.width - 32.0f,
                    28.0f
                },
                96.0f,
                14.0f
            );
            DrawText(
                detail_labels[row],
                (int)info_layout.label_bounds.x,
                (int)info_layout.label_bounds.y,
                18,
                app->theme.text_secondary
            );
            DrawText(
                detail_values[row] != 0 ? detail_values[row] : "--",
                (int)info_layout.value_bounds.x,
                (int)info_layout.value_bounds.y,
                18,
                app->theme.text_primary
            );
        }
    } else {
        DesktopPages_draw_empty_state(
            app,
            (Rectangle){
                detail_rect.x + 12,
                detail_rect.y + 48,
                detail_rect.width - 24,
                DesktopPages_minf(detail_rect.height - 60.0f, 84.0f)
            },
            "等待选择患者",
            "点击左侧患者行后可查看详情信息。"
        );
    }
}

static void DesktopPages_draw_registration(DesktopApp *app, Rectangle panel) {
    Result result;
    Registration registration;
    Rectangle patient_box = { panel.x + 16, panel.y + 82, 260, 34 };
    Rectangle doctor_box = { panel.x + 16, panel.y + 152, 260, 34 };
    Rectangle department_box = { panel.x + 16, panel.y + 222, 260, 34 };
    Rectangle registered_at_box = { panel.x + 16, panel.y + 292, 260, 34 };
    Rectangle input_fields[] = { patient_box, doctor_box, department_box, registered_at_box };

    DrawRectangleRounded(panel, 0.12f, 8, app->theme.panel);
    DrawText("挂号录入", (int)panel.x + 16, (int)panel.y + 16, 24, app->theme.text_primary);

    if (app->state.current_user.role == USER_ROLE_PATIENT) {
        DrawText("当前角色无权进行挂号录入。", (int)panel.x + 16, (int)panel.y + 56, 18, app->theme.error);
        return;
    }

    DesktopPages_release_focus_on_click(
        &app->state.registration_page.active_field,
        input_fields,
        (int)(sizeof(input_fields) / sizeof(input_fields[0]))
    );

    GuiLabel((Rectangle){ panel.x + 16, panel.y + 58, 120, 20 }, "患者编号");
    DesktopPages_draw_text_input(
        patient_box,
        app->state.registration_page.patient_id,
        sizeof(app->state.registration_page.patient_id),
        1,
        &app->state.registration_page.active_field,
        1
    );
    GuiLabel((Rectangle){ panel.x + 16, panel.y + 128, 120, 20 }, "医生编号");
    DesktopPages_draw_text_input(
        doctor_box,
        app->state.registration_page.doctor_id,
        sizeof(app->state.registration_page.doctor_id),
        1,
        &app->state.registration_page.active_field,
        2
    );
    GuiLabel((Rectangle){ panel.x + 16, panel.y + 198, 120, 20 }, "科室编号");
    DesktopPages_draw_text_input(
        department_box,
        app->state.registration_page.department_id,
        sizeof(app->state.registration_page.department_id),
        1,
        &app->state.registration_page.active_field,
        3
    );
    GuiLabel((Rectangle){ panel.x + 16, panel.y + 268, 120, 20 }, "挂号时间");
    DesktopPages_draw_text_input(
        registered_at_box,
        app->state.registration_page.registered_at,
        sizeof(app->state.registration_page.registered_at),
        1,
        &app->state.registration_page.active_field,
        4
    );

    if (GuiButton((Rectangle){ panel.x + 16, panel.y + 342, 160, 38 }, "提交挂号")) {
        memset(&registration, 0, sizeof(registration));
        result = DesktopAdapters_submit_registration(
            &app->application,
            app->state.registration_page.patient_id,
            app->state.registration_page.doctor_id,
            app->state.registration_page.department_id,
            app->state.registration_page.registered_at,
            &registration
        );
        DesktopPages_show_result_message(app, result);
        if (result.success != 0) {
            strncpy(
                app->state.registration_page.last_registration_id,
                registration.registration_id,
                sizeof(app->state.registration_page.last_registration_id) - 1
            );
            app->state.registration_page.last_registration_id[
                sizeof(app->state.registration_page.last_registration_id) - 1
            ] = '\0';
            app->state.dashboard.loaded = 0;
        }
    }

    DrawText(
        TextFormat("最近成功挂号编号: %s",
            app->state.registration_page.last_registration_id[0] != '\0'
                ? app->state.registration_page.last_registration_id
                : "--"
        ),
        (int)panel.x + 320,
        (int)panel.y + 90,
        20,
        app->theme.text_secondary
    );
}

static void DesktopPages_draw_doctor(DesktopApp *app, Rectangle panel) {
    DesktopDoctorPageState *state = &app->state.doctor_page;
    Rectangle left = { panel.x, panel.y, panel.width * 0.52f, panel.height };
    Rectangle right = { panel.x + panel.width * 0.56f, panel.y, panel.width * 0.44f, panel.height };
    Rectangle doctor_box = { left.x + 16, left.y + 80, 220, 30 };
    Rectangle patient_box = { left.x + 16, left.y + 150, 220, 30 };
    Rectangle registration_box = { left.x + 16, left.y + 230, 160, 28 };
    Rectangle chief_box = { left.x + 16, left.y + 280, 340, 28 };
    Rectangle diagnosis_box = { left.x + 16, left.y + 330, 340, 28 };
    Rectangle advice_box = { left.x + 16, left.y + 380, 340, 28 };
    Rectangle visit_time_box = { left.x + 16, left.y + 448, 200, 28 };
    Rectangle visit_id_box = { left.x + 16, left.y + 526, 160, 28 };
    Rectangle exam_item_box = { left.x + 186, left.y + 526, 170, 28 };
    Rectangle exam_type_box = { left.x + 16, left.y + 560, 160, 28 };
    Rectangle requested_at_box = { left.x + 186, left.y + 560, 170, 28 };
    Rectangle examination_id_box = { left.x + 16, left.y + 632, 160, 28 };
    Rectangle exam_result_box = { left.x + 186, left.y + 632, 170, 28 };
    Rectangle completed_at_box = { left.x + 16, left.y + 666, 200, 28 };
    Rectangle input_fields[] = {
        doctor_box, patient_box, registration_box, chief_box, diagnosis_box, advice_box,
        visit_time_box, visit_id_box, exam_item_box, exam_type_box, requested_at_box,
        examination_id_box, exam_result_box, completed_at_box
    };
    Result result;
    bool need_exam = state->need_exam != 0;
    bool need_admission = state->need_admission != 0;
    bool need_medicine = state->need_medicine != 0;

    if (app->state.current_user.role == USER_ROLE_DOCTOR && state->doctor_id[0] == '\0') {
        DesktopPages_copy_message(state->doctor_id, sizeof(state->doctor_id), app->state.current_user.user_id);
    }

    DesktopPages_release_focus_on_click(
        &state->active_field,
        input_fields,
        (int)(sizeof(input_fields) / sizeof(input_fields[0]))
    );

    DrawRectangleRounded(left, 0.12f, 8, app->theme.panel);
    DrawRectangleRoundedLinesEx(left, 0.12f, 8, 1.0f, Fade(app->theme.border, 0.85f));
    DrawText("医生工作台", (int)left.x + 16, (int)left.y + 16, 24, app->theme.text_primary);

    GuiLabel((Rectangle){ left.x + 16, left.y + 56, 120, 20 }, "医生编号");
    DesktopPages_draw_text_input(
        doctor_box,
        state->doctor_id,
        sizeof(state->doctor_id),
        app->state.current_user.role != USER_ROLE_DOCTOR,
        &state->active_field,
        1
    );
    if (GuiButton((Rectangle){ left.x + 246, left.y + 80, 110, 30 }, "待诊列表")) {
        result = DesktopAdapters_query_pending_registrations_by_doctor(
            &app->application,
            state->doctor_id,
            state->output,
            sizeof(state->output)
        );
        DesktopPages_show_result_message(app, result);
    }

    GuiLabel((Rectangle){ left.x + 16, left.y + 126, 160, 20 }, "患者编号 / 历史查询");
    DesktopPages_draw_text_input(patient_box, state->patient_id, sizeof(state->patient_id), 1, &state->active_field, 2);
    if (GuiButton((Rectangle){ left.x + 246, left.y + 150, 110, 30 }, "患者历史")) {
        result = DesktopAdapters_query_patient_history(
            &app->application,
            state->patient_id,
            state->output,
            sizeof(state->output)
        );
        DesktopPages_show_result_message(app, result);
    }

    DrawText("诊疗记录", (int)left.x + 16, (int)left.y + 202, 20, app->theme.text_primary);
    DesktopPages_draw_text_input(registration_box, state->registration_id, sizeof(state->registration_id), 1, &state->active_field, 3);
    GuiLabel((Rectangle){ left.x + 16, left.y + 260, 100, 18 }, "主诉");
    DesktopPages_draw_text_input(chief_box, state->chief_complaint, sizeof(state->chief_complaint), 1, &state->active_field, 4);
    GuiLabel((Rectangle){ left.x + 16, left.y + 310, 100, 18 }, "诊断");
    DesktopPages_draw_text_input(diagnosis_box, state->diagnosis, sizeof(state->diagnosis), 1, &state->active_field, 5);
    GuiLabel((Rectangle){ left.x + 16, left.y + 360, 100, 18 }, "建议");
    DesktopPages_draw_text_input(advice_box, state->advice, sizeof(state->advice), 1, &state->active_field, 6);
    GuiCheckBox((Rectangle){ left.x + 16, left.y + 418, 20, 20 }, "需要检查", &need_exam);
    GuiCheckBox((Rectangle){ left.x + 120, left.y + 418, 20, 20 }, "需要住院", &need_admission);
    GuiCheckBox((Rectangle){ left.x + 224, left.y + 418, 20, 20 }, "需要用药", &need_medicine);
    state->need_exam = need_exam ? 1 : 0;
    state->need_admission = need_admission ? 1 : 0;
    state->need_medicine = need_medicine ? 1 : 0;
    DesktopPages_draw_text_input(visit_time_box, state->visit_time, sizeof(state->visit_time), 1, &state->active_field, 7);
    if (GuiButton((Rectangle){ left.x + 228, left.y + 448, 128, 30 }, "提交诊疗")) {
        result = DesktopAdapters_create_visit_record(
            &app->application,
            state->registration_id,
            state->chief_complaint,
            state->diagnosis,
            state->advice,
            state->need_exam,
            state->need_admission,
            state->need_medicine,
            state->visit_time,
            state->output,
            sizeof(state->output)
        );
        DesktopPages_show_result_message(app, result);
    }

    DrawText("检查记录", (int)left.x + 16, (int)left.y + 498, 20, app->theme.text_primary);
    DesktopPages_draw_text_input(visit_id_box, state->visit_id, sizeof(state->visit_id), 1, &state->active_field, 8);
    DesktopPages_draw_text_input(exam_item_box, state->exam_item, sizeof(state->exam_item), 1, &state->active_field, 9);
    DesktopPages_draw_text_input(exam_type_box, state->exam_type, sizeof(state->exam_type), 1, &state->active_field, 10);
    DesktopPages_draw_text_input(requested_at_box, state->requested_at, sizeof(state->requested_at), 1, &state->active_field, 11);
    if (GuiButton((Rectangle){ left.x + 16, left.y + 594, 160, 30 }, "新增检查")) {
        result = DesktopAdapters_create_examination_record(
            &app->application,
            state->visit_id,
            state->exam_item,
            state->exam_type,
            state->requested_at,
            state->output,
            sizeof(state->output)
        );
        DesktopPages_show_result_message(app, result);
    }
    DesktopPages_draw_text_input(examination_id_box, state->examination_id, sizeof(state->examination_id), 1, &state->active_field, 12);
    DesktopPages_draw_text_input(exam_result_box, state->exam_result, sizeof(state->exam_result), 1, &state->active_field, 13);
    DesktopPages_draw_text_input(completed_at_box, state->completed_at, sizeof(state->completed_at), 1, &state->active_field, 14);
    if (GuiButton((Rectangle){ left.x + 228, left.y + 666, 128, 30 }, "回写结果")) {
        result = DesktopAdapters_complete_examination_record(
            &app->application,
            state->examination_id,
            state->exam_result,
            state->completed_at,
            state->output,
            sizeof(state->output)
        );
        DesktopPages_show_result_message(app, result);
    }

    DesktopPages_draw_output_panel(
        app,
        right,
        "医生操作输出",
        state->output,
        "左侧提交查询、诊疗或检查操作后，这里显示返回结果。"
    );
}

static void DesktopPages_draw_inpatient(DesktopApp *app, Rectangle panel) {
    DesktopInpatientPageState *state = &app->state.inpatient_page;
    Rectangle left = { panel.x, panel.y, panel.width * 0.52f, panel.height };
    Rectangle right = { panel.x + panel.width * 0.56f, panel.y, panel.width * 0.44f, panel.height };
    Rectangle ward_box = { left.x + 148, left.y + 84, 140, 28 };
    Rectangle patient_box = { left.x + 16, left.y + 164, 120, 28 };
    Rectangle inpatient_ward_box = { left.x + 148, left.y + 164, 120, 28 };
    Rectangle bed_box = { left.x + 280, left.y + 164, 120, 28 };
    Rectangle admitted_at_box = { left.x + 16, left.y + 198, 180, 28 };
    Rectangle summary_box = { left.x + 208, left.y + 198, 192, 28 };
    Rectangle admission_box = { left.x + 16, left.y + 312, 140, 28 };
    Rectangle discharged_at_box = { left.x + 168, left.y + 312, 160, 28 };
    Rectangle discharge_summary_box = { left.x + 16, left.y + 346, 312, 28 };
    Rectangle query_patient_box = { left.x + 16, left.y + 430, 160, 28 };
    Rectangle query_bed_box = { left.x + 16, left.y + 464, 160, 28 };
    Rectangle input_fields[] = {
        ward_box, patient_box, inpatient_ward_box, bed_box, admitted_at_box, summary_box,
        admission_box, discharged_at_box, discharge_summary_box, query_patient_box, query_bed_box
    };
    Result result;

    DesktopPages_release_focus_on_click(
        &state->active_field,
        input_fields,
        (int)(sizeof(input_fields) / sizeof(input_fields[0]))
    );

    DrawRectangleRounded(left, 0.12f, 8, app->theme.panel);
    DrawRectangleRoundedLinesEx(left, 0.12f, 8, 1.0f, Fade(app->theme.border, 0.85f));
    DrawText("住院与床位", (int)left.x + 16, (int)left.y + 16, 24, app->theme.text_primary);

    DrawText("病区床位查询", (int)left.x + 16, (int)left.y + 56, 20, app->theme.text_primary);
    if (GuiButton((Rectangle){ left.x + 16, left.y + 84, 120, 30 }, "查看病房")) {
        result = DesktopAdapters_list_wards(&app->application, state->output, sizeof(state->output));
        DesktopPages_show_result_message(app, result);
    }
    DesktopPages_draw_text_input(ward_box, state->ward_id, sizeof(state->ward_id), 1, &state->active_field, 1);
    if (GuiButton((Rectangle){ left.x + 300, left.y + 84, 120, 30 }, "查看床位")) {
        result = DesktopAdapters_list_beds_by_ward(
            &app->application,
            state->ward_id,
            state->output,
            sizeof(state->output)
        );
        DesktopPages_show_result_message(app, result);
    }

    DrawText("入院登记", (int)left.x + 16, (int)left.y + 136, 20, app->theme.text_primary);
    DesktopPages_draw_text_input(patient_box, state->patient_id, sizeof(state->patient_id), 1, &state->active_field, 2);
    DesktopPages_draw_text_input(inpatient_ward_box, state->ward_id, sizeof(state->ward_id), 1, &state->active_field, 3);
    DesktopPages_draw_text_input(bed_box, state->bed_id, sizeof(state->bed_id), 1, &state->active_field, 4);
    DesktopPages_draw_text_input(admitted_at_box, state->admitted_at, sizeof(state->admitted_at), 1, &state->active_field, 5);
    DesktopPages_draw_text_input(summary_box, state->summary, sizeof(state->summary), 1, &state->active_field, 6);
    if (GuiButton((Rectangle){ left.x + 16, left.y + 232, 160, 30 }, "办理入院")) {
        result = DesktopAdapters_admit_patient(
            &app->application,
            state->patient_id,
            state->ward_id,
            state->bed_id,
            state->admitted_at,
            state->summary,
            state->output,
            sizeof(state->output)
        );
        DesktopPages_show_result_message(app, result);
    }

    DrawText("出院办理", (int)left.x + 16, (int)left.y + 284, 20, app->theme.text_primary);
    DesktopPages_draw_text_input(admission_box, state->admission_id, sizeof(state->admission_id), 1, &state->active_field, 7);
    DesktopPages_draw_text_input(discharged_at_box, state->discharged_at, sizeof(state->discharged_at), 1, &state->active_field, 8);
    DesktopPages_draw_text_input(discharge_summary_box, state->summary, sizeof(state->summary), 1, &state->active_field, 9);
    if (GuiButton((Rectangle){ left.x + 340, left.y + 312, 100, 30 }, "办理出院")) {
        result = DesktopAdapters_discharge_patient(
            &app->application,
            state->admission_id,
            state->discharged_at,
            state->summary,
            state->output,
            sizeof(state->output)
        );
        DesktopPages_show_result_message(app, result);
    }

    DrawText("状态查询", (int)left.x + 16, (int)left.y + 402, 20, app->theme.text_primary);
    DesktopPages_draw_text_input(query_patient_box, state->query_patient_id, sizeof(state->query_patient_id), 1, &state->active_field, 10);
    if (GuiButton((Rectangle){ left.x + 188, left.y + 430, 140, 30 }, "查当前住院")) {
        result = DesktopAdapters_query_active_admission_by_patient(
            &app->application,
            state->query_patient_id,
            state->output,
            sizeof(state->output)
        );
        DesktopPages_show_result_message(app, result);
    }
    DesktopPages_draw_text_input(query_bed_box, state->query_bed_id, sizeof(state->query_bed_id), 1, &state->active_field, 11);
    if (GuiButton((Rectangle){ left.x + 188, left.y + 464, 140, 30 }, "查床位患者")) {
        result = DesktopAdapters_query_current_patient_by_bed(
            &app->application,
            state->query_bed_id,
            state->output,
            sizeof(state->output)
        );
        DesktopPages_show_result_message(app, result);
    }

    DesktopPages_draw_output_panel(
        app,
        right,
        "住院操作输出",
        state->output,
        "左侧可查看病房/床位，并完成入院、出院与状态查询。"
    );
}

static void DesktopPages_draw_pharmacy(DesktopApp *app, Rectangle panel) {
    DesktopPharmacyPageState *state = &app->state.pharmacy_page;
    Rectangle left = { panel.x, panel.y, panel.width * 0.52f, panel.height };
    Rectangle right = { panel.x + panel.width * 0.56f, panel.y, panel.width * 0.44f, panel.height };
    Rectangle add_medicine_fields[] = {
        { left.x + 16, left.y + 84, 120, 28 },
        { left.x + 148, left.y + 84, 180, 28 },
        { left.x + 16, left.y + 118, 100, 28 },
        { left.x + 128, left.y + 118, 100, 28 },
        { left.x + 240, left.y + 118, 100, 28 },
        { left.x + 16, left.y + 152, 120, 28 },
        { left.x + 16, left.y + 234, 140, 28 },
        { left.x + 16, left.y + 268, 140, 28 },
        { left.x + 168, left.y + 268, 100, 28 },
        { left.x + 16, left.y + 384, 120, 28 },
        { left.x + 148, left.y + 384, 120, 28 },
        { left.x + 280, left.y + 384, 120, 28 },
        { left.x + 16, left.y + 418, 100, 28 },
        { left.x + 128, left.y + 418, 120, 28 },
        { left.x + 260, left.y + 418, 140, 28 }
    };
    Medicine medicine;
    int quantity = 0;
    int stock = 0;
    int threshold = 0;
    int restock_quantity = 0;
    Result result;

    if (app->state.current_user.role == USER_ROLE_PHARMACY && state->pharmacist_id[0] == '\0') {
        DesktopPages_copy_message(state->pharmacist_id, sizeof(state->pharmacist_id), app->state.current_user.user_id);
    }

    DesktopPages_release_focus_on_click(
        &state->active_field,
        add_medicine_fields,
        (int)(sizeof(add_medicine_fields) / sizeof(add_medicine_fields[0]))
    );

    DrawRectangleRounded(left, 0.12f, 8, app->theme.panel);
    DrawRectangleRoundedLinesEx(left, 0.12f, 8, 1.0f, Fade(app->theme.border, 0.85f));
    DrawText("药房工作台", (int)left.x + 16, (int)left.y + 16, 24, app->theme.text_primary);

    DrawText("新增药品", (int)left.x + 16, (int)left.y + 56, 20, app->theme.text_primary);
    DesktopPages_draw_text_input(add_medicine_fields[0], state->medicine_id, sizeof(state->medicine_id), 1, &state->active_field, 1);
    DesktopPages_draw_text_input(add_medicine_fields[1], state->medicine_name, sizeof(state->medicine_name), 1, &state->active_field, 2);
    DesktopPages_draw_text_input(add_medicine_fields[2], state->price_text, sizeof(state->price_text), 1, &state->active_field, 3);
    DesktopPages_draw_text_input(add_medicine_fields[3], state->stock_text, sizeof(state->stock_text), 1, &state->active_field, 4);
    DesktopPages_draw_text_input(add_medicine_fields[4], state->low_stock_text, sizeof(state->low_stock_text), 1, &state->active_field, 5);
    DesktopPages_draw_text_input(add_medicine_fields[5], state->department_id, sizeof(state->department_id), 1, &state->active_field, 6);
    if (GuiButton((Rectangle){ left.x + 148, left.y + 152, 120, 30 }, "添加药品")) {
        if (DesktopPages_parse_int_text(state->stock_text, &stock) == 0 ||
            DesktopPages_parse_int_text(state->low_stock_text, &threshold) == 0) {
            DesktopAppState_show_message(&app->state, DESKTOP_MESSAGE_ERROR, "库存或阈值格式无效");
        } else {
            memset(&medicine, 0, sizeof(medicine));
            DesktopPages_copy_message(medicine.medicine_id, sizeof(medicine.medicine_id), state->medicine_id);
            DesktopPages_copy_message(medicine.name, sizeof(medicine.name), state->medicine_name);
            DesktopPages_copy_message(medicine.department_id, sizeof(medicine.department_id), state->department_id);
            medicine.price = (float)atof(state->price_text);
            medicine.stock = stock;
            medicine.low_stock_threshold = threshold;
            result = DesktopAdapters_add_medicine(&app->application, &medicine, state->output, sizeof(state->output));
            DesktopPages_show_result_message(app, result);
        }
    }

    DrawText("库存操作", (int)left.x + 16, (int)left.y + 206, 20, app->theme.text_primary);
    DesktopPages_draw_text_input(add_medicine_fields[6], state->stock_query_medicine_id, sizeof(state->stock_query_medicine_id), 1, &state->active_field, 7);
    if (GuiButton((Rectangle){ left.x + 168, left.y + 234, 100, 30 }, "查库存")) {
        result = DesktopAdapters_query_medicine_stock(
            &app->application,
            state->stock_query_medicine_id,
            state->output,
            sizeof(state->output)
        );
        DesktopPages_show_result_message(app, result);
    }
    DesktopPages_draw_text_input(add_medicine_fields[7], state->medicine_id, sizeof(state->medicine_id), 1, &state->active_field, 8);
    DesktopPages_draw_text_input(add_medicine_fields[8], state->restock_quantity_text, sizeof(state->restock_quantity_text), 1, &state->active_field, 9);
    if (GuiButton((Rectangle){ left.x + 280, left.y + 268, 100, 30 }, "入库")) {
        if (DesktopPages_parse_int_text(state->restock_quantity_text, &restock_quantity) == 0) {
            DesktopAppState_show_message(&app->state, DESKTOP_MESSAGE_ERROR, "入库数量格式无效");
        } else {
            result = DesktopAdapters_restock_medicine(
                &app->application,
                state->medicine_id,
                restock_quantity,
                state->output,
                sizeof(state->output)
            );
            DesktopPages_show_result_message(app, result);
        }
    }
    if (GuiButton((Rectangle){ left.x + 16, left.y + 302, 140, 30 }, "低库存提醒")) {
        result = DesktopAdapters_find_low_stock_medicines(
            &app->application,
            state->output,
            sizeof(state->output)
        );
        DesktopPages_show_result_message(app, result);
    }

    DrawText("发药操作", (int)left.x + 16, (int)left.y + 356, 20, app->theme.text_primary);
    DesktopPages_draw_text_input(add_medicine_fields[9], state->patient_id, sizeof(state->patient_id), 1, &state->active_field, 10);
    DesktopPages_draw_text_input(add_medicine_fields[10], state->prescription_id, sizeof(state->prescription_id), 1, &state->active_field, 11);
    DesktopPages_draw_text_input(add_medicine_fields[11], state->medicine_id, sizeof(state->medicine_id), 1, &state->active_field, 12);
    DesktopPages_draw_text_input(add_medicine_fields[12], state->dispense_quantity_text, sizeof(state->dispense_quantity_text), 1, &state->active_field, 13);
    DesktopPages_draw_text_input(add_medicine_fields[13], state->pharmacist_id, sizeof(state->pharmacist_id), app->state.current_user.role != USER_ROLE_PHARMACY, &state->active_field, 14);
    DesktopPages_draw_text_input(add_medicine_fields[14], state->dispensed_at, sizeof(state->dispensed_at), 1, &state->active_field, 15);
    if (GuiButton((Rectangle){ left.x + 16, left.y + 452, 140, 30 }, "执行发药")) {
        if (DesktopPages_parse_int_text(state->dispense_quantity_text, &quantity) == 0) {
            DesktopAppState_show_message(&app->state, DESKTOP_MESSAGE_ERROR, "发药数量格式无效");
        } else {
            result = DesktopAdapters_dispense_medicine(
                &app->application,
                state->patient_id,
                state->prescription_id,
                state->medicine_id,
                quantity,
                state->pharmacist_id,
                state->dispensed_at,
                state->output,
                sizeof(state->output)
            );
            DesktopPages_show_result_message(app, result);
            if (result.success != 0) {
                app->state.dashboard.loaded = 0;
            }
        }
    }

    DesktopPages_draw_output_panel(
        app,
        right,
        "药房操作输出",
        state->output,
        "左侧可完成药品新增、入库、发药、库存查询和低库存提醒。"
    );
}

static void DesktopPages_draw_dispense(DesktopApp *app, Rectangle panel) {
    Rectangle search_box = { panel.x, panel.y, panel.width - 130, 36 };
    Rectangle search_button = { panel.x + panel.width - 112, panel.y, 112, 36 };
    Rectangle input_fields[] = { search_box };
    const LinkedListNode *current = 0;
    int index = 0;
    Result result;
    const int is_patient = app->state.current_user.role == USER_ROLE_PATIENT;
    int should_query = 0;

    DrawText("发药记录查询", (int)panel.x, (int)panel.y - 24, 22, app->theme.text_primary);
    DrawRectangleRounded((Rectangle){ panel.x, panel.y + 52, panel.width, panel.height - 52 }, 0.12f, 8, app->theme.panel);
    DrawRectangleRoundedLinesEx(
        (Rectangle){ panel.x, panel.y + 52, panel.width, panel.height - 52 },
        0.12f,
        8,
        1.0f,
        Fade(app->theme.border, 0.85f)
    );

    if (is_patient) {
        strncpy(
            app->state.dispense_page.patient_id,
            app->state.current_user.user_id,
            sizeof(app->state.dispense_page.patient_id) - 1
        );
        app->state.dispense_page.patient_id[sizeof(app->state.dispense_page.patient_id) - 1] = '\0';
        GuiLabel((Rectangle){ panel.x, panel.y - 2, 320, 24 }, TextFormat("当前患者: %s", app->state.dispense_page.patient_id));
    } else {
        DesktopPages_release_focus_on_click(
            &app->state.dispense_page.active_field,
            input_fields,
            (int)(sizeof(input_fields) / sizeof(input_fields[0]))
        );
        GuiLabel((Rectangle){ panel.x, panel.y - 22, 120, 20 }, "患者编号");
        DesktopPages_draw_text_input(
            search_box,
            app->state.dispense_page.patient_id,
            sizeof(app->state.dispense_page.patient_id),
            1,
            &app->state.dispense_page.active_field,
            1
        );
    }

    should_query = GuiButton(search_button, "查询");
    if (app->state.dispense_page.loaded == 0 && is_patient) {
        should_query = 1;
    }

    if (should_query != 0) {
        if (app->state.dispense_page.patient_id[0] == '\0') {
            DesktopAppState_show_message(&app->state, DESKTOP_MESSAGE_WARNING, "请输入患者编号后再查询");
            app->state.dispense_page.loaded = -1;
            return;
        }
        LinkedList_clear(&app->state.dispense_page.results, free);
        LinkedList_init(&app->state.dispense_page.results);
        result = DesktopAdapters_load_dispense_history(
            &app->application,
            app->state.dispense_page.patient_id,
            &app->state.dispense_page.results
        );
        DesktopPages_show_result_message(app, result);
        app->state.dispense_page.loaded = result.success != 0 ? 1 : -1;
    }

    current = app->state.dispense_page.results.head;
    while (current != 0 && index < 10) {
        const DispenseRecord *record = (const DispenseRecord *)current->data;
        DrawText(
            TextFormat(
                "%s | %s | %s | 数量=%d | %s",
                record->dispense_id,
                record->prescription_id,
                record->medicine_id,
                record->quantity,
                record->dispensed_at
            ),
            (int)panel.x + 16,
            (int)panel.y + 74 + index * 28,
            18,
            app->theme.text_secondary
        );
        current = current->next;
        index++;
    }

    if (index == 0) {
        DesktopPages_draw_empty_state(
            app,
            (Rectangle){ panel.x + 12, panel.y + 66, panel.width - 24, 84 },
            "暂无发药记录",
            "请确认患者编号后点击“查询”。"
        );
    }
}

static void DesktopPages_draw_system(const DesktopApp *app, Rectangle panel) {
    DrawRectangleRounded(panel, 0.12f, 8, app->theme.panel);
    DrawText("系统信息", (int)panel.x + 16, (int)panel.y + 16, 24, app->theme.text_primary);
    DrawText(TextFormat("当前用户: %s", app->state.current_user.user_id), (int)panel.x + 16, (int)panel.y + 60, 18, app->theme.text_secondary);
    DrawText(TextFormat("当前角色: %s", DesktopApp_user_role_label(app->state.current_user.role)), (int)panel.x + 16, (int)panel.y + 90, 18, app->theme.text_secondary);
    if (app->paths != 0) {
        DrawText(TextFormat("users.txt: %s", app->paths->user_path), (int)panel.x + 16, (int)panel.y + 138, 18, app->theme.text_secondary);
        DrawText(TextFormat("patients.txt: %s", app->paths->patient_path), (int)panel.x + 16, (int)panel.y + 166, 18, app->theme.text_secondary);
        DrawText(TextFormat("registrations.txt: %s", app->paths->registration_path), (int)panel.x + 16, (int)panel.y + 194, 18, app->theme.text_secondary);
        DrawText(TextFormat("dispense_records.txt: %s", app->paths->dispense_record_path), (int)panel.x + 16, (int)panel.y + 222, 18, app->theme.text_secondary);
    }
}

void DesktopPages_draw(DesktopApp *app) {
    Rectangle panel;
    const WorkbenchDef *wb;

    if (app == 0) {
        return;
    }

    if (app->state.logged_in == 0) {
        DesktopPages_draw_login(app);
        DesktopPages_draw_message_bar(app);
        return;
    }

    wb = Workbench_get(app->state.current_user.role);

    DesktopPages_draw_topbar(app);
    Workbench_draw_sidebar(app, wb);
    DesktopPages_draw_message_bar(app);

    panel = DesktopPages_main_panel(app);
    if (wb != 0 && wb->draw != 0) {
        wb->draw(app, panel, app->state.workbench_page);
    }
}
