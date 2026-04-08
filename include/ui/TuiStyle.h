/**
 * @file TuiStyle.h
 * @brief TUI 样式与动画模块 - 终端用户界面的视觉呈现系统
 *
 * 本模块提供完整的终端 UI 工具集，包括：
 * 1. ANSI 颜色与样式宏定义（前景色、背景色、粗体、暗淡等）
 * 2. Unicode 制表符与特殊符号宏定义（单线框、双线框、粗线框、方块元素等）
 * 3. 角色主题系统（不同角色使用不同配色方案）
 * 4. 屏幕操作（清屏、光标移动、显隐）
 * 5. CJK 中文字符显示宽度计算
 * 6. Logo/Banner/状态栏/菜单等 UI 组件的打印函数
 * 7. 消息反馈（成功、错误、警告、信息、徽章等）
 * 8. 表格系统（支持彩色行、对齐、边框）
 * 9. 动画系统（打字机、矩阵雨、粒子爆炸、心跳监护、烟花等特效）
 */

#ifndef HIS_UI_TUI_STYLE_H
#define HIS_UI_TUI_STYLE_H

#include <stdio.h>

/* ═══════════════════════════════════════════════════════════════
 *  ANSI 颜色系统 - 终端颜色与样式控制序列
 * ═══════════════════════════════════════════════════════════════ */

/* 基础样式 */
#define TUI_RESET       "\033[0m"
#define TUI_BOLD        "\033[1m"
#define TUI_DIM         "\033[2m"
#define TUI_ITALIC      "\033[3m"
#define TUI_UNDERLINE   "\033[4m"
#define TUI_BLINK       "\033[5m"
#define TUI_REVERSE     "\033[7m"

/* 标准前景色 */
#define TUI_RED         "\033[31m"
#define TUI_GREEN       "\033[32m"
#define TUI_YELLOW      "\033[33m"
#define TUI_BLUE        "\033[34m"
#define TUI_MAGENTA     "\033[35m"
#define TUI_CYAN        "\033[36m"
#define TUI_WHITE       "\033[37m"

/* 亮色前景色（256色模式） */
#define TUI_BRIGHT_RED     "\033[91m"
#define TUI_BRIGHT_GREEN   "\033[92m"
#define TUI_BRIGHT_YELLOW  "\033[93m"
#define TUI_BRIGHT_BLUE    "\033[94m"
#define TUI_BRIGHT_MAGENTA "\033[95m"
#define TUI_BRIGHT_CYAN    "\033[96m"
#define TUI_BRIGHT_WHITE   "\033[97m"

/* 粗体前景色 */
#define TUI_BOLD_RED    "\033[1;31m"
#define TUI_BOLD_GREEN  "\033[1;32m"
#define TUI_BOLD_YELLOW "\033[1;33m"
#define TUI_BOLD_BLUE   "\033[1;34m"
#define TUI_BOLD_MAGENTA "\033[1;35m"
#define TUI_BOLD_CYAN   "\033[1;36m"
#define TUI_BOLD_WHITE  "\033[1;37m"

/* 背景色 */
#define TUI_BG_RED      "\033[41m"
#define TUI_BG_GREEN    "\033[42m"
#define TUI_BG_YELLOW   "\033[43m"
#define TUI_BG_BLUE     "\033[44m"
#define TUI_BG_MAGENTA  "\033[45m"
#define TUI_BG_CYAN     "\033[46m"
#define TUI_BG_WHITE    "\033[47m"

/* 256色自定义调色板宏 */
#define TUI_FG256(n)    "\033[38;5;" #n "m"  /**< 256色前景色 */
#define TUI_BG256(n)    "\033[48;5;" #n "m"  /**< 256色背景色 */

/* ═══════════════════════════════════════════════════════════════
 *  Unicode 制表符 - 用于绘制边框和表格
 * ═══════════════════════════════════════════════════════════════ */

/* 单线制表符 */
#define TUI_H    "\xe2\x94\x80"  /* ─ */
#define TUI_V    "\xe2\x94\x82"  /* │ */
#define TUI_TL   "\xe2\x94\x8c"  /* ┌ */
#define TUI_TR   "\xe2\x94\x90"  /* ┐ */
#define TUI_BL   "\xe2\x94\x94"  /* └ */
#define TUI_BR   "\xe2\x94\x98"  /* ┘ */
#define TUI_LT   "\xe2\x94\x9c"  /* ├ */
#define TUI_RT   "\xe2\x94\xa4"  /* ┤ */
#define TUI_TT   "\xe2\x94\xac"  /* ┬ */
#define TUI_BT   "\xe2\x94\xb4"  /* ┴ */
#define TUI_X    "\xe2\x94\xbc"  /* ┼ */

