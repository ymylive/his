#include "ui/TuiStyle.h"

#include <string.h>

/* ═══════════════════════════════════════════════════════════════
 *  Role Theme System
 * ═══════════════════════════════════════════════════════════════ */

const char *tui_role_color(TuiRoleTheme theme) {
    switch (theme) {
        case TUI_THEME_ADMIN:    return TUI_BOLD_RED;
        case TUI_THEME_DOCTOR:   return TUI_BOLD_GREEN;
        case TUI_THEME_PATIENT:  return TUI_BOLD_BLUE;
        case TUI_THEME_INPATIENT:return TUI_BOLD_MAGENTA;
        case TUI_THEME_PHARMACY: return TUI_BOLD_YELLOW;
        default:                 return TUI_BOLD_CYAN;
    }
}

const char *tui_role_bg(TuiRoleTheme theme) {
    switch (theme) {
        case TUI_THEME_ADMIN:    return TUI_BG_RED;
        case TUI_THEME_DOCTOR:   return TUI_BG_GREEN;
        case TUI_THEME_PATIENT:  return TUI_BG_BLUE;
        case TUI_THEME_INPATIENT:return TUI_BG_MAGENTA;
        case TUI_THEME_PHARMACY: return TUI_BG_YELLOW;
        default:                 return TUI_BG_CYAN;
    }
}

const char *tui_role_icon(TuiRoleTheme theme) {
    switch (theme) {
        case TUI_THEME_ADMIN:    return TUI_GEAR;
        case TUI_THEME_DOCTOR:   return TUI_MEDICAL;
        case TUI_THEME_PATIENT:  return TUI_HEART;
        case TUI_THEME_INPATIENT:return TUI_CIRCLE;
        case TUI_THEME_PHARMACY: return TUI_FLASK;
        default:                 return TUI_STAR;
    }
}

const char *tui_role_label(TuiRoleTheme theme) {
    switch (theme) {
        case TUI_THEME_ADMIN:    return "\xe7\xae\xa1\xe7\x90\x86\xe5\x91\x98"; /* 管理员 */
        case TUI_THEME_DOCTOR:   return "\xe5\x8c\xbb\xe7\x94\x9f";             /* 医生 */
        case TUI_THEME_PATIENT:  return "\xe6\x82\xa3\xe8\x80\x85";             /* 患者 */
        case TUI_THEME_INPATIENT:return "\xe4\xbd\x8f\xe9\x99\xa2";             /* 住院 */
        case TUI_THEME_PHARMACY: return "\xe8\x8d\xaf\xe6\x88\xbf";             /* 药房 */
        default:                 return "HIS";
    }
}

/* ═══════════════════════════════════════════════════════════════
 *  Screen Operations
 * ═══════════════════════════════════════════════════════════════ */

void tui_clear_screen(void) {
    fputs("\033[2J\033[H", stdout);
    fflush(stdout);
}

void tui_move_cursor(FILE *out, int row, int col) {
    if (out == 0) return;
    fprintf(out, "\033[%d;%dH", row, col);
}

void tui_hide_cursor(FILE *out) {
    if (out == 0) return;
    fputs("\033[?25l", out);
    fflush(out);
}

void tui_show_cursor(FILE *out) {
    if (out == 0) return;
    fputs("\033[?25h", out);
    fflush(out);
}

/* ═══════════════════════════════════════════════════════════════
 *  Display Width (CJK-aware)
 * ═══════════════════════════════════════════════════════════════ */

int tui_display_width(const char *text) {
    int width = 0;
    const unsigned char *p = (const unsigned char *)text;

    if (text == 0) return 0;

    while (*p != 0) {
        if (*p < 0x80) {
            width += 1;
            p += 1;
        } else if (*p < 0xC0) {
            p += 1;
        } else if (*p < 0xE0) {
            width += 1;
            p += 2;
        } else if (*p < 0xF0) {
            width += 2;
            p += 3;
        } else {
            width += 2;
            p += 4;
        }
    }
    return width;
}

void tui_pad_right(FILE *out, const char *text, int width) {
    int dw = 0;
    int i = 0;

    if (out == 0) return;
    if (text == 0) text = "";

    dw = tui_display_width(text);
    fputs(text, out);
    for (i = 0; i < width - dw; i++) fputc(' ', out);
}

static void tui_repeat(FILE *out, const char *str, int count) {
    int i = 0;
    for (i = 0; i < count; i++) fputs(str, out);
}

/* ═══════════════════════════════════════════════════════════════
 *  Logo & Banner
 * ═══════════════════════════════════════════════════════════════ */

/* Gradient colors for the logo (red -> yellow -> green -> cyan -> blue -> magenta) */
static const char *GRADIENT_COLORS[] = {
    "\033[1;31m",  /* red */
    "\033[1;33m",  /* yellow */
    "\033[1;32m",  /* green */
    "\033[1;36m",  /* cyan */
    "\033[1;34m",  /* blue */
    "\033[1;35m"   /* magenta */
};
#define GRADIENT_COUNT 6

static const char *LOGO_LINES[] = {
    "  \xe2\x96\x88\xe2\x96\x88\xe2\x95\x97  \xe2\x96\x88\xe2\x96\x88\xe2\x95\x97 \xe2\x96\x88\xe2\x96\x88\xe2\x95\x97 \xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x95\x97",
    "  \xe2\x96\x88\xe2\x96\x88\xe2\x95\x91  \xe2\x96\x88\xe2\x96\x88\xe2\x95\x91 \xe2\x96\x88\xe2\x96\x88\xe2\x95\x91 \xe2\x96\x88\xe2\x96\x88\xe2\x95\x94\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x9d",
    "  \xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x95\x91 \xe2\x96\x88\xe2\x96\x88\xe2\x95\x91 \xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x95\x97",
    "  \xe2\x96\x88\xe2\x96\x88\xe2\x95\x94\xe2\x95\x90\xe2\x95\x90\xe2\x96\x88\xe2\x96\x88\xe2\x95\x91 \xe2\x96\x88\xe2\x96\x88\xe2\x95\x91 \xe2\x95\x9a\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x96\x88\xe2\x96\x88\xe2\x95\x91",
    "  \xe2\x96\x88\xe2\x96\x88\xe2\x95\x91  \xe2\x96\x88\xe2\x96\x88\xe2\x95\x91 \xe2\x96\x88\xe2\x96\x88\xe2\x95\x91 \xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x95\x91",
    "  \xe2\x95\x9a\xe2\x95\x90\xe2\x95\x9d  \xe2\x95\x9a\xe2\x95\x90\xe2\x95\x9d \xe2\x95\x9a\xe2\x95\x90\xe2\x95\x9d \xe2\x95\x9a\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x9d"
};
#define LOGO_LINE_COUNT 6

