#include "ui/Workbench.h"

#include <string.h>

#include "raygui.h"
#include "ui/DesktopApp.h"

/* ── Accent colors per role ── */
static const Color ACCENT_ADMIN     = { 30,  58, 138, 255 };  /* deep navy */
static const Color ACCENT_CLERK     = { 14, 165, 233, 255 };  /* cyan-blue */
static const Color ACCENT_DOCTOR    = { 13, 148, 136, 255 };  /* teal-green */
static const Color ACCENT_PATIENT   = { 56, 189, 248, 255 };  /* sky-blue */
static const Color ACCENT_INPATIENT = { 245, 158, 11, 255 };  /* amber */
static const Color ACCENT_WARD      = { 79,  70, 229, 255 };  /* indigo */
static const Color ACCENT_PHARMACY  = { 22, 163,  74, 255 };  /* herb green */

/* ── Workbench definitions ── */

static const WorkbenchDef g_workbenches[] = {
    {
        USER_ROLE_ADMIN, "系统管理员工作台", "主数据维护与系统总览",
        { 30, 58, 138, 255 },
        {
            { "首页总览", 0 }, { "患者主数据", 1 },
            { "医生与科室", 2 }, { "病房与药品", 3 },
            { "系统信息", 4 }
        },
        5, AdminWorkbench_draw
    },
    {
        USER_ROLE_REGISTRATION_CLERK, "挂号员工作台", "患者接待与挂号办理",
        { 14, 165, 233, 255 },
        {
            { "接待首页", 0 }, { "患者建档", 1 },
            { "患者查询", 2 }, { "挂号办理", 3 },
            { "挂号查询", 4 }
        },
        5, ClerkWorkbench_draw
    },
    {
        USER_ROLE_DOCTOR, "医生工作台", "接诊与诊疗记录",
        { 13, 148, 136, 255 },
        {
            { "接诊首页", 0 }, { "待诊列表", 1 },
            { "患者历史", 2 }, { "看诊记录", 3 },
            { "检查管理", 4 }
        },
        5, DoctorWorkbench_draw
    },
    {
        USER_ROLE_PATIENT, "我的服务台", "个人健康信息自助查询",
        { 56, 189, 248, 255 },
        {
            { "服务首页", 0 }, { "我的挂号", 1 },
            { "我的看诊", 2 }, { "我的住院", 3 },
            { "我的发药", 4 }
        },
        5, PatientWorkbench_draw
    },
    {
        USER_ROLE_INPATIENT_REGISTRAR, "住院登记工作台", "入院与出院办理",
        { 245, 158, 11, 255 },
        {
            { "住院首页", 0 }, { "入院登记", 1 },
            { "出院办理", 2 }, { "住院查询", 3 }
        },
        4, InpatientWorkbench_draw
    },
    {
        USER_ROLE_WARD_MANAGER, "病区管理工作台", "病房床位与转床调度",
        { 79, 70, 229, 255 },
        {
            { "病区首页", 0 }, { "病房总览", 1 },
            { "床位状态", 2 }, { "转床调度", 3 },
            { "出院检查", 4 }
        },
        5, WardWorkbench_draw
    },
    {
        USER_ROLE_PHARMACY, "药房工作台", "药品管理与发药处理",
        { 22, 163, 74, 255 },
        {
            { "药房首页", 0 }, { "药品建档", 1 },
            { "入库补货", 2 }, { "发药处理", 3 },
            { "库存查询", 4 }, { "低库存预警", 5 }
        },
        6, PharmacyWorkbench_draw
    }
};

static const int g_workbench_count = (int)(sizeof(g_workbenches) / sizeof(g_workbenches[0]));

static float Workbench_maxf(float a, float b) {
    return a > b ? a : b;
}

static float Workbench_minf(float a, float b) {
    return a < b ? a : b;
}

static float Workbench_clampf(float value, float min_value, float max_value) {
    return Workbench_maxf(min_value, Workbench_minf(value, max_value));
}

const WorkbenchDef *Workbench_get(UserRole role) {
    int i = 0;
    for (i = 0; i < g_workbench_count; i++) {
        if (g_workbenches[i].role == role) {
            return &g_workbenches[i];
        }
    }
    return &g_workbenches[0];
}

