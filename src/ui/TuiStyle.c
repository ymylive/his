/**
 * @file TuiStyle.c
 * @brief TUI 样式与动画系统实现
 *
 * 本文件实现了 HIS 系统完整的终端用户界面视觉系统，包含：
 * - 角色主题配色（管理员=红, 医生=绿, 患者=蓝, 住院=品红, 药房=黄）
 * - 屏幕操作（清屏、光标控制）
 * - CJK 字符显示宽度计算（3字节以上 UTF-8 按双宽处理）
 * - Logo/Banner/欢迎框/再见框等 UI 组件
 * - 状态栏、节标题、消息反馈等页面元素
 * - 完整的表格绘制系统（支持彩色行）
 * - 菜单框架组件
 * - 丰富的动画特效系统（需要交互式终端支持）
 */

#include "ui/TuiStyle.h"
#include "common/UpdateChecker.h"  /* HIS_VERSION 版本号 */

#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846  /**< 圆周率常量 */
#endif

/* ═══════════════════════════════════════════════════════════════
 *  角色主题系统 - 为各角色分配前景色、背景色、图标和标签
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
 *  屏幕操作 - 终端控制序列
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
 *  显示宽度计算 - 中日韩字符宽度感知
 * ═══════════════════════════════════════════════════════════════ */

/**
 * @brief 计算 UTF-8 字符串的终端显示宽度
 *
 * 根据 UTF-8 编码字节数判断字符宽度：
 * - 1 字节 (ASCII): 宽度 1
 * - 2 字节: 宽度 1（拉丁扩展等）
 * - 3 字节: 宽度 2（CJK 中文字符等）
 * - 4 字节: 宽度 2（表情符号等）
 */
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

/** @brief 重复输出指定字符串若干次 */
static void tui_repeat(FILE *out, const char *str, int count) {
    int i = 0;
    for (i = 0; i < count; i++) fputs(str, out);
}

/** @brief 输出一个 UTF-8 字符并返回消耗的字节数 */
static int tui_fputc_utf8(FILE *out, const unsigned char *p) {
    if (*p < 0x80) {
        fputc(*p, out);
        return 1;
    } else if (*p < 0xE0) {
        fputc(p[0], out); fputc(p[1], out);
        return 2;
    } else if (*p < 0xF0) {
        fputc(p[0], out); fputc(p[1], out); fputc(p[2], out);
        return 3;
    } else {
        fputc(p[0], out); fputc(p[1], out); fputc(p[2], out); fputc(p[3], out);
        return 4;
    }
}

/* ═══════════════════════════════════════════════════════════════
 *  Aurora Whisper - 极光微光装饰系统
 *
 *  设计目标："深色玻璃上偶尔有一缕冷光掠过" —— 高级感 + 灵动感
 *
 *  色相家族 220° (indigo) → 200° (cyan)，不跨色相、饱和度 ≤ 0.65
 *  渲染：以 phase 为扫描中心，Gaussian falloff 决定每列的亮度/字形
 *  动效：phase-based 静态渲染（time(NULL) 驱动，每次重绘画面不同）
 *        + 短 sweep 动画（登录/登出场景，≤ 250ms）
 * ═══════════════════════════════════════════════════════════════ */

/* HSL → RGB（s,l ∈ [0,1], h ∈ [0,360)） */
static void tui_hsl_to_rgb(double h, double s, double l, int *r, int *g, int *b) {
    double c = (1.0 - fabs(2.0 * l - 1.0)) * s;
    double hp = h / 60.0;
    double x = c * (1.0 - fabs(fmod(hp, 2.0) - 1.0));
    double r1 = 0, g1 = 0, b1 = 0;
    double m;
    if      (hp < 1) { r1 = c; g1 = x; }
    else if (hp < 2) { r1 = x; g1 = c; }
    else if (hp < 3) { g1 = c; b1 = x; }
    else if (hp < 4) { g1 = x; b1 = c; }
    else if (hp < 5) { r1 = x; b1 = c; }
    else             { r1 = c; b1 = x; }
    m = l - c / 2.0;
    { int rr = (int)((r1 + m) * 255.0 + 0.5);
      int gg = (int)((g1 + m) * 255.0 + 0.5);
      int bb = (int)((b1 + m) * 255.0 + 0.5);
      if (rr < 0) rr = 0; if (rr > 255) rr = 255;
      if (gg < 0) gg = 0; if (gg > 255) gg = 255;
      if (bb < 0) bb = 0; if (bb > 255) bb = 255;
      *r = rr; *g = gg; *b = bb; }
}

static double tui_smoothstep(double t) {
    if (t < 0.0) t = 0.0;
    if (t > 1.0) t = 1.0;
    return t * t * (3.0 - 2.0 * t);
}

int tui_mosaic_enabled(void) {
    static int cached = -1;
    const char *env;
    if (cached < 0) {
        cached = 1;
        env = getenv("HIS_NO_MOSAIC");
        if (env != 0 && env[0] != '\0' &&
            (env[0] == '1' || env[0] == 't' || env[0] == 'T' ||
             env[0] == 'y' || env[0] == 'Y')) {
            cached = 0;
        } else if ((env = getenv("NO_COLOR")) != 0 && env[0] != '\0') {
            cached = 0;
        } else if ((env = getenv("CI")) != 0 && env[0] != '\0') {
            cached = 0;
        } else if ((env = getenv("TERM")) != 0 &&
                   (env[0] == '\0' || strcmp(env, "dumb") == 0)) {
            cached = 0;
        }
    }
    if (!cached) return 0;
    if (!tui_is_interactive()) return 0;
    /* 窄终端运行时检测（不缓存，允许运行中 resize） */
    if (tui_get_terminal_width() < 60) return 0;
    return 1;
}

/* 为单列采样：在 phase 指示的扫描位置周围给出 Aurora 色和字形 */
static void tui_aurora_sample(int col, int sweep_center,
                              int *r, int *g, int *b, const char **glyph) {
    /* σ = 3.5 → σ² = 12.25；带宽 ±7 列 */
    double d = (double)(col - sweep_center);
    double falloff = exp(-d * d / (2.0 * 12.25));
    double fs = tui_smoothstep(falloff);
    double H = 220.0 - 20.0 * fs; /* indigo → cyan */
    double S = 0.45 + 0.20 * fs;  /* 上限 0.65 */
    double L = 0.18 + 0.52 * fs;  /* 底 0.18（几乎融入深底），峰 0.70 */
    tui_hsl_to_rgb(H, S, L, r, g, b);
    if (falloff < 0.06)      *glyph = " ";
    else if (falloff < 0.22) *glyph = "\xe2\x96\x91"; /* ░ */
    else if (falloff < 0.48) *glyph = "\xe2\x96\x92"; /* ▒ */
    else if (falloff < 0.78) *glyph = "\xe2\x96\x93"; /* ▓ */
    else                     *glyph = "\xe2\x96\x88"; /* █ */
}

void tui_print_mosaic_strip(FILE *out, int width, int phase) {
    int i;
    int r_prev = -1, g_prev = -1, b_prev = -1;
    int sweep_range, cx;
    int r, g, b;
    const char *glyph;

    if (out == 0 || width <= 0) return;
    if (!tui_mosaic_enabled()) {
        fputs(TUI_OC_DIM, out);
        for (i = 0; i < width; i++) fputs("\xe2\x94\x80", out); /* ─ */
        fputs(TUI_RESET, out);
        return;
    }

    /* 扫描范围比宽度大 16，两端留出入/出场空间 */
    sweep_range = width + 16;
    cx = (int)((((unsigned)phase) % (unsigned)sweep_range)) - 8;

    for (i = 0; i < width; i++) {
        tui_aurora_sample(i, cx, &r, &g, &b, &glyph);
        if (r != r_prev || g != g_prev || b != b_prev) {
            fprintf(out, "\033[38;2;%d;%d;%dm", r, g, b);
            r_prev = r; g_prev = g; b_prev = b;
        }
        fputs(glyph, out);
    }
    fputs(TUI_RESET, out);
}

/* 兼容 v7.2.x API —— 在 v7.2.3 已无调用点，保留占位避免破坏 ABI */
void tui_print_mosaic_gutter(FILE *out) {
    (void)out; /* intentionally no-op: UX decision to not decorate message gutters */
}

/* tui_mosaic_splash 定义见下方"动画系统"段（依赖 tui_sleep_ms） */

/* ═══════════════════════════════════════════════════════════════
 *  Logo 与 Banner - 系统标识与标题展示
 * ═══════════════════════════════════════════════════════════════ */

/* 渐变色数组：红 -> 黄 -> 绿 -> 青 -> 蓝 -> 品红 */
static const char *GRADIENT_COLORS[] = {
    "\033[1;31m",  /* red */
    "\033[1;33m",  /* yellow */
    "\033[1;32m",  /* green */
    "\033[1;36m",  /* cyan */
    "\033[1;34m",  /* blue */
    "\033[1;35m"   /* magenta */
};
#define GRADIENT_COUNT 6

/* 256色平滑渐变色板（暖色日落调色板，全局共享） */
const int TUI_GRADIENT_256[] = {
    196, 202, 208, 214, 220, 226,  /* red → orange → yellow */
    190, 154, 118, 82, 46,         /* yellow-green → green */
    47, 48, 49, 50, 51,            /* green → cyan */
    45, 39, 33, 27, 21,            /* cyan → blue */
    57, 93, 129, 165, 201          /* blue → magenta */
};
/* 内部别名，保持已有代码兼容 */
#define GRADIENT_256 TUI_GRADIENT_256
#define GRADIENT_256_COUNT TUI_GRADIENT_256_COUNT

