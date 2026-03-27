#ifndef HIS_UI_DESKTOP_THEME_H
#define HIS_UI_DESKTOP_THEME_H

#include <stddef.h>

#include "raylib.h"

typedef struct DesktopTheme {
    Color background;
    Color panel;
    Color panel_alt;
    Color nav_background;
    Color nav_active;
    Color border;
    Color text_primary;
    Color text_secondary;
    Color accent;
    Color success;
    Color warning;
    Color error;
    int margin;
    int spacing;
    int sidebar_width;
    int topbar_height;
    int card_height;
} DesktopTheme;

typedef int (*DesktopThemeFileExistsFn)(const char *path);

DesktopTheme DesktopTheme_make_default(void);
const char *DesktopTheme_resolve_cjk_font_path(DesktopThemeFileExistsFn file_exists);
const char *DesktopTheme_cjk_glyph_seed_text(void);
int DesktopTheme_candidate_font_count(void);
int DesktopTheme_has_cjk_seed_text(void);
int DesktopTheme_enable_cjk_font(int font_size, char *loaded_path, size_t loaded_path_size);
void DesktopTheme_shutdown_fonts(void);
void DesktopTheme_apply_raygui_style(const DesktopTheme *theme);
void DesktopTheme_draw_text(const char *text, int pos_x, int pos_y, int font_size, Color color);

#ifndef HIS_DESKTOP_THEME_NO_DRAWTEXT_OVERRIDE
#define DrawText(text, pos_x, pos_y, font_size, color) \
    DesktopTheme_draw_text((text), (pos_x), (pos_y), (font_size), (color))
#endif

#endif
