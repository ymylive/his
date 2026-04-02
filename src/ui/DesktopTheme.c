#include "ui/DesktopTheme.h"

#include <stdlib.h>
#include <string.h>

#include "DesktopGlyphSeed.h"
#include "raygui.h"

static Font g_desktop_font;
static int g_desktop_font_loaded = 0;

/* Platform-specific font paths */
#ifdef _WIN32
/* Windows font paths */
static const char *DESKTOP_CJK_FONT_CANDIDATES[] = {
    "C:/Windows/Fonts/NotoSansSC-VF.ttf",
    "C:/Windows/Fonts/msyhl.ttc",
    "C:/Windows/Fonts/msyhbd.ttc",
    "C:/Windows/Fonts/Deng.ttf",
    "C:/Windows/Fonts/Dengb.ttf",
    "C:/Windows/Fonts/simhei.ttf",
    "C:/Windows/Fonts/simfang.ttf",
    "C:/Windows/Fonts/simkai.ttf",
    "C:/Windows/Fonts/msyh.ttc",
    "C:/Windows/Fonts/simsun.ttc",
    "C:/Windows/Fonts/simsunb.ttf"
};
#elif __APPLE__
/* macOS font paths - prefer TTF over TTC for better compatibility */
static const char *DESKTOP_CJK_FONT_CANDIDATES[] = {
    /* System fonts - TTF files (better raylib compatibility) */
    "/System/Library/Fonts/Supplemental/Arial Unicode.ttf",
    "/Library/Fonts/Arial Unicode.ttf",
    /* System fonts - TTC files (may have compatibility issues) */
    "/System/Library/Fonts/STHeiti Light.ttc",
    "/System/Library/Fonts/STHeiti Medium.ttc",
    "/System/Library/Fonts/ヒラギノ角ゴシック W3.ttc",
    "/System/Library/Fonts/ヒラギノ角ゴシック W4.ttc",
    "/System/Library/Fonts/ヒラギノ明朝 ProN.ttc",
    /* User-installed fonts */
    "/Library/Fonts/NotoSansSC-Regular.ttf",
    "/Library/Fonts/NotoSansCJK-Regular.ttc",
    "/Library/Fonts/SourceHanSansSC-Regular.otf",
    /* Homebrew-installed fonts */
    "/usr/local/share/fonts/NotoSansSC-Regular.ttf",
    "/opt/homebrew/share/fonts/NotoSansSC-Regular.ttf",
    /* Fallback */
    "/System/Library/Fonts/PingFang.ttc"
};
#else
/* Linux font paths */
static const char *DESKTOP_CJK_FONT_CANDIDATES[] = {
    "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
    "/usr/share/fonts/truetype/noto/NotoSansCJK-Regular.ttc",
    "/usr/share/fonts/truetype/droid/DroidSansFallbackFull.ttf",
    "/usr/share/fonts/truetype/wqy/wqy-microhei.ttc",
    "/usr/share/fonts/truetype/wqy/wqy-zenhei.ttc",
    "/usr/share/fonts/opentype/noto/NotoSansSC-Regular.otf",
    "/usr/share/fonts/truetype/arphic/uming.ttc"
};
#endif

static const char *DESKTOP_CJK_UI_TEXT = HIS_DESKTOP_GLYPH_SEED_TEXT;

const char *DesktopTheme_cjk_glyph_seed_text(void) {
    return DESKTOP_CJK_UI_TEXT;
}

int DesktopTheme_candidate_font_count(void) {
    return (int)(sizeof(DESKTOP_CJK_FONT_CANDIDATES) / sizeof(DESKTOP_CJK_FONT_CANDIDATES[0]));
}

int DesktopTheme_has_cjk_seed_text(void) {
    return DESKTOP_CJK_UI_TEXT[0] != '\0' ? 1 : 0;
}

static int DesktopTheme_file_exists_raylib(const char *path) {
    return FileExists(path) ? 1 : 0;
}

static void DesktopTheme_copy_string(char *dst, size_t dst_size, const char *src) {
    if (dst == 0 || dst_size == 0) {
        return;
    }

    if (src == 0) {
        dst[0] = '\0';
        return;
    }

    strncpy(dst, src, dst_size - 1);
    dst[dst_size - 1] = '\0';
}

static int DesktopTheme_append_unique_codepoint(int *chars, int count, int codepoint) {
    int i = 0;
    for (i = 0; i < count; i++) {
        if (chars[i] == codepoint) {
            return count;
        }
    }
    chars[count] = codepoint;
    return count + 1;
}