void tui_print_logo(FILE *out) {
    int i = 0;
    if (out == 0) return;

    fputc('\n', out);
    for (i = 0; i < LOGO_LINE_COUNT; i++) {
        fputs(GRADIENT_COLORS[i % GRADIENT_COUNT], out);
        fputs(LOGO_LINES[i], out);
        fputs(TUI_RESET, out);
        fputc('\n', out);
    }
}

void tui_print_gradient_text(FILE *out, const char *text) {
    const unsigned char *p = (const unsigned char *)text;
    int idx = 0;

    if (out == 0 || text == 0) return;

    while (*p != 0) {
        fputs(GRADIENT_COLORS[idx % GRADIENT_COUNT], out);
        if (*p < 0x80) {
            fputc(*p, out);
            p += 1;
        } else if (*p < 0xE0) {
            fputc(p[0], out);
            fputc(p[1], out);
            p += 2;
        } else if (*p < 0xF0) {
            fputc(p[0], out);
            fputc(p[1], out);
            fputc(p[2], out);
            p += 3;
            idx++;
        } else {
            fputc(p[0], out);
            fputc(p[1], out);
            fputc(p[2], out);
            fputc(p[3], out);
            p += 4;
            idx++;
        }
        if (*p >= 0x80) idx++;
    }
    fputs(TUI_RESET, out);
}

void tui_print_banner(FILE *out, const char *title) {
    int title_w = 0;
    int box_w = 0;
    int pad_left = 0;
    int pad_right = 0;
    int i = 0;

    if (out == 0 || title == 0) return;

    title_w = tui_display_width(title);
    box_w = title_w + 6;
    if (box_w < 42) box_w = 42;

    pad_left = (box_w - 2 - title_w) / 2;
    pad_right = box_w - 2 - title_w - pad_left;

    /* Top border with gradient */
    fputs(TUI_BOLD_CYAN, out);
    fputs("  ", out);
    fputs(TUI_DTL, out);
    tui_repeat(out, TUI_DH, box_w - 2);
    fputs(TUI_DTR, out);
    fputs(TUI_RESET, out);
    fputc('\n', out);

    /* Decorative line */
    fputs("  ", out);
    fputs(TUI_BOLD_CYAN TUI_DV TUI_RESET, out);
    fputs(" ", out);
    for (i = 0; i < box_w - 4; i++) {
        fputs(GRADIENT_COLORS[i % GRADIENT_COUNT], out);
        fputs(TUI_BLOCK_UPPER, out);
    }
    fputs(TUI_RESET, out);
    fputs(" ", out);
    fputs(TUI_BOLD_CYAN TUI_DV TUI_RESET, out);
    fputc('\n', out);

    /* Title line */
    fputs("  ", out);
    fputs(TUI_BOLD_CYAN TUI_DV TUI_RESET, out);
    for (i = 0; i < pad_left; i++) fputc(' ', out);
    fputs(TUI_BOLD_WHITE, out);
    fputs(title, out);
    fputs(TUI_RESET, out);
    for (i = 0; i < pad_right; i++) fputc(' ', out);
    fputs(TUI_BOLD_CYAN TUI_DV TUI_RESET, out);
    fputc('\n', out);

    /* Subtitle line */
    {
        const char *sub = TUI_HEAVY_CROSS " Hospital Information System v2.0";
        int sub_w = tui_display_width(sub);
        int spad_l = (box_w - 2 - sub_w) / 2;
        int spad_r = box_w - 2 - sub_w - spad_l;
        fputs("  ", out);
        fputs(TUI_BOLD_CYAN TUI_DV TUI_RESET, out);
        for (i = 0; i < spad_l; i++) fputc(' ', out);
        fputs(TUI_DIM, out);
        fputs(sub, out);
        fputs(TUI_RESET, out);
        for (i = 0; i < spad_r; i++) fputc(' ', out);
        fputs(TUI_BOLD_CYAN TUI_DV TUI_RESET, out);
        fputc('\n', out);
    }

    /* Decorative line */
    fputs("  ", out);
    fputs(TUI_BOLD_CYAN TUI_DV TUI_RESET, out);
    fputs(" ", out);
    for (i = 0; i < box_w - 4; i++) {
        fputs(GRADIENT_COLORS[(box_w - 4 - i) % GRADIENT_COUNT], out);
        fputs(TUI_BLOCK_LOWER, out);
    }
    fputs(TUI_RESET, out);
    fputs(" ", out);
    fputs(TUI_BOLD_CYAN TUI_DV TUI_RESET, out);
    fputc('\n', out);

    /* Bottom border */
    fputs("  ", out);
    fputs(TUI_BOLD_CYAN, out);
    fputs(TUI_DBL, out);
    tui_repeat(out, TUI_DH, box_w - 2);
    fputs(TUI_DBR, out);
    fputs(TUI_RESET, out);
    fputc('\n', out);
    fputc('\n', out);
}

void tui_print_welcome(FILE *out, TuiRoleTheme theme, const char *user_id) {
    const char *color = tui_role_color(theme);
    const char *icon = tui_role_icon(theme);
    const char *label = tui_role_label(theme);

    if (out == 0) return;

    fputc('\n', out);
    fputs("  ", out);
    fputs(color, out);
    fputs(TUI_HTL, out);
    tui_repeat(out, TUI_HH, 40);
    fputs(TUI_HTR, out);
    fputs(TUI_RESET, out);
    fputc('\n', out);

    fputs("  ", out);
    fputs(color, out);
    fputs(TUI_HV, out);
    fputs(TUI_RESET, out);
    fprintf(out, "  %s %s%s%s", TUI_SPARKLE, TUI_BOLD_WHITE, "\xe6\xac\xa2\xe8\xbf\x8e\xe7\x99\xbb\xe5\xbd\x95" /* 欢迎登录 */, TUI_RESET);
    fputs("                       ", out);
    fputs(color, out);
    fputs(TUI_HV, out);
    fputs(TUI_RESET, out);
    fputc('\n', out);

    fputs("  ", out);
    fputs(color, out);
    fputs(TUI_HV, out);
    fputs(TUI_RESET, out);
    fprintf(out, "  %s %s %s%s%s [%s]",
        icon, color, label, TUI_RESET, TUI_DIM, user_id);
    {
        char tmp[128];
        int tw = 0;
        snprintf(tmp, sizeof(tmp), "  %s %s [%s]", icon, label, user_id);
        tw = tui_display_width(tmp);
        { int pad = 40 - tw; int j; for (j = 0; j < pad; j++) fputc(' ', out); }
    }
    fputs(TUI_RESET, out);
    fputs(color, out);
    fputs(TUI_HV, out);
    fputs(TUI_RESET, out);
    fputc('\n', out);

    fputs("  ", out);
    fputs(color, out);
    fputs(TUI_HBL, out);
    tui_repeat(out, TUI_HH, 40);
    fputs(TUI_HBR, out);
    fputs(TUI_RESET, out);
    fputc('\n', out);
    fputc('\n', out);
}

