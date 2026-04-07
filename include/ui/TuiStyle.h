#ifndef HIS_UI_TUI_STYLE_H
#define HIS_UI_TUI_STYLE_H

#include <stdio.h>

/* ═══════════════════════════════════════════════════════════════
 *  ANSI Color System
 * ═══════════════════════════════════════════════════════════════ */

/* Basic */
#define TUI_RESET       "\033[0m"
#define TUI_BOLD        "\033[1m"
#define TUI_DIM         "\033[2m"
#define TUI_ITALIC      "\033[3m"
#define TUI_UNDERLINE   "\033[4m"
#define TUI_BLINK       "\033[5m"
#define TUI_REVERSE     "\033[7m"

/* Standard colors */
#define TUI_RED         "\033[31m"
#define TUI_GREEN       "\033[32m"
#define TUI_YELLOW      "\033[33m"
#define TUI_BLUE        "\033[34m"
#define TUI_MAGENTA     "\033[35m"
#define TUI_CYAN        "\033[36m"
#define TUI_WHITE       "\033[37m"

/* Bright colors (256-color) */
#define TUI_BRIGHT_RED     "\033[91m"
#define TUI_BRIGHT_GREEN   "\033[92m"
#define TUI_BRIGHT_YELLOW  "\033[93m"
#define TUI_BRIGHT_BLUE    "\033[94m"
#define TUI_BRIGHT_MAGENTA "\033[95m"
#define TUI_BRIGHT_CYAN    "\033[96m"
#define TUI_BRIGHT_WHITE   "\033[97m"

/* Bold colors */
#define TUI_BOLD_RED    "\033[1;31m"
#define TUI_BOLD_GREEN  "\033[1;32m"
#define TUI_BOLD_YELLOW "\033[1;33m"
#define TUI_BOLD_BLUE   "\033[1;34m"
#define TUI_BOLD_MAGENTA "\033[1;35m"
#define TUI_BOLD_CYAN   "\033[1;36m"
#define TUI_BOLD_WHITE  "\033[1;37m"

/* Background colors */
#define TUI_BG_RED      "\033[41m"
#define TUI_BG_GREEN    "\033[42m"
#define TUI_BG_YELLOW   "\033[43m"
#define TUI_BG_BLUE     "\033[44m"
#define TUI_BG_MAGENTA  "\033[45m"
#define TUI_BG_CYAN     "\033[46m"
#define TUI_BG_WHITE    "\033[47m"

/* 256-color custom palette */
#define TUI_FG256(n)    "\033[38;5;" #n "m"
#define TUI_BG256(n)    "\033[48;5;" #n "m"

/* ═══════════════════════════════════════════════════════════════
 *  Unicode Box Drawing
 * ═══════════════════════════════════════════════════════════════ */

/* Single line */
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

/* Double line */
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

/* Heavy line */
#define TUI_HH   "\xe2\x94\x81"  /* ━ */
#define TUI_HV   "\xe2\x94\x83"  /* ┃ */
#define TUI_HTL  "\xe2\x94\x8f"  /* ┏ */
#define TUI_HTR  "\xe2\x94\x93"  /* ┓ */
#define TUI_HBL  "\xe2\x94\x97"  /* ┗ */
#define TUI_HBR  "\xe2\x94\x9b"  /* ┛ */

/* Block elements */
#define TUI_BLOCK_FULL   "\xe2\x96\x88"  /* █ */
#define TUI_BLOCK_DARK   "\xe2\x96\x93"  /* ▓ */
#define TUI_BLOCK_MED    "\xe2\x96\x92"  /* ▒ */
#define TUI_BLOCK_LIGHT  "\xe2\x96\x91"  /* ░ */
#define TUI_BLOCK_UPPER  "\xe2\x96\x80"  /* ▀ */
#define TUI_BLOCK_LOWER  "\xe2\x96\x84"  /* ▄ */
#define TUI_BLOCK_LEFT   "\xe2\x96\x8c"  /* ▌ */
#define TUI_BLOCK_RIGHT  "\xe2\x96\x90"  /* ▐ */

/* ═══════════════════════════════════════════════════════════════
 *  Unicode Symbols
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
 *  Role Theme System
 * ═══════════════════════════════════════════════════════════════ */

typedef enum TuiRoleTheme {
    TUI_THEME_DEFAULT = 0,
    TUI_THEME_ADMIN = 1,
    TUI_THEME_DOCTOR = 2,
    TUI_THEME_PATIENT = 3,
    TUI_THEME_INPATIENT = 4,
    TUI_THEME_PHARMACY = 5
} TuiRoleTheme;

const char *tui_role_color(TuiRoleTheme theme);
const char *tui_role_bg(TuiRoleTheme theme);
const char *tui_role_icon(TuiRoleTheme theme);
const char *tui_role_label(TuiRoleTheme theme);

/* ═══════════════════════════════════════════════════════════════
 *  Screen Operations
 * ═══════════════════════════════════════════════════════════════ */

void tui_clear_screen(void);
void tui_move_cursor(FILE *out, int row, int col);
void tui_hide_cursor(FILE *out);
void tui_show_cursor(FILE *out);

/* ═══════════════════════════════════════════════════════════════
 *  Display Width (CJK-aware)
 * ═══════════════════════════════════════════════════════════════ */

int tui_display_width(const char *text);
void tui_pad_right(FILE *out, const char *text, int width);