/* 双线制表符 */
#define TUI_DH   "\xe2\x95\x90"  /* ═ */
#define TUI_DV   "\xe2\x95\x91"  /* ║ */
#define TUI_DTL  "\xe2\x95\x94"  /* ╔ */
#define TUI_DTR  "\xe2\x95\x97"  /* ╗ */
#define TUI_DBL  "\xe2\x95\x9a"  /* ╚ */
#define TUI_DBR  "\xe2\x95\x9d"  /* ╝ */
#define TUI_DLT  "\xe2\x95\xa0"  /* ╠ */
#define TUI_DRT  "\xe2\x95\xa3"  /* ╣ */
#define TUI_DTT  "\xe2\x95\xa6"  /* ╦ */
#define TUI_DBT  "\xe2\x95\xa9"  /* ╩ */
#define TUI_DX   "\xe2\x95\xac"  /* ╬ */

/* 粗线制表符 */
#define TUI_HH   "\xe2\x94\x81"  /* ━ */
#define TUI_HV   "\xe2\x94\x83"  /* ┃ */
#define TUI_HTL  "\xe2\x94\x8f"  /* ┏ */
#define TUI_HTR  "\xe2\x94\x93"  /* ┓ */
#define TUI_HBL  "\xe2\x94\x97"  /* ┗ */
#define TUI_HBR  "\xe2\x94\x9b"  /* ┛ */

/* 方块元素 - 用于进度条、动画等 */
#define TUI_BLOCK_FULL   "\xe2\x96\x88"  /* █ */
#define TUI_BLOCK_DARK   "\xe2\x96\x93"  /* ▓ */
#define TUI_BLOCK_MED    "\xe2\x96\x92"  /* ▒ */
#define TUI_BLOCK_LIGHT  "\xe2\x96\x91"  /* ░ */
#define TUI_BLOCK_UPPER  "\xe2\x96\x80"  /* ▀ */
#define TUI_BLOCK_LOWER  "\xe2\x96\x84"  /* ▄ */
#define TUI_BLOCK_LEFT   "\xe2\x96\x8c"  /* ▌ */
#define TUI_BLOCK_RIGHT  "\xe2\x96\x90"  /* ▐ */

/* ═══════════════════════════════════════════════════════════════
 *  Unicode 符号 - 用于菜单图标和装饰
 * ═══════════════════════════════════════════════════════════════ */

#define TUI_BULLET       "\xe2\x97\x86"  /* ◆ */
#define TUI_BULLET_O     "\xe2\x97\x87"  /* ◇ */
#define TUI_ARROW        "\xe2\x96\xb6"  /* ▶ */
#define TUI_ARROW_R      "\xe2\x96\xb7"  /* ▷ */
#define TUI_CHECK        "\xe2\x9c\x93"  /* ✓ */
#define TUI_CROSS        "\xe2\x9c\x97"  /* ✗ */
#define TUI_DOT          "\xe2\x80\xa2"  /* • */
#define TUI_STAR         "\xe2\x98\x85"  /* ★ */
#define TUI_STAR_O       "\xe2\x98\x86"  /* ☆ */
#define TUI_HEART        "\xe2\x99\xa5"  /* ♥ */
#define TUI_DIAMOND      "\xe2\x99\xa6"  /* ♦ */
#define TUI_CLUB         "\xe2\x99\xa3"  /* ♣ */
#define TUI_SPADE        "\xe2\x99\xa0"  /* ♠ */
#define TUI_CIRCLE       "\xe2\x97\x89"  /* ◉ */
#define TUI_CIRCLE_O     "\xe2\x97\x8b"  /* ○ */
#define TUI_TRIANGLE     "\xe2\x96\xb2"  /* ▲ */
#define TUI_TRIANGLE_D   "\xe2\x96\xbc"  /* ▼ */
#define TUI_LIGHTNING    "\xe2\x9a\xa1"  /* ⚡ */
#define TUI_GEAR         "\xe2\x9a\x99"  /* ⚙ */
#define TUI_MEDICAL      "\xe2\x9a\x95"  /* ⚕ */
#define TUI_FLASK        "\xe2\x9a\x97"  /* ⚗ */
#define TUI_HEAVY_CROSS  "\xe2\x9c\x9a"  /* ✚ */
#define TUI_SPARKLE      "\xe2\x9c\xa8"  /* ✨ */
#define TUI_FIRE         "\xe2\x99\xa8"  /* ♨ */
#define TUI_MUSIC        "\xe2\x99\xaa"  /* ♪ */
#define TUI_SUN          "\xe2\x98\x80"  /* ☀ */
#define TUI_MOON         "\xe2\x98\xbd"  /* ☽ */
#define TUI_UMBRELLA     "\xe2\x98\x82"  /* ☂ */
#define TUI_SNOWFLAKE    "\xe2\x9d\x84"  /* ❄ */
#define TUI_FLOWER       "\xe2\x9d\x80"  /* ❀ */
#define TUI_KEY          "\xe2\x9c\xbf"  /* ✿ */
#define TUI_LOZENGE      "\xe2\x97\x88"  /* ◈ */