void tui_print_goodbye(FILE *out) {
    if (out == 0) return;

    fputc('\n', out);
    fputs("  ", out);
    fputs(TUI_DIM, out);
    tui_repeat(out, TUI_H, 42);
    fputs(TUI_RESET, out);
    fputc('\n', out);

    fputs("  ", out);
    tui_print_gradient_text(out, "\xe6\x84\x9f\xe8\xb0\xa2\xe4\xbd\xbf\xe7\x94\xa8" /* 感谢使用 */);
    fputs(TUI_BOLD_WHITE " HIS " TUI_RESET, out);
    tui_print_gradient_text(out, "\xe7\xb3\xbb\xe7\xbb\x9f" /* 系统 */);
    fputs(TUI_DIM " - ", out);
    tui_print_gradient_text(out, "\xe5\x86\x8d\xe8\xa7\x81" /* 再见 */);
    fprintf(out, " %s", TUI_SPARKLE);
    fputs(TUI_RESET, out);
    fputc('\n', out);

    fputs("  ", out);
    fputs(TUI_DIM, out);
    tui_repeat(out, TUI_H, 42);
    fputs(TUI_RESET, out);
    fputc('\n', out);
    fputc('\n', out);
}

/* ═══════════════════════════════════════════════════════════════
 *  Status Bar & Headers
 * ═══════════════════════════════════════════════════════════════ */

void tui_print_status_bar(FILE *out, const char *role, const char *user_id) {
    if (out == 0) return;

    fputs(TUI_BOLD TUI_BG_BLUE TUI_WHITE, out);
    fprintf(out, " %s ", TUI_BULLET);
    if (role != 0 && user_id != 0) {
        fprintf(out, "%s [%s]", role, user_id);
    } else {
        fputs("\xe6\x9c\xaa\xe7\x99\xbb\xe5\xbd\x95" /* 未登录 */, out);
    }
    fputs("  " TUI_RESET, out);
    fputc('\n', out);
}

void tui_print_status_bar_themed(FILE *out, TuiRoleTheme theme, const char *user_id) {
    const char *bg = tui_role_bg(theme);
    const char *icon = tui_role_icon(theme);
    const char *label = tui_role_label(theme);

    if (out == 0) return;

    /* Left accent block */
    fputs(tui_role_color(theme), out);
    fputs(TUI_BLOCK_FULL TUI_BLOCK_FULL, out);
    fputs(TUI_RESET, out);

    /* Main bar */
    fputs(TUI_BOLD, out);
    fputs(bg, out);
    fputs(TUI_WHITE, out);
    fprintf(out, " %s %s ", icon, label);
    fputs(TUI_RESET, out);

    /* User ID section */
    fputs(TUI_BOLD TUI_BG_BLUE TUI_WHITE, out);
    if (user_id != 0) {
        fprintf(out, " %s %s ", TUI_DOT, user_id);
    }
    fputs(TUI_RESET, out);

    /* Right fade */
    fputs(tui_role_color(theme), out);
    fputs(" " TUI_BLOCK_DARK TUI_BLOCK_MED TUI_BLOCK_LIGHT, out);
    fputs(TUI_RESET, out);
    fputc('\n', out);
}

void tui_print_header(FILE *out, const char *title, int width) {
    int title_w = 0;
    int pad_left = 0;
    int pad_right = 0;
    int i = 0;

    if (out == 0 || title == 0) return;
    if (width < 20) width = 20;

    title_w = tui_display_width(title);
    pad_left = (width - 2 - title_w) / 2;
    pad_right = width - 2 - title_w - pad_left;

    fputs(TUI_BOLD_CYAN, out);
    fputs(TUI_TL, out);
    tui_repeat(out, TUI_H, width - 2);
    fputs(TUI_TR, out);
    fputc('\n', out);

    fputs(TUI_V, out);
    for (i = 0; i < pad_left; i++) fputc(' ', out);
    fputs(TUI_BOLD_WHITE, out);
    fputs(title, out);
    fputs(TUI_BOLD_CYAN, out);
    for (i = 0; i < pad_right; i++) fputc(' ', out);
    fputs(TUI_V, out);
    fputc('\n', out);

    fputs(TUI_LT, out);
    tui_repeat(out, TUI_H, width - 2);
    fputs(TUI_RT, out);
    fputs(TUI_RESET, out);
    fputc('\n', out);
}

void tui_print_section(FILE *out, const char *icon, const char *title) {
    if (out == 0 || title == 0) return;

    fputc('\n', out);
    fputs("  ", out);
    if (icon != 0) {
        fputs(TUI_BOLD_CYAN, out);
        fputs(icon, out);
        fputs(TUI_RESET, out);
        fputc(' ', out);
    }
    fputs(TUI_BOLD_WHITE, out);
    fputs(title, out);
    fputs(TUI_RESET, out);
    fputc('\n', out);
    fputs("  ", out);
    fputs(TUI_DIM, out);
    tui_repeat(out, TUI_H, 38);
    fputs(TUI_RESET, out);
    fputc('\n', out);
}

void tui_print_hline(FILE *out, int width) {
    if (out == 0) return;
    fputs(TUI_DIM, out);
    tui_repeat(out, TUI_H, width);
    fputs(TUI_RESET, out);
    fputc('\n', out);
}

void tui_print_double_hline(FILE *out, int width) {
    if (out == 0) return;
    fputs(TUI_BOLD_CYAN, out);
    tui_repeat(out, TUI_DH, width);
    fputs(TUI_RESET, out);
    fputc('\n', out);
}

/* ═══════════════════════════════════════════════════════════════
 *  Messages & Feedback
 * ═══════════════════════════════════════════════════════════════ */

void tui_print_success(FILE *out, const char *message) {
    if (out == 0 || message == 0) return;
    fprintf(out, "\n  " TUI_BOLD_GREEN "%s %s" TUI_RESET "\n", TUI_CHECK, message);
}

void tui_print_error(FILE *out, const char *message) {
    if (out == 0 || message == 0) return;
    fprintf(out, "\n  " TUI_BOLD_RED "%s %s" TUI_RESET "\n", TUI_CROSS, message);
}

void tui_print_warning(FILE *out, const char *message) {
    if (out == 0 || message == 0) return;
    fprintf(out, "\n  " TUI_BOLD_YELLOW "%s %s" TUI_RESET "\n", TUI_TRIANGLE, message);
}

