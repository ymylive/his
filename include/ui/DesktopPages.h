#ifndef HIS_UI_DESKTOP_PAGES_H
#define HIS_UI_DESKTOP_PAGES_H

#include "ui/DesktopApp.h"

typedef struct DesktopTopbarLayout {
    Rectangle bar_bounds;
    Rectangle title_bounds;
    Rectangle session_bounds;
    Rectangle time_bounds;
    Rectangle logout_bounds;
    float gap;
} DesktopTopbarLayout;

typedef struct DesktopLoginLayout {
    Rectangle card_bounds;
    Rectangle user_box;
    Rectangle password_box;
    Rectangle role_prev_bounds;
    Rectangle role_value_bounds;
    Rectangle role_next_bounds;
    Rectangle login_button_bounds;
    float gap;
} DesktopLoginLayout;

DesktopTopbarLayout DesktopPages_compute_topbar_layout(
    int screen_width,
    const DesktopTheme *theme,
    float title_width,
    float session_width,
    float time_width
);
DesktopLoginLayout DesktopPages_compute_login_layout(
    int screen_width,
    int screen_height,
    const DesktopTheme *theme
);
void DesktopPages_draw(DesktopApp *app);

#endif