/* ═══════════════════════════════════════════════════════════════
 *  256色渐变色板 - 全局共享，用于高级渐变边框和装饰
 * ═══════════════════════════════════════════════════════════════ */

/** @brief 256色平滑渐变色板（暖色日落调色板） */
extern const int TUI_GRADIENT_256[];

/** @brief 渐变色板长度 */
#define TUI_GRADIENT_256_COUNT 26

/* ═══════════════════════════════════════════════════════════════
 *  角色主题系统 - 为不同角色分配独立配色方案
 * ═══════════════════════════════════════════════════════════════ */

/**
 * @brief 角色主题枚举 - 定义各角色的视觉主题
 */
typedef enum TuiRoleTheme {
    TUI_THEME_DEFAULT = 0,
    TUI_THEME_ADMIN = 1,
    TUI_THEME_DOCTOR = 2,
    TUI_THEME_PATIENT = 3,
    TUI_THEME_INPATIENT = 4,
    TUI_THEME_PHARMACY = 5
} TuiRoleTheme;

/** @brief 获取角色主题的前景色 ANSI 序列 */
const char *tui_role_color(TuiRoleTheme theme);
/** @brief 获取角色主题的背景色 ANSI 序列 */
const char *tui_role_bg(TuiRoleTheme theme);
/** @brief 获取角色主题的图标符号 */
const char *tui_role_icon(TuiRoleTheme theme);
/** @brief 获取角色主题的中文标签 */
const char *tui_role_label(TuiRoleTheme theme);

/* ═══════════════════════════════════════════════════════════════
 *  屏幕操作 - 清屏、光标控制
 * ═══════════════════════════════════════════════════════════════ */

/** @brief 清除屏幕并将光标移至左上角 */
void tui_clear_screen(void);
/** @brief 移动光标到指定行列位置（1-based） */
void tui_move_cursor(FILE *out, int row, int col);
/** @brief 隐藏光标 */
void tui_hide_cursor(FILE *out);
/** @brief 显示光标 */
void tui_show_cursor(FILE *out);

/* ═══════════════════════════════════════════════════════════════
 *  显示宽度计算 - 中日韩(CJK)字符感知
 * ═══════════════════════════════════════════════════════════════ */

/** @brief 计算 UTF-8 字符串的终端显示宽度（CJK 字符占 2 列） */
int tui_display_width(const char *text);
/** @brief 输出文本并右侧填充空格至指定宽度 */
void tui_pad_right(FILE *out, const char *text, int width);

/* ═══════════════════════════════════════════════════════════════
 *  Logo 与 Banner - 系统标识打印
 * ═══════════════════════════════════════════════════════════════ */

/** @brief 打印 HIS 系统 ASCII Art Logo（渐变色） */
void tui_print_logo(FILE *out);
/** @brief 打印带边框的标题横幅 */
void tui_print_banner(FILE *out, const char *title);
/** @brief 以渐变色逐字符打印文本 */
void tui_print_gradient_text(FILE *out, const char *text);
/** @brief 打印欢迎登录框（含角色主题色） */
void tui_print_welcome(FILE *out, TuiRoleTheme theme, const char *user_id);
/** @brief 打印再见/退出消息 */
void tui_print_goodbye(FILE *out);

/* ═══════════════════════════════════════════════════════════════
 *  状态栏与标题 - 页面头部组件
 * ═══════════════════════════════════════════════════════════════ */