void tui_print_info(FILE *out, const char *message) {
    if (out == 0 || message == 0) return;
    fprintf(out, "  " TUI_CYAN "%s %s" TUI_RESET "\n", TUI_DOT, message);
}

void tui_print_prompt(FILE *out, const char *text) {
    if (out == 0 || text == 0) return;
    fprintf(out, "\n  " TUI_BOLD_YELLOW "%s %s" TUI_RESET, TUI_ARROW, text);
    fflush(out);
}

void tui_print_badge(FILE *out, const char *text, TuiBadgeColor color) {
    const char *fg = TUI_WHITE;
    const char *bg = TUI_BG_BLUE;

    if (out == 0 || text == 0) return;

    switch (color) {
        case TUI_BADGE_GREEN:   bg = TUI_BG_GREEN;   break;
        case TUI_BADGE_RED:     bg = TUI_BG_RED;     break;
        case TUI_BADGE_YELLOW:  bg = TUI_BG_YELLOW; fg = "\033[30m"; break;
        case TUI_BADGE_BLUE:    bg = TUI_BG_BLUE;    break;
        case TUI_BADGE_MAGENTA: bg = TUI_BG_MAGENTA;  break;
        case TUI_BADGE_CYAN:    bg = TUI_BG_CYAN;    break;
    }

    fputs(TUI_BOLD, out);
    fputs(bg, out);
    fputs(fg, out);
    fprintf(out, " %s ", text);
    fputs(TUI_RESET, out);
}

void tui_print_kv(FILE *out, const char *key, const char *value) {
    if (out == 0 || key == 0) return;
    fprintf(out, "  " TUI_DIM "%s" TUI_RESET " %s" TUI_BOLD_WHITE "%s" TUI_RESET "\n",
        TUI_DOT, "", key);
    if (value != 0) {
        fprintf(out, "    %s\n", value);
    }
}

void tui_print_kv_colored(FILE *out, const char *key, const char *value, const char *value_color) {
    if (out == 0 || key == 0) return;
    fprintf(out, "    " TUI_DIM "%s: " TUI_RESET, key);
    if (value != 0 && value_color != 0) {
        fputs(value_color, out);
        fputs(value, out);
        fputs(TUI_RESET, out);
    } else if (value != 0) {
        fputs(value, out);
    }
    fputc('\n', out);
}

void tui_print_progress(FILE *out, const char *label, int current, int total, int bar_width) {
    int filled = 0;
    int i = 0;

    if (out == 0 || total <= 0) return;
    if (bar_width < 10) bar_width = 10;

    filled = (current * bar_width) / total;
    if (filled > bar_width) filled = bar_width;

    if (label != 0) {
        fprintf(out, "  %s ", label);
    }
    fputs(TUI_DIM TUI_V TUI_RESET, out);
    for (i = 0; i < filled; i++) {
        if (i < bar_width / 3)
            fputs(TUI_BOLD_RED TUI_BLOCK_FULL, out);
        else if (i < (bar_width * 2) / 3)
            fputs(TUI_BOLD_YELLOW TUI_BLOCK_FULL, out);
        else
            fputs(TUI_BOLD_GREEN TUI_BLOCK_FULL, out);
    }
    fputs(TUI_RESET, out);
    for (i = filled; i < bar_width; i++) {
        fputs(TUI_DIM TUI_BLOCK_LIGHT TUI_RESET, out);
    }
    fputs(TUI_DIM TUI_V TUI_RESET, out);
    fprintf(out, " %d/%d", current, total);
    fputc('\n', out);
}

/* ═══════════════════════════════════════════════════════════════
 *  Table System
 * ═══════════════════════════════════════════════════════════════ */

void TuiTable_init(TuiTable *table, int column_count) {
    if (table == 0) return;
    memset(table, 0, sizeof(*table));
    if (column_count > TUI_TABLE_MAX_COLUMNS) column_count = TUI_TABLE_MAX_COLUMNS;
    table->column_count = column_count;
    table->border_color = TUI_BOLD_CYAN;
    table->header_color = TUI_BOLD_WHITE;
}

void TuiTable_set_header(TuiTable *table, int col, const char *header, int width) {
    if (table == 0 || col < 0 || col >= table->column_count || header == 0) return;
    strncpy(table->headers[col], header, sizeof(table->headers[col]) - 1);
    table->headers[col][sizeof(table->headers[col]) - 1] = '\0';
    table->widths[col] = width;
}

void TuiTable_set_colors(TuiTable *table, const char *border_color, const char *header_color) {
    if (table == 0) return;
    if (border_color != 0) table->border_color = border_color;
    if (header_color != 0) table->header_color = header_color;
}

void TuiTable_print_top(const TuiTable *table, FILE *out) {
    int i = 0;
    if (table == 0 || out == 0) return;

    fputs("  ", out);
    fputs(table->border_color, out);
    fputs(TUI_TL, out);
    for (i = 0; i < table->column_count; i++) {
        tui_repeat(out, TUI_H, table->widths[i] + 2);
        fputs(i < table->column_count - 1 ? TUI_TT : TUI_TR, out);
    }
    fputs(TUI_RESET, out);
    fputc('\n', out);
}

void TuiTable_print_header_row(const TuiTable *table, FILE *out) {
    int i = 0;
    if (table == 0 || out == 0) return;

    fputs("  ", out);
    fputs(table->border_color, out);
    fputs(TUI_V, out);
    for (i = 0; i < table->column_count; i++) {
        fputc(' ', out);
        fputs(table->header_color, out);
        tui_pad_right(out, table->headers[i], table->widths[i]);
        fputs(table->border_color, out);
        fputc(' ', out);
        fputs(TUI_V, out);
    }
    fputs(TUI_RESET, out);
    fputc('\n', out);
}

void TuiTable_print_separator(const TuiTable *table, FILE *out) {
    int i = 0;
    if (table == 0 || out == 0) return;

    fputs("  ", out);
    fputs(table->border_color, out);
    fputs(TUI_LT, out);
    for (i = 0; i < table->column_count; i++) {
        tui_repeat(out, TUI_H, table->widths[i] + 2);
        fputs(i < table->column_count - 1 ? TUI_X : TUI_RT, out);
    }
    fputs(TUI_RESET, out);
    fputc('\n', out);
}

void TuiTable_print_row(const TuiTable *table, FILE *out, const char *values[], int count) {
    int i = 0;
    if (table == 0 || out == 0 || values == 0) return;

    fputs("  ", out);
    fputs(table->border_color, out);
    fputs(TUI_V, out);
    fputs(TUI_RESET, out);
    for (i = 0; i < table->column_count; i++) {
        fputc(' ', out);
        if (i < count && values[i] != 0)
            tui_pad_right(out, values[i], table->widths[i]);
        else
            tui_pad_right(out, "", table->widths[i]);
        fputc(' ', out);
        fputs(table->border_color, out);
        fputs(TUI_V, out);
        fputs(TUI_RESET, out);
    }
    fputc('\n', out);
}

