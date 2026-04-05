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
    "患者;医生;系统管理员;住院管理员;药房人员";

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
        return 4;
    }

    if (index > 4) {
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
    float scale = (theme != 0 && theme->scale_factor > 0.0f) ? theme->scale_factor : 1.0f;
    float padding = 24.0f * scale;
    float topbar_height = 84.0f * scale;
    float gap = 18.0f * scale;
    float title_x = 24.0f * scale;
    float title_max_width = title_width > 0.0f ? title_width : 280.0f * scale;
    float session_max_width = session_width > 0.0f ? session_width : 360.0f * scale;
    float time_actual_width = time_width > 0.0f ? time_width : 148.0f * scale;
    float logout_width = 88.0f * scale;
    float logout_height = 34.0f * scale;
    float title_min_width = 180.0f * scale;
    float title_available = 0.0f;
    float session_x = 0.0f;
    float session_available = 0.0f;

    memset(&layout, 0, sizeof(layout));
    if (theme != 0) {
        padding = (float)(theme->margin > 0 ? theme->margin : (int)(24 * scale));
        topbar_height = (float)(theme->topbar_height > 0 ? theme->topbar_height : (int)(84 * scale));
        gap = (float)(theme->spacing > 0 ? theme->spacing : (int)(18 * scale));
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
        (topbar_height - 18.0f * scale) * 0.5f,
        time_actual_width,
        18.0f * scale
    };

    title_available = layout.time_bounds.x - gap - title_x - 120.0f;
    if (title_available < title_min_width) {
        title_available = title_min_width;
    }
    title_max_width = DesktopPages_minf(title_max_width, title_available);
    layout.title_bounds = (Rectangle){
        title_x,
        (topbar_height - 28.0f * scale) * 0.5f,
        title_max_width,
        28.0f * scale
    };

    session_x = layout.title_bounds.x + layout.title_bounds.width + gap;
    session_available = layout.time_bounds.x - gap - session_x;
    if (session_available < 0.0f) {
        session_available = 0.0f;
    }
    layout.session_bounds = (Rectangle){
        session_x,
        (topbar_height - 18.0f * scale) * 0.5f,
        DesktopPages_minf(session_max_width, session_available),
        18.0f * scale
    };

    return layout;
}