static const char *LOGO_LINES[] = {
    "\xe2\x96\x88\xe2\x96\x88\xe2\x95\x97  \xe2\x96\x88\xe2\x96\x88\xe2\x95\x97 \xe2\x96\x88\xe2\x96\x88\xe2\x95\x97 \xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x95\x97",
    "\xe2\x96\x88\xe2\x96\x88\xe2\x95\x91  \xe2\x96\x88\xe2\x96\x88\xe2\x95\x91 \xe2\x96\x88\xe2\x96\x88\xe2\x95\x91 \xe2\x96\x88\xe2\x96\x88\xe2\x95\x94\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x9d",
    "\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x95\x91 \xe2\x96\x88\xe2\x96\x88\xe2\x95\x91 \xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x95\x97",
    "\xe2\x96\x88\xe2\x96\x88\xe2\x95\x94\xe2\x95\x90\xe2\x95\x90\xe2\x96\x88\xe2\x96\x88\xe2\x95\x91 \xe2\x96\x88\xe2\x96\x88\xe2\x95\x91 \xe2\x95\x9a\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x96\x88\xe2\x96\x88\xe2\x95\x91",
    "\xe2\x96\x88\xe2\x96\x88\xe2\x95\x91  \xe2\x96\x88\xe2\x96\x88\xe2\x95\x91 \xe2\x96\x88\xe2\x96\x88\xe2\x95\x91 \xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x95\x91",
    "\xe2\x95\x9a\xe2\x95\x90\xe2\x95\x9d  \xe2\x95\x9a\xe2\x95\x90\xe2\x95\x9d \xe2\x95\x9a\xe2\x95\x90\xe2\x95\x9d \xe2\x95\x9a\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x9d"
};
#define LOGO_LINE_COUNT 6

void tui_print_logo(FILE *out) {
    int i = 0;
    int logo_width = 0;
    if (out == 0) return;

    logo_width = tui_display_width(LOGO_LINES[0]);
    fputc('\n', out);
    for (i = 0; i < LOGO_LINE_COUNT; i++) {
        tui_print_margin(out, logo_width);
        fputs(TUI_OC_ACCENT TUI_BOLD, out);
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
        int bytes;
        fputs(GRADIENT_COLORS[idx % GRADIENT_COUNT], out);
        bytes = tui_fputc_utf8(out, p);
        if (bytes >= 3) idx++; /* CJK: extra column */
        p += bytes;
        if (*p >= 0x80) idx++;
    }
    fputs(TUI_RESET, out);
}

void tui_print_banner(FILE *out, const char *title) {
    int i = 0;
    int use_aurora;
    int phase;

    if (out == 0 || title == 0) return;

    use_aurora = tui_mosaic_enabled();
    phase = (int)(time(0) & 0x7fffffff);

    /* 顶边：Aurora 扫描 or 原 dim 横线（同占 1 行） */
    tui_print_margin(out, 46);
    if (use_aurora) {
        tui_print_mosaic_strip(out, 46, phase);
    } else {
        fputs(TUI_OC_DIM, out);
        for (i = 0; i < 46; i++) fputs(TUI_H, out);
        fputs(TUI_RESET, out);
    }
    fputc('\n', out);

    /* Bold title */
    tui_print_margin(out, 46);
    fputs(TUI_OC_TEXT TUI_BOLD, out);
    fputs(title, out);
    fputs(TUI_RESET, out);
    fputc('\n', out);

    /* 底边：相位偏移 23，让上下的光带错位 */
    tui_print_margin(out, 46);
    if (use_aurora) {
        tui_print_mosaic_strip(out, 46, phase + 23);
    } else {
        fputs(TUI_OC_DIM, out);
        for (i = 0; i < 46; i++) fputs(TUI_H, out);
        fputs(TUI_RESET, out);
    }
    fputc('\n', out);
    fputc('\n', out);
}

void tui_print_welcome(FILE *out, TuiRoleTheme theme, const char *user_id) {
    (void)theme;
    if (out == 0) return;

    /* 替换原来的空行为 Aurora 横条（总行数不变） */
    if (tui_mosaic_enabled()) {
        tui_print_margin(out, 46);
        tui_print_mosaic_strip(out, 46, (int)(time(0) & 0x7fffffff));
        fputc('\n', out);
    } else {
        fputc('\n', out);
    }
    tui_print_margin(out, 46);
    fprintf(out, TUI_OC_SUCCESS "%s" TUI_RESET " " TUI_OC_TEXT "\xe6\xac\xa2\xe8\xbf\x8e\xe5\x9b\x9e\xe6\x9d\xa5" TUI_RESET /* 欢迎回来 */, TUI_CHECK);
    if (user_id != 0) {
        fprintf(out, TUI_OC_MUTED " (%s)" TUI_RESET, user_id);
    }
    fputc('\n', out);

    tui_print_margin(out, 46);
    fprintf(out, TUI_OC_MUTED "HIS v" HIS_VERSION " \xe2\x80\x94 \xe8\xbd\xbb\xe9\x87\x8f\xe7\xba\xa7\xe5\x8c\xbb\xe9\x99\xa2\xe4\xbf\xa1\xe6\x81\xaf\xe7\xb3\xbb\xe7\xbb\x9f" TUI_RESET /* 轻量级医院信息系统 */);
    fputc('\n', out);
    fputc('\n', out);
}

void tui_print_goodbye(FILE *out) {
    if (out == 0) return;

    /* 会话结束，之后无交互 —— 安全释放 200ms Aurora 扫描作为告别仪式 */
    if (tui_mosaic_enabled()) {
        tui_mosaic_splash(out, 200);
    } else {
        fputc('\n', out);
    }
    tui_print_margin(out, 46);
    fprintf(out, TUI_OC_SUCCESS "%s" TUI_RESET " " TUI_OC_TEXT "\xe5\x86\x8d\xe8\xa7\x81\xef\xbc\x81" TUI_RESET "\n" /* 再见！ */, TUI_CHECK);

    tui_print_margin(out, 46);
    fputs(TUI_OC_MUTED "\xe6\x84\x9f\xe8\xb0\xa2\xe4\xbd\xbf\xe7\x94\xa8 HIS \xe7\xb3\xbb\xe7\xbb\x9f" TUI_RESET "\n" /* 感谢使用 HIS 系统 */, out);
    fputc('\n', out);
}

/* ═══════════════════════════════════════════════════════════════
 *  状态栏与标题 - 页面顶部组件
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
    const char *label = tui_role_label(theme);
    int tw = tui_get_terminal_width();

    if (out == 0) return;

    /* Full-width bar with panel background */
    fputs(TUI_OC_BG_PANEL TUI_BOLD TUI_WHITE, out);
    fprintf(out, " %s ", label);
    fputs(TUI_RESET, out);

    /* Right-align: user_id + version */
    {
        char right[128];
        int right_w = 0;
        int label_w = tui_display_width(label) + 2; /* " label " */
        int pad = 0;
        int i = 0;

        snprintf(right, sizeof(right), "%s  HIS v" HIS_VERSION " ",
                 user_id != 0 ? user_id : "");
        right_w = tui_display_width(right);
        pad = tw - label_w - right_w;
        if (pad < 0) pad = 0;

        fputs(TUI_OC_BG_PANEL, out);
        for (i = 0; i < pad; i++) fputc(' ', out);
        fputs(TUI_OC_MUTED, out);
        fputs(right, out);
        fputs(TUI_RESET, out);
    }
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
    (void)icon;
    if (out == 0 || title == 0) return;

    fputc('\n', out);
    tui_print_margin(out, 46);
    fputs(TUI_OC_ACCENT, out);
    fputs(TUI_HV, out);
    fputs(TUI_RESET " ", out);
    fputs(TUI_BOLD_WHITE, out);
    fputs(title, out);
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

void tui_print_gradient_hline(FILE *out, int width) {
    int i = 0;
    if (out == 0) return;
    tui_print_margin(out, 46);
    if (tui_mosaic_enabled()) {
        int w = width > 0 ? width : 46;
        tui_print_mosaic_strip(out, w, (int)(time(0) & 0x7fffffff));
    } else {
        fputs(TUI_OC_BORDER, out);
        for (i = 0; i < width; i++) fputc('-', out);
        fputs(TUI_RESET, out);
    }
    fputc('\n', out);
}

/* ═══════════════════════════════════════════════════════════════
 *  消息与反馈 - 操作结果展示组件
 * ═══════════════════════════════════════════════════════════════ */

void tui_print_success(FILE *out, const char *message) {
    if (out == 0 || message == 0) return;
    fputc('\n', out); tui_print_margin(out, 46);
    fprintf(out, TUI_OC_SUCCESS " %s " TUI_OC_TEXT "%s" TUI_RESET "\n", TUI_CHECK, message);
}

void tui_print_error(FILE *out, const char *message) {
    if (out == 0 || message == 0) return;
    fputc('\n', out); tui_print_margin(out, 46);
    fprintf(out, TUI_OC_ERROR " %s " TUI_OC_TEXT "%s" TUI_RESET "\n", TUI_CROSS, message);
}

void tui_print_warning(FILE *out, const char *message) {
    if (out == 0 || message == 0) return;
    fputc('\n', out); tui_print_margin(out, 46);
    fprintf(out, TUI_OC_WARN " %s " TUI_OC_TEXT "%s" TUI_RESET "\n", TUI_TRIANGLE, message);
}

void tui_print_info(FILE *out, const char *message) {
    if (out == 0 || message == 0) return;
    tui_print_margin(out, 46);
    fprintf(out, TUI_OC_BLUE "%s" TUI_RESET " " TUI_OC_MUTED "%s" TUI_RESET "\n", TUI_DOT, message);
}

void tui_print_prompt(FILE *out, const char *text) {
    if (out == 0 || text == 0) return;
    fputc('\n', out); tui_print_margin(out, 46);
    fprintf(out, TUI_OC_AMBER "%s" TUI_RESET " " TUI_OC_TEXT "%s" TUI_RESET, TUI_ARROW, text);
    fflush(out);
}