static int DesktopTheme_build_font_charset(int **chars_out) {
    int *chars = 0;
    int codepoint_count = 0;
    int i = 0;
    int text_count = 0;
    int *text_codepoints = 0;
    const int max_capacity = 2048;

    if (chars_out == 0) {
        return 0;
    }

    *chars_out = 0;
    chars = (int *)malloc(sizeof(int) * max_capacity);
    if (chars == 0) {
        return 0;
    }

    for (i = 32; i <= 126; i++) {
        codepoint_count = DesktopTheme_append_unique_codepoint(chars, codepoint_count, i);
    }

    text_codepoints = LoadCodepoints(DesktopTheme_cjk_glyph_seed_text(), &text_count);
    if (text_codepoints != 0) {
        for (i = 0; i < text_count; i++) {
            if (codepoint_count >= max_capacity) {
                break;
            }
            codepoint_count = DesktopTheme_append_unique_codepoint(chars, codepoint_count, text_codepoints[i]);
        }
        UnloadCodepoints(text_codepoints);
    }

    *chars_out = chars;
    return codepoint_count;
}

DesktopTheme DesktopTheme_make_default(void) {
    DesktopTheme theme;

    theme.background = (Color){ 248, 250, 252, 255 };
    theme.panel = (Color){ 255, 255, 255, 255 };
    theme.panel_alt = (Color){ 241, 245, 249, 255 };
    theme.nav_background = (Color){ 226, 232, 240, 255 };
    theme.nav_active = (Color){ 37, 99, 235, 255 };
    theme.border = (Color){ 203, 213, 225, 255 };
    theme.text_primary = (Color){ 30, 41, 59, 255 };
    theme.text_secondary = (Color){ 100, 116, 139, 255 };
    theme.accent = (Color){ 249, 115, 22, 255 };
    theme.success = (Color){ 22, 163, 74, 255 };
    theme.warning = (Color){ 217, 119, 6, 255 };
    theme.error = (Color){ 220, 38, 38, 255 };
    theme.margin = 28;
    theme.spacing = 18;
    theme.sidebar_width = 248;
    theme.topbar_height = 84;
    theme.card_height = 132;

    return theme;
}

const char *DesktopTheme_resolve_cjk_font_path(DesktopThemeFileExistsFn file_exists) {
    int i = 0;
    const int candidate_count = DesktopTheme_candidate_font_count();

    if (file_exists == 0) {
        return 0;
    }

    for (i = 0; i < candidate_count; i++) {
        if (file_exists(DESKTOP_CJK_FONT_CANDIDATES[i]) != 0) {
            return DESKTOP_CJK_FONT_CANDIDATES[i];
        }
    }

    return 0;
}

int DesktopTheme_enable_cjk_font(int font_size, char *loaded_path, size_t loaded_path_size) {
    const char *font_path = DesktopTheme_resolve_cjk_font_path(DesktopTheme_file_exists_raylib);
    int *chars = 0;
    int chars_count = 0;
    Font font;

    DesktopTheme_copy_string(loaded_path, loaded_path_size, 0);
    g_desktop_font = GetFontDefault();
    g_desktop_font_loaded = 0;
    GuiSetFont(g_desktop_font);

    if (font_path == 0) {
        return 0;
    }

    chars_count = DesktopTheme_build_font_charset(&chars);
    if (chars_count <= 0 || chars == 0) {
        free(chars);
        return 0;
    }

    font = LoadFontEx(font_path, font_size > 0 ? font_size : 40, chars, chars_count);
    free(chars);

    if (font.texture.id == 0 || font.glyphCount == 0) {
        if (font.texture.id != 0) {
            UnloadFont(font);
        }
        return 0;
    }

    g_desktop_font = font;
    g_desktop_font_loaded = 1;
    SetTextureFilter(g_desktop_font.texture, TEXTURE_FILTER_BILINEAR);
    GuiSetFont(font);
    DesktopTheme_copy_string(loaded_path, loaded_path_size, font_path);
    return 1;
}

void DesktopTheme_shutdown_fonts(void) {
    if (g_desktop_font_loaded != 0) {
        UnloadFont(g_desktop_font);
    }
    g_desktop_font = GetFontDefault();
    g_desktop_font_loaded = 0;
    GuiSetFont(g_desktop_font);
}

