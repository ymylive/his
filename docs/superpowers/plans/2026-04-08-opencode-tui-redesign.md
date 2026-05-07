# HIS TUI Redesign: opencode-Inspired Modern Terminal UI

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Transform HIS terminal UI from theatrical/decorative style to a refined, modern split-pane interface inspired by opencode's dark minimal aesthetic.

**Architecture:** Three-phase redesign: (1) Visual refresh with new color palette and clean borders, (2) Panel layout system for persistent sidebar + main content area, (3) Enhanced keyboard interaction with arrow-key navigation and live-filtered search. All changes stay in pure C with ANSI escape codes — no ncurses dependency.

**Tech Stack:** C11, ANSI escape sequences, `sys/ioctl.h` (POSIX) / Windows Console API

**Design System (from UI/UX Pro Max):**
- Style: Dark Mode (OLED) — high contrast, eye-friendly, terminal-native
- Background: `#020617` (near-black) / `#0F172A` (panel) / `#1E293B` (element)
- Foreground: `#F8FAFC` (text) / `#94A3B8` (muted) / `#334155` (border)
- Accent: `#22C55E` (success green) — medical theme
- Secondary accent: `#F59E0B` (warm amber) — for prompts and highlights
- Error: `#EF4444` / Warning: `#F59E0B` / Info: `#3B82F6`
- Border style: Left-accent `┃` (U+2503 heavy vertical), thin `─` separators
- Spacing: 2-cell horizontal padding, 1-line vertical gaps

---

## File Structure

### Files to Modify
| File | Responsibility | Change Scope |
|------|---------------|-------------|
| `include/ui/TuiStyle.h` | Color macros, function declarations | Add new color constants, panel API, key input types |
| `src/ui/TuiStyle.c` | Rendering, animations | Simplify visuals, add panel system |
| `src/ui/MenuController.c` | Menu buffer rendering | New menu format with left-accent style |
| `src/main.c` | Main loop, flow control | Split-pane loop, remove animations |
| `include/common/InputHelper.h` | Input API | Add key reading, arrow key support |
| `src/common/InputHelper.c` | Input implementation | Raw key reading, interactive line editor |

### Files to Create
| File | Responsibility |
|------|---------------|
| `include/ui/TuiPanel.h` | Panel/layout structs and API |
| `src/ui/TuiPanel.c` | Panel rendering, layout computation, resize handling |

---

## Phase 1: Visual Refresh

### Task 1: New Color Palette Constants

**Files:**
- Modify: `include/ui/TuiStyle.h`

- [ ] **Step 1: Add true-color (24-bit) macros to TuiStyle.h**

Add after the existing `TUI_BG256` macro block (around line 73):

```c
/* ═══════════════════════════════════════════════════════════════
 *  opencode-inspired 暗色主题 - 24位真彩色
 * ═══════════════════════════════════════════════════════════════ */

/* 背景层级（由深到浅） */
#define TUI_OC_BG         "\033[38;2;2;6;23m"      /* #020617 主背景 */
#define TUI_OC_BG_PANEL   "\033[48;2;15;23;42m"     /* #0F172A 面板背景 */
#define TUI_OC_BG_ELEMENT "\033[48;2;30;41;59m"     /* #1E293B 输入/高亮背景 */

/* 文字 */
#define TUI_OC_TEXT       "\033[38;2;248;250;252m"  /* #F8FAFC 主文字 */
#define TUI_OC_MUTED      "\033[38;2;148;163;184m"  /* #94A3B8 次要文字 */
#define TUI_OC_DIM        "\033[38;2;71;85;105m"    /* #475569 暗淡文字 */

/* 边框 */
#define TUI_OC_BORDER     "\033[38;2;51;65;85m"     /* #334155 边框色 */

/* 功能色 */
#define TUI_OC_ACCENT     "\033[38;2;34;197;94m"    /* #22C55E 主强调(绿) */
#define TUI_OC_AMBER      "\033[38;2;245;158;11m"   /* #F59E0B 暖琥珀(提示) */
#define TUI_OC_BLUE       "\033[38;2;59;130;246m"   /* #3B82F6 信息蓝 */
#define TUI_OC_ERROR      "\033[38;2;239;68;68m"    /* #EF4444 错误红 */
#define TUI_OC_WARN       "\033[38;2;245;158;11m"   /* #F59E0B 警告黄 */
#define TUI_OC_SUCCESS    "\033[38;2;34;197;94m"     /* #22C55E 成功绿 */

/* 强调背景 */
#define TUI_OC_BG_ACCENT  "\033[48;2;34;197;94m"    /* #22C55E 绿色背景 */
#define TUI_OC_BG_ERROR   "\033[48;2;239;68;68m"    /* #EF4444 红色背景 */
#define TUI_OC_BG_WARN    "\033[48;2;245;158;11m"   /* #F59E0B 黄色背景 */

/* 高亮选中行 */
#define TUI_OC_BG_SELECT  "\033[48;2;30;41;59m"     /* #1E293B 选中背景 */
```

- [ ] **Step 2: Build and verify no regressions**

Run: `cd /Users/cornna/project/his/his/build && cmake .. -DCMAKE_BUILD_TYPE=Debug && make -j$(sysctl -n hw.ncpu) && ctest --output-on-failure`
Expected: 20/20 PASS (only added macros, no behavior change)

- [ ] **Step 3: Commit**

```bash
git add include/ui/TuiStyle.h
git commit -m "feat: 添加opencode风格24位真彩色主题常量"
```

---

### Task 2: Simplify Message Feedback Functions

**Files:**
- Modify: `src/ui/TuiStyle.c` — functions `tui_print_success`, `tui_print_error`, `tui_print_warning`, `tui_print_info`, `tui_print_prompt`, `tui_print_prompt_themed`

- [ ] **Step 1: Rewrite message functions to use new palette**

Replace each function body. Pattern for all six functions:

