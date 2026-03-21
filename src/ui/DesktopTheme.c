#include "ui/DesktopTheme.h"

#include <stdlib.h>
#include <string.h>

#include "raygui.h"

static Font g_desktop_font;
static int g_desktop_font_loaded = 0;

static const char *DESKTOP_CJK_FONT_CANDIDATES[] = {
    "C:/Windows/Fonts/NotoSansSC-VF.ttf",
    "C:/Windows/Fonts/simhei.ttf",
    "C:/Windows/Fonts/simkai.ttf",
    "C:/Windows/Fonts/msyh.ttc",
    "C:/Windows/Fonts/simsun.ttc"
};

static const char *DESKTOP_CJK_UI_TEXT =
    "登录首页患者挂号发药记录系统未知角色医生系统管理员挂号员住院登记员病区管理员药房人员"
    "轻量级桌面控制页原生工作台用户编号密码退出已退出登录最近暂无当前总数低库存药品刷新查询详情"
    "请选择左侧查看录入当前角色无权进行成功科室时间挂号记录发药记录住院状态联系方式过敏史病史"
    "更稳的登录体验现代医疗后台医生工作台住院管理药房工作台患者查询患者列表详情面板等待选择患者"
    "挂号录入最近成功挂号编号医生编号患者编号历史查询诊疗记录主诉诊断建议需要检查需要住院需要用药"
    "提交诊疗检查记录新增检查回写结果医生操作输出病区床位查询查看病房查看床位入院登记办理入院"
    "出院办理办理出院状态查询查当前住院查床位患者住院与床位住院操作输出病房床位摘要药品新增药品"
    "库存操作查库存入库低库存提醒发药操作执行发药药房操作输出库存或阈值格式无效入库数量格式无效"
    "发药数量格式无效当前患者当前用户信息系统信息桌面后台结果显示";

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
    const int max_capacity = 1024;

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

    text_codepoints = LoadCodepoints(DESKTOP_CJK_UI_TEXT, &text_count);
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
    const int candidate_count = (int)(sizeof(DESKTOP_CJK_FONT_CANDIDATES) / sizeof(DESKTOP_CJK_FONT_CANDIDATES[0]));

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