DesktopLoginLayout DesktopPages_compute_login_layout(
    int screen_width,
    int screen_height,
    const DesktopTheme *theme
) {
    DesktopLoginLayout layout;
    float scale = (theme != 0 && theme->scale_factor > 0.0f) ? theme->scale_factor : 1.0f;
    float margin = 32.0f * scale;
    float gap = 18.0f * scale;
    float card_width = 620.0f;
    float padding = 36.0f * scale;
    float field_height = 44.0f * scale;
    float role_button_width = 42.0f * scale;
    float card_x = 0.0f;
    float card_y = 0.0f;
    float user_y = 0.0f;
    float password_y = 0.0f;
    float role_y = 0.0f;
    float login_y = 0.0f;
    float card_height = 0.0f;
    float role_value_width = 0.0f;
    float field_gap = 32.0f * scale;

    memset(&layout, 0, sizeof(layout));
    if (theme != 0) {
        margin = (float)(theme->margin > 0 ? theme->margin : 32);
        gap = (float)(theme->spacing > 0 ? theme->spacing : 18);
    }

    card_width = DesktopPages_clampf((float)screen_width - margin * 2.0f, 360.0f * scale, 620.0f * scale);
    padding = screen_width <= 1440 ? 32.0f * scale : 36.0f * scale;
    user_y = 120.0f * scale;
    password_y = user_y + field_height + field_gap;
    role_y = password_y + field_height + field_gap;
    login_y = role_y + field_height + field_gap;
    card_height = login_y + 48.0f * scale + 200.0f * scale;
    card_height = DesktopPages_clampf(card_height, 400.0f * scale, (float)screen_height - margin * 2.0f);
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
        48.0f * scale
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
    Color icon_color;
    double elapsed;
    float alpha;

    if (app->state.message.visible == 0) {
        return;
    }

    elapsed = GetTime() - app->state.message.show_time;

    if (elapsed >= 3.0) {
        app->state.message.visible = 0;
        return;
    }

    /* Fade-out during the last 0.5 seconds */
    if (elapsed < 2.5) {
        alpha = 1.0f;
    } else {
        alpha = (float)(3.0 - elapsed) / 0.5f;
    }

    rect = (Rectangle){
        (float)(app->theme.sidebar_width + app->theme.margin),
        (float)(app->theme.topbar_height - (int)(14.0f * app->theme.scale_factor)),
        (float)(GetScreenWidth() - app->theme.sidebar_width - app->theme.margin * 2),
        36.0f * app->theme.scale_factor
    };

    switch (app->state.message.kind) {
        case DESKTOP_MESSAGE_SUCCESS:
            background = app->theme.success;
            icon_color = app->theme.success;
            break;
        case DESKTOP_MESSAGE_WARNING:
            background = app->theme.warning;
            icon_color = app->theme.warning;
            break;
        case DESKTOP_MESSAGE_ERROR:
            background = app->theme.error;
            icon_color = app->theme.error;
            break;
        default:
            background = app->theme.accent;
            icon_color = app->theme.accent;
            break;
    }

    DrawRectangleRounded(rect, 0.2f, 8, Fade(background, 0.18f * alpha));
    DrawRectangleRoundedLinesEx(rect, 0.2f, 8, 1.0f, Fade(background, 0.35f * alpha));
    /* Left accent stripe */
    DrawRectangle((int)rect.x + 2, (int)rect.y + 4, 3, (int)rect.height - 8, Fade(background, alpha));
    /* Icon dot indicating message kind */
    DrawCircle((int)rect.x + 12, (int)(rect.y + rect.height / 2), 4, Fade(icon_color, alpha));
    DrawText(app->state.message.text, (int)rect.x + 26, (int)rect.y + 9, 18, Fade(app->theme.text_primary, alpha));
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
    float login_scale = (app->theme.scale_factor > 0.0f) ? app->theme.scale_factor : 1.0f;
    Rectangle reset_button = (Rectangle){
        login_button.x,
        login_button.y + login_button.height + 12.0f * login_scale,
        login_button.width,
        40.0f * login_scale
    };
    Rectangle input_fields[] = { layout.user_box, layout.password_box };
    User user;
    Result result;
    char reset_message[RESULT_MESSAGE_CAPACITY];
    int saved_border_w;
    int saved_base_n;
    int saved_base_f;
    int saved_base_p;
    int saved_text_n;

    /* ── Background ── */
    DrawRectangleGradientV(0, 0, GetScreenWidth(), GetScreenHeight(),
                           (Color){ 246, 249, 255, 255 }, app->theme.background);
    DrawCircleGradient((int)(card.x - 30), (int)(card.y + 60), 140.0f,
                       Fade(app->theme.nav_active, 0.08f), BLANK);
    DrawCircleGradient((int)(card.x + card.width + 20), (int)(card.y + card.height - 40), 180.0f,
                       Fade(app->theme.accent, 0.06f), BLANK);

    /* ── Card shadow (layered for soft blur) ── */
    DrawRectangleRounded(
        (Rectangle){ card.x + 2, card.y + 4, card.width, card.height },
        0.06f, 10, (Color){ 0, 0, 0, 12 }
    );
    DrawRectangleRounded(
        (Rectangle){ card.x + 1, card.y + 2, card.width, card.height },
        0.06f, 10, (Color){ 0, 0, 0, 8 }
    );

    /* ── Card ── */
    DrawRectangleRounded(card, 0.06f, 10, WHITE);
    DrawRectangleRoundedLinesEx(card, 0.06f, 10, 1.0f, Fade(app->theme.border, 0.4f));

    /* ── Left accent stripe ── */
    DrawRectangle((int)card.x, (int)card.y + 8, 4, (int)card.height - 16,
                  app->theme.nav_active);

    /* ── Medical cross hint (geometric, top-right area) ── */
    {
        float s = app->theme.scale_factor;
        int cx = (int)(card.x + card.width - 52.0f * s);
        int cy = (int)(card.y + 48.0f * s);
        int arm = (int)(12.0f * s + 0.5f);
        int thick = (int)(3.0f * s + 0.5f);
        Color cross_color = Fade(app->theme.nav_active, 0.10f);
        DrawRectangle(cx - thick, cy - arm, thick * 2, arm * 2, cross_color);
        DrawRectangle(cx - arm, cy - thick, arm * 2, thick * 2, cross_color);
    }

    /* ── Title ── */
    {
        float s = app->theme.scale_factor;
        int inset = (int)(36.0f * s + 0.5f);
        DrawText("轻量级 HIS 桌面控制页",
                 (int)card.x + inset, (int)card.y + (int)(32.0f * s), 30, app->theme.text_primary);

        /* ── Version badge ── */
        {
            float bw = 68.0f * s;
            float bh = 24.0f * s;
            Rectangle ver_badge = { card.x + card.width - bw - (int)(32.0f * s), card.y + (int)(32.0f * s), bw, bh };
            DrawRectangleRounded(ver_badge, 0.4f, 8, Fade(app->theme.nav_active, 0.12f));
            DrawText("v2.5.0", (int)ver_badge.x + (int)(10.0f * s), (int)ver_badge.y + (int)(4.0f * s), 16,
                     app->theme.nav_active);
        }

        /* ── Subtitle ── */
        DrawText("原生工作台 / 自动识别账号角色 / 现代医疗后台",
                 (int)card.x + inset, (int)card.y + (int)(70.0f * s), 17,
                 Fade(app->theme.text_secondary, 0.8f));
    }

    /* ── Focus management ── */
    DesktopPages_release_focus_on_click(
        &app->state.login_form.active_field,
        input_fields,
        (int)(sizeof(input_fields) / sizeof(input_fields[0]))
    );

    /* ── User ID field ── */
    Workbench_draw_form_label(app, (int)user_box.x, (int)user_box.y - (int)(26.0f * app->theme.scale_factor),
                              "用户编号", 1);
    Workbench_draw_text_input(
        user_box,
        app->state.login_form.user_id,
        sizeof(app->state.login_form.user_id),
        1,
        &app->state.login_form.active_field,
        1
    );

    /* ── Password field ── */
    Workbench_draw_form_label(app, (int)password_box.x, (int)password_box.y - (int)(26.0f * app->theme.scale_factor),
                              "密码", 1);
    Workbench_draw_text_input(
        password_box,
        app->state.login_form.password,
        sizeof(app->state.login_form.password),
        1,
        &app->state.login_form.active_field,
        2
    );

    /* ── Role selector ── */
    Workbench_draw_form_label(app, (int)role_box.x, (int)role_box.y - (int)(26.0f * app->theme.scale_factor),
                              "角色", 1);

    /* Left arrow pill */
    DrawRectangleRounded(role_prev, 0.3f, 8, Fade(app->theme.border, 0.12f));
    DrawRectangleRoundedLinesEx(role_prev, 0.3f, 8, 1.0f, Fade(app->theme.border, 0.4f));
    if (GuiButton(role_prev, "<")) {
        app->state.login_form.active_field = 0;
        app->state.login_form.role_index = DesktopPages_wrap_role_index(
            app->state.login_form.role_index - 1);
    }

    /* Role value pill */
    DrawRectangleRounded(role_box, 0.15f, 8, Fade(app->theme.nav_active, 0.06f));
    DrawRectangleRoundedLinesEx(role_box, 0.15f, 8, 1.0f, Fade(app->theme.nav_active, 0.3f));
    {
        const char *role_name = DesktopPages_role_name_from_index(
            app->state.login_form.role_index);
        int rw = MeasureText(role_name, 20);
        int scaled_h = (int)(20.0f * app->theme.scale_factor + 0.5f);
        DrawText(role_name,
                 (int)(role_box.x + (role_box.width - rw) / 2),
                 (int)(role_box.y + (role_box.height - scaled_h) / 2), 20, app->theme.nav_active);
    }

    /* Right arrow pill */
    DrawRectangleRounded(role_next, 0.3f, 8, Fade(app->theme.border, 0.12f));
    DrawRectangleRoundedLinesEx(role_next, 0.3f, 8, 1.0f, Fade(app->theme.border, 0.4f));
    if (GuiButton(role_next, ">")) {
        app->state.login_form.active_field = 0;
        app->state.login_form.role_index = DesktopPages_wrap_role_index(
            app->state.login_form.role_index + 1);
    }

    /* ── Login button (filled accent with hover) ── */
    {
        int login_hovered = CheckCollisionPointRec(GetMousePosition(), login_button);
        Color login_bg = app->theme.nav_active;
        if (login_hovered) {
            login_bg = (Color){
                (unsigned char)DesktopPages_minf(login_bg.r + 30, 255),
                (unsigned char)DesktopPages_minf(login_bg.g + 30, 255),
                (unsigned char)DesktopPages_minf(login_bg.b + 30, 255),
                login_bg.a
            };
        }
        DrawRectangleRounded(login_button, 0.18f, 8, login_bg);
    }
    {
        const char *btn_text = "登录";
        int tw = MeasureText(btn_text, 22);
        int scaled_h = (int)(22.0f * app->theme.scale_factor + 0.5f);
        DrawText(btn_text,
                 (int)(login_button.x + (login_button.width - tw) / 2),
                 (int)(login_button.y + (login_button.height - scaled_h) / 2), 22, WHITE);
    }
    /* Invisible click area */
    saved_border_w = GuiGetStyle(BUTTON, BORDER_WIDTH);
    saved_base_n = GuiGetStyle(BUTTON, BASE_COLOR_NORMAL);
    saved_base_f = GuiGetStyle(BUTTON, BASE_COLOR_FOCUSED);
    saved_base_p = GuiGetStyle(BUTTON, BASE_COLOR_PRESSED);
    saved_text_n = GuiGetStyle(BUTTON, TEXT_COLOR_NORMAL);
    GuiSetStyle(BUTTON, BORDER_WIDTH, 0);
    GuiSetStyle(BUTTON, BASE_COLOR_NORMAL, ColorToInt(BLANK));
    GuiSetStyle(BUTTON, BASE_COLOR_FOCUSED, ColorToInt(BLANK));
    GuiSetStyle(BUTTON, BASE_COLOR_PRESSED, ColorToInt(BLANK));
    GuiSetStyle(BUTTON, TEXT_COLOR_NORMAL, ColorToInt(BLANK));
    if (GuiButton(login_button, "")) {
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
    GuiSetStyle(BUTTON, BORDER_WIDTH, saved_border_w);
    GuiSetStyle(BUTTON, BASE_COLOR_NORMAL, saved_base_n);
    GuiSetStyle(BUTTON, BASE_COLOR_FOCUSED, saved_base_f);
    GuiSetStyle(BUTTON, BASE_COLOR_PRESSED, saved_base_p);
    GuiSetStyle(BUTTON, TEXT_COLOR_NORMAL, saved_text_n);

    /* ── Reset button (ghost/outline with hover) ── */
    {
        int reset_hovered = CheckCollisionPointRec(GetMousePosition(), reset_button);
        DrawRectangleRounded(reset_button, 0.18f, 8,
                             reset_hovered ? Fade(app->theme.border, 0.10f) : BLANK);
    }
    DrawRectangleRoundedLinesEx(reset_button, 0.18f, 8, 1.0f,
                                Fade(app->theme.border, 0.5f));
    {
        const char *reset_text = "重置演示数据";
        int tw = MeasureText(reset_text, 18);
        int scaled_h = (int)(18.0f * app->theme.scale_factor + 0.5f);
        DrawText(reset_text,
                 (int)(reset_button.x + (reset_button.width - tw) / 2),
                 (int)(reset_button.y + (reset_button.height - scaled_h) / 2), 18, app->theme.text_secondary);
    }
    /* Invisible click area for reset */
    saved_border_w = GuiGetStyle(BUTTON, BORDER_WIDTH);
    saved_base_n = GuiGetStyle(BUTTON, BASE_COLOR_NORMAL);
    saved_base_f = GuiGetStyle(BUTTON, BASE_COLOR_FOCUSED);
    saved_base_p = GuiGetStyle(BUTTON, BASE_COLOR_PRESSED);
    saved_text_n = GuiGetStyle(BUTTON, TEXT_COLOR_NORMAL);
    GuiSetStyle(BUTTON, BORDER_WIDTH, 0);
    GuiSetStyle(BUTTON, BASE_COLOR_NORMAL, ColorToInt(BLANK));
    GuiSetStyle(BUTTON, BASE_COLOR_FOCUSED, ColorToInt(BLANK));
    GuiSetStyle(BUTTON, BASE_COLOR_PRESSED, ColorToInt(BLANK));
    GuiSetStyle(BUTTON, TEXT_COLOR_NORMAL, ColorToInt(BLANK));
    if (GuiButton(reset_button, "")) {
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
    GuiSetStyle(BUTTON, BORDER_WIDTH, saved_border_w);
    GuiSetStyle(BUTTON, BASE_COLOR_NORMAL, saved_base_n);
    GuiSetStyle(BUTTON, BASE_COLOR_FOCUSED, saved_base_f);
    GuiSetStyle(BUTTON, BASE_COLOR_PRESSED, saved_base_p);
    GuiSetStyle(BUTTON, TEXT_COLOR_NORMAL, saved_text_n);

    /* ── Demo accounts reference panel ── */
    {
        float s = app->theme.scale_factor;
        Color hint_color = Fade(app->theme.text_secondary, 0.45f);
        float panel_x = reset_button.x;
        float panel_y = reset_button.y + reset_button.height + 16.0f * s;
        float col_width = (login_button.width) / 2.0f;
        int font_size = 14;
        int line_h = (int)(20.0f * s + 0.5f);
        int i;

        /* Section title */
        DrawText("演示账号",
                 (int)panel_x, (int)panel_y, 15, hint_color);
        panel_y += 22.0f * s;

        /* 5 accounts in 2 columns (3 left, 2 right) */
        {
            const char *left_col[] = {
                "患者: PAT0001 / patient123",
                "医生: DOC0001 / doctor123",
                "管理员: ADM0001 / admin123"
            };
            const char *right_col[] = {
                "住院管理: INP0001 / inpatient123",
                "药房: PHA0001 / pharmacy123"
            };

            for (i = 0; i < 3; i++) {
                DrawText(left_col[i],
                         (int)panel_x, (int)(panel_y + i * line_h),
                         font_size, hint_color);
            }
            for (i = 0; i < 2; i++) {
                DrawText(right_col[i],
                         (int)(panel_x + col_width), (int)(panel_y + i * line_h),
                         font_size, hint_color);
            }
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

    {
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
    DrawRectangle(0, 0, GetScreenWidth(), (int)(3.0f * app->theme.scale_factor + 0.5f), wb != 0 ? wb->accent : app->theme.nav_active);
    /* Subtle bottom shadow */
    DrawRectangle(0, app->theme.topbar_height, GetScreenWidth(), 1, Fade(app->theme.border, 0.5f));
    DrawRectangle(0, app->theme.topbar_height + 1, GetScreenWidth(), 1, Fade(app->theme.border, 0.2f));
    DrawRectangle(0, app->theme.topbar_height + 2, GetScreenWidth(), 1, Fade(app->theme.border, 0.08f));
    DrawText(title, (int)layout.title_bounds.x, (int)layout.title_bounds.y, 28, app->theme.text_primary);
    DrawCircle((int)layout.session_bounds.x - 12, (int)(layout.session_bounds.y + 9), 4, app->theme.success);
    DrawText(
        session_text,
        (int)layout.session_bounds.x,
        (int)layout.session_bounds.y,
        18,
        app->theme.text_secondary
    );
    DrawText(cached_time_text, (int)layout.time_bounds.x, (int)layout.time_bounds.y, 18, app->theme.text_secondary);

    /* Custom logout button */
    DrawRectangleRounded(layout.logout_bounds, 0.22f, 8, Fade(app->theme.error, 0.08f));
    DrawRectangleRoundedLinesEx(layout.logout_bounds, 0.22f, 8, 1.0f, Fade(app->theme.error, 0.3f));
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
    Workbench_draw_pending_calendar_popup(app);
}