```c
void tui_print_success(FILE *out, const char *message) {
    if (out == 0 || message == 0) return;
    fputc('\n', out);
    tui_print_margin(out, 46);
    fprintf(out, TUI_OC_SUCCESS " %s " TUI_OC_TEXT "%s" TUI_RESET "\n", TUI_CHECK, message);
}

void tui_print_error(FILE *out, const char *message) {
    if (out == 0 || message == 0) return;
    fputc('\n', out);
    tui_print_margin(out, 46);
    fprintf(out, TUI_OC_ERROR " %s " TUI_OC_TEXT "%s" TUI_RESET "\n", TUI_CROSS, message);
}

void tui_print_warning(FILE *out, const char *message) {
    if (out == 0 || message == 0) return;
    fputc('\n', out);
    tui_print_margin(out, 46);
    fprintf(out, TUI_OC_WARN " %s " TUI_OC_TEXT "%s" TUI_RESET "\n", TUI_TRIANGLE, message);
}

void tui_print_info(FILE *out, const char *message) {
    if (out == 0 || message == 0) return;
    tui_print_margin(out, 46);
    fprintf(out, TUI_OC_BLUE "%s" TUI_RESET " " TUI_OC_MUTED "%s" TUI_RESET "\n", TUI_DOT, message);
}

void tui_print_prompt(FILE *out, const char *text) {
    if (out == 0 || text == 0) return;
    fputc('\n', out);
    tui_print_margin(out, 46);
    fprintf(out, TUI_OC_AMBER "%s" TUI_RESET " " TUI_OC_TEXT "%s" TUI_RESET, TUI_ARROW, text);
    fflush(out);
}

void tui_print_prompt_themed(FILE *out, const char *text, TuiRoleTheme theme) {
    if (out == 0 || text == 0) return;
    fputc('\n', out);
    tui_print_margin(out, 46);
    fprintf(out, TUI_OC_ACCENT "%s" TUI_RESET " " TUI_OC_TEXT "%s" TUI_RESET, TUI_ARROW, text);
    fflush(out);
    (void)theme;  /* 统一使用accent色 */
}
```

- [ ] **Step 2: Simplify section header**

Rewrite `tui_print_section()`:

```c
void tui_print_section(FILE *out, const char *icon, const char *title) {
    if (out == 0 || title == 0) return;
    fputc('\n', out);
    tui_print_margin(out, 46);
    fputs(TUI_OC_ACCENT, out);
    fputs(TUI_HV, out);  /* ┃ left accent */
    fputs(TUI_RESET, out);
    fputs(" ", out);
    fputs(TUI_OC_TEXT TUI_BOLD, out);
    fputs(title, out);
    fputs(TUI_RESET, out);
    fputc('\n', out);
    (void)icon;
}
```

- [ ] **Step 3: Simplify horizontal line functions**

Rewrite `tui_print_gradient_hline()`:

```c
void tui_print_gradient_hline(FILE *out, int width) {
    int i = 0;
    if (out == 0) return;
    tui_print_margin(out, 46);
    fputs(TUI_OC_BORDER, out);
    for (i = 0; i < width; i++) fputs(TUI_H, out);
    fputs(TUI_RESET, out);
    fputc('\n', out);
}
```

- [ ] **Step 4: Build and run tests**

Run: `cd /Users/cornna/project/his/his/build && make -j$(sysctl -n hw.ncpu) && ctest --output-on-failure`
Expected: 20/20 PASS

- [ ] **Step 5: Commit**

```bash
git add src/ui/TuiStyle.c
git commit -m "feat: 消息反馈和区段标题改用opencode暗色简洁风格"
```

---

### Task 3: Simplify Logo and Welcome/Goodbye

**Files:**
- Modify: `src/ui/TuiStyle.c` — functions `tui_print_logo`, `tui_print_banner`, `tui_print_welcome`, `tui_print_goodbye`, `tui_print_status_bar_themed`

- [ ] **Step 1: Rewrite logo to use single accent color**

```c
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
```

- [ ] **Step 2: Simplify banner to minimal format**

```c
void tui_print_banner(FILE *out, const char *title) {
    int i = 0;
    if (out == 0 || title == 0) return;
    fputc('\n', out);
    tui_print_margin(out, 46);
    fputs(TUI_OC_DIM, out);
    for (i = 0; i < 46; i++) fputs(TUI_H, out);
    fputs(TUI_RESET, out);
    fputc('\n', out);
    tui_print_margin(out, 46);
    fputs(TUI_OC_TEXT TUI_BOLD, out);
    fputs(title, out);
    fputs(TUI_RESET, out);
    fputc('\n', out);
    tui_print_margin(out, 46);
    fputs(TUI_OC_DIM, out);
    for (i = 0; i < 46; i++) fputs(TUI_H, out);
    fputs(TUI_RESET, out);
    fputc('\n', out);
}
```

- [ ] **Step 3: Simplify welcome to 2-line message**

```c
void tui_print_welcome(FILE *out, TuiRoleTheme theme, const char *user_id) {
    if (out == 0) return;
    fputc('\n', out);
    tui_print_margin(out, 46);
    fprintf(out, TUI_OC_ACCENT "%s" TUI_RESET " " TUI_OC_TEXT TUI_BOLD "欢迎回来" TUI_RESET, TUI_CHECK);
    if (user_id != 0) {
        fprintf(out, TUI_OC_MUTED " (%s)" TUI_RESET, user_id);
    }
    fputc('\n', out);
    tui_print_margin(out, 46);
    fprintf(out, TUI_OC_MUTED "HIS v%s — 轻量级医院信息系统" TUI_RESET "\n", HIS_VERSION);
    (void)theme;
}
```

- [ ] **Step 4: Simplify goodbye**

```c
void tui_print_goodbye(FILE *out) {
    if (out == 0) return;
    fputc('\n', out);
    tui_print_margin(out, 46);
    fprintf(out, TUI_OC_ACCENT "%s" TUI_RESET " " TUI_OC_TEXT "再见！" TUI_RESET "\n", TUI_CHECK);
    tui_print_margin(out, 46);
    fprintf(out, TUI_OC_MUTED "感谢使用 HIS 系统" TUI_RESET "\n\n");
}
```