void TuiTable_print_row_colored(const TuiTable *table, FILE *out, const char *values[], const char *colors[], int count) {
    int i = 0;
    if (table == 0 || out == 0 || values == 0) return;

    fputs("  ", out);
    fputs(table->border_color, out);
    fputs(TUI_V, out);
    fputs(TUI_RESET, out);
    for (i = 0; i < table->column_count; i++) {
        fputc(' ', out);
        if (colors != 0 && i < count && colors[i] != 0)
            fputs(colors[i], out);
        if (i < count && values[i] != 0)
            tui_pad_right(out, values[i], table->widths[i]);
        else
            tui_pad_right(out, "", table->widths[i]);
        fputs(TUI_RESET, out);
        fputc(' ', out);
        fputs(table->border_color, out);
        fputs(TUI_V, out);
        fputs(TUI_RESET, out);
    }
    fputc('\n', out);
}

void TuiTable_print_bottom(const TuiTable *table, FILE *out) {
    int i = 0;
    if (table == 0 || out == 0) return;

    fputs("  ", out);
    fputs(table->border_color, out);
    fputs(TUI_BL, out);
    for (i = 0; i < table->column_count; i++) {
        tui_repeat(out, TUI_H, table->widths[i] + 2);
        fputs(i < table->column_count - 1 ? TUI_BT : TUI_BR, out);
    }
    fputs(TUI_RESET, out);
    fputc('\n', out);
}

void TuiTable_print_empty(const TuiTable *table, FILE *out, const char *message) {
    int total_w = 0;
    int msg_w = 0;
    int pad_l = 0;
    int pad_r = 0;
    int i = 0;

    if (table == 0 || out == 0) return;

    for (i = 0; i < table->column_count; i++)
        total_w += table->widths[i] + 3;
    total_w -= 1;

    msg_w = tui_display_width(message != 0 ? message : "(empty)");
    pad_l = (total_w - msg_w) / 2;
    pad_r = total_w - msg_w - pad_l;

    fputs("  ", out);
    fputs(table->border_color, out);
    fputs(TUI_V, out);
    fputs(TUI_RESET, out);
    for (i = 0; i < pad_l; i++) fputc(' ', out);
    fputs(TUI_DIM, out);
    fputs(message != 0 ? message : "(empty)", out);
    fputs(TUI_RESET, out);
    for (i = 0; i < pad_r; i++) fputc(' ', out);
    fputs(table->border_color, out);
    fputs(TUI_V, out);
    fputs(TUI_RESET, out);
    fputc('\n', out);
}

/* ═══════════════════════════════════════════════════════════════
 *  Menu Styling
 * ═══════════════════════════════════════════════════════════════ */

void tui_print_menu_title(FILE *out, const char *title, int width) {
    tui_print_header(out, title, width);
}

void tui_print_menu_item(FILE *out, int number, const char *label, int width) {
    int content_w = 0;
    int padding = 0;
    int i = 0;

    if (out == 0 || label == 0) return;

    fputs(TUI_BOLD_CYAN TUI_V TUI_RESET, out);
    fputs("  ", out);
    if (number == 0) {
        fprintf(out, TUI_DIM "%d. %s" TUI_RESET, number, label);
    } else {
        fprintf(out, TUI_BOLD_YELLOW "%d" TUI_RESET ". %s", number, label);
    }

    content_w = 5 + tui_display_width(label);
    if (number >= 10) content_w += 1;
    padding = width - 2 - content_w;
    for (i = 0; i < padding; i++) fputc(' ', out);

    fputs(TUI_BOLD_CYAN TUI_V TUI_RESET, out);
    fputc('\n', out);
}

void tui_print_menu_item_icon(FILE *out, int number, const char *icon, const char *label, int width) {
    int content_w = 0;
    int padding = 0;
    int i = 0;

    if (out == 0 || label == 0) return;

    fputs(TUI_BOLD_CYAN TUI_V TUI_RESET, out);
    fputs("  ", out);
    if (number == 0) {
        fprintf(out, TUI_DIM "%d. %s %s" TUI_RESET, number, icon != 0 ? icon : "", label);
    } else {
        fprintf(out, TUI_BOLD_YELLOW "%d" TUI_RESET ". ", number);
        if (icon != 0) {
            fputs(TUI_CYAN, out);
            fputs(icon, out);
            fputs(TUI_RESET, out);
            fputc(' ', out);
        }
        fputs(label, out);
    }

    content_w = 5 + tui_display_width(label);
    if (icon != 0) content_w += tui_display_width(icon) + 1;
    if (number >= 10) content_w += 1;
    padding = width - 2 - content_w;
    for (i = 0; i < padding; i++) fputc(' ', out);

    fputs(TUI_BOLD_CYAN TUI_V TUI_RESET, out);
    fputc('\n', out);
}

void tui_print_menu_bottom(FILE *out, int width) {
    if (out == 0) return;
    fputs(TUI_BOLD_CYAN, out);
    fputs(TUI_BL, out);
    tui_repeat(out, TUI_H, width - 2);
    fputs(TUI_BR, out);
    fputs(TUI_RESET, out);
    fputc('\n', out);
}

/* ═══════════════════════════════════════════════════════════════
 *  Animation System
 * ═══════════════════════════════════════════════════════════════ */

#ifdef _WIN32
#include <windows.h>
static void tui_sleep_ms(int ms) { Sleep(ms); }
static int tui_check_tty(void) { return _isatty(_fileno(stdout)); }
#else
#include <unistd.h>
static void tui_sleep_ms(int ms) { usleep(ms * 1000); }
static int tui_check_tty(void) { return isatty(STDOUT_FILENO); }
#endif

static int g_animation_interactive = -1; /* -1 = not checked */

int tui_is_interactive(void) {
    if (g_animation_interactive < 0)
        g_animation_interactive = tui_check_tty();
    return g_animation_interactive;
}

void tui_delay(int ms) {
    if (!tui_is_interactive()) return;
    if (ms > 0) tui_sleep_ms(ms);
}

/* ── Animated Logo ────────────────────────────────────────────── */