void DesktopTheme_apply_raygui_style(const DesktopTheme *theme) {
    if (theme == 0) {
        return;
    }

    GuiSetStyle(DEFAULT, BORDER_COLOR_NORMAL, ColorToInt(theme->border));
    GuiSetStyle(DEFAULT, BASE_COLOR_NORMAL, ColorToInt(theme->panel));
    GuiSetStyle(DEFAULT, TEXT_COLOR_NORMAL, ColorToInt(theme->text_primary));
    GuiSetStyle(DEFAULT, BORDER_COLOR_FOCUSED, ColorToInt(theme->nav_active));
    GuiSetStyle(DEFAULT, BASE_COLOR_FOCUSED, ColorToInt(theme->panel_alt));
    GuiSetStyle(DEFAULT, TEXT_COLOR_FOCUSED, ColorToInt(theme->text_primary));
    GuiSetStyle(DEFAULT, BORDER_COLOR_PRESSED, ColorToInt(theme->nav_active));
    GuiSetStyle(DEFAULT, BASE_COLOR_PRESSED, ColorToInt(theme->panel_alt));
    GuiSetStyle(DEFAULT, TEXT_COLOR_PRESSED, ColorToInt(theme->text_primary));
    GuiSetStyle(DEFAULT, BORDER_COLOR_DISABLED, ColorToInt(Fade(theme->border, 0.75f)));
    GuiSetStyle(DEFAULT, BASE_COLOR_DISABLED, ColorToInt(Fade(theme->panel_alt, 0.7f)));
    GuiSetStyle(DEFAULT, TEXT_COLOR_DISABLED, ColorToInt(theme->text_secondary));
    GuiSetStyle(DEFAULT, LINE_COLOR, ColorToInt(theme->border));
    GuiSetStyle(DEFAULT, BACKGROUND_COLOR, ColorToInt(theme->background));
    GuiSetStyle(DEFAULT, TEXT_SIZE, 20);
    GuiSetStyle(DEFAULT, TEXT_SPACING, 1);
    GuiSetStyle(LABEL, TEXT_ALIGNMENT, TEXT_ALIGN_LEFT);
    GuiSetStyle(TEXTBOX, TEXT_ALIGNMENT, TEXT_ALIGN_LEFT);

    /* Enhanced widget styling for modern look */
    GuiSetStyle(DEFAULT, BORDER_WIDTH, 1);
    GuiSetStyle(DEFAULT, TEXT_PADDING, 10);

    /* Textbox: softer border, more padding */
    GuiSetStyle(TEXTBOX, BORDER_WIDTH, 1);
    GuiSetStyle(TEXTBOX, TEXT_PADDING, 12);
    GuiSetStyle(TEXTBOX, BORDER_COLOR_NORMAL, ColorToInt(Fade(theme->border, 0.7f)));
    GuiSetStyle(TEXTBOX, BASE_COLOR_NORMAL, ColorToInt((Color){ 255, 255, 255, 255 }));
    GuiSetStyle(TEXTBOX, BORDER_COLOR_FOCUSED, ColorToInt(theme->nav_active));
    GuiSetStyle(TEXTBOX, BASE_COLOR_FOCUSED, ColorToInt((Color){ 255, 255, 255, 255 }));

    /* Button: accent-colored when pressed */
    GuiSetStyle(BUTTON, BORDER_WIDTH, 1);
    GuiSetStyle(BUTTON, TEXT_PADDING, 16);
    GuiSetStyle(BUTTON, BORDER_COLOR_NORMAL, ColorToInt(Fade(theme->border, 0.6f)));
    GuiSetStyle(BUTTON, BASE_COLOR_NORMAL, ColorToInt(theme->panel));
    GuiSetStyle(BUTTON, BORDER_COLOR_FOCUSED, ColorToInt(theme->nav_active));
    GuiSetStyle(BUTTON, BASE_COLOR_FOCUSED, ColorToInt(Fade(theme->nav_active, 0.08f)));
    GuiSetStyle(BUTTON, BORDER_COLOR_PRESSED, ColorToInt(theme->nav_active));
    GuiSetStyle(BUTTON, BASE_COLOR_PRESSED, ColorToInt(Fade(theme->nav_active, 0.15f)));

    /* ListViewEx: clean list styling */
    GuiSetStyle(LISTVIEW, BORDER_WIDTH, 0);
    GuiSetStyle(LISTVIEW, TEXT_PADDING, 8);
    GuiSetStyle(LISTVIEW, BORDER_COLOR_NORMAL, ColorToInt(BLANK));
    GuiSetStyle(LISTVIEW, BASE_COLOR_NORMAL, ColorToInt(BLANK));

    /* Toggle: clean toggle buttons */
    GuiSetStyle(TOGGLE, BORDER_WIDTH, 1);
    GuiSetStyle(TOGGLE, TEXT_PADDING, 12);
    GuiSetStyle(TOGGLE, BORDER_COLOR_PRESSED, ColorToInt(theme->nav_active));
    GuiSetStyle(TOGGLE, BASE_COLOR_PRESSED, ColorToInt(Fade(theme->nav_active, 0.15f)));
    GuiSetStyle(TOGGLE, TEXT_COLOR_PRESSED, ColorToInt(theme->nav_active));

    /* Checkbox */
    GuiSetStyle(CHECKBOX, BORDER_WIDTH, 1);
    GuiSetStyle(CHECKBOX, BORDER_COLOR_PRESSED, ColorToInt(theme->nav_active));
    GuiSetStyle(CHECKBOX, BASE_COLOR_PRESSED, ColorToInt(theme->nav_active));
}

void DesktopTheme_draw_text(const char *text, int pos_x, int pos_y, int font_size, Color color) {
    Font font = g_desktop_font_loaded != 0 ? g_desktop_font : GetFontDefault();
    const char *safe_text = text != 0 ? text : "";

    DrawTextEx(
        font,
        safe_text,
        (Vector2){ (float)pos_x, (float)pos_y },
        (float)font_size,
        1.0f,
        color
    );
}