/* ═══════════════════════════════════════════════════════════════
 *  Logo & Banner
 * ═══════════════════════════════════════════════════════════════ */

void tui_print_logo(FILE *out);
void tui_print_banner(FILE *out, const char *title);
void tui_print_gradient_text(FILE *out, const char *text);
void tui_print_welcome(FILE *out, TuiRoleTheme theme, const char *user_id);
void tui_print_goodbye(FILE *out);

/* ═══════════════════════════════════════════════════════════════
 *  Status Bar & Headers
 * ═══════════════════════════════════════════════════════════════ */

void tui_print_status_bar(FILE *out, const char *role, const char *user_id);
void tui_print_status_bar_themed(FILE *out, TuiRoleTheme theme, const char *user_id);
void tui_print_header(FILE *out, const char *title, int width);
void tui_print_section(FILE *out, const char *icon, const char *title);
void tui_print_hline(FILE *out, int width);
void tui_print_double_hline(FILE *out, int width);

/* ═══════════════════════════════════════════════════════════════
 *  Messages & Feedback
 * ═══════════════════════════════════════════════════════════════ */

void tui_print_success(FILE *out, const char *message);
void tui_print_error(FILE *out, const char *message);
void tui_print_warning(FILE *out, const char *message);
void tui_print_info(FILE *out, const char *message);
void tui_print_prompt(FILE *out, const char *text);

/* Badge: colored inline label */
typedef enum TuiBadgeColor {
    TUI_BADGE_GREEN = 0,
    TUI_BADGE_RED = 1,
    TUI_BADGE_YELLOW = 2,
    TUI_BADGE_BLUE = 3,
    TUI_BADGE_MAGENTA = 4,
    TUI_BADGE_CYAN = 5
} TuiBadgeColor;

void tui_print_badge(FILE *out, const char *text, TuiBadgeColor color);
void tui_print_kv(FILE *out, const char *key, const char *value);
void tui_print_kv_colored(FILE *out, const char *key, const char *value, const char *value_color);

/* Progress bar */
void tui_print_progress(FILE *out, const char *label, int current, int total, int bar_width);

/* ═══════════════════════════════════════════════════════════════
 *  Table System
 * ═══════════════════════════════════════════════════════════════ */

#define TUI_TABLE_MAX_COLUMNS 10

typedef struct TuiTable {
    int column_count;
    int widths[TUI_TABLE_MAX_COLUMNS];
    char headers[TUI_TABLE_MAX_COLUMNS][64];
    int row_count;
    const char *border_color;
    const char *header_color;
} TuiTable;

void TuiTable_init(TuiTable *table, int column_count);
void TuiTable_set_header(TuiTable *table, int col, const char *header, int width);
void TuiTable_set_colors(TuiTable *table, const char *border_color, const char *header_color);
void TuiTable_print_top(const TuiTable *table, FILE *out);
void TuiTable_print_header_row(const TuiTable *table, FILE *out);
void TuiTable_print_separator(const TuiTable *table, FILE *out);
void TuiTable_print_row(const TuiTable *table, FILE *out, const char *values[], int count);
void TuiTable_print_row_colored(const TuiTable *table, FILE *out, const char *values[], const char *colors[], int count);
void TuiTable_print_bottom(const TuiTable *table, FILE *out);
void TuiTable_print_empty(const TuiTable *table, FILE *out, const char *message);

/* ═══════════════════════════════════════════════════════════════
 *  Menu Styling
 * ═══════════════════════════════════════════════════════════════ */

void tui_print_menu_title(FILE *out, const char *title, int width);
void tui_print_menu_item(FILE *out, int number, const char *label, int width);
void tui_print_menu_item_icon(FILE *out, int number, const char *icon, const char *label, int width);
void tui_print_menu_bottom(FILE *out, int width);

/* ═══════════════════════════════════════════════════════════════
 *  Animation System
 * ═══════════════════════════════════════════════════════════════ */

/* TTY detection: animations are skipped when stdout is piped */
int tui_is_interactive(void);

/* Delay in milliseconds (no-op if not interactive) */
void tui_delay(int ms);

/* Animated logo: each line cascades down with color sweep */
void tui_animate_logo(FILE *out);

/* Animated banner: rainbow bar sweeps, then title typewriters in */
void tui_animate_banner(FILE *out, const char *title);

/* Typewriter: prints text char-by-char with per-char delay */
void tui_animate_typewriter(FILE *out, const char *text, int char_delay_ms);

/* Animated welcome: box draws itself, then content fades in */
void tui_animate_welcome(FILE *out, TuiRoleTheme theme, const char *user_id);

/* Animated goodbye: rainbow wave + fade out */
void tui_animate_goodbye(FILE *out);

/* Screen transition: horizontal wipe with gradient blocks */
void tui_animate_transition(FILE *out);

/* Print multi-line string line-by-line with slide-in effect */
void tui_animate_lines(FILE *out, const char *text, int line_delay_ms);

/* Spinner for brief waits (call start, then stop when done) */
void tui_spinner_run(FILE *out, const char *message, int duration_ms);

/* Animated progress bar that fills up */
void tui_animate_progress(FILE *out, const char *label, int steps, int bar_width);

/* Success/error with brief flash highlight */
void tui_animate_success(FILE *out, const char *message);
void tui_animate_error(FILE *out, const char *message);

/* Rainbow color cycling text (each char gets next color) */
void tui_animate_rainbow(FILE *out, const char *text, int cycles, int frame_delay_ms);

#endif