void tui_animate_logo(FILE *out) {
    int i = 0;
    if (out == 0) return;

    if (!tui_is_interactive()) {
        tui_print_logo(out);
        return;
    }

    tui_hide_cursor(out);
    fputc('\n', out);
    for (i = 0; i < LOGO_LINE_COUNT; i++) {
        /* Sweep: print spaces then reveal */
        int j = 0;
        fputs("  ", out);
        fputs(TUI_DIM, out);
        /* ghost pass */
        for (j = 0; j < 24; j++) fputs(TUI_BLOCK_LIGHT, out);
        fputs(TUI_RESET, out);
        fflush(out);
        tui_sleep_ms(30);

        /* overwrite with real line */
        fputs("\r", out);
        fputs(GRADIENT_COLORS[i % GRADIENT_COUNT], out);
        fputs(LOGO_LINES[i], out);
        fputs(TUI_RESET, out);
        fputc('\n', out);
        fflush(out);
        tui_sleep_ms(60);
    }
    tui_show_cursor(out);
}

/* ── Animated Banner ──────────────────────────────────────────── */

void tui_animate_banner(FILE *out, const char *title) {
    int title_w = 0;
    int box_w = 0;
    int pad_left = 0;
    int pad_right = 0;
    int i = 0;

    if (out == 0 || title == 0) return;

    if (!tui_is_interactive()) {
        tui_print_banner(out, title);
        return;
    }

    title_w = tui_display_width(title);
    box_w = title_w + 6;
    if (box_w < 42) box_w = 42;
    pad_left = (box_w - 2 - title_w) / 2;
    pad_right = box_w - 2 - title_w - pad_left;

    tui_hide_cursor(out);

    /* Top border - draw left to right */
    fputs("  ", out);
    fputs(TUI_BOLD_CYAN, out);
    fputs(TUI_DTL, out);
    fflush(out);
    for (i = 0; i < box_w - 2; i++) {
        fputs(TUI_DH, out);
        fflush(out);
        tui_sleep_ms(8);
    }
    fputs(TUI_DTR, out);
    fputs(TUI_RESET, out);
    fputc('\n', out);
    fflush(out);

    /* Rainbow sweep line - animate left to right */
    fputs("  ", out);
    fputs(TUI_BOLD_CYAN TUI_DV TUI_RESET, out);
    fputs(" ", out);
    fflush(out);
    for (i = 0; i < box_w - 4; i++) {
        fputs(GRADIENT_COLORS[i % GRADIENT_COUNT], out);
        fputs(TUI_BLOCK_UPPER, out);
        fputs(TUI_RESET, out);
        fflush(out);
        tui_sleep_ms(12);
    }
    fputs(" ", out);
    fputs(TUI_BOLD_CYAN TUI_DV TUI_RESET, out);
    fputc('\n', out);
    fflush(out);

    /* Title line - typewriter */
    fputs("  ", out);
    fputs(TUI_BOLD_CYAN TUI_DV TUI_RESET, out);
    for (i = 0; i < pad_left; i++) fputc(' ', out);
    fflush(out);
    {
        const unsigned char *p = (const unsigned char *)title;
        fputs(TUI_BOLD_WHITE, out);
        while (*p != 0) {
            if (*p < 0x80) {
                fputc(*p, out);
                p += 1;
            } else if (*p < 0xE0) {
                fputc(p[0], out); fputc(p[1], out);
                p += 2;
            } else if (*p < 0xF0) {
                fputc(p[0], out); fputc(p[1], out); fputc(p[2], out);
                p += 3;
            } else {
                fputc(p[0], out); fputc(p[1], out); fputc(p[2], out); fputc(p[3], out);
                p += 4;
            }
            fflush(out);
            tui_sleep_ms(40);
        }
        fputs(TUI_RESET, out);
    }
    for (i = 0; i < pad_right; i++) fputc(' ', out);
    fputs(TUI_BOLD_CYAN TUI_DV TUI_RESET, out);
    fputc('\n', out);
    fflush(out);

    /* Subtitle */
    {
        const char *sub = TUI_HEAVY_CROSS " Hospital Information System v2.0";
        int sub_w = tui_display_width(sub);
        int spad_l = (box_w - 2 - sub_w) / 2;
        int spad_r = box_w - 2 - sub_w - spad_l;
        fputs("  ", out);
        fputs(TUI_BOLD_CYAN TUI_DV TUI_RESET, out);
        for (i = 0; i < spad_l; i++) fputc(' ', out);
        fputs(TUI_DIM, out);
        fputs(sub, out);
        fputs(TUI_RESET, out);
        for (i = 0; i < spad_r; i++) fputc(' ', out);
        fputs(TUI_BOLD_CYAN TUI_DV TUI_RESET, out);
        fputc('\n', out);
        fflush(out);
        tui_sleep_ms(50);
    }

    /* Bottom rainbow sweep - reverse direction */
    fputs("  ", out);
    fputs(TUI_BOLD_CYAN TUI_DV TUI_RESET, out);
    fputs(" ", out);
    fflush(out);
    for (i = box_w - 5; i >= 0; i--) {
        fputs(GRADIENT_COLORS[i % GRADIENT_COUNT], out);
        fputs(TUI_BLOCK_LOWER, out);
        fputs(TUI_RESET, out);
        fflush(out);
        tui_sleep_ms(10);
    }
    fputs(" ", out);
    fputs(TUI_BOLD_CYAN TUI_DV TUI_RESET, out);
    fputc('\n', out);
    fflush(out);

    /* Bottom border */
    fputs("  ", out);
    fputs(TUI_BOLD_CYAN, out);
    fputs(TUI_DBL, out);
    for (i = 0; i < box_w - 2; i++) {
        fputs(TUI_DH, out);
        fflush(out);
        tui_sleep_ms(6);
    }
    fputs(TUI_DBR, out);
    fputs(TUI_RESET, out);
    fputc('\n', out);
    fputc('\n', out);
    fflush(out);

    tui_show_cursor(out);
}

/* ── Typewriter ───────────────────────────────────────────────── */

void tui_animate_typewriter(FILE *out, const char *text, int char_delay_ms) {
    const unsigned char *p = 0;

    if (out == 0 || text == 0) return;
    if (!tui_is_interactive()) { fputs(text, out); return; }
    if (char_delay_ms <= 0) char_delay_ms = 30;

    p = (const unsigned char *)text;
    while (*p != 0) {
        if (*p < 0x80) {
            fputc(*p, out); p += 1;
        } else if (*p < 0xE0) {
            fputc(p[0], out); fputc(p[1], out); p += 2;
        } else if (*p < 0xF0) {
            fputc(p[0], out); fputc(p[1], out); fputc(p[2], out); p += 3;
        } else {
            fputc(p[0], out); fputc(p[1], out); fputc(p[2], out); fputc(p[3], out); p += 4;
        }
        fflush(out);
        tui_sleep_ms(char_delay_ms);
    }
}

/* ── Animated Welcome ─────────────────────────────────────────── */

