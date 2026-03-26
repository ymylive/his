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

void Workbench_draw_home_cards(
    const DesktopApp *app, Rectangle panel,
    const char *labels[4], const char *values[4], Color accent
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