- [ ] **Step 5: Modernize status bar**

```c
void tui_print_status_bar_themed(FILE *out, TuiRoleTheme theme, const char *user_id) {
    int tw = 0;
    if (out == 0) return;
    tw = tui_get_terminal_width();
    /* 全宽深色背景条 */
    fprintf(out, TUI_OC_BG_PANEL TUI_OC_MUTED);
    {
        int i;
        char left[128];
        char right[64];
        int left_w, right_w, gap;
        snprintf(left, sizeof(left), " %s %s", tui_role_icon(theme), tui_role_label(theme));
        snprintf(right, sizeof(right), "%s  HIS v%s ", user_id ? user_id : "", HIS_VERSION);
        left_w = tui_display_width(left);
        right_w = tui_display_width(right);
        gap = tw - left_w - right_w;
        if (gap < 1) gap = 1;
        fputs(TUI_OC_ACCENT, out);
        fputs(left, out);
        fputs(TUI_OC_MUTED, out);
        for (i = 0; i < gap; i++) fputc(' ', out);
        fputs(right, out);
    }
    fputs(TUI_RESET, out);
    fputc('\n', out);
}
```

- [ ] **Step 6: Build and test**

Run: `make -j$(sysctl -n hw.ncpu) && ctest --output-on-failure`
Expected: 20/20 PASS

- [ ] **Step 7: Commit**

```bash
git add src/ui/TuiStyle.c
git commit -m "feat: Logo/Banner/Welcome/Goodbye/StatusBar改用opencode简洁风格"
```

---

### Task 4: Disable Startup Animations

**Files:**
- Modify: `src/main.c`
- Modify: `src/ui/TuiStyle.c` — animation functions

- [ ] **Step 1: Remove startup animations from main.c**

In `main.c`, replace the startup animation sequence (lines ~149-153) with:

```c
tui_clear_screen();
tui_print_logo(stdout);
tui_print_banner(stdout, "\xe8\xbd\xbb\xe9\x87\x8f\xe7\xba\xa7\xe5\x8c\xbb\xe9\x99\xa2\xe4\xbf\xa1\xe6\x81\xaf\xe7\xb3\xbb\xe7\xbb\x9f" /* 轻量级医院信息系统 */);
```

- [ ] **Step 2: Remove post-login animations from main.c**

Replace the login success animation sequence (lines ~312-316):

```c
tui_clear_screen();
tui_print_welcome(stdout, theme, user_id);
```

Remove `tui_animate_scanline`, `tui_animate_welcome`, `tui_animate_heartbeat` calls.

- [ ] **Step 3: Replace animated menu display with instant**

Replace `tui_animate_lines(stdout, menu_text, 30)` with:

```c
tui_animate_lines(stdout, menu_text, 0);  /* 0ms delay = instant */
```

- [ ] **Step 4: Make all tui_animate_* functions instant**

In `tui_animate_logo()`, `tui_animate_banner()`, `tui_animate_welcome()`, `tui_animate_goodbye()` — add early return to static versions:

```c
void tui_animate_logo(FILE *out) {
    tui_print_logo(out);  /* 直接静态输出 */
}

void tui_animate_banner(FILE *out, const char *title) {
    tui_print_banner(out, title);
}

void tui_animate_welcome(FILE *out, TuiRoleTheme theme, const char *user_id) {
    tui_print_welcome(out, theme, user_id);
}

void tui_animate_goodbye(FILE *out) {
    tui_print_goodbye(out);
}
```

- [ ] **Step 5: Build and test**

Run: `make -j$(sysctl -n hw.ncpu) && ctest --output-on-failure`
Expected: 20/20 PASS

- [ ] **Step 6: Commit**

```bash
git add src/main.c src/ui/TuiStyle.c
git commit -m "feat: 移除启动动画，改为即时显示"
```

---

### Task 5: Modern Menu Style (Left-Accent)

**Files:**
- Modify: `src/ui/MenuController.c` — `menu_append_hline`, `menu_append_title`, `menu_append_item`, `menu_append_footer`

- [ ] **Step 1: Rewrite menu border to thin line**

```c
static int menu_append_hline(
    char *buf, size_t cap, size_t *used,
    const char *left, const char *right
) {
    int w = 0;
    int i = 0;
    (void)left; (void)right;
    w = snprintf(buf + *used, cap - *used, TUI_OC_DIM);
    if (w < 0 || (size_t)w >= cap - *used) return -1;
    *used += (size_t)w;
    for (i = 0; i < MENU_INNER + 2; i++) {
        w = snprintf(buf + *used, cap - *used, "\xe2\x94\x80");  /* ─ */
        if (w < 0 || (size_t)w >= cap - *used) return -1;
        *used += (size_t)w;
    }
    w = snprintf(buf + *used, cap - *used, TUI_RESET "\n");
    if (w < 0 || (size_t)w >= cap - *used) return -1;
    *used += (size_t)w;
    return 0;
}
```

- [ ] **Step 2: Rewrite menu title with left-accent**

```c
static int menu_append_title(
    char *buf, size_t cap, size_t *used,
    const char *title
) {
    int w = 0;
    /* ┃ + space + bold title */
    w = snprintf(buf + *used, cap - *used,
                 TUI_OC_ACCENT "\xe2\x94\x83" TUI_RESET  /* ┃ */
                 " " TUI_OC_TEXT TUI_BOLD "%s" TUI_RESET "\n",
                 title);
    if (w < 0 || (size_t)w >= cap - *used) return -1;
    *used += (size_t)w;
    return 0;
}
```

- [ ] **Step 3: Rewrite menu item with clean indentation**