void tui_animate_welcome(FILE *out, TuiRoleTheme theme, const char *user_id) {
    const char *color = tui_role_color(theme);
    const char *icon = tui_role_icon(theme);
    const char *label = tui_role_label(theme);
    int i = 0;

    if (out == 0) return;
    if (!tui_is_interactive()) { tui_print_welcome(out, theme, user_id); return; }

    tui_hide_cursor(out);
    fputc('\n', out);

    /* Draw top border animated */
    fputs("  ", out);
    fputs(color, out);
    fputs(TUI_HTL, out);
    fflush(out);
    for (i = 0; i < 40; i++) {
        fputs(TUI_HH, out);
        fflush(out);
        tui_sleep_ms(10);
    }
    fputs(TUI_HTR, out);
    fputs(TUI_RESET, out);
    fputc('\n', out);
    fflush(out);

    /* Welcome text - typewriter */
    fputs("  ", out);
    fputs(color, out);
    fputs(TUI_HV, out);
    fputs(TUI_RESET, out);
    fputs("  ", out);
    fputs(TUI_SPARKLE, out);
    fputs(" ", out);
    fputs(TUI_BOLD_WHITE, out);
    tui_animate_typewriter(out, "\xe6\xac\xa2\xe8\xbf\x8e\xe7\x99\xbb\xe5\xbd\x95" /* 欢迎登录 */, 80);
    fputs(TUI_RESET, out);
    /* pad to fill */
    for (i = 0; i < 23; i++) fputc(' ', out);
    fputs(color, out);
    fputs(TUI_HV, out);
    fputs(TUI_RESET, out);
    fputc('\n', out);
    fflush(out);
    tui_sleep_ms(100);

    /* Role info line */
    fputs("  ", out);
    fputs(color, out);
    fputs(TUI_HV, out);
    fputs(TUI_RESET, out);
    fprintf(out, "  %s %s%s%s", icon, color, label, TUI_RESET);
    fprintf(out, TUI_DIM " [%s]" TUI_RESET, user_id != 0 ? user_id : "");
    {
        char tmp[128];
        int tw = 0;
        snprintf(tmp, sizeof(tmp), "  %s %s [%s]", icon, label, user_id != 0 ? user_id : "");
        tw = tui_display_width(tmp);
        for (i = 0; i < 40 - tw; i++) fputc(' ', out);
    }
    fputs(color, out);
    fputs(TUI_HV, out);
    fputs(TUI_RESET, out);
    fputc('\n', out);
    fflush(out);
    tui_sleep_ms(100);

    /* Draw bottom border animated */
    fputs("  ", out);
    fputs(color, out);
    fputs(TUI_HBL, out);
    fflush(out);
    for (i = 0; i < 40; i++) {
        fputs(TUI_HH, out);
        fflush(out);
        tui_sleep_ms(8);
    }
    fputs(TUI_HBR, out);
    fputs(TUI_RESET, out);
    fputc('\n', out);
    fputc('\n', out);
    fflush(out);

    tui_show_cursor(out);
}

/* ── Animated Goodbye ─────────────────────────────────────────── */

void tui_animate_goodbye(FILE *out) {
    int frame = 0;
    int i = 0;
    /* UTF-8 bytes for 感谢使用HIS系统再见 */
    const char *words[] = {
        "\xe6\x84\x9f", "\xe8\xb0\xa2", "\xe4\xbd\xbf", "\xe7\x94\xa8",
        " H", "I", "S ", "\xe7\xb3\xbb", "\xe7\xbb\x9f",
        " - ", "\xe5\x86\x8d", "\xe8\xa7\x81", " ", TUI_SPARKLE
    };
    int word_count = 14;

    if (out == 0) return;
    if (!tui_is_interactive()) { tui_print_goodbye(out); return; }

    tui_hide_cursor(out);
    fputc('\n', out);

    /* Animated divider sweep */
    fputs("  ", out);
    for (i = 0; i < 42; i++) {
        fputs(GRADIENT_COLORS[i % GRADIENT_COUNT], out);
        fputs(TUI_H, out);
        fputs(TUI_RESET, out);
        fflush(out);
        tui_sleep_ms(15);
    }
    fputc('\n', out);
    fflush(out);
    tui_sleep_ms(200);

    /* Words appear one by one with color cycling */
    fputs("  ", out);
    for (frame = 0; frame < word_count; frame++) {
        fputs(GRADIENT_COLORS[frame % GRADIENT_COUNT], out);
        fputs(TUI_BOLD, out);
        fputs(words[frame], out);
        fputs(TUI_RESET, out);
        fflush(out);
        tui_sleep_ms(120);
    }
    fputc('\n', out);
    fflush(out);
    tui_sleep_ms(200);

    /* Bottom divider - reverse sweep */
    fputs("  ", out);
    for (i = 41; i >= 0; i--) {
        fputs(GRADIENT_COLORS[i % GRADIENT_COUNT], out);
        fputs(TUI_H, out);
        fputs(TUI_RESET, out);
        fflush(out);
        tui_sleep_ms(12);
    }
    fputc('\n', out);
    fputc('\n', out);
    fflush(out);

    tui_show_cursor(out);
}

/* ── Screen Transition ────────────────────────────────────────── */

void tui_animate_transition(FILE *out) {
    int row = 0;
    int i = 0;

    if (out == 0 || !tui_is_interactive()) return;

    tui_hide_cursor(out);
    for (row = 0; row < 8; row++) {
        fputs("  ", out);
        for (i = 0; i < 44; i++) {
            fputs(GRADIENT_COLORS[(i + row) % GRADIENT_COUNT], out);
            fputs(TUI_BLOCK_MED, out);
        }
        fputs(TUI_RESET, out);
        fputc('\n', out);
        fflush(out);
        tui_sleep_ms(25);
    }
    tui_sleep_ms(80);
    /* Clear the transition area */
    for (row = 0; row < 8; row++) {
        fprintf(out, "\033[A");  /* move up */
    }
    for (row = 0; row < 8; row++) {
        fputs("\033[2K\n", out); /* clear line */
    }
    for (row = 0; row < 8; row++) {
        fprintf(out, "\033[A");  /* move up */
    }
    fflush(out);
    tui_show_cursor(out);
}

/* ── Animate Lines ────────────────────────────────────────────── */

void tui_animate_lines(FILE *out, const char *text, int line_delay_ms) {
    const char *start = 0;
    const char *end = 0;

    if (out == 0 || text == 0) return;
    if (!tui_is_interactive()) { fputs(text, out); return; }
    if (line_delay_ms <= 0) line_delay_ms = 40;

    start = text;
    while (*start != '\0') {
        end = start;
        while (*end != '\0' && *end != '\n') end++;

        /* Print this line */
        fwrite(start, 1, (size_t)(end - start), out);
        fputc('\n', out);
        fflush(out);
        tui_sleep_ms(line_delay_ms);

        if (*end == '\n') end++;
        start = end;
    }
}