void tui_print_prompt_themed(FILE *out, const char *text, TuiRoleTheme theme) {
    (void)theme;
    if (out == 0 || text == 0) return;
    fputc('\n', out); tui_print_margin(out, 46);
    fprintf(out, TUI_OC_ACCENT "%s" TUI_RESET " " TUI_OC_TEXT "%s" TUI_RESET, TUI_ARROW, text);
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
    int pct = 0;

    if (out == 0 || total <= 0) return;
    if (bar_width < 10) bar_width = 10;

    filled = (current * bar_width) / total;
    if (filled > bar_width) filled = bar_width;
    pct = (current * 100) / total;

    { int content_w = bar_width + 8; if (label != 0) content_w += tui_display_width(label) + 1; tui_print_margin(out, content_w); }
    if (label != 0) {
        fprintf(out, "%s ", label);
    }
    fprintf(out, "\033[38;5;240m" TUI_V "\033[0m");
    for (i = 0; i < filled; i++) {
        /* Smooth 256-color gradient: red(196) → yellow(226) → green(46) */
        int color = 0;
        if (i < bar_width / 3)
            color = 196 + (i * 6 / (bar_width / 3));  /* red shades */
        else if (i < (bar_width * 2) / 3)
            color = 208 + (((i - bar_width / 3) * 18) / (bar_width / 3));  /* orange → yellow */
        else
            color = 46;  /* green */
        fprintf(out, "\033[38;5;%dm" TUI_BLOCK_FULL, color);
    }
    fputs(TUI_RESET, out);
    for (i = filled; i < bar_width; i++) {
        fprintf(out, "\033[38;5;238m" TUI_BLOCK_LIGHT);
    }
    fprintf(out, "\033[0m\033[38;5;240m" TUI_V "\033[0m");
    fprintf(out, " \033[38;5;%dm%d/%d\033[0m", pct >= 100 ? 46 : 252, current, total);
    fputc('\n', out);
}

/* ═══════════════════════════════════════════════════════════════
 *  表格系统 - 终端表格绘制与格式化
 * ═══════════════════════════════════════════════════════════════ */

void TuiTable_init(TuiTable *table, int column_count) {
    if (table == 0) return;
    memset(table, 0, sizeof(*table));
    if (column_count > TUI_TABLE_MAX_COLUMNS) column_count = TUI_TABLE_MAX_COLUMNS;
    table->column_count = column_count;
    table->border_color = TUI_OC_BORDER;
    table->header_color = TUI_OC_TEXT TUI_BOLD;
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

    tui_print_margin(out, 46);
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

    tui_print_margin(out, 46);
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

    tui_print_margin(out, 46);
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

    tui_print_margin(out, 46);
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

    tui_print_margin(out, 46);
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

    tui_print_margin(out, 46);
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

    tui_print_margin(out, 46);
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
 *  菜单样式 - 菜单框架组件
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
    if (number == 0) {
        fprintf(out, "  " TUI_DIM "%d. %s" TUI_RESET, number, label);
    } else {
        /* Numbered badge + label */
        fprintf(out, " \033[48;5;236m\033[38;5;214m %d \033[0m ", number);
        fprintf(out, "\033[38;5;252m%s\033[0m", label);
    }

    content_w = 5 + tui_display_width(label);
    if (number >= 10) content_w += 1;
    if (number != 0) content_w += 2; /* badge padding */
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
    if (number == 0) {
        fprintf(out, "  " TUI_DIM "%d. %s %s" TUI_RESET, number, icon != 0 ? icon : "", label);
    } else {
        /* Numbered badge */
        fprintf(out, " \033[48;5;236m\033[38;5;214m %d \033[0m ", number);
        /* Icon with distinct color */
        if (icon != 0) {
            fprintf(out, "\033[38;5;51m");  /* bright cyan */
            fputs(icon, out);
            fputs(TUI_RESET, out);
            fputc(' ', out);
        }
        fprintf(out, "\033[38;5;252m%s\033[0m", label);
    }

    content_w = 5 + tui_display_width(label);
    if (icon != 0) content_w += tui_display_width(icon) + 1;
    if (number >= 10) content_w += 1;
    if (number != 0) content_w += 2; /* badge padding */
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
 *  动画系统 - 终端动画特效引擎
 *  所有动画在非交互模式（管道重定向）下自动降级为静态输出
 * ═══════════════════════════════════════════════════════════════ */

#ifdef _WIN32
#ifndef __MINGW32__
#include <windows.h>
#include <io.h>
static void tui_sleep_ms(int ms) { Sleep(ms); }
static int tui_check_tty(void) { return _isatty(_fileno(stdout)); }
#else
/* MinGW: POSIX sleep + Windows console API */
#include <windows.h>
#include <unistd.h>
static void tui_sleep_ms(int ms) { usleep(ms * 1000); }
static int tui_check_tty(void) { return isatty(STDOUT_FILENO); }
#endif
#else
#include <unistd.h>
#include <sys/ioctl.h>
static void tui_sleep_ms(int ms) { usleep(ms * 1000); }
static int tui_check_tty(void) { return isatty(STDOUT_FILENO); }
#endif

static int g_animation_interactive = -1; /* -1 = 未检测，0 = 非交互，1 = 交互 */

int tui_is_interactive(void) {
    if (g_animation_interactive < 0)
        g_animation_interactive = tui_check_tty();
    return g_animation_interactive;
}

/**
 * @brief 获取终端宽度（列数）
 * @return 终端列数，无法获取时返回 80
 */
int tui_get_terminal_width(void) {
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi)) {
        return csbi.srWindow.Right - csbi.srWindow.Left + 1;
    }
#else
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0) {
        return ws.ws_col;
    }
#endif
    return 80;
}

int tui_get_terminal_height(void) {
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi)) {
        return csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
    }
#else
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_row > 0) {
        return ws.ws_row;
    }
#endif
    return 24;
}

/**
 * @brief 打印居中左边距
 * @param out    输出流
 * @param width  内容的显示宽度
 */
void tui_print_margin(FILE *out, int width) {
    int term_w = tui_get_terminal_width();
    int margin = (term_w - width) / 2;
    int i;
    if (margin < 2) margin = 2;
    for (i = 0; i < margin; i++) {
        fputc(' ', out);
    }
}

void tui_delay(int ms) {
    if (!tui_is_interactive()) return;
    if (ms > 0) tui_sleep_ms(ms);
}

/* ── Animated Logo ────────────────────────────────────────────── */

void tui_animate_logo(FILE *out) {
    tui_print_logo(out);
}

/* ── Animated Banner ──────────────────────────────────────────── */

void tui_animate_banner(FILE *out, const char *title) {
    tui_print_banner(out, title);
}

/* ── Typewriter ───────────────────────────────────────────────── */

void tui_animate_typewriter(FILE *out, const char *text, int char_delay_ms) {
    const unsigned char *p = 0;

    if (out == 0 || text == 0) return;
    if (!tui_is_interactive()) { fputs(text, out); return; }
    if (char_delay_ms <= 0) char_delay_ms = 30;

    p = (const unsigned char *)text;
    while (*p != 0) {
        p += tui_fputc_utf8(out, p);
        fflush(out);
        tui_sleep_ms(char_delay_ms);
    }
}

/* ── Animated Welcome ─────────────────────────────────────────── */

void tui_animate_welcome(FILE *out, TuiRoleTheme theme, const char *user_id) {
    tui_print_welcome(out, theme, user_id);
}

/* ── Animated Goodbye ─────────────────────────────────────────── */

void tui_animate_goodbye(FILE *out) {
    tui_print_goodbye(out);
}

/* ── Screen Transition ────────────────────────────────────────── */