WorkbenchButtonGroupLayout Workbench_compute_button_group_layout(
    Rectangle bounds,
    int button_count,
    float min_button_width,
    float button_height,
    float preferred_gap
) {
    WorkbenchButtonGroupLayout layout;
    int index = 0;
    float gap = preferred_gap > 0.0f ? preferred_gap : 12.0f;
    float height = button_height > 0.0f ? button_height : bounds.height;
    float usable_width = 0.0f;
    float button_width = 0.0f;

    memset(&layout, 0, sizeof(layout));
    layout.count = button_count > 0 ? button_count : 0;
    if (layout.count > WORKBENCH_LAYOUT_BUTTON_MAX) {
        layout.count = WORKBENCH_LAYOUT_BUTTON_MAX;
    }
    if (layout.count <= 0) {
        return layout;
    }

    usable_width = bounds.width - gap * (float)(layout.count - 1);
    button_width = usable_width / (float)layout.count;
    layout.gap = gap;
    layout.mode = button_width >= min_button_width ? WORKBENCH_ACTION_LAYOUT_ROW : WORKBENCH_ACTION_LAYOUT_STACK;

    if (layout.mode == WORKBENCH_ACTION_LAYOUT_ROW) {
        for (index = 0; index < layout.count; index++) {
            layout.buttons[index] = (Rectangle){
                bounds.x + (button_width + gap) * (float)index,
                bounds.y,
                button_width,
                height
            };
        }
        return layout;
    }

    button_width = bounds.width;
    for (index = 0; index < layout.count; index++) {
        layout.buttons[index] = (Rectangle){
            bounds.x,
            bounds.y + (height + gap) * (float)index,
            button_width,
            height
        };
    }
    return layout;
}

WorkbenchInfoRowLayout Workbench_compute_info_row_layout(
    Rectangle bounds,
    float label_width,
    float preferred_gap
) {
    WorkbenchInfoRowLayout layout;
    float gap = preferred_gap > 0.0f ? preferred_gap : 12.0f;
    float clamped_label_width = Workbench_clampf(label_width, 72.0f, bounds.width);
    float value_width = bounds.width - clamped_label_width - gap;

    memset(&layout, 0, sizeof(layout));
    layout.gap = gap;
    layout.label_bounds = (Rectangle){
        bounds.x,
        bounds.y,
        clamped_label_width,
        bounds.height
    };
    if (value_width < 0.0f) {
        value_width = 0.0f;
    }
    layout.value_bounds = (Rectangle){
        bounds.x + clamped_label_width + gap,
        bounds.y,
        value_width,
        bounds.height
    };
    layout.wrap_lines = 1;
    if (value_width <= 240.0f && bounds.height >= 60.0f) {
        layout.wrap_lines = 2;
    }
    if (value_width <= 160.0f && bounds.height >= 90.0f) {
        layout.wrap_lines = 3;
    }
    return layout;
}

WorkbenchTextPanelLayout Workbench_compute_text_panel_layout(
    Rectangle bounds,
    float padding,
    float title_height,
    float content_gap
) {
    WorkbenchTextPanelLayout layout;
    float inset = padding > 0.0f ? padding : 12.0f;
    float header_height = title_height > 0.0f ? title_height : 24.0f;
    float gap = content_gap > 0.0f ? content_gap : 16.0f;
    float content_top = bounds.y + inset + header_height + gap;
    float content_height = bounds.height - (content_top - bounds.y) - inset;

    memset(&layout, 0, sizeof(layout));
    layout.gap = gap;
    layout.title_bounds = (Rectangle){
        bounds.x + inset,
        bounds.y + inset,
        bounds.width - inset * 2.0f,
        header_height
    };
    if (content_height < 0.0f) {
        content_height = 0.0f;
    }
    layout.content_bounds = (Rectangle){
        bounds.x + inset,
        content_top,
        bounds.width - inset * 2.0f,
        content_height
    };
    layout.wrap_width = layout.content_bounds.width;
    return layout;
}

WorkbenchListDetailLayout Workbench_compute_list_detail_layout(
    Rectangle bounds,
    float list_ratio,
    float preferred_gap,
    float min_column_width
) {
    WorkbenchListDetailLayout layout;
    float gap = preferred_gap > 0.0f ? preferred_gap : 20.0f;
    float min_width = min_column_width > 0.0f ? min_column_width : 240.0f;
    float available_width = bounds.width - gap;
    float left_width = 0.0f;
    float top_height = 0.0f;

    memset(&layout, 0, sizeof(layout));
    layout.gap = gap;

    if (available_width >= min_width * 2.0f) {
        left_width = available_width * Workbench_clampf(list_ratio, 0.35f, 0.65f);
        if (left_width < min_width) {
            left_width = min_width;
        }
        if ((available_width - left_width) < min_width) {
            left_width = available_width - min_width;
        }
        layout.list_bounds = (Rectangle){ bounds.x, bounds.y, left_width, bounds.height };
        layout.detail_bounds = (Rectangle){
            bounds.x + left_width + gap,
            bounds.y,
            bounds.width - left_width - gap,
            bounds.height
        };
        layout.is_stacked = 0;
        return layout;
    }

    top_height = (bounds.height - gap) * 0.46f;
    layout.list_bounds = (Rectangle){ bounds.x, bounds.y, bounds.width, top_height };
    layout.detail_bounds = (Rectangle){
        bounds.x,
        bounds.y + top_height + gap,
        bounds.width,
        bounds.height - top_height - gap
    };
    layout.is_stacked = 1;
    return layout;
}