/* ── Spinner ──────────────────────────────────────────────────── */

static const char *SPINNER_FRAMES[] = {
    "\xe2\xa0\x8b", /* ⠋ */
    "\xe2\xa0\x99", /* ⠙ */
    "\xe2\xa0\xb9", /* ⠹ */
    "\xe2\xa0\xb8", /* ⠸ */
    "\xe2\xa0\xbc", /* ⠼ */
    "\xe2\xa0\xb4", /* ⠴ */
    "\xe2\xa0\xa6", /* ⠦ */
    "\xe2\xa0\xa7", /* ⠧ */
    "\xe2\xa0\x87", /* ⠇ */
    "\xe2\xa0\x8f"  /* ⠏ */
};
#define SPINNER_FRAME_COUNT 10

void tui_spinner_run(FILE *out, const char *message, int duration_ms) {
    int elapsed = 0;
    int frame = 0;
    int step = 80;

    if (out == 0) return;
    if (!tui_is_interactive()) {
        if (message != 0) fprintf(out, "  %s\n", message);
        return;
    }
    if (duration_ms <= 0) duration_ms = 500;

    tui_hide_cursor(out);
    while (elapsed < duration_ms) {
        fprintf(out, "\r  %s%s%s %s%s%s",
            GRADIENT_COLORS[frame % GRADIENT_COUNT],
            SPINNER_FRAMES[frame % SPINNER_FRAME_COUNT],
            TUI_RESET,
            TUI_DIM,
            message != 0 ? message : "",
            TUI_RESET);
        fflush(out);
        tui_sleep_ms(step);
        elapsed += step;
        frame++;
    }
    /* Clear spinner line */
    fputs("\r\033[2K", out);
    fflush(out);
    tui_show_cursor(out);
}

/* ── Animated Progress ────────────────────────────────────────── */

void tui_animate_progress(FILE *out, const char *label, int steps, int bar_width) {
    int i = 0;
    int j = 0;

    if (out == 0) return;
    if (!tui_is_interactive()) {
        tui_print_progress(out, label, steps, steps, bar_width);
        return;
    }
    if (steps <= 0) steps = 20;
    if (bar_width < 10) bar_width = 20;

    tui_hide_cursor(out);
    for (i = 0; i <= steps; i++) {
        int filled = (i * bar_width) / steps;
        fputs("\r  ", out);
        if (label != 0) fprintf(out, "%s ", label);
        fputs(TUI_DIM TUI_V TUI_RESET, out);
        for (j = 0; j < filled; j++) {
            if (j < bar_width / 3)
                fputs(TUI_BOLD_RED TUI_BLOCK_FULL, out);
            else if (j < (bar_width * 2) / 3)
                fputs(TUI_BOLD_YELLOW TUI_BLOCK_FULL, out);
            else
                fputs(TUI_BOLD_GREEN TUI_BLOCK_FULL, out);
        }
        fputs(TUI_RESET, out);
        for (j = filled; j < bar_width; j++) {
            fputs(TUI_DIM TUI_BLOCK_LIGHT TUI_RESET, out);
        }
        fputs(TUI_DIM TUI_V TUI_RESET, out);
        fprintf(out, " %d%%", (i * 100) / steps);
        fputs("   ", out);
        fflush(out);
        tui_sleep_ms(30);
    }
    fputc('\n', out);
    fflush(out);
    tui_show_cursor(out);
}

/* ── Animated Success / Error ─────────────────────────────────── */

void tui_animate_success(FILE *out, const char *message) {
    if (out == 0 || message == 0) return;
    if (!tui_is_interactive()) { tui_print_success(out, message); return; }

    /* Flash: reverse green → normal green */
    fprintf(out, "\n  " TUI_REVERSE TUI_BOLD_GREEN " %s %s " TUI_RESET, TUI_CHECK, message);
    fflush(out);
    tui_sleep_ms(150);
    fprintf(out, "\r\033[2K");
    fprintf(out, "\n  " TUI_BOLD_GREEN "%s %s" TUI_RESET "\n", TUI_CHECK, message);
    fflush(out);
}

void tui_animate_error(FILE *out, const char *message) {
    if (out == 0 || message == 0) return;
    if (!tui_is_interactive()) { tui_print_error(out, message); return; }

    /* Flash: reverse red → normal red */
    fprintf(out, "\n  " TUI_REVERSE TUI_BOLD_RED " %s %s " TUI_RESET, TUI_CROSS, message);
    fflush(out);
    tui_sleep_ms(150);
    fprintf(out, "\r\033[2K");
    fprintf(out, "\n  " TUI_BOLD_RED "%s %s" TUI_RESET "\n", TUI_CROSS, message);
    fflush(out);
}

/* ── Rainbow Text Animation ───────────────────────────────────── */

void tui_animate_rainbow(FILE *out, const char *text, int cycles, int frame_delay_ms) {
    int cycle = 0;
    int offset = 0;
    int len = 0;
    const unsigned char *p = 0;

    if (out == 0 || text == 0) return;
    if (!tui_is_interactive()) { tui_print_gradient_text(out, text); return; }
    if (cycles <= 0) cycles = 3;
    if (frame_delay_ms <= 0) frame_delay_ms = 100;

    len = tui_display_width(text);
    tui_hide_cursor(out);

    for (cycle = 0; cycle < cycles * GRADIENT_COUNT; cycle++) {
        offset = cycle;
        p = (const unsigned char *)text;
        fputs("\r  ", out);
        while (*p != 0) {
            fputs(GRADIENT_COLORS[offset % GRADIENT_COUNT], out);
            fputs(TUI_BOLD, out);
            if (*p < 0x80) {
                fputc(*p, out); p += 1;
                offset++;
            } else if (*p < 0xE0) {
                fputc(p[0], out); fputc(p[1], out); p += 2;
                offset++;
            } else if (*p < 0xF0) {
                fputc(p[0], out); fputc(p[1], out); fputc(p[2], out); p += 3;
                offset++;
            } else {
                fputc(p[0], out); fputc(p[1], out); fputc(p[2], out); fputc(p[3], out); p += 4;
                offset++;
            }
        }
        fputs(TUI_RESET, out);
        /* pad to clear any leftover */
        { int tw = tui_display_width(text); int pad = len - tw + 2; int pi; for (pi = 0; pi < pad; pi++) fputc(' ', out); }
        fflush(out);
        tui_sleep_ms(frame_delay_ms);
    }
    fputc('\n', out);
    fflush(out);
    tui_show_cursor(out);
}