```c
static int menu_append_item(
    char *buf, size_t cap, size_t *used,
    int number, const char *icon, const char *label, int is_dim
) {
    int w = 0;
    const char *num_color = is_dim ? TUI_OC_DIM : TUI_OC_AMBER;
    const char *lbl_color = is_dim ? TUI_OC_DIM : TUI_OC_MUTED;
    (void)icon;
    w = snprintf(buf + *used, cap - *used,
                 TUI_OC_BORDER "\xe2\x94\x83" TUI_RESET  /* ┃ */
                 "  %s%d." TUI_RESET " %s%s" TUI_RESET "\n",
                 num_color, number, lbl_color, label);
    if (w < 0 || (size_t)w >= cap - *used) return -1;
    *used += (size_t)w;
    return 0;
}
```

- [ ] **Step 4: Simplify footer**

```c
static int menu_append_footer(
    char *buf, size_t cap, size_t *used
) {
    int w = 0;
    w = snprintf(buf + *used, cap - *used,
                 TUI_OC_DIM "HIS v" HIS_VERSION TUI_RESET "\n");
    if (w < 0 || (size_t)w >= cap - *used) return -1;
    *used += (size_t)w;
    return 0;
}
```

- [ ] **Step 5: Build and test**

Run: `make -j$(sysctl -n hw.ncpu) && ctest --output-on-failure`
Expected: 20/20 PASS (test_menu_controller checks for Chinese label substrings which are unchanged)

- [ ] **Step 6: Commit**

```bash
git add src/ui/MenuController.c
git commit -m "feat: 菜单改用左侧竖线+简洁排版的opencode风格"
```

---

### Task 6: Modern Table Style

**Files:**
- Modify: `src/ui/TuiStyle.c` — `TuiTable_print_top`, `TuiTable_print_header_row`, `TuiTable_print_separator`, `TuiTable_print_row`, `TuiTable_print_bottom`

- [ ] **Step 1: Rewrite table borders to thin/muted style**

Replace table border rendering to use `TUI_OC_BORDER` for all borders and `TUI_OC_ACCENT` for header text:

```c
void TuiTable_print_top(const TuiTable *table, FILE *out) {
    int i, col;
    if (table == 0 || out == 0) return;
    tui_print_margin(out, 46);
    fputs(TUI_OC_BORDER, out);
    for (col = 0; col < table->column_count; col++) {
        for (i = 0; i < table->widths[col] + 2; i++) fputs(TUI_H, out);
        if (col < table->column_count - 1) fputs(TUI_H, out);
    }
    fputs(TUI_RESET "\n", out);
}
```

Similar pattern for `TuiTable_print_separator`, `TuiTable_print_bottom`.

For `TuiTable_print_header_row`: use `TUI_OC_TEXT TUI_BOLD` for header text.
For `TuiTable_print_row`: use `TUI_OC_MUTED` for cell text, `TUI_OC_BORDER` for `│` separators.

- [ ] **Step 2: Build and test**

Run: `make -j$(sysctl -n hw.ncpu) && ctest --output-on-failure`
Expected: 20/20 PASS

- [ ] **Step 3: Commit**

```bash
git add src/ui/TuiStyle.c
git commit -m "feat: 表格改用暗色简洁边框风格"
```

---

## Phase 2: Panel Layout System

### Task 7: Terminal Size Detection

**Files:**
- Modify: `include/ui/TuiStyle.h`
- Modify: `src/ui/TuiStyle.c`

- [ ] **Step 1: Add terminal height function**

In TuiStyle.h, add declaration:

```c
int tui_get_terminal_height(void);
```

In TuiStyle.c, implement (right after `tui_get_terminal_width`):

```c
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
```

- [ ] **Step 2: Build and test**

Run: `make -j$(sysctl -n hw.ncpu) && ctest --output-on-failure`
Expected: 20/20 PASS

- [ ] **Step 3: Commit**

```bash
git add include/ui/TuiStyle.h src/ui/TuiStyle.c
git commit -m "feat: 添加终端高度检测函数"
```

---

### Task 8: Panel Layout System

**Files:**
- Create: `include/ui/TuiPanel.h`
- Create: `src/ui/TuiPanel.c`
- Modify: `CMakeLists.txt` — add TuiPanel.c to build

- [ ] **Step 1: Create TuiPanel.h**

```c
/**
 * @file TuiPanel.h
 * @brief 面板布局系统 - 分屏渲染引擎
 */

#ifndef HIS_UI_TUI_PANEL_H
#define HIS_UI_TUI_PANEL_H

#include <stdio.h>

/** 面板区域定义 */
typedef struct TuiPanel {
    int top;      /**< 起始行 (1-based) */
    int left;     /**< 起始列 (1-based) */
    int width;    /**< 宽度(列数) */
    int height;   /**< 高度(行数) */
} TuiPanel;

/** 三区布局: 侧栏 + 主区 + 底栏 */
typedef struct TuiLayout {
    TuiPanel sidebar;   /**< 左侧菜单栏 */
    TuiPanel main;      /**< 右侧主内容区 */
    TuiPanel footer;    /**< 底部状态栏 */
    int term_width;     /**< 终端宽度 */
    int term_height;    /**< 终端高度 */
} TuiLayout;

/**
 * @brief 根据终端尺寸计算三区布局
 * 侧栏占30%（最小20列，最大40列），底栏占1行
 */
void TuiLayout_compute(TuiLayout *layout);

/**
 * @brief 清空面板区域
 */
void TuiPanel_clear(FILE *out, const TuiPanel *panel);

/**
 * @brief 在面板内的相对位置写入文字
 * @param row 面板内行偏移 (0-based)
 * @param col 面板内列偏移 (0-based)
 */
void TuiPanel_write_at(FILE *out, const TuiPanel *panel, int row, int col, const char *text);

/**
 * @brief 绘制面板左侧竖线边框
 */
void TuiPanel_draw_left_border(FILE *out, const TuiPanel *panel, const char *color);

/**
 * @brief 在面板内逐行输出多行文本（自动换行到面板内）
 * @param start_row 起始行 (0-based)
 * @return 输出的行数
 */
int TuiPanel_write_lines(FILE *out, const TuiPanel *panel, int start_row, const char *text);

/**
 * @brief 绘制面板底部水平分隔线
 */
void TuiPanel_draw_bottom_border(FILE *out, const TuiPanel *panel, const char *color);

/**
 * @brief 将光标移动到面板内指定位置
 */
void TuiPanel_move_to(FILE *out, const TuiPanel *panel, int row, int col);

#endif /* HIS_UI_TUI_PANEL_H */
```

