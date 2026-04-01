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

    if (GetTime() - app->state.message.show_time >= 3.0) {
        app->state.message.visible = 0;
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
    Rectangle reset_button = (Rectangle){
        login_button.x,
        login_button.y + login_button.height + 12.0f,
        login_button.width,
        40.0f
    };
    Rectangle input_fields[] = { layout.user_box, layout.password_box };
    User user;
    Result result;
    char reset_message[RESULT_MESSAGE_CAPACITY];

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
    Workbench_draw_text_input(
        user_box,
        app->state.login_form.user_id,
        sizeof(app->state.login_form.user_id),
        1,
        &app->state.login_form.active_field,
        1
    );

    GuiLabel((Rectangle){ password_box.x, password_box.y - 24, 120, 20 }, "密码");
    Workbench_draw_text_input(
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
    if (GuiButton(reset_button, "重置演示数据")) {
        result = DesktopAdapters_reset_demo_data(
            &app->application,
            app->paths,
            reset_message,
            sizeof(reset_message)
        );
        DesktopPages_show_result_message(app, result);
        if (result.success != 0) {
            DesktopAppState_logout(&app->state);
            memset(&app->state.login_form, 0, sizeof(app->state.login_form));
            app->state.login_form.role_index = DesktopApp_role_index_from_role(USER_ROLE_PATIENT);
        }
    }
}

static void DesktopPages_draw_topbar(DesktopApp *app) {
    static int cached_title_width = 0;
    static time_t last_time = 0;
    static char cached_time_text[64] = "--:--";

    const WorkbenchDef *wb = Workbench_get(app->state.current_user.role);
    const char *title = "Lightweight HIS Desktop";
    char session_text[128];
    DesktopTopbarLayout layout;
    time_t now = time(0);

    if (cached_title_width == 0) {
        cached_title_width = MeasureText(title, 28);
    }

    if (now != last_time) {
        struct tm *local = localtime(&now);
        if (local != 0) {
            strftime(cached_time_text, sizeof(cached_time_text), "%Y-%m-%d %H:%M", local);
        }
        last_time = now;
    }

    snprintf(
        session_text,
        sizeof(session_text),
        "%s | %s",
        app->state.current_user.user_id,
        DesktopApp_user_role_label(app->state.current_user.role)
    );

    layout = DesktopPages_compute_topbar_layout(
        GetScreenWidth(),
        &app->theme,
        (float)cached_title_width,
        (float)MeasureText(session_text, 18),
        (float)MeasureText(cached_time_text, 18)
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
    DrawText(cached_time_text, (int)layout.time_bounds.x, (int)layout.time_bounds.y, 18, app->theme.text_secondary);

    if (GuiButton(layout.logout_bounds, "退出")) {
        MenuApplication_logout(&app->application);
        DesktopAppState_logout(&app->state);
        DesktopAppState_show_message(&app->state, DESKTOP_MESSAGE_INFO, "已退出登录");
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