/** @brief 打印简单状态栏（蓝底白字） */
void tui_print_status_bar(FILE *out, const char *role, const char *user_id);
/** @brief 打印带角色主题的状态栏 */
void tui_print_status_bar_themed(FILE *out, TuiRoleTheme theme, const char *user_id);
/** @brief 打印居中标题框 */
void tui_print_header(FILE *out, const char *title, int width);
/** @brief 打印带图标和下划线的节标题 */
void tui_print_section(FILE *out, const char *icon, const char *title);
/** @brief 打印单线水平分隔线 */
void tui_print_hline(FILE *out, int width);
/** @brief 打印双线水平分隔线 */
void tui_print_double_hline(FILE *out, int width);
/** @brief 打印256色渐变粗线分隔线 */
void tui_print_gradient_hline(FILE *out, int width);

/* ═══════════════════════════════════════════════════════════════
 *  消息与反馈 - 操作结果展示
 * ═══════════════════════════════════════════════════════════════ */

/** @brief 打印成功消息（绿色勾号） */
void tui_print_success(FILE *out, const char *message);
/** @brief 打印错误消息（红色叉号） */
void tui_print_error(FILE *out, const char *message);
/** @brief 打印警告消息（黄色三角） */
void tui_print_warning(FILE *out, const char *message);
/** @brief 打印信息消息（蓝色圆点） */
void tui_print_info(FILE *out, const char *message);
/** @brief 打印输入提示符（箭头 + 文本） */
void tui_print_prompt(FILE *out, const char *text);
/** @brief 打印角色主题色输入提示符 */
void tui_print_prompt_themed(FILE *out, const char *text, TuiRoleTheme theme);

/** @brief 徽章颜色枚举 - 用于彩色内联标签 */
typedef enum TuiBadgeColor {
    TUI_BADGE_GREEN = 0,
    TUI_BADGE_RED = 1,
    TUI_BADGE_YELLOW = 2,
    TUI_BADGE_BLUE = 3,
    TUI_BADGE_MAGENTA = 4,
    TUI_BADGE_CYAN = 5
} TuiBadgeColor;

/** @brief 打印彩色徽章标签 */
void tui_print_badge(FILE *out, const char *text, TuiBadgeColor color);
/** @brief 打印键值对信息 */
void tui_print_kv(FILE *out, const char *key, const char *value);
/** @brief 打印带自定义颜色的键值对信息 */
void tui_print_kv_colored(FILE *out, const char *key, const char *value, const char *value_color);

/** @brief 打印静态进度条（label 当前/总数） */
void tui_print_progress(FILE *out, const char *label, int current, int total, int bar_width);

/* ═══════════════════════════════════════════════════════════════
 *  表格系统 - 终端表格绘制
 * ═══════════════════════════════════════════════════════════════ */

#define TUI_TABLE_MAX_COLUMNS 10  /**< 表格最大列数 */

/**
 * @brief 表格结构体 - 描述表格的列定义和样式
 */
typedef struct TuiTable {
    int column_count;                          /**< 列数 */
    int widths[TUI_TABLE_MAX_COLUMNS];         /**< 每列的显示宽度 */
    char headers[TUI_TABLE_MAX_COLUMNS][64];   /**< 每列的表头文字 */
    int row_count;                             /**< 数据行数 */
    const char *border_color;                  /**< 边框颜色 ANSI 序列 */
    const char *header_color;                  /**< 表头颜色 ANSI 序列 */
} TuiTable;

/** @brief 初始化表格结构体 */
void TuiTable_init(TuiTable *table, int column_count);
/** @brief 设置指定列的表头文字和宽度 */
void TuiTable_set_header(TuiTable *table, int col, const char *header, int width);
/** @brief 设置表格的边框色和表头色 */
void TuiTable_set_colors(TuiTable *table, const char *border_color, const char *header_color);
/** @brief 打印表格顶部边框 */
void TuiTable_print_top(const TuiTable *table, FILE *out);
/** @brief 打印表格表头行 */
void TuiTable_print_header_row(const TuiTable *table, FILE *out);
/** @brief 打印表格行间分隔线 */
void TuiTable_print_separator(const TuiTable *table, FILE *out);
/** @brief 打印一行数据 */
void TuiTable_print_row(const TuiTable *table, FILE *out, const char *values[], int count);
/** @brief 打印带自定义颜色的一行数据 */
void TuiTable_print_row_colored(const TuiTable *table, FILE *out, const char *values[], const char *colors[], int count);
/** @brief 打印表格底部边框 */
void TuiTable_print_bottom(const TuiTable *table, FILE *out);
/** @brief 打印空表格提示消息 */
void TuiTable_print_empty(const TuiTable *table, FILE *out, const char *message);