/* ── Sidebar with role-specific nav ── */

void Workbench_draw_sidebar(DesktopApp *app, const WorkbenchDef *wb) {
    int i = 0;
    Rectangle btn;

    if (app == 0 || wb == 0) {
        return;
    }

    DrawRectangle(0, app->theme.topbar_height, app->theme.sidebar_width,
                  GetScreenHeight() - app->theme.topbar_height, app->theme.nav_background);
    DrawLine(app->theme.sidebar_width - 1, app->theme.topbar_height,
             app->theme.sidebar_width - 1, GetScreenHeight(), app->theme.border);

    /* Role accent stripe */
    DrawRectangle(0, app->theme.topbar_height, 4,
                  GetScreenHeight() - app->theme.topbar_height, wb->accent);

    /* Role title */
    DrawText(wb->title, 18, app->theme.topbar_height + 18, 20, app->theme.text_primary);
    DrawText(wb->subtitle, 18, app->theme.topbar_height + 44, 16, app->theme.text_secondary);

    for (i = 0; i < wb->nav_count; i++) {
        btn = (Rectangle){
            18.0f,
            (float)(app->theme.topbar_height + 78 + i * 52),
            (float)(app->theme.sidebar_width - 36),
            40.0f
        };

        if (app->state.workbench_page == wb->nav[i].page_id) {
            DrawRectangleRounded(btn, 0.22f, 8, Fade(wb->accent, 0.18f));
            DrawRectangle((int)btn.x, (int)btn.y, 4, (int)btn.height, wb->accent);
        }

        if (GuiButton(btn, wb->nav[i].label)) {
            app->state.workbench_page = wb->nav[i].page_id;
        }
    }
}

/* ── Metric card (text value variant) ── */

void Workbench_draw_metric_card(
    const DesktopApp *app, Rectangle rect,
    const char *label, const char *value, Color accent
) {
    if (app == 0) return;

    DrawRectangleRounded(rect, 0.18f, 8, app->theme.panel);
    DrawRectangleRoundedLinesEx(rect, 0.18f, 8, 1.0f, Fade(app->theme.border, 0.7f));
    DrawRectangle((int)rect.x, (int)rect.y + 4, 5, (int)rect.height - 8, accent);
    DrawText(label, (int)rect.x + 20, (int)rect.y + 14, 17, app->theme.text_secondary);
    DrawText(value != 0 ? value : "--", (int)rect.x + 20, (int)rect.y + 42, 30, app->theme.text_primary);
}

/* ── Section header ── */

void Workbench_draw_section_header(
    const DesktopApp *app, int x, int y, const char *title
) {
    if (app == 0) return;
    DrawText(title, x, y, 20, app->theme.text_primary);
    DrawLine(x, y + 26, x + 320, y + 26, Fade(app->theme.border, 0.6f));
}

/* ── Quick action button ── */

void Workbench_draw_quick_action_btn(
    DesktopApp *app, Rectangle rect,
    const char *label, Color accent, int *clicked
) {
    if (app == 0 || clicked == 0) return;

    DrawRectangleRounded(rect, 0.22f, 8, Fade(accent, 0.10f));
    DrawRectangleRoundedLinesEx(rect, 0.22f, 8, 1.0f, Fade(accent, 0.35f));
    *clicked = GuiButton(rect, label);
}

/* ── Info row (label: value) ── */

void Workbench_draw_info_row(
    const DesktopApp *app, int x, int y,
    const char *label, const char *value
) {
    WorkbenchInfoRowLayout layout;

    if (app == 0) return;
    layout = Workbench_compute_info_row_layout(
        (Rectangle){ (float)x, (float)y, (float)(GetScreenWidth() - x - 32), 28.0f },
        110.0f,
        10.0f
    );
    DrawText(label, (int)layout.label_bounds.x, (int)layout.label_bounds.y, 18, app->theme.text_secondary);
    DrawText(
        value != 0 ? value : "--",
        (int)layout.value_bounds.x,
        (int)layout.value_bounds.y,
        18,
        app->theme.text_primary
    );
}

/* ── Home page 4-card row ── */

void Workbench_draw_home_cards(
    const DesktopApp *app, Rectangle panel,
    const char *labels[4], const char *values[4], Color accent
) {
    int i = 0;
    float cw = (panel.width - 18.0f * 3) / 4.0f;
    float ch = 80.0f;

    for (i = 0; i < 4; i++) {
        Rectangle card = {
            panel.x + i * (cw + 18.0f),
            panel.y,
            cw, ch
        };
        Workbench_draw_metric_card(app, card, labels[i], values[i], accent);
    }
}