- [ ] **Step 2: Create TuiPanel.c**

```c
/**
 * @file TuiPanel.c
 * @brief 面板布局系统实现
 */

#include "ui/TuiPanel.h"
#include "ui/TuiStyle.h"
#include <string.h>

void TuiLayout_compute(TuiLayout *layout) {
    int tw, th, sidebar_w;
    if (layout == 0) return;

    tw = tui_get_terminal_width();
    th = tui_get_terminal_height();
    layout->term_width = tw;
    layout->term_height = th;

    /* 侧栏: 30% 宽度, 最小20列, 最大40列 */
    sidebar_w = tw * 30 / 100;
    if (sidebar_w < 20) sidebar_w = 20;
    if (sidebar_w > 40) sidebar_w = 40;

    /* 侧栏 */
    layout->sidebar.top = 1;
    layout->sidebar.left = 1;
    layout->sidebar.width = sidebar_w;
    layout->sidebar.height = th - 1;  /* 留1行给底栏 */

    /* 主区 */
    layout->main.top = 1;
    layout->main.left = sidebar_w + 1;
    layout->main.width = tw - sidebar_w;
    layout->main.height = th - 1;

    /* 底栏 */
    layout->footer.top = th;
    layout->footer.left = 1;
    layout->footer.width = tw;
    layout->footer.height = 1;
}

void TuiPanel_clear(FILE *out, const TuiPanel *panel) {
    int r, c;
    if (out == 0 || panel == 0) return;
    for (r = 0; r < panel->height; r++) {
        tui_move_cursor(out, panel->top + r, panel->left);
        for (c = 0; c < panel->width; c++) fputc(' ', out);
    }
    fflush(out);
}

void TuiPanel_write_at(FILE *out, const TuiPanel *panel, int row, int col, const char *text) {
    if (out == 0 || panel == 0 || text == 0) return;
    if (row < 0 || row >= panel->height) return;
    tui_move_cursor(out, panel->top + row, panel->left + col);
    fputs(text, out);
}

void TuiPanel_draw_left_border(FILE *out, const TuiPanel *panel, const char *color) {
    int r;
    if (out == 0 || panel == 0) return;
    if (color != 0) fputs(color, out);
    for (r = 0; r < panel->height; r++) {
        tui_move_cursor(out, panel->top + r, panel->left + panel->width - 1);
        fputs(TUI_HV, out);  /* ┃ */
    }
    fputs(TUI_RESET, out);
    fflush(out);
}

void TuiPanel_draw_bottom_border(FILE *out, const TuiPanel *panel, const char *color) {
    int c;
    if (out == 0 || panel == 0) return;
    tui_move_cursor(out, panel->top + panel->height, panel->left);
    if (color != 0) fputs(color, out);
    for (c = 0; c < panel->width; c++) fputs(TUI_H, out);  /* ─ */
    fputs(TUI_RESET, out);
    fflush(out);
}

void TuiPanel_move_to(FILE *out, const TuiPanel *panel, int row, int col) {
    if (out == 0 || panel == 0) return;
    tui_move_cursor(out, panel->top + row, panel->left + col);
}

int TuiPanel_write_lines(FILE *out, const TuiPanel *panel, int start_row, const char *text) {
    const char *p = text;
    int row = start_row;
    if (out == 0 || panel == 0 || text == 0) return 0;

    while (*p != '\0' && row < panel->height) {
        const char *nl = p;
        int line_len;
        while (*nl != '\0' && *nl != '\n') nl++;
        line_len = (int)(nl - p);

        tui_move_cursor(out, panel->top + row, panel->left);
        /* 清空行 */
        {
            int c;
            for (c = 0; c < panel->width; c++) fputc(' ', out);
        }
        tui_move_cursor(out, panel->top + row, panel->left + 2);  /* 2格内边距 */
        fwrite(p, 1, (size_t)line_len, out);

        row++;
        if (*nl == '\n') nl++;
        p = nl;
    }
    fflush(out);
    return row - start_row;
}
```

- [ ] **Step 3: Add TuiPanel.c to CMakeLists.txt**

Find the `his_common` library target source list and add `src/ui/TuiPanel.c`.

- [ ] **Step 4: Build and test**

Run: `cmake .. && make -j$(sysctl -n hw.ncpu) && ctest --output-on-failure`
Expected: 20/20 PASS

- [ ] **Step 5: Commit**

```bash
git add include/ui/TuiPanel.h src/ui/TuiPanel.c CMakeLists.txt
git commit -m "feat: 新增面板布局系统(TuiPanel)支持分屏渲染"
```

---

### Task 9: Split-Pane Main Loop

**Files:**
- Modify: `src/main.c`

- [ ] **Step 1: Add TuiPanel include and layout setup**

After login success, compute layout and enter split-pane mode:

```c
#include "ui/TuiPanel.h"

/* 登录成功后 */
{
    TuiLayout layout;
    TuiLayout_compute(&layout);
    tui_clear_screen();
    tui_hide_cursor(stdout);

    /* 绘制侧栏边框 */
    TuiPanel_draw_left_border(stdout, &layout.sidebar, TUI_OC_BORDER);

    /* 侧栏: 角色标题 + 菜单 */
    TuiPanel_write_at(stdout, &layout.sidebar, 1, 2,
                      TUI_OC_ACCENT TUI_BOLD);
    fprintf(stdout, "%s %s" TUI_RESET,
            tui_role_icon(theme), MenuController_role_label(role));
    /* 渲染菜单到侧栏 */
    TuiPanel_write_lines(stdout, &layout.sidebar, 3, menu_text);

    /* 底栏: 状态信息 */
    tui_move_cursor(stdout, layout.footer.top, layout.footer.left);
    fprintf(stdout, TUI_OC_BG_PANEL TUI_OC_MUTED " %s | %s  ",
            MenuController_role_label(role), user_id);
    /* 填满底栏 */
    {
        int i;
        for (i = ...; i < layout.term_width; i++) fputc(' ', stdout);
    }
    fputs(TUI_RESET, stdout);

    /* 主区: 提示 */
    TuiPanel_write_at(stdout, &layout.main, 1, 2, TUI_OC_MUTED "请选择操作" TUI_RESET);

    /* 输入区在主区底部 */
    TuiPanel_move_to(stdout, &layout.main, layout.main.height - 2, 2);
    fprintf(stdout, TUI_OC_AMBER "%s" TUI_RESET " " TUI_OC_TEXT "请输入: " TUI_RESET, TUI_ARROW);
    tui_show_cursor(stdout);
    fflush(stdout);
}
```

- [ ] **Step 2: Render operation output into main panel**

After `MenuApplication_execute_action()`, capture output and render it into `layout.main` panel using `TuiPanel_write_lines`. The sidebar menu stays visible throughout.

- [ ] **Step 3: Handle terminal resize**

Add SIGWINCH handler (POSIX only) to recompute layout:

```c
#ifndef _WIN32
#include <signal.h>
static volatile int g_resize_flag = 0;
static void handle_sigwinch(int sig) { (void)sig; g_resize_flag = 1; }
/* In main(): signal(SIGWINCH, handle_sigwinch); */
/* In loop: if (g_resize_flag) { g_resize_flag = 0; TuiLayout_compute(&layout); redraw(); } */
#endif
```

- [ ] **Step 4: Build and test**

Run: `make -j$(sysctl -n hw.ncpu) && ctest --output-on-failure`
Expected: 20/20 PASS

- [ ] **Step 5: Commit**

```bash
git add src/main.c
git commit -m "feat: 主循环改用分屏布局(侧栏菜单+主区内容+底栏状态)"
```

---

## Phase 3: Enhanced Interaction

### Task 10: Raw Key Reading

**Files:**
- Modify: `include/common/InputHelper.h`
- Modify: `src/common/InputHelper.c`

- [ ] **Step 1: Add key code enum and read_key function**

In InputHelper.h:

```c
/** 按键类型枚举 */
typedef enum InputKey {
    INPUT_KEY_NONE = 0,
    INPUT_KEY_CHAR,        /**< 普通字符 */
    INPUT_KEY_ENTER,       /**< 回车 */
    INPUT_KEY_ESC,         /**< ESC */
    INPUT_KEY_UP,          /**< 上箭头 */
    INPUT_KEY_DOWN,        /**< 下箭头 */
    INPUT_KEY_LEFT,        /**< 左箭头 */
    INPUT_KEY_RIGHT,       /**< 右箭头 */
    INPUT_KEY_BACKSPACE,   /**< 退格 */
    INPUT_KEY_TAB,         /**< Tab */
    INPUT_KEY_CTRL_Q,      /**< Ctrl+Q 退出 */
    INPUT_KEY_CTRL_C       /**< Ctrl+C 中断 */
} InputKey;

/** 按键事件 */
typedef struct InputEvent {
    InputKey key;          /**< 按键类型 */
    char ch;               /**< 如果key==INPUT_KEY_CHAR则为字符值 */
} InputEvent;

/**
 * @brief 读取单个按键（原始模式）
 * @return InputEvent 按键事件
 */
InputEvent InputHelper_read_key(FILE *input);
```

- [ ] **Step 2: Implement read_key in InputHelper.c**

```c
InputEvent InputHelper_read_key(FILE *input) {
    InputEvent event = {INPUT_KEY_NONE, 0};
    int ch;

#ifndef _WIN32
    /* POSIX: 使用termios进入raw模式 */
    struct termios old_term, new_term;
    tcgetattr(fileno(input), &old_term);
    new_term = old_term;
    new_term.c_lflag &= ~(ICANON | ECHO);
    new_term.c_cc[VMIN] = 1;
    new_term.c_cc[VTIME] = 0;
    tcsetattr(fileno(input), TCSANOW, &new_term);

    ch = fgetc(input);

    if (ch == 0x1B) {
        int next = fgetc(input);
        if (next == '[') {
            int arrow = fgetc(input);
            switch (arrow) {
                case 'A': event.key = INPUT_KEY_UP; break;
                case 'B': event.key = INPUT_KEY_DOWN; break;
                case 'C': event.key = INPUT_KEY_RIGHT; break;
                case 'D': event.key = INPUT_KEY_LEFT; break;
                default: event.key = INPUT_KEY_ESC; break;
            }
        } else {
            event.key = INPUT_KEY_ESC;
        }
    } else if (ch == '\n' || ch == '\r') {
        event.key = INPUT_KEY_ENTER;
    } else if (ch == 127 || ch == 8) {
        event.key = INPUT_KEY_BACKSPACE;
    } else if (ch == '\t') {
        event.key = INPUT_KEY_TAB;
    } else if (ch == 17) {  /* Ctrl+Q */
        event.key = INPUT_KEY_CTRL_Q;
    } else if (ch == 3) {   /* Ctrl+C */
        event.key = INPUT_KEY_CTRL_C;
    } else if (ch >= 32 && ch < 127) {
        event.key = INPUT_KEY_CHAR;
        event.ch = (char)ch;
    }

    tcsetattr(fileno(input), TCSANOW, &old_term);
#else
    /* Windows: 使用 _getch() */
    ch = _getch();
    if (ch == 0 || ch == 0xE0) {
        int ext = _getch();
        switch (ext) {
            case 72: event.key = INPUT_KEY_UP; break;
            case 80: event.key = INPUT_KEY_DOWN; break;
            case 75: event.key = INPUT_KEY_LEFT; break;
            case 77: event.key = INPUT_KEY_RIGHT; break;
            default: break;
        }
    } else if (ch == 27) {
        event.key = INPUT_KEY_ESC;
    } else if (ch == '\r' || ch == '\n') {
        event.key = INPUT_KEY_ENTER;
    } else if (ch == 8) {
        event.key = INPUT_KEY_BACKSPACE;
    } else if (ch == 17) {
        event.key = INPUT_KEY_CTRL_Q;
    } else if (ch >= 32 && ch < 127) {
        event.key = INPUT_KEY_CHAR;
        event.ch = (char)ch;
    }
#endif

    return event;
}
```