/* ═══════════════════════════════════════════════════════════════
 *  菜单样式 - 菜单框架组件
 * ═══════════════════════════════════════════════════════════════ */

/** @brief 打印菜单标题（委托给 tui_print_header） */
void tui_print_menu_title(FILE *out, const char *title, int width);
/** @brief 打印菜单项（编号 + 标签） */
void tui_print_menu_item(FILE *out, int number, const char *label, int width);
/** @brief 打印带图标的菜单项（编号 + 图标 + 标签） */
void tui_print_menu_item_icon(FILE *out, int number, const char *icon, const char *label, int width);
/** @brief 打印菜单底部边框 */
void tui_print_menu_bottom(FILE *out, int width);

/* ═══════════════════════════════════════════════════════════════
 *  动画系统 - 终端动画特效
 *  注意: 当 stdout 被管道重定向时，动画自动跳过
 * ═══════════════════════════════════════════════════════════════ */

/** @brief 检测当前终端是否为交互式（非管道重定向） */
int tui_is_interactive(void);

/** @brief 延迟指定毫秒（非交互模式下不执行） */
void tui_delay(int ms);

/** @brief 动画 Logo：逐行级联显示，带颜色渐变效果 */
void tui_animate_logo(FILE *out);

/** @brief 动画 Banner：彩虹条扫过 + 标题打字机效果 */
void tui_animate_banner(FILE *out, const char *title);

/** @brief 打字机效果：逐字符输出文本，每个字符间有延迟 */
void tui_animate_typewriter(FILE *out, const char *text, int char_delay_ms);

/** @brief 动画欢迎框：边框自绘 + 内容渐入 */
void tui_animate_welcome(FILE *out, TuiRoleTheme theme, const char *user_id);

/** @brief 动画再见：彩虹波浪 + 淡出效果 */
void tui_animate_goodbye(FILE *out);

/** @brief 屏幕过渡动画：渐变方块水平擦除 */
void tui_animate_transition(FILE *out);

/** @brief 逐行滑入动画：多行文本按行依次显示 */
void tui_animate_lines(FILE *out, const char *text, int line_delay_ms);

/** @brief 旋转加载指示器：在指定时长内显示旋转动画 */
void tui_spinner_run(FILE *out, const char *message, int duration_ms);

/** @brief 动画进度条：从 0% 填充到 100% */
void tui_animate_progress(FILE *out, const char *label, int steps, int bar_width);

/** @brief 成功消息动画：绿色闪烁高亮 */
void tui_animate_success(FILE *out, const char *message);
/** @brief 错误消息动画：红色闪烁高亮 */
void tui_animate_error(FILE *out, const char *message);

/** @brief 彩虹文本动画：颜色循环变幻 */
void tui_animate_rainbow(FILE *out, const char *text, int cycles, int frame_delay_ms);

/* ── 高级动画特效 ─────────────────────────────────────────── */

/** @brief 矩阵数字雨动画：医疗符号从上往下飘落 */
void tui_animate_matrix_rain(FILE *out, int width, int height, int duration_ms);

/** @brief 粒子爆炸动画：从中心向四周扩散 */
void tui_animate_particle_explosion(FILE *out, int width, int height);

/** @brief 心电图动画：ECG 波形扫描显示 */
void tui_animate_heartbeat(FILE *out, int width, int beats);

/** @brief 文本故障/扭曲效果动画 */
void tui_animate_glitch(FILE *out, const char *text, int intensity, int duration_ms);

/** @brief 烟花庆祝动画 */
void tui_animate_fireworks(FILE *out, int width, int height, int count);

/** @brief DNA 双螺旋旋转动画 */
void tui_animate_dna_helix(FILE *out, int height, int frames);

/** @brief 文本淡入动画：通过方块密度渐变揭示文字 */
void tui_animate_fade_reveal(FILE *out, const char *text, int steps);

/** @brief 正弦波浪文本动画 */
void tui_animate_wave_text(FILE *out, const char *text, int cycles, int frame_delay_ms);

/** @brief 等离子体渐变背景动画 */
void tui_animate_plasma(FILE *out, int width, int height, int frames);

/** @brief 角色主题入场动画：组合等离子+汇聚+扩展效果 */
void tui_animate_entrance(FILE *out, TuiRoleTheme theme);

/** @brief 水平光带扫描动画：一道高光从左扫到右 */
void tui_animate_scanline(FILE *out, int width, int height);

/** @brief Logo呼吸脉冲动画：Logo在明暗间平滑过渡 */
void tui_animate_logo_pulse(FILE *out, int cycles);

#endif