void tui_animate_transition(FILE *out) {
    int row = 0;
    int i = 0;

    if (out == 0 || !tui_is_interactive()) return;

    tui_hide_cursor(out);
    for (row = 0; row < 8; row++) {
        tui_print_margin(out, 46);
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
    if (!tui_is_interactive()) {
        const char *p = text;
        while (*p != '\0') {
            const char *nl = p;
            while (*nl != '\0' && *nl != '\n') nl++;
            tui_print_margin(out, 46);
            fwrite(p, 1, (size_t)(nl - p), out);
            fputc('\n', out);
            if (*nl == '\n') nl++;
            p = nl;
        }
        return;
    }
    if (line_delay_ms <= 0) line_delay_ms = 40;

    start = text;
    while (*start != '\0') {
        end = start;
        while (*end != '\0' && *end != '\n') end++;

        /* Print this line */
        tui_print_margin(out, 46);
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
    int msg_w = 0;

    if (out == 0) return;
    if (!tui_is_interactive()) {
        if (message != 0) {
            msg_w = tui_display_width(message);
            tui_print_margin(out, msg_w);
            fprintf(out, "%s\n", message);
        }
        return;
    }
    if (duration_ms <= 0) duration_ms = 500;

    msg_w = tui_display_width(message != 0 ? message : "") + 2; /* spinner + space + message */
    tui_hide_cursor(out);
    while (elapsed < duration_ms) {
        fputs("\r\033[2K", out);
        tui_print_margin(out, msg_w);
        fprintf(out, "%s%s%s %s%s%s",
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

/* ── 马赛克动态启动画面 ──────────────────────────────────────── */

void tui_mosaic_splash(FILE *out, int duration_ms) {
    int tw, width, sweep_range, frame_count, f;
    const int step_ms = 16; /* ~60fps */

    if (out == 0) return;
    if (!tui_mosaic_enabled()) return;

    /* 严格预算上限：避免再出现 v7.2.0 的启动卡顿 */
    if (duration_ms <= 0) duration_ms = 200;
    if (duration_ms > 400) duration_ms = 400;

    tw = tui_get_terminal_width();
    width = tw > 20 ? tw - 4 : 40;
    if (width > 72) width = 72;

    sweep_range = width + 16;
    frame_count = duration_ms / step_ms;
    if (frame_count < 6) frame_count = 6;

    tui_hide_cursor(out);
    for (f = 0; f < frame_count; f++) {
        /* 扫描中心从左扫到右：-8 → width+8 */
        int phase = (int)((long)(f) * sweep_range / (frame_count - 1));
        fputs("\r\033[2K", out);
        tui_print_margin(out, width);
        tui_print_mosaic_strip(out, width, phase);
        fflush(out);
        tui_sleep_ms(step_ms);
    }
    fputc('\n', out);
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
        int content_w = bar_width + 8; /* bar + borders + percentage + spaces */
        if (label != 0) content_w += tui_display_width(label) + 1;
        fputs("\r\033[2K", out);
        tui_print_margin(out, content_w);
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
    int msg_w = 0;
    if (out == 0 || message == 0) return;
    if (!tui_is_interactive()) { tui_print_success(out, message); return; }

    msg_w = tui_display_width(message) + 4; /* icon + spaces + message */
    /* Flash: reverse green → normal green */
    fputc('\n', out);
    tui_print_margin(out, msg_w);
    fprintf(out, TUI_REVERSE TUI_BOLD_GREEN " %s %s " TUI_RESET, TUI_CHECK, message);
    fflush(out);
    tui_sleep_ms(150);
    fputs("\r\033[2K", out);
    fputc('\n', out);
    tui_print_margin(out, msg_w);
    fprintf(out, TUI_BOLD_GREEN "%s %s" TUI_RESET "\n", TUI_CHECK, message);
    fflush(out);
}

void tui_animate_error(FILE *out, const char *message) {
    int msg_w = 0;
    if (out == 0 || message == 0) return;
    if (!tui_is_interactive()) { tui_print_error(out, message); return; }

    msg_w = tui_display_width(message) + 4; /* icon + spaces + message */
    /* Flash: reverse red → normal red */
    fputc('\n', out);
    tui_print_margin(out, msg_w);
    fprintf(out, TUI_REVERSE TUI_BOLD_RED " %s %s " TUI_RESET, TUI_CROSS, message);
    fflush(out);
    tui_sleep_ms(150);
    fputs("\r\033[2K", out);
    fputc('\n', out);
    tui_print_margin(out, msg_w);
    fprintf(out, TUI_BOLD_RED "%s %s" TUI_RESET "\n", TUI_CROSS, message);
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
        fputs("\r\033[2K", out);
        tui_print_margin(out, len);
        while (*p != 0) {
            fputs(GRADIENT_COLORS[offset % GRADIENT_COUNT], out);
            fputs(TUI_BOLD, out);
            p += tui_fputc_utf8(out, p);
            offset++;
        }
        fputs(TUI_RESET, out);
        fflush(out);
        tui_sleep_ms(frame_delay_ms);
    }
    fputc('\n', out);
    fflush(out);
    tui_show_cursor(out);
}

/* ═══════════════════════════════════════════════════════════════
 *  高级动画特效 - 矩阵雨/粒子爆炸/心跳/故障/烟花/DNA螺旋等
 * ═══════════════════════════════════════════════════════════════ */

/* 简单伪随机数生成器（xorshift32 算法） */
static unsigned int tui_rand_state = 0;

static void tui_rand_seed(void) {
    tui_rand_state = (unsigned int)time(0) ^ 0xDEADBEEF;
}

static int tui_rand(void) {
    tui_rand_state ^= tui_rand_state << 13;
    tui_rand_state ^= tui_rand_state >> 17;
    tui_rand_state ^= tui_rand_state << 5;
    return (int)(tui_rand_state & 0x7FFF);
}

/* 256色辅助函数 */
static void tui_set_fg256(FILE *out, int color) {
    fprintf(out, "\033[38;5;%dm", color);
}

static void tui_set_bg256(FILE *out, int color) {
    fprintf(out, "\033[48;5;%dm", color);
}

/* 预计算正弦查找表，用于提升动画性能（256 个条目，范围 -1.0 到 1.0） */
static double SIN_LUT[256];
static int g_sin_lut_ready = 0;

static void tui_init_sin_lut(void) {
    int i;
    if (g_sin_lut_ready) return;
    for (i = 0; i < 256; i++) {
        SIN_LUT[i] = sin(i * 2.0 * M_PI / 256.0);
    }
    g_sin_lut_ready = 1;
}

static double tui_fast_sin(double x) {
    int idx = ((int)(x * 256.0 / (2.0 * M_PI)) % 256 + 256) % 256;
    return SIN_LUT[idx];
}

/* ── Matrix Digital Rain ─────────────────────────────────────── */

void tui_animate_matrix_rain(FILE *out, int width, int height, int duration_ms) {
    /* 数字流 + 少量医疗符号，保持简洁不杂乱 */
    static const char *RAIN_CHARS[] = {
        "0", "1", "0", "1", "0", "1",   /* 以二进制数字为主 */
        "0", "1", "0", "1",
        "\xe2\x9a\x95", /* ⚕ */
        "\xe2\x9c\x9a", /* ✚ */
        "\xe2\x99\xa5", /* ♥ */
        "H", "I", "S"
    };
    #define RAIN_CHAR_COUNT 16

    int drops[60]; /* column positions, max 60 columns */
    int speeds[60];
    int elapsed = 0;
    int step = 50;
    int col = 0;
    int r = 0;

    if (out == 0 || !tui_is_interactive()) return;
    if (width <= 0) width = 44;
    if (width > 60) width = 60;
    if (height <= 0) height = 16;
    if (duration_ms <= 0) duration_ms = 2000;

    tui_rand_seed();

    /* Initialize drop positions */
    for (col = 0; col < width; col++) {
        drops[col] = -(tui_rand() % height);
        speeds[col] = 1 + (tui_rand() % 3);
    }

    tui_hide_cursor(out);

    /* Reserve screen space */
    for (r = 0; r < height; r++) fputc('\n', out);
    fprintf(out, "\033[%dA", height); /* move back up */
    fflush(out);

    while (elapsed < duration_ms) {
        /* Draw frame */
        for (r = 0; r < height; r++) {
            tui_move_cursor(out, r + 1, 1);
            tui_print_margin(out, width);
            for (col = 0; col < width; col++) {
                int dist = r - drops[col];
                if (dist == 0) {
                    /* Head of drop: bright white/green */
                    tui_set_fg256(out, 15); /* bright white */
                    fputs(RAIN_CHARS[tui_rand() % RAIN_CHAR_COUNT], out);
                } else if (dist > 0 && dist < 6) {
                    /* Trail: fading green */
                    int green_shades[] = {46, 40, 34, 28, 22};
                    tui_set_fg256(out, green_shades[dist - 1]);
                    fputs(RAIN_CHARS[tui_rand() % RAIN_CHAR_COUNT], out);
                } else if (dist >= 6 && dist < 10) {
                    /* Far trail: dim digits only */
                    tui_set_fg256(out, 22);
                    fputs(RAIN_CHARS[tui_rand() % 2], out);  /* 仅 "0" 或 "1" */
                } else {
                    fputc(' ', out);
                }
            }
            fputs(TUI_RESET, out);
        }
        fflush(out);

        /* Update drops */
        for (col = 0; col < width; col++) {
            drops[col] += speeds[col];
            if (drops[col] > height + 10) {
                drops[col] = -(tui_rand() % (height / 2));
                speeds[col] = 1 + (tui_rand() % 3);
            }
        }

        tui_sleep_ms(step);
        elapsed += step;
    }

    /* Clear the rain area */
    for (r = 0; r < height; r++) {
        tui_move_cursor(out, r + 1, 1);
        fputs("\033[2K", out);
    }
    tui_move_cursor(out, 1, 1);
    fflush(out);
    tui_show_cursor(out);
    #undef RAIN_CHAR_COUNT
}

/* ── Particle Explosion ──────────────────────────────────────── */

void tui_animate_particle_explosion(FILE *out, int width, int height) {
    #define MAX_PARTICLES 40
    double px[MAX_PARTICLES], py[MAX_PARTICLES];
    double vx[MAX_PARTICLES], vy[MAX_PARTICLES];
    int colors[MAX_PARTICLES];
    int particle_char[MAX_PARTICLES];
    static const char *PARTICLE_SYMS[] = {
        "\xe2\x9c\xa8", /* ✨ */
        "\xe2\x97\x86", /* ◆ */
        "\xe2\x97\x87", /* ◇ */
        "\xe2\x80\xa2", /* • */
        "\xe2\x98\x85", /* ★ */
        "\xe2\x96\xb2", /* ▲ */
        "\xe2\x99\xa6", /* ♦ */
        "\xe2\x9a\xa1"  /* ⚡ */
    };
    int frame = 0;
    int i = 0;
    int r = 0;
    double cx = 0;
    double cy = 0;
    double angle = 0;
    double speed = 0;
    int margin_col = 0;

    if (out == 0 || !tui_is_interactive()) return;
    if (width <= 0) width = 44;
    if (height <= 0) height = 12;

    { int tw = tui_get_terminal_width(); margin_col = (tw - width) / 2; if (margin_col < 2) margin_col = 2; }
    tui_rand_seed();
    cx = width / 2.0;
    cy = height / 2.0;

    /* Initialize particles radiating from center */
    for (i = 0; i < MAX_PARTICLES; i++) {
        px[i] = cx;
        py[i] = cy;
        angle = (tui_rand() % 360) * M_PI / 180.0;
        speed = 0.3 + (tui_rand() % 100) / 100.0 * 1.5;
        vx[i] = cos(angle) * speed;
        vy[i] = sin(angle) * speed * 0.5; /* aspect ratio correction */
        colors[i] = (tui_rand() % GRADIENT_COUNT);
        particle_char[i] = tui_rand() % 8;
    }

    tui_hide_cursor(out);

    /* Reserve space */
    for (r = 0; r < height; r++) fputc('\n', out);
    fprintf(out, "\033[%dA", height);
    fflush(out);

    for (frame = 0; frame < 25; frame++) {
        /* Clear frame */
        for (r = 0; r < height; r++) {
            tui_move_cursor(out, r + 1, 1);
            fputs("\033[2K", out);
        }

        /* Draw particles */
        for (i = 0; i < MAX_PARTICLES; i++) {
            int ix = (int)(px[i] + 0.5);
            int iy = (int)(py[i] + 0.5);
            if (ix >= 0 && ix < width && iy >= 0 && iy < height) {
                tui_move_cursor(out, iy + 1, ix + margin_col);
                /* Fade based on distance from center */
                if (frame < 8) {
                    fputs(GRADIENT_COLORS[colors[i]], out);
                    fputs(TUI_BOLD, out);
                } else if (frame < 16) {
                    fputs(GRADIENT_COLORS[colors[i]], out);
                } else {
                    fputs(TUI_DIM, out);
                    fputs(GRADIENT_COLORS[colors[i]], out);
                }
                fputs(PARTICLE_SYMS[particle_char[i]], out);
                fputs(TUI_RESET, out);
            }
            /* Update physics */
            px[i] += vx[i];
            py[i] += vy[i];
            vy[i] += 0.03; /* gravity */
            vx[i] *= 0.97; /* air resistance */
        }

        fflush(out);
        tui_sleep_ms(60);
    }

    /* Clear */
    for (r = 0; r < height; r++) {
        tui_move_cursor(out, r + 1, 1);
        fputs("\033[2K", out);
    }
    tui_move_cursor(out, 1, 1);
    fflush(out);
    tui_show_cursor(out);
    #undef MAX_PARTICLES
}

/* ── Heartbeat Monitor ───────────────────────────────────────── */

/**
 * @brief 心电图内部定位：恢复保存的光标位置并移动到指定偏移
 * @param out   输出流
 * @param row_offset 相对于保存位置的行偏移（0=同行）
 * @param col   目标列号
 */
static void ecg_move(FILE *out, int row_offset, int col) {
    fprintf(out, "\033[u");  /* 恢复到保存的锚点位置 */
    if (row_offset > 0) {
        fprintf(out, "\033[%dB", row_offset);  /* 向下移动 */
    }
    fprintf(out, "\033[%dG", col);  /* 移到目标列 */
}

void tui_animate_heartbeat(FILE *out, int width, int beats) {
    /*
     * 真实 PQRST 心电图波形数据（行偏移量，负=向上，正=向下）
     * 波形结构: 基线 → P波(小隆起) → PR段 → QRS波群(尖锐脉冲) →
     *           ST段 → T波(宽隆起) → 基线
     */
    static const int ECG[] = {
        /* 基线平坦段 */
         0, 0, 0, 0, 0, 0, 0, 0,
        /* P波：心房去极化，温和向上隆起 */
         0,-1,-1,-1,-1, 0,
        /* PR段：短暂平坦 */
         0, 0, 0,
        /* QRS波群：心室去极化，特征性尖锐脉冲 */
         1, 2,            /* Q波下行 */
        -2,-5,-7,-6,-3,   /* R波急剧上冲至顶峰 */
         1, 3, 2,         /* S波下行回落 */
        /* ST段：回归基线 */
         0, 0, 0,
        /* T波：心室复极化，宽缓隆起 */
         0,-1,-2,-2,-2,-1, 0,
        /* 尾部基线平坦段 */
         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    };
    #define ECG_LEN 52
    #define ECG_ROWS 9
    #define ECG_FRAME_TOP 2       /* 监护仪框架起始行 */
    #define ECG_WAVE_TOP 3        /* 波形绘制区起始行 */

    int beat = 0;
    int pos = 0;
    int i = 0;
    int baseline = 5;             /* 基线位于波形区第5行(0-indexed)，留出上方空间给R峰 */
    int total_rows = ECG_ROWS + 3; /* 框架总高度(含顶部标签行+顶边框+底边框) */
    int frame_w = 0;
    int margin_col = 0;           /* 居中起始列 */

    if (out == 0 || !tui_is_interactive()) return;
    if (width <= 0) width = 48;
    if (width > 60) width = 60;
    if (beats <= 0) beats = 2;
    frame_w = width + 2;          /* 框架宽度 = 波形宽度 + 左右边框 */
    { int tw = tui_get_terminal_width(); margin_col = (tw - frame_w) / 2; if (margin_col < 2) margin_col = 2; }

    tui_hide_cursor(out);

    /* 预留屏幕空间 */
    fputc('\n', out);
    for (i = 0; i < total_rows; i++) fputc('\n', out);
    fprintf(out, "\033[%dA", total_rows);  /* 回到预留区顶部 */
    fprintf(out, "\033[s");                /* 保存此位置作为锚点 */
    fflush(out);

    /* ── 绘制监护仪框架 ── */

    /* 标签行: ♥ ECG Monitor */
    ecg_move(out, 0, margin_col);
    tui_set_fg256(out, 34);  /* 暗绿色 */
    fputs(TUI_BOLD, out);
    fputs(TUI_HEART, out);
    fputs(" ECG", out);
    fputs(TUI_RESET, out);
    tui_set_fg256(out, 238);
    fputs(" Monitor", out);
    fputs(TUI_RESET, out);

    /* 顶边框: ╔═══════════════════════╗ */
    ecg_move(out,1, margin_col);
    tui_set_fg256(out, 22);  /* 深绿色边框 */
    fputs(TUI_DTL, out);
    for (i = 0; i < width; i++) fputs(TUI_DH, out);
    fputs(TUI_DTR, out);
    fputs(TUI_RESET, out);

    /* 侧边框: ║ ... ║ (9行波形区) */
    for (i = 0; i < ECG_ROWS; i++) {
        ecg_move(out,2 + i, margin_col);
        tui_set_fg256(out, 22);
        fputs(TUI_DV, out);
        /* 暗色网格线 */
        {
            int c = 0;
            for (c = 0; c < width; c++) {
                if (i == baseline) {
                    tui_set_fg256(out, 236);
                    fputs(TUI_H, out);     /* 基线用暗灰色水平虚线标记 */
                } else {
                    fputc(' ', out);
                }
            }
        }
        tui_set_fg256(out, 22);
        fputs(TUI_DV, out);
        fputs(TUI_RESET, out);
    }

    /* 底边框: ╚══════ BPM: 72 ═══════╝ */
    {
        char bpm_label[32];
        int label_len = 0;
        int pad_left = 0;
        int pad_right = 0;

        snprintf(bpm_label, sizeof(bpm_label), " BPM: 72 ");
        label_len = (int)strlen(bpm_label);
        pad_left = (width - label_len) / 2;
        pad_right = width - label_len - pad_left;

        ecg_move(out,2 + ECG_ROWS, margin_col);
        tui_set_fg256(out, 22);
        fputs(TUI_DBL, out);
        for (i = 0; i < pad_left; i++) fputs(TUI_DH, out);
        tui_set_fg256(out, 46);
        fputs(TUI_BOLD, out);
        fputs(bpm_label, out);
        fputs(TUI_RESET, out);
        tui_set_fg256(out, 22);
        for (i = 0; i < pad_right; i++) fputs(TUI_DH, out);
        fputs(TUI_DBR, out);
        fputs(TUI_RESET, out);
    }
    fflush(out);

    /* ── 绘制波形 ── */
    for (beat = 0; beat < beats; beat++) {
        for (pos = 0; pos < ECG_LEN; pos++) {
            int col = (beat * ECG_LEN + pos) % width;
            int curr_y = baseline + ECG[pos];
            int prev_y = (pos > 0) ? baseline + ECG[pos - 1]
                                   : (beat > 0 ? baseline + ECG[ECG_LEN - 1] : baseline);
            int row = 0;
            int y_min = 0;
            int y_max = 0;

            /* 新一轮扫描开始时清除波形区内容 */
            if (col == 0 && (beat > 0 || pos > 0)) {
                for (row = 0; row < ECG_ROWS; row++) {
                    ecg_move(out,2 + row, margin_col + 1);
                    for (i = 0; i < width; i++) {
                        if (row == baseline) {
                            tui_set_fg256(out, 236);
                            fputs(TUI_H, out);
                        } else {
                            fputc(' ', out);
                        }
                    }
                    fputs(TUI_RESET, out);
                }
            }

            /* 选择波形颜色: R峰用亮绿色，其余用标准绿色 */
            if (ECG[pos] <= -5) {
                tui_set_fg256(out, 82);  /* 亮绿色 - R峰 */
                fputs(TUI_BOLD, out);
            } else {
                tui_set_fg256(out, 46);  /* 标准绿色 */
            }

            /* 绘制连续线段：连接前一个点和当前点 */
            if (curr_y == prev_y) {
                /* 水平线 */
                if (curr_y >= 0 && curr_y < ECG_ROWS) {
                    ecg_move(out,2 + curr_y, margin_col + 1 + col);
                    fputs(TUI_H, out);  /* ─ */
                }
            } else {
                /* 垂直或斜线：绘制从 prev_y 到 curr_y 的连线 */
                y_min = prev_y < curr_y ? prev_y : curr_y;
                y_max = prev_y < curr_y ? curr_y : prev_y;
                if (y_min < 0) y_min = 0;
                if (y_max >= ECG_ROWS) y_max = ECG_ROWS - 1;

                if (y_max - y_min <= 1) {
                    /* 差值为1: 用斜线字符 */
                    if (curr_y >= 0 && curr_y < ECG_ROWS) {
                        ecg_move(out,2 + curr_y, margin_col + 1 + col);
                        fputs(curr_y < prev_y ? "/" : "\\", out);
                    }
                } else {
                    /* 大于1的跨度: 用竖线连接中间行，端点用拐角 */
                    for (row = y_min; row <= y_max; row++) {
                        if (row >= 0 && row < ECG_ROWS) {
                            ecg_move(out,2 + row, margin_col + 1 + col);
                            if (row == curr_y) {
                                fputs(TUI_H, out);    /* ─ 当前点用水平线 */
                            } else if (row == y_min || row == y_max) {
                                fputs(TUI_V, out);    /* │ 端点 */
                            } else {
                                fputs(TUI_V, out);    /* │ 中间段 */
                            }
                        }
                    }
                }
            }
            fputs(TUI_RESET, out);
            fflush(out);
            tui_sleep_ms(30);
        }
    }

    /* 心跳闪烁效果：让心形符号闪烁一次 */
    ecg_move(out, 0, margin_col);
    tui_set_fg256(out, 196);  /* 红色 */
    fputs(TUI_BOLD, out);
    fputs(TUI_HEART, out);
    fputs(TUI_RESET, out);
    fflush(out);
    tui_sleep_ms(300);
    ecg_move(out, 0, margin_col);
    tui_set_fg256(out, 34);
    fputs(TUI_BOLD, out);
    fputs(TUI_HEART, out);
    fputs(TUI_RESET, out);
    fflush(out);

    /* 移动光标到框架下方 */
    ecg_move(out,3 + ECG_ROWS, 1);
    fputc('\n', out);
    fflush(out);
    tui_show_cursor(out);

    #undef ECG_LEN
    #undef ECG_ROWS
    #undef ECG_FRAME_TOP
    #undef ECG_WAVE_TOP
}

/* ── Glitch Effect ───────────────────────────────────────────── */

void tui_animate_glitch(FILE *out, const char *text, int intensity, int duration_ms) {
    static const char *GLITCH_CHARS[] = {
        "\xe2\x96\x88", "\xe2\x96\x93", "\xe2\x96\x92", "\xe2\x96\x91",
        "#", "@", "&", "%", "$", "!", "?", "/", "\\", "|",
        "\xe2\x94\x80", "\xe2\x94\x82"
    };
    #define GLITCH_CHAR_COUNT 16
    int elapsed = 0;
    int step = 50;
    int text_len = 0;

    if (out == 0 || text == 0) return;
    if (!tui_is_interactive()) { fputs(text, out); fputc('\n', out); return; }
    if (intensity <= 0) intensity = 3;
    if (duration_ms <= 0) duration_ms = 800;

    tui_rand_seed();
    text_len = tui_display_width(text);

    tui_hide_cursor(out);

    while (elapsed < duration_ms) {
        const unsigned char *p = (const unsigned char *)text;
        int char_idx = 0;
        int glitch_count = intensity;

        fputs("\r\033[2K", out);
        tui_print_margin(out, text_len);

        /* Decide which positions get glitched this frame */
        if (elapsed > duration_ms * 3 / 4) {
            glitch_count = 1; /* fewer glitches as we settle */
        }

        p = (const unsigned char *)text;
        char_idx = 0;
        while (*p != 0) {
            int should_glitch = 0;
            int ci = 0;

            /* Random glitch at some positions */
            for (ci = 0; ci < glitch_count; ci++) {
                if ((tui_rand() % text_len) == char_idx) {
                    should_glitch = 1;
                    break;
                }
            }

            if (should_glitch) {
                /* Random color + random char */
                int color_shift = tui_rand() % 6;
                static const char *GLITCH_COLORS[] = {
                    "\033[91m", "\033[92m", "\033[93m", "\033[95m", "\033[96m", "\033[97m"
                };
                fputs(GLITCH_COLORS[color_shift], out);
                fputs(GLITCH_CHARS[tui_rand() % GLITCH_CHAR_COUNT], out);
                fputs(TUI_RESET, out);
                /* Skip the original char bytes */
                if (*p < 0x80) p += 1;
                else if (*p < 0xE0) p += 2;
                else if (*p < 0xF0) p += 3;
                else p += 4;
            } else {
                /* Normal char */
                fputs(TUI_BOLD_WHITE, out);
                if (*p < 0x80) {
                    fputc(*p, out); p += 1;
                } else if (*p < 0xE0) {
                    fputc(p[0], out); fputc(p[1], out); p += 2;
                } else if (*p < 0xF0) {
                    fputc(p[0], out); fputc(p[1], out); fputc(p[2], out); p += 3;
                } else {
                    fputc(p[0], out); fputc(p[1], out); fputc(p[2], out); fputc(p[3], out); p += 4;
                }
                fputs(TUI_RESET, out);
            }

            char_idx++;
        }

        fflush(out);
        tui_sleep_ms(step);
        elapsed += step;
    }

    /* Final clean render */
    fputs("\r\033[2K", out);
    tui_print_margin(out, text_len);
    fputs(TUI_BOLD_WHITE, out);
    fputs(text, out);
    fputs(TUI_RESET, out);
    fputc('\n', out);
    fflush(out);
    tui_show_cursor(out);
    #undef GLITCH_CHAR_COUNT
}

/* ── Fireworks ───────────────────────────────────────────────── */

void tui_animate_fireworks(FILE *out, int width, int height, int count) {
    #define FW_MAX_SPARKS 30
    int fw = 0;
    int frame = 0;
    int r = 0;
    int i = 0;
    int margin_col = 0;

    static const char *SPARK_SYMS[] = {
        "\xe2\x9c\xa8", /* ✨ */
        "\xe2\x80\xa2", /* • */
        "\xe2\x97\x86", /* ◆ */
        "\xe2\x98\x85", /* ★ */
        "\xe2\x97\x87", /* ◇ */
        "\xe2\x9a\xa1", /* ⚡ */
        "\xe2\x9d\x84", /* ❄ */
        "\xe2\x9d\x80"  /* ❀ */
    };

    if (out == 0 || !tui_is_interactive()) return;
    if (width <= 0) width = 50;
    if (height <= 0) height = 14;
    if (count <= 0) count = 3;

    { int tw = tui_get_terminal_width(); margin_col = (tw - width) / 2; if (margin_col < 2) margin_col = 2; }
    tui_rand_seed();
    tui_hide_cursor(out);

    /* Reserve space */
    for (r = 0; r < height; r++) fputc('\n', out);
    fprintf(out, "\033[%dA", height);
    fflush(out);

    for (fw = 0; fw < count; fw++) {
        /* Launch position */
        int launch_x = 5 + tui_rand() % (width - 10);
        int burst_y = 2 + tui_rand() % (height / 3);
        int color_base = tui_rand() % GRADIENT_COUNT;

        /* Phase 1: Launch trail going up */
        {
            int trail_y = 0;
            for (trail_y = height - 1; trail_y > burst_y; trail_y--) {
                /* Clear previous trail dot */
                if (trail_y < height - 1) {
                    tui_move_cursor(out, trail_y + 2, launch_x + margin_col);
                    tui_set_fg256(out, 240);
                    fputs(TUI_DOT, out);
                    fputs(TUI_RESET, out);
                }
                /* Draw current position */
                tui_move_cursor(out, trail_y + 1, launch_x + margin_col);
                fputs(TUI_BOLD_YELLOW, out);
                fputs(TUI_BLOCK_FULL, out);
                fputs(TUI_RESET, out);
                fflush(out);
                tui_sleep_ms(30);
            }
            /* Clear launch trail */
            for (trail_y = height - 1; trail_y > burst_y; trail_y--) {
                tui_move_cursor(out, trail_y + 1, launch_x + margin_col);
                fputc(' ', out);
            }
        }

        /* Phase 2: Explosion burst */
        {
            double sx[FW_MAX_SPARKS], sy[FW_MAX_SPARKS];
            double svx[FW_MAX_SPARKS], svy[FW_MAX_SPARKS];
            int spark_sym[FW_MAX_SPARKS];

            for (i = 0; i < FW_MAX_SPARKS; i++) {
                double angle = (tui_rand() % 360) * M_PI / 180.0;
                double spd = 0.4 + (tui_rand() % 100) / 100.0 * 1.2;
                sx[i] = (double)launch_x;
                sy[i] = (double)burst_y;
                svx[i] = cos(angle) * spd;
                svy[i] = sin(angle) * spd * 0.5;
                spark_sym[i] = tui_rand() % 8;
            }

            for (frame = 0; frame < 18; frame++) {
                /* Clear area */
                for (r = 0; r < height; r++) {
                    tui_move_cursor(out, r + 1, margin_col);
                    for (i = 0; i < width; i++) fputc(' ', out);
                }

                /* Draw sparks */
                for (i = 0; i < FW_MAX_SPARKS; i++) {
                    int ix = (int)(sx[i] + 0.5);
                    int iy = (int)(sy[i] + 0.5);
                    if (ix >= 0 && ix < width && iy >= 0 && iy < height) {
                        tui_move_cursor(out, iy + 1, ix + margin_col);
                        if (frame < 6) {
                            fputs(GRADIENT_COLORS[(color_base + i) % GRADIENT_COUNT], out);
                            fputs(TUI_BOLD, out);
                        } else if (frame < 12) {
                            fputs(GRADIENT_COLORS[(color_base + i) % GRADIENT_COUNT], out);
                        } else {
                            fputs(TUI_DIM, out);
                        }
                        fputs(SPARK_SYMS[spark_sym[i]], out);
                        fputs(TUI_RESET, out);
                    }
                    sx[i] += svx[i];
                    sy[i] += svy[i];
                    svy[i] += 0.04; /* gravity */
                    svx[i] *= 0.96;
                }
                fflush(out);
                tui_sleep_ms(55);
            }
        }

        tui_sleep_ms(200);
    }

    /* Final clear */
    for (r = 0; r < height; r++) {
        tui_move_cursor(out, r + 1, 1);
        fputs("\033[2K", out);
    }
    tui_move_cursor(out, 1, 1);
    fflush(out);
    tui_show_cursor(out);
    #undef FW_MAX_SPARKS
}

/* ── DNA Double Helix ────────────────────────────────────────── */

void tui_animate_dna_helix(FILE *out, int height, int frames) {
    int frame = 0;
    int row = 0;

    static const int PAIR_COLORS[] = { 196, 46, 33, 226 }; /* red, green, blue, yellow */

    if (out == 0 || !tui_is_interactive()) return;
    if (height <= 0) height = 12;
    if (frames <= 0) frames = 30;

    tui_init_sin_lut();
    tui_rand_seed();
    tui_hide_cursor(out);

    {
    int dna_w = 40; /* total DNA display width (amplitude 10 * 2 + some margin) */
    int dna_margin = 0;
    int dna_center = 0;
    { int tw = tui_get_terminal_width(); dna_margin = (tw - dna_w) / 2; if (dna_margin < 2) dna_margin = 2; }
    dna_center = dna_margin + dna_w / 2;

    /* Reserve space */
    fputc('\n', out);
    for (row = 0; row < height; row++) fputc('\n', out);
    fprintf(out, "\033[%dA", height);
    fflush(out);

    for (frame = 0; frame < frames; frame++) {
        for (row = 0; row < height; row++) {
            double phase = (row + frame) * 0.5;
            double x = tui_fast_sin(phase) * 10.0;
            int left_pos = dna_center + (int)(x);
            int right_pos = dna_center - (int)(x);
            int pair_idx = (row + frame) % 4;
            int i_b = 0;

            tui_move_cursor(out, row + 2, 1);
            fputs("\033[2K", out); /* clear line */

            if (left_pos < right_pos) {
                /* Left strand visible, bridge, right strand */
                { int p; for (p = 0; p < left_pos; p++) fputc(' ', out); }
                tui_set_fg256(out, 51); /* cyan */
                fputs(TUI_BOLD, out);
                fputs("\xe2\x97\x89", out); /* ◉ left node */

                /* Bridge (base pair) */
                if (left_pos + 2 < right_pos) {
                    tui_set_fg256(out, PAIR_COLORS[pair_idx]);
                    fputs(TUI_DIM, out);
                    for (i_b = left_pos + 2; i_b < right_pos; i_b++) fputs(TUI_H, out);
                }

                tui_set_fg256(out, 213); /* pink */
                fputs(TUI_BOLD, out);
                fputs("\xe2\x97\x89", out); /* ◉ right node */
                fputs(TUI_RESET, out);
            } else if (right_pos < left_pos) {
                { int p; for (p = 0; p < right_pos; p++) fputc(' ', out); }
                tui_set_fg256(out, 213);
                fputs(TUI_BOLD, out);
                fputs("\xe2\x97\x89", out);

                if (right_pos + 2 < left_pos) {
                    tui_set_fg256(out, PAIR_COLORS[pair_idx]);
                    fputs(TUI_DIM, out);
                    for (i_b = right_pos + 2; i_b < left_pos; i_b++) fputs(TUI_H, out);
                }

                tui_set_fg256(out, 51);
                fputs(TUI_BOLD, out);
                fputs("\xe2\x97\x89", out);
                fputs(TUI_RESET, out);
            } else {
                /* Crossed - show X */
                { int p; for (p = 0; p < left_pos; p++) fputc(' ', out); }
                tui_set_fg256(out, 226); /* yellow */
                fputs(TUI_BOLD, out);
                fputs(TUI_DX, out); /* ╬ */
                fputs(TUI_RESET, out);
            }
        }
        fflush(out);
        tui_sleep_ms(80);
    }

    /* Clear */
    for (row = 0; row < height + 1; row++) {
        tui_move_cursor(out, row + 1, 1);
        fputs("\033[2K", out);
    }
    tui_move_cursor(out, 1, 1);
    fflush(out);
    tui_show_cursor(out);
    } /* end dna_center scope */
}

/* ── Fade Reveal ─────────────────────────────────────────────── */

void tui_animate_fade_reveal(FILE *out, const char *text, int steps) {
    /* Block density progression: ░ → ▒ → ▓ → █ → actual text */
    static const char *DENSITY[] = {
        "\xe2\x96\x91", /* ░ */
        "\xe2\x96\x92", /* ▒ */
        "\xe2\x96\x93", /* ▓ */
        "\xe2\x96\x88"  /* █ */
    };
    int step = 0;
    int text_dw = 0;
    int i = 0;

    if (out == 0 || text == 0) return;
    if (!tui_is_interactive()) { text_dw = tui_display_width(text); tui_print_margin(out, text_dw); fputs(text, out); fputc('\n', out); return; }
    if (steps <= 0) steps = 4;

    text_dw = tui_display_width(text);
    tui_hide_cursor(out);

    /* Each density level */
    for (step = 0; step < 4 && step < steps; step++) {
        fputs("\r\033[2K", out);
        tui_print_margin(out, text_dw);
        fputs(GRADIENT_COLORS[step % GRADIENT_COUNT], out);
        for (i = 0; i < text_dw; i++) {
            fputs(DENSITY[step], out);
        }
        fputs(TUI_RESET, out);
        fflush(out);
        tui_sleep_ms(100);
    }

    /* Final: reveal actual text with color sweep */
    fputs("\r\033[2K", out);
    tui_print_margin(out, text_dw);
    {
        const unsigned char *p = (const unsigned char *)text;
        int idx = 0;
        while (*p != 0) {
            fputs(GRADIENT_COLORS[idx % GRADIENT_COUNT], out);
            fputs(TUI_BOLD, out);
            p += tui_fputc_utf8(out, p);
            fputs(TUI_RESET, out);
            fflush(out);
            tui_sleep_ms(20);
            idx++;
        }
    }
    fputc('\n', out);
    fflush(out);
    tui_show_cursor(out);
}

/* ── Wave Text Animation ─────────────────────────────────────── */

void tui_animate_wave_text(FILE *out, const char *text, int cycles, int frame_delay_ms) {
    int frame = 0;
    int total_frames = 0;
    int max_height = 3;
    int wave_margin = 0;

    if (out == 0 || text == 0) return;
    if (!tui_is_interactive()) { int tw = tui_display_width(text); tui_print_margin(out, tw); fputs(text, out); fputc('\n', out); return; }
    if (cycles <= 0) cycles = 2;
    if (frame_delay_ms <= 0) frame_delay_ms = 60;

    total_frames = cycles * 20;
    { int text_dw = tui_display_width(text); int tw = tui_get_terminal_width(); wave_margin = (tw - text_dw * 2) / 2; if (wave_margin < 2) wave_margin = 2; }

    tui_init_sin_lut();
    tui_hide_cursor(out);

    /* Reserve space for wave (max_height * 2 + 1 rows) */
    { int r; for (r = 0; r < max_height * 2 + 1; r++) fputc('\n', out); }
    fprintf(out, "\033[%dA", max_height * 2 + 1);

    for (frame = 0; frame < total_frames; frame++) {
        const unsigned char *p = (const unsigned char *)text;
        int char_idx = 0;
        int r = 0;

        /* Clear wave area */
        for (r = 0; r < max_height * 2 + 1; r++) {
            tui_move_cursor(out, r + 1, 1);
            fputs("\033[2K", out);
        }

        /* Draw each character at its wave position */
        p = (const unsigned char *)text;
        char_idx = 0;
        while (*p != 0) {
            double wave_y = tui_fast_sin((char_idx * 0.6) + (frame * 0.3)) * max_height;
            int row_offset = max_height + (int)(wave_y + 0.5);
            int col = char_idx * 2 + wave_margin;
            char ch_buf[5];
            int ch_len = 0;

            /* Extract one character */
            if (*p < 0x80) {
                ch_buf[0] = *p; ch_len = 1; p += 1;
            } else if (*p < 0xE0) {
                ch_buf[0] = p[0]; ch_buf[1] = p[1]; ch_len = 2; p += 2;
            } else if (*p < 0xF0) {
                ch_buf[0] = p[0]; ch_buf[1] = p[1]; ch_buf[2] = p[2]; ch_len = 3; p += 3;
            } else {
                ch_buf[0] = p[0]; ch_buf[1] = p[1]; ch_buf[2] = p[2]; ch_buf[3] = p[3]; ch_len = 4; p += 4;
            }
            ch_buf[ch_len] = '\0';

            if (row_offset >= 0 && row_offset < max_height * 2 + 1) {
                tui_move_cursor(out, row_offset + 1, col);
                fputs(GRADIENT_COLORS[(char_idx + frame) % GRADIENT_COUNT], out);
                fputs(TUI_BOLD, out);
                fputs(ch_buf, out);
                fputs(TUI_RESET, out);
            }
            char_idx++;
        }
        fflush(out);
        tui_sleep_ms(frame_delay_ms);
    }

    /* Clear and show final text normally */
    { int r; for (r = 0; r < max_height * 2 + 1; r++) {
        tui_move_cursor(out, r + 1, 1);
        fputs("\033[2K", out);
    }}
    tui_move_cursor(out, max_height + 1, 1);
    fputs("\033[2K", out);
    { int tw = tui_display_width(text); tui_print_margin(out, tw); }
    fputs(TUI_BOLD_WHITE, out);
    fputs(text, out);
    fputs(TUI_RESET, out);
    fputc('\n', out);

    /* Move to end */
    tui_move_cursor(out, max_height * 2 + 2, 1);
    fflush(out);
    tui_show_cursor(out);
}

/* ── Plasma Gradient Effect ──────────────────────────────────── */

void tui_animate_plasma(FILE *out, int width, int height, int frames) {
    int frame = 0;
    int row = 0;
    int col = 0;
    int margin_col = 0;

    if (out == 0 || !tui_is_interactive()) return;
    if (width <= 0) width = 44;
    if (height <= 0) height = 8;
    if (frames <= 0) frames = 30;

    { int tw = tui_get_terminal_width(); margin_col = (tw - width) / 2; if (margin_col < 2) margin_col = 2; }
    tui_init_sin_lut();
    tui_hide_cursor(out);

    /* Reserve space */
    for (row = 0; row < height; row++) fputc('\n', out);
    fprintf(out, "\033[%dA", height);

    for (frame = 0; frame < frames; frame++) {
        for (row = 0; row < height; row++) {
            tui_move_cursor(out, row + 1, margin_col);
            for (col = 0; col < width; col++) {
                /* Plasma function: combine multiple sine waves */
                double v = 0.0;
                int color256 = 0;

                v = tui_fast_sin(col * 0.15 + frame * 0.2);
                v += tui_fast_sin(row * 0.2 + frame * 0.15);
                v += tui_fast_sin((col + row) * 0.1 + frame * 0.1);
                v += tui_fast_sin((double)(col * col + row * row) * 0.01);
                v = v / 4.0; /* normalize to -1..1 */

                /* Map to 256-color: use color range 16-231 (color cube) */
                {
                    int r_val = (int)((tui_fast_sin(v * M_PI) + 1.0) * 2.5);
                    int g_val = (int)((tui_fast_sin(v * M_PI + 2.094) + 1.0) * 2.5);
                    int b_val = (int)((tui_fast_sin(v * M_PI + 4.189) + 1.0) * 2.5);
                    if (r_val > 5) r_val = 5;
                    if (g_val > 5) g_val = 5;
                    if (b_val > 5) b_val = 5;
                    color256 = 16 + 36 * r_val + 6 * g_val + b_val;
                }

                tui_set_bg256(out, color256);
                fputc(' ', out);
            }
            fputs(TUI_RESET, out);
        }
        fflush(out);
        tui_sleep_ms(50);
    }

    /* Clear */
    for (row = 0; row < height; row++) {
        tui_move_cursor(out, row + 1, 1);
        fputs("\033[2K", out);
    }
    tui_move_cursor(out, 1, 1);
    fflush(out);
    tui_show_cursor(out);
}

/* ── Themed Entrance Animation ───────────────────────────────── */

void tui_animate_entrance(FILE *out, TuiRoleTheme theme) {
    const char *color = tui_role_color(theme);
    const char *icon = tui_role_icon(theme);
    int i = 0;
    int row = 0;
    int ent_w = 44;
    int margin_col = 0;
    int center_col = 0;

    if (out == 0 || !tui_is_interactive()) return;

    { int tw = tui_get_terminal_width(); margin_col = (tw - ent_w) / 2; if (margin_col < 2) margin_col = 2; }
    center_col = margin_col + ent_w / 2;

    tui_init_sin_lut();
    tui_hide_cursor(out);

    /* Phase 1: Plasma flash (brief) */
    for (row = 0; row < 6; row++) fputc('\n', out);
    fprintf(out, "\033[6A");
    {
        int f = 0;
        for (f = 0; f < 8; f++) {
            for (row = 0; row < 6; row++) {
                tui_move_cursor(out, row + 1, margin_col);
                for (i = 0; i < ent_w; i++) {
                    double v = tui_fast_sin(i * 0.2 + f * 0.5) + tui_fast_sin(row * 0.3 + f * 0.4);
                    int bright = (int)((v + 2.0) * 32.0);
                    if (bright > 255) bright = 255;
                    if (bright < 0) bright = 0;
                    /* Use grayscale ramp 232-255 */
                    tui_set_bg256(out, 232 + (bright * 23 / 255));
                    fputc(' ', out);
                }
                fputs(TUI_RESET, out);
            }
            fflush(out);
            tui_sleep_ms(40);
        }
    }

    /* Phase 2: Converge to center with icon */
    {
        int step = 0;
        for (step = 22; step >= 0; step--) {
            for (row = 0; row < 6; row++) {
                tui_move_cursor(out, row + 1, 1);
                fputs("\033[2K", out);
            }

            /* Left particle */
            tui_move_cursor(out, 3, margin_col + (22 - step));
            fputs(color, out);
            fputs(TUI_BOLD, out);
            for (i = 0; i < 3 && i <= 22 - step; i++) {
                fputs(TUI_HH, out);
            }
            fputs(TUI_RESET, out);

            /* Right particle */
            {
                int rpos = margin_col + ent_w - (22 - step);
                if (rpos > margin_col) {
                    tui_move_cursor(out, 3, rpos - 3);
                    fputs(color, out);
                    fputs(TUI_BOLD, out);
                    for (i = 0; i < 3 && i <= 22 - step; i++) {
                        fputs(TUI_HH, out);
                    }
                    fputs(TUI_RESET, out);
                }
            }

            if (step == 0) {
                /* Center collision - show icon with burst */
                tui_move_cursor(out, 3, center_col);
                fputs(color, out);
                fputs(TUI_BOLD, out);
                fputs(icon, out);
                fputs(TUI_RESET, out);
            }

            fflush(out);
            tui_sleep_ms(step > 10 ? 15 : 25);
        }
    }

    /* Phase 3: Expand from center icon */
    {
        int step = 0;
        for (step = 0; step < 22; step++) {
            tui_move_cursor(out, 3, 1);
            fputs("\033[2K", out);
            tui_move_cursor(out, 3, center_col - step);
            fputs(color, out);
            tui_repeat(out, TUI_HH, step * 2 + 1);
            fputs(TUI_RESET, out);

            /* Center icon stays */
            tui_move_cursor(out, 3, center_col);
            fputs(color, out);
            fputs(TUI_BOLD, out);
            fputs(icon, out);
            fputs(TUI_RESET, out);

            fflush(out);
            tui_sleep_ms(15);
        }
    }

    tui_sleep_ms(150);

    /* Clear entrance area */
    for (row = 0; row < 6; row++) {
        tui_move_cursor(out, row + 1, 1);
        fputs("\033[2K", out);
    }
    tui_move_cursor(out, 1, 1);
    fflush(out);
    tui_show_cursor(out);
}

/* ── Scanline Sweep ─────────────────────────────────────────── */

void tui_animate_scanline(FILE *out, int width, int height) {
    int sweep_col = 0;
    int row = 0;
    int c = 0;
    int margin_col = 0;

    if (out == 0 || !tui_is_interactive()) return;
    if (width <= 0) width = 48;
    if (height <= 0) height = 6;
    { int tw = tui_get_terminal_width(); margin_col = (tw - width) / 2; if (margin_col < 2) margin_col = 2; }

    tui_hide_cursor(out);

    /* 预留空间 */
    for (row = 0; row < height; row++) fputc('\n', out);
    fprintf(out, "\033[%dA", height);
    fflush(out);

    /* 一道亮光带从左到右扫过暗色背景 */
    for (sweep_col = -3; sweep_col < width + 4; sweep_col++) {
        for (row = 0; row < height; row++) {
            tui_move_cursor(out, row + 1, margin_col);
            for (c = 0; c < width; c++) {
                int dist = c - sweep_col;
                if (dist < 0) dist = -dist;

                if (dist == 0) {
                    fprintf(out, "\033[48;5;255m\033[38;5;231m");
                    fputs(TUI_BLOCK_FULL, out);
                } else if (dist == 1) {
                    fprintf(out, "\033[48;5;252m");
                    fputc(' ', out);
                } else if (dist == 2) {
                    fprintf(out, "\033[48;5;246m");
                    fputc(' ', out);
                } else if (dist == 3) {
                    fprintf(out, "\033[48;5;240m");
                    fputc(' ', out);
                } else if (dist == 4) {
                    fprintf(out, "\033[48;5;236m");
                    fputc(' ', out);
                } else {
                    fprintf(out, "\033[48;5;233m");
                    fputc(' ', out);
                }
            }
            fputs(TUI_RESET, out);
        }
        fflush(out);
        tui_sleep_ms(16);
    }

    /* 清除扫描区域 */
    for (row = 0; row < height; row++) {
        tui_move_cursor(out, row + 1, 1);
        fputs("\033[2K", out);
    }
    tui_move_cursor(out, 1, 1);
    fflush(out);
    tui_show_cursor(out);
}

/* ── Logo Breathing Pulse ───────────────────────────────────── */

void tui_animate_logo_pulse(FILE *out, int cycles) {
    int cycle = 0;
    int step = 0;
    int i = 0;
    /* 亮度阶梯: 暗灰 → 全亮 → 暗灰 (灰阶 232-255) */
    static const int BRIGHTNESS[] = {
        236, 238, 240, 242, 244, 246, 248, 250, 252, 254, 255,
        254, 252, 250, 248, 246, 244, 242, 240, 238, 236
    };
    #define PULSE_STEPS 21

    if (out == 0 || !tui_is_interactive()) { tui_print_logo(out); return; }
    if (cycles <= 0) cycles = 2;

    {
        int logo_width = tui_display_width(LOGO_LINES[0]);
        int term_w = tui_get_terminal_width();
        int margin_cols = (term_w - logo_width) / 2;
        if (margin_cols < 2) margin_cols = 2;

        tui_hide_cursor(out);
        fputc('\n', out);

        /* 预留 Logo 行空间 */
        for (i = 0; i < LOGO_LINE_COUNT; i++) fputc('\n', out);
        fprintf(out, "\033[%dA", LOGO_LINE_COUNT);
        fflush(out);

        for (cycle = 0; cycle < cycles; cycle++) {
            for (step = 0; step < PULSE_STEPS; step++) {
                int gray = BRIGHTNESS[step];
                for (i = 0; i < LOGO_LINE_COUNT; i++) {
                    tui_move_cursor(out, i + 2, 1);
                    fputs("\033[2K", out);
                    tui_print_margin(out, logo_width);
                    if (gray >= 250) {
                        fputs(GRADIENT_COLORS[i % GRADIENT_COUNT], out);
                    } else {
                        fprintf(out, "\033[38;5;%dm", gray);
                    }
                    fputs(LOGO_LINES[i], out);
                    fputs(TUI_RESET, out);
                }
                fflush(out);
                tui_sleep_ms(45);
            }
        }

        /* 最终以彩色渐变定格 */
        for (i = 0; i < LOGO_LINE_COUNT; i++) {
            tui_move_cursor(out, i + 2, 1);
            fputs("\033[2K", out);
            tui_print_margin(out, logo_width);
            fputs(GRADIENT_COLORS[i % GRADIENT_COUNT], out);
            fputs(LOGO_LINES[i], out);
            fputs(TUI_RESET, out);
        }
    }
    tui_move_cursor(out, LOGO_LINE_COUNT + 2, 1);
    fflush(out);
    tui_show_cursor(out);

    #undef PULSE_STEPS
}