- [ ] **Step 3: Build and test**

Run: `make -j$(sysctl -n hw.ncpu) && ctest --output-on-failure`
Expected: 20/20 PASS

- [ ] **Step 4: Commit**

```bash
git add include/common/InputHelper.h src/common/InputHelper.c
git commit -m "feat: 添加原始按键读取(支持方向键/Ctrl组合键)"
```

---

### Task 11: Arrow-Key Menu Navigation

**Files:**
- Modify: `include/ui/MenuController.h`
- Modify: `src/ui/MenuController.c`

- [ ] **Step 1: Add interactive menu select function declaration**

In MenuController.h:

```c
/**
 * @brief 交互式菜单选择（支持方向键导航）
 * @param role       角色（决定菜单选项）
 * @param panel      面板区域（用于渲染定位）
 * @param input      输入流
 * @param out_action 输出选择的操作
 * @return Result    成功或失败
 */
Result MenuController_interactive_select(
    MenuRole role,
    void *panel,  /* TuiPanel* 但避免循环依赖 */
    FILE *input,
    MenuAction *out_action
);
```

- [ ] **Step 2: Implement interactive menu with highlight**

In MenuController.c:

```c
Result MenuController_interactive_select(
    MenuRole role,
    void *panel_ptr,
    FILE *input,
    MenuAction *out_action
) {
    const TuiPanel *panel = (const TuiPanel *)panel_ptr;
    const MenuOption *options = 0;
    size_t option_count = 0;
    int selected = 0;  /* 当前高亮索引 */
    int redraw = 1;

    if (out_action == 0) return Result_make_failure("action output missing");

    options = MenuController_options_for_role(role, &option_count);
    if (options == 0) return Result_make_failure("role menu not found");

    for (;;) {
        if (redraw) {
            size_t i;
            for (i = 0; i < option_count; i++) {
                int is_selected = ((int)i == selected);
                /* 移动光标到面板内对应行 */
                TuiPanel_write_at(stdout, panel, 3 + (int)i, 2, "");
                if (is_selected) {
                    fprintf(stdout, TUI_OC_BG_SELECT TUI_OC_ACCENT " > %d. %s " TUI_RESET,
                            options[i].selection, options[i].label);
                } else {
                    fprintf(stdout, "   " TUI_OC_AMBER "%d." TUI_RESET " " TUI_OC_MUTED "%s" TUI_RESET,
                            options[i].selection, options[i].label);
                }
                /* 清除行尾 */
                fputs("\033[K", stdout);
            }
            fflush(stdout);
            redraw = 0;
        }

        InputEvent ev = InputHelper_read_key(input);
        switch (ev.key) {
            case INPUT_KEY_UP:
                if (selected > 0) { selected--; redraw = 1; }
                break;
            case INPUT_KEY_DOWN:
                if (selected < (int)option_count - 1) { selected++; redraw = 1; }
                break;
            case INPUT_KEY_ENTER:
                *out_action = options[selected].action;
                return Result_make_success("action selected");
            case INPUT_KEY_ESC:
                *out_action = MENU_ACTION_BACK;
                return Result_make_success("back selected");
            case INPUT_KEY_CTRL_Q:
                *out_action = MENU_ACTION_BACK;
                return Result_make_failure("quit requested");
            case INPUT_KEY_CHAR:
                /* 数字直选仍然支持 */
                if (ev.ch >= '0' && ev.ch <= '9') {
                    int num = ev.ch - '0';
                    size_t j;
                    for (j = 0; j < option_count; j++) {
                        if (options[j].selection == num) {
                            *out_action = options[j].action;
                            return Result_make_success("action selected");
                        }
                    }
                }
                break;
            default:
                break;
        }
    }
}
```

- [ ] **Step 3: Build and test**

Run: `make -j$(sysctl -n hw.ncpu) && ctest --output-on-failure`
Expected: 20/20 PASS

- [ ] **Step 4: Commit**

```bash
git add include/ui/MenuController.h src/ui/MenuController.c
git commit -m "feat: 添加方向键导航的交互式菜单选择"
```

---

### Task 12: Integrate Interactive Menu into Main Loop

**Files:**
- Modify: `src/main.c`

- [ ] **Step 1: Replace number-input menu with interactive select**

In the role action loop, replace the `tui_animate_lines` + `InputHelper_read_line` + `MenuController_parse_role_selection` pattern with:

```c
/* 操作选择循环 */
for (;;) {
    MenuAction action = MENU_ACTION_INVALID;

    /* 侧栏: 渲染角色菜单标题 */
    TuiPanel_write_at(stdout, &layout.sidebar, 1, 2, "");
    fprintf(stdout, TUI_OC_ACCENT TUI_BOLD "%s %s" TUI_RESET,
            tui_role_icon(theme), MenuController_role_label(role));

    /* 交互式菜单选择 */
    result = MenuController_interactive_select(
        role, &layout.sidebar, stdin, &action);

    if (MenuController_is_back_action(action)) {
        MenuApplication_logout(&application);
        break;
    }

    /* 在主区执行并显示结果 */
    TuiPanel_clear(stdout, &layout.main);
    TuiPanel_write_at(stdout, &layout.main, 0, 2, "");
    /* 执行操作... */
    result = MenuApplication_execute_action(&application, action, stdin, stdout);
    /* 操作完成后自动回到菜单选择，无需"按回车继续" */
}
```

