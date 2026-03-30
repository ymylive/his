#ifndef HIS_UI_WORKBENCH_H
#define HIS_UI_WORKBENCH_H

#include "domain/User.h"
#include "raylib.h"

#define WORKBENCH_NAV_MAX 8
#define WORKBENCH_OUTPUT_CAPACITY 8192

typedef struct DesktopApp DesktopApp;

typedef struct WorkbenchNavItem {
    const char *label;
    int page_id;
} WorkbenchNavItem;

typedef void (*WorkbenchDrawFn)(DesktopApp *app, Rectangle content, int page_id);

typedef struct WorkbenchDef {
    UserRole role;
    const char *title;
    const char *subtitle;
    Color accent;
    WorkbenchNavItem nav[WORKBENCH_NAV_MAX];
    int nav_count;
    WorkbenchDrawFn draw;
} WorkbenchDef;

typedef enum WorkbenchActionLayoutMode {
    WORKBENCH_ACTION_LAYOUT_ROW = 0,
    WORKBENCH_ACTION_LAYOUT_STACK = 1
} WorkbenchActionLayoutMode;

#define WORKBENCH_LAYOUT_BUTTON_MAX 8

typedef struct WorkbenchButtonGroupLayout {
    Rectangle buttons[WORKBENCH_LAYOUT_BUTTON_MAX];
    int count;
    float gap;
    WorkbenchActionLayoutMode mode;
} WorkbenchButtonGroupLayout;

typedef struct WorkbenchInfoRowLayout {
    Rectangle label_bounds;
    Rectangle value_bounds;
    float gap;
    int wrap_lines;
} WorkbenchInfoRowLayout;

typedef struct WorkbenchTextPanelLayout {
    Rectangle title_bounds;
    Rectangle content_bounds;
    float wrap_width;
    float gap;
} WorkbenchTextPanelLayout;

typedef struct WorkbenchListDetailLayout {
    Rectangle list_bounds;
    Rectangle detail_bounds;
    float gap;
    int is_stacked;
} WorkbenchListDetailLayout;

/* Registry */
const WorkbenchDef *Workbench_get(UserRole role);

/* Common drawing helpers */
void Workbench_draw_sidebar(DesktopApp *app, const WorkbenchDef *wb);

void Workbench_draw_metric_card(
    const DesktopApp *app, Rectangle rect,
    const char *label, const char *value, Color accent
);

void Workbench_draw_section_header(
    const DesktopApp *app, int x, int y, const char *title
);

void Workbench_draw_quick_action_btn(
    DesktopApp *app, Rectangle rect,
    const char *label, Color accent, int *clicked
);

void Workbench_draw_info_row(
    const DesktopApp *app, int x, int y,
    const char *label, const char *value
);

/* Enhanced UI components */
void Workbench_draw_form_label(
    const DesktopApp *app, int x, int y,
    const char *label, int required
);

void Workbench_draw_status_badge(
    const DesktopApp *app, Rectangle rect,
    const char *text, Color bg_color
);

void Workbench_draw_bed_grid(
    const DesktopApp *app, Rectangle panel,
    const char *ward_name, int bed_count,
    const int *occupied_beds, int occupied_count
);

void Workbench_draw_data_table_header(
    const DesktopApp *app, Rectangle rect,
    const char **headers, const float *widths, int column_count
);

void Workbench_draw_data_table_row(
    DesktopApp *app, Rectangle rect,
    const char **values, const float *widths, int column_count,
    int is_selected, int *clicked
);

void Workbench_draw_icon_button(
    DesktopApp *app, Rectangle rect,
    const char *icon, const char *label, Color accent, int *clicked
);

void Workbench_draw_progress_bar(
    const DesktopApp *app, Rectangle rect,
    float progress, Color fill_color
);

void Workbench_draw_search_box(
    DesktopApp *app, Rectangle rect,
    char *text, int text_size, int *active_field, int field_id,
    int *search_clicked
);

void Workbench_draw_home_cards(
    const DesktopApp *app, Rectangle panel,
    const char *labels[4], const char *values[4], Color accent
);
WorkbenchButtonGroupLayout Workbench_compute_button_group_layout(
    Rectangle bounds,
    int button_count,
    float min_button_width,
    float button_height,
    float preferred_gap
);
WorkbenchInfoRowLayout Workbench_compute_info_row_layout(
    Rectangle bounds,
    float label_width,
    float preferred_gap
);
WorkbenchTextPanelLayout Workbench_compute_text_panel_layout(
    Rectangle bounds,
    float padding,
    float title_height,
    float content_gap
);
WorkbenchListDetailLayout Workbench_compute_list_detail_layout(
    Rectangle bounds,
    float list_ratio,
    float preferred_gap,
    float min_column_width
);

/* PLACEHOLDER_WORKBENCH_DECLS */

/* Workbench draw entry points */
void AdminWorkbench_draw(DesktopApp *app, Rectangle panel, int page);
void ClerkWorkbench_draw(DesktopApp *app, Rectangle panel, int page);
void DoctorWorkbench_draw(DesktopApp *app, Rectangle panel, int page);
void PatientWorkbench_draw(DesktopApp *app, Rectangle panel, int page);
void InpatientWorkbench_draw(DesktopApp *app, Rectangle panel, int page);
void WardWorkbench_draw(DesktopApp *app, Rectangle panel, int page);
void PharmacyWorkbench_draw(DesktopApp *app, Rectangle panel, int page);

#endif