- [ ] **Step 2: Build and test**

Run: `make -j$(sysctl -n hw.ncpu) && ctest --output-on-failure`
Expected: 20/20 PASS

- [ ] **Step 3: Commit**

```bash
git add src/main.c
git commit -m "feat: 主循环集成方向键交互式菜单"
```

---

### Task 13: Live-Filtered Search Selector

**Files:**
- Modify: `src/ui/MenuApplication.c` — `MenuApplication_prompt_select_option`

- [ ] **Step 1: Add interactive search with real-time filtering**

Rewrite `MenuApplication_prompt_select_option()` to support character-by-character input with live filtering:

```c
static Result MenuApplication_prompt_select_option(
    MenuApplicationPromptContext *context,
    const char *title,
    const MenuApplicationSelectionOption *options,
    int option_count,
    char *out_selected_id,
    size_t id_capacity
) {
    char query[64] = {0};
    int query_len = 0;
    int selected = 0;
    int filtered_indices[MENU_APPLICATION_SELECT_OPTION_MAX];
    int filtered_count = 0;
    int i;

    /* 初始: 所有选项 */
    for (i = 0; i < option_count; i++) filtered_indices[i] = i;
    filtered_count = option_count;

    for (;;) {
        /* 显示标题和搜索框 */
        fprintf(context->output, "\033[2K\r");
        fprintf(context->output, TUI_OC_ACCENT "%s" TUI_RESET " " TUI_OC_TEXT "%s" TUI_RESET,
                TUI_HV, title);
        fprintf(context->output, "\n\033[2K");
        fprintf(context->output, TUI_OC_AMBER "搜索: " TUI_RESET TUI_OC_TEXT "%s" TUI_RESET "\033[K",
                query);

        /* 显示过滤结果(最多显示8条) */
        {
            int show = filtered_count < 8 ? filtered_count : 8;
            for (i = 0; i < show; i++) {
                int idx = filtered_indices[i];
                fprintf(context->output, "\n\033[2K");
                if (i == selected) {
                    fprintf(context->output, TUI_OC_BG_SELECT TUI_OC_ACCENT " > %s" TUI_RESET,
                            options[idx].label);
                } else {
                    fprintf(context->output, "   " TUI_OC_MUTED "%s" TUI_RESET,
                            options[idx].label);
                }
            }
            /* 如果有更多结果 */
            if (filtered_count > show) {
                fprintf(context->output, "\n\033[2K" TUI_OC_DIM "  ... 还有 %d 项" TUI_RESET,
                        filtered_count - show);
            }
        }
        fflush(context->output);

        /* 读取按键 */
        {
            InputEvent ev = InputHelper_read_key(context->input);
            switch (ev.key) {
                case INPUT_KEY_UP:
                    if (selected > 0) selected--;
                    break;
                case INPUT_KEY_DOWN:
                    if (selected < filtered_count - 1 && selected < 7) selected++;
                    break;
                case INPUT_KEY_ENTER:
                    if (filtered_count > 0) {
                        int idx = filtered_indices[selected];
                        strncpy(out_selected_id, options[idx].id, id_capacity - 1);
                        out_selected_id[id_capacity - 1] = '\0';
                        return Result_make_success("option selected");
                    }
                    break;
                case INPUT_KEY_ESC:
                    return Result_make_failure(INPUT_HELPER_ESC_MESSAGE);
                case INPUT_KEY_BACKSPACE:
                    if (query_len > 0) {
                        query[--query_len] = '\0';
                        /* 重新过滤 */
                        filtered_count = 0;
                        selected = 0;
                        for (i = 0; i < option_count; i++) {
                            if (query_len == 0 || strstr(options[i].label, query) != 0) {
                                filtered_indices[filtered_count++] = i;
                            }
                        }
                    }
                    break;
                case INPUT_KEY_CHAR:
                    if (query_len < (int)sizeof(query) - 1) {
                        query[query_len++] = ev.ch;
                        query[query_len] = '\0';
                        /* 重新过滤 */
                        filtered_count = 0;
                        selected = 0;
                        for (i = 0; i < option_count; i++) {
                            if (strstr(options[i].label, query) != 0) {
                                filtered_indices[filtered_count++] = i;
                            }
                        }
                    }
                    break;
                default:
                    break;
            }
        }

        /* 光标回到搜索框位置(向上移动) */
        {
            int lines_to_go_up = 1 + (filtered_count < 8 ? filtered_count : 8);
            if (filtered_count > 8) lines_to_go_up++;
            fprintf(context->output, "\033[%dA", lines_to_go_up);
        }
    }
}
```

- [ ] **Step 2: Build and test**

Run: `make -j$(sysctl -n hw.ncpu) && ctest --output-on-failure`
Expected: 20/20 PASS

- [ ] **Step 3: Commit**

```bash
git add src/ui/MenuApplication.c
git commit -m "feat: 实体选择器改用实时过滤+方向键导航"
```

---

## Verification

### End-to-End Testing

After completing all tasks:

1. **Build all platforms locally:**
```bash
cd /Users/cornna/project/his/his/build
cmake .. -DCMAKE_BUILD_TYPE=Debug
make -j$(sysctl -n hw.ncpu)
ctest --output-on-failure
```
Expected: 20/20 tests pass

2. **Visual verification:**
```bash
./build/his
```
- Verify: instant startup (no animations), dark muted colors, left-accent `┃` borders
- Verify: split-pane layout (sidebar menu + main content area + status footer)
- Verify: arrow keys navigate menu, Enter selects, ESC goes back
- Verify: patient/doctor search shows live-filtered results as you type
- Verify: resize terminal → layout recomputes correctly

3. **Cross-platform CI:**
```bash
git push origin main
```
Verify: GitHub CI passes on Linux, macOS, Windows (MinGW)

4. **Windows-specific:**
- Verify `_getch()` key reading works for arrow keys
- Verify true-color (`\033[38;2;R;G;Bm`) renders in Windows Terminal (Windows 10+)
